#pragma once
#include "IncludeVulkan.hpp"
#include <mutex>

#include "../../../cmake-build-debug/_deps/relacy-src/relacy/stdlib/mutex.hpp"

namespace shuttle_engine::memory {

    using VmaVirtualBlockHandle = void*;
    using VmaVirtualAllocationHandle = void*;

    struct VirtualAllocation {
        vk::DeviceSize size;
        vk::DeviceSize offset;
        VmaVirtualAllocationHandle handle;
    };

    // Легковесный объект-ссылка
    class VirtualBlock {
        friend class UniqueVirtualBlock;
    public:
        VirtualBlock() = default;

        [[nodiscard]] vk::ResultValue<VirtualAllocation> allocate(vk::DeviceSize size, vk::DeviceSize alignment);
        void deallocate(VirtualAllocation allocation);

        [[nodiscard]] operator bool() const { return handle != nullptr; }

        VirtualBlock& operator=(VirtualBlock const& other)
        {
            this->handle = other.handle;
            return *this;
        }
        VirtualBlock(VirtualBlock const& other) : handle{other.handle}, guardMutex{std::mutex{}} {}

    private:
        VmaVirtualBlockHandle handle = nullptr;
        std::mutex guardMutex;

        // Приватный конструктор, доступный только UniqueVirtualBlock
        explicit VirtualBlock(VmaVirtualBlockHandle h) : handle(h) {}
    };

    // Владелец (RAII)
    class UniqueVirtualBlock {
    public:
        static vk::ResultValue<UniqueVirtualBlock> create(vk::DeviceSize size);

        ~UniqueVirtualBlock();
        UniqueVirtualBlock(UniqueVirtualBlock&& other) noexcept;
        UniqueVirtualBlock& operator=(UniqueVirtualBlock&& other) noexcept;

        UniqueVirtualBlock(const UniqueVirtualBlock&) = delete;
        UniqueVirtualBlock& operator=(const UniqueVirtualBlock&) = delete;

        VirtualBlock& get() { return block; }
        VirtualBlock* operator->() { return &block; }
        UniqueVirtualBlock() = default;

    private:
        explicit UniqueVirtualBlock(const VirtualBlock& b) : block(b) {}
        VirtualBlock block;
    };

}
