#include "core/network/prediction/network_prediction_system.h"

#include <algorithm>

namespace CoreEngine {
    void NetworkPredictionSystem::Reset() noexcept {
        buffer_.Reset();
        stats_ = {};
        next_sequence_ = 1;
    }

    void NetworkPredictionSystem::RecordPrediction(const PlayerInputCommand &command,
                                                   const PredictedMovementState &state) noexcept {
        buffer_.Store(command, state);
        ++stats_.commands_recorded;
    }

    ReconciliationResult NetworkPredictionSystem::Reconcile(const PredictedMovementState &authoritative_state,
                                                            std::uint32_t confirmed_sequence) noexcept {
        const ReconciliationResult result = reconciliation_.Compare(
            buffer_.Find(confirmed_sequence),
            authoritative_state,
            confirmed_sequence);

        stats_.last_position_error = result.position_error;
        if (result.action == ReconciliationAction::SmoothCorrection) {
            ++stats_.corrections;
        } else if (result.action == ReconciliationAction::HardSnap) {
            ++stats_.hard_snaps;
        }

        return result;
    }

    std::span<const PlayerInputCommand> NetworkPredictionSystem::BuildRedundantCommandBatch(
        const PlayerInputCommand &latest) noexcept {
        command_scratch_[0] = latest;
        std::uint8_t count = 1;

        for (std::uint32_t sequence = latest.sequence; sequence > 1 && count < kMaxInputCommandsPerPacket; --sequence) {
            const PredictionRecord *record = buffer_.Find(sequence - 1u);
            if (record == nullptr) {
                break;
            }

            command_scratch_[count++] = record->command;
        }

        for (std::uint8_t i = 0; i < count; ++i) {
            outgoing_commands_[i] = command_scratch_[count - 1u - i];
        }

        return std::span<const PlayerInputCommand>{outgoing_commands_.data(), count};
    }
} // namespace CoreEngine
