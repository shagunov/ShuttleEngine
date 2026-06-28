//
// Created by Shagu on 18.06.2026.
//

#ifndef HELLOTRIANGLE_COMMANDBUFFER_HPP
#define HELLOTRIANGLE_COMMANDBUFFER_HPP

#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "Command.hpp"

namespace shuttle_engine {

    inline int getCommandPriority(CommandType type) {
        switch (type) {
            case CommandType::CloseApplication: return 0;
            case CommandType::ToggleConsole:    return 1;
            case CommandType::ToggleUI:         return 2;
            case CommandType::MoveForward:
            case CommandType::MoveBackward:
            case CommandType::MoveLeft:
            case CommandType::MoveRight:
            case CommandType::MoveUp:
            case CommandType::MoveDown:
            case CommandType::Rotate:           return 3;
            default:                            return 4;
        }
    }

    class CommandBuffer {
    public:
        CommandBuffer() {
            commands.reserve(128);
        }

        void push(const Command& cmd) {
            commands.push_back(cmd);
        }

        void clear() {
            commands.clear();
        }

        [[nodiscard]] const std::vector<Command>& getCommands() const {
            return commands;
        }

        void optimize() {
            if (commands.empty()) return;

            bool hasRotate = false;
            float totalDx = 0.0f;
            float totalDy = 0.0f;

            auto it = commands.begin();
            while (it != commands.end()) {
                if (it->type == CommandType::Rotate) {
                    hasRotate = true;
                    totalDx += it->vec2.x;
                    totalDy += it->vec2.y;
                    it = commands.erase(it);
                } else {
                    ++it;
                }
            }

            if (hasRotate) {
                Command rotateCmd;
                rotateCmd.type = CommandType::Rotate;
                rotateCmd.vec2.x = totalDx;
                rotateCmd.vec2.y = totalDy;
                commands.push_back(rotateCmd);
            }

            std::stable_sort(commands.begin(), commands.end(), [](const Command& a, const Command& b) {
                return getCommandPriority(a.type) < getCommandPriority(b.type);
            });
        }

    private:
        std::vector<Command> commands;
    };

} // namespace shuttle_engine


#endif //HELLOTRIANGLE_COMMANDBUFFER_HPP
