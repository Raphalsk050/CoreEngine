#include "platform/assimp/assimp_model_importer.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_loadso.h>
#include <assimp/cimport.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#ifndef CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_NAME
#if defined(_WIN32)
#define CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_NAME "assimp-vc143-mt.dll"
#elif defined(__APPLE__)
#define CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_NAME "libassimp.6.dylib"
#else
#define CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_NAME "libassimp.so.6"
#endif
#endif

#ifndef CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_PATH
#define CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_PATH ""
#endif

namespace CoreEngine {
    namespace {
        using AiImportFileFn = const aiScene *(*)(const char *, unsigned int);
        using AiReleaseImportFn = void (*)(const aiScene *);
        using AiGetErrorStringFn = const char *(*)();

        struct AssimpRuntime {
            SDL_SharedObject *library = nullptr;
            AiImportFileFn import_file = nullptr;
            AiReleaseImportFn release_import = nullptr;
            AiGetErrorStringFn get_error_string = nullptr;
            std::string error_message;

            ~AssimpRuntime() {
                if (library != nullptr) {
                    SDL_UnloadObject(library);
                    library = nullptr;
                }
            }

            [[nodiscard]] bool IsLoaded() const {
                return library != nullptr && import_file != nullptr && release_import != nullptr &&
                       get_error_string != nullptr;
            }

            [[nodiscard]] bool EnsureLoaded() {
                if (IsLoaded()) {
                    return true;
                }

                const std::string base_path_library = BuildBasePathLibraryName();
                if (!LoadLibrary(base_path_library.c_str()) &&
                    !LoadLibrary(CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_NAME) &&
                    !LoadLibrary(CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_PATH)) {
                    return false;
                }

                return LoadRequiredSymbols();
            }

            [[nodiscard]] bool LoadLibrary(const char *library_path) {
                if (library_path == nullptr || library_path[0] == '\0') {
                    return false;
                }

                library = SDL_LoadObject(library_path);
                if (library == nullptr) {
                    error_message = "Failed to load Assimp runtime '" + std::string(library_path) + "': " +
                                    SDL_GetError();
                    return false;
                }

                return true;
            }

            [[nodiscard]] bool LoadRequiredSymbols() {
                import_file = LoadSymbol<AiImportFileFn>("aiImportFile");
                release_import = LoadSymbol<AiReleaseImportFn>("aiReleaseImport");
                get_error_string = LoadSymbol<AiGetErrorStringFn>("aiGetErrorString");

                if (IsLoaded()) {
                    return true;
                }

                if (library != nullptr) {
                    SDL_UnloadObject(library);
                    library = nullptr;
                }

                error_message = "Failed to resolve required Assimp C API symbols";
                return false;
            }

            template<typename Fn>
            [[nodiscard]] Fn LoadSymbol(const char *name) {
                return reinterpret_cast<Fn>(SDL_LoadFunction(library, name));
            }

            [[nodiscard]] static std::string BuildBasePathLibraryName() {
                const char *base_path = SDL_GetBasePath();
                if (base_path == nullptr || base_path[0] == '\0') {
                    return {};
                }

                return std::string{base_path} + CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_NAME;
            }
        };

        struct SceneGuard {
            const aiScene *scene = nullptr;
            AiReleaseImportFn release = nullptr;

            ~SceneGuard() {
                if (scene != nullptr && release != nullptr) {
                    release(scene);
                }
            }
        };

        [[nodiscard]] unsigned int BuildPostProcessFlags(const ModelLoadDesc &desc) {
            unsigned int flags = 0;

            if (desc.triangulate) {
                flags |= aiProcess_Triangulate;
            }
            if (desc.join_identical_vertices) {
                flags |= aiProcess_JoinIdenticalVertices;
            }
            if (desc.generate_normals) {
                flags |= aiProcess_GenNormals;
            }
            if (desc.calculate_tangents) {
                flags |= aiProcess_CalcTangentSpace;
            }
            if (desc.convert_to_left_handed) {
                flags |= aiProcess_ConvertToLeftHanded;
            } else if (desc.flip_uvs) {
                flags |= aiProcess_FlipUVs;
            }

            return flags;
        }

        [[nodiscard]] std::string MeshName(const aiMesh &mesh, std::uint32_t index) {
            if (mesh.mName.length > 0) {
                return mesh.mName.C_Str();
            }

            return "Mesh_" + std::to_string(index);
        }

        [[nodiscard]] Math::Vec3 ReadPosition(const aiVector3D &value) {
            return {value.x, value.y, value.z};
        }

        [[nodiscard]] Math::Vec3 ReadNormal(const aiMesh &mesh, std::uint32_t vertex_index) {
            if (mesh.mNormals == nullptr) {
                return {0.f, 1.f, 0.f};
            }

            const aiVector3D &normal = mesh.mNormals[vertex_index];
            return {normal.x, normal.y, normal.z};
        }

