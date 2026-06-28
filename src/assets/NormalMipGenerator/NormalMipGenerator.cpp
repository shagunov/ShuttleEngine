#include "NormalMipGenerator.hpp"
#include <fstream>
#include <stdexcept>
#include <algorithm> // For std::max

// Вспомогательная функция (можешь использовать свою)
static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open shader!");
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

namespace shuttle_engine::Core {

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

    NormalMipGenerator::NormalMipGenerator(vk::Device device)
        : device(device) 
    {
        // Инициализация пула дескрипторов (для временных сетов)
        std::array poolSizes{
            vk::DescriptorPoolSize{ vk::DescriptorType::eStorageImage, 2 } // src и dst для одного вызова
        };
        descriptorPool = device.createDescriptorPoolUnique({
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = 16, // Нужен только один сет за раз
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        }).value;

        // Инициализация Compute Pipeline
        if (initComputePipeline() != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to initialize NormalMipGenerator compute pipeline!");
        }
    }

    vk::Result NormalMipGenerator::initComputePipeline() {
        // 1. Описание биндингов для дескрипторов (Set 0)
        // Оба изображения (src и dst) используем как Storage Image в Compute шейдере
        std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
            vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eCompute
            },
            vk::DescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eCompute
            }
        };

        // Создаем Descriptor Set Layout
        auto resLayout = device.createDescriptorSetLayoutUnique({
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        });
        if (resLayout.result != vk::Result::eSuccess) return resLayout.result;
        descriptorSetLayout = std::move(resLayout.value);

        // 2. Создаем Pipeline Layout
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .setLayoutCount = 1,
            .pSetLayouts = &*descriptorSetLayout
        };
        auto resPipeLayout = device.createPipelineLayoutUnique(pipelineLayoutInfo);
        if (resPipeLayout.result != vk::Result::eSuccess) return resPipeLayout.result;
        pipelineLayout = std::move(resPipeLayout.value);

        // 3. Загружаем шейдер и создаем Shader Module
        // Убедись, что путь "shaders/normal_mip_gen.comp.spv" верен относительно .exe
        auto shaderCode = readFile("shaders/normal_mip_gen.comp.spv");

        auto resModule = device.createShaderModuleUnique({
            .codeSize = shaderCode.size(),
            .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
        });
        if (resModule.result != vk::Result::eSuccess) return resModule.result;
        vk::UniqueShaderModule shaderModule = std::move(resModule.value);

        // 4. Описываем стадию шейдера
        vk::PipelineShaderStageCreateInfo stageInfo{
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = *shaderModule,
            .pName = "main" // Имя входной функции в GLSL
        };

        // 5. Создаем Compute Pipeline
        vk::ComputePipelineCreateInfo pipelineInfo{
            .stage = stageInfo,
            .layout = *pipelineLayout
        };

        // В Vulkan создание пайплайна может вернуть ошибку или результат с пайплайном
        auto resPipeline = device.createComputePipelineUnique(nullptr, pipelineInfo);
        if (resPipeline.result != vk::Result::eSuccess) return resPipeline.result;
        computePipeline = std::move(resPipeline.value);

        return vk::Result::eSuccess;
    }

    void NormalMipGenerator::generateAndCopy(
        vk::CommandBuffer cmd,
        vk::Image srcImage,
        vk::Image dstImage,
        vk::Format format, // Добавили параметр
        uint32_t width, uint32_t height,
        uint32_t mipLevels
    ) {
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);

        // --- Mip 0: Копирование ---
        transitionMip(cmd, dstImage, 0, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                      {}, vk::AccessFlagBits::eTransferWrite,
                      vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);

        vk::ImageCopy copyRegion0{
            .srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
            .dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
            .extent = { width, height, 1 }
        };
        cmd.copyImage(srcImage, vk::ImageLayout::eTransferDstOptimal, dstImage, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion0);

        transitionMip(cmd, dstImage, 0, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                      vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
                      vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader);

        // --- Цикл генерации последующих мипов ---
        for (uint32_t i = 1; i < mipLevels; ++i) {
            uint32_t nextMipWidth  = std::max(1u, width >> i);
            uint32_t nextMipHeight = std::max(1u, height >> i);

            // 1. Барьеры для рабочего изображения (srcImage)
            transitionMip(cmd, srcImage, i - 1, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral,
                          vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
                          vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader);

            transitionMip(cmd, srcImage, i, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
                          {}, vk::AccessFlagBits::eShaderWrite,
                          vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader);

            // 2. Дескрипторы (Используем переданный format!)
            vk::UniqueImageView u_srcView = createMipView(device, srcImage, format, i - 1);
            vk::UniqueImageView u_dstView = createMipView(device, srcImage, format, i);

            vk::DescriptorSetAllocateInfo allocInfo{ .descriptorPool = *descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &*descriptorSetLayout };
            auto sets = device.allocateDescriptorSets(allocInfo).value;
            vk::DescriptorSet currentSet = sets[0];

            vk::DescriptorImageInfo srcInfo{ {}, *u_srcView, vk::ImageLayout::eGeneral };
            vk::DescriptorImageInfo dstInfo{ {}, *u_dstView, vk::ImageLayout::eGeneral };

            std::array<vk::WriteDescriptorSet, 2> writes{
                vk::WriteDescriptorSet{
                    .dstSet = currentSet,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageImage,
                    .pImageInfo = &srcInfo
                },
                vk::WriteDescriptorSet{
                    .dstSet = currentSet,
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageImage,
                    .pImageInfo = &dstInfo
                }
            };
            device.updateDescriptorSets(writes, {});
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, 1, &currentSet, 0, nullptr);

            // 3. Вычисление
            cmd.dispatch((nextMipWidth + 7) / 8, (nextMipHeight + 7) / 8, 1);

            // 4. Барьеры и копирование в финальное изображение
            transitionMip(cmd, srcImage, i, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
                          vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead,
                          vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer);

            transitionMip(cmd, dstImage, i, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                          {}, vk::AccessFlagBits::eTransferWrite,
                          vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);

            vk::ImageCopy copyRegion{
                .srcSubresource = { vk::ImageAspectFlagBits::eColor, i, 0, 1 },
                .dstSubresource = { vk::ImageAspectFlagBits::eColor, i, 0, 1 },
                .extent = { nextMipWidth, nextMipHeight, 1 }
            };
            cmd.copyImage(srcImage, vk::ImageLayout::eTransferSrcOptimal, dstImage, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);

            transitionMip(cmd, dstImage, i, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
                          vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader);
        }
    }
}