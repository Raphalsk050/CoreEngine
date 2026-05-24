#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace CoreEngine {
    enum class EditorAssetKind {
        Model,
        Texture,
        Shader,
        Audio,
        Scene,
        Other,
    };

    struct EditorAssetRecord {
        std::filesystem::path root;
        std::filesystem::path path;
        std::filesystem::path relative_path;
        EditorAssetKind kind = EditorAssetKind::Other;
        std::uintmax_t size_bytes = 0;
    };

    /**
     * @brief Indexes project assets for editor browsing and import commands.
     *
     * Responsibility: scan configured asset roots on demand, classify files by
     * extension, and expose immutable records to the UI without tying asset
     * browsing to the renderer or resource lifetime systems.
     */
    class EditorAssetRegistry final {
    public:
        void SetRoots(std::vector<std::filesystem::path> roots);

        void Refresh();

        [[nodiscard]] std::span<const EditorAssetRecord> Assets() const noexcept;

        [[nodiscard]] std::span<const std::filesystem::path> Roots() const noexcept;

        [[nodiscard]] const std::string &LastError() const noexcept {
            return last_error_;
        }

        [[nodiscard]] static EditorAssetKind Classify(const std::filesystem::path &path);

        [[nodiscard]] static const char *ToString(EditorAssetKind kind) noexcept;

    private:
        std::vector<std::filesystem::path> roots_;
        std::vector<EditorAssetRecord> assets_;
        std::string last_error_;
    };
} // namespace CoreEngine
