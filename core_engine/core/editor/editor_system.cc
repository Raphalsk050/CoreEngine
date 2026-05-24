#include "core/editor/editor_system.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>

#include "imgui.h"

#include "core/application/application.h"
#include "core/application/frame_context.h"
#include "core/debug/debug_draw.h"
#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/hierarchy_component.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/name_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/node.h"
#include "core/ecs/world.h"
#include "core/input/input_system.h"
#include "core/log/log.h"
#include "core/math/math.h"
#include "core/render/camera.h"
#include "core/render/material.h"
#include "core/render/render_system.h"

namespace CoreEngine {
    namespace {
        constexpr const char *kNodeDragPayload = "CoreEngine.Editor.Node";

        [[nodiscard]] bool IsValidEntity(World &world, entt::entity entity) noexcept {
            return entity != entt::null && world.Registry().valid(entity);
        }

        [[nodiscard]] Node ToNode(World &world, entt::entity entity) noexcept {
            return IsValidEntity(world, entity) ? Node{entity, &world} : Node{};
        }

        [[nodiscard]] EditorSystem::TransformSnapshot CaptureTransform(Node node) {
            if (!node.IsValid()) {
                return {};
            }

            const TransformComponent &transform = node.GetComponent<TransformComponent>();
            return EditorSystem::TransformSnapshot{
                .position = transform.Position(),
                .rotation = transform.Rotation(),
                .scale = transform.Scale(),
            };
        }

        void ApplyTransform(Node node, const EditorSystem::TransformSnapshot &snapshot) {
            if (!node.IsValid()) {
                return;
            }

            TransformComponent &transform = node.GetComponent<TransformComponent>();
            transform.SetPosition(snapshot.position);
            transform.SetRotation(Math::Normalize(snapshot.rotation));
            transform.SetScale(snapshot.scale);
        }

        [[nodiscard]] bool SameTransform(const EditorSystem::TransformSnapshot &lhs,
                                         const EditorSystem::TransformSnapshot &rhs) noexcept {
            return lhs.position.x == rhs.position.x &&
                   lhs.position.y == rhs.position.y &&
                   lhs.position.z == rhs.position.z &&
                   lhs.rotation.w == rhs.rotation.w &&
                   lhs.rotation.x == rhs.rotation.x &&
                   lhs.rotation.y == rhs.rotation.y &&
                   lhs.rotation.z == rhs.rotation.z &&
                   lhs.scale.x == rhs.scale.x &&
                   lhs.scale.y == rhs.scale.y &&
                   lhs.scale.z == rhs.scale.z;
        }

        [[nodiscard]] bool ContainsCaseInsensitive(std::string_view text, std::string_view filter) {
            if (filter.empty()) {
                return true;
            }

            auto it = std::search(text.begin(),
                                  text.end(),
                                  filter.begin(),
                                  filter.end(),
                                  [](char lhs, char rhs) {
                                      return std::tolower(static_cast<unsigned char>(lhs)) ==
                                             std::tolower(static_cast<unsigned char>(rhs));
                                  });
            return it != text.end();
        }

        [[nodiscard]] const char *LogLevelName(LogLevel level) noexcept {
            switch (level) {
                case LogLevel::Debug:
                    return "Debug";
                case LogLevel::Info:
                    return "Info";
                case LogLevel::Warn:
                    return "Warn";
                case LogLevel::Error:
                    return "Error";
                case LogLevel::Fatal:
                    return "Fatal";
            }

            return "Unknown";
        }

        [[nodiscard]] ImVec4 LogLevelColor(LogLevel level) noexcept {
            switch (level) {
                case LogLevel::Debug:
                    return {0.62f, 0.66f, 0.72f, 1.0f};
                case LogLevel::Info:
                    return {0.62f, 0.86f, 0.68f, 1.0f};
                case LogLevel::Warn:
                    return {0.98f, 0.76f, 0.35f, 1.0f};
                case LogLevel::Error:
                case LogLevel::Fatal:
                    return {1.0f, 0.42f, 0.38f, 1.0f};
            }

            return {1.0f, 1.0f, 1.0f, 1.0f};
        }

        [[nodiscard]] MaterialHandle ResolveEditorMaterial(RenderSystem &render_system, const Math::Vec4 &color) {
            return Material::Unlit(UnlitProps{.color = color}).Resolve(render_system);
        }

        [[nodiscard]] const char *PrimitiveName(PrimitiveType type) noexcept {
            switch (type) {
                case PrimitiveType::Cube:
                    return "Cube";
                case PrimitiveType::Plane:
                    return "Plane";
                case PrimitiveType::Quad:
                    return "Quad";
                case PrimitiveType::Sphere:
                    return "Sphere";
                case PrimitiveType::Count:
                    break;
            }

            return "Primitive";
        }

        class RenameNodeCommand final : public IEditorCommand {
        public:
            RenameNodeCommand(entt::entity entity, std::string before, std::string after)
                : entity_(entity), before_(std::move(before)), after_(std::move(after)) {
            }

            [[nodiscard]] std::string_view Name() const noexcept override {
                return "Rename Node";
            }

            [[nodiscard]] bool Execute(EditorCommandContext &context) override {
                return Apply(context.world, after_);
            }

            [[nodiscard]] bool Undo(EditorCommandContext &context) override {
                return Apply(context.world, before_);
            }

