#pragma once
#include "i_possessable.h"

namespace Game {
    class IController {
    public:
        virtual ~IController() = default;

        virtual void Possess(IPossessable &possessable) = 0;

        virtual void Unpossess() = 0;
    };
} // Game
