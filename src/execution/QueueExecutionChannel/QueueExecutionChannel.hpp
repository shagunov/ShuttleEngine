//
// Created by Shagu on 23.06.2026.
//

#ifndef HELLOTRIANGLE_QUEUEEXECUTIONCHANNEL_HPP
#define HELLOTRIANGLE_QUEUEEXECUTIONCHANNEL_HPP
#include <coroutine>
#include <functional>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "IncludeVulkan.hpp"
#include "../Executor.hpp"
#include "concurrentqueue.h"
#include "exec/start_detached.hpp"
#include "exec/asio/completion_token.hpp"
#include "execution/FenceWaiter/FenceWaiter.hpp"

namespace shuttle_engine::execution {

    enum class DeviceExecutionStatus {
        ePending,
        eSuspended,
        eDone
    };


    class DeviceExecutionTask {
    public:
        std::atomic<DeviceExecutionStatus> currentStatus{};
        void setContinuation(std::coroutine_handle<> continuation, Executor* executor) {
            currentCoroutineHandle = continuation;
            this->mExecutor = executor;
        }
        [[nodiscard]] vk::Result getResult() const {
            return taskResult;
        }

        // Метод пробуждения (вызывается из FenceWaiter при срабатывании фенса)
        void signalGpuDone() noexcept {
            DeviceExecutionStatus expected = DeviceExecutionStatus::eSuspended;

            // Зеркальный lock-free цикл с weak версией для FenceWaiter
            while (!currentStatus.compare_exchange_weak(expected, DeviceExecutionStatus::eDone,
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_acquire))
            {
                if (expected != DeviceExecutionStatus::eSuspended) {
                    // Корутина еще не успела уснуть (статус ePending).
                    // Мы просто выставили eDone. Корутина зайдет в await_ready, увидит eDone и побежит дальше сама.
                    currentStatus.store(DeviceExecutionStatus::eDone, std::memory_order_release);
                    return;
                }
            }

            // Если CAS сработал, значит корутина спала (eSuspended).
            // Извлекаем хэндл и экзекутор и отправляем задачу в пул потоков stdexec
            auto handle = currentCoroutineHandle;
            auto* exec = mExecutor;

            if (handle && exec) {
                exec->schedule(handle);
            }
        }
    private:
        std::coroutine_handle<> currentCoroutineHandle;
        Executor* mExecutor{};
        vk::Result taskResult{};
    };

    struct DeviceExecutionAwaitable {
        explicit DeviceExecutionAwaitable(DeviceExecutionTask task);
    private:
        std::shared_ptr<DeviceExecutionTask> task;
    };

    struct DeviceExecutionAwaiter {
        DeviceExecutionAwaiter(DeviceExecutionAwaitable&& awaitable, Executor* executor);

        [[nodiscard]] bool await_ready() const {
            return task->currentStatus.load(std::memory_order_acquire) == DeviceExecutionStatus::eDone;
        }

        [[nodiscard]] bool await_suspend(std::coroutine_handle<> coroutineHandle) const {
            task->setContinuation(coroutineHandle, executor);
            auto expectedStatus = DeviceExecutionStatus::ePending;
            while (!task->currentStatus.compare_exchange_weak(
                expectedStatus,
                DeviceExecutionStatus::eSuspended,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {

                if (expectedStatus != DeviceExecutionStatus::ePending) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] vk::Result await_resume() const {
            return task->getResult();
        }

    private:
        std::shared_ptr<DeviceExecutionTask> task;
        Executor* executor{};
    };

    struct QueueExecutionRequest {
        std::function<vk::Result(vk::Queue, vk::Fence)> request;
    };

    class QueueExecutionGroup {
    public:
        DeviceExecutionAwaitable sendQueueRequest(const QueueExecutionRequest& request, FenceWaiter& fenceWaiter);
    private:
        moodycamel::ConcurrentQueue<vk::Queue> freeQueuePool;
        moodycamel::ConcurrentQueue<QueueExecutionRequest> requestQueue;
        float priority = 1.0f;
        uint32_t queueFamilyIndex = 0;
    };

    class QueueExecutionChannel {
    public:
        DeviceExecutionAwaitable sendQueueRequest(QueueExecutionRequest request, FenceWaiter& fenceWaiter);
    private:
        std::vector<QueueExecutionGroup> compatibleGroups;
    };

} // shuttle_engine::execution

#endif //HELLOTRIANGLE_QUEUEEXECUTIONCHANNEL_HPP
