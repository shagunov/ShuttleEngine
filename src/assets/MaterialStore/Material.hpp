#pragma once
#include "IncludeVulkan.hpp"
#include <glm/glm.hpp>

namespace shuttle_engine::render {

    // Используем enum class для типизации, но с возможностью побитовых операций
    enum MaterialFlags : uint32_t {
        None            = 0,
        DoubleSided     = 1 << 0, // Не делать Culling (Backface Culling OFF)
        AlphaMasked     = 1 << 1, // Использовать AlphaCutoff в шейдере
        AlphaBlended    = 1 << 2, // Включить Blending (Alpha Test)
        DepthWriteOff   = 1 << 3, // Не писать в Depth Buffer (для полупрозрачных)
        Unlit           = 1 << 4, // Игнорировать освещение (Emissive only)
        Wireframe       = 1 << 5, // Режим сетки (если нужно для дебага)
    };

    // Операторы для удобной работы с флагами (битовая маска)
    constexpr MaterialFlags operator|(MaterialFlags a, MaterialFlags b) {
        return static_cast<MaterialFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    constexpr bool operator&(MaterialFlags a, MaterialFlags b) {
        return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
    }

    // alignas(16) гарантирует, что структура всегда начинается с границы 16 байт,
    // что критично для массивов в SSBO.
    struct alignas(16) Material {

        // 1. Цветовые факторы
        // alignas(16) уже есть у glm::vec4, здесь 16 байт.
        glm::vec4 baseColorFactor;

        // 2. PBR параметры (Metallic, Roughness, AO, AlphaCutoff)
        // x: metallic, y: roughness, z: occlusion, w: alphaCutoff
        // 16 байт
        glm::vec4 pbrFactors;

        // 3. Индексы текстур (Bindless indices)
        // 16 байт
        uint32_t albedoIdx;
        uint32_t normalIdx;
        uint32_t ormIdx;       // Упакованная (AO, Roughness, Metallic)
        uint32_t emissiveIdx;

        // 4. Дополнительные данные и сэмплер
        // 16 байт
        uint32_t samplerIdx;     // Индекс в сэмплер-куче (Set 0)
        float    emissiveStrength;
        uint32_t flags;          // Битовые флаги (DoubleSided, AlphaMode и т.д.)
        uint32_t padding;        // Заполнитель для выравнивания до 16 байт
    };
}
