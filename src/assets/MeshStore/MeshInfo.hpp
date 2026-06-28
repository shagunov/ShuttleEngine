#pragma once
#include "IncludeVulkan.hpp"
#include <glm/glm.hpp>       // Для glm::vec3 и т.д.

namespace shuttle_engine::render {

    // Атрибуты вершины (без позиции). Выровнены по 16 байт для std430.
    // Это будет лежать в AttributeMegaBuffer.
    struct alignas(16) VertexAttributes {
        glm::vec3 normal;
        float padding1 = 0.0f; // Выравнивание glm::vec3 до 16 байт
        glm::vec2 uv;
        glm::vec2 padding2;    // Выравнивание glm::vec2 до 16 байт
        glm::vec4 tangent;     // glm::vec4 = 16 байт
    };
    // Общий размер VertexAttributes: 16 + 16 + 16 = 48 байт. Идеально для кэш-линий.

    // Метаданные меша для GPU. Позволяют шейдеру находить геометрию по BDA.
    // Это будет лежать в MeshInfoBuffer.
    struct alignas(16) MeshInfo {
        vk::DeviceAddress positionAddress = 0;   // BDA-указатель на начало позиций меша в PositionMegaBuffer
        vk::DeviceAddress attributeAddress = 0;  // BDA-указатель на начало атрибутов меша в AttributeMegaBuffer
        vk::DeviceAddress indexAddress = 0;      // BDA-указатель на начало индексов меша в IndexMegaBuffer

        uint32_t indexCount = 0;        // Количество индексов
        uint32_t vertexOffset = 0;      // Смещение к первой вершине этого меша в MegaBuffer (если нужно)
        uint32_t materialId = 0;        // ID материала из MaterialStore
        uint32_t padding = 0;           // Заполнитель для выравнивания до 16*3 = 48 байт
    };
    // Общий размер MeshInfo: 8*3 + 4*4 = 24 + 16 = 40. Выравнивается до 48 байт.
    // (uint64_t - 8 байт, uint32_t - 4 байта)
    // 3 * 8 (BDA) = 24 байта
    // 4 * 4 (uint32_t) = 16 байт
    // Итого 40 байт. Выравнивание до 48 байт.

} // namespace shuttle_engine::render
