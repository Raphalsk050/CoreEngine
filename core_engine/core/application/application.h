#pragma once
#include "core/assert/assert.h"

namespace CoreEngine {
    class IApplicationService {
    public:
        virtual ~IApplicationService() = default;

        virtual void RequestShutdown() = 0;

        [[nodiscard]] virtual bool IsShutdownRequested() const = 0;
    };

    class Application {
    public:
        static void Bind(IApplicationService &service);

        static void Unbind();

        static void RequestShutdown();

        static bool IsShutdownRequested();
    };
}
