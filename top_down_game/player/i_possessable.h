#pragma once

namespace TopDownGame {
    class IPossessable {
    public:
        virtual ~IPossessable() = default;
        virtual void OnPossessed() = 0;
        virtual void OnUnpossessed() = 0;
    };
} // namespace TopDownGame
