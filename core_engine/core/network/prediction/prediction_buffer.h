#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/math/math.h"
#include "core/network/prediction/player_input_command.h"

namespace CoreEngine {
    struct PredictedMovementState {
        Math::Vec3 position{0.0f, 0.0f, 0.0f};
        Math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Math::Vec3 velocity{0.0f, 0.0f, 0.0f};
        std::uint32_t movement_flags = 0;
    };

    struct PredictionRecord {
        PlayerInputCommand command{};
        PredictedMovementState state{};
        bool valid = false;
    };

    /**
     * @brief Fixed-capacity ring buffer for local movement prediction history.
     *
     * Responsibility: keep command/state pairs for reconciliation without
     * allocating during fixed simulation ticks.
     */
    template<std::size_t Capacity = 256>
    class PredictionBuffer {
    public:
        static_assert((Capacity & (Capacity - 1u)) == 0u, "PredictionBuffer capacity must be a power of two");

        void Reset() noexcept {
            for (PredictionRecord &record: records_) {
                record = {};
            }
        }

        void Store(const PlayerInputCommand &command, const PredictedMovementState &state) noexcept {
            PredictionRecord &record = records_[Index(command.sequence)];
            record.command = command;
            record.state = state;
            record.valid = true;
        }

        [[nodiscard]] const PredictionRecord *Find(std::uint32_t sequence) const noexcept {
            const PredictionRecord &record = records_[Index(sequence)];
            if (!record.valid || record.command.sequence != sequence) {
                return nullptr;
            }
            return &record;
        }

        [[nodiscard]] PredictionRecord *Find(std::uint32_t sequence) noexcept {
            PredictionRecord &record = records_[Index(sequence)];
            if (!record.valid || record.command.sequence != sequence) {
                return nullptr;
            }
            return &record;
        }

    private:
        [[nodiscard]] static constexpr std::size_t Index(std::uint32_t sequence) noexcept {
            return static_cast<std::size_t>(sequence) & (Capacity - 1u);
        }

        std::array<PredictionRecord, Capacity> records_{};
    };
} // namespace CoreEngine
