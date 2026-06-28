//
// Created by Shagu on 20.06.2026.
//

#ifndef HELLOTRIANGLE_PBRMAINRENDERPASS_HPP
#define HELLOTRIANGLE_PBRMAINRENDERPASS_HPP
#include "rendering/Render/IRenderPass.hpp"

namespace shuttle_engine::Core {
    class PbrMainRenderPass final : public IRenderPass{
    public:
        [[nodiscard]] static vk::ResultValue<PbrMainRenderPass> create(
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
        PbrMainRenderPass() = default;
        vk::Result init(
            vk::Device device,
            vk::RenderPass renderPass,
            uint32_t subpass,
            vk::DescriptorSetLayout samplerSetLayout,
            vk::DescriptorSetLayout sceneSetLayout,
            vk::DescriptorSetLayout materialSetLayout
        );

        vk::UniquePipeline mainPipeline;
        vk::UniquePipelineLayout mainPipelineLayout;
    };

    class PbrMainRenderPassFactory final : public IRenderPassFactory {
    public:
        PbrMainRenderPassFactory() = default;

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

#endif //HELLOTRIANGLE_PBRMAINRENDERPASS_HPP
