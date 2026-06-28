#pragma once
#include "IncludeVulkan.hpp"
#include "../../memory/DeviceAllocator/DeviceAllocator.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <mutex>
#include <cstdint>

namespace shuttle_engine::render {
    struct alignas(16) SceneNode {
        glm::mat4 localTransform{1.0f}; // 64
        int32_t parentIndex = -1; // 4
        uint32_t meshId = 0;      // 4
        uint32_t materialId = 0;  // 4
        uint32_t transformId = 0; // 4
        uint32_t depth = 0xFFFFFFFFu; // 4 - инициализируем как невычисленное
        // 64 + 6*4 = 88 байт -> выровняется до 96, но GPU читает по std430 fine
    };
}

namespace shuttle_engine::assets {

class NodeStore {
public:
    // allocator управляет VMA, device - логич. устройство
    NodeStore(vk::Device device, memory::DeviceAllocator& allocator, uint32_t maxNodes, uint32_t framesInFlight);
    ~NodeStore() = default;

    NodeStore(const NodeStore&) = delete;
    NodeStore& operator=(const NodeStore&) = delete;

    // Добавляем ноду. Если transformId == UINT32_MAX, используем id == index
    uint32_t addNode(const render::SceneNode& node);

    // Обновляем локальную матрицу на CPU
    void updateLocalTransform(uint32_t nodeIndex, const glm::mat4& newLocal);

    // Копируем весь массив nodes_ в host-visible буфер для frameIdx
    void syncToFrame(uint32_t frameIdx);

    // Получить BDA адрес этого host-visible буфера для frameIdx
    vk::DeviceAddress getBufferAddress(uint32_t frameIdx) const noexcept;

    uint32_t getNodeCount() const noexcept { return static_cast<uint32_t>(nodes_.size()); }
    uint32_t getMaxNodes() const noexcept { return maxNodes_; }

private:
    vk::Device device_;
    memory::DeviceAllocator& allocator_;
    uint32_t maxNodes_;
    uint32_t framesInFlight_;

    std::vector<render::SceneNode> nodes_;
    mutable std::mutex nodesMutex_;

    struct FrameResources {
        memory::UniqueAllocatedBuffer buffer; // host-visible CPU->GPU
        uint8_t* mappedPtr = nullptr;
        vk::DeviceAddress bda = 0;
    };
    std::vector<FrameResources> frames_;
};

} // namespace shuttle_engine::assets
