#pragma once
#include "IncludeVulkan.hpp"
#include "../GeometryStore/GeometryStore.hpp"
#include "../TextureStore/TextureStore.hpp"
#include "../MaterialStore/MaterialStore.hpp"
#include "../AssimpLoader/AssimpLoader.hpp"
#include "memory/DeviceAllocator/DeviceAllocator.hpp"
#include "memory/StagingBufferController/StagingBufferController.hpp"

namespace shuttle_engine::Core {

    class AssetManager {
    public:
        AssetManager(vk::Device device, memory::DeviceAllocator& allocator);

        // Основная точка входа для загрузки Bistro
        void loadFullScene(const std::string& scenePath,
                           vk::CommandBuffer cmd,
                           memory::StagingBufferController& staging,
                           vk::Fence uploadFence);

        GeometryStore& getGeometry() { return geometryStore; }
        MaterialStore& getMaterials() { return materialStore; }
        TextureStore& getTextures() { return textureStore; }

    private:
        vk::Device device;
        GeometryStore geometryStore;
        MaterialStore materialStore;
        TextureStore textureStore;

        // Для создания дескрипторов
        vk::DescriptorPool descriptorPool;
        vk::DescriptorSetLayout materialLayout;
    };
}
