#pragma once
#include "i_possessable.h"

namespace TopDownGame {

    class Character : public IPossessable {
    public:
        void OnPossessed() override;
        void OnUnpossessed() override;

    private:
        bool is_possessed_ = false;
    };

} // namespace TopDownGame
