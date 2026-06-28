#pragma once
#include <imgui.h>
#include "../../UiRender/UiRender.hpp" // Для IuiPainter
#include "../../UiRender/UiRender.hpp" // Это не дублирование, просто инклуд для FPSCounterPainter и т.д.
#include "../../PbrRender/Render.hpp" // Для PbrRender (чтобы передать в SunLightControlPanel)
#include "../../Painters/SunLightControlPanel/SunLightControlPanel.hpp" // Твоя панель
#include "../../Input/ActiveMask.hpp"
#include "../../Input/InputFocus.hpp"
#include "../../Input/ModularInputSettings.hpp"
#include "../../Camera/Camera.hpp"

// Forward declaration для Engine (повтор, чтобы быть уверенным)
namespace shuttle_engine { class Engine; }

namespace shuttle_engine {

    class DebugOverlayPainter : public IuiPainter {
    public:
        DebugOverlayPainter() = delete;
        DebugOverlayPainter(FPSCounterPainter fpsCounterPainter, SunLightControlPanel sunLightControlPanel) : fpsCounterPainter(fpsCounterPainter), sunLightControlPanel(sunLightControlPanel) {}

        // Здесь агрегируем все наши дебаг-панели
        void drawUi(Engine& engine) override {
            ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver); // Размер окна

            if (ImGui::Begin("Debug Overlay", nullptr, ImGuiWindowFlags_MenuBar)) { // Главное окно дебага
                if (ImGui::BeginMenuBar()) {
                    if (ImGui::BeginMenu("Panels")) {
                        // Здесь можно добавлять чекбоксы для включения/выключения отдельных панелей
                        ImGui::MenuItem("Performance", nullptr, &showFpsCounter);
                        ImGui::MenuItem("Light Settings", nullptr, &showLightSettings);
                        ImGui::MenuItem("Camera Settings", nullptr, &showCameraSettings);
                        ImGui::MenuItem("Input Settings", nullptr, &showInputSettings);
                        ImGui::MenuItem("Engine State", nullptr, &showEngineState);
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenuBar();
                }


                // --- Вывод всех панелей ---
                if (showFpsCounter) {
                    fpsCounterPainter.drawUi(engine); // Передаем Engine дальше
                }
                if (showLightSettings) {
                    sunLightControlPanel.drawUi(engine); // Твоя панель света
                }
                if (showCameraSettings) {
                    drawCameraSettingsPanel(engine);
                }
                if (showInputSettings) {
                    drawInputSettingsPanel(engine);
                }
                if (showEngineState) {
                    drawEngineStatePanel(engine);
                }

            }
            ImGui::End(); // End Debug Overlay
        }

    private:
        // Инстансы всех под-панелей
        FPSCounterPainter fpsCounterPainter;
        SunLightControlPanel sunLightControlPanel; // Твоя существующая панель

        // Флаги видимости панелей
        bool showFpsCounter = true;
        bool showLightSettings = true;
        bool showCameraSettings = true;
        bool showInputSettings = true;
        bool showEngineState = true;

        // --- Новые дебаг-панели ---
        void drawCameraSettingsPanel(Engine& engine) {
            ImGui::Begin("Camera Settings", &showCameraSettings);
            ImGui::Text("Position: %.2f, %.2f, %.2f", engine.camera.getPosition().x, engine.camera.getPosition().y, engine.camera.getPosition().z);
            ImGui::Text("Direction: %.2f, %.2f, %.2f", engine.camera.getForwardVector().x, engine.camera.getForwardVector().y, engine.camera.getForwardVector().z);
            float speed = engine.camera.getMovementSpeed();
            if (ImGui::SliderFloat("Speed", &speed, 1.0f, 100.0f)) {
                engine.camera.setMovementSpeed(speed);
            }
            ImGui::End();
        }

        void drawInputSettingsPanel(Engine& engine) {
            ImGui::Begin("Input Settings", &showInputSettings);
            if (ImGui::Button("Reload Input Bindings")) {
                engine.inputSettings.loadFromManifest("config/input/manifest.cfg");
            }
            ImGui::Text("--- Active Bindings ---");
            for (const auto& [moduleName, actionMap] : engine.inputSettings.moduleMaps) {
                if (ImGui::TreeNode(moduleName.c_str())) {
                    for (const auto& [keycode, cmdType] : actionMap.keyBindings) {
                        ImGui::Text("%s = %d (%s)", SDL_GetKeyName(keycode), static_cast<int>(cmdType), commandTypeToString(cmdType));
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::End();
        }

        void drawEngineStatePanel(Engine& engine) {
            ImGui::Begin("Engine State", &showEngineState);
            ImGui::Text("Focus State: %s", engine.focusState == InputFocusState::Game ? "Game" : "Debug UI");
            ImGui::Text("Active Modules:");
            if (engine.activeMask.isEnabled(EngineModule::CameraMove)) ImGui::BulletText("Camera Move");
            if (engine.activeMask.isEnabled(EngineModule::CameraLook)) ImGui::BulletText("Camera Look");
            if (engine.activeMask.isEnabled(EngineModule::DebugUI)) ImGui::BulletText("Debug UI");
            if (engine.activeMask.isEnabled(EngineModule::GameWorld)) ImGui::BulletText("Game World");
            ImGui::End();
        }

        // Вспомогательная функция для вывода имени команды
        const char* commandTypeToString(CommandType type) {
            switch (type) {
                case CommandType::MoveForward: return "MoveForward";
                case CommandType::MoveBackward: return "MoveBackward";
                case CommandType::MoveLeft: return "MoveLeft";
                case CommandType::MoveRight: return "MoveRight";
                case CommandType::MoveUp: return "MoveUp";
                case CommandType::MoveDown: return "MoveDown";
                case CommandType::Rotate: return "Rotate";
                case CommandType::ToggleConsole: return "ToggleConsole";
                case CommandType::ToggleUI: return "ToggleUI";
                case CommandType::CloseApplication: return "CloseApplication";
                case CommandType::None: return "None";
                default: return "Unknown";
            }
        }
    };

} // namespace shuttle_engine