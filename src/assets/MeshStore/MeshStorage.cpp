#include "MeshStorage.hpp"
#include <iostream>

namespace shuttle_engine::assets {

    MeshStorage::MeshStorage(vk::Device device, memory::DeviceAllocator& allocator)
        : device_(device), allocator_(allocator) {}

    uint32_t MeshStorage::addMesh(HostMeshData&& hostMesh) {
        auto id = static_cast<uint32_t>(pendingMeshes_.size());
        pendingMeshes_.push_back(std::move(hostMesh));
        return id;
    }

    uint64_t MeshStorage::buildAndUpload(vk::CommandBuffer cmd, memory::StagingBufferController& staging) {
        if (pendingMeshes_.empty()) throw std::runtime_error("No meshes to upload!");

        // 1. Считаем размеры
        size_t totalVertices = 0;
        size_t totalIndices = 0;
        for (const auto& m : pendingMeshes_) {
            totalVertices += m.positions.size();
            totalIndices += m.indices.size();
        }

        size_t posSize  = totalVertices * sizeof(glm::vec3);
        size_t attrSize = totalVertices * sizeof(render::VertexAttributes);
        size_t idxSize  = totalIndices * sizeof(uint32_t);
        size_t infoSize = pendingMeshes_.size() * sizeof(render::MeshInfo);

        // 2. Создаем GPU-буферы (DEVICE_LOCAL + ShaderDeviceAddress)
        auto createBuf = [&](size_t size, vk::BufferUsageFlags usage) {
            auto [res, buf] = allocator_.createAndAllocateBufferUnique(
                { .size = size, .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | usage },
                memory::MemoryUsage::eGpuOnly
            );
            return std::move(buf);
        };

        positionMegaBuffer_  = createBuf(posSize,  vk::BufferUsageFlagBits::eStorageBuffer);
        attributeMegaBuffer_ = createBuf(attrSize, vk::BufferUsageFlagBits::eStorageBuffer);
        indexMegaBuffer_     = createBuf(idxSize,  vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
        meshInfoBuffer_      = createBuf(infoSize, vk::BufferUsageFlagBits::eStorageBuffer);

        // Получаем BDA для шейдеров
        auto basePosBDA  = device_.getBufferAddress({.buffer = *positionMegaBuffer_});
        auto baseAttrBDA = device_.getBufferAddress({.buffer = *attributeMegaBuffer_});
        auto baseIdxBDA  = device_.getBufferAddress({.buffer = *indexMegaBuffer_});
        meshInfoBufferBDA_   = device_.getBufferAddress({.buffer = *meshInfoBuffer_});

        // 3. Упаковка в Staging
        auto stagingAlloc = staging.allocate(posSize + attrSize + idxSize + infoSize, 16);
        auto* ptr = static_cast<uint8_t*>(stagingAlloc->mappedPointer);

        meshInfos_.resize(pendingMeshes_.size());
                // Смещения внутри staging-аллокации
        size_t posOffset  = 0;
        size_t attrOffset = posSize;
        size_t idxOffset  = posSize + attrSize;
        size_t infoOffset = posSize + attrSize + idxSize;

        meshInfos_.resize(pendingMeshes_.size());

        uint32_t vOffset = 0; // Смещение в вершинах (для BDA адресации)
        uint32_t iOffset = 0; // Смещение в индексах (для BDA адресации)

        for (size_t i = 0; i < pendingMeshes_.size(); ++i) {
            const auto& m = pendingMeshes_[i];

            // 1. Копируем позиции (glm::vec3)
            size_t pBytes = m.positions.size() * sizeof(glm::vec3);
            std::memcpy(ptr + posOffset + (vOffset * sizeof(glm::vec3)), m.positions.data(), pBytes);

            // 2. Копируем атрибуты (render::VertexAttributes)
            size_t aBytes = m.attributes.size() * sizeof(render::VertexAttributes);
            std::memcpy(ptr + attrOffset + (vOffset * sizeof(render::VertexAttributes)), m.attributes.data(), aBytes);

            // 3. Копируем индексы (uint32_t)
            size_t iBytes = m.indices.size() * sizeof(uint32_t);
            std::memcpy(ptr + idxOffset + (iOffset * sizeof(uint32_t)), m.indices.data(), iBytes);

            // 4. Формируем метаданные для GPU
            meshInfos_[i] = {
                .positionAddress  = basePosBDA + (vOffset * sizeof(glm::vec3)),
                .attributeAddress = baseAttrBDA + (vOffset * sizeof(render::VertexAttributes)),
                .indexAddress     = baseIdxBDA + (iOffset * sizeof(uint32_t)),
                .indexCount       = static_cast<uint32_t>(m.indices.size()),
                .vertexOffset     = vOffset,
                .materialId       = m.materialId,
                .padding          = 0
            };

            // Обновляем глобальные счетчики смещений
            vOffset += static_cast<uint32_t>(m.positions.size());
            iOffset += static_cast<uint32_t>(m.indices.size());
        }

        // 5. Копируем сам массив MeshInfo в конец (infoOffset)
        std::memcpy(ptr + infoOffset, meshInfos_.data(), pendingMeshes_.size() * sizeof(render::MeshInfo));

                // 5. Записываем команды копирования (из staging на GPU)
        // Мы копируем каждый сегмент из staging в соответствующий целевой GPU буфер

        // Позиции
        vk::BufferCopy posCopy{ .srcOffset = stagingAlloc->offset + posOffset, .dstOffset = 0, .size = posSize };
        cmd.copyBuffer(stagingAlloc->buffer, *positionMegaBuffer_, 1, &posCopy);

        // Атрибуты
        vk::BufferCopy attrCopy{ .srcOffset = stagingAlloc->offset + attrOffset, .dstOffset = 0, .size = attrSize };
        cmd.copyBuffer(stagingAlloc->buffer, *attributeMegaBuffer_, 1, &attrCopy);

        // Индексы
        vk::BufferCopy idxCopy{ .srcOffset = stagingAlloc->offset + idxOffset, .dstOffset = 0, .size = idxSize };
        cmd.copyBuffer(stagingAlloc->buffer, *indexMegaBuffer_, 1, &idxCopy);

        // MeshInfo
        vk::BufferCopy infoCopy{ .srcOffset = stagingAlloc->offset + infoOffset, .dstOffset = 0, .size = infoSize };
        cmd.copyBuffer(stagingAlloc->buffer, *meshInfoBuffer_, 1, &infoCopy);

        // 6. Барьер памяти (Transfer -> Shader Read)
        // Чтобы шейдеры не начали читать буферы, пока GPU не закончит копирование
        std::array<vk::BufferMemoryBarrier, 4> barriers{};
        auto createBarrier = [&](vk::Buffer buf, uint32_t i) {
            barriers[i] = vk::BufferMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = buf,
                .offset = 0,
                .size = VK_WHOLE_SIZE
            };
        };

        createBarrier(*positionMegaBuffer_, 0);
        createBarrier(*attributeMegaBuffer_, 1);
        createBarrier(*indexMegaBuffer_, 2);
        createBarrier(*meshInfoBuffer_, 3);

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eComputeShader,
            {}, {}, {barriers}, nullptr
        );

        // 7. Очистка
        pendingMeshes_.clear();
        pendingMeshes_.shrink_to_fit();

        return meshInfoBufferBDA_;

    }
}