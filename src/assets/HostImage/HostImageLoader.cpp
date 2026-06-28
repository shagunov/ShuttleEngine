#include "HostImageLoader.hpp"
#include <iostream>

// Подключаем реализацию stb_image только в одном .cpp файле!
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace shuttle_engine::assets {

    std::optional<HostImage> HostImageLoader::loadFromFile(const std::string& filepath) noexcept {
        int width = 0;
        int height = 0;
        int channels = 0;

        // Загружаем изображение, принудительно запрашивая 4 канала (RGBA)
        unsigned char* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STB_IMAGE_IMPLEMENTATION);
        
        if (!pixels) {
            std::cerr << "[HostImageLoader] Failed to load: " << filepath 
                      << " | Reason: " << stbi_failure_reason() << std::endl;
            return std::nullopt;
        }

        size_t imageSize = static_cast<size_t>(width) * height * 4;

        // Возвращаем HostImage. unique_ptr сам примет этот указатель 
        // и вызовет лямбду с stbi_image_free при уничтожении!
        return HostImage(
            reinterpret_cast<uint8_t*>(pixels),
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            4,
            imageSize,
            vk::Format::eR8G8B8A8Srgb, // По умолчанию для текстур. В AssetManager можно переопределить на Unorm для ORM
            [](uint8_t* p) { 
                if (p) ::stbi_image_free(p); 
            }
        );
    }

} // namespace shuttle_engine::assets
