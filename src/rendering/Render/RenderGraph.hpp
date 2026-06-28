#pragma once
#include "IncludeVulkan.hpp"
#include <vector>
#include <string>
#include <memory>
#include <utility>

#include "IRenderPass.hpp" // Твои интерфейсы IRenderPass, IRenderPassFactory, RenderPassInfo
#include "DeviceAllocator/DeviceAllocator.hpp" // Твой аллокатор памяти

namespace shuttle_engine::Core {

    struct RegisteredPass {
        std::string name;
        std::unique_ptr<IRenderPass> pass;               // Исполнитель
        vk::UniqueRenderPass vkRenderPass;               // Владелец объекта RenderPass
        std::vector<vk::UniqueFramebuffer> framebuffers; // Фреймбуферы под каждый кадр свопчейна
        std::vector<vk::ClearValue> clearValues;         // Значения очистки экрана
        vk::Extent2D extent;                             // Размер области рендеринга
    };

    class RenderGraph {
    public:
        RenderGraph(vk::Device device, resources::DeviceAllocator allocator)
            : device(device), allocator(allocator) {}

        // Регистрация фабрик проходов
        void registerPassFactory(std::string name, std::unique_ptr<IRenderPassFactory> factory) {
            factories.emplace_back(std::move(name), std::move(factory));
        }

        // Инициализация глобальных макетов дескрипторов (Set 0, Set 1, Set 2)
        vk::Result initLayouts();

        // Создание глобального набора сэмплеров (Set 0)
        vk::Result initSamplers();

        // Сборка графа: создание RenderPass-ов, пайплайнов и привязка ресурсов
        vk::Result compile(const std::vector<std::vector<vk::ImageView>>& attachmentsPerPass, vk::Extent2D extent);

        // Запись команд кадра
        void recordFrame(
            vk::CommandBuffer cmd,
            DeviceSceneData const& sceneData,
            FrameData const& frameData,
            uint32_t imageIndex
        );

        void destroy();

    private:
        vk::Device device;
        resources::DeviceAllocator allocator;
        std::vector<std::pair<std::string, std::unique_ptr<IRenderPassFactory>>> factories;
        std::vector<RegisteredPass> registeredPasses;

        // Глобальные сэмплеры
        vk::UniqueSampler materialSampler;
        vk::UniqueSampler shadowSampler;

        // Глобальные Layouts
        vk::UniqueDescriptorSetLayout samplerSetLayout;
        vk::UniqueDescriptorSetLayout sceneDataSetLayout;
        vk::UniqueDescriptorSetLayout materialSetLayout;

        // Глобальный набор дескрипторов (Set 0)
        vk::UniqueDescriptorPool samplerDescriptorPool;
        vk::UniqueDescriptorSet samplerSet;
    };
}