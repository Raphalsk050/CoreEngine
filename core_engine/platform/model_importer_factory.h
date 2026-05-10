#pragma once

#include <memory>

#include "core/assets/i_model_importer.h"

namespace CoreEngine {
    [[nodiscard]] std::unique_ptr<IModelImporter> CreateModelImporter();
} // namespace CoreEngine
