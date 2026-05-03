#include <memory>

#include "imgui.h"

#include "core/math/math.h"

#include "player_controller.h"
#include "player_pawn.h"
#include "third_person_camera_controller.h"
#include "core/i_game_app.h"
#include "core/application/application.h"
#include "core/ecs/world.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/input/input_codes.h"
#include "core/input/input_system.h"
#include "core/log/log.h"
#include "core/math/math.h"
#include "core/render/camera.h"
#include "core/render/material.h"
#include "core/render/primitive_type.h"
#include "core/render/render_system.h"
#include "core/window/window_system.h"
#include "shaders/test_shader.h"

struct TestShaderProps {
    alignas(16) CoreEngine::Math::Vec4 color{1.f, 1.f, 1.f, 1.f};
    float alpha = 1.0f;
};

//
// struct FogParams {
//     CoreEngine::Math::Mat4 inv_view_proj{1.0f};
//     CoreEngine::Math::Vec4 camera_position_time{};
// };

// this example here serves just to demonstrates that the Render pass is working
// to try this just change the .stage from BeforeMainScene to BeforeImGui to see
// that all the main scene will change to a green color and the imgui panel will
// continue to be shown :)
// class DepthDebugPass final : public CoreEngine::IRenderPass {
// public:
//     [[nodiscard]] CoreEngine::RenderPassDesc Describe() const override {
//         return {
//             .name = "DepthDebugPass",
//             .stage = CoreEngine::RenderPassStage::UI,
//             .order = 0,
//         };
//     }
//
//     void Execute(CoreEngine::RenderPassContext &context) override {
//         if (!debug_target_.IsValid()) {
//             CoreEngine::FrameBufferDesc fb;
//             fb.width = context.SurfaceWidth();
//             fb.height = context.SurfaceHeight();
//             fb.has_color = true;
//             fb.sample_color = true;
//             fb.has_depth = false;
//             fb.color_format = CoreEngine::FrameBufferFormat::SwapChainColor;
//
//             debug_target_ = context.CreateFrameBuffer(fb);
//         }
//
//         if (!shader_.IsValid()) {
//             CoreEngine::ShaderProgramDesc shader;
//             shader.pixel_shader_source = R"HLSL(
// float3 RaymarchFog(float3 cameraPos, float3 worldPos, float3 sceneColor)
// {
//     float3 ray = worldPos - cameraPos;
//     float distanceToScene = length(ray);
//     float3 rayDir = ray / max(distanceToScene, 0.0001);
//
//     const int stepCount = 32;
//     float stepLength = distanceToScene / stepCount;
//
//     float3 fogColor = float3(0.55, 0.62, 0.70);
//     float baseDensity = 0.035;
//     float heightFalloff = 0.12;
//     float fogBaseHeight = 0.0;
//
//     float transmittance = 1.0;
//     float3 accumulatedFog = 0.0;
//
//     for (int i = 0; i < stepCount; ++i)
//     {
//         float t = (i + 0.5) * stepLength;
//         float3 samplePos = cameraPos + rayDir * t;
//
//         float heightDensity = exp(-(samplePos.y - fogBaseHeight) * heightFalloff);
//         float density = baseDensity * saturate(heightDensity);
//
//         float extinction = density * stepLength;
//         float stepTransmittance = exp(-extinction);
//
//         float fogAmount = 1.0 - stepTransmittance;
//
//         accumulatedFog += transmittance * fogAmount * fogColor;
//         transmittance *= stepTransmittance;
//
//         if (transmittance < 0.01)
//             break;
//     }
//
//     return sceneColor * transmittance + accumulatedFog;
// }
//
// Texture2D<float> g_SceneDepth;
// SamplerState g_SceneDepth_sampler;
//
// Texture2D<float4> g_SceneColor;
// SamplerState g_SceneColor_sampler;
//
// cbuffer FogParams
// {
//     float4x4 g_InvViewProj;
//     float4 g_CameraPositionTime;
// };
//
// struct VSOutput
// {
//     float4 pos : SV_POSITION;
//     float2 uv  : TEXCOORD0;
// };
//
// float4 main(VSOutput i) : SV_TARGET
// {
//     float2 uv = i.uv;
//
//     float depth = g_SceneDepth.Sample(g_SceneDepth_sampler, uv);
//     float4 clipPos = float4((uv.x * 2.0) - 1.0, 1.0 - (uv.y * 2.0), depth, 1.0);
//
//     float4 world = mul(g_InvViewProj, clipPos);
//     world.xyz /= world.w;
//
//     float4 sceneColor = g_SceneColor.Sample(g_SceneColor_sampler, uv);
//
//     float3 finalColor = RaymarchFog(g_CameraPositionTime.xyz, world.xyz, sceneColor.rgb);
//     return float4(finalColor, 1.0);
// }
// )HLSL";
//
//             shader.bindings = {
//                 CoreEngine::ShaderBindingDesc::Texture(
//                     "g_SceneDepth",
//                     CoreEngine::ShaderBindingScope::Pass,
//                     CoreEngine::ShaderStage::Pixel),
//
//                 CoreEngine::ShaderBindingDesc::Texture(
//                     "g_SceneColor",
//                     CoreEngine::ShaderBindingScope::Pass,
//                     CoreEngine::ShaderStage::Pixel),
//
//                 CoreEngine::ShaderBindingDesc::UniformBuffer(
//                     "FogParams",
//                     sizeof(FogParams),
//                     CoreEngine::ShaderBindingScope::Pass,
//                     CoreEngine::ShaderStage::Pixel),
//             };
//
//             shader_ = context.CreateShaderProgram(shader);
//         }
//
//         CoreEngine::FrameBufferDepthView scene_depth =
//                 context.GetGlobalDepthTexture(CoreEngine::GlobalTextureSlot::SceneDepth);
//
//         CoreEngine::FrameBufferColorView scene_color =
//                 context.GetGlobalColorTexture(CoreEngine::GlobalTextureSlot::SceneColor);
//
//         if (!scene_depth.IsValid() || !scene_color.IsValid() || !debug_target_.IsValid() || !shader_.IsValid()) {
//             return;
//         }
//
//         const FogParams fog_params = BuildFogParams(context);
//
//         context.SetFrameBuffer(debug_target_);
//         context.UseShaderProgram(shader_);
//         context.BindTexture("g_SceneDepth", scene_depth);
//         context.BindTexture("g_SceneColor", scene_color);
//         context.BindUniform("FogParams", fog_params);
//         context.DrawFullscreenTriangle();
//
//         CoreEngine::FrameBufferColorView color =
//                 context.GetFrameBufferColorView(debug_target_);
//
//         if (!color.IsValid()) {
//             return;
//         }
//
//         context.SetSwapChainFrameBuffer();
//
//         ImGui::Begin("Depth Debug");
//         ImGui::Image(color.native_handle, ImGui::GetContentRegionAvail());
//         ImGui::End();
//     }
//
//     void ReleaseResources(CoreEngine::IRenderBackend &backend) override {
//         if (debug_target_.IsValid()) {
//             backend.DestroyFrameBuffer(debug_target_);
//             debug_target_ = {};
//         }
//
//         if (shader_.IsValid()) {
//             backend.DestroyShaderProgram(shader_);
//             shader_ = {};
//         }
//     }
//
// private:
//     static FogParams BuildFogParams(CoreEngine::RenderPassContext &context) {
//         const CoreEngine::TransformComponent *active_transform = nullptr;
//         const CoreEngine::CameraComponent *active_camera = nullptr;
//
//         auto view = context.GetWorld().View<CoreEngine::TransformComponent, CoreEngine::CameraComponent>();
//         for (const auto &[entity, transform, camera]: view.each()) {
//             (void) entity;
//
//             if (!camera.enabled) {
//                 continue;
//             }
//
//             if (active_camera == nullptr || camera.priority > active_camera->priority) {
//                 active_camera = &camera;
//                 active_transform = &transform;
//             }
//         }
//
//         FogParams params;
//         if (active_transform == nullptr || active_camera == nullptr) {
//             return params;
//         }
//
//         const int width = context.SurfaceWidth() > 0 ? context.SurfaceWidth() : 1;
//         const int height = context.SurfaceHeight() > 0 ? context.SurfaceHeight() : 1;
//         const float aspect_ratio = active_camera->aspect_mode == CoreEngine::CameraAspectMode::Fixed
//                                        ? active_camera->fixed_aspect_ratio
//                                        : static_cast<float>(width) / static_cast<float>(height);
//
//         const CoreEngine::Math::Vec3 forward =
//                 active_transform->rotation * CoreEngine::Math::Vec3{0.0f, 0.0f, 1.0f};
//         const CoreEngine::Math::Vec3 up =
//                 active_transform->rotation * CoreEngine::Math::Vec3{0.0f, 1.0f, 0.0f};
//
//         const CoreEngine::Math::Mat4 view_matrix =
//                 CoreEngine::Math::LookAtLH(active_transform->position, active_transform->position + forward, up);
//
//         CoreEngine::Math::Mat4 projection_matrix{1.0f};
//         if (active_camera->projection_type == CoreEngine::CameraProjectionType::Perspective) {
//             projection_matrix = CoreEngine::Math::PerspectiveLH(
//                 CoreEngine::Math::Deg2Rad(active_camera->fov_y_degrees),
//                 aspect_ratio,
//                 active_camera->near_z,
//                 active_camera->far_z);
//         } else {
//             const float half_height = active_camera->orthographic_height * 0.5f;
//             const float half_width = half_height * aspect_ratio;
//             projection_matrix = CoreEngine::Math::OrthoLH(
//                 -half_width,
//                 half_width,
//                 -half_height,
//                 half_height,
//                 active_camera->near_z,
//                 active_camera->far_z);
//         }
//
//         params.inv_view_proj = CoreEngine::Math::Inverse(projection_matrix * view_matrix);
//         params.camera_position_time = CoreEngine::Math::Vec4(
//             active_transform->position.x,
//             active_transform->position.y,
//             active_transform->position.z,
//             static_cast<float>(context.TotalSeconds()));
//         return params;
//     }
//
//     CoreEngine::FrameBufferHandle debug_target_{};
//     CoreEngine::ShaderProgramHandle shader_{};
// };

