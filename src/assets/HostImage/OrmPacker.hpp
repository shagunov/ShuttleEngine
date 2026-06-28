#pragma once
#include "HostImage.hpp"
#include <optional>

namespace shuttle_engine::assets {

    class OrmPacker {
    public:
        // Объединяет три черно-белых изображения в одно цветное ORM (R8G8B8A8_UNORM).
        // Если какая-то из текстур отсутствует, передается nullptr.
        [[nodiscard]] static HostImage pack(
            const HostImage* ao,
            const HostImage* rough,
            const HostImage* metal) noexcept;
    };

} // namespace shuttle_engine::assets
