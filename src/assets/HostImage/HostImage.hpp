#pragma once
#include "IncludeVulkan.hpp"
#include <memory>

// Объявляем функцию очистки из stb_image как extern "C".
// Это позволяет нам использовать её в делейтере без подключения stb_image.h в хедере,
// что сохраняет скорость компиляции (build times) на высоте.


namespace shuttle_engine::assets {

    class HostImage {
    public:
        // Сигнатура нашего удалителя — обычный указатель на функцию.
        // Это позволяет передавать как лямбды без захвата, так и обычные C-функции.
        using DeleterType = void(*)(uint8_t*);

        // Конструктор по умолчанию: создает пустой объект с пустым удалителем-заглушкой
        HostImage()
            : data(nullptr, [](uint8_t*) {}), width(0), height(0), channels(0), size(0), format(vk::Format::eR8G8B8A8Srgb) {}

        // Конструктор, принимающий сырые данные и кастомный удалитель
        HostImage(uint8_t* rawData, uint32_t w, uint32_t h, uint32_t ch, size_t sz, vk::Format fmt, DeleterType deleter)
            : data(rawData, deleter), width(w), height(h), channels(ch), size(sz), format(fmt) {}

        // Компилятор сгенерирует идеальные move-операции автоматически (std::unique_ptr позаботится обо всем)
        HostImage(HostImage&&) noexcept = default;
        HostImage& operator=(HostImage&&) noexcept = default;

        // Копирование строго запрещено — пиксели весят много, копировать их неявно нельзя!
        HostImage(const HostImage&) = delete;
        HostImage& operator=(const HostImage&) = delete;

        ~HostImage() = default;

        // Владеющий указатель с кастомным удалителем. Он автоматически очистит память в деструкторе!
        std::unique_ptr<uint8_t, DeleterType> data;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channels = 0;
        size_t size = 0;
        vk::Format format = vk::Format::eR8G8B8A8Srgb;

        [[nodiscard]] bool isEmpty() const noexcept { return data == nullptr; }
        [[nodiscard]] uint8_t* getRawData() const noexcept { return data.get(); }
    };

    using UniqueHostImage = std::unique_ptr<HostImage>;
}
