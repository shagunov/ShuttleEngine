#include "Engine/Engine.hpp"
#include <iostream>

int main(int, char**) {
	try {
		// Создаем движок через безопасную фабрику
		auto engine = shuttle_engine::Engine::create();

		// Запускаем игровой цикл
		engine->run();

		return EXIT_SUCCESS;
	}
	catch (const std::exception& ex) {
		std::cerr << "[CRITICAL ERROR] " << ex.what() << '\n';
		return EXIT_FAILURE;
	}
}
