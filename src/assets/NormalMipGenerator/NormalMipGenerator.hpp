#pragma once
#include "IncludeVulkan.hpp"
#include <vector>

namespace shuttle_engine::Core {

    class NormalMipGenerator {
    public:
        NormalMipGenerator(vk::Device device);
        ~NormalMipGenerator() = default;

        NormalMipGenerator(const NormalMipGenerator&) = delete;
        NormalMipGenerator& operator=(const NormalMipGenerator&) = delete;
        NormalMipGenerator(NormalMipGenerator&&) noexcept = default;
        NormalMipGenerator& operator=(NormalMipGenerator&&) noexcept = default;

        // Генерирует мипы в srcImage, затем копирует их поочередно в dstImage
        void generateAndCopy(
            vk::CommandBuffer cmd,
            vk::Image srcImage,  // Рабочий Image (Temporary)
            vk::Image dstImage,  // Финальный Image (Immutable)
            vk::Format format,
            uint32_t width, uint32_t height,
            uint32_t mipLevels
        );

    private:
        vk::Result initComputePipeline();

        vk::Device device;
        vk::UniqueDescriptorSetLayout descriptorSetLayout;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniquePipeline computePipeline;
        vk::UniqueDescriptorPool descriptorPool; // Пул для временных DescriptorSet
    };

} // namespace shuttle_engine::Core
