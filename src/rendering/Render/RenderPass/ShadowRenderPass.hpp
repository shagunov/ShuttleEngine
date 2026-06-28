//
// Created by Shagu on 20.06.2026.
//

#ifndef HELLOTRIANGLE_SHADOWRENDERPASS_HPP
#define HELLOTRIANGLE_SHADOWRENDERPASS_HPP
#include "IncludeVulkan.hpp"
#include "PbrRender/Render.hpp"
#include "rendering/Render/IRenderPass.hpp"

namespace shuttle_engine::Core {
    class ShadowRenderPass final : public IRenderPass {
    public:
        [[nodiscard]] static vk::ResultValue<ShadowRenderPass> create(
            vk::Device device,
            vk::RenderPass renderPass,
            uint32_t subpass,
            vk::DescriptorSetLayout samplerSetLayout,
            vk::DescriptorSetLayout sceneSetLayout,
            vk::DescriptorSetLayout materialSetLayout
        );

        void recordCommandBufferCommands(
            vk::CommandBuffer cmd,
            DeviceSceneData const& sceneData,
            FrameData const& frameData,
            vk::DescriptorSet samplerSet
        ) const override;
    private:
        ShadowRenderPass() = default;
        vk::Result init(
            vk::Device device,
            vk::RenderPass renderPass,
            uint32_t subpass,
            vk::DescriptorSetLayout samplerSetLayout,
            vk::DescriptorSetLayout sceneSetLayout,
            vk::DescriptorSetLayout materialSetLayout
        );

        vk::UniquePipeline shadowPipeline;
        vk::UniquePipelineLayout shadowPipelineLayout;
    };

    class ShadowRenderPassFactory final : public IRenderPassFactory {
    public:
        ShadowRenderPassFactory() = default;

        [[nodiscard]] RenderPassInfo getRenderPassInfo() const override;

        [[nodiscard]] vk::ResultValue<std::unique_ptr<IRenderPass>> createRenderPass(
            vk::Device device,
            vk::RenderPass renderPass,
            uint32_t subpass,
            vk::DescriptorSetLayout samplerSetLayout,
            vk::DescriptorSetLayout sceneSetLayout,
            vk::DescriptorSetLayout materialSetLayout) const override;
    };
} // shuttle_engine

#endif //HELLOTRIANGLE_SHADOWRENDERPASS_HPP