class MyGameApp final : public CoreEngine::IGameApp {
public:
    MyGameApp() = default;

    void Init(const CoreEngine::EngineContext &context) override {
        if (!player_controller_.Init(context)) {
            CoreEngine::Log::Warn("Game", "Failed to bind one or more player input actions");
        }

        // depth_pass = context.render_system.AddRenderPass(
        //     std::make_unique<DepthDebugPass>());

        const CoreEngine::MeshHandle cube_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Cube);
        const CoreEngine::MeshHandle plane_mesh =
                context.render_system.GetOrCreatePrimitive(CoreEngine::PrimitiveType::Plane);
        const CoreEngine::MaterialHandle cube_material =
                CoreEngine::Material::Unlit({.color = {1.2f, 0.6f, 1.0f, 1.0f}}).Resolve(context.render_system);

        TestShaderProps shader_props;
        shader_props.color = {1.0, 0.0, 0.0, 1.0};
        shader_props.alpha = 0.1f;

        auto custom_material = CoreEngine::Material::Custom(Game::Shaders::kTestVS, Game::Shaders::kTestPS,
                                                            shader_props);

        const CoreEngine::MaterialHandle player_material = custom_material.Resolve(context.render_system);

        const CoreEngine::MaterialHandle plane_material =
                CoreEngine::Material::Unlit({.color = {0.5f, 0.6f, 1.0f, 1.0f}}).Resolve(context.render_system);

