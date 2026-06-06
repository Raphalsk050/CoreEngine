#include "platform/platform_services_factory.h"

#include "platform/sdl/sdl_platform_services.h"

namespace CoreEngine {
    std::unique_ptr<IPlatformServices> CreatePlatformServices() { return std::make_unique<SdlPlatformServices>(); }
} // namespace CoreEngine
