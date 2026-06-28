#include "SdlWindow.hpp"
#include <SDL2/SDL_vulkan.h>
#include <stdexcept>
#include <iostream>

namespace shuttle_engine {

    SdlWindow::SdlWindow(char const* title, int width, int height) {
        window = SDL_CreateWindow(
            title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );
        if (!window) {
            throw std::runtime_error("Failed to create SDL window");
        }
        std::cout << "[Window] OS Window created successfully.\n";
    }

    vk::SurfaceKHR SdlWindow::createVulkanSurface(vk::Instance const& instance) const {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
            throw std::runtime_error("Failed to create Vulkan surface");
        }
        return surface;
    }

    vk::UniqueSurfaceKHR SdlWindow::createVulkanSurfaceUnique(vk::Instance const& instance) const {
        return vk::UniqueSurfaceKHR(createVulkanSurface(instance), instance);
    }

    vk::Extent2D SdlWindow::getExtent() const {
        int32_t width, height;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);
        return {
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height)
        };
    }

    void SdlWindow::setPosition(int x, int y) { SDL_SetWindowPosition(window, x, y); }
    void SdlWindow::setSize(int width, int height) { SDL_SetWindowSize(window, width, height); }
    void SdlWindow::close() { SDL_HideWindow(window); } // Мягкое закрытие
    void SdlWindow::show() { SDL_ShowWindow(window); }
    void SdlWindow::hide() { SDL_HideWindow(window); }
    void SdlWindow::maximize() { SDL_MaximizeWindow(window); }
    void SdlWindow::minimize() { SDL_MinimizeWindow(window); }
    void SdlWindow::restore() { SDL_RestoreWindow(window); }

    // Добавь в SdlWindow.cpp:
    void SdlWindow::setFullscreen(bool enabled) {
        if (enabled) {
            // Переключает в режим "Borderless Fullscreen" на текущем разрешении рабочего стола
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            std::cout << "[Window] Switched to Borderless Fullscreen mode.\n";
        } else {
            // Возвращает в оконный режим
            SDL_SetWindowFullscreen(window, 0);
            std::cout << "[Window] Switched to Windowed mode.\n";
        }
    }

    void SdlWindow::setBorderless(bool enabled) {
        // Включает или выключает рамку окна (заголовок, кнопки закрыть/свернуть)
        SDL_SetWindowBordered(window, enabled ? SDL_FALSE : SDL_TRUE);
    }


    SdlWindow::~SdlWindow() {
        if (window) {
            SDL_DestroyWindow(window);
            std::cout << "[Window] OS Window destroyed.\n";
        }
    }

} // namespace shuttle_engine
