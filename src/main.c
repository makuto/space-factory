#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"

// From Cakelisp
#include "SDL.cake.hpp"
#include "SpaceFactory.cake.hpp"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

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

// Rendering
const int c_arbitraryDelayTimeMilliseconds = 10;
const char c_tileSize = 32;

// Ship
const float c_shipThrust = 300.f;
const float c_maxSpeed = 1500.f;

// Physics
const float c_drag = 1.f;
const float c_deadLimit = 0.02f;  // the minimum velocity below which we are stationary

//
// Grid
//

typedef unsigned char GridCell;

typedef struct GridSpace
{
	unsigned char width;
	unsigned char height;
	GridCell* data;
} GridSpace;

#define GridCellAt(gridSpace, x, y) gridSpace->data[(y * gridSpace->width) + x]

// Let's try to keep it static for now
/* static GridSpace* createGridSpace(unsigned char width, unsigned char height) */
/* { */
/* 	char* mem = (char*)malloc(sizeof(GridSpace) + (width * height * sizeof(GridCell))); */
/* 	GridSpace* newSpace = (GridSpace*)mem; */
/* 	newSpace->width = width; */
/* 	newSpace->height = height; */
/* 	newSpace->data = (GridCell*)(mem + sizeof(GridSpace)); */
/* 	return newSpace; */
/* } */

/* static void freeGridSpace(GridSpace* gridSpace) */
/* { */
/* 	free(gridSpace); */
/* } */

static void renderGridSpaceText(GridSpace* gridSpace)
{
	for (int y = 0; y < gridSpace->height; ++y)
	{
		for (int x = 0; x < gridSpace->width; ++x)
		{
			fprintf(stderr, "%c", GridCellAt(gridSpace, x, y));
		}
		fprintf(stderr, "\n");
	}
}

typedef enum TextureTransform
{
	TextureTransform_None,
	TextureTransform_FlipHorizontal,
	TextureTransform_FlipVertical,
	TextureTransform_Clockwise90,
	TextureTransform_CounterClockwise90,
} TextureTransform;

const float c_transformsToAngles[] = {0.f, 0.f, 0.f, 90.f, 270.f};
const SDL_RendererFlip c_transformsToSDLRenderFlips[] = {
    SDL_FLIP_NONE, SDL_FLIP_HORIZONTAL, SDL_FLIP_VERTICAL, SDL_FLIP_NONE, SDL_FLIP_NONE};

typedef struct CharacterSheetCellAssociation
{
	char key;
	// Values
	char row;
	char column;
	char transform;
} CharacterSheetCellAssociation;

typedef struct TileSheet
{
	// That's how big the tile sheet is!
	CharacterSheetCellAssociation associations[4 * 4];

	SDL_Texture* texture;
} TileSheet;

