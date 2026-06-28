#pragma once
#include <SDL2/SDL.h>
#include "IncludeVulkan.hpp"

namespace shuttle_engine {

	class SdlWindow {
	public:
		SdlWindow(char const* title, int width, int height);

		SDL_Window* getWindow() { return window; }

		[[nodiscard]] vk::SurfaceKHR createVulkanSurface(vk::Instance const& instance) const;
		[[nodiscard]] vk::UniqueSurfaceKHR createVulkanSurfaceUnique(vk::Instance const& instance) const;

		// Запрет копирования/перемещения (окно уникально)
		SdlWindow(SdlWindow const&) = delete;
		SdlWindow& operator=(SdlWindow const&) = delete;
		SdlWindow(SdlWindow&&) = delete;
		SdlWindow& operator=(SdlWindow&&) = delete;

		vk::Extent2D getExtent() const;

		// Чистые методы изменения состояния окна
		void setPosition(int x, int y);
		void setSize(int width, int height);
		void close();
		void show();
		void hide();
		void maximize();
		void minimize();
		void restore();

		// Добавь в SdlWindow.hpp в public:
		void setFullscreen(bool enabled);
		void setBorderless(bool enabled); // На случай, если захочется сделать просто окно без рамок


		~SdlWindow();

	private:
		SDL_Window* window = nullptr;
	};

} // namespace shuttle_engine
