#include "core/editor/editor_asset_registry.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <system_error>
#include <unordered_set>

namespace CoreEngine {
    namespace {
        [[nodiscard]] std::filesystem::path NormalizePath(const std::filesystem::path &path) {
            std::error_code error;
            std::filesystem::path normalized = std::filesystem::weakly_canonical(path, error);
            if (!error) {
                return normalized;
            }

            normalized = std::filesystem::absolute(path, error);
            return error ? path : normalized;
        }

        [[nodiscard]] std::string LowerExtension(const std::filesystem::path &path) {
            std::string extension = path.extension().string();
            std::ranges::transform(extension, extension.begin(), [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
            return extension;
        }

        [[nodiscard]] bool ExtensionIn(const std::string &extension, std::span<const char *const> values) {
            return std::ranges::find(values, extension) != values.end();
        }
    } // namespace

    void EditorAssetRegistry::SetRoots(std::vector<std::filesystem::path> roots) {
        roots_.clear();
        roots_.reserve(roots.size());

        std::unordered_set<std::string> unique_roots;
        for (const std::filesystem::path &root: roots) {
            if (root.empty()) {
                continue;
            }

            std::filesystem::path normalized = NormalizePath(root);
            const std::string key = normalized.generic_string();
            if (unique_roots.insert(key).second) {
                roots_.push_back(std::move(normalized));
            }
        }
    }

    void EditorAssetRegistry::Refresh() {
        assets_.clear();
        last_error_.clear();

        for (const std::filesystem::path &root: roots_) {
            std::error_code error;
            if (!std::filesystem::exists(root, error) || !std::filesystem::is_directory(root, error)) {
                continue;
            }

            const auto options = std::filesystem::directory_options::skip_permission_denied;
            std::filesystem::recursive_directory_iterator it{root, options, error};
            const std::filesystem::recursive_directory_iterator end;
            while (!error && it != end) {
                const std::filesystem::directory_entry &entry = *it;
                if (entry.is_regular_file(error)) {
                    std::filesystem::path relative = std::filesystem::relative(entry.path(), root, error);
                    if (error) {
                        relative = entry.path().filename();
                        error.clear();
                    }

                    std::uintmax_t size_bytes = entry.file_size(error);
                    if (error) {
                        size_bytes = 0;
                        error.clear();
                    }

                    assets_.push_back(EditorAssetRecord{
                        .root = root,
                        .path = entry.path(),
                        .relative_path = std::move(relative),
                        .kind = Classify(entry.path()),
                        .size_bytes = size_bytes,
                    });
                }

                it.increment(error);
            }

            if (error && last_error_.empty()) {
                last_error_ = error.message();
                error.clear();
            }
        }

        std::ranges::sort(assets_, [](const EditorAssetRecord &lhs, const EditorAssetRecord &rhs) {
            if (lhs.kind != rhs.kind) {
                return lhs.kind < rhs.kind;
            }

            return lhs.relative_path.generic_string() < rhs.relative_path.generic_string();
        });
    }

    std::span<const EditorAssetRecord> EditorAssetRegistry::Assets() const noexcept {
        return assets_;
    }

    std::span<const std::filesystem::path> EditorAssetRegistry::Roots() const noexcept {
        return roots_;
    }

    EditorAssetKind EditorAssetRegistry::Classify(const std::filesystem::path &path) {
        const std::string extension = LowerExtension(path);

        constexpr std::array model_extensions{".obj", ".fbx", ".gltf", ".glb"};
        if (ExtensionIn(extension, model_extensions)) {
            return EditorAssetKind::Model;
        }

        constexpr std::array texture_extensions{".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr", ".dds"};
        if (ExtensionIn(extension, texture_extensions)) {
            return EditorAssetKind::Texture;
        }

        constexpr std::array shader_extensions{".hlsl", ".glsl", ".vert", ".frag", ".comp"};
        if (ExtensionIn(extension, shader_extensions)) {
            return EditorAssetKind::Shader;
        }

        constexpr std::array audio_extensions{".wav", ".ogg", ".mp3"};
        if (ExtensionIn(extension, audio_extensions)) {
            return EditorAssetKind::Audio;
        }

        constexpr std::array scene_extensions{".scene", ".level", ".world"};
        if (ExtensionIn(extension, scene_extensions)) {
            return EditorAssetKind::Scene;
        }

        return EditorAssetKind::Other;
    }

    const char *EditorAssetRegistry::ToString(EditorAssetKind kind) noexcept {
        switch (kind) {
            case EditorAssetKind::Model:
                return "Model";
            case EditorAssetKind::Texture:
                return "Texture";
            case EditorAssetKind::Shader:
                return "Shader";
            case EditorAssetKind::Audio:
                return "Audio";
            case EditorAssetKind::Scene:
                return "Scene";
            case EditorAssetKind::Other:
                return "Other";
        }

        return "Unknown";
    }
} // namespace CoreEngine
