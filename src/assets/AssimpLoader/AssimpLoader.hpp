//
// Created by Shagu on 25.05.2026.
//

#ifndef HELLOTRIANGLE_ASSIMPLOADER_HPP
#define HELLOTRIANGLE_ASSIMPLOADER_HPP
#include <assimp/Importer.hpp>

#include "assets/TextureLoader/TextureLoader.hpp"
#include "HostRenderData/HostRenderData.hpp"


namespace shuttle_engine {
    class AssimpLoader {
    public:
        HostSceneData loadScene(
            const std::string &filename,
            vk::Device device,
            memory::DeviceAllocator& allocator,
            memory::StagingBufferController const& stagingBufferController,
            vk::Queue transferQueue,
            vk::Queue computeQueue,
            vk::CommandPool transferCommandPool,
            vk::CommandPool computeCommandPool,
            assets::TextureLoader const& textureLoader);
    private:
        Assimp::Importer importer;
    };
} // shuttle_engine

#endif //HELLOTRIANGLE_ASSIMPLOADER_HPP
