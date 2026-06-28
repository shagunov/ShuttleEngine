//
// Created by Shagu on 23.06.2026.
//

#ifndef HELLOTRIANGLE_QUEUEEXECUTIONCONTROLLER_HPP
#define HELLOTRIANGLE_QUEUEEXECUTIONCONTROLLER_HPP
#include <concurrentqueue.h>
#include "IncludeVulkan.hpp"
#include "execution/FenceWaiter/FenceWaiter.hpp"

namespace shuttle_engine::execution {
    class QueueExecutionController {
    public:
    private:
        moodycamel::ConcurrentQueue<vk::Fence> freeFencePool;
        moodycamel::ConcurrentQueue<vk::Fence> waitFences;

        FenceWaiter fenceWaiter;
    };
} // shuttle_engine

#endif //HELLOTRIANGLE_QUEUEEXECUTIONCONTROLLER_HPP
