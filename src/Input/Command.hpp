//
// Created by Shagu on 18.06.2026.
//

#ifndef HELLOTRIANGLE_COMMAND_HPP
#define HELLOTRIANGLE_COMMAND_HPP
#include <cstdint>

namespace shuttle_engine {

    enum class CommandType : uint32_t {
        None = 0,
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        Rotate,
        ToggleConsole,
        ToggleUI,
        CloseApplication
    };

    struct Command {
        CommandType type = CommandType::None;
        union {
            float value;
            struct {
                float x;
                float y;
            } vec2;
        };
    };

} // namespace shuttle_engine


#endif //HELLOTRIANGLE_COMMAND_HPP
