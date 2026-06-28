//
// Created by Shagu on 18.06.2026.
//

#ifndef HELLOTRIANGLE_APPLICATIONCONTROLLER_HPP
#define HELLOTRIANGLE_APPLICATIONCONTROLLER_HPP
#include <vector>
#include <string>
#include <SDL2/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>

#include "Sdl/SdlLibrary/SdlLibrary.hpp"
#include "Sdl/SdlWindow/SdlWindow.hpp"
#include "Command.hpp"
#include "CommandBuffer.hpp"
#include "InputTranslator.hpp"
#include "ActiveMask.hpp"
#include "InputFocus.hpp"
#include "ModularInputSettings.hpp"

namespace shuttle_engine {

    class ApplicationController {
    public:
        static bool processEvents(
            SdlLibrary& sdlLib,
            SdlWindow& window,
            ActiveMask& activeMask,
            InputFocusState& focusState,
            const ModularInputSettings& inputSettings,
            CommandBuffer& commandBuffer) // Используем наш CommandBuffer!
        {
            SDL_Event event;
            bool keepRunning = true;

            // Динамически собираем список активных контекстов
            std::vector<std::string> activeContexts;
            activeContexts.push_back("Global"); // Глобальный ввод активен всегда

            if (focusState == InputFocusState::Game) {
                if (activeMask.isEnabled(EngineModule::CameraMove)) {
                    activeContexts.push_back("Camera");
                }
            } else if (focusState == InputFocusState::DebugUI) {
                if (activeMask.isEnabled(EngineModule::DebugUI)) {
                    activeContexts.push_back("DebugUI");
                }
            }

            while (sdlLib.pollEvent(event)) {
                if (event.type == SDL_QUIT) {
                    keepRunning = false;
                    continue;
                }

                // Передача событий в ImGui (если UI активен)
                if (activeMask.isEnabled(EngineModule::DebugUI)) {
                    // Важно: эта функция объявлена внутри ImGui_ImplSDL2
                    ImGui_ImplSDL2_ProcessEvent(&event);

                    ImGuiIO& io = ImGui::GetIO();
                    if (focusState == InputFocusState::DebugUI) {
                        // Если ImGui забрал мышь или клаву — не пускаем событие в игру
                        if (io.WantCaptureKeyboard || io.WantCaptureMouse) {
                            if (event.type == SDL_KEYDOWN) {
                                Command cmd = InputTranslator::translateKey(event.key, inputSettings, activeContexts);
                                if (cmd.type == CommandType::ToggleConsole) {
                                    commandBuffer.push(cmd);
                                }
                                if (cmd.type == CommandType::CloseApplication) {
                                    keepRunning = false;
                                }
                            }
                            continue;
                        }
                    }
                }

                // Обработка клавиатурного ввода
                if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                    Command cmd = InputTranslator::translateKey(event.key, inputSettings, activeContexts);
                    if (cmd.type == CommandType::CloseApplication) {
                        keepRunning = false;
                    } else if (cmd.type != CommandType::None) {
                        commandBuffer.push(cmd);
                    }
                }

                // Обработка мыши (только в игровом режиме)
                if (event.type == SDL_MOUSEMOTION) {
                    if (activeMask.isEnabled(EngineModule::CameraLook) && focusState == InputFocusState::Game) {
                        Command cmd = InputTranslator::translateMouseMove(event.motion);
                        commandBuffer.push(cmd);
                    }
                }
            }

            // Синхронизируем состояние курсора мыши в зависимости от фокуса
            if (focusState == InputFocusState::Game) {
                sdlLib.setRelativeMouseMode(true);
            } else {
                sdlLib.setRelativeMouseMode(false);
            }

            return keepRunning;
        }
    };

} // namespace shuttle_engine
#endif //HELLOTRIANGLE_APPLICATIONCONTROLLER_HPP
