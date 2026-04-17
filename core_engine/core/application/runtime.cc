#include "core/application/runtime.h"
#include "core/IGameApp.h"
#include <memory>

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

  app.Init();

  running_ = true;

  while (!IsShutdownRequested()) {
    app.Update(0.016f);
  }

  app.Shutdown();

  Shutdown(app);
  return 0;
}

World &Runtime::GetWorld() { return *world_; }

const World &Runtime::GetWorld() const { return *world_; }

bool Runtime::Initialize() {
  shutdown_requested_.store(false, std::memory_order_release);
  Application::Bind(*this);

  InitializeSink();
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
  WorldAccess::Bind(*world_);
}

void Runtime::Tick(IGameApp *app, float deltaTime) {}

void Runtime::Shutdown(IGameApp &app) {
  Log::Unbind();
  Application::Unbind();
  console_sink_.reset();
  logger_.reset();
}
} // namespace CoreEngine
