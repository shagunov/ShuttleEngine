//
// Created by Shagu on 18.06.2026.
//

#ifndef HELLOTRIANGLE_ACTIVEMASK_HPP
#define HELLOTRIANGLE_ACTIVEMASK_HPP
#include <cstdint>

namespace shuttle_engine {

    enum class EngineModule : uint32_t {
        None        = 0,
        CameraMove  = 1 << 0,
        CameraLook  = 1 << 1,
        DebugUI     = 1 << 2,
        GameWorld   = 1 << 3
    };

    struct ActiveMask {
        uint32_t mask = 0xFFFFFFFF;

        void enable(EngineModule module) { mask |= static_cast<uint32_t>(module); }
        void disable(EngineModule module) { mask &= ~static_cast<uint32_t>(module); }
        bool isEnabled(EngineModule module) const { return (mask & static_cast<uint32_t>(module)) != 0; }
        void toggle(EngineModule module) { mask ^= static_cast<uint32_t>(module); }
    };
} // namespace shuttle_engine

#endif //HELLOTRIANGLE_ACTIVEMASK_HPP
