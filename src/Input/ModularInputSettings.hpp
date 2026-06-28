//
// Created by Shagu on 18.06.2026.
//

#ifndef HELLOTRIANGLE_MODULARSETTINGS_HPP
#define HELLOTRIANGLE_MODULARSETTINGS_HPP
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <SDL2/SDL.h>
#include "Command.hpp"

namespace shuttle_engine {

    struct ActionMap {
        std::unordered_map<SDL_Keycode, CommandType> keyBindings;
    };

    class ModularInputSettings {
    public:
        std::unordered_map<std::string, ActionMap> moduleMaps;

        bool loadFromManifest(const std::string& manifestPath) {
            std::ifstream manifestFile(manifestPath);
            if (!manifestFile.is_open()) {
                std::cerr << "[InputSettings] ERROR: Failed to open manifest: " << manifestPath << "\n";
                std::cerr << "[InputSettings] Loading hardcoded fallback bindings...\n";
                loadFallbacks();
                return false;
            }

            moduleMaps.clear();
            std::string line;
            uint32_t lineNum = 0;

            while (std::getline(manifestFile, line)) {
                lineNum++;
                trim(line);
                if (line.empty() || line[0] == '#') continue;

                size_t eqPos = line.find('=');
                if (eqPos == std::string::npos) continue;

                std::string moduleName = line.substr(0, eqPos);
                std::string configPath = line.substr(eqPos + 1);
                trim(moduleName);
                trim(configPath);

                loadModuleConfig(moduleName, configPath);
            }

            return true;
        }

    private:
        void loadModuleConfig(const std::string& moduleName, const std::string& filePath) {
            std::ifstream file(filePath);
            if (!file.is_open()) {
                std::cerr << "[InputSettings] Failed to open config for [" << moduleName << "] at: " << filePath << "\n";
                return;
            }

            ActionMap map;
            std::string line;
            while (std::getline(file, line)) {
                trim(line);
                if (line.empty() || line[0] == '#') continue;

                size_t eqPos = line.find('=');
                if (eqPos == std::string::npos) continue;

                std::string cmdName = line.substr(0, eqPos);
                std::string keyName = line.substr(eqPos + 1);
                trim(cmdName);
                trim(keyName);

                CommandType cmdType = stringToCommand(cmdName);
                SDL_Keycode keycode = SDL_GetKeyFromName(keyName.c_str());

                if (cmdType != CommandType::None && keycode != SDLK_UNKNOWN) {
                    map.keyBindings[keycode] = cmdType;
                }
            }

            moduleMaps[moduleName] = map;
            std::cout << "[InputSettings] Module [" << moduleName << "] loaded. Bindings count: " << map.keyBindings.size() << "\n";
        }

        static void trim(std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
        }

        static CommandType stringToCommand(const std::string& name) {
            if (name == "MoveForward")      return CommandType::MoveForward;
            if (name == "MoveBackward")     return CommandType::MoveBackward;
            if (name == "MoveLeft")         return CommandType::MoveLeft;
            if (name == "MoveRight")        return CommandType::MoveRight;
            if (name == "MoveUp")           return CommandType::MoveUp;
            if (name == "MoveDown")         return CommandType::MoveDown;
            if (name == "ToggleConsole")    return CommandType::ToggleConsole;
            if (name == "ToggleUI")         return CommandType::ToggleUI;
            if (name == "CloseApplication") return CommandType::CloseApplication;
            return CommandType::None;
        }

        void loadFallbacks() {
            ActionMap global, camera, ui;
            global.keyBindings[SDLK_ESCAPE] = CommandType::CloseApplication;
            camera.keyBindings[SDLK_w] = CommandType::MoveForward;
            camera.keyBindings[SDLK_s] = CommandType::MoveBackward;
            camera.keyBindings[SDLK_a] = CommandType::MoveLeft;
            camera.keyBindings[SDLK_d] = CommandType::MoveRight;
            ui.keyBindings[SDLK_BACKQUOTE] = CommandType::ToggleConsole;

            moduleMaps["Global"] = global;
            moduleMaps["Camera"] = camera;
            moduleMaps["DebugUI"] = ui;
        }
    };

} // namespace shuttle_engine

#endif //HELLOTRIANGLE_MODULARSETTINGS_HPP
