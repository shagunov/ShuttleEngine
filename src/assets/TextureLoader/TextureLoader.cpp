#include "TextureLoader.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

// Наш хелпер для переходов лайаутов мипов
namespace shuttle_engine::Core {
    void transitionMip(vk::CommandBuffer cmd, vk::Image img, uint32_t mip, 
                       vk::ImageLayout oldL, vk::ImageLayout newL, 
                       vk::AccessFlags srcA, vk::AccessFlags dstA, 
                       vk::PipelineStageFlags srcS, vk::PipelineStageFlags dstS) 
    {
        vk::ImageMemoryBarrier barrier{
            .srcAccessMask = srcA,
            .dstAccessMask = dstA,
            .oldLayout = oldL,
            .newLayout = newL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = img,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = mip,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        cmd.pipelineBarrier(srcS, dstS, {}, nullptr, nullptr, barrier);
    }
}

namespace shuttle_engine::assets {

    TextureUploadTx TextureLoader::prepareUpload(
        memory::DeviceAllocator& allocator,
        memory::StagingBufferController& staging,
        const HostImage& hostImage) const 
    {
        if (hostImage.isEmpty()) {
            throw std::runtime_error("[TextureLoader] Attempted to prepare upload for empty HostImage!");
        }

        TextureUploadTx tx;
        tx.width = hostImage.width;
        tx.height = hostImage.height;
        tx.format = hostImage.format;
        tx.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(tx.width, tx.height)))) + 1;

        // 1. Создаем ФИНАЛЬНОЕ (Immutable, GPU-only) изображение
        auto [resDst, uniqueDstImage] = allocator.createAndAllocateImageUnique(
            {
                .imageType = vk::ImageType::e2D,
                .format = tx.format,
                .extent = { tx.width, tx.height, 1 },
                .mipLevels = tx.mipLevels,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                .sharingMode = vk::SharingMode::eExclusive,
                .initialLayout = vk::ImageLayout::eUndefined
            },
            memory::MemoryUsage::eGpuOnly
        );
        if (resDst != vk::Result::eSuccess) throw std::runtime_error("[TextureLoader] Failed to allocate Final Image!");
        tx.finalImage = std::move(uniqueDstImage);

        // 2. Создаем ВРЕМЕННОЕ (src, с поддержкой STORAGE для Compute-генератора нормалей) изображение
        auto [resSrc, uniqueSrcImage] = allocator.createAndAllocateImageUnique(
            {
                .imageType = vk::ImageType::e2D,
                .format = tx.format,
                .extent = { tx.width, tx.height, 1 },
                .mipLevels = tx.mipLevels,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                .sharingMode = vk::SharingMode::eExclusive,
                .initialLayout = vk::ImageLayout::eUndefined
            },
            memory::MemoryUsage::eGpuOnly
        );
        if (resSrc != vk::Result::eSuccess) throw std::runtime_error("[TextureLoader] Failed to allocate Temporary Image!");
        tx.temporaryImage = std::move(uniqueSrcImage);

        // 3. Выделяем память в мега-стейджинг буфере
        auto stagingRes = staging.allocate(hostImage.size, 16);
        if (stagingRes.result != vk::Result::eSuccess) {
            throw std::runtime_error("[TextureLoader] Failed to allocate Staging memory!");
        }
        tx.stagingAlloc = stagingRes.value;

        // 4. Копируем пиксели из HostImage напрямую в Mapped Memory стейджинга (Zero-Copy!)
        std::memcpy(tx.stagingAlloc.mappedPointer, hostImage.getRawData(), hostImage.size);

        return tx; // Возвращаем готовую транзакцию
    }

    void TextureLoader::recordUploadCommands(
        vk::CommandBuffer cmd,
        const TextureUploadTx& tx,
        render::TextureType type)
    {
        // 1. Переводим временный srcImage Mip 0 в TransferDstOptimal
        Core::transitionMip(cmd, *tx.temporaryImage, 0, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                            {}, vk::AccessFlagBits::eTransferWrite,
                            vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);

        // 2. Копируем данные из staging в Mip 0 временного изображения на GPU
        vk::BufferImageCopy region{
            .bufferOffset = tx.stagingAlloc.offset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { tx.width, tx.height, 1 }
        };
        cmd.copyBufferToImage(tx.stagingAlloc.buffer, *tx.temporaryImage, vk::ImageLayout::eTransferDstOptimal, region);

        // 3. Запускаем соответствующий генератор мип-мапов
        if (type == render::TextureType::Normal) {
            normalMipGen_.generateAndCopy(cmd, *tx.temporaryImage, *tx.finalImage, tx.format, tx.width, tx.height, tx.mipLevels);
        } else {
            generalMipGen_.generateAndCopy(cmd, *tx.temporaryImage, *tx.finalImage, tx.format, tx.width, tx.height, tx.mipLevels);
        }
    }

} // namespace shuttle_engine::assets