#pragma once
#include "HostImage.hpp"
#include <string>
#include <optional>

namespace shuttle_engine::assets {

    class HostImageLoader {
    public:
        // Загружает изображение с диска и возвращает HostImage.
        // Автоматически приводит всё к RGBA8 (4 канала).
        [[nodiscard]] static std::optional<HostImage> loadFromFile(const std::string& filepath) noexcept;
    };

} // namespace shuttle_engine::assets
