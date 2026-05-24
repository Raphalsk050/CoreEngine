#pragma once
#include <atomic>
#include <memory>

#include "application.h"
#include "core/application/frame_context.h"
#include "core/debug/debug_draw.h"
#include "core/ecs/world_access.h"
#include "core/log/logger.h"
#include "core/log/sink/console_sink.h"
#include "core/ecs/world.h"
#include "engine.h"
#include "core/audio/audio_system.h"
#include "core/input/input_system.h"
#include "core/network/multiplayer_system.h"
#include "core/network/player/network_player_system.h"
#include "core/platform/i_platform_services.h"
#include "core/render/render_system.h"
#include "core/simulation/simulation_scheduler.h"
#include "core/time/frame_clock.h"
#include "core/window/window_system.h"

namespace CoreEngine {
    class IGameApp;
    class EditorLogSink;
    class EditorSystem;
    class OnlineSystem;
    class SteamMultiplayerDebugPanel;

    class Runtime : public IApplicationService, public IWorldService {
    public:
        void RequestShutdown() override;

        bool IsShutdownRequested() const override;

        explicit Runtime(const EngineConfig &config);

        ~Runtime();

        int Run(IGameApp &app);

        World &GetWorld() override;

        const World &GetWorld() const override;

    private:
        bool Initialize();

        void InitializeSink();

        void InitializeWorld();

        [[nodiscard]] bool InitializeWindowBackend();

        void InitializeInputSystem();

        [[nodiscard]] bool InitializeAudioBackend();

        void InitializeOnlineSystem();

        [[nodiscard]] bool InitializeRenderBackend();

        void Tick(const FrameContext &frame);

        void RenderDeveloperUi(const FrameContext &frame);

        void Shutdown();

        [[nodiscard]] Logger &GetLogger() const { return *logger_; }

        bool running_ = false;
        EngineConfig config_;
        FrameClock frame_clock_;
        std::unique_ptr<Logger> logger_;
        std::unique_ptr<World> world_;
        std::unique_ptr<DebugDrawSystem> debug_draw_system_;
        std::shared_ptr<ConsoleSink> console_sink_;
        std::unique_ptr<WindowSystem> window_system_;
        std::unique_ptr<AudioSystem> audio_system_;
        std::unique_ptr<InputSystem> input_system_;
        std::unique_ptr<OnlineSystem> online_system_;
        std::unique_ptr<SimulationScheduler> simulation_scheduler_;
        std::unique_ptr<MultiplayerSystem> multiplayer_system_;
        std::unique_ptr<NetworkPlayerSystem> network_player_system_;
        std::unique_ptr<RenderSystem> render_system_;
        std::unique_ptr<IPlatformServices> platform_services_;
        std::shared_ptr<EditorLogSink> editor_log_sink_;
        std::unique_ptr<EditorSystem> editor_system_;
        std::unique_ptr<SteamMultiplayerDebugPanel> steam_multiplayer_debug_panel_;
        RenderBackendType resolved_render_backend_ = RenderBackendType::None;
        std::atomic_bool shutdown_requested_{false};
    };
} // namespace CoreEngine
