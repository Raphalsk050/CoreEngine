#pragma once

#include <cstdint>
#include <span>

#include "core/ecs/node.h"
#include "core/network/network_input_command_queue.h"
#include "core/network/network_peer.h"
#include "core/network/network_session.h"
#include "core/network/network_stats.h"
#include "core/network/prediction/network_prediction_system.h"
#include "core/network/prediction/reconciliation.h"
#include "core/network/replication/network_archetype_registry.h"
#include "core/network/replication/network_replicator.h"

namespace CoreEngine {
    class NetworkSystem;
    class SimulationScheduler;
    struct SimulationFrame;
    class World;

    /**
     * @brief App-facing multiplayer facade owned by the runtime.
     *
     * Responsibility: coordinate prediction, replication, and session access
     * through a narrow gameplay API while keeping protocol ownership inside the engine.
     */
    class MultiplayerSystem final {
    public:
        bool Initialize(NetworkSystem &network_system, World &world);

        void Shutdown() noexcept;

        void BeginSimulationTick(const SimulationFrame &frame) noexcept;

        void EndSimulationTick(const SimulationFrame &frame) noexcept;

        void UpdatePresentation(float delta_seconds) noexcept;

        [[nodiscard]] bool IsInitialized() const noexcept {
            return network_system_ != nullptr;
        }

        [[nodiscard]] NetworkRole Role() const noexcept;

        [[nodiscard]] NetworkSessionState SessionState() const noexcept;

        [[nodiscard]] std::span<const NetworkPeer> Peers() const noexcept;

        [[nodiscard]] const NetworkStats &Stats() const noexcept;

        [[nodiscard]] std::span<const QueuedPlayerInputCommand> InputCommands() const noexcept;

        [[nodiscard]] NetworkEntityId RegisterEntity(Node node,
                                                     const NetworkEntityRegistrationDesc &desc);

        [[nodiscard]] NetworkEntityId RegisterEntity(Node node,
                                                     NetworkEntityId network_id,
                                                     PeerId owner_peer,
                                                     bool local_authority);

        [[nodiscard]] NetworkEntityId RegisterEntity(Node node, PeerId owner_peer, bool local_authority);

        void UnregisterEntity(NetworkEntityId network_id) noexcept;

        [[nodiscard]] Node FindNode(NetworkEntityId network_id) const noexcept;

        [[nodiscard]] NetworkEntityId GetNetworkId(Node node) const noexcept;

        [[nodiscard]] bool HasAuthority(Node node) const noexcept;

        [[nodiscard]] bool IsOwningClient(Node node, PeerId peer) const noexcept;

        [[nodiscard]] bool RegisterArchetype(const NetworkArchetypeDesc &desc) noexcept;

        void UnregisterArchetype(NetworkArchetypeId archetype_id) noexcept;

        [[nodiscard]] const NetworkReplicatorStats &ReplicationStats() const noexcept;

        void CaptureLocalInputSample(const SimulationScheduler &scheduler) noexcept;

        [[nodiscard]] PlayerInputCommand BuildLocalPlayerInputCommand(
            const LocalPlayerInputDesc &input) noexcept;

        bool SubmitLocalPlayerInput(NetworkEntityId local_entity_id,
                                    const PlayerInputCommand &command,
                                    const PredictedMovementState &predicted_state);

        [[nodiscard]] LocalPlayerReconciliationPlan BuildLocalPlayerReconciliationPlan(Node node) noexcept;

        template <typename ReplayFn>
        void ReplayLocalPlayerInputCommands(const LocalPlayerReconciliationPlan &plan,
                                            ReplayFn replay) noexcept {
            for (const PlayerInputCommand &command: plan.replay_commands) {
                prediction_system_.UpdatePredictionState(command, replay(command));
            }
        }

    private:
        struct LocalInputSampleStamp {
            std::uint32_t tick = 0;
            std::uint16_t sub_tick = 0;
            bool valid = false;
        };

        NetworkSystem *network_system_ = nullptr;
        NetworkPredictionSystem prediction_system_;
        NetworkReplicator replicator_;
        LocalInputSampleStamp latest_local_input_stamp_{};
    };
} // namespace CoreEngine
