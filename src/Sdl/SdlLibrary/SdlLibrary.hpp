#pragma once
#include <vector>
#include <SDL2/SDL.h>
#include "../SdlKeyboard/SdlKeyCode.hpp"
#include "../SdlKeyboard/SdlKeyMode.hpp"

namespace shuttle_engine {

	class SdlLibrary {
	public:
		SdlLibrary();

		// Запрет копирования и перемещения
		SdlLibrary(SdlLibrary const&) = delete;
		SdlLibrary& operator=(SdlLibrary const&) = delete;
		SdlLibrary(SdlLibrary&&) = delete;
		SdlLibrary& operator=(SdlLibrary&&) = delete;

		void setRelativeMouseMode(bool enabled);
		void postQuitEvent();

		// САМЫЙ ВАЖНЫЙ МЕТОД: Опрашивает одно событие. Без аллокаций!
		// Возвращает true, если событие есть, и запишет его в 'event'
		bool pollEvent(SDL_Event& event);

		[[nodiscard]] static std::vector<char const*> getSurfaceRequiredExtensions();

		~SdlLibrary();
	};

} // namespace shuttle_engine
