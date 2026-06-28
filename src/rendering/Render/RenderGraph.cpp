//
// Created by Shagu on 20.06.2026.
//

#include "RenderGraph.hpp"

namespace shuttle_engine::Core {
    // Инициализация глобальных макетов дескрипторов (Set 0, Set 1, Set 2)
    vk::Result RenderGraph::initLayouts() {
        // --- Set 0: Глобальные сэмплеры (Linear + Shadow) ---
        std::array samplerBindings{
            vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
            vk::DescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = vk::DescriptorType::eSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            }
        };

        auto res0 = device.createDescriptorSetLayoutUnique({
            .bindingCount = static_cast<uint32_t>(samplerBindings.size()),
            .pBindings = samplerBindings.data()
        });
        if (res0.result != vk::Result::eSuccess) return res0.result;
        samplerSetLayout = std::move(res0.value);

        // --- Set 1: Данные сцены (Камера, Свет, Матрицы моделей) ---
        std::array sceneBindings{
            // Binding 0: Camera UBO (View/Proj, CameraPos)
            vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
            },

            // Binding 1: Model SSBO (world matrix, normal matrix)
            vk::DescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex
            },

            // Binding 2: Light Info UBO (Метаданные: кол-во источников, Ambient)
            vk::DescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex
            },

            // Binding 3: Light SSBO (Массив DirectionalLightData: цвет, напрвление, матрицы теней)
            vk::DescriptorSetLayoutBinding{
                .binding = 3,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex
            },

            // Binding 4: Shadow Map Image (Только текстура/массив текстур)
            vk::DescriptorSetLayoutBinding{
                .binding = 4,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
        };

        auto res1 = device.createDescriptorSetLayoutUnique({
            .bindingCount = static_cast<uint32_t>(sceneBindings.size()),
            .pBindings = sceneBindings.data()
        });
        if (res1.result != vk::Result::eSuccess) return res1.result;
        sceneDataSetLayout = std::move(res1.value);

        // --- Set 2: Материалы (UBO параметров + 5 текстур) ---
        std::array<vk::DescriptorSetLayoutBinding, 6> matBindings;
        // Material params UBO
        matBindings[0] = vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment
        };
        // 5 текстур (Albedo, Normal, ORM, Emission, Displacement)
        for (uint32_t i = 1; i <= 5; ++i) {
            matBindings[i] = vk::DescriptorSetLayoutBinding{
                .binding = i,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            };
        }

        auto res2 = device.createDescriptorSetLayoutUnique({
            .bindingCount = static_cast<uint32_t>(matBindings.size()),
            .pBindings = matBindings.data()
        });
        if (res2.result != vk::Result::eSuccess) return res2.result;
        materialSetLayout = std::move(res2.value);

        return vk::Result::eSuccess;
    }

    // Создание глобального набора сэмплеров (Set 0)
    vk::Result RenderGraph::initSamplers() {

        auto [createShadowSamplerResult, uniqueShadowSampler] = device.createSamplerUnique(
            vk::SamplerCreateInfo {
                .magFilter = vk::Filter::eLinear,
                .minFilter = vk::Filter::eLinear,
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = vk::SamplerAddressMode::eClampToBorder, // Граница карты теней
                .addressModeV = vk::SamplerAddressMode::eClampToBorder,
                .addressModeW = vk::SamplerAddressMode::eClampToBorder,
                .compareEnable = vk::True,              // ЭТО ВКЛЮЧАЕТ СРАВНЕНИЕ ГЛУБИНЫ
                .compareOp = vk::CompareOp::eLess,    // Если глубина пикселя < глубины в карте -> СВЕТ
                .borderColor = vk::BorderColor::eFloatOpaqueWhite, // За границами - белый (свет)
            }
        );

        if (createShadowSamplerResult != vk::Result::eSuccess) {
            return createShadowSamplerResult;
        }

        auto [createMaterialSamplerResult, uniqueMaterialSampler] = device.createSamplerUnique(
            vk::SamplerCreateInfo {
                .magFilter = vk::Filter::eLinear,
                .minFilter = vk::Filter::eLinear,
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = vk::SamplerAddressMode::eRepeat, // Повторяем текстуру
                .addressModeV = vk::SamplerAddressMode::eRepeat,
                .addressModeW = vk::SamplerAddressMode::eRepeat,
                .anisotropyEnable = vk::True,                     // Включаем анизотропию
                .maxAnisotropy = 16.0f,                         // Максимальное качество
                .compareEnable = vk::False,                      // Сравнение не нужно
                .minLod = 0.0f,
                .maxLod = vk::LodClampNone                     // Используем все мип-уровни
            }
        );
        if (createMaterialSamplerResult != vk::Result::eSuccess) {
            return createMaterialSamplerResult;
        }

        shadowSampler = std::move(uniqueShadowSampler);
        materialSampler = std::move(uniqueMaterialSampler);

        // 1. Создаем пул под дескриптор сэмплеров
        std::array<vk::DescriptorPoolSize, 1> poolSizes{
            vk::DescriptorPoolSize{ .type = vk::DescriptorType::eSampler, .descriptorCount = 2 }
        };

        auto resPool = device.createDescriptorPoolUnique({
            .maxSets = 1,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        });
        if (resPool.result != vk::Result::eSuccess) return resPool.result;
        samplerDescriptorPool = std::move(resPool.value);

        // 2. Аллоцируем DescriptorSet
        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = *samplerDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*samplerSetLayout
        };

        auto resAlloc = device.allocateDescriptorSetsUnique(allocInfo);
        if (resAlloc.result != vk::Result::eSuccess) return resAlloc.result;
        samplerSet = std::move(resAlloc.value[0]);

        // 3. Записываем сэмплеры в дескриптор
        std::array samplerInfos{
            vk::DescriptorImageInfo{ .sampler = *materialSampler },
            vk::DescriptorImageInfo{ .sampler = *shadowSampler }
        };

        std::array writes{
            vk::WriteDescriptorSet{
                .dstSet = *samplerSet,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eSampler,
                .pImageInfo = &samplerInfos[0]
            },
            vk::WriteDescriptorSet{
                .dstSet = *samplerSet,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eSampler,
                .pImageInfo = &samplerInfos[1]
            }
        };

        device.updateDescriptorSets(writes, {});
        return vk::Result::eSuccess;
    }

    // Сборка графа: создание RenderPass-ов, пайплайнов и привязка ресурсов
    vk::Result RenderGraph::compile(const std::vector<std::vector<vk::ImageView>>& attachmentsPerPass, vk::Extent2D extent) {
        if (attachmentsPerPass.size() != factories.size()) {
            return vk::Result::eErrorInitializationFailed;
        }

        registeredPasses.clear();
        registeredPasses.reserve(factories.size());

        for (size_t i = 0; i < factories.size(); ++i) {
            auto& [name, factory] = factories[i];

            // 1. Извлекаем требования к проходу
            RenderPassInfo info = factory->getRenderPassInfo();

            // 2. Создаем системный VkRenderPass
            auto rpRes = device.createRenderPassUnique(info.renderPassCreateInfo);
            if (rpRes.result != vk::Result::eSuccess) return rpRes.result;
            vk::UniqueRenderPass vkRp = std::move(rpRes.value);

            // 3. Фабрика создает "исполнителя" прохода
            auto createRes = factory->createRenderPass(
                device,
                *vkRp,
                0, // subpass
                *samplerSetLayout,
                *sceneDataSetLayout,
                *materialSetLayout
            );
            if (createRes.result != vk::Result::eSuccess) return createRes.result;
            std::unique_ptr<IRenderPass> pass = std::move(createRes.value);

            // 4. Создаем Framebuffer для каждого кадра свопчейна
            const auto& attachmentsVec = attachmentsPerPass[i];
            std::vector<vk::UniqueFramebuffer> fbs;
            fbs.reserve(attachmentsVec.size());

            for (const auto& imgView : attachmentsVec) {
                vk::FramebufferCreateInfo fbci{
                    .renderPass = *vkRp,
                    .attachmentCount = 1, // Для MVP предполагаем 1 аттачмент (Shadow или Color)
                    .pAttachments = &imgView,
                    .width = extent.width,
                    .height = extent.height,
                    .layers = 1
                };
                auto fbRes = device.createFramebufferUnique(fbci);
                if (fbRes.result != vk::Result::eSuccess) return fbRes.result;
                fbs.push_back(std::move(fbRes.value));
            }

            // 5. Сохраняем готовую запись
            registeredPasses.push_back({
                .name = name,
                .pass = std::move(pass),
                .vkRenderPass = std::move(vkRp),
                .framebuffers = std::move(fbs),
                .clearValues = info.clearValues,
                .extent = extent
            });
        }
        return vk::Result::eSuccess;
    }

    // Запись команд кадра
    void RenderGraph::recordFrame(
        vk::CommandBuffer cmd,
        DeviceSceneData const& sceneData,
        FrameData const& frameData,
        uint32_t imageIndex
    ) {
        for (auto& entry : registeredPasses) {
            if (imageIndex >= entry.framebuffers.size()) continue;

            vk::RenderPassBeginInfo beginInfo{
                .renderPass = *entry.vkRenderPass,
                .framebuffer = *entry.framebuffers[imageIndex],
                .renderArea = vk::Rect2D{ {0, 0}, entry.extent },
                .clearValueCount = static_cast<uint32_t>(entry.clearValues.size()),
                .pClearValues = entry.clearValues.empty() ? nullptr : entry.clearValues.data()
            };

            cmd.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

            // Вызываем проход, прокидывая глобальный набор сэмплеров (Set 0)
            entry.pass->recordCommandBufferCommands(cmd, sceneData, frameData, *samplerSet);

            cmd.endRenderPass();
        }
    }

    void RenderGraph::destroy() {
        registeredPasses.clear();
        factories.clear();
        samplerSet.reset();
        samplerDescriptorPool.reset();
        samplerSetLayout.reset();
        sceneDataSetLayout.reset();
        materialSetLayout.reset();
    }
} // shuttle_engine