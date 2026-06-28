#include "Engine.hpp"
#include "../AssimpLoader/AssimpLoader.hpp"
#include "../Terrain/Terrain.hpp"
#include "../Painters/SunLightControlPanel/SunLightControlPanel.hpp"
#include "../VulkanDebugger/VulkanDebugger.hpp"
#include "VkBootstrap.h"
#include <iostream>

#include "Input/ApplicationController.hpp"
#include "Input/CommandExecutor.hpp"
#include "Painters/DebugOverlay/DebugOverlay.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace shuttle_engine {

    // Фабричный метод
    std::unique_ptr<Engine> Engine::create() {
        // Используем raw-new, так как конструктор приватный (make_unique не сработает)
        auto engine = std::unique_ptr<Engine>(new Engine());
        engine->init();
        return std::move(engine);
    }

    Engine::Engine()
        : window("Shuttle Engine - Adriatic Flight", 1800, 1000)
        , camera(glm::vec3{10.0f, 30.3f, 0.0f})
        , cameraController(camera)
    {
        camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    }

    Engine::~Engine() {
        if (uniqueDevice) {
            auto waitRes = uniqueDevice->waitIdle();
            (void)waitRes;
        }

        // Уничтожаем UI пока живо логическое устройство
        if (uiRender.has_value()) {
            uiRender->destroy(*uniqueDevice);
        }

        std::cout << "[Shutdown] GPU idle confirmed. Engine destroyed cleanly.\n";
    }

    void Engine::init() {
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        window.setBorderless(true);
        window.setFullscreen(true);

        auto requiredSurfaceExtensions = SdlLibrary::getSurfaceRequiredExtensions();
        requiredSurfaceExtensions.push_back(vk::EXTDebugUtilsExtensionName);
        VulkanDebugger debugger{};
        auto messengerCreateInfo = debugger.getDebugMessengerCreateInfo();

        // 1. Создание Instance
        vkb::InstanceBuilder instanceBuilder;
        auto instanceResult = instanceBuilder.
            set_app_name("Shuttle Engine - Adriatic Flight").
            request_validation_layers(true).
            set_engine_name("Shuttle Engine").
            enable_extensions(requiredSurfaceExtensions).
            set_debug_messenger_severity(messengerCreateInfo.messageSeverity).
            set_debug_messenger_type(messengerCreateInfo.messageType).
            set_debug_callback(messengerCreateInfo.pfnUserCallback).
            build();

        if (!instanceResult.has_value()) {
            throw std::runtime_error("Failed to create Vulkan instance: " + instanceResult.error().message());
        }

        vk::Instance instance{instanceResult.value().instance};
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

        uniqueInstance = vk::UniqueInstance{
            instance,
            vk::UniqueHandleTraits<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{nullptr, VULKAN_HPP_DEFAULT_DISPATCHER}
        };

        messenger = vk::UniqueDebugUtilsMessengerEXT{
            vk::DebugUtilsMessengerEXT{instanceResult.value().debug_messenger},
            vk::UniqueHandleTraits<vk::DebugUtilsMessengerEXT, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{instance, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER}
        };

        // 2. Создание Surface
        uniqueSurface = window.createVulkanSurfaceUnique(instance);

        // 3. Выбор Physical Device
        auto vkbPhysicalDeviceResult = vkb::PhysicalDeviceSelector{instanceResult.value(), *uniqueSurface}.
            set_minimum_version(1, 0).
            set_required_features(
                vk::PhysicalDeviceFeatures{
                    .multiDrawIndirect = vk::True,
                    .samplerAnisotropy = vk::True
                }
            ).
            add_required_extension(vk::KHRSwapchainExtensionName).
            select();

        if (!vkbPhysicalDeviceResult.has_value()) {
            throw std::runtime_error("Failed to select physical device: " + vkbPhysicalDeviceResult.error().message());
        }
        physicalDevice = vkbPhysicalDeviceResult.value().physical_device;

        // 4. Создание Logical Device
        auto vkbDeviceResult = vkb::DeviceBuilder{vkbPhysicalDeviceResult.value()}.
            set_allocation_callbacks(nullptr).
            build();

        if (!vkbDeviceResult.has_value()) {
            throw std::runtime_error("Failed to create logical device: " + vkbDeviceResult.error().message());
        }

        vk::Device rawDevice = vkbDeviceResult.value().device;
        VULKAN_HPP_DEFAULT_DISPATCHER.init(rawDevice);
        uniqueDevice = vk::UniqueDevice{
            rawDevice,
            vk::UniqueHandleTraits<vk::Device, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter{ nullptr, VULKAN_HPP_DEFAULT_DISPATCHER }
        };

        // Получение очередей
        auto graphicsQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::graphics);
        auto computeQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::compute);
        auto transferQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::transfer);
        auto presentationQueueResult = vkbDeviceResult.value().get_queue_and_index(vkb::QueueType::present);

        graphicsQueue = graphicsQueueResult.value().first;
        presentationQueue = presentationQueueResult.value().first;
        transferQueue = transferQueueResult.value().first;

        graphicsQueueFamilyIndex = graphicsQueueResult.value().second;
        transferQueueFamilyIndex = transferQueueResult.value().second;
        presentationQueueFamilyIndex = presentationQueueResult.value().second;

        // 5. Инициализация Аллокатора VMA (Emplace в std::optional!)
        auto [createAllocatorResult, rawUniqueAllocator] = resources::UniqueAllocator::makeUnique(
            *uniqueInstance,
            *uniqueDevice,
            physicalDevice,
            vk::detail::defaultDispatchLoaderDynamic
        );
        if (createAllocatorResult != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create VMA allocator");
        }
        uniqueAllocator.emplace(std::move(rawUniqueAllocator));

        // Command Pool для трансфера
        auto [createCommandPoolResult, rawUniqueTransferCommandPool] = uniqueDevice->createCommandPoolUnique(
            vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = transferQueueFamilyIndex
            }
        );
        if (createCommandPoolResult != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create transfer command pool");
        }
        uniqueTransferCommandPool = std::move(rawUniqueTransferCommandPool);

        // 6. Инициализация рендерера
        std::cout << "[Init] Initializing PbrRender pipeline...\n";
        auto [pbrRes, rawPbrRender] = PbrRender::create(*uniqueDevice, vk::ImageLayout::ePresentSrcKHR);
        if (pbrRes != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create PbrRender system");
        }
        pbrRender.emplace(std::move(rawPbrRender));

        // 7. Загрузка сцены (Ассет-менеджмент)
        std::cout << "[Scene] Loading 3D mesh via Assimp...\n";
        AssimpLoader loader;
        HostSceneData globalScene = loader.loadScene("assets/models/lowe/scene.gltf");
        HostSceneData ufo = loader.loadScene("assets/models/Rigged_UFO_gltf/Rigged_Modular UFO 2.8.glb.gltf");

        TerrainProperties terrainProperties{
            .meshResolution = vk::Extent2D{ .width = 2048, .height = 2048 },
            .worldSize = {1024.0f, 1024.0f},
            .minHeight = -40.0f,
            .maxHeight = 40.0f
        };

        HostMeshData terrainMesh = TerrainGeometryGenerator::createFromHeightMap(terrainProperties, Image1D16bit("assets/terrain/novotroitsk_terrain.png"));
        HostMaterialData terrainMaterial = loadFromFiles(
            "assets/material/whispy-grass-meadow-ue/wispy-grass-meadow_albedo.png",
            "assets/material/whispy-grass-meadow-ue/wispy-grass-meadow_normal-dx.png",
            "assets/material/whispy-grass-meadow-ue/wispy-grass-meadow_roughness.png",
            "assets/material/whispy-grass-meadow-ue/wispy-grass-meadow_ao.png",
            "assets/material/whispy-grass-meadow-ue/wispy-grass-meadow_metallic.png",
            ""
        );

        HostSceneData tankSceneData = loader.loadScene("assets/models/tank/small_lpg_tank_4k.gltf");
        globalScene.merge(tankSceneData, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -25.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(10.0f, 10.0f, 10.0f)));
        globalScene.merge(ufo, glm::translate(glm::mat4(1.0f), glm::vec3(30.0f, 10.0f, 60.0f)));
        globalScene.addTerrain(terrainMesh, terrainMaterial, glm::translate(glm::mat4(1), glm::vec3(0.0f, -50.0f, 0.0f)));

        std::cout << "[Scene] Uploading buffers and textures to GPU Local memory...\n";
        auto [uploadRes, rawDeviceSceneData] = pbrRender->uploadScene(
            std::move(globalScene),
            transferQueue,
            *uniqueDevice,
            *uniqueTransferCommandPool,
            **uniqueAllocator
        );
        if (uploadRes != vk::Result::eSuccess) {
            throw std::runtime_error("CRITICAL: Failed to upload scene data to GPU");
        }
        deviceSceneData = std::move(rawDeviceSceneData);

        // 8. Настройка Свопчейна и Кадров
        std::array descriptorPoolSizes {
            vk::DescriptorPoolSize{ .type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 10 },
            vk::DescriptorPoolSize{ .type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 10 },
            vk::DescriptorPoolSize{ .type = vk::DescriptorType::eSampledImage, .descriptorCount = 10 }
        };

        auto [createFrameDataDescriptorPool, rawFramePool] = uniqueDevice->createDescriptorPoolUnique(
            {
                .maxSets = 10,
                .poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size()),
                .pPoolSizes = descriptorPoolSizes.data()
            }
        );
        frameDataDescriptorPool = std::move(rawFramePool);

        swapchainContext = SwapchainContext{
            .physicalDevice = physicalDevice,
            .device = *uniqueDevice,
            .surface = *uniqueSurface,
            .graphicsQueueFamily = graphicsQueueFamilyIndex,
            .presentQueueFamily = presentationQueueFamilyIndex
        };

        auto [createSwapchainResult, swapchain] = createSwapchain(swapchainContext, window.getExtent());
        if (createSwapchainResult != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create swapchain");
        }

        auto [createRenderTargetsResult, renderTargets] = pbrRender->createRenderTargets(
            *uniqueDevice,
            **uniqueAllocator,
            swapchain.images,
            swapchain.extent
        );
        if (createRenderTargetsResult != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create render targets");
        }

        auto [createFrameManagerResult, frameManager] = FrameManager::create(*uniqueDevice, frameCount, swapchain.imageCount);
        if (createFrameManagerResult != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create frameManager");
        }

        auto [createFrameDatasResult, rawFrameDatas] = pbrRender->createFrameDatas(
            *uniqueDevice,
            **uniqueAllocator,
            { .width = 4096, .height = 4096 },
            *frameDataDescriptorPool,
            frameCount
        );
        if (createFrameDatasResult != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create frame datas");
        }
        frameDatas = std::move(rawFrameDatas);

        activeResources.emplace(SwapchainResources{
            .swapchain = std::move(swapchain),
            .frameManager = std::move(frameManager),
            .renderTargets = std::move(renderTargets)
        });

        // 9. Инициализация ввода и UI
        if (!inputSettings.loadFromManifest("config/input/manifest.cfg")) {
            std::cerr << "[Main] WARNING: Failed to load input configuration! Using fallback defaults.\n";
        }

        activeMask.mask = 0;
        activeMask.enable(EngineModule::CameraMove);
        activeMask.enable(EngineModule::CameraLook);
        activeMask.enable(EngineModule::GameWorld);
        focusState = InputFocusState::Game;

        auto uiRenderResultValue = UiRender::create(
            window,
            instance,
            physicalDevice,
            *uniqueDevice,
            graphicsQueueFamilyIndex,
            graphicsQueue,
            activeResources->swapchain.imageCount,
            pbrRender->getMainRenderPass()
        );
        uiRender.emplace(std::move(uiRenderResultValue.value));

        camera.setWindowSize(activeResources->swapchain.extent.width, activeResources->swapchain.extent.height);
        std::cout << "[Init] Engine initialization completed. Ready to fly.\n";
    }

    void Engine::recreateAllResources() {
        auto [result, newResources] = retireController.updateSwapchainResources(
            swapchainContext,
            window.getExtent(),
            **uniqueAllocator,
            *pbrRender,
            frameCount,
            std::move(*activeResources)
        );

        if (result != vk::Result::eSuccess) {
            throw std::runtime_error("Fatal: Swapchain recreation failed");
        }

        activeResources.emplace(std::move(newResources));
        camera.setWindowSize(activeResources->swapchain.extent.width, activeResources->swapchain.extent.height);
    }

    void Engine::run() {
        auto [createUniqueGraphicsCommandPoolResult, uniqueGraphicsCommandPool] = uniqueDevice->createCommandPoolUnique(
            {
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = graphicsQueueFamilyIndex
            }
        );
        if (createUniqueGraphicsCommandPoolResult != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create graphics command pool");
        }

        auto [allocateGraphicsCommandBufferResult, uniqueGraphicsCommandBuffers] = uniqueDevice->allocateCommandBuffersUnique(
            {
                .commandPool = *uniqueGraphicsCommandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = frameCount
            }
        );
        if (allocateGraphicsCommandBufferResult != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create graphics command buffers");
        }

        auto lastTime = std::chrono::high_resolution_clock::now();
        std::cout << "[Run] Entering main render loop.\n";

        DebugOverlayPainter debugPainter{FPSCounterPainter{},
                    SunLightControlPanel{
                        deviceSceneData.directionalLightDatas[0].direction,
                        deviceSceneData.directionalLightDatas[0].color,
                        deviceSceneData.directionalLightDatas[0].color[3]}};

        while (true) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
            lastTime = currentTime;

            // --- ФАЗА 1: Опрос событий ---
            commandBuffer.clear();
            bool keepRunning = ApplicationController::processEvents(
                sdlLibrary,
                window,
                activeMask,
                focusState,
                inputSettings,
                commandBuffer
            );

            if (!keepRunning) {
                break;
            }

            // --- ФАЗА 2: Выполнение команд ---
            commandBuffer.optimize();
            CommandExecutor::executeQueue(commandBuffer.getCommands(), cameraController, activeMask, focusState);

            // --- ФАЗА 3: Обновление физики ---
            if (activeMask.isEnabled(EngineModule::GameWorld)) {
                cameraController.update(deltaTime);
            }

            // --- ФАЗА 4: Ожидание GPU ---
            auto prepareRes = activeResources->frameManager.prepareFrameSlot(*uniqueDevice, currentFrameIndex);
            if (prepareRes != vk::Result::eSuccess) {
                throw std::runtime_error("Fatal: Failed to prepare frame slot");
            }

            // --- ФАЗА 5: Отрисовка UI ---
            if (activeMask.isEnabled(EngineModule::DebugUI)) {


                uiRender->drawUi(debugPainter, *this);
            }

            retireController.renderRetireUpdate(currentFrameIndex);

            // --- ФАЗА 6: Acquire ---
            auto acquireResult = activeResources->frameManager.acquireNextImage(
                *uniqueDevice,
                *activeResources->swapchain.swapchain,
                currentFrameIndex
            );

            if (acquireResult.result == vk::Result::eSuccess) {
                uint32_t const imageIndex = acquireResult.value;
                retireController.presentRetireUpdate(imageIndex);

                // --- ФАЗА 7: Загрузка матриц на GPU ---
                auto updateRes = PbrRender::updateSceneData(
                    **uniqueAllocator,
                    deviceSceneData,
                    frameDatas[currentFrameIndex],
                    camera.getViewMatrix(),
                    camera.getProjectionMatrix(),
                    camera.getShortProjectionMatrix(),
                    camera.getPosition()
                );
                if (updateRes != vk::Result::eSuccess) {
                    throw std::runtime_error("Failed to update scene data buffer on GPU");
                }

                // --- ФАЗА 8: Запись Vulkan команд ---
                vk::CommandBuffer cmd = uniqueGraphicsCommandBuffers[currentFrameIndex].get();
                cmd.reset();
                cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

                pbrRender->recordRenderFrameCommands(
                    deviceSceneData,
                    cmd,
                    frameDatas[currentFrameIndex],
                    activeResources->renderTargets[imageIndex],
                    [&](vk::CommandBuffer drawCmd) {
                        if (activeMask.isEnabled(EngineModule::DebugUI)){
                            uiRender->recordDrawCommands(drawCmd);
                        }
                    }
                );

                cmd.end();

                // --- ФАЗА 9: Submit & Present ---
                auto submitRes = activeResources->frameManager.submitRenderCommands(
                    graphicsQueue,
                    cmd,
                    currentFrameIndex,
                    imageIndex
                );
                if (submitRes != vk::Result::eSuccess) {
                    throw std::runtime_error("Failed to submit command buffer");
                }

                auto presentResult = activeResources->frameManager.present(
                    presentationQueue,
                    *activeResources->swapchain.swapchain,
                    imageIndex
                );

                if (presentResult == vk::Result::eSuboptimalKHR || presentResult == vk::Result::eErrorOutOfDateKHR) {
                    recreateAllResources();
                } else if (presentResult != vk::Result::eSuccess) {
                    throw std::runtime_error("Fatal: Failed to present");
                }

                currentFrameIndex = (currentFrameIndex + 1) % frameCount;
            }
            else if (acquireResult.result == vk::Result::eSuboptimalKHR || acquireResult.result == vk::Result::eErrorOutOfDateKHR) {
                recreateAllResources();
            }
            else {
                throw std::runtime_error("Fatal: Failed to acquire next image");
            }
        }
        if (auto result = uniqueDevice->waitIdle(); result != vk::Result::eSuccess ) {
            throw std::runtime_error("Fatal: Failed to wait for device idle");
        }
    }

} // namespace shuttle_engine