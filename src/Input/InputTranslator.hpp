//
// Created by Shagu on 18.06.2026.
//

#ifndef HELLOTRIANGLE_INPUTTRANSLATOR_HPP
#define HELLOTRIANGLE_INPUTTRANSLATOR_HPP

#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include "Command.hpp"
#include "ModularInputSettings.hpp"

namespace shuttle_engine {

    class InputTranslator {
    public:
        // --- ДОБАВЬ ЭТУ СТАТИЧЕСКУЮ ФУНКЦИЮ-ХЕЛПЕР ---
        static bool isMovementCommand(CommandType type) {
            return type == CommandType::MoveForward ||
                   type == CommandType::MoveBackward ||
                   type == CommandType::MoveLeft ||
                   type == CommandType::MoveRight ||
                   type == CommandType::MoveUp ||
                   type == CommandType::MoveDown ||
                   type == CommandType::Rotate; // Rotate - это тоже "движение" (взгляд)
        }
        // ----------------------------------------------

        // Переводит клавишу на основе списка АКТИВНЫХ в данный момент контекстов
        static Command translateKey(
            const SDL_KeyboardEvent& keyEvent,
            const ModularInputSettings& settings,
            const std::vector<std::string>& activeContexts)
        {
            Command cmd;
            cmd.type = CommandType::None;
            cmd.value = (keyEvent.state == SDL_PRESSED) ? 1.0f : 0.0f;

            // Ищем клавишу по очереди во всех активных контекстах
            for (const auto& contextName : activeContexts) {
                auto mapIt = settings.moduleMaps.find(contextName);
                if (mapIt != settings.moduleMaps.end()) {
                    const auto& keyBindings = mapIt->second.keyBindings;
                    auto keyIt = keyBindings.find(keyEvent.keysym.sym);
                    if (keyIt != keyBindings.end()) {
                        cmd.type = keyIt->second;
                        if (keyEvent.state == SDL_PRESSED) { // Генерируем команду ТОЛЬКО при нажатии
                            // И только если это не команда движения, которая должна быть 1.0f / 0.0f
                            if (isMovementCommand(cmd.type)) {
                                cmd.value = 1.0f; // При движении всегда 1.0f при нажатии
                            }
                            return cmd;
                        } else {
                            // Это KEYUP или не нажата (если это не Move, то просто None)
                            if (isMovementCommand(cmd.type)) {
                                cmd.value = 0.0f; // При отпускании - 0.0f
                                return cmd;
                            }
                            cmd.type = CommandType::None; // Для ToggleConsole KEYUP не генерируем команду
                            return cmd;
                        }
                        return cmd; // Возвращаем первую найденную команду
                    }
                }
            }
            return cmd;
        }

        // Переводит дельту движения мыши в команду вращения
        static Command translateMouseMove(const SDL_MouseMotionEvent& mouseEvent) {
            Command cmd;
            cmd.type = CommandType::Rotate;
            cmd.vec2.x = static_cast<float>(mouseEvent.xrel);
            cmd.vec2.y = static_cast<float>(mouseEvent.yrel);
            return cmd;
        }
    };

} // namespace shuttle_engine

#endif //HELLOTRIANGLE_INPUTTRANSLATOR_HPP
