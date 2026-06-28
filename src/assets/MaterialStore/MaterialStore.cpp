#include "MaterialStore.hpp"
#include <iostream>
#include <algorithm> // For std::min

namespace shuttle_engine::render {

    MaterialStore::MaterialStore(vk::Device device, memory::DeviceAllocator& allocator, size_t maxMaterials)
        : device_(device), allocator_(allocator), maxMaterials_(maxMaterials) 
    {
        materials_.reserve(maxMaterials_);
        createGpuBuffers();
        createDefaultMaterial(); // Создаем дефолтный материал при инициализации
    }

    MaterialStore::~MaterialStore() {
        // UniqueAllocatedBuffer автоматически уничтожит VkBuffer и VmaAllocation
        // при выходе из области видимости
    }

    void MaterialStore::createGpuBuffers() {
        // 1. Создаем SSBO для материалов на GPU (DEVICE_LOCAL_BIT, с BDA)
        auto [resBuf, uniqueMaterialBuffer] = allocator_.createAndAllocateBufferUnique(
            {
                .size = maxMaterials_ * sizeof(GpuMaterial),
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | 
                         vk::BufferUsageFlagBits::eTransferDst |   // Куда будут копировать данные
                         vk::BufferUsageFlagBits::eShaderDeviceAddress, // Важно: для BDA
                .sharingMode = vk::SharingMode::eExclusive
            },
            memory::MemoryUsage::eGpuOnly // Только быстрая VRAM!
        );
        if (resBuf != vk::Result::eSuccess) throw std::runtime_error("[MaterialStore] Failed to create material SSBO!");
        materialBuffer_ = std::move(uniqueMaterialBuffer);
        materialBufferBDA_ = allocator_.getBufferDeviceAddress(*materialBuffer_); // Получаем BDA-адрес

        // 2. Создаем временный CPU-видимый буфер для быстрой записи данных CPU (HOST_VISIBLE)
        auto [resStg, uniqueCpuVisibleStagingBuffer] = allocator_.createAndAllocateBufferUnique(
            {
                .size = maxMaterials_ * sizeof(GpuMaterial),
                .usage = vk::BufferUsageFlagBits::eTransferSrc, // Откуда будут копироваться данные
                .sharingMode = vk::SharingMode::eExclusive
            },
            memory::MemoryUsage::eCpuToGpu, // Host-visible
            memory::AllocationCreateFlags(static_cast<uint32_t>(memory::AllocationCreateFlagBits::eMapped) |
                                          static_cast<uint32_t>(memory::AllocationCreateFlagBits::eHostAccessSequentialWrite))
        );
        if (resStg != vk::Result::eSuccess) throw std::runtime_error("[MaterialStore] Failed to create CPU-visible staging buffer for materials!");
        cpuVisibleStagingBuffer_ = std::move(uniqueCpuVisibleStagingBuffer);
        cpuVisibleMappedPtr_ = static_cast<uint8_t*>(allocator_.getMappedPointer(*cpuVisibleStagingBuffer_));
        if (!cpuVisibleMappedPtr_) throw std::runtime_error("[MaterialStore] Failed to map CPU-visible staging buffer for materials!");
    }

    void MaterialStore::createDefaultMaterial() {
        GpuMaterial defaultMat;
        defaultMat.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f}; // Белый цвет
        defaultMat.pbrFactors = {0.0f, 1.0f, 1.0f, 0.5f};     // Metallic=0, Roughness=1, AO=1 (матовый, неметаллический, полностью освещенный)
        
        // Эти индексы будут указывать на дефолтные текстуры, которые хранятся в TextureStore (ID 0, 1, 2)
        // AssetManager при создании этого defaultMat должен будет запросить TextureStore о default IDs
        // Для компиляции сейчас оставляем 0, AssetManager потом это обновит.
        defaultMat.albedoIdx   = 0; // Placeholder
        defaultMat.normalIdx   = 0; // Placeholder
        defaultMat.ormIdx      = 0; // Placeholder
        defaultMat.emissiveIdx = 0; // Placeholder

        defaultMat.samplerIdx = 0; // Индекс на LinearRepeat Sampler (Set 0)
        defaultMat.emissiveStrength = 0.0f; // Не светится
        defaultMat.flags = 0;

        defaultMaterialId_ = addMaterial(defaultMat); // Добавляем в свой список
        isDirty_ = true; // Считаем, что буфер изменился
    }

    uint32_t MaterialStore::addMaterial(const Material& material) {
        if (materials_.size() >= maxMaterials_) {
            throw std::runtime_error("[MaterialStore] Material buffer is full! Max materials: " + std::to_string(maxMaterials_));
        }
        uint32_t id = static_cast<uint32_t>(materials_.size());
        materials_.push_back(material);
        isDirty_ = true; // Помечаем, что GPU-буфер требует обновления
        return id;
    }

    uint64_t MaterialStore::updateGpuBuffer(vk::CommandBuffer cmd, vk::Fence fence) {
        if (!isDirty_) {
            return materialBufferBDA_; // Если изменений нет, возвращаем текущий адрес
        }
        
        // Копируем все CPU-данные в CPU-видимый буфер
        size_t currentSize = materials_.size() * sizeof(Material);
        std::memcpy(cpuVisibleMappedPtr_, materials_.data(), currentSize);

        // Записываем команды копирования в GPU-буфер
        vk::BufferCopy copyRegion{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = currentSize
        };
        cmd.copyBuffer(*cpuVisibleStagingBuffer_, *materialBuffer_, 1, &copyRegion);

        // Барьер: Ждем завершения копирования (из Transfer в Shader Read), 
        // прежде чем шейдеры смогут читать эти материалы.
        vk::BufferMemoryBarrier barrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eShaderRead,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = *materialBuffer_,
            .offset = 0,
            .size = VK_WHOLE_SIZE // Весь буфер
        };
        
        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader,
            {}, {}, {barrier}, {}
        );

        isDirty_ = false; // Сбрасываем флаг, буфер обновлен
        return materialBufferBDA_; // Возвращаем BDA-адрес для Push Constants
    }

} // namespace shuttle_engine::render