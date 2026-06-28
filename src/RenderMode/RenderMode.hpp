//
// Created by Shagu on 19.06.2026.
//

#ifndef HELLOTRIANGLE_RENDERMODE_HPP
#define HELLOTRIANGLE_RENDERMODE_HPP
#include "Engine/Engine.hpp"

namespace shuttle_engine {
    class RenderMode {
    public:
        virtual void enter(Engine& engine) = 0;
        virtual void exit(Engine& engine) = 0;

        virtual void prepareFrame(Engine& engine, uint32_t frameIndex, float deltaTime) = 0;
        virtual void recordDrawFrame(vk::CommandBuffer commandBuffer, Engine& engine, uint32_t frameIndex, uint32_t imageIndex) = 0;
        virtual ~RenderMode() = default;
    };
} // shattle_engine

#endif //HELLOTRIANGLE_RENDERMODE_HPP
