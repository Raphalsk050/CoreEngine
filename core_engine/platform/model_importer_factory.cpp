#include "platform/model_importer_factory.h"

#include "platform/assimp/assimp_model_importer.h"

namespace CoreEngine {
    std::unique_ptr<IModelImporter> CreateModelImporter() { return std::make_unique<AssimpModelImporter>(); }
} // namespace CoreEngine
