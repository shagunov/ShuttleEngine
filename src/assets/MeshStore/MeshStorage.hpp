#pragma once
#include "IncludeVulkan.hpp"
#include "MeshInfo.hpp"
#include "../../memory/DeviceAllocator/DeviceAllocator.hpp"
#include "../../memory/StagingBufferController/StagingBufferController.hpp"
#include <vector>

namespace shuttle_engine::assets {

    // Структура, которую мы заполняем при чтении из файла (Assimp)
    struct HostMeshData {
        std::vector<glm::vec3> positions;
        std::vector<render::VertexAttributes> attributes;
        std::vector<uint32_t> indices;
        uint32_t materialId = 0;
    };

    class MeshStorage {
    public:
        MeshStorage(vk::Device device, memory::DeviceAllocator& allocator);

        // Добавляем меш в список "на загрузку"
        uint32_t addMesh(HostMeshData&& hostMesh);

        // Превращаем всё накопленное в GPU-шные Mega-Buffers
        // Возвращает BDA-адрес буфера MeshInfo для Push Constants
        vk::DeviceAddress buildAndUpload(vk::CommandBuffer cmd, memory::StagingBufferController& staging);

        [[nodiscard]] vk::DeviceAddress getMeshInfoBufferAddress() const noexcept { return meshInfoBufferBDA_; }

    private:
        vk::Device device_;
        memory::DeviceAllocator& allocator_;

        std::vector<HostMeshData> pendingMeshes_;
        std::vector<render::MeshInfo> meshInfos_;

        // Mega-буферы (Device Local)
        memory::UniqueAllocatedBuffer positionMegaBuffer_;
        memory::UniqueAllocatedBuffer attributeMegaBuffer_;
        memory::UniqueAllocatedBuffer indexMegaBuffer_;
        memory::UniqueAllocatedBuffer meshInfoBuffer_;

        uint64_t meshInfoBufferBDA_ = 0;
    };
}
