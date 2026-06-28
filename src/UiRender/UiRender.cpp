//
// Created by Shagu on 14.06.2026.
// Refactored on 18.06.2026.
//

#include "UiRender.hpp"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_sdl2.h>
#include <stdexcept>

namespace shuttle_engine {

    thread_local VkResult UiRenderCreateResult;

    vk::ResultValue<UiRender> UiRender::create(SdlWindow &window, vk::Instance instance,
        vk::PhysicalDevice physicalDevice, vk::Device device, uint32_t queueFamilyIndex, vk::Queue queue,
        uint32_t imageCount, vk::RenderPass renderPass) {

        UiRender result;

        std::array poolSizes = {
            vk::DescriptorPoolSize{ vk::DescriptorType::eSampler, 100 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eCombinedImageSampler, 100 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eSampledImage, 100 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eStorageImage, 100 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eUniformTexelBuffer, 100 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eStorageTexelBuffer, 100 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eUniformBuffer, 100 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eStorageBuffer, 100 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eUniformBufferDynamic, 100 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eStorageBufferDynamic, 100 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eInputAttachment, 100 }
        };

        auto [createDescriptorPoolResult, uniqueDescriptorPool] = device.createDescriptorPoolUnique(
            {
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = 100,
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data()
            }
        );
        if (createDescriptorPoolResult != vk::Result::eSuccess) return {createDescriptorPoolResult, {}};

        result.uiDescriptorPool = std::move(uniqueDescriptorPool);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        ImGui::StyleColorsDark();

        ImGui_ImplSDL2_InitForVulkan(window.getWindow());

        ImGui_ImplVulkan_InitInfo initInfo {
            .ApiVersion = vk::makeApiVersion(0, 1, 0, 0),
            .Instance = instance,
            .PhysicalDevice = physicalDevice,
            .Device = device,
            .QueueFamily = queueFamilyIndex,
            .Queue = queue,
            .DescriptorPool = *result.uiDescriptorPool,
            .MinImageCount = imageCount,
            .ImageCount = imageCount,
            .PipelineInfoMain = {
                .RenderPass = renderPass,
                .Subpass = 0,
                .MSAASamples = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1),
            },
            .UseDynamicRendering = false,
            .Allocator = nullptr,
            .CheckVkResultFn = [] (VkResult result) { UiRenderCreateResult = result; },
            .MinAllocationSize = 2048 * 2048
        };

        ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* function_name, void* user_data) {
            return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
                static_cast<VkInstance>(*reinterpret_cast<vk::Instance*>(user_data)),
                function_name
            );
        }, &instance);

        ImGui_ImplVulkan_Init(&initInfo);

        if (auto initResult = result.initRenderPass(device, vk::ImageLayout::eColorAttachmentOptimal); initResult != vk::Result::eSuccess) {
            return {initResult, {}};
        }

        return {static_cast<vk::Result>(UiRenderCreateResult), std::move(result)};
    }

    void UiRender::drawUi(IuiPainter& painter, Engine& engine) { // Принимаем Engine&
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        painter.drawUi(engine); // Вызываем painter с Engine&
        ImGui::Render();
    }

    void UiRender::recordDrawCommands(vk::CommandBuffer cmdBuffer) const {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
    }

    // В UiRender.cpp
    void UiRender::recordDrawOffscreenCommands(vk::CommandBuffer cmdBuffer, ImGuiRenderTarget const& target) const {
        // Очищаем цветом (например, черным, так как UI всё перекроет)
        vk::ClearValue clearColor(vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}});

        // Начинаем RenderPass для UI
        vk::RenderPassBeginInfo renderPassInfo{
            .renderPass = *uiRenderPass, // Используем наш новый uiRenderPass
            .framebuffer = *target.targetFramebuffer,
            .renderArea = vk::Rect2D{ {0,0}, target.attachmentExtent },
            .clearValueCount = 1,
            .pClearValues = &clearColor
        };

        cmdBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

        // Здесь ImGui сам рисует все свои элементы, включая ImGui::Image()
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);

        cmdBuffer.endRenderPass();
    }


    void UiRender::destroy(vk::Device device) {
        if (device.waitIdle() != vk::Result::eSuccess) throw std::runtime_error("wait-idle");
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    std::vector<ImGuiVulkanImage> UiRender::createVulkanViewportImages(std::vector<OffscreenRenderTarget> const &offscreenRenderTarget, uint32_t frameCount) const {
        std::vector<ImGuiVulkanImage> images(frameCount);
        for (unsigned int i = 0; i < frameCount; i++) {
            images[i] = {ImGui_ImplVulkan_AddTexture(*offscreenRenderTarget[i].colorAttachmentImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};
        }
        return images;
    }

    vk::ResultValue<std::vector<ImGuiRenderTarget>> UiRender::createRenderTargets(
        vk::Device device,
        std::vector<vk::Image> const& imageAttachments,
        vk::Extent2D attachmentExtent) {

        std::vector<ImGuiRenderTarget> renderTargets;
        renderTargets.reserve(imageAttachments.size());

        for (auto imageAttachment: imageAttachments) {
            auto [imageViewCreateResult, uniqueAttachmentImageView] = device.createImageViewUnique(
                {
                    .image = imageAttachment,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eB8G8R8A8Srgb,
                    .components {
                        .r = vk::ComponentSwizzle::eR,
                        .g = vk::ComponentSwizzle::eG,
                        .b = vk::ComponentSwizzle::eB,
                        .a = vk::ComponentSwizzle::eA
                    },
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                }
            );
            if (imageViewCreateResult != vk::Result::eSuccess) {
                return {imageViewCreateResult, {}};
            }

            auto [framebufferCreateResult, uniqueAttachmentFramebuffer] = device.createFramebufferUnique(
                {
                    .renderPass = *uiRenderPass,
                    .attachmentCount = 1,
                    .pAttachments = &*uniqueAttachmentImageView,
                    .width = attachmentExtent.width,
                    .height = attachmentExtent.height,
                    .layers = 1
                }
            );
            if (framebufferCreateResult != vk::Result::eSuccess) {
                return {framebufferCreateResult, {}};
            }

            renderTargets.emplace_back(
                std::move(uniqueAttachmentImageView),
                std::move(uniqueAttachmentFramebuffer),
                attachmentExtent
            );
        }
        return {vk::Result::eSuccess, std::move(renderTargets)};
    }

    vk::Result UiRender::initRenderPass(vk::Device device, vk::ImageLayout finalLayout) {
        std::array attachmentDescriptions {
            vk::AttachmentDescription{
                .format = vk::Format::eB8G8R8A8Srgb,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = finalLayout
            }
        };

        std::array attachmentReferences {
            vk::AttachmentReference{
                .attachment = 0,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            }
        };

        vk::SubpassDescription subpassDescription {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReferences[0]
        };

        std::array subpassDependencies {
            vk::SubpassDependency{
                .srcSubpass = vk::SubpassExternal,
                .dstSubpass = 0,
                .srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
                .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .srcAccessMask = {},
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dependencyFlags = vk::DependencyFlagBits::eByRegion
            },
            vk::SubpassDependency{
                .srcSubpass = 0,
                .dstSubpass = vk::SubpassExternal,
                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dstAccessMask = {},
                .dependencyFlags = vk::DependencyFlagBits::eByRegion
            }
        };

        vk::RenderPassCreateInfo const renderPassCreateInfo {
            .attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size()),
            .pAttachments = attachmentDescriptions.data(),
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = static_cast<uint32_t>(subpassDependencies.size()),
            .pDependencies = subpassDependencies.data()
        };

        auto uiRenderPassResultValue = device.createRenderPassUnique(renderPassCreateInfo);
        if (uiRenderPassResultValue.result != vk::Result::eSuccess) {
            return uiRenderPassResultValue.result;
        }
        uiRenderPass = std::move(uiRenderPassResultValue.value);
        return vk::Result::eSuccess;
    }
} // shuttle_engine