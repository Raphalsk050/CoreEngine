#include "core/application/application.h"

#include <atomic>

namespace CoreEngine {
    namespace {
        std::atomic<IApplicationService *> applicationService{nullptr};

        IApplicationService *GetApplicationService() { return applicationService.load(std::memory_order_acquire); }
    } // namespace

    void Application::Bind(IApplicationService &service) {
        applicationService.store(&service, std::memory_order_release);
    }

    void Application::Unbind() { applicationService.store(nullptr, std::memory_order_release); }

    void Application::RequestShutdown() {
        IApplicationService *service = GetApplicationService();
        if (service == nullptr) {
            return;
        }

        service->RequestShutdown();
    }

    bool Application::IsShutdownRequested() {
        IApplicationService *service = GetApplicationService();
        if (service == nullptr) {
            return false;
        }

        return service->IsShutdownRequested();
    }
} // namespace CoreEngine
