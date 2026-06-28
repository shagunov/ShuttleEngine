#pragma once
#include "IncludeVulkan.hpp" // Для vk::Device
#include "Texture.hpp"       // Твоя структура render::Texture
#include <vector>
#include <optional>
#include <stdexcept>
#include <array>

#include "memory/DeviceAllocator/DeviceAllocator.hpp"
#include "memory/StagingBufferController/StagingBufferController.hpp"

namespace shuttle_engine::assets {

    // Класс для управления глобальной кучей текстур (Bindless Texture Array)
    class TextureStore {
    public:
        // Конструктор: maxTextures - максимальное количество текстур в куче, setIndex - номер сета
        TextureStore(vk::Device device, uint32_t maxTextures, uint32_t setIndex);
        ~TextureStore(); // Деструктор для очистки Vulkan-объектов

        TextureStore(const TextureStore&) = delete;
        TextureStore& operator=(const TextureStore&) = delete;
        TextureStore(TextureStore&&) noexcept = default;
        TextureStore& operator=(TextureStore&&) noexcept = default;

        // Регистрирует новую текстуру в куче и возвращает её TextureID (индекс в массиве)
        // Принимает готовый render::Texture (image, imageView)
        // Возвращает ID дефолтной текстуры, если произошла ошибка
        uint32_t registerTexture(render::Texture&& texture, render::TextureType type = render::TextureType::Generic);

        // Получает DescriptorSetLayout для Set 1 (для PipelineLayout)
        [[nodiscard]] vk::DescriptorSetLayout getDescriptorSetLayout() const noexcept { return descriptorSetLayout_; }
        // Получает сам DescriptorSet для Set 1 (для vkCmdBindDescriptorSets)
        [[nodiscard]] vk::DescriptorSet getDescriptorSet() const noexcept { return descriptorSet_; }

        // Возвращает ID дефолтной текстуры для Albedo (белая)
        [[nodiscard]] uint32_t getDefaultAlbedoId() const noexcept { return defaultAlbedoId_; }
        // Возвращает ID дефолтной текстуры для Normal (0.5, 0.5, 1.0)
        [[nodiscard]] uint32_t getDefaultNormalId() const noexcept { return defaultNormalId_; }
        // Возвращает ID дефолтной текстуры для ORM (1.0, 1.0, 0.0)
        [[nodiscard]] uint32_t getDefaultOrmId() const noexcept { return defaultOrmId_; }

    private:
        vk::Device device_;
        uint32_t maxTextures_;
        uint32_t setIndex_; // Номер дескрипторного сета (например, 1)

        std::vector<std::optional<render::Texture>> textures_; // Хранилище объектов Texture на CPU
        uint32_t nextFreeId_ = 0; // Следующий свободный ID

        // Vulkan объекты для Bindless Heap
        vk::DescriptorSetLayout descriptorSetLayout_ = nullptr;
        vk::DescriptorPool descriptorPool_ = nullptr;
        vk::DescriptorSet descriptorSet_ = nullptr;

        // ID дефолтных текстур
        uint32_t defaultAlbedoId_ = 0;
        uint32_t defaultNormalId_ = 0;
        uint32_t defaultOrmId_ = 0; // Для ORM (1.0, 1.0, 0.0)

        // Вспомогательный метод для создания Vulkan-объектов (Layout, Pool, Set)
        void createVulkanObjects();
        // Вспомогательный метод для создания и регистрации дефолтных текстур
        void createDefaultTextures(class TextureLoader& textureLoader,
                                   memory::DeviceAllocator& allocator,
                                   memory::StagingBufferController& stagingController);

        // Внутренний метод для обновления дескриптора в Bindless Heap
        void updateDescriptorSet(uint32_t textureId, vk::ImageView view, vk::DescriptorType type = vk::DescriptorType::eSampledImage);
    };

} // namespace shuttle_engine::assets
