#pragma once

#include <memory>

#include "core/assets/i_model_importer.h"

namespace CoreEngine {
    class AssimpModelImporter final : public IModelImporter {
    public:
        AssimpModelImporter();

        ~AssimpModelImporter() override;

        [[nodiscard]] ModelLoadResult Load(const ModelLoadDesc &desc) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace CoreEngine