static void renderGridSpaceFromTileSheet(GridSpace* gridSpace, int originX, int originY,
                                         SDL_Renderer* renderer, TileSheet* tileSheet)
{
	for (int cellY = 0; cellY < gridSpace->height; ++cellY)
	{
		for (int cellX = 0; cellX < gridSpace->width; ++cellX)
		{
			char tileToFind = GridCellAt(gridSpace, cellX, cellY);
			for (int tileAssociation = 0; tileAssociation < ARRAY_SIZE(tileSheet->associations);
			     ++tileAssociation)
			{
				CharacterSheetCellAssociation* association =
				    &tileSheet->associations[tileAssociation];
				if (tileToFind != association->key)
					continue;

				int textureX = association->column * c_tileSize;
				int textureY = association->row * c_tileSize;
				int screenX = originX + (cellX * c_tileSize);
				int screenY = originY + (cellY * c_tileSize);
				SDL_Rect sourceRectangle = {textureX, textureY, c_tileSize, c_tileSize};
				SDL_Rect destinationRectangle = {screenX, screenY, c_tileSize, c_tileSize};
				SDL_RenderCopyEx(renderer, tileSheet->texture, &sourceRectangle,
				                 &destinationRectangle,
				                 c_transformsToAngles[association->transform],
				                 /*rotate about (default = center)*/ NULL,
				                 c_transformsToSDLRenderFlips[association->transform]);
				break;
			}
		}
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
// Physics
//

struct Vec2
{
	float x;
	float y;
};

struct RigidBody
{
	Vec2 position;
	Vec2 velocity;
};

float Magnitude(Vec2* vec)
{
	return sqrt(vec->x * vec->x + vec->y * vec->y);
}

void UpdatePhysics(RigidBody* object, float dt)
{
	// update via implicit euler integration
	object->velocity.x /= (1.f + (dt * c_drag));
	object->velocity.y /= (1.f + (dt * c_drag));
	//	 if(Magnitude(&object->velocity)<=c_deadLimit){
	//		 object->velocity.x = 0;
	//		 object->velocity.y = 0;
	//	 }

	object->position.x += object->velocity.x * dt;
	object->position.y += object->velocity.y * dt;
}

RigidBody SpawnPlayerPhys()
{
	RigidBody player;
	player.position.x = 100.f;
	player.position.y = 100.f;
	player.velocity.x = 0.f;
	player.velocity.y = 0.f;
	return player;
}

//
// Objects
//

typedef struct Object
{
	char type;
	RigidBody body;
} Object;

Object objects[1024] = {0};

void renderObjects(SDL_Renderer* renderer, TileSheet* tileSheet)
{
	for (int i = 0; i < ARRAY_SIZE(objects); ++i)
	{
		const Object* currentObject = &objects[i];
		if (!currentObject->type)
			continue;

		for (int tileAssociation = 0; tileAssociation < ARRAY_SIZE(tileSheet->associations);
		     ++tileAssociation)
		{
			CharacterSheetCellAssociation* association = &tileSheet->associations[tileAssociation];
			if (currentObject->type != association->key)
				continue;

			int textureX = association->column * c_tileSize;
			int textureY = association->row * c_tileSize;
			int screenX = currentObject->body.position.x;
			int screenY = currentObject->body.position.y;
			SDL_Rect sourceRectangle = {textureX, textureY, c_tileSize, c_tileSize};
			SDL_Rect destinationRectangle = {screenX, screenY, c_tileSize, c_tileSize};
			SDL_RenderCopyEx(renderer, tileSheet->texture, &sourceRectangle, &destinationRectangle,
			                 c_transformsToAngles[association->transform],
			                 /*rotate about (default = center)*/ NULL,
			                 c_transformsToSDLRenderFlips[association->transform]);
			break;
		}
	}
}

//
// Main
//

int main(int numArguments, char** arguments)
{
	fprintf(stderr, "Hello, world!\n");

	SDL_Window* window = NULL;
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	if (!(sdlInitializeFor2d((&window), "Space Factory", 1920, 1080)))
	{
		fprintf(stderr, "Failed to initialize SDL\n");
		return 1;
	}

	// Initialize the hardware-accelerated 2D renderer
	// I arbitrarily pick the first one.
	// TODO: Figure out why this opens a new window
	sdlList2dRenderDrivers();
	SDL_Renderer* renderer =
	    SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer)
	{
		sdlPrintError();
		return 1;
	}
	if (SDL_RenderSetVSync(renderer, 1) != 0 || SDL_GL_SetSwapInterval(1) != 0)
	{
		sdlPrintError();
		return 1;
	}

	// Set up bundled data
	initializeCakelisp();

	// Load tile sheet into texture
	SDL_Texture* tileSheetTexture = NULL;
	{
		SDL_RWops* tileSheetRWOps =
		    SDL_RWFromMem(startTilesheetBmp, endTilesheetBmp - startTilesheetBmp);
		SDL_Surface* tileSheetSurface = SDL_LoadBMP_RW(tileSheetRWOps, /*freesrc=*/1);
		if (!tileSheetSurface)
		{
			fprintf(stderr, "Failed to load tile sheet\n");
			return 1;
		}
		// Use pure black as our chroma key
		SDL_SetColorKey(tileSheetSurface, SDL_TRUE, SDL_MapRGB(tileSheetSurface->format, 0, 0, 0));
		tileSheetTexture = SDL_CreateTextureFromSurface(renderer, tileSheetSurface);
		SDL_FreeSurface(tileSheetSurface);
		if (!tileSheetTexture)
		{
			sdlPrintError();
			return 1;
		}
	}
	// TODO: Turn this into a direct lookup table
	TileSheet tileSheet = {{
	                           // Wall
	                           {'#', 0, 0, TextureTransform_None},
	                           // Floor
	                           {'.', 0, 1, TextureTransform_None},
	                           // Conveyor to left
	                           {'<', 0, 2, TextureTransform_None},
	                           // Conveyor to right
	                           {'>', 0, 2, TextureTransform_FlipHorizontal},
	                           // Conveyor to up
	                           {'A', 0, 2, TextureTransform_Clockwise90},
	                           // Conveyor to down
	                           {'V', 0, 2, TextureTransform_CounterClockwise90},
	                           // Furnace
	                           {'f', 2, 1, TextureTransform_None},
	                           // Intake from right
	                           {'c', 2, 0, TextureTransform_None},
	                           // Engine to left (unpowered)
	                           {'l', 1, 1, TextureTransform_FlipHorizontal},
	                           {'r', 1, 1, TextureTransform_None},
	                           {'u', 1, 1, TextureTransform_Clockwise90},
	                           {'d', 1, 1, TextureTransform_CounterClockwise90},

	                           // Objects
	                           // Unrefined fuel
	                           {'U', 0, 3, TextureTransform_None},
	                       },
	                       tileSheetTexture};

	// Make some grids
	GridSpace playerShipData = {0};
	playerShipData.width = 18;
	playerShipData.height = 7;
	GridCell playerShipCells[18 * 7];
	playerShipData.data = playerShipCells;
	GridSpace* playerShip = &playerShipData;
	{
		setGridSpaceFromString(playerShip,
		                       "#######d##########"
		                       "#................#"
		                       "l<<<<<<<<<<<<<f<<c"
		                       "l<<<<<<<<<.V<<f<<c"
		                       "#........A.V.....#"
		                       "#........A<<.....r"
		                       "#######u##########");

		renderGridSpaceText(playerShip);
	}
	RigidBody playerPhys = SpawnPlayerPhys();

	// Make some objects
	for (int i = 0; i < 15; ++i)
	{
		Object* testObject = &objects[i];
		testObject->type = 'U';
		testObject->body.position.x = (float)(rand() % 1000);
		testObject->body.position.y = (float)(rand() % 1000);
	}

	// Main loop
	Uint64 lastFrameNumTicks = SDL_GetPerformanceCounter();
	const Uint64 performanceNumTicksPerSecond = SDL_GetPerformanceFrequency();
	const char* exitReason = NULL;
	while (!(exitReason))
	{
		Uint64 currentCounterTicks = SDL_GetPerformanceCounter();
		Uint64 frameDiffTicks = (currentCounterTicks - lastFrameNumTicks);
		float deltaTime = (frameDiffTicks / ((float)performanceNumTicksPerSecond));

		fprintf(stderr, "%f\n", deltaTime);

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

		float shipThrust = c_shipThrust * deltaTime;
		if (currentKeyStates[SDL_SCANCODE_W] || currentKeyStates[SDL_SCANCODE_UP])
		{
			playerPhys.velocity.y += -shipThrust;
		}
		if (currentKeyStates[SDL_SCANCODE_S] || currentKeyStates[SDL_SCANCODE_DOWN])
		{
			playerPhys.velocity.y += shipThrust;
		}
		if (currentKeyStates[SDL_SCANCODE_A] || currentKeyStates[SDL_SCANCODE_LEFT])
		{
			playerPhys.velocity.x += -shipThrust;
		}
		if (currentKeyStates[SDL_SCANCODE_D] || currentKeyStates[SDL_SCANCODE_RIGHT])
		{
			playerPhys.velocity.x += shipThrust;
		}

		if (playerPhys.velocity.y > c_maxSpeed) playerPhys.velocity.y = c_maxSpeed;
		if (playerPhys.velocity.y < -c_maxSpeed) playerPhys.velocity.y = -c_maxSpeed;
		if (playerPhys.velocity.x > c_maxSpeed) playerPhys.velocity.x = c_maxSpeed;
		if (playerPhys.velocity.x < -c_maxSpeed) playerPhys.velocity.x = -c_maxSpeed;

		UpdatePhysics(&playerPhys, deltaTime);

		SDL_RenderClear(renderer);

		renderGridSpaceFromTileSheet(playerShip, playerPhys.position.x, playerPhys.position.y,
		                             renderer, &tileSheet);

		renderObjects(renderer, &tileSheet);

		lastFrameNumTicks = SDL_GetPerformanceCounter();
		SDL_RenderPresent(renderer);
		SDL_UpdateWindowSurface(window);
		SDL_Delay(c_arbitraryDelayTimeMilliseconds);
	}

	if (exitReason)
	{
		fprintf(stderr, "Exiting. Reason: %s\n", exitReason);
	}
	sdlShutdown(window);

	return 0;
}
