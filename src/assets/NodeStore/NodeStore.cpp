#include "NodeStore.hpp"
#include <stdexcept>
#include <cstring>

using namespace shuttle_engine::assets;

NodeStore::NodeStore(vk::Device device, memory::DeviceAllocator& allocator, uint32_t maxNodes, uint32_t framesInFlight)
    : device_(device), allocator_(allocator), maxNodes_(maxNodes), framesInFlight_(framesInFlight)
{
    if (maxNodes_ == 0 || framesInFlight_ == 0) throw std::runtime_error("NodeStore: invalid params");

    nodes_.reserve(maxNodes_);

    frames_.resize(framesInFlight_);
    size_t bufSize = static_cast<size_t>(maxNodes_) * sizeof(render::SceneNode);

    for (uint32_t i = 0; i < framesInFlight_; ++i) {
        auto [res, buf] = allocator_.createAndAllocateBufferUnique(
            {
                .size = bufSize,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                .sharingMode = vk::SharingMode::eExclusive
            },
            memory::MemoryUsage::eCpuToGpu,
            memory::AllocationCreateFlags(static_cast<uint32_t>(memory::AllocationCreateFlagBits::eMapped) |
                                          static_cast<uint32_t>(memory::AllocationCreateFlagBits::eHostAccessSequentialWrite))
        );
        if (res != vk::Result::eSuccess) throw std::runtime_error("NodeStore: allocate frame buffer failed");

        frames_[i].buffer = std::move(buf);
        frames_[i].mappedPtr = static_cast<uint8_t*>(allocator_.getMappedPointer(*frames_[i].buffer));
        frames_[i].bda = device.getBufferAddress({.buffer = *frames_[i].buffer});

        if (!frames_[i].mappedPtr) throw std::runtime_error("NodeStore: mapping failed");
    }
}

uint32_t NodeStore::addNode(const render::SceneNode& node) {
    std::lock_guard lock(nodesMutex_);
    if (nodes_.size() >= maxNodes_) throw std::runtime_error("NodeStore: max nodes exceeded");
    render::SceneNode tmp = node;
    // Если parent == -1 -> depth = 0, иначе mark as unknown
    tmp.depth = (tmp.parentIndex == -1) ? 0u : 0xFFFFFFFFu;
    auto idx = static_cast<uint32_t>(nodes_.size());
    if (tmp.transformId == UINT32_MAX) tmp.transformId = idx;
    nodes_.push_back(tmp);
    return idx;
}

void NodeStore::updateLocalTransform(uint32_t nodeIndex, const glm::mat4& newLocal) {
    std::lock_guard lock(nodesMutex_);
    if (nodeIndex >= nodes_.size()) throw std::out_of_range("updateLocalTransform: invalid index");
    nodes_[nodeIndex].localTransform = newLocal;
    // помечать depth не надо — depth вычисляется на GPU
    // но можно по желанию сбросить depth у потомков на CPU (необязательно)
}

void NodeStore::syncToFrame(uint32_t frameIdx) {
    if (frameIdx >= framesInFlight_) throw std::out_of_range("syncToFrame: invalid frame index");

    std::lock_guard lock(nodesMutex_);
    size_t copyBytes = nodes_.size() * sizeof(render::SceneNode);
    if (copyBytes > 0) {
        std::memcpy(frames_[frameIdx].mappedPtr, nodes_.data(), copyBytes);
    }
    // Zero remainder to avoid staled memory reads by GPU (optional)
    size_t totalBytes = static_cast<size_t>(maxNodes_) * sizeof(render::SceneNode);
    if (copyBytes < totalBytes) std::memset(frames_[frameIdx].mappedPtr + copyBytes, 0, totalBytes - copyBytes);

    // Если память non-coherent — нужно flush (через allocator API). Предполагаем coherent or allocator handles it.
}

vk::DeviceAddress NodeStore::getBufferAddress(uint32_t frameIdx) const noexcept {
    if (frameIdx >= framesInFlight_) return 0;
    return frames_[frameIdx].bda;
}