#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "core/assets/model_asset.h"
#include "core/editor/editor_asset_registry.h"
#include "core/editor/editor_command_history.h"
#include "core/editor/editor_log_sink.h"
#include "core/input/input_codes.h"
#include "core/math/math.h"
#include "core/render/primitive_type.h"
#include "core/render/render_handle.h"

namespace CoreEngine {
    struct FrameContext;

    enum class EditorPlayState {
        Editing,
        Playing,
        Paused,
    };

    enum class EditorTool {
        Select,
        Translate,
        Rotate,
        Scale,
    };

    struct EditorSystemDesc {
        std::filesystem::path project_root;
        std::vector<std::filesystem::path> asset_roots;
        std::shared_ptr<EditorLogSink> log_sink;
    };

    /**
     * @brief Owns the in-engine authoring editor UI and editor-only state.
     *
     * Responsibility: render and coordinate editor panels, scene selection,
     * asset browsing/import, undo/redo, play-state gating, and editor camera
     * control without moving gameplay logic into the engine editor layer.
     */
    class EditorSystem final {
    public:
        struct TransformSnapshot {
            Math::Vec3 position{0.0f, 0.0f, 0.0f};
            Math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
            Math::Vec3 scale{1.0f, 1.0f, 1.0f};
        };

        EditorSystem();

        ~EditorSystem();

        EditorSystem(const EditorSystem &) = delete;

        EditorSystem &operator=(const EditorSystem &) = delete;

        void Initialize(EditorSystemDesc desc);

        void Shutdown();

        void Render(const FrameContext &frame);

        [[nodiscard]] bool ShouldRunGame() const noexcept;

        [[nodiscard]] bool ConsumeSingleStepRequest() noexcept;

        [[nodiscard]] EditorPlayState PlayState() const noexcept {
            return play_state_;
        }

    private:
        struct PendingModelImport {
            ModelHandle handle;
            std::filesystem::path path;
            std::string instance_name;
        };

        void ConfigureImGui();

        void RenderDockspace();

        void RenderMainMenu(const FrameContext &frame);

        void RenderToolbar(const FrameContext &frame);

        void RenderSceneHierarchy(const FrameContext &frame);

        void RenderNodeTree(const FrameContext &frame, entt::entity entity);

        void RenderInspector(const FrameContext &frame);

        void RenderAssetBrowser(const FrameContext &frame);

        void RenderConsole();

        void RenderViewportOverlay(const FrameContext &frame);

        void RenderMetricsPanel(const FrameContext &frame);

        void RenderSettingsPanel();

        void HandleShortcuts(const FrameContext &frame);

        void UpdateEditorCamera(const FrameContext &frame);

        void DrawSelectionDebug(const FrameContext &frame);

        void PumpPendingImports(const FrameContext &frame);

        void RefreshAssets();

        void Select(entt::entity entity) noexcept;

        void ClearSelection() noexcept;

        [[nodiscard]] entt::entity SelectedEntity(const FrameContext &frame) const noexcept;

        [[nodiscard]] bool HasSelection(const FrameContext &frame) const noexcept;

        void CreateEmptyNode(const FrameContext &frame);

        void CreateCameraNode(const FrameContext &frame);

        void CreatePrimitiveNode(const FrameContext &frame, PrimitiveType type);

        void DeleteSelectedNode(const FrameContext &frame);

        void ImportModelAsset(const FrameContext &frame, const EditorAssetRecord &asset);

        void ExecuteUndo(const FrameContext &frame);

        void ExecuteRedo(const FrameContext &frame);

        void SetPlayState(EditorPlayState state) noexcept;

        [[nodiscard]] EditorCommandContext CommandContext(const FrameContext &frame) const noexcept;

        [[nodiscard]] static const char *ToString(EditorPlayState state) noexcept;

        [[nodiscard]] static const char *ToString(EditorTool tool) noexcept;

        std::filesystem::path project_root_;
        EditorAssetRegistry asset_registry_;
        std::shared_ptr<EditorLogSink> log_sink_;
        EditorCommandHistory command_history_;
        std::vector<PendingModelImport> pending_model_imports_;

        EditorPlayState play_state_ = EditorPlayState::Editing;
        EditorTool active_tool_ = EditorTool::Select;
        entt::entity selected_entity_ = entt::null;
        entt::entity inspected_entity_ = entt::null;

        std::array<char, 160> name_buffer_{};
        std::array<char, 128> asset_filter_buffer_{};
        std::string name_before_edit_;
        EditorAssetKind asset_filter_kind_ = EditorAssetKind::Other;
        int selected_asset_index_ = -1;
        bool show_all_asset_kinds_ = true;
        bool auto_scroll_console_ = true;
        bool transform_edit_active_ = false;
        TransformSnapshot transform_before_edit_{};

        bool show_scene_hierarchy_ = true;
        bool show_inspector_ = true;
        bool show_asset_browser_ = true;
        bool show_console_ = true;
        bool show_metrics_ = true;
        bool show_settings_ = false;

        bool editor_camera_enabled_ = true;
        bool editor_camera_override_during_play_ = false;
        float editor_camera_speed_ = 8.0f;
        float editor_camera_yaw_ = 0.0f;
        float editor_camera_pitch_ = 0.0f;
        Math::Vec3 editor_camera_position_{0.0f, 2.0f, -8.0f};

        bool single_step_requested_ = false;
        bool initialized_ = false;
    };
} // namespace CoreEngine
