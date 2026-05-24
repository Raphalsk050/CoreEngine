#include "core/application/runtime.h"
#include "core/i_game_app.h"
#include <memory>

#include "core/editor/editor_log_sink.h"
#include "core/editor/editor_system.h"
#include "core/network/network_system.h"
#include "core/online/online_system.h"
#include "core/online/steam/steam_config.h"
#include "core/online/steam/steam_multiplayer_debug_panel.h"
#include "core/render/render_desc.h"
#include "core/window/window_event.h"
#include "platform/model_importer_factory.h"
#include "platform/platform_services_factory.h"
#include "platform/render_backend_factory.h"

namespace {
    const char *RenderBackendName(CoreEngine::RenderBackendType backend) {
        switch (backend) {
            case CoreEngine::RenderBackendType::None:
                return "None";
            case CoreEngine::RenderBackendType::DiligentD3D11:
                return "DiligentD3D11";
            case CoreEngine::RenderBackendType::DiligentD3D12:
                return "DiligentD3D12";
            case CoreEngine::RenderBackendType::DiligentVulkan:
                return "DiligentVulkan";
        }

        return "Unknown";
    }

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

    Runtime::Runtime(const EngineConfig &config)
        : config_(config), platform_services_(CreatePlatformServices()) {
    }

    Runtime::~Runtime() = default;