        player_pawn_ = Game::PlayerPawn(
            context.world.CreateNode("Player"),
            Game::MovementComponent{
                .crouch_speed = 1.5f,
                .walk_speed = 3.0f,
                .run_speed = 6.0f,
                .default_movement_type = Game::MovementType::Walk,
            });

        player_pawn_.Node().SetPosition(CoreEngine::Math::Vec3(2.0f, 0.0f, 0.0f));
        player_pawn_.Node().AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = cube_mesh,
                .material = player_material,
            });


        CoreEngine::CameraComponent camera{.priority = 1, .enabled = false};
        camera_node_ = context.world.CreateNode("MainCamera");
        secondary_camera_node_ = context.world.CreateNode("SecondaryCamera");
        secondary_camera_node_.AddComponent<CoreEngine::CameraComponent>(camera);
        secondary_camera_node_.AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = cube_mesh,
                .material = player_material,
            });

        camera_node_.AddComponent<CoreEngine::CameraComponent>();
        camera_node_.SetPosition({0.0f, 1.5f, -4.0f});
        camera_node_.AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = cube_mesh,
                .material = player_material,
            });

        third_person_camera_controller_.Attach(camera_node_, player_pawn_.Node());
        third_person_camera_controller_.SetFocusOffset({0.0f, 1.25f, 0.0f});
        third_person_camera_controller_.SetDistance(4.0f);

        player_controller_.AttachCameraController(third_person_camera_controller_);
        player_controller_.Possess(player_pawn_);

        plane_node_ = context.world.CreateNode("Plane");
        plane_node_.AddComponent<CoreEngine::MeshRendererComponent>(
            CoreEngine::MeshRendererComponent{
                .mesh = plane_mesh,
                .material = plane_material,
            });

        plane_node_.SetPosition({0.f, -0.75f, 0.f});
        plane_node_.SetScale({3.f, 1.f, 3.f});
        plane_node_.SetRotation(
            CoreEngine::Math::AngleAxis(CoreEngine::Math::Deg2Rad(180.0f), CoreEngine::Math::Vec3(0.0f, 0.f, 1.f)));

        ApplyCursorMode(context.window_system, CoreEngine::WindowCursorMode::CURSOR_NORMAL);
    }

    void Update(const CoreEngine::FrameContext &frame) override {
        if (frame.input_system.WasKeyPressed(CoreEngine::Key::Escape)) {
            CoreEngine::Application::RequestShutdown();
        }

        player_controller_.Update(frame);
        RenderDebugUi(frame);

        auto &editor_camera = secondary_camera_node_.GetComponent<CoreEngine::CameraComponent>();

        if (frame.input_system.WasKeyPressed(CoreEngine::Key::Tab)) {
            switch (current_cursor_mode_) {
                case CoreEngine::WindowCursorMode::CURSOR_NORMAL:
                    ApplyCursorMode(frame.window_system, CoreEngine::WindowCursorMode::CURSOR_CONSTRAINED_AND_HIDDEN);
                    editor_camera.enabled = false;
                    editor_camera.priority = 0;
                    break;
                case CoreEngine::WindowCursorMode::CURSOR_CONSTRAINED_AND_HIDDEN:
                default:
                    ApplyCursorMode(frame.window_system, CoreEngine::WindowCursorMode::CURSOR_NORMAL);
                    editor_camera.enabled = true;
                    editor_camera.priority = 10;
                    break;
            }
        }
    }

    void Shutdown(const CoreEngine::EngineContext &context) override {
        // context.render_system.RemoveRenderPass(depth_pass);
        // depth_pass = {};
        player_controller_.DetachCameraController();
        player_controller_.Unpossess();
    }

