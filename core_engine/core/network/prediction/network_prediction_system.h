#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "core/network/prediction/prediction_buffer.h"
#include "core/network/prediction/reconciliation.h"

namespace CoreEngine {
    struct NetworkPredictionStats {
        std::uint64_t commands_recorded = 0;
        std::uint64_t corrections = 0;
        std::uint64_t hard_snaps = 0;
        float last_position_error = 0.0f;
    };

    /**
     * @brief Owns local player prediction command history and reconciliation stats.
     *
     * Responsibility: provide allocation-free command redundancy and correction
     * bookkeeping for the owning client pawn.
     */
    class NetworkPredictionSystem {
    public:
        void Reset() noexcept;

        void RecordPrediction(const PlayerInputCommand &command,
                              const PredictedMovementState &state) noexcept;

        [[nodiscard]] ReconciliationResult Reconcile(const PredictedMovementState &authoritative_state,
                                                     std::uint32_t confirmed_sequence) noexcept;

        [[nodiscard]] std::span<const PlayerInputCommand> BuildRedundantCommandBatch(const PlayerInputCommand &latest) noexcept;

        [[nodiscard]] const NetworkPredictionStats &Stats() const noexcept {
            return stats_;
        }

        [[nodiscard]] std::uint32_t NextSequence() noexcept {
            return next_sequence_++;
        }

    private:
        PredictionBuffer<256> buffer_;
        Reconciliation reconciliation_;
        NetworkPredictionStats stats_;
        std::array<PlayerInputCommand, kMaxInputCommandsPerPacket> outgoing_commands_{};
        std::array<PlayerInputCommand, kMaxInputCommandsPerPacket> command_scratch_{};
        std::uint32_t next_sequence_ = 1;
    };
} // namespace CoreEngine
