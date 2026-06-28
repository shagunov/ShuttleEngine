//
// Created by Shagu on 14.06.2026.
// Refactored on 18.06.2026.
//

#ifndef HELLOTRIANGLE_UIRENDER_HPP
#define HELLOTRIANGLE_UIRENDER_HPP
#include <imgui.h>

#include "IncludeVulkan.hpp"
#include "PbrRender/Render.hpp"
#include "Sdl/SdlWindow/SdlWindow.hpp"

namespace shuttle_engine {
    class Engine;

    class IuiPainter {
    public:
        // Теперь каждый рисовальщик принимает ссылку на Engine
        virtual void drawUi(Engine& engine) = 0;
        virtual ~IuiPainter() = default;
    };

    // --- ОБНОВИ ВСЕХ СВОИХ СУЩЕСТВУЮЩИХ ПРЕДКОВ (HelloWorld, Demo, FPSCounter) ---
    // Пример:
    class HelloWorldPainter : public IuiPainter {
    public:
        void drawUi(Engine& engine) override {
            ImGui::Begin("Hello World");
            ImGui::Text("Hello World");
            ImGui::End();
        }
    };

    class DemoWindowPainter : public IuiPainter {
    public:
        void drawUi(Engine& engine) override {
            ImGui::ShowDemoWindow();
        }
    };

    class FPSCounterPainter : public IuiPainter {
    public:
        void drawUi(Engine& engine) override {
            ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            float fps = ImGui::GetIO().Framerate;
            float ms = 1000.0f / fps;

            ImGui::Text("FPS: ");
            ImGui::SameLine();
            ImGui::TextColored(fps > 60.0f ? ImVec4(0,1,0,1) : ImVec4(1,1,0,1), "%.1f", fps);

            ImGui::Text("Frame Time: %.3f ms", ms);

            static float values[90] = { 0 };
            static int values_offset = 0;
            static double refresh_time = 0.0;

            if (refresh_time == 0.0) refresh_time = ImGui::GetTime();

            while (refresh_time < ImGui::GetTime()) {
                values[values_offset] = ms;
                values_offset = (values_offset + 1) % 90;
                refresh_time += 1.0 / 30.0; // 30 обновлений в секунду
            }

            ImGui::PlotLines("Latency", values, 90, values_offset, nullptr, 0.0f, 33.0f, ImVec2(0, 50));

            ImGui::End();
        }
    };

    struct ImGuiVulkanImage {
        vk::DescriptorSet descriptorSet;
    };

    struct ImGuiRenderTarget {
        vk::UniqueImageView colorAttachmentImageView;
        vk::UniqueFramebuffer targetFramebuffer;
        vk::Extent2D attachmentExtent;
    };

    class UiRender {
    public:
        static vk::ResultValue<UiRender> create(
            SdlWindow& window,
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Device device,
            uint32_t queueFamilyIndex,
            vk::Queue queue,
            uint32_t imageCount,
            vk::RenderPass renderPass
        );

        // Метод bindInputEventHandler УДАЛЕН. Ввод теперь обрабатывает ApplicationController.

        void drawUi(IuiPainter & painter, Engine& engine);

        void recordDrawCommands(vk::CommandBuffer cmdBuffer) const;

        void recordDrawOffscreenCommands(vk::CommandBuffer cmdBuffer, ImGuiRenderTarget const& target) const;

        void destroy(vk::Device device);

        [[nodiscard]] std::vector<ImGuiVulkanImage> createVulkanViewportImages(std::vector<OffscreenRenderTarget> const& offscreenRenderTarget, uint32_t frameCount) const;
        [[nodiscard]] vk::ResultValue<std::vector<ImGuiRenderTarget>> createRenderTargets(
            vk::Device device,
            std::vector<vk::Image> const& imageAttachments,
            vk::Extent2D attachmentExtent
        );

    private:

        vk::Result initRenderPass(vk::Device device, vk::ImageLayout finalLayout);

        vk::UniqueDescriptorPool uiDescriptorPool;
        vk::UniqueRenderPass uiRenderPass;
    };
} // shuttle_engine

#endif //HELLOTRIANGLE_UIRENDER_HPP
