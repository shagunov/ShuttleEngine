//
// Created by Shagu on 20.06.2026.
//

#ifndef HELLOTRIANGLE_IRENDERPASS_HPP
#define HELLOTRIANGLE_IRENDERPASS_HPP
#include <memory>
#include "IncludeVulkan.hpp"

#include "DeviceAllocator/DeviceAllocator.hpp"
#include "PbrRender/Render.hpp"

namespace shuttle_engine::Core {

    class RenderPassInfo {
    public:
        std::vector<vk::AttachmentDescription> attachments;
        std::vector<vk::SubpassDescription> subpasses;
        std::vector<vk::SubpassDependency> dependencies;
        std::vector<vk::ClearValue> clearValues;
        std::vector<vk::AttachmentReference> attachmentReferences;
        vk::RenderPassCreateInfo renderPassCreateInfo;
    };


    class IRenderPass {
    public:
        virtual void recordCommandBufferCommands(
            vk::CommandBuffer cmd,
            DeviceSceneData const& sceneData,
            FrameData const& frameData,
            vk::DescriptorSet samplerSet
        ) const = 0;

        virtual ~IRenderPass() = default;
    };

    class IRenderPassFactory {
    public:
        [[nodiscard]] virtual RenderPassInfo getRenderPassInfo() const = 0;

        [[nodiscard]] virtual vk::ResultValue<std::unique_ptr<IRenderPass>> createRenderPass(
            vk::Device device,
            vk::RenderPass renderPass,
            uint32_t subpass,
            vk::DescriptorSetLayout samplerSetLayout,
            vk::DescriptorSetLayout sceneSetLayout,
            vk::DescriptorSetLayout materialSetLayout) const = 0;

        virtual ~IRenderPassFactory() = default;
    };
}


#endif //HELLOTRIANGLE_IRENDERPASS_HPP