        private:
            [[nodiscard]] bool Apply(World &world, const std::string &name) const {
                Node node = ToNode(world, entity_);
                if (!node.IsValid()) {
                    return false;
                }

                NameComponent &component = node.GetComponent<NameComponent>();
                component.name = name.empty() ? "Node" : name;
                return true;
            }

            entt::entity entity_ = entt::null;
            std::string before_;
            std::string after_;
        };

        class SetNodeTransformCommand final : public IEditorCommand {
        public:
            SetNodeTransformCommand(entt::entity entity,
                                    EditorSystem::TransformSnapshot before,
                                    EditorSystem::TransformSnapshot after)
                : entity_(entity), before_(before), after_(after) {
            }

            [[nodiscard]] std::string_view Name() const noexcept override {
                return "Set Transform";
            }

            [[nodiscard]] bool Execute(EditorCommandContext &context) override {
                return Apply(context.world, after_);
            }

            [[nodiscard]] bool Undo(EditorCommandContext &context) override {
                return Apply(context.world, before_);
            }

        private:
            [[nodiscard]] bool Apply(World &world, const EditorSystem::TransformSnapshot &snapshot) const {
                Node node = ToNode(world, entity_);
                if (!node.IsValid()) {
                    return false;
                }

                ApplyTransform(node, snapshot);
                return true;
            }

            entt::entity entity_ = entt::null;
            EditorSystem::TransformSnapshot before_{};
            EditorSystem::TransformSnapshot after_{};
        };
    } // namespace

    EditorSystem::EditorSystem() : command_history_(128) {
    }

    EditorSystem::~EditorSystem() = default;

    void EditorSystem::Initialize(EditorSystemDesc desc) {
        project_root_ = desc.project_root;
        if (project_root_.empty()) {
            std::error_code error;
            project_root_ = std::filesystem::current_path(error);
            if (error) {
                project_root_.clear();
            }
        }

        std::vector<std::filesystem::path> roots = std::move(desc.asset_roots);
        if (roots.empty() && !project_root_.empty()) {
            roots.push_back(project_root_ / "app" / "assets");
            roots.push_back(project_root_ / "core_engine" / "assets");
        }

        log_sink_ = std::move(desc.log_sink);
        asset_registry_.SetRoots(std::move(roots));
        RefreshAssets();
        initialized_ = true;

        Log::Info("Editor", "CoreEngine editor initialized");
    }

    void EditorSystem::Shutdown() {
        pending_model_imports_.clear();
        command_history_.Clear();
        selected_entity_ = entt::null;
        inspected_entity_ = entt::null;
        initialized_ = false;
    }

    void EditorSystem::Render(const FrameContext &frame) {
        if (!initialized_ || ImGui::GetCurrentContext() == nullptr) {
            return;
        }

        ConfigureImGui();
        HandleShortcuts(frame);
        PumpPendingImports(frame);
        UpdateEditorCamera(frame);
        DrawSelectionDebug(frame);

        RenderDockspace();
        RenderMainMenu(frame);
        RenderToolbar(frame);

        if (show_scene_hierarchy_) {
            RenderSceneHierarchy(frame);
        }
        if (show_inspector_) {
            RenderInspector(frame);
        }
        if (show_asset_browser_) {
            RenderAssetBrowser(frame);
        }
        if (show_console_) {
            RenderConsole();
        }
        if (show_metrics_) {
            RenderMetricsPanel(frame);
        }
        if (show_settings_) {
            RenderSettingsPanel();
        }

        RenderViewportOverlay(frame);
    }

    bool EditorSystem::ShouldRunGame() const noexcept {
        return play_state_ == EditorPlayState::Playing;
    }

    bool EditorSystem::ConsumeSingleStepRequest() noexcept {
        const bool requested = single_step_requested_;
        single_step_requested_ = false;
        return requested;
    }

    void EditorSystem::ConfigureImGui() {
        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 3.0f;
    }

    void EditorSystem::RenderDockspace() {
        // This vendored ImGui build does not expose docking. Keep this hook so
        // the editor layout can adopt docking later without touching Runtime.
    }

    void EditorSystem::RenderMainMenu(const FrameContext &frame) {
        if (!ImGui::BeginMainMenuBar()) {
            return;
        }

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Empty Node", "Ctrl+N")) {
                CreateEmptyNode(frame);
            }
            if (ImGui::MenuItem("Refresh Assets", "F5")) {
                RefreshAssets();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                Application::RequestShutdown();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (!command_history_.CanUndo()) {
                ImGui::BeginDisabled();
            }
            const std::string undo_label = command_history_.CanUndo()
                                               ? std::string{"Undo "} + std::string{command_history_.UndoName()}
                                               : "Undo";
            if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z")) {
                ExecuteUndo(frame);
            }
            if (!command_history_.CanUndo()) {
                ImGui::EndDisabled();
            }

