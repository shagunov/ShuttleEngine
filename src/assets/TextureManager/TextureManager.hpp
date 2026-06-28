#pragma once
#include "IncludeVulkan.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

#include "../NormalMipGenerator/NormalMipGenerator.hpp"
#include "../GeneralMipGenerator/GeneralMipGenerator.hpp"
#include "memory/DeviceAllocator/DeviceAllocator.hpp"
#include "memory/StagingBufferController/StagingBufferController.hpp"

namespace shuttle_engine::Core {

    struct TextureResource {
        memory::UniqueAllocatedImage image;
        vk::UniqueImageView imageView;
    };

    class TextureManager {
    public:
        // Теперь конструктор легкий
        TextureManager(vk::Device device, memory::DeviceAllocator allocator);
        ~TextureManager() = default;

        TextureManager(const TextureManager&) = delete;
        TextureManager& operator=(const TextureManager&) = delete;
        TextureManager(TextureManager&&) noexcept = default;
        TextureManager& operator=(TextureManager&&) noexcept = default;

        // Главный метод: теперь принимает CommandBuffer извне
        TextureResource loadTexture(
            vk::CommandBuffer cmd,            // Буфер, куда пишем команды
            memory::StagingBufferController& staging,
            const uint8_t* data,
            size_t dataSize,
            uint32_t width, uint32_t height,
            vk::Format format,
            bool isNormalMap,
            vk::Fence uploadFence
        );

    private:
        vk::Device device;
        memory::DeviceAllocator allocator;

        NormalMipGenerator normalMipGen;
        GeneralMipGenerator generalMipGen;

        // Общая "рабочая область" для генерации мипов
        memory::UniqueAllocatedImage workingImage;
        uint32_t currentWorkingWidth = 0;
        uint32_t currentWorkingHeight = 0;
        vk::Format currentWorkingFormat = vk::Format::eUndefined;

        void ensureWorkingImage(uint32_t width, uint32_t height, vk::Format format);
    };

} // namespace shuttle_engine::Core
