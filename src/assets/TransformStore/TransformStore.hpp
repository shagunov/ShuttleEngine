#pragma once
#include "IncludeVulkan.hpp"
#include "../../memory/DeviceAllocator/DeviceAllocator.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace shuttle_engine::assets {

    class TransformStore {
    public:
        TransformStore(vk::Device device, memory::DeviceAllocator& allocator, uint32_t maxNodes, uint32_t framesInFlight);
        ~TransformStore();

        TransformStore(const TransformStore&) = delete;
        TransformStore& operator=(const TransformStore&) = delete;

        // основной метод: сначала propagation (depth), затем flatten by depth
        // nodeBufferAddr: буфер нод (host-visible), куда node.depth может быть обновлён compute-шадером
        void recordFlatteningCommands(
            vk::CommandBuffer cmd,
            uint32_t frameIdx,
            vk::DeviceAddress nodeBufferAddr,
            uint32_t nodeCount,
            uint32_t maxDepthHint = 16 // безопасный максимум проходов
        );

        [[nodiscard]] vk::DeviceAddress getWorldMatricesAddress(uint32_t frameIdx) const noexcept;

    private:
        struct FrameData {
            memory::UniqueAllocatedBuffer worldBuffer; // DEVICE_LOCAL mat4[]
            vk::DeviceAddress bda = 0;
        };

        vk::Device device_;
        memory::DeviceAllocator& allocator_;
        uint32_t maxNodes_;
        uint32_t framesInFlight_;

        std::vector<FrameData> frames_;
        // counter buffer (host-visible small) для итераций propagation
        memory::UniqueAllocatedBuffer counterBuffer_;
        uint8_t* counterMappedPtr_ = nullptr;
        vk::DeviceAddress counterBda_ = 0;

        // пайплайн compute (we'll use one shader with mode push constant or two shaders)
        vk::UniquePipeline computePipeline_;
        vk::UniquePipelineLayout pipelineLayout_;

        void createComputePipeline(); // загружает shader(s) и создает layout
    };
}
