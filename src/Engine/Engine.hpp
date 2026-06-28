#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <optional> // Для поздней инициализации на стеке

#include "../Sdl/SdlLibrary/SdlLibrary.hpp"
#include "../Sdl/SdlWindow/SdlWindow.hpp"
#include "../DeviceAllocator/DeviceAllocator.hpp"
#include "../Camera/Camera.hpp"
#include "../CameraController/CameraController.hpp"
#include "../PbrRender/Render.hpp"
#include "../FrameManager/FrameManager.hpp"
#include "../RetireController/RetireController.hpp"
#include "../SwapchainFactory/SwapchainFactory.hpp"
#include "../UiRender/UiRender.hpp"

#include "../Input/Command.hpp"
#include "../Input/InputFocus.hpp"
#include "../Input/ActiveMask.hpp"
#include "../Input/ModularInputSettings.hpp"
#include "../Input/CommandBuffer.hpp"

namespace shuttle_engine {

    class Engine {
    public:

        friend class DebugOverlayPainter;

        // Единственный способ создать движок — через статический метод фабрики
        [[nodiscard]] static std::unique_ptr<Engine> create();

        ~Engine();

        // Полный запрет копирования и перемещения (гарантия стабильного адреса в памяти)
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        void run();

    private:
        // Приватный конструктор: вызывается только внутри метода Engine::create()
        Engine();
        void init();

        // 1. Системный слой (уничтожается последним)
        SdlLibrary sdlLibrary;
        SdlWindow window;

        // 2. Vulkan контекст и отладка
        vk::UniqueInstance uniqueInstance;
        vk::UniqueDebugUtilsMessengerEXT messenger;
        vk::UniqueSurfaceKHR uniqueSurface;
        vk::PhysicalDevice physicalDevice;
        vk::UniqueDevice uniqueDevice;

        // Очереди
        uint32_t graphicsQueueFamilyIndex = 0;
        uint32_t transferQueueFamilyIndex = 0;
        uint32_t presentationQueueFamilyIndex = 0;
        vk::Queue graphicsQueue;
        vk::Queue presentationQueue;
        vk::Queue transferQueue;

        // 3. Ресурсы и память GPU (используем optional для ленивой инициализации)
        vk::UniqueCommandPool uniqueTransferCommandPool;
        std::optional<resources::UniqueAllocator> uniqueAllocator;

        // 4. Пайплайн рендеринга и данные сцены
        std::optional<PbrRender> pbrRender;
        DeviceSceneData deviceSceneData;

        // 5. Камера
        Camera camera;
        CameraController cameraController;

        // 6. Кадры и свопчейн
        uint32_t frameCount = 2U;
        uint32_t currentFrameIndex = 0U;
        vk::UniqueDescriptorPool frameDataDescriptorPool;
        std::vector<FrameData> frameDatas;

        SwapchainContext swapchainContext;
        std::optional<SwapchainResources> activeResources;
        RetireController retireController;

        // 7. Ввод и интерфейс
        CommandBuffer commandBuffer;
        ModularInputSettings inputSettings;
        ActiveMask activeMask;
        InputFocusState focusState;
        std::optional<UiRender> uiRender;

        // Вспомогательные методы
        void recreateAllResources();

    };

} // namespace shuttle_engine
