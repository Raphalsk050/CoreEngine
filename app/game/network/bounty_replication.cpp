#include "bounty_replication.h"

#include <limits>

#include "core/ecs/world.h"
#include "core/network/multiplayer_system.h"
#include "game/components/bounty_gameplay_components.h"

namespace Game {
    namespace {
        constexpr std::uint16_t kMaxReplicatedItems = 256;

        template<typename Component>
        [[nodiscard]] bool HasComponent(const CoreEngine::World &world, entt::entity entity) noexcept {
            return world.TryGetComponent<Component>(entity) != nullptr;
        }

        template<typename Component>
        [[nodiscard]] Component &EnsureComponent(CoreEngine::Node node) {
            if (auto *component = node.TryGetComponent<Component>(); component != nullptr) {
                return *component;
            }

            return node.AddComponent<Component>();
        }

        [[nodiscard]] bool WriteArmorPart(CoreEngine::MessageWriter &writer, const ArmorPart &part) {
            return writer.WriteFloat(part.hit_points) &&
                   writer.WriteFloat(part.max_hit_points);
        }

        [[nodiscard]] bool ReadArmorPart(CoreEngine::MessageReader &reader, ArmorPart &part) noexcept {
            return reader.ReadFloat(part.hit_points) &&
                   reader.ReadFloat(part.max_hit_points);
        }

        [[nodiscard]] bool SerializeHealth(const CoreEngine::World &world,
                                           entt::entity entity,
                                           CoreEngine::MessageWriter &writer) {
            const auto *component = world.TryGetComponent<HealthComponent>(entity);
            return component != nullptr &&
                   writer.WriteFloat(component->health) &&
                   writer.WriteFloat(component->max_health) &&
                   writer.WriteBool(component->alive) &&
                   writer.WriteBool(component->concussed);
        }

        [[nodiscard]] bool ApplyHealth(CoreEngine::World &world,
                                       CoreEngine::Node node,
                                       CoreEngine::MessageReader &reader) {
            (void) world;
            HealthComponent &component = EnsureComponent<HealthComponent>(node);
            return reader.ReadFloat(component.health) &&
                   reader.ReadFloat(component.max_health) &&
                   reader.ReadBool(component.alive) &&
                   reader.ReadBool(component.concussed);
        }

        [[nodiscard]] bool SerializeArmor(const CoreEngine::World &world,
                                          entt::entity entity,
                                          CoreEngine::MessageWriter &writer) {
            const auto *component = world.TryGetComponent<ArmorSegmentsComponent>(entity);
            return component != nullptr &&
                   WriteArmorPart(writer, component->head) &&
                   WriteArmorPart(writer, component->torso) &&
                   WriteArmorPart(writer, component->left_arm) &&
                   WriteArmorPart(writer, component->right_arm) &&
                   WriteArmorPart(writer, component->legs);
        }

        [[nodiscard]] bool ApplyArmor(CoreEngine::World &world,
                                      CoreEngine::Node node,
                                      CoreEngine::MessageReader &reader) {
            (void) world;
            ArmorSegmentsComponent &component = EnsureComponent<ArmorSegmentsComponent>(node);
            return ReadArmorPart(reader, component.head) &&
                   ReadArmorPart(reader, component.torso) &&
                   ReadArmorPart(reader, component.left_arm) &&
                   ReadArmorPart(reader, component.right_arm) &&
                   ReadArmorPart(reader, component.legs);
        }

