//
// Created by Shagu on 20.06.2026.
//

#include "ShadowRenderPass.hpp"

#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle_engine::Core {
    vk::ResultValue<ShadowRenderPass> ShadowRenderPass::create(
        vk::Device device,
        vk::RenderPass renderPass,
        uint32_t subpass,
        vk::DescriptorSetLayout samplerSetLayout,
        vk::DescriptorSetLayout sceneSetLayout,
        vk::DescriptorSetLayout materialSetLayout) {
        ShadowRenderPass shadowRenderPass;
        auto result = shadowRenderPass.init(
            device,
            renderPass,
            subpass,
            samplerSetLayout,
            sceneSetLayout,
            materialSetLayout
        );
        return {result, std::move(shadowRenderPass)};
    }

    void ShadowRenderPass::recordCommandBufferCommands(
        vk::CommandBuffer cmd,
        DeviceSceneData const &sceneData,
        FrameData const &frameData,
        vk::DescriptorSet samplerSet) const {

        vk::Viewport viewport{
            .x = 0.0f, .y = 0.0f,
            .width = static_cast<float>(frameData.shadowExtent.width),
            .height = static_cast<float>(frameData.shadowExtent.height),
            .minDepth = 0.0f, .maxDepth = 1.0f
        };
        vk::Rect2D scissor{
            .offset = {0, 0},
            .extent = frameData.shadowExtent
        };

        cmd.setViewport(0, 1, &viewport);
        cmd.setScissor(0, 1, &scissor);
        cmd.setDepthBias(0.0f, 0.0f, 0.0f);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *shadowPipeline);

        std::array shadowDescriptorSets{
            frameData.sceneDataSet
        };

        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *shadowPipelineLayout,
            0, shadowDescriptorSets,
            {}
        );

        uint32_t totalIndirectCount = 0;
        for (auto indirectDraw : sceneData.indirectDraws) {
            totalIndirectCount += indirectDraw.commandCount;
        }

        vk::Buffer vBuffer = *sceneData.vertexBuffer;
        vk::DeviceSize offset = sceneData.positionAttributeOffset;
        cmd.bindVertexBuffers(0, 1, &vBuffer, &offset);
        cmd.bindIndexBuffer(*sceneData.indexBuffer, sceneData.indexBufferOffset, vk::IndexType::eUint32);

        cmd.drawIndexedIndirect(
            *sceneData.indirectDrawBuffer,
            sceneData.indirectDrawBufferOffset,
            totalIndirectCount,
            sizeof(vk::DrawIndexedIndirectCommand)
        );
    }

    vk::Result ShadowRenderPass::init(
        vk::Device device,
        vk::RenderPass renderPass,
        uint32_t subpass,
        vk::DescriptorSetLayout,
        vk::DescriptorSetLayout sceneSetLayout,
        vk::DescriptorSetLayout) {

        vk::DescriptorSetLayout const shadowLayouts[] {
            sceneSetLayout
        };

        vk::PipelineLayoutCreateInfo const createInfo{
            .setLayoutCount = std::size(shadowLayouts),
            .pSetLayouts = shadowLayouts
        };

        auto result = device.createPipelineLayoutUnique(createInfo);
        if (result.result != vk::Result::eSuccess) return result.result;

        shadowPipelineLayout = std::move(result.value);

        // 1. Шейдеры (только Vertex, так как фрагментный для тени не нужен)
        auto vertModule = loadAndCreateShaderModuleUnique(device, "shaders/shadow.vert.spv");
        if (vertModule.result != vk::Result::eSuccess) {
            return vertModule.result;
        }
        vk::PipelineShaderStageCreateInfo shaderStages[] = {
            {
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = *vertModule.value,
                .pName = "main"
            },
        };

        // Только для позиции
        vk::VertexInputBindingDescription vertexBindingDescription {
            .binding = 0,
            .stride = sizeof(PositionAttribute),
            .inputRate = vk::VertexInputRate::eVertex
        };

        // Атрибут для позиции
        vk::VertexInputAttributeDescription vertexAttributeDescription {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = 0
        };

        // 2. Состояния
        vk::PipelineVertexInputStateCreateInfo vertexInput{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexBindingDescription,
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = &vertexAttributeDescription
        }; // Твой формат вершин
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList };
        vk::PipelineViewportStateCreateInfo viewport{ .viewportCount = 1, .scissorCount = 1 };

        // ВАЖНО для теней: Depth Bias (против "теневых прыщей")
        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_TRUE, // Включаем!
            .lineWidth = 1.0f
        };

        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE
        };

        vk::PipelineDepthStencilStateCreateInfo depthStencil{
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = vk::CompareOp::eLess
        };

        // Нет ColorBlendState (так как нет цветового буфера!)
        vk::PipelineColorBlendStateCreateInfo colorBlend{ .logicOpEnable = VK_FALSE };

        // Динамические состояния для Viewport (чтобы использовать Атлас теней)
        vk::DynamicState dynamicStates[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eDepthBias };
        vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = 3, .pDynamicStates = dynamicStates };

        vk::GraphicsPipelineCreateInfo pipelineInfo{
            .stageCount = 1, .pStages = shaderStages,
            .pVertexInputState = &vertexInput, .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewport, .pRasterizationState = &rasterizer, .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil, .pColorBlendState = &colorBlend,
            .pDynamicState = &dynamicState,
            .layout = shadowPipelineLayout.get(),
            .renderPass = renderPass,
            .subpass = subpass
        };

        auto res = device.createGraphicsPipelineUnique(nullptr, pipelineInfo);
        if (res.result != vk::Result::eSuccess) return res.result;
        shadowPipeline = std::move(res.value);
        return vk::Result::eSuccess;
    }

    RenderPassInfo ShadowRenderPassFactory::getRenderPassInfo() const {
        RenderPassInfo info;
        info.attachments = {
            {
                .format = vk::Format::eD32SfloatS8Uint,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            }
        };

        info.attachmentReferences = {
            {
                .attachment = 0,
                .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
            }
        };

        info.subpasses = {
            {
                .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                .colorAttachmentCount = 0,
                .pDepthStencilAttachment = &info.attachmentReferences[0]
            }
        };

        // Зависимости отличные! Они синхронизируют запись глубины и последующее чтение.
        info.dependencies = {
            {
                .srcSubpass = vk::SubpassExternal,
                .dstSubpass = 0,
                // Смена: теперь мы ждем начала прохода, а не фрагментного шейдера
                .srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
                .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
                .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
                .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                .dependencyFlags = vk::DependencyFlagBits::eByRegion
            },
            {
                .srcSubpass = 0,
                .dstSubpass = vk::SubpassExternal,
                .srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests,
                .dstStageMask = vk::PipelineStageFlagBits::eFragmentShader,
                .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                .dependencyFlags = vk::DependencyFlagBits::eByRegion
            }
        };

        info.renderPassCreateInfo = {
            .attachmentCount = 1,
            .pAttachments = info.attachments.data(),
            .subpassCount = 1,
            .pSubpasses = info.subpasses.data(),
            .dependencyCount = 2,
            .pDependencies = info.dependencies.data()
        };
        return info;
    }

    vk::ResultValue<std::unique_ptr<IRenderPass>> ShadowRenderPassFactory::createRenderPass(
        vk::Device device,
        vk::RenderPass renderPass,
        uint32_t subpass,
        vk::DescriptorSetLayout samplerSetLayout,
        vk::DescriptorSetLayout sceneSetLayout,
        vk::DescriptorSetLayout materialSetLayout) const {

        auto [createRenderPassResult, shadowRenderPass] = ShadowRenderPass::create(
            device,
            renderPass,
            subpass,
            samplerSetLayout,
            sceneSetLayout,
            materialSetLayout
        );
        return {createRenderPassResult, std::make_unique<ShadowRenderPass>(std::move(shadowRenderPass))};
    }
} // shuttle_engine