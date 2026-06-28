
#include "vk_mem_alloc.h"
#include "VirtualBlock.hpp"

namespace shuttle_engine::memory {

    // --- VirtualBlock methods ---

    vk::ResultValue<VirtualAllocation> VirtualBlock::allocate(vk::DeviceSize size, vk::DeviceSize alignment) {
        std::lock_guard lock(guardMutex);
        VmaVirtualAllocationCreateInfo info = { .size = size, .alignment = alignment };
        VmaVirtualAllocation vmaAlloc = nullptr;
        VkDeviceSize offset = 0;

        VkResult res = vmaVirtualAllocate(static_cast<VmaVirtualBlock>(handle), &info, &vmaAlloc, &offset);
        if (res != VK_SUCCESS) return { static_cast<vk::Result>(res), {} };

        return { static_cast<vk::Result>(res), VirtualAllocation{size, offset, vmaAlloc} };
    }

    void VirtualBlock::deallocate(VirtualAllocation alloc) {
        std::lock_guard lock(guardMutex);
        vmaVirtualFree(static_cast<VmaVirtualBlock>(handle), static_cast<VmaVirtualAllocation>(alloc.handle));
    }

    // --- UniqueVirtualBlock methods ---

    vk::ResultValue<UniqueVirtualBlock> UniqueVirtualBlock::create(vk::DeviceSize size) {
        VmaVirtualBlockCreateInfo info = { .size = size };
        VmaVirtualBlock vmaBlock = nullptr;
        if (auto result = vmaCreateVirtualBlock(&info, &vmaBlock); result != VK_SUCCESS) return {static_cast<vk::Result>(result), {}};
        return {vk::Result::eSuccess, UniqueVirtualBlock(VirtualBlock(vmaBlock))};
    }

    UniqueVirtualBlock::~UniqueVirtualBlock() {
        if (block.handle) {
            vmaDestroyVirtualBlock(static_cast<VmaVirtualBlock>(block.handle));
        }
    }

    UniqueVirtualBlock::UniqueVirtualBlock(UniqueVirtualBlock&& other) noexcept : block(other.block) {
        other.block = VirtualBlock(nullptr);
    }

    UniqueVirtualBlock& UniqueVirtualBlock::operator=(UniqueVirtualBlock&& other) noexcept {
        if (this != &other) {
            if (block.handle) vmaDestroyVirtualBlock(static_cast<VmaVirtualBlock>(block.handle));
            block = other.block;
            other.block = VirtualBlock(nullptr);
        }
        return *this;
    }

}