        [[nodiscard]] bool SerializeInventory(const CoreEngine::World &world,
                                              entt::entity entity,
                                              CoreEngine::MessageWriter &writer) {
            const auto *component = world.TryGetComponent<InventoryComponent>(entity);
            if (component == nullptr || component->item_ids.size() > kMaxReplicatedItems) {
                return false;
            }

            if (!writer.WriteUInt16(static_cast<std::uint16_t>(component->item_ids.size())) ||
                !writer.WriteUInt16(component->capacity)) {
                return false;
            }

            for (std::uint32_t item_id: component->item_ids) {
                if (!writer.WriteUInt32(item_id)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] bool ApplyInventory(CoreEngine::World &world,
                                          CoreEngine::Node node,
                                          CoreEngine::MessageReader &reader) {
            (void) world;
            InventoryComponent &component = EnsureComponent<InventoryComponent>(node);
            std::uint16_t count = 0;
            if (!reader.ReadUInt16(count) || count > kMaxReplicatedItems ||
                !reader.ReadUInt16(component.capacity)) {
                return false;
            }

            component.item_ids.clear();
            component.item_ids.reserve(count);
            for (std::uint16_t i = 0; i < count; ++i) {
                std::uint32_t item_id = 0;
                if (!reader.ReadUInt32(item_id)) {
                    component.item_ids.clear();
                    return false;
                }
                component.item_ids.push_back(item_id);
            }

            return true;
        }

        [[nodiscard]] bool SerializeEquipment(const CoreEngine::World &world,
                                              entt::entity entity,
                                              CoreEngine::MessageWriter &writer) {
            const auto *component = world.TryGetComponent<EquipmentComponent>(entity);
            return component != nullptr &&
                   writer.WriteUInt32(component->weapon_item_id) &&
                   writer.WriteUInt32(component->gadget_item_id) &&
                   writer.WriteUInt8(component->selected_slot);
        }

        [[nodiscard]] bool ApplyEquipment(CoreEngine::World &world,
                                          CoreEngine::Node node,
                                          CoreEngine::MessageReader &reader) {
            (void) world;
            EquipmentComponent &component = EnsureComponent<EquipmentComponent>(node);
            return reader.ReadUInt32(component.weapon_item_id) &&
                   reader.ReadUInt32(component.gadget_item_id) &&
                   reader.ReadUInt8(component.selected_slot);
        }

        [[nodiscard]] bool SerializeBountyBeacon(const CoreEngine::World &world,
                                                 entt::entity entity,
                                                 CoreEngine::MessageWriter &writer) {
            const auto *component = world.TryGetComponent<BountyBeaconComponent>(entity);
            return component != nullptr &&
                   writer.WriteUInt64(component->original_owner_player) &&
                   writer.WriteUInt64(component->current_carrier_player) &&
                   writer.WriteBool(component->on_ground) &&
                   writer.WriteBool(component->extracted);
        }

        [[nodiscard]] bool ApplyBountyBeacon(CoreEngine::World &world,
                                             CoreEngine::Node node,
                                             CoreEngine::MessageReader &reader) {
            (void) world;
            BountyBeaconComponent &component = EnsureComponent<BountyBeaconComponent>(node);
            return reader.ReadUInt64(component.original_owner_player) &&
                   reader.ReadUInt64(component.current_carrier_player) &&
                   reader.ReadBool(component.on_ground) &&
                   reader.ReadBool(component.extracted);
        }

        [[nodiscard]] bool SerializeBountyBeaconCarrier(const CoreEngine::World &world,
                                                        entt::entity entity,
                                                        CoreEngine::MessageWriter &writer) {
            const auto *component = world.TryGetComponent<BountyBeaconCarrierComponent>(entity);
            if (component == nullptr || component->carried_beacons.size() > kMaxReplicatedItems) {
                return false;
            }

            if (!writer.WriteUInt16(static_cast<std::uint16_t>(component->carried_beacons.size()))) {
                return false;
            }

            for (CoreEngine::NetworkEntityId beacon: component->carried_beacons) {
                if (!writer.WriteUInt64(beacon)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] bool ApplyBountyBeaconCarrier(CoreEngine::World &world,
                                                    CoreEngine::Node node,
                                                    CoreEngine::MessageReader &reader) {
            (void) world;
            BountyBeaconCarrierComponent &component = EnsureComponent<BountyBeaconCarrierComponent>(node);
            std::uint16_t count = 0;
            if (!reader.ReadUInt16(count) || count > kMaxReplicatedItems) {
                return false;
            }

            component.carried_beacons.clear();
            component.carried_beacons.reserve(count);
            for (std::uint16_t i = 0; i < count; ++i) {
                CoreEngine::NetworkEntityId beacon = 0;
                if (!reader.ReadUInt64(beacon)) {
                    component.carried_beacons.clear();
                    return false;
                }
                component.carried_beacons.push_back(beacon);
            }

            return true;
        }

        [[nodiscard]] bool SerializeCapture(const CoreEngine::World &world,
                                            entt::entity entity,
                                            CoreEngine::MessageWriter &writer) {
            const auto *component = world.TryGetComponent<CaptureStateComponent>(entity);
            return component != nullptr &&
                   writer.WriteUInt64(component->captor_player) &&
                   writer.WriteFloat(component->cast_remaining_seconds) &&
                   writer.WriteBool(component->capturable) &&
                   writer.WriteBool(component->captured);
        }

        [[nodiscard]] bool ApplyCapture(CoreEngine::World &world,
                                        CoreEngine::Node node,
                                        CoreEngine::MessageReader &reader) {
            (void) world;
            CaptureStateComponent &component = EnsureComponent<CaptureStateComponent>(node);
            return reader.ReadUInt64(component.captor_player) &&
                   reader.ReadFloat(component.cast_remaining_seconds) &&
                   reader.ReadBool(component.capturable) &&
                   reader.ReadBool(component.captured);
        }

        [[nodiscard]] bool SerializeExtraction(const CoreEngine::World &world,
                                               entt::entity entity,
                                               CoreEngine::MessageWriter &writer) {
            const auto *component = world.TryGetComponent<ExtractionStateComponent>(entity);
            return component != nullptr &&
                   writer.WriteUInt8(static_cast<std::uint8_t>(component->state)) &&
                   writer.WriteFloat(component->timer_seconds) &&
                   writer.WriteBool(component->public_event_active);
        }

        [[nodiscard]] bool ApplyExtraction(CoreEngine::World &world,
                                           CoreEngine::Node node,
                                           CoreEngine::MessageReader &reader) {
            (void) world;
            ExtractionStateComponent &component = EnsureComponent<ExtractionStateComponent>(node);
            std::uint8_t state = 0;
            if (!reader.ReadUInt8(state) ||
                !reader.ReadFloat(component.timer_seconds) ||
                !reader.ReadBool(component.public_event_active)) {
                return false;
            }

            component.state = static_cast<ExtractionState>(state);
            return true;
        }

        [[nodiscard]] bool SerializeTargetAssignment(const CoreEngine::World &world,
                                                     entt::entity entity,
                                                     CoreEngine::MessageWriter &writer) {
            const auto *component = world.TryGetComponent<TargetAssignmentComponent>(entity);
            return component != nullptr &&
                   writer.WriteUInt64(component->target_player) &&
                   writer.WriteUInt64(component->hunter_player) &&
                   writer.WriteUInt64(component->required_beacon) &&
                   writer.WriteUInt8(static_cast<std::uint8_t>(component->state));
        }

        [[nodiscard]] bool ApplyTargetAssignment(CoreEngine::World &world,
                                                 CoreEngine::Node node,
                                                 CoreEngine::MessageReader &reader) {
            (void) world;
            TargetAssignmentComponent &component = EnsureComponent<TargetAssignmentComponent>(node);
            std::uint8_t state = 0;
            if (!reader.ReadUInt64(component.target_player) ||
                !reader.ReadUInt64(component.hunter_player) ||
                !reader.ReadUInt64(component.required_beacon) ||
                !reader.ReadUInt8(state)) {
                return false;
            }

            component.state = static_cast<TargetObjectiveState>(state);
            return true;
        }

        [[nodiscard]] bool SerializeAIState(const CoreEngine::World &world,
                                            entt::entity entity,
                                            CoreEngine::MessageWriter &writer) {
            const auto *component = world.TryGetComponent<AIStateComponent>(entity);
            return component != nullptr &&
                   writer.WriteUInt8(static_cast<std::uint8_t>(component->state)) &&
                   writer.WriteUInt64(component->target_entity);
        }

        [[nodiscard]] bool ApplyAIState(CoreEngine::World &world,
                                        CoreEngine::Node node,
                                        CoreEngine::MessageReader &reader) {
            (void) world;
            AIStateComponent &component = EnsureComponent<AIStateComponent>(node);
            std::uint8_t state = 0;
            if (!reader.ReadUInt8(state) ||
                !reader.ReadUInt64(component.target_entity)) {
                return false;
            }

            component.state = static_cast<AIState>(state);
            return true;
        }

        [[nodiscard]] bool RegisterComponent(CoreEngine::MultiplayerSystem &multiplayer,
                                             CoreEngine::ReplicatedComponentDesc desc) noexcept {
            return multiplayer.RegisterReplicatedComponent(desc);
        }
    } // namespace

    bool RegisterBountyReplicatedComponents(CoreEngine::MultiplayerSystem &multiplayer) noexcept {
        bool ok = true;
        ok = RegisterComponent(multiplayer, {
                 .component_type_id = kHealthComponentTypeId,
                 .serialization_version = 1,
                 .authority = CoreEngine::AuthorityPolicy::ServerOnly,
                 .reliability = CoreEngine::ReplicationReliability::UnreliableSnapshot,
                 .max_send_rate = 20,
                 .flags = static_cast<std::uint32_t>(CoreEngine::ReplicatedComponentFlags::Critical),
                 .has_component = &HasComponent<HealthComponent>,
                 .serialize = &SerializeHealth,
                 .apply = &ApplyHealth,
             }) && ok;
        ok = RegisterComponent(multiplayer, {
                 .component_type_id = kArmorSegmentsComponentTypeId,
                 .serialization_version = 1,
                 .authority = CoreEngine::AuthorityPolicy::ServerOnly,
                 .reliability = CoreEngine::ReplicationReliability::UnreliableSnapshot,
                 .max_send_rate = 10,
                 .has_component = &HasComponent<ArmorSegmentsComponent>,
                 .serialize = &SerializeArmor,
                 .apply = &ApplyArmor,
             }) && ok;
        ok = RegisterComponent(multiplayer, {
                 .component_type_id = kInventoryComponentTypeId,
                 .serialization_version = 1,
                 .authority = CoreEngine::AuthorityPolicy::OwnerOnlyPrivate,
                 .reliability = CoreEngine::ReplicationReliability::ReliableEvent,
                 .flags = static_cast<std::uint32_t>(CoreEngine::ReplicatedComponentFlags::OwnerOnly),
                 .has_component = &HasComponent<InventoryComponent>,
                 .serialize = &SerializeInventory,
                 .apply = &ApplyInventory,
             }) && ok;
        ok = RegisterComponent(multiplayer, {
                 .component_type_id = kEquipmentComponentTypeId,
                 .serialization_version = 1,
                 .authority = CoreEngine::AuthorityPolicy::ServerOnly,
                 .reliability = CoreEngine::ReplicationReliability::ReliableEvent,
                 .has_component = &HasComponent<EquipmentComponent>,
                 .serialize = &SerializeEquipment,
                 .apply = &ApplyEquipment,
             }) && ok;
        ok = RegisterComponent(multiplayer, {
                 .component_type_id = kBountyBeaconComponentTypeId,
                 .serialization_version = 1,
                 .authority = CoreEngine::AuthorityPolicy::ServerOnly,
                 .reliability = CoreEngine::ReplicationReliability::ReliableEvent,
                 .flags = static_cast<std::uint32_t>(CoreEngine::ReplicatedComponentFlags::Critical),
                 .has_component = &HasComponent<BountyBeaconComponent>,
                 .serialize = &SerializeBountyBeacon,
                 .apply = &ApplyBountyBeacon,
             }) && ok;
        ok = RegisterComponent(multiplayer, {
                 .component_type_id = kBountyBeaconCarrierComponentTypeId,
                 .serialization_version = 1,
                 .authority = CoreEngine::AuthorityPolicy::ServerOnly,
                 .reliability = CoreEngine::ReplicationReliability::ReliableEvent,
                 .flags = static_cast<std::uint32_t>(CoreEngine::ReplicatedComponentFlags::Critical),
                 .has_component = &HasComponent<BountyBeaconCarrierComponent>,
                 .serialize = &SerializeBountyBeaconCarrier,
                 .apply = &ApplyBountyBeaconCarrier,
             }) && ok;
        ok = RegisterComponent(multiplayer, {
                 .component_type_id = kCaptureStateComponentTypeId,
                 .serialization_version = 1,
                 .authority = CoreEngine::AuthorityPolicy::ServerOnly,
                 .reliability = CoreEngine::ReplicationReliability::ReliableEvent,
                 .flags = static_cast<std::uint32_t>(CoreEngine::ReplicatedComponentFlags::Critical),
                 .has_component = &HasComponent<CaptureStateComponent>,
                 .serialize = &SerializeCapture,
                 .apply = &ApplyCapture,
             }) && ok;
        ok = RegisterComponent(multiplayer, {
                 .component_type_id = kExtractionStateComponentTypeId,
                 .serialization_version = 1,
                 .authority = CoreEngine::AuthorityPolicy::ServerOnly,
                 .reliability = CoreEngine::ReplicationReliability::ReliableEvent,
                 .flags = static_cast<std::uint32_t>(CoreEngine::ReplicatedComponentFlags::Critical),
                 .has_component = &HasComponent<ExtractionStateComponent>,
                 .serialize = &SerializeExtraction,
                 .apply = &ApplyExtraction,
             }) && ok;
        ok = RegisterComponent(multiplayer, {
                 .component_type_id = kTargetAssignmentComponentTypeId,
                 .serialization_version = 1,
                 .authority = CoreEngine::AuthorityPolicy::OwnerOnlyPrivate,
                 .reliability = CoreEngine::ReplicationReliability::ReliableEvent,
                 .flags = static_cast<std::uint32_t>(CoreEngine::ReplicatedComponentFlags::OwnerOnly),
                 .has_component = &HasComponent<TargetAssignmentComponent>,
                 .serialize = &SerializeTargetAssignment,
                 .apply = &ApplyTargetAssignment,
             }) && ok;
        ok = RegisterComponent(multiplayer, {
                 .component_type_id = kAIStateComponentTypeId,
                 .serialization_version = 1,
                 .authority = CoreEngine::AuthorityPolicy::ServerOnly,
                 .reliability = CoreEngine::ReplicationReliability::UnreliableSnapshot,
                 .max_send_rate = 10,
                 .has_component = &HasComponent<AIStateComponent>,
                 .serialize = &SerializeAIState,
                 .apply = &ApplyAIState,
             }) && ok;
        return ok;
    }
} // namespace Game
