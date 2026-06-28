#pragma once
#include "IncludeVulkan.hpp"

// Здесь может быть и твой VmaAllocation/UniqueAllocatedImage, если ты его используешь напрямую
// #include "../DeviceAllocator/UniqueAllocatedImage.hpp"

namespace shuttle_engine::render {

    enum class TextureType : uint32_t {
        // Стандартные типы текстур
        Albedo,              // Основной цвет. Должен быть в формате sRGB.
        Normal,              // Карта нормалей. Должна быть в формате UNORM.
        ORM,                 // Occlusion, Roughness, Metallic. Должна быть в формате UNORM.
        Emissive,            // Излучающий цвет. Должна быть в формате sRGB или UNORM (зависит от применения).
        Metallic,            // Отдельная карта металличности. Должна быть в формате UNORM.
        Roughness,           // Отдельная карта шероховатости. Должна быть в формате UNORM.
        AmbientOcclusion,    // Отдельная карта AO. Должна быть в формате UNORM.
        Height,              // Карта высот/дисплейсмента. Должна быть в формате UNORM (float).
        Specular,            // Карта спекулярности (для Blinn-Phong).

        // Специальные типы (если ты их будешь использовать)
        CubeMap,             // Кубмапа окружения (для IBL)
        ShadowMap,           // Карта теней (Depth Image)
        UI,                  // Текстура для пользовательского интерфейса

        Generic              // Любая другая текстура, для которой не нужны особые правила.
    };

    struct Texture {
        // Raw Vulkan handles.
        // Если ты используешь UniqueAllocatedImage, то эти поля можно убрать
        // и хранить UniqueAllocatedImage image_;
        vk::Image image = nullptr;
        vk::ImageView imageView = nullptr;

        vk::Format format = vk::Format::eUndefined;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 1;

        // --- Строго Move-Only ---
        // Конструктор по умолчанию
        Texture() = default;
        // Деструктор
        ~Texture() = default;

        // Запрет копирования
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        // Move-конструктор
        Texture(Texture&& other) noexcept
            : image(other.image),
              imageView(other.imageView),
              format(other.format),
              width(other.width),
              height(other.height),
              mipLevels(other.mipLevels)
        {
            // После перемещения, "другой" объект не должен владеть ресурсами
            other.image = nullptr;
            other.imageView = nullptr;
            other.width = 0;
            other.height = 0;
            other.mipLevels = 0;
            other.format = vk::Format::eUndefined;
        }

        // Move-оператор присваивания
        Texture& operator=(Texture&& other) noexcept {
            if (this != &other) {
                // Сначала очищаем текущие ресурсы (если есть)
                // !!! ВНИМАНИЕ: Если ты используешь VMA/UniqueAllocatedImage,
                //     то деструктор UniqueAllocatedImage сделает это сам.
                //     Сейчас предполагается, что эти raw VkHandles будут
                //     уничтожены в TextureStore, когда он будет удален.

                image = other.image;
                imageView = other.imageView;
                format = other.format;
                width = other.width;
                height = other.height;
                mipLevels = other.mipLevels;

                // Перемещаем владение
                other.image = nullptr;
                other.imageView = nullptr;
                other.width = 0;
                other.height = 0;
                other.mipLevels = 0;
                other.format = vk::Format::eUndefined;
            }
            return *this;
        }

        [[nodiscard]] explicit operator bool() const noexcept { return image != nullptr; }
    };

} // namespace shuttle_engine::render