private:
    void RenderDebugUi(const CoreEngine::FrameContext &frame) {
        if (ImGui::GetCurrentContext() == nullptr) {
            return;
        }

        ImGui::SetNextWindowPos(ImVec2{16.0f, 16.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2{360.0f, 0.0f}, ImGuiCond_FirstUseEver);

        if (ImGui::Begin("CoreEngine Debug")) {
            ImGui::Text("FPS: %.0f", 1.0 / frame.delta_time);
        }
        ImGui::End();
    }

    void ApplyCursorMode(CoreEngine::WindowSystem &window_system, CoreEngine::WindowCursorMode cursor_mode) {
        if (window_system.SetWindowCursorMode(cursor_mode)) {
            current_cursor_mode_ = cursor_mode;
        }
    }

    CoreEngine::Node plane_node_;
    CoreEngine::Node camera_node_;
    CoreEngine::Node secondary_camera_node_;
    Game::ThirdPersonCameraController third_person_camera_controller_;
    Game::PlayerPawn player_pawn_;
    Game::PlayerController player_controller_;
    CoreEngine::WindowCursorMode current_cursor_mode_ = CoreEngine::WindowCursorMode::CURSOR_NORMAL;
    float debug_value_ = 0.5f;
    float debug_color_[3] = {0.25f, 0.55f, 0.9f};
    int debug_counter_ = 0;
    // CoreEngine::RenderPassHandle depth_pass{};
};

int main() {
    std::unique_ptr<CoreEngine::IGameApp> app = std::make_unique<MyGameApp>();

    CoreEngine::EngineConfig config;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.fullscreen = false;
    config.decorated = true;
    config.resizable = true;
    config.windowTitle = "CoreEngine - Player Input Demo";
    config.renderBackend = CoreEngine::RenderBackendType::DiligentVulkan;
    config.vsync = false;
    config.enableImGui = true;

    return CoreEngine::RunEngine(std::move(app), config);
}
