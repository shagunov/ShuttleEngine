#include "TextureManager.hpp"

namespace shuttle_engine::Core {

    // Вынесем хелпер барьеров в анонимный namespace, чтобы не дублировать
    namespace {
        void transitionMip(vk::CommandBuffer cmd, vk::Image img, uint32_t mip,
                           vk::ImageLayout oldL, vk::ImageLayout newL,
                           vk::AccessFlags srcA, vk::AccessFlags dstA,
                           vk::PipelineStageFlags srcS, vk::PipelineStageFlags dstS) {
            vk::ImageMemoryBarrier barrier{
                .srcAccessMask = srcA, .dstAccessMask = dstA,
                .oldLayout = oldL, .newLayout = newL,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = img,
                .subresourceRange = { vk::ImageAspectFlagBits::eColor, mip, 1, 0, 1 }
            };
            cmd.pipelineBarrier(srcS, dstS, {}, nullptr, nullptr, barrier);
        }
    }

    TextureManager::TextureManager(vk::Device device, memory::DeviceAllocator allocator)
        : device(device), allocator(allocator),
          normalMipGen(device), generalMipGen(device) {}

    void TextureManager::ensureWorkingImage(uint32_t width, uint32_t height, vk::Format format) {
        if (!workingImage || currentWorkingWidth < width || currentWorkingHeight < height || currentWorkingFormat != format) {
            uint32_t mips = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

            auto [res, img] = allocator.createAndAllocateImageUnique({
                .imageType = vk::ImageType::e2D,
                .format = format,
                .extent = {width, height, 1},
                .mipLevels = mips,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
                         vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                .initialLayout = vk::ImageLayout::eUndefined
            }, memory::MemoryUsage::eGpuOnly);

            workingImage = std::move(img);
            currentWorkingWidth = width;
            currentWorkingHeight = height;
            currentWorkingFormat = format;
        }
    }
} // namespace shuttle_engine::Core