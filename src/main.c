#include <assert.h>
#include <stdio.h>

#include "SDL.h"

// From Cakelisp
#include "SDL.cake.hpp"

typedef unsigned char GridCell;

struct GridSpace
{
	unsigned char width;
	unsigned char height;
	GridCell* data;
};

#define GridCellAt(gridSpace, x, y) gridSpace->data[(y * gridSpace->width) + x]

static GridSpace* createGridSpace(unsigned char width, unsigned char height)
{
	char* mem = (char*)malloc(sizeof(GridSpace) + (width * height * sizeof(GridCell)));
	GridSpace* newSpace = (GridSpace*)mem;
	newSpace->width = width;
	newSpace->height = height;
	newSpace->data = (GridCell*)(mem + sizeof(GridSpace));
	return newSpace;
}

static void freeGridSpace(GridSpace* gridSpace)
{
	free(gridSpace);
}

static void renderGridSpace(GridSpace* gridSpace)
{
	for (int y = 0; y < gridSpace->height; ++y)
	{
		for (int x = 0; x < gridSpace->width; ++x)
		{
			// draw...
			fprintf(stderr, "%c", GridCellAt(gridSpace, x, y));
		}
		fprintf(stderr, "\n");
	}
}

static void setGridSpaceFromString(GridSpace* gridSpace, const char* str)
{
	GridCell* writeHead = gridSpace->data;
	GridCell* gridSpaceEnd = writeHead + (gridSpace->width * gridSpace->height);
	for (const char* c = str; *c != 0; ++c)
	{
		if (*c == '\n')
			continue;

		assert(writeHead < gridSpaceEnd && "GridSpace doesn't have enough room to fit the string.");
		*writeHead = *c;
		++writeHead;
	}
}

//
// Main
//

void initializeCakelisp();

int main(int numArguments, char** arguments)
{
	fprintf(stderr, "Hello, world!\n");

	SDL_Window* window = NULL;
	if (!(sdlInitializeFor2d((&window), "Space Factory", 1920, 1080)))
	{
		return 1;
	}
	const char* exitReason = NULL;

	initializeCakelisp();

	GridSpace* playerShip = createGridSpace(20, 9);

	setGridSpaceFromString(playerShip,
	                       "...................."
	                       ".##################."
	                       ".#................#."
	                       ".#................#."
	                       ".#................#."
	                       ".#................#."
	                       ".#................#."
	                       ".##################."
	                       "....................");

	renderGridSpace(playerShip);

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
