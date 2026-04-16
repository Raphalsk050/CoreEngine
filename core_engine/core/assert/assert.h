#pragma once
#include <cassert>
#include "engine.h"

namespace CoreEngine {
    /**
     * Assert in debug builds and compile out in ship builds.
     *
     * Use this macro for invariant checks that must never fail in development.
     */
#if defined(CENGINE_DEBUG_BUILD) && CENGINE_DEBUG_BUILD
#define CENGINE_ASSERT(condition, message) assert((condition) && (message))
#else
#define CENGINE_ASSERT(condition, message) ((void)sizeof(condition))
#endif
}
