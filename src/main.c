#include <stdio.h>
#include "SDL.h"
#include "SDL.cake.hpp"

int main(int numArguments, char** arguments)
{
	fprintf(stderr, "Hello, world!\n");
	SDL_Window* window = NULL;
	if (!(sdlInitializeFor2d((&window), "Space Factory", 1920, 1080)))
	{
		return 1;
	}
	const char* exitReason = NULL;

	while (!(exitReason))
	{
		SDL_Event event;
		while (SDL_PollEvent((&event)))
		{
			if ((event.type == SDL_QUIT))
			{
				exitReason = "Window event";
			}
		}
		const Uint8* currentKeyStates = SDL_GetKeyboardState(NULL);
		if (currentKeyStates[SDL_SCANCODE_ESCAPE])
		{
			exitReason = "Escape pressed";
		}
		SDL_UpdateWindowSurface(window);
	}

	if (exitReason)
	{
		fprintf(stderr, "Exiting. Reason: %s\n", exitReason);
	}
	sdlShutdown(window);

	return 0;
}
