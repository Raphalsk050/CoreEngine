#include "core/network/prediction/reconciliation.h"

#include "core/math/math.h"

namespace CoreEngine {
    Reconciliation::Reconciliation(ReconciliationDesc desc) noexcept
        : desc_(desc) {
    }

    ReconciliationResult Reconciliation::Compare(const PredictionRecord *record,
                                                 const PredictedMovementState &authoritative_state,
                                                 std::uint32_t confirmed_sequence) const noexcept {
        if (record == nullptr) {
            return ReconciliationResult{
                .action = ReconciliationAction::MissingPrediction,
                .confirmed_sequence = confirmed_sequence,
            };
        }

        const float position_error = Math::Distance(record->state.position, authoritative_state.position);
        ReconciliationAction action = ReconciliationAction::None;
        if (position_error > desc_.hard_snap_position_error) {
            action = ReconciliationAction::HardSnap;
        } else if (position_error > desc_.small_position_error) {
            action = ReconciliationAction::SmoothCorrection;
        }

        return ReconciliationResult{
            .action = action,
            .position_error = position_error,
            .confirmed_sequence = confirmed_sequence,
        };
    }
} // namespace CoreEngine