    int Runtime::Run(IGameApp &app) {
        if (!Initialize()) {
            Shutdown();
            return 1;
        }

        EngineContext engineContext{
            .world = *world_,
            .debug_draw = *debug_draw_system_,
            .audio_system = *audio_system_,
            .input_system = *input_system_,
            .online_system = *online_system_,
            .multiplayer = *multiplayer_system_,
            .network_players = *network_player_system_,
            .simulation_scheduler = *simulation_scheduler_,
            .window_system = *window_system_,
            .render_system = *render_system_,
        };

        app.Init(engineContext);

        running_ = true;

        while (!IsShutdownRequested()) {
            const float deltaTime = frame_clock_.TickSeconds();
            debug_draw_system_->BeginFrame(deltaTime);
            FrameContext frameContext{
                EngineContext{
                    .world = *world_,
                    .debug_draw = *debug_draw_system_,
                    .audio_system = *audio_system_,
                    .input_system = *input_system_,
                    .online_system = *online_system_,
                    .multiplayer = *multiplayer_system_,
                    .network_players = *network_player_system_,
                    .simulation_scheduler = *simulation_scheduler_,
                    .window_system = *window_system_,
                    .render_system = *render_system_,
                },
                deltaTime,
            };

            Tick(frameContext);
            online_system_->BeginFrame();
            render_system_->BeginImGuiFrame();
            RenderDeveloperUi(frameContext);

            const bool run_single_step = editor_system_ != nullptr && editor_system_->ConsumeSingleStepRequest();
            const bool run_game_frame = editor_system_ == nullptr || editor_system_->ShouldRunGame() || run_single_step;
            const float simulation_delta = run_single_step
                                               ? simulation_scheduler_->Clock().FixedDeltaTime()
                                               : deltaTime;

            simulation_scheduler_->BeginFrame(run_game_frame ? simulation_delta : 0.0f);
            if (run_game_frame) {
                SimulationFrame simulation_frame;
                while (simulation_scheduler_->ConsumeFixedFrame(simulation_frame)) {
                    multiplayer_system_->BeginSimulationTick(simulation_frame);
                    network_player_system_->FixedUpdate(simulation_frame);
                    app.FixedUpdate(FixedFrameContext{
                        EngineContext{
                            .world = *world_,
                            .debug_draw = *debug_draw_system_,
                            .audio_system = *audio_system_,
                            .input_system = *input_system_,
                            .online_system = *online_system_,
                            .multiplayer = *multiplayer_system_,
                            .network_players = *network_player_system_,
                            .simulation_scheduler = *simulation_scheduler_,
                            .window_system = *window_system_,
                            .render_system = *render_system_,
                        },
                        simulation_frame,
                    });
                    multiplayer_system_->EndSimulationTick(simulation_frame);
                }
                multiplayer_system_->UpdatePresentation(simulation_delta);
                multiplayer_system_->CaptureLocalInputSample(*simulation_scheduler_);
                app.Update(frameContext);
            }
            online_system_->EndFrame();
            render_system_->RenderFrame(*world_, frame_clock_, deltaTime);
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
        InitializeOnlineSystem();

        resolved_render_backend_ = SelectAvailableRenderBackend(config_.renderBackend);
        if (resolved_render_backend_ != config_.renderBackend) {
            Log::Warn("Render",
                      "Requested render backend '{}' is unavailable in this build; using '{}'.",
                      RenderBackendName(config_.renderBackend),
                      RenderBackendName(resolved_render_backend_));
        }

        if (!InitializeWindowBackend()) {
            return false;
        }

        InitializeInputSystem();
        if (!InitializeAudioBackend()) {
            return false;
        }

        if (!InitializeRenderBackend()) {
            return false;
        }

        return true;
    }

    void Runtime::InitializeSink() {
        logger_ = std::make_unique<Logger>();
        Log::Bind(*logger_);
        console_sink_ = std::make_shared<ConsoleSink>();
        logger_->AddSink(console_sink_);
        if (config_.enableEditor) {
            editor_log_sink_ = std::make_shared<EditorLogSink>();
            logger_->AddSink(editor_log_sink_);
        }
    }

    void Runtime::InitializeWorld() {
        world_ = std::make_unique<World>();
        debug_draw_system_ = std::make_unique<DebugDrawSystem>();
        WorldAccess::Bind(*this);
    }

    bool Runtime::InitializeWindowBackend() {
        if (platform_services_ == nullptr) {
            Log::Error("Window", "Platform services are not available");
            return false;
        }

        window_system_ = platform_services_->CreateWindowSystem();
        if (window_system_ == nullptr) {
            Log::Error("Window", "Window system could not be created");
            return false;
        }

        WindowDesc desc;
        desc.width = config_.windowWidth;
        desc.height = config_.windowHeight;
        desc.title = config_.windowTitle;
        desc.resizable = config_.resizable;
        desc.highDpi = config_.highDPI;
        desc.decorated = config_.decorated;
        desc.fullscreen = config_.fullscreen;
        desc.surface_type = SelectWindowSurfaceType(resolved_render_backend_);

        if (!window_system_->Initialize(desc)) {
            Log::Error("Window", window_system_->LastError());
            return false;
        }

        return true;
    }

    void Runtime::InitializeInputSystem() {
        if (platform_services_ != nullptr) {
            input_system_ = platform_services_->CreateInputSystem();
        }

        if (input_system_ == nullptr) {
            input_system_ = std::make_unique<InputSystem>();
        }
    }

    bool Runtime::InitializeAudioBackend() {
        if (platform_services_ == nullptr) {
            Log::Error("Audio", "Platform services are not available");
            return false;
        }

        audio_system_ = platform_services_->CreateAudioSystem();
        if (audio_system_ == nullptr) {
            Log::Error("Audio", "Audio system could not be created");
            return false;
        }

        AudioDesc desc;

        if (!audio_system_->Initialize(desc)) {
            Log::Error("Audio", audio_system_->LastError());
            return false;
        }

        return true;
    }

    void Runtime::InitializeOnlineSystem() {
        online_system_ = std::make_unique<OnlineSystem>(kDefaultSteamAppId);
        if (!online_system_->Initialize()) {
            Log::Warn("Online", "Online system failed to initialize.");
        }

        simulation_scheduler_ = std::make_unique<SimulationScheduler>();
        simulation_scheduler_->Configure(FixedTickClockDesc{});

        multiplayer_system_ = std::make_unique<MultiplayerSystem>();
        multiplayer_system_->Initialize(online_system_->Network(), *world_);

        network_player_system_ = std::make_unique<NetworkPlayerSystem>();
        network_player_system_->Initialize(*multiplayer_system_, *world_);
    }

    bool Runtime::InitializeRenderBackend() {
        std::unique_ptr<IRenderBackend> backend = CreateRenderBackend(resolved_render_backend_);
        if (backend == nullptr) {
            Log::Error("Render",
                       "Requested render backend is not available. Enable CORE_ENGINE_ENABLE_DILIGENT or choose RenderBackendType::None.");
            backend = CreateRenderBackend(RenderBackendType::None);
        }

        if (backend == nullptr) {
            Log::Error("Render", "No render backend could be created");
            return false;
        }

        render_system_ = std::make_unique<RenderSystem>(std::move(backend), CreateModelImporter());
        render_system_->SetDebugDrawSystem(debug_draw_system_.get());

        RenderDesc desc;
        desc.backend = resolved_render_backend_;
        desc.vsync = config_.vsync;
        const bool enable_imgui = config_.enableImGui || config_.enableEditor;
        desc.enable_imgui = enable_imgui;
        desc.width = config_.windowWidth;
        desc.height = config_.windowHeight;

        if (!render_system_->Initialize(desc, window_system_->GetNativeHandle())) {
            Log::Error("Render", render_system_->LastError());
            return false;
        }

        if (config_.enableEditor) {
            editor_system_ = std::make_unique<EditorSystem>();
            editor_system_->Initialize(EditorSystemDesc{
                .project_root = config_.projectRoot,
                .asset_roots = config_.editorAssetRoots,
                .log_sink = editor_log_sink_,
            });
        }

        if (enable_imgui) {
            steam_multiplayer_debug_panel_ = std::make_unique<SteamMultiplayerDebugPanel>();
        }

        return true;
    }

    void Runtime::Tick(const FrameContext &frame) {
        (void) frame;

        input_system_->BeginFrame();
        window_system_->BeginFrame();

        if (platform_services_ != nullptr) {
            platform_services_->PumpEvents(*window_system_);
        } else {
            window_system_->PollEvents();
        }

        input_system_->CommitFrame();

        for (const WindowEvent &event: window_system_->Events()) {
            if (event.type == WindowEventType::Resized || event.type == WindowEventType::PixelSizeChanged) {
                render_system_->Resize(event.width, event.height);
            }
        }

        if (window_system_->ShouldClose()) {
            RequestShutdown();
        }
    }

    void Runtime::RenderDeveloperUi(const FrameContext &frame) {
        if ((!config_.enableImGui && !config_.enableEditor) || online_system_ == nullptr) {
            return;
        }

        if (editor_system_ != nullptr) {
            editor_system_->Render(frame);
        }

        if (steam_multiplayer_debug_panel_ != nullptr) {
            steam_multiplayer_debug_panel_->Render(*online_system_);
        }
    }

    void Runtime::Shutdown() {
        Application::Unbind();
        WorldAccess::Unbind();

        if (editor_system_ != nullptr) {
            editor_system_->Shutdown();
            editor_system_.reset();
        }

        steam_multiplayer_debug_panel_.reset();

        if (render_system_ != nullptr) {
            render_system_->Shutdown();
            render_system_.reset();
        }

        if (online_system_ != nullptr) {
            if (network_player_system_ != nullptr) {
                network_player_system_->Shutdown();
                network_player_system_.reset();
            }
            if (multiplayer_system_ != nullptr) {
                multiplayer_system_->Shutdown();
                multiplayer_system_.reset();
            }
            simulation_scheduler_.reset();
            online_system_->Shutdown();
            online_system_.reset();
        }

        if (platform_services_ != nullptr) {
            platform_services_->ReleaseInputResources();
        }
        input_system_.reset();

        if (audio_system_ != nullptr) {
            audio_system_->Shutdown();
            audio_system_.reset();
        }

        if (window_system_ != nullptr) {
            window_system_->Shutdown();
            window_system_.reset();
        }

        if (platform_services_ != nullptr) {
            platform_services_->Shutdown();
        }

        world_.reset();
        debug_draw_system_.reset();

        Log::Unbind();
        editor_log_sink_.reset();
        console_sink_.reset();
        logger_.reset();
    }
} // namespace CoreEngine
