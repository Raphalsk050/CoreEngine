#pragma once

#include <cstdint>

#include "core/network/prediction/prediction_buffer.h"

namespace CoreEngine {
    struct ReconciliationDesc {
        float small_position_error = 0.05f;
        float hard_snap_position_error = 1.5f;
    };

    enum class ReconciliationAction : std::uint8_t {
        None,
        SmoothCorrection,
        HardSnap,
        MissingPrediction,
    };

    struct ReconciliationResult {
        ReconciliationAction action = ReconciliationAction::None;
        float position_error = 0.0f;
        std::uint32_t confirmed_sequence = 0;
    };

    /**
     * @brief Classifies authoritative snapshots against predicted movement.
     *
     * Responsibility: keep correction policy independent from transport and
     * gameplay code so presentation smoothing can evolve separately.
     */
    class Reconciliation {
    public:
        explicit Reconciliation(ReconciliationDesc desc = {}) noexcept;

        [[nodiscard]] ReconciliationResult Compare(const PredictionRecord *record,
                                                   const PredictedMovementState &authoritative_state,
                                                   std::uint32_t confirmed_sequence) const noexcept;

    private:
        ReconciliationDesc desc_;
    };
} // namespace CoreEngine
