#include <core/IGameApp.h>
#include <iostream>
#include <string>

#include "core/application/application.h"
#include "core/log/log.h"


class MyGameApp final : public CoreEngine::IGameApp {
public:
  MyGameApp() = default;

  void Init() override {
  }

  void Update(float delta_time) override {
    CoreEngine::Log::Debug("Game", "Update: DeltaTime: " + std::to_string(delta_time));
    CoreEngine::Log::Info("Game", "Update: DeltaTime: " + std::to_string(delta_time));
    CoreEngine::Log::Warn("Game", "Update: DeltaTime: " + std::to_string(delta_time));
    CoreEngine::Log::Error("Game", "Update: DeltaTime: " + std::to_string(delta_time));
    CoreEngine::Log::Fatal("Game", "Update: DeltaTime: " + std::to_string(delta_time));

    CoreEngine::Application::RequestShutdown();
  }

  void Shutdown() override {
  }
};

int main() {
  std::unique_ptr<CoreEngine::IGameApp> gameApp = std::make_unique<MyGameApp>();

  CoreEngine::EngineConfig config;
  config.windowWidth = 1024;
  config.windowHeight = 768;
  config.windowTitle = "My Awesome Game";

  return CoreEngine::RunEngine(std::move(gameApp), config);
}