            if (!command_history_.CanRedo()) {
                ImGui::BeginDisabled();
            }
            const std::string redo_label = command_history_.CanRedo()
                                               ? std::string{"Redo "} + std::string{command_history_.RedoName()}
                                               : "Redo";
            if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y")) {
                ExecuteRedo(frame);
            }
            if (!command_history_.CanRedo()) {
                ImGui::EndDisabled();
            }

            ImGui::Separator();
            if (!HasSelection(frame)) {
                ImGui::BeginDisabled();
            }
            if (ImGui::MenuItem("Delete Selected", "Del")) {
                DeleteSelectedNode(frame);
            }
            if (!HasSelection(frame)) {
                ImGui::EndDisabled();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Empty Node")) {
                CreateEmptyNode(frame);
            }
            if (ImGui::MenuItem("Camera")) {
                CreateCameraNode(frame);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cube")) {
                CreatePrimitiveNode(frame, PrimitiveType::Cube);
            }
            if (ImGui::MenuItem("Plane")) {
                CreatePrimitiveNode(frame, PrimitiveType::Plane);
            }
            if (ImGui::MenuItem("Quad")) {
                CreatePrimitiveNode(frame, PrimitiveType::Quad);
            }
            if (ImGui::MenuItem("Sphere")) {
                CreatePrimitiveNode(frame, PrimitiveType::Sphere);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Play")) {
            if (ImGui::MenuItem("Play", "F6", false, play_state_ != EditorPlayState::Playing)) {
                SetPlayState(EditorPlayState::Playing);
            }
            if (ImGui::MenuItem("Pause", "F7", false, play_state_ == EditorPlayState::Playing)) {
                SetPlayState(EditorPlayState::Paused);
            }
            if (ImGui::MenuItem("Stop", "F8", false, play_state_ != EditorPlayState::Editing)) {
                SetPlayState(EditorPlayState::Editing);
            }
            if (ImGui::MenuItem("Step", "F10", false, play_state_ != EditorPlayState::Playing)) {
                play_state_ = EditorPlayState::Paused;
                single_step_requested_ = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Scene Hierarchy", nullptr, &show_scene_hierarchy_);
            ImGui::MenuItem("Inspector", nullptr, &show_inspector_);
            ImGui::MenuItem("Asset Browser", nullptr, &show_asset_browser_);
            ImGui::MenuItem("Console", nullptr, &show_console_);
            ImGui::MenuItem("Metrics", nullptr, &show_metrics_);
            ImGui::MenuItem("Settings", nullptr, &show_settings_);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    void EditorSystem::RenderToolbar(const FrameContext &frame) {
        (void) frame;
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (!ImGui::Begin("Editor Toolbar", nullptr, flags)) {
            ImGui::End();
            return;
        }

        if (play_state_ == EditorPlayState::Playing) {
            if (ImGui::Button("Pause")) {
                SetPlayState(EditorPlayState::Paused);
            }
        } else if (ImGui::Button("Play")) {
            SetPlayState(EditorPlayState::Playing);
        }

        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            SetPlayState(EditorPlayState::Editing);
        }
        ImGui::SameLine();
        if (ImGui::Button("Step")) {
            play_state_ = EditorPlayState::Paused;
            single_step_requested_ = true;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        for (EditorTool tool: {EditorTool::Select, EditorTool::Translate, EditorTool::Rotate, EditorTool::Scale}) {
            const bool selected = active_tool_ == tool;
            if (ImGui::Selectable(ToString(tool), selected, 0, ImVec2{82.0f, 0.0f})) {
                active_tool_ = tool;
            }
            ImGui::SameLine();
        }

        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Checkbox("Editor Camera", &editor_camera_enabled_);

        ImGui::End();
    }

    void EditorSystem::RenderSceneHierarchy(const FrameContext &frame) {
        World &world = frame.world;
        if (!ImGui::Begin("Scene Hierarchy", &show_scene_hierarchy_)) {
            ImGui::End();
            return;
        }

        if (ImGui::Button("+ Empty")) {
            CreateEmptyNode(frame);
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Camera")) {
            CreateCameraNode(frame);
        }
        ImGui::SameLine();
        if (!HasSelection(frame)) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Delete")) {
            DeleteSelectedNode(frame);
        }
        if (!HasSelection(frame)) {
            ImGui::EndDisabled();
        }

        ImGui::Separator();

        auto view = world.Registry().view<NameComponent>();
        for (const entt::entity entity: view) {
            const HierarchyComponent *hierarchy = world.TryGetComponent<HierarchyComponent>(entity);
            if (hierarchy != nullptr && hierarchy->parent != entt::null && world.Registry().valid(hierarchy->parent)) {
                continue;
            }

            RenderNodeTree(frame, entity);
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kNodeDragPayload)) {
                if (payload->DataSize == sizeof(entt::entity)) {
                    const entt::entity dropped = *static_cast<const entt::entity *>(payload->Data);
                    Node node = ToNode(world, dropped);
                    if (node.IsValid()) {
                        node.ClearParent(true);
                        Select(dropped);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::End();
    }

    void EditorSystem::RenderNodeTree(const FrameContext &frame, entt::entity entity) {
        World &world = frame.world;
        Node node = ToNode(world, entity);
        if (!node.IsValid()) {
            return;
        }

        const std::string name = node.GetName();
        const bool selected = selected_entity_ == entity;
        const bool leaf = node.GetChildCount() == 0;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (leaf) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const auto id = static_cast<std::uintptr_t>(static_cast<std::uint32_t>(entity));
        const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void *>(id), flags, "%s", name.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            Select(entity);
        }

        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(kNodeDragPayload, &entity, sizeof(entity));
            ImGui::TextUnformatted(name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kNodeDragPayload)) {
                if (payload->DataSize == sizeof(entt::entity)) {
                    const entt::entity dropped = *static_cast<const entt::entity *>(payload->Data);
                    Node dropped_node = ToNode(world, dropped);
                    if (dropped_node.IsValid() && dropped_node.SetParent(node, true)) {
                        Select(dropped);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (!leaf && opened) {
            constexpr std::uint32_t kMaxRenderedChildren = 4096u;
            std::uint32_t rendered_children = 0;
            const HierarchyComponent *hierarchy = node.TryGetComponent<HierarchyComponent>();
            entt::entity child = hierarchy == nullptr ? entt::null : hierarchy->first_child;
            while (child != entt::null && world.Registry().valid(child) && rendered_children < kMaxRenderedChildren) {
                RenderNodeTree(frame, child);
                const HierarchyComponent *child_hierarchy = world.TryGetComponent<HierarchyComponent>(child);
                child = child_hierarchy == nullptr ? entt::null : child_hierarchy->next_sibling;
                ++rendered_children;
            }
            ImGui::TreePop();
        }
    }

    void EditorSystem::RenderInspector(const FrameContext &frame) {
        World &world = frame.world;
        if (!ImGui::Begin("Inspector", &show_inspector_)) {
            ImGui::End();
            return;
        }

        const entt::entity selected = SelectedEntity(frame);
        if (selected == entt::null) {
            ImGui::TextDisabled("No node selected.");
            ImGui::End();
            return;
        }

        Node node = ToNode(world, selected);
        if (!node.IsValid()) {
            ClearSelection();
            ImGui::TextDisabled("Selected node is no longer valid.");
            ImGui::End();
            return;
        }

        if (inspected_entity_ != selected) {
            inspected_entity_ = selected;
            const std::string name = node.GetName();
            std::snprintf(name_buffer_.data(), name_buffer_.size(), "%s", name.c_str());
            name_before_edit_ = name;
        }

        ImGui::Text("Node ID: %u", node.Id());
        if (ImGui::InputText("Name", name_buffer_.data(), name_buffer_.size())) {
            if (name_buffer_[0] == '\0') {
                std::snprintf(name_buffer_.data(), name_buffer_.size(), "%s", "Node");
            }
        }
        if (ImGui::IsItemActivated()) {
            name_before_edit_ = node.GetName();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string next_name{name_buffer_.data()};
            if (next_name.empty()) {
                next_name = "Node";
            }
            if (next_name != name_before_edit_) {
                EditorCommandContext command_context = CommandContext(frame);
                (void) command_history_.Execute(
                    command_context,
                    std::make_unique<RenameNodeCommand>(selected, name_before_edit_, next_name));
            }
        }

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            TransformComponent &transform = node.GetComponent<TransformComponent>();
            Math::Vec3 position = transform.Position();
            Math::Quat rotation = transform.Rotation();
            Math::Vec3 scale = transform.Scale();

            auto capture_before_edit = [&]() {
                if (!transform_edit_active_) {
                    transform_before_edit_ = CaptureTransform(node);
                    transform_edit_active_ = true;
                }
            };

            const bool position_changed = ImGui::DragFloat3("Position", Math::ValuePtr(position), 0.05f);
            if (ImGui::IsItemActivated()) {
                capture_before_edit();
            }
            if (position_changed) {
                transform.SetPosition(position);
            }

            const bool rotation_changed = ImGui::DragFloat4("Rotation (Quat)", Math::ValuePtr(rotation), 0.01f);
            if (ImGui::IsItemActivated()) {
                capture_before_edit();
            }
            if (rotation_changed) {
                transform.SetRotation(Math::Normalize(rotation));
            }

            const bool scale_changed = ImGui::DragFloat3("Scale", Math::ValuePtr(scale), 0.05f, 0.001f, 1000.0f);
            if (ImGui::IsItemActivated()) {
                capture_before_edit();
            }
            if (scale_changed) {
                transform.SetScale(scale);
            }

            if (transform_edit_active_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                const TransformSnapshot after = CaptureTransform(node);
                if (!SameTransform(transform_before_edit_, after)) {
                    EditorCommandContext command_context = CommandContext(frame);
                    (void) command_history_.Execute(
                        command_context,
                        std::make_unique<SetNodeTransformCommand>(
                            selected,
                            transform_before_edit_,
                            after));
                }
                transform_edit_active_ = false;
            }
        }

        if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
            Node parent = node.GetParent();
            ImGui::Text("Parent: %s", parent.IsValid() ? parent.GetName().c_str() : "<root>");
            ImGui::Text("Children: %u", node.GetChildCount());
            if (parent.IsValid() && ImGui::Button("Unparent")) {
                node.ClearParent(true);
            }
        }

        CameraComponent *camera = node.TryGetComponent<CameraComponent>();
        if (ImGui::CollapsingHeader("Camera", camera != nullptr ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            if (camera == nullptr) {
                if (ImGui::Button("Add Camera Component")) {
                    node.AddComponent<CameraComponent>();
                }
            } else {
                const char *projection_items[] = {"Perspective", "Orthographic"};
                int projection_index = camera->projection_type == CameraProjectionType::Perspective ? 0 : 1;
                if (ImGui::Combo("Projection", &projection_index, projection_items, 2)) {
                    camera->projection_type = projection_index == 0
                                                  ? CameraProjectionType::Perspective
                                                  : CameraProjectionType::Orthographic;
                }
                ImGui::DragFloat("FOV Y", &camera->fov_y_degrees, 0.25f, 1.0f, 179.0f);
                ImGui::DragFloat("Ortho Height", &camera->orthographic_height, 0.1f, 0.01f, 10000.0f);
                ImGui::DragFloat("Near Z", &camera->near_z, 0.001f, 0.001f, camera->far_z - 0.001f);
                ImGui::DragFloat("Far Z", &camera->far_z, 1.0f, camera->near_z + 0.001f, 100000.0f);
                ImGui::InputInt("Priority", &camera->priority);
                ImGui::Checkbox("Enabled", &camera->enabled);
                if (ImGui::Button("Remove Camera Component")) {
                    node.RemoveComponent<CameraComponent>();
                }
            }
        }

        MeshRendererComponent *renderer = node.TryGetComponent<MeshRendererComponent>();
        if (ImGui::CollapsingHeader("Mesh Renderer", renderer != nullptr ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            if (renderer == nullptr) {
                if (ImGui::Button("Add Cube Renderer")) {
                    const MeshHandle mesh = frame.render_system.GetOrCreatePrimitive(PrimitiveType::Cube);
                    const MaterialHandle material = ResolveEditorMaterial(frame.render_system, {0.8f, 0.82f, 0.9f, 1.0f});
                    if (mesh.IsValid() && material.IsValid()) {
                        node.AddComponent<MeshRendererComponent>(MeshRendererComponent{
                            .mesh = mesh,
                            .material = material,
                            .visible = true,
                        });
                    }
                }
            } else {
                ImGui::Checkbox("Visible", &renderer->visible);
                ImGui::Checkbox("Cast Shadows", &renderer->cast_shadows);
                ImGui::Text("Mesh: %u:%u", renderer->mesh.id, renderer->mesh.generation);
                ImGui::Text("Material: %u:%u", renderer->material.id, renderer->material.generation);
                for (PrimitiveType type: {PrimitiveType::Cube, PrimitiveType::Plane, PrimitiveType::Quad, PrimitiveType::Sphere}) {
                    if (ImGui::Button(PrimitiveName(type))) {
                        MeshHandle mesh = frame.render_system.GetOrCreatePrimitive(type);
                        if (mesh.IsValid()) {
                            renderer->mesh = mesh;
                        }
                    }
                    ImGui::SameLine();
                }
                ImGui::NewLine();
                if (ImGui::Button("Remove Mesh Renderer")) {
                    node.RemoveComponent<MeshRendererComponent>();
                }
            }
        }

        ImGui::End();
    }

    void EditorSystem::RenderAssetBrowser(const FrameContext &frame) {
        if (!ImGui::Begin("Asset Browser", &show_asset_browser_)) {
            ImGui::End();
            return;
        }

        if (ImGui::Button("Refresh")) {
            RefreshAssets();
        }
        ImGui::SameLine();
        ImGui::InputText("Filter", asset_filter_buffer_.data(), asset_filter_buffer_.size());

        ImGui::SameLine();
        const char *preview = show_all_asset_kinds_ ? "All" : EditorAssetRegistry::ToString(asset_filter_kind_);
        if (ImGui::BeginCombo("Type", preview)) {
            if (ImGui::Selectable("All", show_all_asset_kinds_)) {
                show_all_asset_kinds_ = true;
            }
            for (EditorAssetKind kind: {EditorAssetKind::Model,
                                        EditorAssetKind::Texture,
                                        EditorAssetKind::Shader,
                                        EditorAssetKind::Audio,
                                        EditorAssetKind::Scene,
                                        EditorAssetKind::Other}) {
                const bool selected = !show_all_asset_kinds_ && asset_filter_kind_ == kind;
                if (ImGui::Selectable(EditorAssetRegistry::ToString(kind), selected)) {
                    show_all_asset_kinds_ = false;
                    asset_filter_kind_ = kind;
                }
            }
            ImGui::EndCombo();
        }

        const std::span<const EditorAssetRecord> assets = asset_registry_.Assets();
        const std::string_view filter{asset_filter_buffer_.data()};

        if (ImGui::BeginTable("Assets", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Root");
            ImGui::TableHeadersRow();

            int visible_index = 0;
            for (std::size_t i = 0; i < assets.size(); ++i) {
                const EditorAssetRecord &asset = assets[i];
                const std::string relative = asset.relative_path.generic_string();
                if (!show_all_asset_kinds_ && asset.kind != asset_filter_kind_) {
                    continue;
                }
                if (!ContainsCaseInsensitive(relative, filter)) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const bool selected = selected_asset_index_ == static_cast<int>(i);
                if (ImGui::Selectable(relative.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_asset_index_ = static_cast<int>(i);
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                    asset.kind == EditorAssetKind::Model) {
                    ImportModelAsset(frame, asset);
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(EditorAssetRegistry::ToString(asset.kind));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%llu KB", static_cast<unsigned long long>((asset.size_bytes + 1023u) / 1024u));
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(asset.root.filename().generic_string().c_str());
                ++visible_index;
            }

            (void) visible_index;
            ImGui::EndTable();
        }

        const bool has_selected_asset = selected_asset_index_ >= 0 &&
                                        selected_asset_index_ < static_cast<int>(assets.size());
        if (!has_selected_asset ||
            assets[static_cast<std::size_t>(selected_asset_index_)].kind != EditorAssetKind::Model) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Import Selected Model") && has_selected_asset) {
            ImportModelAsset(frame, assets[static_cast<std::size_t>(selected_asset_index_)]);
        }
        if (!has_selected_asset ||
            assets[static_cast<std::size_t>(selected_asset_index_)].kind != EditorAssetKind::Model) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::Text("%llu assets", static_cast<unsigned long long>(assets.size()));
        if (!asset_registry_.LastError().empty()) {
            ImGui::TextColored(ImVec4{1.0f, 0.42f, 0.38f, 1.0f}, "Scan warning: %s", asset_registry_.LastError().c_str());
        }

        ImGui::End();
    }

    void EditorSystem::RenderConsole() {
        if (!ImGui::Begin("Console", &show_console_)) {
            ImGui::End();
            return;
        }

        if (log_sink_ == nullptr) {
            ImGui::TextDisabled("Editor log sink is not available.");
            ImGui::End();
            return;
        }

        if (ImGui::Button("Clear")) {
            log_sink_->Clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &auto_scroll_console_);
        ImGui::SameLine();
        ImGui::Text("Dropped: %llu", static_cast<unsigned long long>(log_sink_->DroppedCount()));

        ImGui::Separator();

        const std::vector<EditorLogEntry> entries = log_sink_->Snapshot();
        ImGui::BeginChild("ConsoleScroll", ImVec2{0.0f, 0.0f}, true, ImGuiWindowFlags_HorizontalScrollbar);
        for (const EditorLogEntry &entry: entries) {
            ImGui::PushStyleColor(ImGuiCol_Text, LogLevelColor(entry.level));
            ImGui::TextUnformatted(LogLevelName(entry.level));
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text("[%s] %s", entry.category.c_str(), entry.text.c_str());
        }
        if (auto_scroll_console_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        ImGui::End();
    }

    void EditorSystem::RenderViewportOverlay(const FrameContext &frame) {
        ImGuiIO &io = ImGui::GetIO();
        const ImVec2 padding{12.0f, 36.0f};
        ImGui::SetNextWindowPos(ImVec2{io.DisplaySize.x - 320.0f - padding.x, padding.y}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.42f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("Viewport Overlay", nullptr, flags)) {
            ImGui::Text("State: %s", ToString(play_state_));
            ImGui::Text("Tool: %s", ToString(active_tool_));
            ImGui::Text("Frame: %.2f ms", frame.delta_time * 1000.0f);
            ImGui::Text("Nodes: %llu", static_cast<unsigned long long>(frame.world.GetNodeCount()));
            ImGui::Text("Pending imports: %llu", static_cast<unsigned long long>(pending_model_imports_.size()));
        }
        ImGui::End();
    }

    void EditorSystem::RenderMetricsPanel(const FrameContext &frame) {
        if (!ImGui::Begin("Editor Metrics", &show_metrics_)) {
            ImGui::End();
            return;
        }

        const ImGuiIO &io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Delta: %.3f ms", frame.delta_time * 1000.0f);
        ImGui::Text("World nodes: %llu", static_cast<unsigned long long>(frame.world.GetNodeCount()));
        ImGui::Text("Assets indexed: %llu", static_cast<unsigned long long>(asset_registry_.Assets().size()));
        ImGui::Text("Pending model imports: %llu", static_cast<unsigned long long>(pending_model_imports_.size()));
        ImGui::Text("Undo: %s", command_history_.CanUndo() ? command_history_.UndoName().data() : "<empty>");
        ImGui::Text("Redo: %s", command_history_.CanRedo() ? command_history_.RedoName().data() : "<empty>");

        ImGui::End();
    }

    void EditorSystem::RenderSettingsPanel() {
        if (!ImGui::Begin("Editor Settings", &show_settings_)) {
            ImGui::End();
            return;
        }

        ImGui::Text("Project root:");
        ImGui::TextWrapped("%s", project_root_.generic_string().c_str());
        ImGui::Separator();
        ImGui::Checkbox("Editor camera", &editor_camera_enabled_);
        ImGui::Checkbox("Override camera while playing", &editor_camera_override_during_play_);
        ImGui::DragFloat("Camera speed", &editor_camera_speed_, 0.25f, 0.1f, 500.0f);
        ImGui::DragFloat3("Camera position", Math::ValuePtr(editor_camera_position_), 0.1f);
        ImGui::DragFloat("Camera yaw", &editor_camera_yaw_, 0.01f);
        ImGui::DragFloat("Camera pitch", &editor_camera_pitch_, 0.01f, -1.45f, 1.45f);

        ImGui::Separator();
        ImGui::Text("Asset roots:");
        for (const std::filesystem::path &root: asset_registry_.Roots()) {
            ImGui::BulletText("%s", root.generic_string().c_str());
        }

        ImGui::End();
    }

    void EditorSystem::HandleShortcuts(const FrameContext &frame) {
        ImGuiIO &io = ImGui::GetIO();
        if (io.WantTextInput) {
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
            CreateEmptyNode(frame);
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            ExecuteUndo(frame);
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            ExecuteRedo(frame);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            DeleteSelectedNode(frame);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
            RefreshAssets();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) {
            SetPlayState(EditorPlayState::Playing);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) {
            SetPlayState(EditorPlayState::Paused);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F8, false)) {
            SetPlayState(EditorPlayState::Editing);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F10, false)) {
            play_state_ = EditorPlayState::Paused;
            single_step_requested_ = true;
        }
    }

    void EditorSystem::UpdateEditorCamera(const FrameContext &frame) {
        if (!editor_camera_enabled_ ||
            (play_state_ == EditorPlayState::Playing && !editor_camera_override_during_play_)) {
            frame.render_system.ClearCameraOverride();
            return;
        }

        InputSystem &input = frame.input_system;
        ImGuiIO &io = ImGui::GetIO();
        const bool camera_input_active = input.IsMouseButtonDown(MouseButton::Right) && !io.WantTextInput;

        const float mouse_sensitivity = 0.0035f;
        if (camera_input_active) {
            const InputVector2 mouse_delta = input.MouseDelta();
            editor_camera_yaw_ += mouse_delta.x * mouse_sensitivity;
            editor_camera_pitch_ = std::clamp(editor_camera_pitch_ - mouse_delta.y * mouse_sensitivity,
                                              -1.45f,
                                              1.45f);
        }

        const float cos_pitch = std::cos(editor_camera_pitch_);
        const Math::Vec3 forward = Math::Normalize(Math::Vec3{
            std::sin(editor_camera_yaw_) * cos_pitch,
            std::sin(editor_camera_pitch_),
            std::cos(editor_camera_yaw_) * cos_pitch,
        });
        Math::Vec3 right = Math::Cross(Math::Vec3{0.0f, 1.0f, 0.0f}, forward);
        if (Math::LengthSquared(right) <= 1.0e-6f) {
            right = {1.0f, 0.0f, 0.0f};
        } else {
            right = Math::Normalize(right);
        }
        const Math::Vec3 up = Math::Normalize(Math::Cross(forward, right));

        if (camera_input_active) {
            const float speed_multiplier = input.IsKeyDown(Key::LeftShift) || input.IsKeyDown(Key::RightShift)
                                               ? 4.0f
                                               : 1.0f;
            const float step = editor_camera_speed_ * speed_multiplier * std::max(frame.delta_time, 0.0f);
            if (input.IsKeyDown(Key::W)) {
                editor_camera_position_ += forward * step;
            }
            if (input.IsKeyDown(Key::S)) {
                editor_camera_position_ = editor_camera_position_ - forward * step;
            }
            if (input.IsKeyDown(Key::D)) {
                editor_camera_position_ += right * step;
            }
            if (input.IsKeyDown(Key::A)) {
                editor_camera_position_ = editor_camera_position_ - right * step;
            }
            if (input.IsKeyDown(Key::E)) {
                editor_camera_position_ += Math::Vec3{0.0f, 1.0f, 0.0f} * step;
            }
            if (input.IsKeyDown(Key::Q)) {
                editor_camera_position_ = editor_camera_position_ - Math::Vec3{0.0f, 1.0f, 0.0f} * step;
            }
        }

        const ImVec2 display_size = io.DisplaySize;
        const float aspect = display_size.y > 1.0f ? display_size.x / display_size.y : 16.0f / 9.0f;
        Camera camera;
        camera.LookAt(editor_camera_position_, editor_camera_position_ + forward, up)
              .Perspective(60.0f, aspect, 0.01f, 2000.0f);
        frame.render_system.SetCamera(camera);
    }

    void EditorSystem::DrawSelectionDebug(const FrameContext &frame) {
        const entt::entity selected = SelectedEntity(frame);
        if (selected == entt::null) {
            return;
        }

        Node node = ToNode(frame.world, selected);
        if (!node.IsValid()) {
            return;
        }

        const Math::Vec3 position = node.GetWorldPosition();
        const Math::Vec3 scale = node.GetWorldScale();
        const Math::Vec3 half_extents{
            std::max(std::abs(scale.x), 0.25f) * 0.5f,
            std::max(std::abs(scale.y), 0.25f) * 0.5f,
            std::max(std::abs(scale.z), 0.25f) * 0.5f,
        };

        frame.debug_draw.DrawBox(position,
                                 half_extents,
                                 node.GetWorldRotation(),
                                 DebugDrawStyle{
                                     .color = {1.0f, 0.82f, 0.18f, 1.0f},
                                     .duration_seconds = 0.0f,
                                     .depth_test = false,
                                 });
    }

    void EditorSystem::PumpPendingImports(const FrameContext &frame) {
        for (auto it = pending_model_imports_.begin(); it != pending_model_imports_.end();) {
            const ModelLoadState state = frame.render_system.GetModelLoadState(it->handle);
            if (state == ModelLoadState::Ready) {
                ModelInstance instance = frame.render_system.InstantiateModel(
                    frame.world,
                    it->handle,
                    {},
                    ModelInstantiationDesc{.root_name = it->instance_name, .visible = true});
                if (instance.IsValid()) {
                    Select(instance.root.Handle());
                    Log::Info("Editor", "Imported model '{}'", it->path.generic_string());
                }
                it = pending_model_imports_.erase(it);
                continue;
            }

            if (state == ModelLoadState::Failed || state == ModelLoadState::Invalid) {
                const std::string error = frame.render_system.GetModelLoadError(it->handle);
                Log::Warn("Editor",
                          "Failed to import model '{}': {}",
                          it->path.generic_string(),
                          error.empty() ? "unknown error" : error);
                it = pending_model_imports_.erase(it);
                continue;
            }

            ++it;
        }
    }

    void EditorSystem::RefreshAssets() {
        asset_registry_.Refresh();
        selected_asset_index_ = -1;
        Log::Info("Editor",
                  "Indexed {} editor assets",
                  static_cast<unsigned long long>(asset_registry_.Assets().size()));
    }

    void EditorSystem::Select(entt::entity entity) noexcept {
        selected_entity_ = entity;
    }

    void EditorSystem::ClearSelection() noexcept {
        selected_entity_ = entt::null;
        inspected_entity_ = entt::null;
    }

    entt::entity EditorSystem::SelectedEntity(const FrameContext &frame) const noexcept {
        return IsValidEntity(frame.world, selected_entity_) ? selected_entity_ : entt::null;
    }

    bool EditorSystem::HasSelection(const FrameContext &frame) const noexcept {
        return SelectedEntity(frame) != entt::null;
    }

    void EditorSystem::CreateEmptyNode(const FrameContext &frame) {
        Node node = frame.world.CreateNode("Empty Node");
        Select(node.Handle());
        Log::Info("Editor", "Created empty node {}", node.Id());
    }

    void EditorSystem::CreateCameraNode(const FrameContext &frame) {
        Node node = frame.world.CreateNode("Camera");
        node.SetPosition({0.0f, 2.0f, -5.0f});
        node.AddComponent<CameraComponent>();
        Select(node.Handle());
        Log::Info("Editor", "Created camera node {}", node.Id());
    }

    void EditorSystem::CreatePrimitiveNode(const FrameContext &frame, PrimitiveType type) {
        const MeshHandle mesh = frame.render_system.GetOrCreatePrimitive(type);
        const MaterialHandle material = ResolveEditorMaterial(frame.render_system, {0.78f, 0.80f, 0.86f, 1.0f});
        if (!mesh.IsValid() || !material.IsValid()) {
            Log::Warn("Editor", "Could not create primitive node because render resources are unavailable");
            return;
        }

        Node node = frame.world.CreateNode(PrimitiveName(type));
        node.AddComponent<MeshRendererComponent>(MeshRendererComponent{
            .mesh = mesh,
            .material = material,
            .visible = true,
        });
        Select(node.Handle());
        Log::Info("Editor", "Created {} primitive node {}", PrimitiveName(type), node.Id());
    }

    void EditorSystem::DeleteSelectedNode(const FrameContext &frame) {
        Node node = ToNode(frame.world, SelectedEntity(frame));
        if (!node.IsValid()) {
            ClearSelection();
            return;
        }

        const std::string name = node.GetName();
        const std::uint32_t id = node.Id();
        frame.world.DestroyNode(node);
        ClearSelection();
        Log::Info("Editor", "Deleted node '{}' ({})", name, id);
    }

    void EditorSystem::ImportModelAsset(const FrameContext &frame, const EditorAssetRecord &asset) {
        if (asset.kind != EditorAssetKind::Model) {
            return;
        }

        ModelHandle handle = frame.render_system.LoadModelAsync(ModelLoadDesc{
            .path = asset.path.string(),
            .triangulate = true,
            .join_identical_vertices = true,
            .generate_normals = true,
            .calculate_tangents = false,
            .convert_to_left_handed = true,
            .flip_uvs = false,
            .merge_submeshes = false,
            .merge_mode = ModelMergeMode::None,
            .load_materials = true,
        });
        if (!handle.IsValid()) {
            Log::Warn("Editor", "Could not start model import '{}'", asset.path.generic_string());
            return;
        }

        pending_model_imports_.push_back(PendingModelImport{
            .handle = handle,
            .path = asset.path,
            .instance_name = asset.path.stem().string(),
        });
        Log::Info("Editor", "Queued model import '{}'", asset.path.generic_string());
    }

    void EditorSystem::ExecuteUndo(const FrameContext &frame) {
        EditorCommandContext command_context = CommandContext(frame);
        if (!command_history_.Undo(command_context)) {
            return;
        }
        Log::Debug("Editor", "Undo executed");
    }

    void EditorSystem::ExecuteRedo(const FrameContext &frame) {
        EditorCommandContext command_context = CommandContext(frame);
        if (!command_history_.Redo(command_context)) {
            return;
        }
        Log::Debug("Editor", "Redo executed");
    }

    void EditorSystem::SetPlayState(EditorPlayState state) noexcept {
        play_state_ = state;
    }

    EditorCommandContext EditorSystem::CommandContext(const FrameContext &frame) const noexcept {
        return EditorCommandContext{
            .world = frame.world,
            .render_system = frame.render_system,
        };
    }

    const char *EditorSystem::ToString(EditorPlayState state) noexcept {
        switch (state) {
            case EditorPlayState::Editing:
                return "Editing";
            case EditorPlayState::Playing:
                return "Playing";
            case EditorPlayState::Paused:
                return "Paused";
        }

        return "Unknown";
    }

    const char *EditorSystem::ToString(EditorTool tool) noexcept {
        switch (tool) {
            case EditorTool::Select:
                return "Select";
            case EditorTool::Translate:
                return "Translate";
            case EditorTool::Rotate:
                return "Rotate";
            case EditorTool::Scale:
                return "Scale";
        }

        return "Unknown";
    }
} // namespace CoreEngine
