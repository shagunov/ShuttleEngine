//
// Created by Shagu on 23.06.2026.
//

#ifndef HELLOTRIANGLE_EXECUTOR_HPP
#define HELLOTRIANGLE_EXECUTOR_HPP

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "exec/start_detached.hpp"

// 1. Реализуем ваш класс Executor на базе пула потоков stdexec
class Executor {
public:
    // Инициализируем пул, например, на 4 или 8 рабочих потоков (воркеров)
    explicit Executor(size_t threadCount) : m_ThreadPool(threadCount), m_Scheduler{m_ThreadPool.get_scheduler()} {
        m_Scheduler = m_ThreadPool.get_scheduler();
    }

    // Метод, который вызывает ваш FenceWaiter, когда GPU закончил работу.
    // Он заставляет корутину "проснуться" внутри пула потоков stdexec!
    void schedule(std::coroutine_handle<> handle) noexcept {
        // В модели stdexec мы создаем ленивую цепочку (sender):
        // Ожидаем на шедулере -> выполняем возобновление корутины
        auto resumeSender = stdexec::schedule(m_Scheduler)
            | stdexec::then([handle] {
                if (handle && !handle.done()) {
                    handle.resume(); // Корутина продолжает выполнение на воркере stdexec!
                }
            });

        // Запускаем цепочку в "огненном" режиме (выстрелил и забыл) в фоне пула
        exec::start_detached(resumeSender);
    }

private:
    exec::static_thread_pool m_ThreadPool;
    decltype(std::declval<exec::static_thread_pool>().get_scheduler()) m_Scheduler;
};

#endif //HELLOTRIANGLE_EXECUTOR_HPP
