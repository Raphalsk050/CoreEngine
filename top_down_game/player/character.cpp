#include "character.h"

namespace TopDownGame {

    void Character::OnPossessed() { is_possessed_ = true; }
    void Character::OnUnpossessed() { is_possessed_ = false; }
} // namespace TopDownGame
