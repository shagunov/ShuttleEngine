#pragma once
#include "IncludeVulkan.hpp"
#include <vector>
#include "HostRenderData/HostRenderData.hpp"
#include "memory/DeviceAllocator/DeviceAllocator.hpp"
#include "../../memory/StagingBufferController/StagingBufferController.hpp"

namespace shuttle_engine::Core {

    // То, что реально лежит в видеопамяти
    struct DeviceMeshData {
        memory::UniqueAllocatedBuffer vertexBuffer;
        vk::DeviceSize positionOffset;
        vk::DeviceSize attributeOffset; // Normal, UV, Tangent

        memory::UniqueAllocatedBuffer indexBuffer;

        memory::UniqueAllocatedBuffer indirectBuffer; // Команды для glMultiDrawIndirect
        uint32_t drawCount;

        memory::UniqueAllocatedBuffer modelSsbo; // Матрицы всех объектов
    };

    class GeometryManager {
    public:
        GeometryManager(vk::Device device, memory::DeviceAllocator allocator);

        // Принимает данные от Assimp и пакует их для GPU
        DeviceMeshData loadSceneGeometry(
            vk::CommandBuffer cmd,
            memory::StagingBufferController& staging,
            const HostSceneData& hostScene, // Твои данные из Assimp
            vk::Fence uploadFence
        );

    private:
        vk::Device device;
        memory::DeviceAllocator allocator;
    };

} // namespace shuttle_engine::Core
