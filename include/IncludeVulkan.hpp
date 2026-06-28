#pragma once
// Гарантируем, что ВСЕ файлы проекта компилируются с динамическим диспетчером
#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

#ifndef VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_EXCEPTIONS
#endif

#ifndef VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_CONSTRUCTORS
#endif
#include <vulkan/vulkan.hpp>

static vk::UniqueImageView createMipView(vk::Device device, vk::Image image, vk::Format format, uint32_t mipLevel) {
    return device.createImageViewUnique({
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = { vk::ImageAspectFlagBits::eColor, mipLevel, 1, 0, 1 }
    }).value;
}