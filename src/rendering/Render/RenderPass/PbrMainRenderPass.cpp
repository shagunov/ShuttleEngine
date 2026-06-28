//
// Created by Shagu on 20.06.2026.
//

#include "PbrMainRenderPass.hpp"

#include <memory>

#include "assimp/Vertex.h"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle_engine::Core {
    RenderPassInfo PbrMainRenderPassFactory::getRenderPassInfo() const {
        RenderPassInfo info;

        // 1. Описываем аттачменты
        info.attachments = {
            { // Color Attachment
                .format = vk::Format::eB8G8R8A8Srgb,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::ePresentSrcKHR
            },
            { // Depth Attachment
                .format = vk::Format::eD32SfloatS8Uint,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eDontCare,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
            }
        };

        info.attachmentReferences = {
            {
                .attachment = 0,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            },
            {
                .attachment = 1,
                .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
            }
        };

        info.subpasses = {
            vk::SubpassDescription{
                .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                .colorAttachmentCount = 1,
                .pColorAttachments = &info.attachmentReferences[0],
                .pDepthStencilAttachment = &info.attachmentReferences[1]
            }
        };

        info.dependencies = {
            {
                .srcSubpass = vk::SubpassExternal,
                .dstSubpass = 0,
                .srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
                .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .srcAccessMask = {},
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dependencyFlags = vk::DependencyFlagBits::eByRegion
            },
            {
                .srcSubpass = 0,
                .dstSubpass = vk::SubpassExternal,
                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dstAccessMask = {},
                .dependencyFlags = vk::DependencyFlagBits::eByRegion
            }
        };

        info.renderPassCreateInfo = {
            .attachmentCount = 2,
            .pAttachments = info.attachments.data(),
            .subpassCount = 1,
            .pSubpasses = info.subpasses.data(),
            .dependencyCount = 2,
            .pDependencies = info.dependencies.data()
        };

        return info;
    }

    vk::ResultValue<std::unique_ptr<IRenderPass>> PbrMainRenderPassFactory::createRenderPass(
    vk::Device device,
    vk::RenderPass renderPass,
    uint32_t subpass,
    vk::DescriptorSetLayout samplerSetLayout,
    vk::DescriptorSetLayout sceneSetLayout,
    vk::DescriptorSetLayout materialSetLayout) const {

        auto [res, pbr] = PbrMainRenderPass::create(device, renderPass, subpass, samplerSetLayout, sceneSetLayout, materialSetLayout);
        if (res != vk::Result::eSuccess) return {res, nullptr};

        return {vk::Result::eSuccess, std::make_unique<PbrMainRenderPass>(std::move(pbr))};
    }


    vk::ResultValue<PbrMainRenderPass> PbrMainRenderPass::create(
        vk::Device device,
        vk::RenderPass renderPass,
        uint32_t subpass,
        vk::DescriptorSetLayout samplerSetLayout,
        vk::DescriptorSetLayout sceneSetLayout,
        vk::DescriptorSetLayout materialSetLayout
    ) {
        PbrMainRenderPass pass;
        vk::Result res = pass.init(device, renderPass, subpass, samplerSetLayout, sceneSetLayout, materialSetLayout);
        if (res != vk::Result::eSuccess) return { res, {} };
        return { vk::Result::eSuccess, std::move(pass) };
    }

    vk::Result PbrMainRenderPass::init(
        vk::Device device,
        vk::RenderPass renderPass,
        uint32_t subpass,
        vk::DescriptorSetLayout samplerSetLayout,
        vk::DescriptorSetLayout sceneSetLayout,
        vk::DescriptorSetLayout materialSetLayout
    ) {

        // ВАЖНО: порядок в массиве определяет номер set в шейдере: layout(set = 0, binding = X) и layout(set = 1, binding = Y)
        vk::DescriptorSetLayout const layouts[] = {
            samplerSetLayout,     // Будет доступен как set = 0
            sceneSetLayout, // Будет доступен как set = 1
            materialSetLayout,  // Будет доступен как set = 2
        };

        vk::PipelineLayoutCreateInfo const createInfo{
            .setLayoutCount = std::size(layouts),
            .pSetLayouts = layouts
        };

        auto res = device.createPipelineLayoutUnique(createInfo);
        if (res.result != vk::Result::eSuccess) return res.result;

        mainPipelineLayout = std::move(res.value);

        // 1. Загрузка шейдеров
        auto vertModule = loadAndCreateShaderModuleUnique(device, "shaders/pbr.vert.spv");
        auto fragModule = loadAndCreateShaderModuleUnique(device, "shaders/pbr.frag.spv");

        vk::PipelineShaderStageCreateInfo shaderStages[] = {
            {
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = *vertModule.value,
                .pName = "main"
            },
            {
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = *fragModule.value,
                .pName = "main"
            }
        };

        // 2. Описание формата вершин (Вершинный Буфер)
        std::array vertexBindingDescriptions {
            // For positions only
            vk::VertexInputBindingDescription{
                .binding = 0,
                .stride = sizeof(PositionAttribute),
                .inputRate = vk::VertexInputRate::eVertex
            },
            // For normals, tangents, uvs
            vk::VertexInputBindingDescription{
                .binding = 1,
                .stride = sizeof(NormalTangentUvAttribute), // Шаг равен размеру всей структуры вершины
                .inputRate = vk::VertexInputRate::eVertex
            }
        };

        std::array vertexAttributeDescriptions {
            vk::VertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = 0
            },
            vk::VertexInputAttributeDescription{
                .location = 1,
                .binding = 1,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = offsetof(NormalTangentUvAttribute, normal)
            },
            vk::VertexInputAttributeDescription{
                .location = 2,
                .binding = 1,
                .format = vk::Format::eR32G32Sfloat,
                .offset = offsetof(NormalTangentUvAttribute, uv)
            },
            vk::VertexInputAttributeDescription{
                .location = 3,
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = offsetof(NormalTangentUvAttribute, tangent)
            }
        };

        vk::PipelineVertexInputStateCreateInfo vertexInput {
            .vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDescriptions.size()),
            .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size()),
            .pVertexAttributeDescriptions = vertexAttributeDescriptions.data()
        };

        // 3. Сборка геометрии (треугольники)
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE
        };

        // 4. Настройка Viewport и Scissor (делаем динамическими, как и в тенях)
        vk::PipelineViewportStateCreateInfo viewportState {
            .viewportCount = 1,
            .scissorCount = 1
        };

        // 5. Растеризатор (для PBR важен правильный обход граней)
        vk::PipelineRasterizationStateCreateInfo rasterizer {
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack, // Отсекаем задние грани для оптимизации
            .frontFace = vk::FrontFace::eCounterClockwise, // Направление обхода по часовой стрелке
            .depthBiasEnable = VK_FALSE, // Здесь смещение глубины выключено!
            .lineWidth = 1.0f
        };

        // 6. Мультисемплинг (выключен для простоты, 1 сэмпл)
        vk::PipelineMultisampleStateCreateInfo multisampling {
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE
        };

        // 7. Тест глубины (ОБЯЗАТЕЛЕН, иначе сцена превратится в кашу)
        vk::PipelineDepthStencilStateCreateInfo depthStencil {
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = vk::CompareOp::eLess, // Отрисовываем то, что ближе к камере
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE
        };

        // 8. Смешивание цветов (Color Blending)
        // Для непрозрачного PBR-материала смешивание выключено, но запись во все каналы включена
        vk::PipelineColorBlendAttachmentState colorBlendAttachment {
            .blendEnable = VK_FALSE,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        };

        vk::PipelineColorBlendStateCreateInfo colorBlending {
            .logicOpEnable = VK_FALSE,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment
        };

        // 9. Динамические состояния (чтобы менять размер окна без пересоздания пайплайна)
        std::array dynamicStates {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo dynamicState {
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        // 10. Финальная сборка Graphics Pipeline
        vk::GraphicsPipelineCreateInfo const pipelineCreateInfo {
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInput,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = mainPipelineLayout.get(), // Макет основного конвейера
            .renderPass = renderPass,  // Проход основного рендеринга,
            .subpass = subpass
        };

        auto mainPipelineResultValue = device.createGraphicsPipelineUnique(nullptr, pipelineCreateInfo);
        if (mainPipelineResultValue.result != vk::Result::eSuccess) {
            return mainPipelineResultValue.result;
        }
        mainPipeline = std::move(mainPipelineResultValue.value);

        return vk::Result::eSuccess;
    }

    void PbrMainRenderPass::recordCommandBufferCommands(
            vk::CommandBuffer cmd,
            DeviceSceneData const& sceneData,
            FrameData const& frameData,
            vk::DescriptorSet samplerSet
    ) const {
        // bind pipeline
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mainPipeline);

        // bind descriptor sets: samplerSet is bound by RenderGraph or passed in frameData.sceneDataSet
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mainPipelineLayout, 0, 1, &frameData.sceneDataSet, 0, nullptr);

        // bind VBO/IBO
        vk::Buffer vBuf = *sceneData.vertexBuffer;
        vk::DeviceSize vOff = 0;
        cmd.bindVertexBuffers(0, 1, &vBuf, &vOff);
        cmd.bindIndexBuffer(*sceneData.indexBuffer, sceneData.indexBufferOffset, vk::IndexType::eUint32);

        // iterate groups
        for (const auto& drawGroup : sceneData.indirectDraws) {
            // bind material descriptor set:
            auto matSet = sceneData.materials[drawGroup.materialIndex].materialSet;
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mainPipelineLayout, 2, 1, &matSet, 0, nullptr);

            cmd.drawIndexedIndirect(*sceneData.indirectDrawBuffer, drawGroup.indirectBufferOffset, drawGroup.commandCount, sizeof(vk::DrawIndexedIndirectCommand));
        }
    }
} // shuttle_engine
