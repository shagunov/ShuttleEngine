#include "CameraController.hpp"
#include <cmath>

namespace shuttle_engine {

    CameraController::CameraController(Camera& camera) : camera(camera) {}

    void CameraController::handleCommand(const Command& cmd) {
        switch (cmd.type) {
            case CommandType::MoveForward:
                (cmd.value > 0.0f) ? moveFlags |= Forward : moveFlags &= ~Forward;
                break;
            case CommandType::MoveBackward:
                (cmd.value > 0.0f) ? moveFlags |= Backward : moveFlags &= ~Backward;
                break;
            case CommandType::MoveLeft:
                (cmd.value > 0.0f) ? moveFlags |= Left : moveFlags &= ~Left;
                break;
            case CommandType::MoveRight:
                (cmd.value > 0.0f) ? moveFlags |= Right : moveFlags &= ~Right;
                break;
            case CommandType::MoveUp:
                (cmd.value > 0.0f) ? moveFlags |= Up : moveFlags &= ~Up;
                break;
            case CommandType::MoveDown:
                (cmd.value > 0.0f) ? moveFlags |= Down : moveFlags &= ~Down;
                break;

            case CommandType::Rotate:
                mouseDeltaX += cmd.vec2.x;
                mouseDeltaY += cmd.vec2.y;
                break;

            default:
                break;
        }
    }

    void CameraController::update(float deltaTime) {
        // 1. Обработка перемещения
        glm::vec3 moveVec{0.0f};
        if (moveFlags & Forward)  moveVec.z -= 1.0f;
        if (moveFlags & Backward) moveVec.z += 1.0f;
        if (moveFlags & Left)     moveVec.x -= 1.0f;
        if (moveFlags & Right)    moveVec.x += 1.0f;
        if (moveFlags & Up)       moveVec.y += 1.0f;
        if (moveFlags & Down)     moveVec.y -= 1.0f;

        if (glm::length(moveVec) > 0.0f) {
            camera.moveLocal(glm::normalize(moveVec), deltaTime);
        }

        // 2. Обработка вращения (Pitch & Yaw)
        if (std::abs(mouseDeltaX) > 0.0f || std::abs(mouseDeltaY) > 0.0f) {
            constexpr float sensitivity = 0.1f;
            camera.rotateEuler(-mouseDeltaY * sensitivity, -mouseDeltaX * sensitivity, 0.0f, deltaTime);

            // Сбрасываем накопленные за кадр значения
            mouseDeltaX = 0.0f;
            mouseDeltaY = 0.0f;
        }
    }

} // namespace shuttle_engine
