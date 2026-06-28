#include "GeneralMipGenerator.hpp"
#include <algorithm> // For std::max

namespace shuttle_engine::Core {

    // Объявляем transitionMip, чтобы можно было использовать здесь
    void transitionMip(vk::CommandBuffer cmd, vk::Image img, uint32_t mip, 
                       vk::ImageLayout oldL, vk::ImageLayout newL, 
                       vk::AccessFlags srcA, vk::AccessFlags dstA, 
                       vk::PipelineStageFlags srcS, vk::PipelineStageFlags dstS); // Forward declaration

    void GeneralMipGenerator::generateAndCopy(
        vk::CommandBuffer cmd,
        vk::Image srcImage,  // Рабочий Image
        vk::Image dstImage,  // Финальный Image
        vk::Format format,
        uint32_t width, uint32_t height,
        uint32_t mipLevels
    ) {
        // --- Mip 0: Копирование из srcImage (TransferDstOptimal) в dstImage (Undefined)
        // srcImage Mip 0 уже должен быть в eTransferDstOptimal после copyBufferToImage
        // Final Image Mip 0 - переводим из Undefined в TransferDstOptimal
        transitionMip(cmd, dstImage, 0, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                      {}, vk::AccessFlagBits::eTransferWrite,
                      vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);

        vk::ImageCopy copyRegion0{
            .srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
            .dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
            .extent = { width, height, 1 }
        };
        cmd.copyImage(srcImage, vk::ImageLayout::eTransferDstOptimal, dstImage, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion0);

        // Финализируем Mip 0 в dstImage
        transitionMip(cmd, dstImage, 0, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                      vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
                      vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader);

        auto mipWidth = static_cast<int32_t>(width);
        auto mipHeight = static_cast<int32_t>(height);

        for (uint32_t i = 1; i < mipLevels; ++i) {
            mipWidth = std::max(1, mipWidth / 2);
            mipHeight = std::max(1, mipHeight / 2);

            // 1. Переводим srcImage Mip (i-1) в TransferSrcOptimal
            transitionMip(cmd, srcImage, i - 1, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferSrcOptimal,
                          vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead, // Из предыдущего шага Compute/Blit
                          vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);

            // 2. Переводим srcImage Mip i в TransferDstOptimal (для записи blit)
            transitionMip(cmd, srcImage, i, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                          {}, vk::AccessFlagBits::eTransferWrite,
                          vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);

            // 3. Blit (srcImage Mip i-1 -> srcImage Mip i)
            vk::ImageBlit blit{
                .srcSubresource = { vk::ImageAspectFlagBits::eColor, i - 1, 0, 1 },
                .srcOffsets = { {vk::Offset3D{0,0,0}, vk::Offset3D{mipWidth * 2, mipHeight * 2, 1}} },
                .dstSubresource = { vk::ImageAspectFlagBits::eColor, i, 0, 1 },
                .dstOffsets = { {vk::Offset3D{0,0,0}, vk::Offset3D{mipWidth, mipHeight, 1}} }
            };
            cmd.blitImage(srcImage, vk::ImageLayout::eTransferSrcOptimal, srcImage, vk::ImageLayout::eTransferDstOptimal, 1, &blit, vk::Filter::eLinear);

            // 4. Переводим srcImage Mip i в TransferSrcOptimal (для копирования в finalImage)
            transitionMip(cmd, srcImage, i, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                          vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
                          vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);

            // 5. Переводим dstImage Mip i в TransferDstOptimal (для записи)
            transitionMip(cmd, dstImage, i, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                          {}, vk::AccessFlagBits::eTransferWrite,
                          vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);

            // 6. Копируем srcImage Mip i в dstImage Mip i
            vk::ImageCopy copyRegion{
                .srcSubresource = { vk::ImageAspectFlagBits::eColor, i, 0, 1 },
                .dstSubresource = { vk::ImageAspectFlagBits::eColor, i, 0, 1 },
                .extent = { static_cast<uint32_t>(mipWidth), static_cast<uint32_t>(mipHeight), 1 }
            };
            cmd.copyImage(srcImage, vk::ImageLayout::eTransferSrcOptimal, dstImage, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);

            // 7. Финализируем dstImage Mip i
            transitionMip(cmd, dstImage, i, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
                          vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader);
        }
    }
}