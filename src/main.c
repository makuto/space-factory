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

const int c_arbitraryDelayTimeMilliseconds = 10;
const char c_tileSize = 32;

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
			for (int tileAssociation = 0; tileAssociation < sizeof(tileSheet->associations) /
			                                                    sizeof(tileSheet->associations[0]);
			     ++tileAssociation)
			{
				CharacterSheetCellAssociation* association =
				    &tileSheet->associations[tileAssociation];
				if (tileToFind == association->key)
				{
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

// Physics
//

struct Vec2
{
	float x;
	float y;
};

const float c_drag = 0.1f;
const float c_deadLimit = 0.02f;  // the minimum velocity below which we are stationary

struct RigidBody
{
	Vec2 Position;
	Vec2 Velocity;
};

float Magnitude(Vec2* vec)
{
	return sqrt(vec->x * vec->x + vec->y * vec->y);
}

void UpdatePhysics(RigidBody* object, float dt)
{
	// update via implicit euler integration
	object->Velocity.x /= (1.f + dt * c_drag);
	object->Velocity.y /= (1.f + dt * c_drag);
	//	 if(Magnitude(&object->Velocity)<=c_deadLimit){
	//		 object->Velocity.x = 0;
	//		 object->Velocity.y = 0;
	//	 }

	object->Position.x += object->Velocity.x * dt;
	object->Position.y += object->Velocity.y * dt;
}

RigidBody SpawnPlayerPhys()
{
	RigidBody player;
	player.Position.x = 100.f;
	player.Position.y = 100.f;
	player.Velocity.x = 0.f;
	player.Velocity.y = 0.f;
	return player;
}

//

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
		tileSheetTexture = SDL_CreateTextureFromSurface(renderer, tileSheetSurface);
		SDL_FreeSurface(tileSheetSurface);
		if (!tileSheetTexture)
		{
			sdlPrintError();
			return 1;
		}
	}
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
		                       "##################"
		                       "#................#"
		                       "l<<<<<<<<<<<<<f<<c"
		                       "l<<<<<<<<<.V<<f<<c"
		                       "#........A.V.....#"
		                       "#........A<<.....#"
		                       "##################");

		renderGridSpaceText(playerShip);
	}

	RigidBody playerPhys = SpawnPlayerPhys();
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

		const float shipThrust = 20.f;
		if (currentKeyStates[SDL_SCANCODE_W] || currentKeyStates[SDL_SCANCODE_UP])
		{
			playerPhys.Velocity.y = -shipThrust;
		}
		if (currentKeyStates[SDL_SCANCODE_S] || currentKeyStates[SDL_SCANCODE_DOWN])
		{
			playerPhys.Velocity.y = shipThrust;
		}
		if (currentKeyStates[SDL_SCANCODE_A] || currentKeyStates[SDL_SCANCODE_LEFT])
		{
			playerPhys.Velocity.x = -shipThrust;
		}
		if (currentKeyStates[SDL_SCANCODE_D] || currentKeyStates[SDL_SCANCODE_RIGHT])
		{
			playerPhys.Velocity.x = shipThrust;
		}

		SDL_RenderClear(renderer);

		// if (SDL_RenderCopy(renderer, tileSheetTexture, NULL, NULL) != 0)
		//{
		//	sdlPrintError();
		//	exitReason = "SDL encountered error";
		//}

		renderGridSpaceFromTileSheet(playerShip, playerPhys.Position.x, playerPhys.Position.y,
		                             renderer, &tileSheet);
		UpdatePhysics(&playerPhys, .1);

		// fprintf(stderr, "Player Position x: %f y:
		// %f",playerPhys.Position.x,playerPhys.Position.y);
		SDL_RenderPresent(renderer);
		SDL_Delay(c_arbitraryDelayTimeMilliseconds);
		SDL_UpdateWindowSurface(window);
	}

	if (exitReason)
	{
		fprintf(stderr, "Exiting. Reason: %s\n", exitReason);
	}
	sdlShutdown(window);

	return 0;
}
