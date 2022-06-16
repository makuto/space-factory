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

typedef struct CharacterSheetCellAssociation
{
	char key;
	// Values
	char row;
	char column;
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
					SDL_RenderCopy(renderer, tileSheet->texture, &sourceRectangle,
					               &destinationRectangle);
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


//Physics 
//


 struct Vec2{
	 float x;
	 float y;
 };

const float drag = 0; 
const float dead_limit = .02;//the minimum velocity below which we are stationary

 struct RigidBody{
	 Vec2 Position;
	 Vec2 Velocity;
 };


float Magnitude(Vec2* vec){
	return sqrt(vec->x*vec->x + vec->y*vec->y);
}


void UpdatePhysics(RigidBody* object, float dt){
	 //update via implicit euler integration 
	 object->Velocity.x  /=(1+dt*drag);
	 object->Velocity.y  /=(1+dt*drag);
//	 if(Magnitude(&object->Velocity)<=dead_limit){
//		 object->Velocity.x = 0;
//		 object->Velocity.y = 0;
//	 }

	 object->Position.x += object->Velocity.x *dt; 
	 object->Position.y += object->Velocity.y *dt; 

 }


RigidBody SpawnPlayerPhys(){
	RigidBody player;
	player.Position.x = 1.0;
	player.Position.y = 1.0;
	player.Velocity.x = 10.0;
	player.Velocity.y = 10.0;
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
	TileSheet tileSheet = {{{'#', 0, 0}, {'.', 0, 1}}, tileSheetTexture};

	// Make some grids
	GridSpace* playerShip = createGridSpace(20, 9);
	{
		setGridSpaceFromString(playerShip,
		                       "                    "
		                       " ################## "
		                       " #................# "
		                       " #................# "
		                       " #................# "
		                       " #................# "
		                       " #................# "
		                       " ################## "
		                       "                    ");

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

		SDL_RenderClear(renderer);

		// if (SDL_RenderCopy(renderer, tileSheetTexture, NULL, NULL) != 0)
		//{
		//	sdlPrintError();
		//	exitReason = "SDL encountered error";
		//}
		
		renderGridSpaceFromTileSheet(playerShip, playerPhys.Position.x, playerPhys.Position.y, renderer, &tileSheet);
		UpdatePhysics(&playerPhys,.1);

         	//fprintf(stderr, "Player Position x: %f y: %f",playerPhys.Position.x,playerPhys.Position.y);
		SDL_RenderPresent(renderer);
		SDL_Delay(c_arbitraryDelayTimeMilliseconds);
		 SDL_UpdateWindowSurface(window); 
	}

	freeGridSpace(playerShip);

	if (exitReason)
	{
		fprintf(stderr, "Exiting. Reason: %s\n", exitReason);
	}
	sdlShutdown(window);

	return 0;
}
