#include "TransformStore.hpp"
#include <stdexcept>
#include <vector>
#include <fstream>

using namespace shuttle_engine::assets;

static std::vector<char> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("readFile failed");
    auto size = (size_t)f.tellg();
    std::vector<char> buf(size);
    f.seekg(0);
    f.read(buf.data(), static_cast<std::streamsize>(size));
    return buf;
}

TransformStore::TransformStore(vk::Device device, memory::DeviceAllocator& allocator, uint32_t maxNodes, uint32_t framesInFlight)
    : device_(device), allocator_(allocator), maxNodes_(maxNodes), framesInFlight_(framesInFlight)
{
    if (!maxNodes_ || !framesInFlight_) throw std::runtime_error("TransformStore invalid params");

    frames_.resize(framesInFlight_);
    size_t bufSize = static_cast<size_t>(maxNodes_) * sizeof(glm::mat4);

    for (uint32_t i = 0; i < framesInFlight_; ++i) {
        auto [res, buf] = allocator_.createAndAllocateBufferUnique(
            {
                .size = bufSize,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
                .sharingMode = vk::SharingMode::eExclusive
            },
            memory::MemoryUsage::eGpuOnly
        );
        if (res != vk::Result::eSuccess) throw std::runtime_error("TransformStore allocate world buffer failed");
        frames_[i].worldBuffer = std::move(buf);
        frames_[i].bda = device.getBufferAddress({.buffer = *frames_[i].worldBuffer});
    }

    // counter buffer: host-visible uint32_t
    auto [cres, cbuf] = allocator_.createAndAllocateBufferUnique(
        {
            .size = sizeof(uint32_t),
            .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive
        },
        memory::MemoryUsage::eCpuToGpu,
        memory::AllocationCreateFlags(static_cast<uint32_t>(memory::AllocationCreateFlagBits::eMapped) |
                                      static_cast<uint32_t>(memory::AllocationCreateFlagBits::eHostAccessSequentialWrite))
    );
    if (cres != vk::Result::eSuccess) throw std::runtime_error("TransformStore counter alloc failed");
    counterBuffer_ = std::move(cbuf);
    counterMappedPtr_ = static_cast<uint8_t*>(allocator_.getMappedPointer(*counterBuffer_));
    counterBda_ = device.getBufferAddress({.buffer = *counterBuffer_});

    createComputePipeline();
}

TransformStore::~TransformStore() = default;

void TransformStore::createComputePipeline() {
    // Здесь создаём pipelineLayout_ с PushConstants:
    // struct Push { vk::DeviceAddress nodeBufferAddr; vk::DeviceAddress worldBufferAddr; uint32_t nodeCount; uint32_t currentDepth; uint32_t mode; };
    // Загружаем один shader node_flattening.comp.spv который поддерживает modes: 0=propagateDepth, 1=flattenDepth
    auto shaderCode = readFile("shaders/node_flattening.comp.spv");
    vk::ShaderModuleCreateInfo smci{ .codeSize = shaderCode.size(), .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data()) };
    auto sm = device_.createShaderModuleUnique(smci).value;

    vk::PushConstantRange pc{ vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint64_t)*2 + sizeof(uint32_t)*2 + sizeof(uint32_t) }; // nodeAddr, worldAddr, nodeCount, currentDepth, mode
    vk::PipelineLayoutCreateInfo plci{ .pushConstantRangeCount = 1, .pPushConstantRanges = &pc };
    pipelineLayout_ = device_.createPipelineLayoutUnique(plci).value;

    vk::PipelineShaderStageCreateInfo stage{ .stage = vk::ShaderStageFlagBits::eCompute, .module = *sm, .pName = "main" };
    vk::ComputePipelineCreateInfo cpci{ .stage = stage, .layout = *pipelineLayout_ };
    computePipeline_ = device_.createComputePipelineUnique(nullptr, cpci).value;
}

vk::DeviceAddress TransformStore::getWorldMatricesAddress(uint32_t frameIdx) const noexcept {
    if (frameIdx >= framesInFlight_) return 0;
    return frames_[frameIdx].bda;
}

void TransformStore::recordFlatteningCommands(
    vk::CommandBuffer cmd,
    uint32_t frameIdx,
    vk::DeviceAddress nodeBufferAddr,
    uint32_t nodeCount,
    uint32_t maxDepthHint
) {
    if (frameIdx >= framesInFlight_ || nodeCount == 0) return;

    // --- Phase A: Depth Propagation (iterative) ---
    // We'll loop up to maxDepthHint times. Before each dispatch, set counter=0
    uint32_t workGroups = (nodeCount + 63) / 64;

    for (uint32_t iter = 0; iter < maxDepthHint; ++iter) {
        // reset counter to 0 (host visible mapped pointer)
        // If mapped/coherent, we can write directly; otherwise use fillBuffer + memory barrier.
        if (counterMappedPtr_) {
            uint32_t zero = 0;
            std::memcpy(counterMappedPtr_, &zero, sizeof(uint32_t));
            // If non-coherent, flush using allocator API (not shown)
        }

        // push constants: node addr, world addr (we use same world buffer for writes), nodeCount, currentDepth=iter, mode=0 (propagate)
        struct PC { vk::DeviceAddress nodeAddr; vk::DeviceAddress worldAddr; uint32_t nodeCount; uint32_t depthOrMode; uint32_t mode; };
        PC pc;
        pc.nodeAddr = nodeBufferAddr;
        pc.worldAddr = frames_[frameIdx].bda;
        pc.nodeCount = nodeCount;
        pc.depthOrMode = iter; // not used in propagation, but kept
        pc.mode = 0; // propagate mode

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline_);
        cmd.pushConstants(*pipelineLayout_, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PC), &pc);
        cmd.dispatch(workGroups, 1, 1);

        // barrier: ensure shader writes to node.depth are visible to next iteration
        vk::MemoryBarrier mb{ .srcAccessMask =  vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, mb, nullptr, nullptr);

        // Optionally read counter back to CPU to early-exit loop.
        // For MVP we can run full maxDepthHint iterations to keep logic simple and avoid readback.
    }

    // --- Phase B: Flatten by depth levels ---
    // For depth = 0 .. maxDepthHint-1, dispatch a pass computing world matrices for nodes with node.depth == currentDepth
    for (uint32_t d = 0; d < maxDepthHint; ++d) {
        struct PC2 { vk::DeviceAddress nodeAddr; vk::DeviceAddress worldAddr; uint32_t nodeCount; uint32_t currentDepth; uint32_t mode; };
        PC2 pc2{};
        pc2.nodeAddr = nodeBufferAddr;
        pc2.worldAddr = frames_[frameIdx].bda;
        pc2.nodeCount = nodeCount;
        pc2.currentDepth = d;
        pc2.mode = 1; // flatten mode

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline_);
        cmd.pushConstants(*pipelineLayout_, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PC2), &pc2);
        cmd.dispatch(workGroups, 1, 1);

        // barrier to ensure writes visible for next depth
        vk::MemoryBarrier mb{ .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, mb, nullptr, nullptr);
    }

    // final barrier: make world buffer readable by vertex shader
    vk::BufferMemoryBarrier finalBarrier{
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = *frames_[frameIdx].worldBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader,
                        {}, {}, {finalBarrier}, {});
}