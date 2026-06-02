#include <memory>

#include "imgui.h"
#include "core/i_game_app.h"
#include "core/ecs/world.h"
#include "core/ecs/components/mesh_renderer_component.h"
#include "core/math/math.h"
#include "core/render/primitives.h"
#include "core/render/primitive_type.h"

constexpr int CUBE_COUNT = 20;

struct Cube {
    Cube() = default;
    CoreEngine::Math::Vec3 position{0.0f};
    float size = 1.0f;
};

class SandboxApp final : public CoreEngine::IGameApp {
public:
    void Init(const CoreEngine::EngineContext &context) override {
        Cube cube[CUBE_COUNT];

        for (int i = 0; i < std::sqrt(CUBE_COUNT); i++) {
            for (int j = 0; j < std::sqrt(CUBE_COUNT); j++) {
                int current_cube_index = i+j*CUBE_COUNT;
                cube[i+j*CUBE_COUNT].position = CoreEngine::Math::Vec3{float(i) + cube[current_cube_index].size/2.0f,float(j) + cube[current_cube_index].size/2.0f,0};
            }
        }

        for (const auto &c : cube) {
            int counter = 0;
            CoreEngine::Node cube_node = context.world.CreateNode(std::format("Cube: %d",counter));
            cube_node.SetPosition(c.position);
            cube_node.SetScale(CoreEngine::Math::Vec3(c.size));
            cube_node.AddComponent<CoreEngine::MeshRendererComponent>(CoreEngine::MeshHandle());

            counter++;
        }
    }

    void Update(const CoreEngine::FrameContext &frame) override {
        (void) frame;
    }

    void Shutdown(const CoreEngine::EngineContext &context) override {
        (void) context;
    }

    private:


};

int main() {
    auto app = std::make_unique<SandboxApp>();

    CoreEngine::EngineConfig config;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.resizable = true;
    config.windowTitle = "CoreEngine Sandbox";
    config.renderBackend = CoreEngine::RenderBackendType::None;
    config.enableImGui = false;

    return CoreEngine::RunEngine(std::move(app), config);
}
