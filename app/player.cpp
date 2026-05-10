#include "player.h"

#include "core/application/engine_context.h"
#include "core/application/frame_context.h"
#include "core/assets/model_asset.h"
#include "core/ecs/components/camera_component.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/ecs/world.h"
#include "core/log/log.h"
#include "core/render/material.h"
#include "core/render/render_system.h"
#include "core/render/texture_desc.h"

namespace Game {
    namespace {
        struct PlayerShaderProps {
            alignas(16) CoreEngine::Math::Vec4 color{1.f, 1.f, 1.f, 1.f};
            float alpha = 1.0f;
        };

        constexpr const char *kPlayerModelPath = "app/assets/models/mandalorian_armored.glb";
        constexpr const char *kPlayerTexturePath = "app/assets/textures/uv_mapping.png";
        constexpr const char *kPlayerVertexShaderPath = "app/assets/shaders/custom_shader_vertex.hlsl";
        constexpr const char *kPlayerPixelShaderPath = "app/assets/shaders/custom_shader_pixel.hlsl";

        [[nodiscard]] MovementComponent MakeDefaultMovementComponent() noexcept {
            return MovementComponent{
                .crouch_speed = 1.5f,
                .walk_speed = 3.0f,
                .run_speed = 6.0f,
                .default_movement_type = MovementType::Walk,
            };
        }
    }

    bool Player::Initialize(const CoreEngine::EngineContext &context) {
        const bool input_bound = player_controller_.Init(context);
        if (!input_bound) {
            CoreEngine::Log::Warn("Game", "Failed to bind one or more player input actions");
        }

        CreatePawn(context.world);
        LoadPlayerTexture(context.render_system);

        const CoreEngine::MaterialHandle player_material = LoadPlayerMaterial(context.render_system);
        LoadPlayerModel(context, player_material);

        CreateCamera(context.world);
        AttachController();

        initialized_ = true;
        return input_bound;
    }

    void Player::Update(const CoreEngine::FrameContext &frame) {
        if (!initialized_) {
            return;
        }

        player_controller_.Update(frame);
    }

    void Player::Shutdown() {
        player_controller_.DetachCameraController();
        player_controller_.Unpossess();
        initialized_ = false;
    }

    void Player::CreatePawn(CoreEngine::World &world) {
        player_pawn_ = PlayerPawn(world.CreateNode("Player"), MakeDefaultMovementComponent());
        player_pawn_.Node().SetPosition(CoreEngine::Math::Vec3(2.0f, 0.0f, 0.0f));
        player_renderer_node_ = world.CreateNode("PlayerRendererNode");
        player_renderer_node_.SetParent(player_pawn_.Node());
        player_renderer_node_.SetRotation(
            CoreEngine::Math::AngleAxis(CoreEngine::Math::Deg2Rad(180.0f), {0.0, 1.0, 0.0}));
    }

    void Player::CreateCamera(CoreEngine::World &world) {
        camera_node_ = world.CreateNode("MainCamera");
        camera_node_.AddComponent<CoreEngine::CameraComponent>();
        camera_node_.SetPosition({0.0f, 1.5f, -4.0f});

        third_person_camera_controller_.Attach(camera_node_, player_pawn_.Node());
        third_person_camera_controller_.SetFocusOffset({0.0f, 1.25f, 0.0f});
        third_person_camera_controller_.SetDistance(4.0f);
    }

    void Player::LoadPlayerTexture(CoreEngine::RenderSystem &render_system) {
        player_texture_ = render_system.LoadTexture2DAsync(CoreEngine::TextureLoadDesc{
            .path = kPlayerTexturePath,
            .format = CoreEngine::TextureFormat::RGBA8Unorm,
            .generate_mipmaps = true
        });
    }

    CoreEngine::MaterialHandle Player::LoadPlayerMaterial(CoreEngine::RenderSystem &render_system) const {
        PlayerShaderProps shader_props;
        shader_props.color = {1.0f, 0.0f, 0.0f, 1.0f};
        shader_props.alpha = 0.1f;

        CoreEngine::Material player_material = CoreEngine::MaterialBuilder{}
                .Vertex(kPlayerVertexShaderPath, true)
                .Pixel(kPlayerPixelShaderPath, true)
                .Texture("g_Albedo", player_texture_)
                .Properties(shader_props)
                .Build();

        return player_material.Resolve(render_system);
    }

    void Player::LoadPlayerModel(const CoreEngine::EngineContext &context,
                                 CoreEngine::MaterialHandle player_material) {
        CoreEngine::Future<CoreEngine::ModelHandle> model = context.render_system.LoadModelAsyncFuture(
            CoreEngine::ModelLoadDesc{
                .path = kPlayerModelPath,
                .merge_submeshes = true
            });

        model.Then([this, &render_system = context.render_system, player_material](
        const CoreEngine::FutureResult<CoreEngine::ModelHandle> &result) {
                if (!result.IsSuccess()) {
                    CoreEngine::Log::Error("Game", "Failed to load player model: {}", result.ErrorMessage());
                    return;
                }

                const CoreEngine::MeshHandle mesh = render_system.GetModelMesh(result.Value(), 0);
                if (!mesh.IsValid()) {
                    CoreEngine::Log::Error("Game", "Player model loaded without a valid mesh");
                    return;
                }

                AddPlayerComponents(mesh, player_material);
            });
    }

    void Player::AddPlayerComponents(CoreEngine::MeshHandle mesh,
                                     CoreEngine::MaterialHandle material) {
        if (!player_renderer_node_.IsValid()) {
            return;
        }

        CoreEngine::MeshRendererComponent renderer{
            .mesh = mesh,
            .material = material,
        };

        if (player_renderer_node_.HasComponent<CoreEngine::MeshRendererComponent>()) {
            player_renderer_node_.GetComponent<CoreEngine::MeshRendererComponent>() = renderer;
            return;
        }

        player_renderer_node_.AddComponent<CoreEngine::MeshRendererComponent>(renderer);
    }

    void Player::AttachController() {
        player_controller_.AttachCameraController(third_person_camera_controller_);
        player_controller_.Possess(player_pawn_);
    }
} // namespace Game
