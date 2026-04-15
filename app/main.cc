#include <core/IGameApp.h>
#include <iostream>


class MyGameApp final : public CoreEngine::IGameApp {
public:
  MyGameApp() = default;

  void Init() override {
  }

  void Update(float delta_time) override {
    std::cout << "Running MyGameApp with delta_time = " << delta_time << std::endl;
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
