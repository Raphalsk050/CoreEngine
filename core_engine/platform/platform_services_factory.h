#pragma once

#include <memory>

#include "core/platform/i_platform_services.h"

namespace CoreEngine {
    [[nodiscard]] std::unique_ptr<IPlatformServices> CreatePlatformServices();
} // namespace CoreEngine
