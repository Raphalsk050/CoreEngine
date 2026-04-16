#pragma once
#include "application.h"
#include "engine.h"
#include "core/log/logger.h"
#include "core/log/sink/console_sink.h"

namespace CoreEngine {
    class IGameApp;

    class Runtime : public IApplicationService {
    public:
        void RequestShutdown() override;

        bool IsShutdownRequested() const override;

        explicit Runtime(const EngineConfig &config);

        int Run(IGameApp &app);

    private:
        bool Initialize();

        void InitializeSink();

        void Tick(IGameApp *app, float deltaTime);

        void Shutdown(IGameApp &app);

        [[nodiscard]] Logger &GetLogger() const {
            return *logger_;
        }

        bool running_ = false;
        EngineConfig config_;
        std::unique_ptr<Logger> logger_;
        std::shared_ptr<ConsoleSink> console_sink_;
        std::atomic_bool shutdown_requested_{false};
    };
}
