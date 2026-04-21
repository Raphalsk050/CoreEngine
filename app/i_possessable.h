#pragma once

namespace Game {
    class IPossessable {
    public:
        virtual ~IPossessable() = default;

        virtual void Possess() = 0;

        virtual void UnPossess() = 0;
    };
}