        [[nodiscard]] Math::Vec3 ReadColor(const aiMesh &mesh, std::uint32_t vertex_index) {
            if (mesh.mColors[0] == nullptr) {
                return {1.f, 1.f, 1.f};
            }

            const aiColor4D &color = mesh.mColors[0][vertex_index];
            return {color.r, color.g, color.b};
        }

        [[nodiscard]] Math::Vec2 ReadUv(const aiMesh &mesh, std::uint32_t vertex_index) {
            if (mesh.mTextureCoords[0] == nullptr) {
                return {0.f, 0.f};
            }

            const aiVector3D &uv = mesh.mTextureCoords[0][vertex_index];
            return {uv.x, uv.y};
        }

        [[nodiscard]] bool ConvertMesh(const aiMesh &source,
                                       std::uint32_t mesh_index,
                                       ModelMeshAsset &target,
                                       std::string &error_message) {
            if (!source.HasPositions() || !source.HasFaces()) {
                error_message = "Assimp mesh has no positions or faces";
                return false;
            }

            target.name = MeshName(source, mesh_index);
            target.vertices.reserve(source.mNumVertices);
            target.indices.reserve(static_cast<std::size_t>(source.mNumFaces) * 3u);

            for (std::uint32_t vertex_index = 0; vertex_index < source.mNumVertices; ++vertex_index) {
                StaticMeshVertex vertex;
                vertex.position = ReadPosition(source.mVertices[vertex_index]);
                vertex.normal = ReadNormal(source, vertex_index);
                vertex.color = ReadColor(source, vertex_index);
                vertex.uv = ReadUv(source, vertex_index);
                target.vertices.push_back(vertex);
            }

            for (std::uint32_t face_index = 0; face_index < source.mNumFaces; ++face_index) {
                const aiFace &face = source.mFaces[face_index];
                if (face.mNumIndices != 3u) {
                    error_message = "Assimp mesh contains a non-triangulated face";
                    return false;
                }

                for (std::uint32_t index = 0; index < face.mNumIndices; ++index) {
                    if (face.mIndices[index] >= source.mNumVertices) {
                        error_message = "Assimp mesh contains an out-of-range vertex index";
                        return false;
                    }
                    target.indices.push_back(face.mIndices[index]);
                }
            }

            return target.IsValid();
        }

        [[nodiscard]] ModelLoadResult ConvertScene(const aiScene &scene, const std::string &path) {
            ModelLoadResult result;

            if ((scene.mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene.HasMeshes()) {
                result.error_message = "Assimp imported an incomplete scene: " + path;
                return result;
            }

            result.asset.meshes.reserve(scene.mNumMeshes);

            for (std::uint32_t mesh_index = 0; mesh_index < scene.mNumMeshes; ++mesh_index) {
                const aiMesh *mesh = scene.mMeshes[mesh_index];
                if (mesh == nullptr) {
                    result.error_message = "Assimp scene contains a null mesh";
                    return result;
                }

                ModelMeshAsset converted;
                if (!ConvertMesh(*mesh, mesh_index, converted, result.error_message)) {
                    result.error_message += ": " + path;
                    return result;
                }

                result.asset.meshes.push_back(std::move(converted));
            }

            if (!result.asset.IsValid()) {
                result.error_message = "Assimp scene did not produce renderable meshes: " + path;
            }

            return result;
        }
    } // namespace

    struct AssimpModelImporter::Impl {
        std::mutex mutex;
        AssimpRuntime runtime;
    };

    AssimpModelImporter::AssimpModelImporter()
        : impl_(std::make_unique<Impl>()) {
    }

    AssimpModelImporter::~AssimpModelImporter() = default;

    ModelLoadResult AssimpModelImporter::Load(const ModelLoadDesc &desc) {
        ModelLoadResult result;
        if (!desc.IsValid()) {
            result.error_message = "Invalid model load request";
            return result;
        }

        std::lock_guard lock{impl_->mutex};
        if (!impl_->runtime.EnsureLoaded()) {
            result.error_message = impl_->runtime.error_message;
            return result;
        }

        const unsigned int flags = BuildPostProcessFlags(desc);
        SceneGuard scene{
            .scene = impl_->runtime.import_file(desc.path.c_str(), flags),
            .release = impl_->runtime.release_import,
        };

        if (scene.scene == nullptr) {
            const char *assimp_error = impl_->runtime.get_error_string();
            result.error_message = "Failed to import model '" + desc.path + "'";
            if (assimp_error != nullptr && assimp_error[0] != '\0') {
                result.error_message += ": ";
                result.error_message += assimp_error;
            }
            return result;
        }

        return ConvertScene(*scene.scene, desc.path);
    }
} // namespace CoreEngine
