#pragma once
#include "IncludeVulkan.hpp"

namespace shuttle_engine::Core {

    class GeneralMipGenerator {
    public:
        GeneralMipGenerator(vk::Device device) : device(device) {}
        ~GeneralMipGenerator() = default;

        GeneralMipGenerator(const GeneralMipGenerator&) = delete;
        GeneralMipGenerator& operator=(const GeneralMipGenerator&) = delete;
        GeneralMipGenerator(GeneralMipGenerator&&) noexcept = default;
        GeneralMipGenerator& operator=(GeneralMipGenerator&&) noexcept = default;

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
        vk::Device device;
    };

} // namespace shuttle_engine::Core
