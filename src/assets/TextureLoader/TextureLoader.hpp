#pragma once
#include "../HostImage/HostImage.hpp"
#include "../GeneralMipGenerator/GeneralMipGenerator.hpp"
#include "../NormalMipGenerator/NormalMipGenerator.hpp"
#include "../../memory/DeviceAllocator/DeviceAllocator.hpp" // Путь к твоему аллокатору
#include "../../memory/StagingBufferController/StagingBufferController.hpp"
#include "assets/TextureStore/Texture.hpp"

namespace shuttle_engine::assets {

    // Структура, которая возвращается после CPU-подготовки.
    // Она содержит аллокацию в стейджинге и временное (src) и финальное (dst) изображения.
    struct TextureUploadTx {
        memory::StagingAllocation stagingAlloc;
        memory::UniqueAllocatedImage temporaryImage; // Временный srcImage (с флагом STORAGE для нормалей)
        memory::UniqueAllocatedImage finalImage;     // Финальный dstImage (Immutable, eGpuOnly)
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 1;
        vk::Format format = vk::Format::eUndefined;
    };

    class TextureLoader {
    public:
        // Конструктор принимает только устройство, необходимое для инициализации генераторов мипов
        explicit TextureLoader(vk::Device device)
            : generalMipGen_(device)
            , normalMipGen_(device) {}

        ~TextureLoader() = default;

        // ЭТАП 1: CPU-подготовка (без обращения к GPU очереди).
        // Аллоцирует память на GPU (Dst + Temp) и в Staging, копирует пиксели на CPU.
        [[nodiscard]] TextureUploadTx prepareUpload(
            memory::DeviceAllocator& allocator,
            memory::StagingBufferController& staging,
            const HostImage& hostImage) const;

        // ЭТАП 2: Только запись команд.
        // Берет подготовленную транзакцию и пишет команды перехода лайаутов, копирования и генерации мипов
        // в переданный ИЗВНЕ командный буфер. Никаких сабмитов!
        void recordUploadCommands(
            vk::CommandBuffer cmd,
            const TextureUploadTx& tx,
            render::TextureType type);

    private:
        Core::GeneralMipGenerator generalMipGen_;
        Core::NormalMipGenerator normalMipGen_;
    };

} // namespace shuttle_engine::assets
