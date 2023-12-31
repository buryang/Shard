#include "System/Input/InputSystem.h"
#include <cstring>
#include "gtest/gtest.h"


using namespace Shard::System::Input;

#ifdef USE_RAWINPUT
TEST(TEST_INPUT_SYSTEM, TEST_RAWINPUT_API) {

	for (;;) {

	}
}
#elif defined(USE_SDL)
#include <SDL3/SDL.h>
TEST(TEST_INPUT_SYSTEM, TEST_SDL_API) {

	SDL_Init(SDL_INIT_EVERYTHING);
	auto window = SDL_CreateWindow("GTEST", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0u);
	bool quit = false;
	InputSystem::Instance().Init();
	for (;;) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_EVENT_QUIT:
				quit = true;
				break;
			case SDL_WINDOWEVENT:
				switch (event.window.event) {
				case SDL_WINDOWEVENT_CLOSE:
					quit = true;
					break;

				}
				break;
			}
			InputMessage message;
			new(&message)SDL_Event(event);
			InputSystem::Instance().ForwardMessage(message);
			const auto& mouse_state = InputSystem::Instance().GetMouseState();
			EXPECT_TRUE(mouse_state.mouse_left_ != EButtonState::eNone) << "mouse state not work";
		}
		if (quit) {
			break;
		}
		SDL_Delay(100);
	}

	InputSystem::Instance().UnInit();
	SDL_DestroyWindow(window);
	SDL_Quit();
}
#endif