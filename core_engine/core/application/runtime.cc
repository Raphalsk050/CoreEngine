#include "runtime.h"
#include "core/IGameApp.h"
#include <memory>

namespace CoreEngine {
    int RunEngine(std::unique_ptr<IGameApp> app, const EngineConfig &config) {
        if (!app)
            return 1;

        Runtime runtime(config);

        return runtime.Run(*app);
    }

    Runtime::Runtime(const EngineConfig &config) {
        config_ = config;
    }

    int Runtime::Run(IGameApp &app) {
        Initialize();

        app.Init();

        running_ = true;

        while (running_) {
            app.Update(0.016);
        }

        app.Shutdown();

        Shutdown(app);
        return 0;
    }

    bool Runtime::Initialize() {
        return true;
    }

    void Runtime::Tick(IGameApp *app, float deltaTime) {
    }

    void Runtime::Shutdown(IGameApp &app) {
    }
}
