#include "StagingBufferController.hpp"

namespace shuttle_engine::memory {

    vk::ResultValue<StagingBufferController> StagingBufferController::create(DeviceAllocator allocator, vk::Device device, size_t size) {
        auto [resBuf, uniqueStagingBuffer] = allocator.createAndAllocateBufferUnique(
            {
                .size = size,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive
            },
            MemoryUsage::eCpuToGpu,
            AllocationCreateFlags(
                static_cast<uint32_t>(AllocationCreateFlagBits::eMapped) |
                static_cast<uint32_t>(AllocationCreateFlagBits::eHostAccessSequentialWrite)
            )
        );
        if (resBuf != vk::Result::eSuccess) return {resBuf, {}};

        auto [resVB, uniqueVBlock] = UniqueVirtualBlock::create(size);
        if (resVB != vk::Result::eSuccess) return {resVB, {}};

        auto* basePtr = static_cast<uint8_t*>(allocator.getMappedPointer(*uniqueStagingBuffer));
        if (!basePtr) return {vk::Result::eErrorOutOfHostMemory, {}};

        return {vk::Result::eSuccess, StagingBufferController(
            std::move(uniqueStagingBuffer), basePtr, size, std::move(uniqueVBlock), device
        )};
    }

    vk::ResultValue<StagingAllocation> StagingBufferController::allocate(size_t size, size_t alignment) noexcept {
        // Очищаем мусор перед выделением
        collectGarbage();

        auto res = virtualBlock->allocate(size, alignment);
        if (res.result != vk::Result::eSuccess) return {res.result, {}};

        return {
            vk::Result::eSuccess,
            StagingAllocation{
                .buffer = *stagingBuffer,
                .offset = res.value.offset,
                .size = res.value.size,
                .mappedPointer = baseMappedPointer + res.value.offset,
                .vmaAlloc = res.value
            }
        };
    }

    void StagingBufferController::freeDeferred(StagingAllocation alloc, vk::Fence fence) noexcept {
        if (!alloc) return;
        std::lock_guard lg(mutex);
        pendingFrees.push_back({alloc, fence});
    }

    void StagingBufferController::collectGarbage() noexcept {
        std::lock_guard lg(mutex);

        for (size_t i = 0; i < pendingFrees.size(); ) {
            if (device.getFenceStatus(pendingFrees[i].fence) == vk::Result::eSuccess) {
                // Освобождаем в VMA
                virtualBlock->deallocate(pendingFrees[i].alloc.vmaAlloc);

                // Swap-with-back для быстрого удаления
                if (i != pendingFrees.size() - 1) {
                    pendingFrees[i] = pendingFrees.back();
                }
                pendingFrees.pop_back();
            } else {
                ++i;
            }
        }
    }
}
