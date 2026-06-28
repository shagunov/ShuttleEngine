#include "SdlLibrary.hpp"
#include <SDL2/SDL_vulkan.h>
#include <stdexcept>
#include <iostream>

namespace shuttle_engine {

	SdlLibrary::SdlLibrary() {
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
			throw std::runtime_error("Failed to initialize SDL");
		}
		std::cout << "[SDL] Subsystems initialized successfully.\n";
	}

	void SdlLibrary::setRelativeMouseMode(bool enabled) {
		SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
	}

	void SdlLibrary::postQuitEvent() {
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		SDL_PushEvent(&quitEvent);
	}

	// Идеально быстрый опрос без выделения памяти на куче
	bool SdlLibrary::pollEvent(SDL_Event& event) {
		return SDL_PollEvent(&event) != 0;
	}

	std::vector<char const*> SdlLibrary::getSurfaceRequiredExtensions() {
		uint32_t sdlExtensionCount = 0;
		if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlExtensionCount, nullptr)) {
			throw std::runtime_error("Failed to get SDL Vulkan extension count");
		}
		std::vector<char const*> extensions(sdlExtensionCount);
		if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlExtensionCount, extensions.data())) {
			throw std::runtime_error("Failed to get SDL Vulkan extensions");
		}
		return extensions;
	}

	SdlLibrary::~SdlLibrary() {
		SDL_Quit();
		std::cout << "[SDL] Subsystems deinitialized.\n";
	}

} // namespace shuttle_engine
