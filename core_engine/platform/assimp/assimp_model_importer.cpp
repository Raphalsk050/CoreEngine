#include "platform/assimp/assimp_model_importer.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_loadso.h>
#include <assimp/GltfMaterial.h>
#include <assimp/cimport.h>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>

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
        using AiImportFileFn = const aiScene *(*) (const char *, unsigned int);
        using AiReleaseImportFn = void (*)(const aiScene *);
        using AiGetErrorStringFn = const char *(*) ();
        using AiGetMaterialStringFn = aiReturn (*)(const aiMaterial *, const char *, unsigned int, unsigned int,
                                                   aiString *);
        using AiGetMaterialColorFn = aiReturn (*)(const aiMaterial *, const char *, unsigned int, unsigned int,
                                                  aiColor4D *);
        using AiGetMaterialFloatArrayFn = aiReturn (*)(const aiMaterial *, const char *, unsigned int, unsigned int,
                                                       ai_real *, unsigned int *);
        using AiGetMaterialTextureCountFn = unsigned int (*)(const aiMaterial *, aiTextureType);
        using AiGetMaterialTextureFn = aiReturn (*)(const aiMaterial *, aiTextureType, unsigned int, aiString *,
                                                    aiTextureMapping *, unsigned int *, ai_real *, aiTextureOp *,
                                                    aiTextureMapMode *, unsigned int *);

        struct AssimpRuntime {
            SDL_SharedObject *library = nullptr;
            AiImportFileFn import_file = nullptr;
            AiReleaseImportFn release_import = nullptr;
            AiGetErrorStringFn get_error_string = nullptr;
            AiGetMaterialStringFn get_material_string = nullptr;
            AiGetMaterialColorFn get_material_color = nullptr;
            AiGetMaterialFloatArrayFn get_material_float_array = nullptr;
            AiGetMaterialTextureCountFn get_material_texture_count = nullptr;
            AiGetMaterialTextureFn get_material_texture = nullptr;
            std::string error_message;

            ~AssimpRuntime() {
                if (library != nullptr) {
                    SDL_UnloadObject(library);
                    library = nullptr;
                }
            }

            [[nodiscard]] bool IsLoaded() const {
                return library != nullptr && import_file != nullptr && release_import != nullptr &&
                       get_error_string != nullptr && get_material_string != nullptr && get_material_color != nullptr &&
                       get_material_float_array != nullptr && get_material_texture_count != nullptr &&
                       get_material_texture != nullptr;
            }

            [[nodiscard]] bool EnsureLoaded() {
                if (IsLoaded()) {
                    return true;
                }

                const std::string base_path_library = BuildBasePathLibraryName();
                if (!LoadLibrary(base_path_library.c_str()) && !LoadLibrary(CORE_ENGINE_ASSIMP_RUNTIME_LIBRARY_NAME) &&
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
                    error_message =
                            "Failed to load Assimp runtime '" + std::string(library_path) + "': " + SDL_GetError();
                    return false;
                }

                return true;
            }

            [[nodiscard]] bool LoadRequiredSymbols() {
                import_file = LoadSymbol<AiImportFileFn>("aiImportFile");
                release_import = LoadSymbol<AiReleaseImportFn>("aiReleaseImport");
                get_error_string = LoadSymbol<AiGetErrorStringFn>("aiGetErrorString");
                get_material_string = LoadSymbol<AiGetMaterialStringFn>("aiGetMaterialString");
                get_material_color = LoadSymbol<AiGetMaterialColorFn>("aiGetMaterialColor");
                get_material_float_array = LoadSymbol<AiGetMaterialFloatArrayFn>("aiGetMaterialFloatArray");
                get_material_texture_count = LoadSymbol<AiGetMaterialTextureCountFn>("aiGetMaterialTextureCount");
                get_material_texture = LoadSymbol<AiGetMaterialTextureFn>("aiGetMaterialTexture");

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

        [[nodiscard]] ModelMergeMode ResolveMergeMode(const ModelLoadDesc &desc) {
            if (desc.merge_mode != ModelMergeMode::None) {
                return desc.merge_mode;
            }

            return ModelMergeMode::None;
        }

        [[nodiscard]] std::string MeshName(const aiMesh &mesh, std::uint32_t index) {
            if (mesh.mName.length > 0) {
                return mesh.mName.C_Str();
            }

            return "Mesh_" + std::to_string(index);
        }

        [[nodiscard]] std::string MaterialName(const AssimpRuntime &runtime, const aiMaterial &material,
                                               std::uint32_t index) {
            aiString name{};
            if (runtime.get_material_string(&material, AI_MATKEY_NAME, &name) == AI_SUCCESS && name.length > 0) {
                return name.C_Str();
            }

            return "Material_" + std::to_string(index);
        }

        [[nodiscard]] std::string NodeName(const aiNode &node, std::uint32_t index) {
            if (node.mName.length > 0) {
                return node.mName.C_Str();
            }

            return "Node_" + std::to_string(index);
        }

        [[nodiscard]] Math::Mat4 ReadMatrix(const aiMatrix4x4 &source) {
            Math::Mat4 target{1.f};
            target.At(0, 0) = source.a1;
            target.At(0, 1) = source.a2;
            target.At(0, 2) = source.a3;
            target.At(0, 3) = source.a4;
            target.At(1, 0) = source.b1;
            target.At(1, 1) = source.b2;
            target.At(1, 2) = source.b3;
            target.At(1, 3) = source.b4;
            target.At(2, 0) = source.c1;
            target.At(2, 1) = source.c2;
            target.At(2, 2) = source.c3;
            target.At(2, 3) = source.c4;
            target.At(3, 0) = source.d1;
            target.At(3, 1) = source.d2;
            target.At(3, 2) = source.d3;
            target.At(3, 3) = source.d4;
            return target;
        }

        [[nodiscard]] Math::Vec3 ReadPosition(const aiVector3D &value) { return {value.x, value.y, value.z}; }

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

        [[nodiscard]] Math::Vec4 ReadTangent(const aiMesh &mesh, std::uint32_t vertex_index) {
            if (mesh.mTangents == nullptr) {
                return {0.f, 0.f, 0.f, 1.f};
            }

            const aiVector3D &source_tangent = mesh.mTangents[vertex_index];
            const Math::Vec3 tangent{source_tangent.x, source_tangent.y, source_tangent.z};
            float handedness = 1.f;

            if (mesh.mNormals != nullptr && mesh.mBitangents != nullptr) {
                const aiVector3D &source_normal = mesh.mNormals[vertex_index];
                const aiVector3D &source_bitangent = mesh.mBitangents[vertex_index];
                const Math::Vec3 normal{source_normal.x, source_normal.y, source_normal.z};
                const Math::Vec3 bitangent{source_bitangent.x, source_bitangent.y, source_bitangent.z};
                handedness = Math::Dot(Math::Cross(normal, tangent), bitangent) < 0.f ? -1.f : 1.f;
            }

            return {tangent.x, tangent.y, tangent.z, handedness};
        }

        [[nodiscard]] bool TryReadMaterialColor(const AssimpRuntime &runtime, const aiMaterial &material,
                                                const char *key, unsigned int type, unsigned int index,
                                                aiColor4D &out_color) {
            return runtime.get_material_color(&material, key, type, index, &out_color) == AI_SUCCESS;
        }

        [[nodiscard]] float ReadMaterialFloat(const AssimpRuntime &runtime, const aiMaterial &material, const char *key,
                                              unsigned int type, unsigned int index, float fallback) {
            ai_real value = static_cast<ai_real>(fallback);
            unsigned int value_count = 1u;
            if (runtime.get_material_float_array(&material, key, type, index, &value, &value_count) == AI_SUCCESS &&
                value_count > 0u) {
                return static_cast<float>(value);
            }

            return fallback;
        }

        [[nodiscard]] ModelTextureAsset BuildEmbeddedTextureAsset(const aiScene &scene, const std::string &model_path,
                                                                  const std::string &raw_path,
                                                                  ModelTextureSemantic semantic, bool srgb) {
            if (raw_path.size() <= 1u || raw_path.front() != '*') {
                return {};
            }

            char *parse_end = nullptr;
            const unsigned long texture_index = std::strtoul(raw_path.c_str() + 1, &parse_end, 10);
            if (parse_end == raw_path.c_str() + 1 ||
                texture_index > static_cast<unsigned long>(std::numeric_limits<unsigned int>::max()) ||
                static_cast<unsigned int>(texture_index) >= scene.mNumTextures || scene.mTextures == nullptr) {
                return {};
            }

            const aiTexture *texture = scene.mTextures[static_cast<unsigned int>(texture_index)];
            if (texture == nullptr || texture->pcData == nullptr || texture->mWidth == 0u || texture->mHeight != 0u) {
                return {};
            }

            const auto *bytes = reinterpret_cast<const unsigned char *>(texture->pcData);
            ModelTextureAsset asset{
                    .semantic = semantic,
                    .path = model_path + "#embedded_" + std::to_string(texture_index),
                    .data = {},
                    .srgb = srgb,
            };
            asset.data.assign(bytes, bytes + texture->mWidth);
            return asset;
        }

        [[nodiscard]] ModelTextureAsset BuildFileTextureAsset(const std::string &model_path,
                                                              const std::string &raw_path,
                                                              ModelTextureSemantic semantic, bool srgb) {
            std::filesystem::path path{raw_path};
            if (!path.is_absolute()) {
                const std::filesystem::path base_path = std::filesystem::path{model_path}.parent_path();
                path = base_path / path;
            }

            return ModelTextureAsset{
                    .semantic = semantic,
                    .path = path.lexically_normal().generic_string(),
                    .data = {},
                    .srgb = srgb,
            };
        }

        [[nodiscard]] ModelTextureAsset BuildTextureAsset(const aiScene &scene, const std::string &model_path,
                                                          const aiString &texture_path, ModelTextureSemantic semantic,
                                                          bool srgb) {
            if (texture_path.length == 0) {
                return {};
            }

            const std::string raw_path = texture_path.C_Str();
            if (raw_path.empty()) {
                return {};
            }

            if (raw_path.front() == '*') {
                return BuildEmbeddedTextureAsset(scene, model_path, raw_path, semantic, srgb);
            }

            return BuildFileTextureAsset(model_path, raw_path, semantic, srgb);
        }

        [[nodiscard]] bool HasTextureSemantic(const ModelMaterialAsset &material, ModelTextureSemantic semantic) {
            return std::any_of(material.textures.begin(), material.textures.end(),
                               [semantic](const ModelTextureAsset &texture) { return texture.semantic == semantic; });
        }

        void AppendMaterialTextureIfPresent(const AssimpRuntime &runtime, const aiScene &scene,
                                            const aiMaterial &source, const std::string &model_path,
                                            ModelTextureSemantic semantic, aiTextureType assimp_type, bool srgb,
                                            ModelMaterialAsset &target) {
            if (HasTextureSemantic(target, semantic) ||
                runtime.get_material_texture_count(&source, assimp_type) == 0u) {
                return;
            }

            aiString texture_path{};
            if (runtime.get_material_texture(&source, assimp_type, 0u, &texture_path, nullptr, nullptr, nullptr,
                                             nullptr, nullptr, nullptr) != AI_SUCCESS) {
                return;
            }

            ModelTextureAsset texture = BuildTextureAsset(scene, model_path, texture_path, semantic, srgb);
            if (!texture.IsValid()) {
                return;
            }

            target.textures.push_back(std::move(texture));
        }

        [[nodiscard]] ModelMaterialAsset ConvertMaterial(const AssimpRuntime &runtime, const aiScene &scene,
                                                         const aiMaterial &source, std::uint32_t material_index,
                                                         const std::string &model_path) {
            ModelMaterialAsset target;
            target.name = MaterialName(runtime, source, material_index);

            aiColor4D color{};
            if (TryReadMaterialColor(runtime, source, AI_MATKEY_BASE_COLOR, color) ||
                TryReadMaterialColor(runtime, source, AI_MATKEY_COLOR_DIFFUSE, color)) {
                target.base_color = {color.r, color.g, color.b, color.a};
            }

            target.base_color.w = ReadMaterialFloat(runtime, source, AI_MATKEY_OPACITY, target.base_color.w);
            target.metallic = ReadMaterialFloat(runtime, source, AI_MATKEY_METALLIC_FACTOR, target.metallic);
            target.roughness = ReadMaterialFloat(runtime, source, AI_MATKEY_ROUGHNESS_FACTOR, target.roughness);

            AppendMaterialTextureIfPresent(runtime, scene, source, model_path, ModelTextureSemantic::BaseColor,
                                           aiTextureType_BASE_COLOR, true, target);
            AppendMaterialTextureIfPresent(runtime, scene, source, model_path, ModelTextureSemantic::BaseColor,
                                           aiTextureType_DIFFUSE, true, target);
            AppendMaterialTextureIfPresent(runtime, scene, source, model_path, ModelTextureSemantic::Normal,
                                           aiTextureType_NORMALS, false, target);
            AppendMaterialTextureIfPresent(runtime, scene, source, model_path, ModelTextureSemantic::Metallic,
                                           aiTextureType_METALNESS, false, target);
            AppendMaterialTextureIfPresent(runtime, scene, source, model_path, ModelTextureSemantic::Roughness,
                                           aiTextureType_DIFFUSE_ROUGHNESS, false, target);
            AppendMaterialTextureIfPresent(runtime, scene, source, model_path, ModelTextureSemantic::MetallicRoughness,
                                           aiTextureType_GLTF_METALLIC_ROUGHNESS, false, target);
            AppendMaterialTextureIfPresent(runtime, scene, source, model_path, ModelTextureSemantic::Emissive,
                                           aiTextureType_EMISSIVE, true, target);
            AppendMaterialTextureIfPresent(runtime, scene, source, model_path, ModelTextureSemantic::Occlusion,
                                           aiTextureType_AMBIENT_OCCLUSION, false, target);
            AppendMaterialTextureIfPresent(runtime, scene, source, model_path, ModelTextureSemantic::Occlusion,
                                           aiTextureType_LIGHTMAP, false, target);

            return target;
        }

        void EnsureDefaultMaterial(ModelAsset &asset) {
            if (!asset.materials.empty()) {
                return;
            }

            asset.materials.push_back(ModelMaterialAsset{
                    .name = "DefaultMaterial",
                    .base_color = {1.f, 1.f, 1.f, 1.f},
                    .metallic = 0.f,
                    .roughness = 1.f,
                    .textures = {},
            });
        }

        [[nodiscard]] bool ConvertMesh(const aiMesh &source, std::uint32_t mesh_index, std::uint32_t material_count,
                                       ModelMeshAsset &target, std::string &error_message) {
            if (!source.HasPositions() || !source.HasFaces()) {
                error_message = "Assimp mesh has no positions or faces";
                return false;
            }

            target.name = MeshName(source, mesh_index);
            target.material_index = source.mMaterialIndex < material_count ? source.mMaterialIndex : 0u;
            target.vertices.reserve(source.mNumVertices);
            target.indices.reserve(static_cast<std::size_t>(source.mNumFaces) * 3u);

            for (std::uint32_t vertex_index = 0; vertex_index < source.mNumVertices; ++vertex_index) {
                StaticMeshVertex vertex;
                vertex.position = ReadPosition(source.mVertices[vertex_index]);
                vertex.normal = ReadNormal(source, vertex_index);
                vertex.color = ReadColor(source, vertex_index);
                vertex.uv = ReadUv(source, vertex_index);
                vertex.custom0 = ReadTangent(source, vertex_index);
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

        [[nodiscard]] bool ConvertNodeHierarchy(const aiNode &source, std::uint32_t parent_index,
                                                std::size_t mesh_count, ModelAsset &asset, std::string &error_message) {
            if (asset.nodes.size() >= static_cast<std::size_t>(kInvalidModelNodeIndex)) {
                error_message = "Model node hierarchy exceeds the 32-bit node index limit";
                return false;
            }

            const auto node_index = static_cast<std::uint32_t>(asset.nodes.size());
            ModelNodeAsset node;
            node.name = NodeName(source, node_index);
            node.parent_index = parent_index;
            node.local_transform = ReadMatrix(source.mTransformation);
            node.mesh_indices.reserve(source.mNumMeshes);

            for (std::uint32_t mesh_slot = 0; mesh_slot < source.mNumMeshes; ++mesh_slot) {
                const std::uint32_t mesh_index = source.mMeshes[mesh_slot];
                if (mesh_index >= mesh_count) {
                    error_message = "Assimp node references an out-of-range mesh";
                    return false;
                }

                node.mesh_indices.push_back(mesh_index);
            }

            asset.nodes.push_back(std::move(node));

            for (std::uint32_t child_index = 0; child_index < source.mNumChildren; ++child_index) {
                const aiNode *child = source.mChildren[child_index];
                if (child == nullptr) {
                    error_message = "Assimp node hierarchy contains a null child";
                    return false;
                }

                if (!ConvertNodeHierarchy(*child, node_index, mesh_count, asset, error_message)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] bool ReserveMergedMesh(const std::vector<const ModelMeshAsset *> &sources, ModelMeshAsset &target,
                                             const std::string &path, std::string &error_message) {
            const auto max_index = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max());
            const auto max_container_size = static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
            std::uint64_t vertex_count = 0;
            std::uint64_t index_count = 0;

            for (const ModelMeshAsset *mesh: sources) {
                if (mesh == nullptr || !mesh->IsValid()) {
                    error_message = "Cannot merge an invalid model mesh: " + path;
                    return false;
                }

                const auto mesh_vertex_count = static_cast<std::uint64_t>(mesh->vertices.size());
                const auto mesh_index_count = static_cast<std::uint64_t>(mesh->indices.size());
                if (vertex_count + mesh_vertex_count > max_index + 1u ||
                    vertex_count + mesh_vertex_count > max_container_size ||
                    index_count + mesh_index_count > max_container_size) {
                    error_message = "Merged model mesh is too large: " + path;
                    return false;
                }

                vertex_count += mesh_vertex_count;
                index_count += mesh_index_count;
            }

            target.vertices.reserve(static_cast<std::size_t>(vertex_count));
            target.indices.reserve(static_cast<std::size_t>(index_count));
            return true;
        }

        [[nodiscard]] bool AppendMeshToMergedMesh(const ModelMeshAsset &source, ModelMeshAsset &target,
                                                  std::string &error_message) {
            if (!source.IsValid()) {
                error_message = "Cannot merge an invalid model mesh";
                return false;
            }

            const auto max_index = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max());
            const auto base_vertex = static_cast<std::uint64_t>(target.vertices.size());
            const auto source_vertex_count = static_cast<std::uint64_t>(source.vertices.size());

            if (base_vertex + source_vertex_count > max_index + 1u) {
                error_message = "Merged model mesh exceeds the 32-bit index limit";
                return false;
            }

            for (const std::uint32_t index: source.indices) {
                if (base_vertex + static_cast<std::uint64_t>(index) > max_index) {
                    error_message = "Merged model mesh contains an out-of-range vertex index";
                    return false;
                }
            }

            target.vertices.insert(target.vertices.end(), source.vertices.begin(), source.vertices.end());

            const auto index_offset = static_cast<std::uint32_t>(base_vertex);
            for (const std::uint32_t index: source.indices) {
                target.indices.push_back(index_offset + index);
            }

            return true;
        }

        [[nodiscard]] bool BuildMergedMesh(const std::vector<const ModelMeshAsset *> &sources, std::string name,
                                           std::uint32_t material_index, const std::string &path,
                                           ModelMeshAsset &merged, std::string &error_message) {
            merged.name = std::move(name);
            merged.material_index = material_index;
            if (!ReserveMergedMesh(sources, merged, path, error_message)) {
                return false;
            }

            for (const ModelMeshAsset *mesh: sources) {
                if (!AppendMeshToMergedMesh(*mesh, merged, error_message)) {
                    error_message += ": " + path;
                    return false;
                }
            }

            return merged.IsValid();
        }

        [[nodiscard]] std::string MergedMaterialMeshName(const ModelAsset &asset, std::uint32_t material_index) {
            if (material_index < asset.materials.size() && !asset.materials[material_index].name.empty()) {
                return "Merged_" + asset.materials[material_index].name;
            }

            return "MergedMaterial_" + std::to_string(material_index);
        }

        [[nodiscard]] bool MergeAllSubmeshes(ModelAsset &asset, const std::string &path, std::string &error_message) {
            std::vector<const ModelMeshAsset *> sources;
            sources.reserve(asset.meshes.size());
            for (const ModelMeshAsset &mesh: asset.meshes) {
                sources.push_back(&mesh);
            }

            ModelMeshAsset merged;
            const std::uint32_t material_index = sources.empty() ? 0u : sources.front()->material_index;
            if (!BuildMergedMesh(sources, "MergedModel", material_index, path, merged, error_message)) {
                return false;
            }

            asset.meshes.clear();
            asset.meshes.push_back(std::move(merged));
            return asset.IsValid();
        }

        [[nodiscard]] bool MergeSubmeshesByMaterial(ModelAsset &asset, const std::string &path,
                                                    std::string &error_message) {
            std::vector<std::uint32_t> material_order;
            material_order.reserve(asset.meshes.size());

            for (const ModelMeshAsset &mesh: asset.meshes) {
                if (std::find(material_order.begin(), material_order.end(), mesh.material_index) ==
                    material_order.end()) {
                    material_order.push_back(mesh.material_index);
                }
            }

            std::vector<ModelMeshAsset> merged_meshes;
            merged_meshes.reserve(material_order.size());
            for (const std::uint32_t material_index: material_order) {
                std::vector<const ModelMeshAsset *> sources;
                for (const ModelMeshAsset &mesh: asset.meshes) {
                    if (mesh.material_index == material_index) {
                        sources.push_back(&mesh);
                    }
                }

                ModelMeshAsset merged;
                if (!BuildMergedMesh(sources, MergedMaterialMeshName(asset, material_index), material_index, path,
                                     merged, error_message)) {
                    return false;
                }

                merged_meshes.push_back(std::move(merged));
            }

            asset.meshes = std::move(merged_meshes);
            return asset.IsValid();
        }

        [[nodiscard]] bool MergeSubmeshes(ModelAsset &asset, ModelMergeMode merge_mode, const std::string &path,
                                          std::string &error_message) {
            if (merge_mode == ModelMergeMode::None || asset.meshes.size() <= 1u) {
                return asset.IsValid();
            }

            if (merge_mode == ModelMergeMode::All) {
                return MergeAllSubmeshes(asset, path, error_message);
            }

            return MergeSubmeshesByMaterial(asset, path, error_message);
        }

        void BuildFlatMeshNodes(ModelAsset &asset) {
            asset.nodes.clear();
            asset.nodes.reserve(asset.meshes.size());

            for (std::uint32_t mesh_index = 0; mesh_index < asset.meshes.size(); ++mesh_index) {
                asset.nodes.push_back(ModelNodeAsset{
                        .name = asset.meshes[mesh_index].name.empty() ? "MeshNode_" + std::to_string(mesh_index)
                                                                      : asset.meshes[mesh_index].name,
                        .parent_index = kInvalidModelNodeIndex,
                        .local_transform = Math::Identity(),
                        .mesh_indices = {mesh_index},
                });
            }
        }

        [[nodiscard]] ModelLoadResult ConvertScene(const AssimpRuntime &runtime, const aiScene &scene,
                                                   const ModelLoadDesc &desc) {
            ModelLoadResult result;

            if ((scene.mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene.HasMeshes()) {
                result.error_message = "Assimp imported an incomplete scene: " + desc.path;
                return result;
            }

            if (desc.load_materials && scene.mMaterials != nullptr && scene.mNumMaterials > 0u) {
                result.asset.materials.reserve(scene.mNumMaterials);
                for (std::uint32_t material_index = 0; material_index < scene.mNumMaterials; ++material_index) {
                    const aiMaterial *material = scene.mMaterials[material_index];
                    if (material == nullptr) {
                        result.error_message = "Assimp scene contains a null material";
                        return result;
                    }

                    result.asset.materials.push_back(
                            ConvertMaterial(runtime, scene, *material, material_index, desc.path));
                }
            }
            EnsureDefaultMaterial(result.asset);

            result.asset.meshes.reserve(scene.mNumMeshes);
            const auto material_count = static_cast<std::uint32_t>(result.asset.materials.size());

            for (std::uint32_t mesh_index = 0; mesh_index < scene.mNumMeshes; ++mesh_index) {
                const aiMesh *mesh = scene.mMeshes[mesh_index];
                if (mesh == nullptr) {
                    result.error_message = "Assimp scene contains a null mesh";
                    return result;
                }

                ModelMeshAsset converted;
                if (!ConvertMesh(*mesh, mesh_index, material_count, converted, result.error_message)) {
                    result.error_message += ": " + desc.path;
                    return result;
                }

                result.asset.meshes.push_back(std::move(converted));
            }

            const ModelMergeMode merge_mode = ResolveMergeMode(desc);
            if (merge_mode == ModelMergeMode::None) {
                if (scene.mRootNode != nullptr &&
                    !ConvertNodeHierarchy(*scene.mRootNode, kInvalidModelNodeIndex, result.asset.meshes.size(),
                                          result.asset, result.error_message)) {
                    result.error_message += ": " + desc.path;
                    return result;
                }
            }

            if (!MergeSubmeshes(result.asset, merge_mode, desc.path, result.error_message)) {
                return result;
            }

            if (merge_mode != ModelMergeMode::None || result.asset.nodes.empty()) {
                BuildFlatMeshNodes(result.asset);
            }

            if (!result.asset.IsValid()) {
                result.error_message = "Assimp scene did not produce renderable meshes: " + desc.path;
            }

            return result;
        }
    } // namespace

    struct AssimpModelImporter::Impl {
        std::mutex mutex;
        AssimpRuntime runtime;
    };

    AssimpModelImporter::AssimpModelImporter() : impl_(std::make_unique<Impl>()) {}

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

        ModelLoadDesc import_desc = desc;
        if (import_desc.material_pipeline == ModelMaterialPipeline::PbrStandard) {
            import_desc.calculate_tangents = true;
        }

        const unsigned int flags = BuildPostProcessFlags(import_desc);
        SceneGuard scene{
                .scene = impl_->runtime.import_file(import_desc.path.c_str(), flags),
                .release = impl_->runtime.release_import,
        };

        if (scene.scene == nullptr) {
            const char *assimp_error = impl_->runtime.get_error_string();
            result.error_message = "Failed to import model '" + import_desc.path + "'";
            if (assimp_error != nullptr && assimp_error[0] != '\0') {
                result.error_message += ": ";
                result.error_message += assimp_error;
            }
            return result;
        }

        return ConvertScene(impl_->runtime, *scene.scene, import_desc);
    }
} // namespace CoreEngine
