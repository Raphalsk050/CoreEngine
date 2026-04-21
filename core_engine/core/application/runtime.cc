#include "core/application/runtime.h"
#include "core/i_game_app.h"
#include <memory>

#include "core/render/render_desc.h"
#include "core/window/window_event.h"
#include "platform/render_backend_factory.h"
#include "platform/sdl/sdl_audio_backend.h"
#include "platform/sdl/sdl_window_backend.h"

namespace {
    CoreEngine::WindowSurfaceType SelectWindowSurfaceType(CoreEngine::RenderBackendType backend) {
#if defined(__APPLE__)
        if (backend == CoreEngine::RenderBackendType::DiligentVulkan) {
            return CoreEngine::WindowSurfaceType::Metal;
        }
#endif

        return CoreEngine::WindowSurfaceType::Default;
    }
}
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
            .render_system = *render_system_,
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
                .render_system = *render_system_,
            };

            Tick(frameContext);
            app.Update(frameContext);
            render_system_->RenderFrame(*world_);
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
        InitializeWindowBackend();
        InitializeAudioBackend();
        InitializeRenderBackend();
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
        desc.surface_type = SelectWindowSurfaceType(config_.renderBackend);

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

    void Runtime::InitializeRenderBackend() {
        std::unique_ptr<IRenderBackend> backend = CreateRenderBackend(config_.renderBackend);
        if (backend == nullptr) {
            Log::Error("Render", "Requested render backend is not available. Enable CORE_ENGINE_ENABLE_DILIGENT or choose RenderBackendType::None.");
            backend = CreateRenderBackend(RenderBackendType::None);
        }

        render_system_ = std::make_unique<RenderSystem>(std::move(backend));

        RenderDesc desc;
        desc.backend = config_.renderBackend;
        desc.vsync   = config_.vsync;
        desc.width   = config_.windowWidth;
        desc.height  = config_.windowHeight;

        if (!render_system_->Initialize(desc, window_system_->GetNativeHandle())) {
            Log::Error("Render", render_system_->LastError());
        }
    }

    void Runtime::Tick(const FrameContext &frame) {
        (void) frame;
        window_system_->PollEvents();

        for (const WindowEvent &event: window_system_->Events()) {
            if (event.type == WindowEventType::Resized || event.type == WindowEventType::PixelSizeChanged) {
                render_system_->Resize(event.width, event.height);
            }
        }

        if (window_system_->ShouldClose()) {
            RequestShutdown();
        }
    }

    void Runtime::Shutdown() {
        Application::Unbind();

        if (render_system_ != nullptr) {
            render_system_->Shutdown();
            render_system_.reset();
        }

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
