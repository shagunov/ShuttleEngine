#pragma once
#include "IncludeVulkan.hpp"
#include "../DeviceAllocator/DeviceAllocator.hpp"
#include "../VirtualBlock/VirtualBlock.hpp"
#include <mutex>
#include <vector>

namespace shuttle_engine::memory {

    struct StagingAllocation {
        vk::Buffer buffer;
        vk::DeviceSize offset = 0;
        vk::DeviceSize size = 0;
        void* mappedPointer = nullptr;
        VirtualAllocation vmaAlloc{};
        explicit operator bool() const noexcept { return vmaAlloc.handle != nullptr; }
    };

    struct PendingFree {
        StagingAllocation alloc;
        vk::Fence fence;
    };

    class StagingBufferController {
    public:
        [[nodiscard]] static vk::ResultValue<StagingBufferController> create(DeviceAllocator allocator, vk::Device device, size_t size);

        StagingBufferController() = default;
        ~StagingBufferController() = default;

        StagingBufferController(StagingBufferController&& other) noexcept :
            stagingBuffer(std::move(other.stagingBuffer)),
            baseMappedPointer(other.baseMappedPointer),
            bufferSize(other.bufferSize),
            virtualBlock(std::move(other.virtualBlock)), device(other.device),
            mutex(std::mutex{}) {
            other.bufferSize = 0;
            other.baseMappedPointer = nullptr;
        }

        StagingBufferController& operator=(StagingBufferController&& other) noexcept {
            stagingBuffer = std::move(other.stagingBuffer);
            baseMappedPointer = other.baseMappedPointer;
            other.baseMappedPointer = nullptr;
            bufferSize = other.bufferSize;
            other.bufferSize = 0;
            virtualBlock = std::move(other.virtualBlock);
            return *this;
        }

        StagingBufferController(StagingBufferController const& other) noexcept = delete;
        StagingBufferController& operator=(StagingBufferController const& other) noexcept = delete;

        // Потокобезопасная аллокация
        [[nodiscard]] vk::ResultValue<StagingAllocation> allocate(size_t size, size_t alignment) noexcept;

        // Отложенное освобождение: добавляет в очередь ожидания фенса
        void freeDeferred(StagingAllocation alloc, vk::Fence fence) noexcept;

        // Опрашивает фенсы и освобождает готовую память (вызывать раз в кадр)
        void collectGarbage() noexcept;

        [[nodiscard]] vk::Buffer getBuffer() const noexcept { return *stagingBuffer; }

    private:
        StagingBufferController(UniqueAllocatedBuffer&& buffer, uint8_t* basePtr, size_t bufSize, UniqueVirtualBlock&& vblock, vk::Device dev)
            : stagingBuffer(std::move(buffer)), baseMappedPointer(basePtr), bufferSize(bufSize), virtualBlock(std::move(vblock)), device(dev) {}

        UniqueAllocatedBuffer stagingBuffer;
        uint8_t* baseMappedPointer = nullptr;
        size_t bufferSize = 0;

        UniqueVirtualBlock virtualBlock;
        vk::Device device = nullptr;

        std::mutex mutex;
        std::vector<PendingFree> pendingFrees;
    };
}
