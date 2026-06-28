//
// Created by Shagu on 23.06.2026.
//

#ifndef HELLOTRIANGLE_FENCEWAITER_HPP
#define HELLOTRIANGLE_FENCEWAITER_HPP
#include "IncludeVulkan.hpp"
#include <vector>

namespace shuttle_engine::execution {

    struct

    class FenceWaiter {
        void addFence();
    private:
        std::vector<vk::Fence> fencePool;
        std::vector<vk::Fence> fenceWaitQueue;
    };
}

#endif //HELLOTRIANGLE_FENCEWAITER_HPP