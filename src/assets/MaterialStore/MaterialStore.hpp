#pragma once
#include "IncludeVulkan.hpp" // Для vk::Device и т.д.
#include "Material.hpp"   // Твоя структура render::GpuMaterial

#include <vector>
#include <stdexcept>
#include <mutex> // Для потокобезопасности, если Materials могут добавляться в разных потоках

#include "memory/DeviceAllocator/DeviceAllocator.hpp"

namespace shuttle_engine::render {

    // Класс для управления глобальным SSBO материалов
    class MaterialStore {
    public:
        // Конструктор: maxMaterials - максимальное количество материалов в хранилище
        MaterialStore(vk::Device device,
                      memory::DeviceAllocator& allocator,
                      size_t maxMaterials);
        ~MaterialStore();

        MaterialStore(const MaterialStore&) = delete;
        MaterialStore& operator=(const MaterialStore&) = delete;
        MaterialStore(MaterialStore&&) noexcept = default;
        MaterialStore& operator=(MaterialStore&&) noexcept = default;

        // Добавляет новый материал в хранилище на CPU и возвращает его MaterialID
        uint32_t addMaterial(const Material& material);

        // Обновляет GPU-буфер, копируя туда все материалы с CPU.
        // Возвращает BDA-адрес буфера материалов для Push Constants.
        // Принимает CommandBuffer и Fence, чтобы выполнить копирование асинхронно.
        uint64_t updateGpuBuffer(vk::CommandBuffer cmd, vk::Fence fence);

        // Возвращает BDA-адрес буфера материалов (для Push Constants)
        [[nodiscard]] uint64_t getDeviceAddress() const noexcept { return materialBufferBDA_; }

        // Возвращает ID дефолтного материала (например, чисто белого)
        [[nodiscard]] uint32_t getDefaultMaterialId() const noexcept { return defaultMaterialId_; }

    private:
        vk::Device device_;
        memory::DeviceAllocator& allocator_;
        size_t maxMaterials_;

        std::vector<Material> materials_; // Хранилище материалов на CPU
        bool isDirty_ = false; // Флаг: были ли изменения на CPU, требующие обновления GPU-буфера

        memory::UniqueAllocatedBuffer materialBuffer_; // SSBO материалов на GPU (DEVICE_LOCAL)
        vk::DeviceAddress materialBufferBDA_ = 0; // BDA адрес этого SSBO

        // Временный CPU-видимый буфер для быстрой записи материалов CPU
        memory::UniqueAllocatedBuffer cpuVisibleStagingBuffer_; // HOST_VISIBLE
        uint8_t* cpuVisibleMappedPtr_ = nullptr; // Замапленный указатель

        uint32_t defaultMaterialId_ = 0; // ID дефолтного материала (белый, матовый)

        void createGpuBuffers();
        void createDefaultMaterial(); // Создает и добавляет дефолтный материал
    };

} // namespace shuttle_engine::render
