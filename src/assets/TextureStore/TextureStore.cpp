#include "TextureStore.hpp"
#include "../HostImage/HostImageLoader.hpp" // Для создания HostImage для заглушек
#include "../TextureLoader/TextureLoader.hpp"   // Для загрузки HostImage заглушек на GPU
#include "../../memory/DeviceAllocator/DeviceAllocator.hpp"
#include "../../memory/StagingBufferController/StagingBufferController.hpp"
#include <iostream>
#include <glm/glm.hpp>

namespace shuttle_engine::assets {

    // Вспомогательные функции для создания 1x1 HostImage (для дефолтных текстур)
    static HostImage makeDefaultImage(glm::vec4 color, vk::Format format = vk::Format::eR8G8B8A8Srgb) {
        uint32_t width = 1;
        uint32_t height = 1;
        size_t size = 4; // RGBA

        uint8_t* rawData = new (std::nothrow) uint8_t[size];
        if (!rawData) throw std::runtime_error("Failed to allocate memory for default image!");

        rawData[0] = static_cast<uint8_t>(color.r * 255);
        rawData[1] = static_cast<uint8_t>(color.g * 255);
        rawData[2] = static_cast<uint8_t>(color.b * 255);
        rawData[3] = static_cast<uint8_t>(color.a * 255);

        return HostImage(
            rawData,
            width, height, 4, size,
            format,
            [](uint8_t* p) { delete[] p; }
        );
    }

    TextureStore::TextureStore(vk::Device device, uint32_t maxTextures, uint32_t setIndex)
        : device_(device), maxTextures_(maxTextures), setIndex_(setIndex) 
    {
        textures_.resize(maxTextures);
        createVulkanObjects();
    }

    TextureStore::~TextureStore() {
        if (descriptorSetLayout_) device_.destroyDescriptorSetLayout(descriptorSetLayout_);
        if (descriptorPool_) device_.destroyDescriptorPool(descriptorPool_);
        
        // Освобождаем все VkImage и VkImageView, которыми мы владеем
        for (auto& optionalTex : textures_) {
            if (optionalTex.has_value()) {
                if (optionalTex->imageView) device_.destroyImageView(optionalTex->imageView);
                if (optionalTex->image) device_.destroyImage(optionalTex->image);
                // Если ты используешь VMA, то здесь VmaAllocator.destroyImage(image, allocation)
            }
        }
    }

    void TextureStore::createVulkanObjects() {
        // --- 1. DescriptorSetLayout ---
        vk::DescriptorSetLayoutBinding textureBinding{
            .binding = 0, // Биндинг 0 в Set 1 для массива текстур
            .descriptorType = vk::DescriptorType::eSampledImage,
            .descriptorCount = maxTextures_,
            .stageFlags = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute,
            .pImmutableSamplers = nullptr
        };

        vk::DescriptorBindingFlagsEXT bindingFlags = 
            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound | 
            vk::DescriptorBindingFlagBitsEXT::eUpdateAfterBind;
        
        vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT layoutBindingFlags{
            .bindingCount = 1,
            .pBindingFlags = &bindingFlags
        };

        vk::DescriptorSetLayoutCreateInfo layoutInfo{
            .pNext = &layoutBindingFlags,
            .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, // Для обновления на лету
            .bindingCount = 1,
            .pBindings = &textureBinding
        };
        descriptorSetLayout_ = device_.createDescriptorSetLayout(layoutInfo).value;

        // --- 2. DescriptorPool ---
        vk::DescriptorPoolSize poolSize{
            .type = vk::DescriptorType::eSampledImage,
            .descriptorCount = maxTextures_ // Pool должен вмещать все дескрипторы для текстур
        };
        vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
            .maxSets = 1, // Нам нужен только один глобальный сет для текстур
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
        };
        descriptorPool_ = device_.createDescriptorPool(poolInfo).value;

