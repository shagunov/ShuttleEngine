#include "OrmPacker.hpp"
#include <algorithm>
#include <iostream>

namespace shuttle_engine::assets {

    // Безопасное чтение пикселя с масштабированием (Nearest-Neighbor)
    static uint8_t sampleChannel(const HostImage* img, uint32_t x, uint32_t y, uint32_t targetW, uint32_t targetH, uint8_t defaultValue) noexcept {
        if (!img || img->isEmpty() || !img->getRawData()) {
            return defaultValue;
        }

        // Fast path: если размеры совпадают, читаем напрямую
        if (img->width == targetW && img->height == targetH) {
            size_t index = (static_cast<size_t>(y) * img->width + x) * img->channels;
            return img->getRawData()[index]; // Берем только красный (R) канал
        }

        // Slow path: масштабируем координаты
        uint32_t srcX = std::clamp((x * img->width) / targetW, 0u, img->width - 1);
        uint32_t srcY = std::clamp((y * img->height) / targetH, 0u, img->height - 1);
        
        size_t index = (static_cast<size_t>(srcY) * img->width + srcX) * img->channels;
        return img->getRawData()[index];
    }

    HostImage OrmPacker::pack(
        const HostImage* ao,
        const HostImage* rough,
        const HostImage* metal) noexcept 
    {
        // 1. Вычисляем целевое разрешение (берем МАКСИМАЛЬНОЕ из доступных, чтобы сохранить детализацию)
        uint32_t targetW = 0;
        uint32_t targetH = 0;

        if (ao && !ao->isEmpty()) {
            targetW = std::max(targetW, ao->width);
            targetH = std::max(targetH, ao->height);
        }
        if (rough && !rough->isEmpty()) {
            targetW = std::max(targetW, rough->width);
            targetH = std::max(targetH, rough->height);
        }
        if (metal && !metal->isEmpty()) {
            targetW = std::max(targetW, metal->width);
            targetH = std::max(targetH, metal->height);
        }

        // Если ни одной текстуры нет вообще, возвращаем дефолтную ORM заглушку 1x1
        // AO = 1.0 (255), Roughness = 1.0 (255), Metallic = 0.0 (0), Alpha = 1.0 (255)
        if (targetW == 0 || targetH == 0) {
            uint8_t* fallbackData = new uint8_t[4]{ 255, 255, 0, 255 };
            return HostImage(
                fallbackData,
                1, 1, 4, 4,
                vk::Format::eR8G8B8A8Unorm, // Не sRGB!
                [](uint8_t* p) { delete[] p; }
            );
        }

        // 2. Аллоцируем память под итоговый буфер на CPU
        size_t totalSize = static_cast<size_t>(targetW) * targetH * 4;
        uint8_t* ormData = new uint8_t[totalSize];

        // 3. Выполняем сборку попиксельно
        // Карта ORM расшифровывается как:
        // Red (R)   -> Ambient Occlusion (Дефолт = 255)
        // Green (G) -> Roughness         (Дефолт = 255)
        // Blue (B)  -> Metallic          (Дефолт = 0)
        // Alpha (A) -> Padding           (Всегда = 255)
        for (uint32_t y = 0; y < targetH; ++y) {
            for (uint32_t x = 0; x < targetW; ++x) {
                size_t pixelOffset = (static_cast<size_t>(y) * targetW + x) * 4;

                ormData[pixelOffset + 0] = sampleChannel(ao,    x, y, targetW, targetH, 255); // O
                ormData[pixelOffset + 1] = sampleChannel(rough, x, y, targetW, targetH, 255); // R
                ormData[pixelOffset + 2] = sampleChannel(metal, x, y, targetW, targetH, 0);   // M
                ormData[pixelOffset + 3] = 255;                                               // A
            }
        }

        // 4. Возвращаем HostImage с UNORM форматом и кастомным делейтером delete[]
        return {
            ormData,
            targetW,
            targetH,
            4,
            totalSize,
            vk::Format::eR8G8B8A8Unorm, // ORM всегда UNORM, сэмплировать как sRGB её нельзя!
            [](uint8_t* p) { delete[] p; }
        };
    }

} // namespace shuttle_engine::assets