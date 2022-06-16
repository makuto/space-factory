#include <assert.h>
#include <stdio.h>

#include "SDL.h"

// From Cakelisp
#include "SDL.cake.hpp"
#include "SpaceFactory.cake.hpp"

//
// SpaceFactory.cake generates these
//

extern unsigned char _binary_assets_TileSheet_bmp_start;
extern unsigned char _binary_assets_TileSheet_bmp_end;
static unsigned char* startTilesheetBmp = (&_binary_assets_TileSheet_bmp_start);
static unsigned char* endTilesheetBmp = (&_binary_assets_TileSheet_bmp_end);

//
// Constants
//

int c_arbitraryDelayTimeMilliseconds = 10;

//
// Grid
//

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

int main(int numArguments, char** arguments)
{
	fprintf(stderr, "Hello, world!\n");

	SDL_Window* window = NULL;
	if (!(sdlInitializeFor2d((&window), "Space Factory", 1920, 1080)))
	{
		fprintf(stderr, "Failed to initialize SDL\n");
		return 1;
	}

	// Initialize the hardware-accelerated 2D renderer
	// I arbitrarily pick the first one.
	// TODO: Figure out why this opens a new window
	sdlList2dRenderDrivers();
	SDL_Renderer* renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED);
	if (!renderer)
	{
		sdlPrintError();
		return 1;
	}

	// Set up bundled data
	initializeCakelisp();

	// Load tile sheet into texture
	SDL_Texture* tileSheet = NULL;
	{
		SDL_RWops* tileSheetRWOps =
		    SDL_RWFromMem(startTilesheetBmp, endTilesheetBmp - startTilesheetBmp);
		SDL_Surface* tileSheetSurface = SDL_LoadBMP_RW(tileSheetRWOps, /*freesrc=*/1);
		if (!tileSheetSurface)
		{
			fprintf(stderr, "Failed to load tile sheet\n");
			return 1;
		}
		tileSheet = SDL_CreateTextureFromSurface(renderer, tileSheetSurface);
		SDL_FreeSurface(tileSheetSurface);
		if (!tileSheet)
		{
			sdlPrintError();
			return 1;
		}
	}

	// Make some grids
	GridSpace* playerShip = createGridSpace(20, 9);
	{
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

		if (SDL_RenderCopy(renderer, tileSheet, NULL, NULL) != 0)
		{
			sdlPrintError();
			exitReason = "SDL encountered error";
		}

		SDL_RenderPresent(renderer);
		SDL_Delay(c_arbitraryDelayTimeMilliseconds);
		/* SDL_UpdateWindowSurface(window); */
	}

	if (exitReason)
	{
		fprintf(stderr, "Exiting. Reason: %s\n", exitReason);
	}
	sdlShutdown(window);

	return 0;
}
