#include "core/application/runtime.h"
#include "core/i_game_app.h"
#include <memory>

#include "platform/sdl/sdl_audio_backend.h"
#include "platform/sdl/sdl_window_backend.h"

namespace CoreEngine {
    int RunEngine(std::unique_ptr<IGameApp> app, const EngineConfig &config) {
        if (!app)
            return 1;

        Runtime runtime(config);

        return runtime.Run(*app);
    }

    void Runtime::RequestShutdown() {
        shutdown_requested_.store(true, std::memory_order_release);
    }

    bool Runtime::IsShutdownRequested() const {
        return shutdown_requested_.load(std::memory_order_acquire);
    }

    Runtime::Runtime(const EngineConfig &config) { config_ = config; }

    int Runtime::Run(IGameApp &app) {
        Initialize();

        EngineContext engineContext{
            .world = *world_,
            .audio_system = *audio_system_,
            .window_system = *window_system_,
        };

        app.Init(engineContext);

        running_ = true;

        while (!IsShutdownRequested()) {
            const float deltaTime = frame_clock_.TickSeconds();
            FrameContext frameContext{
                .delta_time = deltaTime,
                .world = *world_,
                .audio_system = *audio_system_,
                .window_system = *window_system_,
            };

            Tick(frameContext);
            app.Update(frameContext);
        }

        app.Shutdown(engineContext);

        Shutdown();
        return 0;
    }

    World &Runtime::GetWorld() { return *world_; }

    const World &Runtime::GetWorld() const { return *world_; }

    bool Runtime::Initialize() {
        shutdown_requested_.store(false, std::memory_order_release);
        Application::Bind(*this);
        frame_clock_ = FrameClock();

        InitializeSink();
        InitializeWorld();
        InitializeAudioBackend();
        InitializeWindowBackend();
        return true;
    }

    void Runtime::InitializeSink() {
        logger_ = std::make_unique<Logger>();
        Log::Bind(*logger_);
        console_sink_ = std::make_shared<ConsoleSink>();
        logger_->AddSink(console_sink_);
    }

    void Runtime::InitializeWorld() {
        world_ = std::make_unique<World>();
        WorldAccess::Bind(*this);
    }

    void Runtime::InitializeWindowBackend() {
        // TODO(rafael): improve this in the future to take off the SDL specification from here
        auto backend = std::make_unique<SdlWindowBackend>(sdl_context_);
        window_system_ = std::make_unique<WindowSystem>(std::move(backend));

        WindowDesc desc;
        desc.width = config_.windowWidth;
        desc.height = config_.windowHeight;
        desc.title = config_.windowTitle;
        desc.resizable = config_.resizable;
        desc.highDpi = config_.highDPI;
        desc.decorated = config_.decorated;
        desc.fullscreen = config_.fullscreen;


        if (!window_system_->Initialize(desc)) {
            Log::Error("Window", window_system_->LastError());
        }
    }

    void Runtime::InitializeAudioBackend() {
        auto backend = std::make_unique<SdlAudioBackend>(sdl_context_);
        audio_system_ = std::make_unique<AudioSystem>(std::move(backend));

        AudioDesc desc;

        if (!audio_system_->Initialize(desc)) {
            Log::Error("Audio", audio_system_->LastError());
            return;
        }
    }

    void Runtime::Tick(const FrameContext &frame) {
        (void) frame;
        window_system_->PollEvents();

        if (window_system_->ShouldClose()) {
            RequestShutdown();
        }
    }

    void Runtime::Shutdown() {
        Application::Unbind();

        if (audio_system_ != nullptr) {
            audio_system_->Shutdown();
            audio_system_.reset();
        }

        if (window_system_ != nullptr) {
            window_system_->Shutdown();
            window_system_.reset();
        }

        Log::Unbind();
        console_sink_.reset();
        logger_.reset();
    }
} // namespace CoreEngine
