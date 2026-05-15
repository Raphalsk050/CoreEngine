#include "gameplay/systems/armor_system.h"

#include <algorithm>

namespace Game {
    float ArmorSystem::ApplyDamage(CoreEngine::ArmorSegmentsComponent &armor,
                                   HitRegion region,
                                   float damage) const noexcept {
        CoreEngine::ArmorPart *part = &armor.torso;
        switch (region) {
            case HitRegion::Head:
                part = &armor.head;
                break;
            case HitRegion::Torso:
                part = &armor.torso;
                break;
            case HitRegion::LeftArm:
                part = &armor.left_arm;
                break;
            case HitRegion::RightArm:
                part = &armor.right_arm;
                break;
            case HitRegion::Legs:
                part = &armor.legs;
                break;
        }

        const float absorbed = std::min(part->hit_points, damage);
        part->hit_points -= absorbed;
        return damage - absorbed;
    }

    void ArmorSystem::FixedUpdate(const GameplaySystemContext &context) noexcept {
        (void) context;
    }
} // namespace Game
