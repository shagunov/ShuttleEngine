#include "GeometryManager.hpp"
#include <glm/glm.hpp>

#include "PbrRender/Render.hpp"

namespace shuttle_engine::Core {

    // Вспомогательная структура для расчета смещений
    struct MeshAllocation {
        uint32_t indexCount;
        uint32_t firstIndex;
        int32_t vertexOffset;
    };

    GeometryManager::GeometryManager(vk::Device device, memory::DeviceAllocator allocator)
        : device(device), allocator(allocator) {}

    DeviceMeshData GeometryManager::loadSceneGeometry(
        vk::CommandBuffer cmd,
        memory::StagingBufferController& staging,
        const HostSceneData& hostScene,
        vk::Fence uploadFence
    ) {
        // 1. ПРЕДВАРИТЕЛЬНЫЙ РАСЧЕТ РАЗМЕРОВ
        // Собираем все данные из всех мешей в плоские векторы (на время упаковки)
        std::vector<PositionAttribute> allPositions;
        std::vector<NormalTangentUvAttribute> allAttribs;
        std::vector<uint32_t> allIndices;
        std::vector<vk::DrawIndexedIndirectCommand> indirectCommands;
        std::vector<ModelData> allModelMatrices;

        // ... здесь логика обхода дерева нод (как в твоем старом PbrRender) ...
        // Но теперь мы пишем всё в один блок памяти!

        vk::DeviceSize posSize = allPositions.size() * sizeof(PositionAttribute);
        vk::DeviceSize attrSize = allAttribs.size() * sizeof(NormalTangentUvAttribute);
        vk::DeviceSize idxSize = allIndices.size() * sizeof(uint32_t);
        vk::DeviceSize indSize = indirectCommands.size() * sizeof(vk::DrawIndexedIndirectCommand);
        vk::DeviceSize matSize = allModelMatrices.size() * sizeof(ModelData);

        vk::DeviceSize totalStagingNeeded = posSize + attrSize + idxSize + indSize + matSize;

        // 2. ЗАПРОС ПАМЯТИ У STAGING
        auto stagingAlloc = staging.requestSpace(totalStagingNeeded, uploadFence);
        auto* ptr = static_cast<uint8_t*>(stagingAlloc.mappedPointer);
        vk::DeviceSize baseOffset = stagingAlloc.offset;

        // 3. КОПИРОВАНИЕ (MEMCPY) - Это работает мгновенно
        std::memcpy(ptr, allPositions.data(), posSize);
        std::memcpy(ptr + posSize, allAttribs.data(), attrSize);
        std::memcpy(ptr + posSize + attrSize, allIndices.data(), idxSize);
        std::memcpy(ptr + posSize + attrSize + idxSize, indirectCommands.data(), indSize);
        std::memcpy(ptr + posSize + attrSize + idxSize + indSize, allModelMatrices.data(), matSize);

        // 4. СОЗДАНИЕ GPU БУФЕРОВ
        DeviceMeshData result;

        // Вершинный буфер (Position + Attribs)
        auto [rv, vb] = allocator.createAndAllocateBufferUnique({
            .size = posSize + attrSize,
            .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
        }, memory::MemoryUsage::eGpuOnly);
        result.vertexBuffer = std::move(vb);
        result.positionOffset = 0;
        result.attributeOffset = posSize;

        // Индексный буфер
        auto [ri, ib] = allocator.createAndAllocateBufferUnique({
            .size = idxSize,
            .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst
        }, memory::MemoryUsage::eGpuOnly);
        result.indexBuffer = std::move(ib);

        // Буфер команд отрисовки
        auto [rnd, nbuf] = allocator.createAndAllocateBufferUnique({
            .size = indSize,
            .usage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst
        }, memory::MemoryUsage::eGpuOnly);
        result.indirectBuffer = std::move(nbuf);
        result.drawCount = static_cast<uint32_t>(indirectCommands.size());

        // SSBO для матриц
        auto [rm, mbuf] = allocator.createAndAllocateBufferUnique({
            .size = matSize,
            .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
        }, memory::MemoryUsage::eGpuOnly);
        result.modelSsbo = std::move(mbuf);

        // 5. ОДНА КОМАНДА КОПИРОВАНИЯ НА GPU
        // Копируем всё из Staging в соответствующие Device буферы
        cmd.copyBuffer(staging.getBuffer(), *result.vertexBuffer, {vk::BufferCopy{baseOffset, 0, posSize + attrSize}});
        cmd.copyBuffer(staging.getBuffer(), *result.indexBuffer, {vk::BufferCopy{baseOffset + posSize + attrSize, 0, idxSize}});
        cmd.copyBuffer(staging.getBuffer(), *result.indirectBuffer, {vk::BufferCopy{baseOffset + posSize + attrSize + idxSize, 0, indSize}});
        cmd.copyBuffer(staging.getBuffer(), *result.modelSsbo, {vk::BufferCopy{baseOffset + posSize + attrSize + idxSize + indSize, 0, matSize}});

        return result;
    }

} // namespace shuttle_engine::Core