        // --- 3. DescriptorSet ---
        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = descriptorPool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetLayout_
        };
        descriptorSet_ = device_.allocateDescriptorSets(allocInfo).value[0];
    }

    void TextureStore::createDefaultTextures(TextureLoader& textureLoader, 
                                           memory::DeviceAllocator& allocator, 
                                           memory::StagingBufferController& stagingController) 
    {
        // Для загрузки дефолтных текстур потребуется CommandBuffer и Fence,
        // так как TextureLoader.prepareUpload() и .recordUploadCommands() работают с GPU.
        // Этот код должен быть в твоем AssetManager или другом месте,
        // которое управляет CommandBuffers и сабмитами!
        
        // Пример (псевдокод), как это будет выглядеть:
        // vk::CommandBuffer cmd = someAssetManager.beginSingleTimeCommands();
        // vk::Fence fence = someAssetManager.createFence();

        // HostImage albedoDef = makeDefaultImage(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), vk::Format::eR8G8B8A8Srgb); // Белый
        // HostImage normalDef = makeDefaultImage(glm::vec4(0.5f, 0.5f, 1.0f, 1.0f), vk::Format::eR8G8B8A8Unorm); // Вектор (0,0,1)
        // HostImage ormDef    = makeDefaultImage(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), vk::Format::eR8G8B8A8Unorm); // AO=1, Rough=1, Metal=0

        // TextureUploadTx albedoTx = textureLoader.prepareUpload(allocator, stagingController, albedoDef);
        // TextureUploadTx normalTx = textureLoader.prepareUpload(allocator, stagingController, normalDef);
        // TextureUploadTx ormTx    = textureLoader.prepareUpload(allocator, stagingController, ormDef);

        // textureLoader.recordUploadCommands(cmd, albedoTx, TextureType::Albedo);
        // textureLoader.recordUploadCommands(cmd, normalTx, TextureType::Normal);
        // textureLoader.recordUploadCommands(cmd, ormTx, TextureType::ORM);

        // someAssetManager.endSingleTimeCommands(cmd, queue, fence); // Сабмит и ожидание

        // defaultAlbedoId_ = registerTexture(albedoTx.finalImage.release()); // Передаем владение
        // defaultNormalId_ = registerTexture(normalTx.finalImage.release());
        // defaultOrmId_    = registerTexture(ormTx.finalImage.release());

        // HACK: Временная заглушка без реальной загрузки, просто для компиляции!
        render::Texture dummyTex;
        dummyTex.width = 1; dummyTex.height = 1; dummyTex.mipLevels = 1;
        dummyTex.format = vk::Format::eR8G8B8A8Srgb; // Placeholder
        dummyTex.image = nullptr; dummyTex.imageView = nullptr;

        defaultAlbedoId_  = registerTexture(std::move(dummyTex), render::TextureType::Generic);
        defaultNormalId_  = registerTexture(std::move(dummyTex), render::TextureType::Generic);
        defaultOrmId_     = registerTexture(std::move(dummyTex), render::TextureType::Generic);

        // Конец HACK
    }

    uint32_t TextureStore::registerTexture(render::Texture&& texture, render::TextureType type) {
        if (!texture.image || !texture.imageView) {
            std::cerr << "[TextureStore] Attempted to register empty GPU texture. Using default ORM ID." << std::endl;
            return defaultOrmId_; // Возвращаем ID дефолтной, если передали пустую
        }

        uint32_t id = nextFreeId_;
        if (id >= maxTextures_) {
            std::cerr << "[TextureStore] Texture heap is full! Cannot register new texture. Using default ORM ID." << std::endl;
            return defaultOrmId_; // Если куча полна, возвращаем дефолтную
        }

        textures_[id] = std::move(texture); // Передаем владение объектом render::Texture
        updateDescriptorSet(id, textures_[id]->imageView);

        nextFreeId_++; // Двигаем указатель на следующий свободный слот
        return id;
    }

    void TextureStore::updateDescriptorSet(uint32_t textureId, vk::ImageView view, vk::DescriptorType type) {
        if (textureId >= maxTextures_) {
            throw std::out_of_range("[TextureStore] Texture ID out of bounds for descriptor update!");
        }

        vk::DescriptorImageInfo imageInfo{
            .sampler = nullptr, // Сэмплеры у нас в Set 0
            .imageView = view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal // Финальный лейаут текстуры
        };

        vk::WriteDescriptorSet write{
            .dstSet = descriptorSet_,
            .dstBinding = 0, // Биндинг 0 в этом сете для массива текстур
            .dstArrayElement = textureId, // Индекс в массиве
            .descriptorCount = 1,
            .descriptorType = type,
            .pImageInfo = &imageInfo
        };

        device_.updateDescriptorSets(write, {});
    }

} // namespace shuttle_engine::assets