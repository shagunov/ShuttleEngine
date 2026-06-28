#include "AssetManager.hpp"

#include "assets/AssimpLoader/AssimpLoader.hpp"
#include "HostRenderData/HostRenderData.hpp"
#include "memory/DeviceAllocator/DeviceAllocator.hpp"

namespace shuttle_engine::Core {

    AssetManager::AssetManager(vk::Device device, memory::DeviceAllocator& allocator)
        : device(device), geometryStore(device, allocator),
          materialStore(device), textureStore(device)
    {
        // Инициализируй DescriptorPool и Layout здесь,
        // так как они нужны для создания DescriptorSet'ов материалов
    }

    void AssetManager::loadFullScene(const std::string& scenePath, vk::CommandBuffer cmd, memory::StagingBufferController& staging, vk::Fence uploadFence) {
        AssimpLoader loader;
        HostSceneData hostScene = loader.loadScene(scenePath);

        // 1. Загрузка Геометрии (через наш новый GeometryStore)
        for (const auto& mesh : hostScene.meshes) {
            geometryStore.addMesh(cmd, staging, mesh, uploadFence);
        }

        // 2. Загрузка Материалов
        for (const auto& hostMat : hostScene.materials) {
            DeviceMaterialInfo devMat;

            // Загружаем текстуры (TextureStore вернет TextureID, мы получим view)
            auto getTexView = [&](const std::optional<HostImageData>& data) -> vk::ImageView {
                if (!data) return defaultTextureView;
                TextureID tid = textureStore.getOrLoadTexture(data.value(), cmd, staging, uploadFence);
                return textureStore.get(tid).imageView.get();
            };

            devMat.textures = {
                getTexView(hostMat.albedoTexture),
                getTexView(hostMat.normalTexture),
                getTexView(hostMat.ormTexture),
                getTexView(hostMat.emissiveTexture),
                getTexView(hostMat.heightTexture)
            };

            // Создаем UBO и DescriptorSet (как мы обсуждали выше)
            // ... (код заполнения UBO и UpdateDescriptorSets) ...

            materialStore.addMaterial(std::move(devMat));
        }

        // 3. После этого в RenderGraph ты получишь SceneContext
        // с готовыми GeometryStore и MaterialStore.
    }
}
