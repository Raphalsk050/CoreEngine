#pragma once
#include <atomic>
#include "application.h"
#include "core/application/frame_context.h"
#include "core/ecs/world_access.h"
#include "core/log/logger.h"
#include "core/log/sink/console_sink.h"
#include "core/ecs/world.h"
#include "engine.h"
#include "core/audio/audio_system.h"
#include "core/render/render_system.h"
#include "core/time/frame_clock.h"
#include "core/window/window_system.h"
#include "platform/sdl/sdl_context.h"

namespace CoreEngine {
    class IGameApp;

    class Runtime : public IApplicationService, public IWorldService {
    public:
        void RequestShutdown() override;

        bool IsShutdownRequested() const override;

        explicit Runtime(const EngineConfig &config);

        int Run(IGameApp &app);

        World &GetWorld() override;

        const World &GetWorld() const override;

    private:
        bool Initialize();

        void InitializeSink();

        void InitializeWorld();

        void InitializeWindowBackend();

        void InitializeAudioBackend();

        void InitializeRenderBackend();

        void Tick(const FrameContext &frame);

        void Shutdown();

        [[nodiscard]] Logger &GetLogger() const { return *logger_; }

        bool running_ = false;
        EngineConfig config_;
        FrameClock frame_clock_;
        std::unique_ptr<Logger> logger_;
        std::unique_ptr<World> world_;
        std::shared_ptr<ConsoleSink> console_sink_;
        std::unique_ptr<WindowSystem> window_system_;
        std::unique_ptr<AudioSystem> audio_system_;
        std::unique_ptr<RenderSystem> render_system_;
        SdlContext sdl_context_;
        std::atomic_bool shutdown_requested_{false};
    };
} // namespace CoreEngine
