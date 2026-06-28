#pragma once
#include "../Camera/Camera.hpp" // Проверь этот путь!
#include "../Input/Command.hpp"  // Проверь этот путь!

namespace shuttle_engine {

    class CameraController {
    public:
        explicit CameraController(Camera& camera);

        // Принимает нашу команду вместо сырых SDL-событий
        void handleCommand(const Command& cmd);

        // Физическое обновление позиции (вызывать каждый кадр)
        void update(float deltaTime);

    private:
        Camera& camera;

        enum MoveDir : uint32_t {
            None = 0,
            Forward = 1 << 0,
            Backward = 1 << 1,
            Left = 1 << 2,
            Right = 1 << 3,
            Up = 1 << 4,
            Down = 1 << 5
        };

        uint32_t moveFlags = None;

        // Аккумулированная дельта мыши за кадр
        float mouseDeltaX = 0.0f;
        float mouseDeltaY = 0.0f;
    };

} // namespace shuttle_engine
