#include "core/network/prediction/network_prediction_system.h"

#include <algorithm>

namespace CoreEngine {
    namespace {
        constexpr std::uint8_t kInputCommandRedundancy = 3;
    }

    void NetworkPredictionSystem::Reset() noexcept {
        buffer_.Reset();
        stats_ = {};
        next_sequence_ = 1;
        last_reconciled_sequence_ = 0;
        received_initial_authority_ = false;
    }

    void NetworkPredictionSystem::RecordPrediction(const PlayerInputCommand &command,
                                                   const PredictedMovementState &state) noexcept {
        buffer_.Store(command, state);
        ++stats_.commands_recorded;
    }

    void NetworkPredictionSystem::UpdatePredictionState(const PlayerInputCommand &command,
                                                        const PredictedMovementState &state) noexcept {
        buffer_.Store(command, state);
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

        for (std::uint32_t sequence = latest.sequence;
             sequence > 1 && count < std::min(kInputCommandRedundancy, kMaxInputCommandsPerPacket);
             --sequence) {
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

    std::span<const PlayerInputCommand> NetworkPredictionSystem::BuildReplayCommandBatch(
        std::uint32_t confirmed_sequence,
        std::uint32_t latest_sequence) noexcept {
        if (latest_sequence <= confirmed_sequence) {
            return {};
        }

        std::size_t count = 0;
        for (std::uint32_t sequence = confirmed_sequence + 1u;
             sequence <= latest_sequence && count < replay_commands_.size();
             ++sequence) {
            const PredictionRecord *record = buffer_.Find(sequence);
            if (record == nullptr) {
                break;
            }

            replay_commands_[count++] = record->command;
        }

        return std::span<const PlayerInputCommand>{replay_commands_.data(), count};
    }

    LocalPlayerReconciliationPlan NetworkPredictionSystem::BuildReconciliationPlan(
        const PredictedMovementState &authoritative_state,
        std::uint32_t confirmed_sequence) noexcept {
        if (confirmed_sequence <= last_reconciled_sequence_ && received_initial_authority_) {
            return LocalPlayerReconciliationPlan{
                .authoritative_state = authoritative_state,
            };
        }

        if (!received_initial_authority_ && confirmed_sequence == 0u) {
            received_initial_authority_ = true;
            last_reconciled_sequence_ = 0;
            ++stats_.hard_snaps;

            LocalPlayerReconciliationPlan plan{
                .result = ReconciliationResult{
                    .action = ReconciliationAction::HardSnap,
                    .confirmed_sequence = 0,
                },
                .authoritative_state = authoritative_state,
            };
            plan.replay_commands = BuildReplayCommandBatch(0, LastIssuedSequence());
            return plan;
        }

        LocalPlayerReconciliationPlan plan{
            .result = Reconcile(authoritative_state, confirmed_sequence),
            .authoritative_state = authoritative_state,
        };
        last_reconciled_sequence_ = confirmed_sequence;
        received_initial_authority_ = true;

        if (plan.ShouldApplyAuthoritativeState()) {
            plan.replay_commands = BuildReplayCommandBatch(confirmed_sequence, LastIssuedSequence());
        }

        return plan;
    }
} // namespace CoreEngine
