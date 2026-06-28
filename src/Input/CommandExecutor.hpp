#pragma once
#include <vector>
#include "Command.hpp"
#include "ActiveMask.hpp"
#include "InputFocus.hpp"
#include "../CameraController/CameraController.hpp" // Проверь этот путь!

namespace shuttle_engine {

    class CommandExecutor {
    public:
        static void executeQueue(
            const std::vector<Command>& optimizedCommands,
            CameraController& cameraController,
            ActiveMask& activeMask,
            InputFocusState& focusState)
        {
            for (const auto& cmd : optimizedCommands) {
                switch (cmd.type) {
                    case CommandType::MoveForward:
                    case CommandType::MoveBackward:
                    case CommandType::MoveLeft:
                    case CommandType::MoveRight:
                    case CommandType::MoveUp:
                    case CommandType::MoveDown:
                    case CommandType::Rotate:
                        cameraController.handleCommand(cmd);
                        break;

                    case CommandType::ToggleConsole:
                        // Логика переключения фокуса ввода на лету
                        if (focusState == InputFocusState::Game) {
                            focusState = InputFocusState::DebugUI;
                            activeMask.enable(EngineModule::DebugUI);
                            std::cout << "[Executor] Focus shifted to Debug UI.\n";
                        } else {
                            focusState = InputFocusState::Game;
                            activeMask.disable(EngineModule::DebugUI);
                            std::cout << "[Executor] Focus shifted to Game.\n";
                        }
                        break;

                    default:
                        break;
                }
            }
        }
    };

} // namespace shuttle_engine
