#pragma once

#include "core/assets/model_asset.h"

namespace CoreEngine {
    class IModelImporter {
    public:
        virtual ~IModelImporter() = default;

        [[nodiscard]] virtual ModelLoadResult Load(const ModelLoadDesc &desc) = 0;
    };
} // namespace CoreEngine
