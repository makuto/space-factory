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

// Math
struct Vec2
{
	float x;
	float y;
};

//
// Constants
//

// Rendering

//
// Camera
//
typedef SDL_FRect Camera;
const float c_cameraEaseFactor = 5.f;

/* const int c_arbitraryDelayTimeMilliseconds = 10; */
const char c_tileSize = 32;

// Ship
const float c_shipThrust = 300.f;
const float c_maxSpeed = 1500.f;
const float c_defaultStartFuel = 2.f;
const float c_fuelConsumptionRate = 1.f;

// Physics
const float c_playerDrag = 0.f;
const float c_objectDrag = 0.f;
const float c_deadLimit = 0.02f;  // the minimum velocity below which we are stationary

// Factory
const int c_maxFuel = 5;
const unsigned char c_transitionThreshold = 128;
const unsigned char c_conveyorTransitionPerSecond = 250;
const unsigned char c_furnaceTransitionPerSecond = 100;

//
// Factory Cells
//
struct EngineCell
{
	float fuel;
	bool firing;
};

//
// Grid
//

struct GridCell
{
	unsigned char type;
	union
	{
		EngineCell engineCell;
	} data;
};

typedef struct GridSpace
{
	unsigned char width;
	unsigned char height;
	GridCell* data;
} GridSpace;

#define GridCellAt(gridSpace, x, y) (gridSpace->data[(y * gridSpace->width) + x])

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
			fprintf(stderr, "%c", GridCellAt(gridSpace, x, y).type);
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

bool isEngineTile(unsigned char c)
{
	return c == 'u' || c == 'l' || c == 'r' || c == 'd';
}

static void renderGridSpaceFromTileSheet(GridSpace* gridSpace, int originX, int originY,
                                         SDL_Renderer* renderer, TileSheet* tileSheet,
                                         Camera* camera)
{
	for (int cellY = 0; cellY < gridSpace->height; ++cellY)
	{
		for (int cellX = 0; cellX < gridSpace->width; ++cellX)
		{
			char tileToFind = GridCellAt(gridSpace, cellX, cellY).type;
			for (int tileAssociation = 0; tileAssociation < ARRAY_SIZE(tileSheet->associations);
			     ++tileAssociation)
			{
				CharacterSheetCellAssociation* association =
				    &tileSheet->associations[tileAssociation];
				if (tileToFind != association->key)
					continue;

				int textureX = association->column * c_tileSize;
				int textureY = association->row * c_tileSize;
				int screenX = originX + (cellX * c_tileSize) - camera->x;
				int screenY = originY + (cellY * c_tileSize) - camera->y;
				if (isEngineTile(tileToFind))
				{
					// if this is an engine tile, and its firing, swap the off sprite for the on
					// sprite, and draw the trail
					if (GridCellAt(gridSpace, cellX, cellY).data.engineCell.firing)
					{
						textureX += c_tileSize;
						SDL_Rect sourceRectangle = {textureX + c_tileSize, textureY, c_tileSize,
						                            c_tileSize};
						// compute the trail sprite location
						int trailX = screenX;
						int trailY = screenY;
						if (tileToFind == 'u')
							trailY += c_tileSize;
						if (tileToFind == 'd')
							trailY -= c_tileSize;
						if (tileToFind == 'l')
							trailX -= c_tileSize;
						if (tileToFind == 'r')
							trailX += c_tileSize;
						SDL_Rect destinationRectangle = {trailX, trailY, c_tileSize, c_tileSize};
						SDL_RenderCopyEx(renderer, tileSheet->texture, &sourceRectangle,
						                 &destinationRectangle,
						                 c_transformsToAngles[association->transform],
						                 /*rotate about (default = center)*/ NULL,
						                 c_transformsToSDLRenderFlips[association->transform]);
					}
				}
				SDL_Rect sourceRectangle = {textureX, textureY, c_tileSize, c_tileSize};
				SDL_Rect destinationRectangle = {screenX, screenY, c_tileSize, c_tileSize};
				SDL_RenderCopyEx(renderer, tileSheet->texture, &sourceRectangle,
				                 &destinationRectangle,
				                 c_transformsToAngles[association->transform],
				                 /*rotate about (default = center)*/ NULL,
				                 c_transformsToSDLRenderFlips[association->transform]);

				// always draw the fuel display sprite for engines
				if (isEngineTile(tileToFind))
				{
					const int c_meterShortLength = 5;
					const int c_meterLongLength = 30;
					int meterPosX = screenX;
					int meterPosY = screenY;
					int meterWidth;
					int meterHeight;
					if (tileToFind == 'u')
					{
						meterWidth = c_meterLongLength;
						meterHeight = c_meterShortLength;
					}
					if (tileToFind == 'd')
					{
						meterPosY += c_tileSize - 5;
						meterWidth = c_meterLongLength;
						meterHeight = c_meterShortLength;
					}
					if (tileToFind == 'l')
					{
						meterPosX = screenX + c_tileSize - 5;
						meterWidth = c_meterShortLength;
						meterHeight = c_meterLongLength;
					}
					if (tileToFind == 'r')
					{
						meterWidth = c_meterShortLength;
						meterHeight = c_meterLongLength;
					}

					SDL_Rect fuelMeterRect = {meterPosX, meterPosY, meterWidth + 2,
					                          meterHeight + 2};
					SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
					SDL_RenderDrawRect(renderer, &fuelMeterRect);
					float fuelPercentage =
					    (GridCellAt(gridSpace, cellX, cellY).data.engineCell.fuel) / c_maxFuel;
					if (meterWidth > meterHeight)
					{
						meterWidth *= fuelPercentage;
					}
					else
					{
						meterHeight *= fuelPercentage;
					}

					SDL_Rect fuelRect = {meterPosX + 1, meterPosY + 1, meterWidth, meterHeight};
					SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
					SDL_RenderFillRect(renderer, &fuelRect);

					SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
				}

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
		writeHead->type = *c;
		if (isEngineTile(*c))
		{
			writeHead->data.engineCell.fuel = c_defaultStartFuel;
			writeHead->data.engineCell.firing = false;
		}

		++writeHead;
	}
}

static void renderStarField(SDL_Renderer* renderer, Camera* camera)
{
	static SDL_FRect stars[128] = {0};
	static SDL_FRect dynstars[128] = {0};
	static bool starsInitialized = false;
	if (!starsInitialized)
	{
		for (int i = 0; i < ARRAY_SIZE(stars); ++i)
		{
			stars[i].x = rand() % 1920;
			stars[i].y = rand() % 1080;
			stars[i].w = rand() % 5 + 1;
			stars[i].h = rand() % 5 + 1;
			dynstars[i].w = stars[i].w;
			dynstars[i].h = stars[i].h;
		}
		starsInitialized = true;
	}

	for (int i = 0; i < ARRAY_SIZE(stars); ++i)
	{
		dynstars[i].x = stars[i].x - camera->x/1000;
		dynstars[i].y = stars[i].y - camera->y/1000;
	}

	SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
	SDL_RenderFillRectsF(renderer, dynstars, ARRAY_SIZE(dynstars));
}

//
// Physics
//

struct RigidBody
{
	Vec2 position;
	Vec2 velocity;
};

float Magnitude(Vec2* vec)
{
	return sqrt(vec->x * vec->x + vec->y * vec->y);
}

void UpdatePhysics(RigidBody* object, float drag, float dt)
{
	// update via implicit euler integration
	object->velocity.x /= (1.f + (dt * drag));
	object->velocity.y /= (1.f + (dt * drag));
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
	bool inFactory;
	unsigned char tileX;
	unsigned char tileY;
	// Once this reaches a certain threshold, transition
	unsigned char transition;
	RigidBody body;
} Object;

Object objects[1024] = {0};

void renderObjects(SDL_Renderer* renderer, TileSheet* tileSheet, Camera* camera)
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
			int screenX = currentObject->body.position.x - camera->x;
			int screenY = currentObject->body.position.y - camera->y;
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

int controlEnginesInDirection(GridSpace* gridSpace, char tileType, bool set)
{
	assert(isEngineTile(tileType) &&
	       "tile passed to controlEnginesInDirection was not an engine tile");
	int count = 0;
	for (int cellY = 0; cellY < gridSpace->height; ++cellY)
	{
		for (int cellX = 0; cellX < gridSpace->width; ++cellX)
		{
			GridCell* cell = &GridCellAt(gridSpace, cellX, cellY);
			if (tileType == cell->type && cell->data.engineCell.fuel > 0)
			{
				cell->data.engineCell.firing = set;
				count++;
			}
		}
	}
	return count;
}

void updateEngineFuel(GridSpace* gridSpace, float deltaTime)
{
	for (int cellY = 0; cellY < gridSpace->height; ++cellY)
	{
		for (int cellX = 0; cellX < gridSpace->width; ++cellX)
		{
			GridCell* cell = &GridCellAt(gridSpace, cellX, cellY);
			if (isEngineTile(cell->type) && cell->data.engineCell.firing)
			{
				cell->data.engineCell.fuel -= c_fuelConsumptionRate * deltaTime;

				if (cell->data.engineCell.fuel <= 0.f)
				{
					cell->data.engineCell.fuel = 0.f;
					cell->data.engineCell.firing = false;
				}
			}
		}
	}
}

typedef struct TransitionDelta
{
	char tile;
	char x;
	char y;
} TransitionDelta;

static const TransitionDelta c_transitions[] = {{'c', -1, 0},
                                                {'<', -1, 0},
                                                {'V', 0, 1},
                                                {'A', 0, -1},
                                                {'>', 1, 0},
                                                // TODO
                                                {'f', -1, 0}};
typedef struct TileDelta
{
	char x;
	char y;
	char conveyor;
} TileDelta;
// Useful to check all cardinal directions of a tile
static const TileDelta c_deltas[] = {{-1, 0, '<'}, {1, 0, '>'}, {0, -1, 'A'}, {0, 1, 'V'}};

void doFactory(GridSpace* gridSpace, float deltaTime)
{
	for (int cellY = 0; cellY < gridSpace->height; ++cellY)
	{
		for (int cellX = 0; cellX < gridSpace->width; ++cellX)
		{
			GridCell* cell = &GridCellAt(gridSpace, cellX, cellY);
			switch (cell->type)
			{
				case 'c':
				case '<':
				case 'V':
				case 'A':
				case '>':
				{
					for (int i = 0; i < ARRAY_SIZE(objects); ++i)
					{
						Object* currentObject = &objects[i];
						if (!currentObject->type || currentObject->tileX != cellX ||
						    currentObject->tileY != cellY)
							continue;

						// TODO This isn't very safe because if <1, the object will never transition
						currentObject->transition += c_conveyorTransitionPerSecond * deltaTime;
						if (currentObject->transition > c_transitionThreshold)
						{
							for (int transitionIndex = 0;
							     transitionIndex < ARRAY_SIZE(c_transitions); ++transitionIndex)
							{
								if (c_transitions[transitionIndex].tile != cell->type)
									continue;
								currentObject->tileX += c_transitions[transitionIndex].x;
								currentObject->tileY += c_transitions[transitionIndex].y;
								currentObject->transition = 0;
							}
						}
					}
					break;
				}
					// Furnaces always output to cells away from them
				case 'f':
				{
					for (int i = 0; i < ARRAY_SIZE(objects); ++i)
					{
						Object* currentObject = &objects[i];
						if (!currentObject->type || currentObject->tileX != cellX ||
						    currentObject->tileY != cellY)
							continue;

						// Move objects along which aren't unrefined the same speed as a conveyor
						if (currentObject->type == 'U')
							currentObject->transition += c_furnaceTransitionPerSecond * deltaTime;
						else
							currentObject->transition += c_conveyorTransitionPerSecond * deltaTime;
						if (currentObject->transition > c_transitionThreshold)
						{
							// TODO: Make generic FindAwayConveyor function
							for (int directionIndex = 0; directionIndex < ARRAY_SIZE(c_deltas);
							     ++directionIndex)
							{
								char directionCellX = cellX + c_deltas[directionIndex].x;
								char directionCellY = cellY + c_deltas[directionIndex].y;
								GridCell* currentCell =
								    &GridCellAt(gridSpace, directionCellX, directionCellY);
								// Filter out any cells which aren't conveyors
								// TODO: Check this cell against grid size (otherwise, we WILL
								// crash)
								if (currentCell->type != '<' && currentCell->type != 'V' &&
								    currentCell->type != 'A' && currentCell->type != '>')
									continue;
								GridCell* cellTo =
								    &GridCellAt(gridSpace, directionCellX, directionCellY);
								// TODO: Hack to "randomly" distribute objects in directions
								if (cellTo->type == c_deltas[directionIndex].conveyor &&
								    rand() % 4 == 0)
								{
									if (currentObject->type == 'U')
										currentObject->type = 'R';
									currentObject->tileX += c_deltas[directionIndex].x;
									currentObject->tileY += c_deltas[directionIndex].y;
									currentObject->transition = 0;
									break;
								}
							}
						}
					}
					break;
				}
				case 'l':
				case 'r':
				case 'u':
				case 'd':
				{
					for (int i = 0; i < ARRAY_SIZE(objects); ++i)
					{
						Object* currentObject = &objects[i];
						if (!currentObject->type || currentObject->tileX != cellX ||
						    currentObject->tileY != cellY)
							continue;

						// Only refined objects will give fuel; everything else just gets destroyed
						if (currentObject->type == 'R')
							cell->data.engineCell.fuel += 1.f;
						currentObject->type = 0;
					}
					break;
				}
				default:
					break;
			}
		}
	}
}

void snapCameraToGrid(Camera* camera, Vec2* position, GridSpace* grid, float deltaTime)
{
	// center camera over its position
	// compute grid center
	float x = position->x + (grid->width * c_tileSize) / 2;
	float y = position->y + (grid->height * c_tileSize) / 2;

	float cameraOffsetX = (camera->w / 2.f);
	float cameraOffsetY = (camera->h / 2.f);
	float goalX = x;
	float goalY = y;
	camera->x += cameraOffsetX;
	camera->y += cameraOffsetY;
	// Ease in
	float deltaX = goalX - camera->x;
	float deltaY = goalY - camera->y;
	/* static float deltaSmoothing[10][2] = {0}; */
	/* static char deltaSmoothingWriteHead = 0; */
	/* deltaSmoothing[deltaSmoothingWriteHead][0] = deltaX; */
	/* deltaSmoothing[deltaSmoothingWriteHead][1] = deltaY; */
	/* ++deltaSmoothingWriteHead; */
	/* if (deltaSmoothingWriteHead >= ARRAY_SIZE(deltaSmoothing)) */
	/* 	deltaSmoothingWriteHead = 0; */
	/* float averageDelta[2] = {0}; */
	/* for(int i = 0; i < ARRAY_SIZE(deltaSmoothing); ++i) */
	/* { */
	/* 	averageDelta[0] += deltaSmoothing[i][0]; */
	/* 	averageDelta[1] += deltaSmoothing[i][1]; */
	/* } */
	/* averageDelta[0] /= ARRAY_SIZE(deltaSmoothing); */
	/* averageDelta[1] /= ARRAY_SIZE(deltaSmoothing); */
	camera->x += deltaX * c_cameraEaseFactor * deltaTime;
	camera->y += deltaY * c_cameraEaseFactor * deltaTime;
	camera->x -= cameraOffsetX;
	camera->y -= cameraOffsetY;

	/* fprintf(stderr, "%f, %f (player: %f, %f) delta %f %f\n", camera->x, camera->y, position->x, position->y, deltaX, deltaY); */
}

void updateObjects(RigidBody* playerPhys, GridSpace* playerShipData, float deltaTime)
{
	for (int i = 0; i < ARRAY_SIZE(objects); i++)
	{
		Object* currentObject = &objects[i];
		if (!currentObject->type)
			continue;
		if (!currentObject->inFactory)
			UpdatePhysics(&currentObject->body, c_objectDrag, deltaTime);
		else
		{
			// if the object has been captured into the ship factory, snap it to its tile
			// location, and don't update any other physics
			currentObject->body.position.x =
			    (currentObject->tileX * c_tileSize) + playerPhys->position.x;
			currentObject->body.position.y =
			    (currentObject->tileY * c_tileSize) + playerPhys->position.y;
			currentObject->body.velocity.x = 0;
			currentObject->body.velocity.y = 0;
			continue;
		}

		// check for collisions by converting to tile space when in the proximity of the ship
		float objShipLocalX = (currentObject->body.position.x - playerPhys->position.x) / c_tileSize;
		float objShipLocalY = (currentObject->body.position.y - playerPhys->position.y) / c_tileSize;
		if (objShipLocalY < -1 || objShipLocalY > (playerShipData->height + 1) ||
		    objShipLocalX < -1 || objShipLocalX > (playerShipData->width + 1))
			continue;

		unsigned char shipTileX = (unsigned char)objShipLocalX;
		unsigned char shipTileY = (unsigned char)objShipLocalY;

		GridCell cell = {0};
		if (objShipLocalX < playerShipData->width && objShipLocalX >= 0 &&
		    objShipLocalY < playerShipData->height && objShipLocalY >= 0)
			cell = GridCellAt(playerShipData, shipTileX, shipTileY);

		// otherwise check collisions with solid tiles, and update accordingly
		if (cell.type == '#' || isEngineTile(cell.type))
		{
			if (objShipLocalY > 0 && objShipLocalY <= playerShipData->height)
			{
				// detect collision with edges
				if (playerPhys->velocity.x > 0 && ((int)objShipLocalX == playerShipData->width - 1 ||
				                                  (int)objShipLocalX == playerShipData->width - 2))
				{
					currentObject->body.velocity.x = playerPhys->velocity.x * 1.2f;
					playerPhys->velocity.x -= 10.f;
				}

				if (playerPhys->velocity.x < 0 && shipTileX == 0)
				{
					currentObject->body.velocity.x = playerPhys->velocity.x * 1.2f;
					playerPhys->velocity.x -= 10.f;
				}
			}
			if (objShipLocalX > 0 && objShipLocalX <= playerShipData->width)
			{
				// detect collision with edges
				if (playerPhys->velocity.y > 0 && (int)objShipLocalY == playerShipData->height - 1)
				{
					currentObject->body.velocity.y = playerPhys->velocity.y * 1.2f;
					playerPhys->velocity.y -= 10.f;
				}
				if (playerPhys->velocity.y < 0 && (int)objShipLocalY == 0)
				{
					currentObject->body.velocity.y = playerPhys->velocity.y * 1.2f;
					playerPhys->velocity.y -= 10.f;
				}
			}
		}

		// if the object makes it inside the via an inport let it inside, and mark it as inside
		// the factory
		if (objShipLocalX > 0 && objShipLocalX <= playerShipData->width)
		{
			if (objShipLocalY > 0 && objShipLocalY <= playerShipData->height)
			{
				if (cell.type == 'c')
				{
					currentObject->body.position.x =
					    (shipTileX * c_tileSize) + playerPhys->position.x;
					currentObject->body.position.y =
					    (shipTileY * c_tileSize) + playerPhys->position.y;
					currentObject->body.velocity.x = 0;
					currentObject->body.velocity.y = 0;
					currentObject->tileX = shipTileX;
					currentObject->tileY = shipTileY;
					currentObject->inFactory = true;
				}
			}
		}
	}
}

//
// Ship editing
//

static GridCell* pickGridCellFromWorldSpace(Vec2 gridSpaceWorldPosition, GridSpace* searchGridSpace,
                                            Vec2 pickWorldPosition,
                                            unsigned char* selectionCellXOut,
                                            unsigned char* selectionCellYOut)
{
	float gridSpaceX = (pickWorldPosition.x - gridSpaceWorldPosition.x) / c_tileSize;
	float gridSpaceY = (pickWorldPosition.y - gridSpaceWorldPosition.y) / c_tileSize;
	if (gridSpaceY < 0 || gridSpaceY >= searchGridSpace->height || gridSpaceX < 0 ||
	    gridSpaceX >= searchGridSpace->width)
		return NULL;

	unsigned char cellX = (unsigned char)gridSpaceX;
	unsigned char cellY = (unsigned char)gridSpaceY;

	if (selectionCellXOut)
		*selectionCellXOut = cellX;
	if (selectionCellYOut)
		*selectionCellYOut = cellY;
	return &GridCellAt(searchGridSpace, cellX, cellY);
}

static void drawOutlineRectangle(SDL_Renderer* renderer, SDL_Rect* rectangleToOutline,
                                 Uint32 mouseButtonState)
{
	const int selectionRectanglePadding = 3;
	SDL_Rect selectionRectangle = *rectangleToOutline;
	selectionRectangle.x -= selectionRectanglePadding;
	selectionRectangle.y -= selectionRectanglePadding;
	selectionRectangle.w += selectionRectanglePadding * 2;
	selectionRectangle.h += selectionRectanglePadding * 2;

	if (mouseButtonState & SDL_BUTTON_LMASK)
		SDL_SetRenderDrawColor(renderer, 251, 227, 205, 255);
	else
		SDL_SetRenderDrawColor(renderer, 255, 178, 109, 255);

	// Indicate selection
	SDL_RenderFillRect(renderer, &selectionRectangle);
}

static void renderText(SDL_Renderer* renderer, TileSheet* tileSheet, int x, int y,
					   const char* text)
{
	const int c_fontStartX = 0;
	const int c_fontStartY = 99;
	const int c_fontWidth = 7;
	const int c_fontHeight = 10;
	const int c_charactersPerRow = 18;
	const int c_fontVerticalSpace = 5;
	for (const char* read = text; *read; ++read)
	{
		// Only uppercase
		if (*read < 'A' || *read > 'Z')
			continue;
		char index = *read - 'A';
		int textureX = c_fontStartX + ((index % c_charactersPerRow) * c_fontWidth);
		int textureY = c_fontStartY + ((index / c_charactersPerRow) * (c_fontHeight + c_fontVerticalSpace));
		int screenX = x + ((read - text) * c_fontWidth);
		int screenY = y;
		SDL_Rect sourceRectangle = {textureX, textureY, c_fontWidth, c_fontHeight};
		SDL_Rect destinationRectangle = {screenX, screenY, c_fontWidth, c_fontHeight};
		SDL_RenderCopy(renderer, tileSheet->texture, &sourceRectangle, &destinationRectangle);
	}
}

static void renderNumber(SDL_Renderer* renderer, TileSheet* tileSheet, int x, int y,
                         unsigned int value)
{
	char numberBuffer[16] = {0};
	snprintf(numberBuffer, sizeof(numberBuffer) - 1, "%d", value);
	const int c_fontStartX = 56;
	const int c_fontStartY = 114;
	const int c_fontWidth = 7;
	const int c_fontHeight = 10;
	for (const char* read = numberBuffer; *read; ++read)
	{
		int textureX = c_fontStartX + ((*read - '0') * c_fontWidth);
		int textureY = c_fontStartY;
		int screenX = x + ((read - numberBuffer) * c_fontWidth);
		int screenY = y;
		SDL_Rect sourceRectangle = {textureX, textureY, c_fontWidth, c_fontHeight};
		SDL_Rect destinationRectangle = {screenX, screenY, c_fontWidth, c_fontHeight};
		SDL_RenderCopy(renderer, tileSheet->texture, &sourceRectangle, &destinationRectangle);
	}
}

static void doEditUI(SDL_Renderer* renderer, TileSheet* tileSheet, int windowWidth,
                     int windowHeight, Vec2 cameraPosition, Vec2 gridSpaceWorldPosition,
                     GridSpace* editGridSpace)
{
	int mouseX = 0;
	int mouseY = 0;
	Uint32 mouseButtonState = SDL_GetMouseState(&mouseX, &mouseY);
	static char currentSelectedButtonIndex = 0;
	char editButtons[] = {'#', '.', '<', '>', 'A', 'V', 'f', 'c', 'l', 'r', 'u', 'd'};
	const char* editButtonLabels[] = {
	    "WALL",     "FLOOR",  "CONVEYOR LEFT", "CONVEYOR RIGHT", "CONVEYOR UP", "CONVEYOR DOWN",
	    "REFINERY", "INTAKE", "ENGINE LEFT",   "ENGINE RIGHT",   "ENGINE UP",   "ENGINE DOWN"};
	static unsigned short inventory[] = {/*'#'=*/100, /*'.'=*/999, /*'<'=*/100, /*'>'=*/100,
	                                     /*'A'=*/100, /*'V'=*/100, /*'f'=*/8,   /*'c'=*/8,
	                                     /*'l'=*/8,   /*'r'=*/8,   /*'u'=*/8,   /*'d'=*/8};
	int buttonMarginX = 5;
	int startButtonBarX =
	    ((windowWidth / 2) - ((ARRAY_SIZE(editButtons) * (c_tileSize + buttonMarginX)) / 2));
	int buttonBarY = 32;

	renderText(renderer, tileSheet, startButtonBarX, buttonBarY - 18, "INVENTORY");

	for (int buttonIndex = 0; buttonIndex < ARRAY_SIZE(editButtons); ++buttonIndex)
	{
		for (int tileAssociation = 0; tileAssociation < ARRAY_SIZE(tileSheet->associations);
		     ++tileAssociation)
		{
			CharacterSheetCellAssociation* association = &tileSheet->associations[tileAssociation];
			if (editButtons[buttonIndex] != association->key)
				continue;

			int textureX = association->column * c_tileSize;
			int textureY = association->row * c_tileSize;
			int screenX = startButtonBarX + (buttonIndex * (c_tileSize + buttonMarginX));
			int screenY = buttonBarY;
			SDL_Rect sourceRectangle = {textureX, textureY, c_tileSize, c_tileSize};
			SDL_Rect destinationRectangle = {screenX, screenY, c_tileSize, c_tileSize};

			if (mouseX >= destinationRectangle.x &&
			    mouseX <= destinationRectangle.x + destinationRectangle.w &&
			    mouseY >= destinationRectangle.y &&
			    mouseY <= destinationRectangle.y + destinationRectangle.h)
			{
				if (mouseButtonState & SDL_BUTTON_LMASK)
					currentSelectedButtonIndex = buttonIndex;

				drawOutlineRectangle(renderer, &destinationRectangle, mouseButtonState);

				renderText(renderer, tileSheet, screenX, screenY + c_tileSize + 18,
				           editButtonLabels[buttonIndex]);
			}
			else if (currentSelectedButtonIndex == buttonIndex)
			{
				// TODO: This should probably be a different color
				drawOutlineRectangle(renderer, &destinationRectangle, mouseButtonState);
			}

			SDL_RenderCopyEx(renderer, tileSheet->texture, &sourceRectangle, &destinationRectangle,
			                 c_transformsToAngles[association->transform],
			                 /*rotate about (default = center)*/ NULL,
			                 c_transformsToSDLRenderFlips[association->transform]);

			renderNumber(renderer, tileSheet, screenX, screenY + c_tileSize + 5,
			             inventory[buttonIndex]);
			break;
		}
	}

	Vec2 pickWorldPosition = {mouseX + cameraPosition.x, mouseY + cameraPosition.y};
	unsigned char selectedCellX = 0;
	unsigned char selectedCellY = 0;
	GridCell* selectedCell = pickGridCellFromWorldSpace(
	    gridSpaceWorldPosition, editGridSpace, pickWorldPosition, &selectedCellX, &selectedCellY);
	if (selectedCell)
	{
		for (int tileAssociation = 0; tileAssociation < ARRAY_SIZE(tileSheet->associations);
		     ++tileAssociation)
		{
			CharacterSheetCellAssociation* association = &tileSheet->associations[tileAssociation];
			if (editButtons[currentSelectedButtonIndex] != association->key)
				continue;

			int textureX = association->column * c_tileSize;
			int textureY = association->row * c_tileSize;
			int screenX =
			    (gridSpaceWorldPosition.x - cameraPosition.x) + (selectedCellX * c_tileSize);
			int screenY =
			    (gridSpaceWorldPosition.y - cameraPosition.y) + (selectedCellY * c_tileSize);
			SDL_Rect sourceRectangle = {textureX, textureY, c_tileSize, c_tileSize};
			SDL_Rect destinationRectangle = {screenX, screenY, c_tileSize, c_tileSize};

			drawOutlineRectangle(renderer, &destinationRectangle, mouseButtonState);

			SDL_RenderCopyEx(renderer, tileSheet->texture, &sourceRectangle, &destinationRectangle,
			                 c_transformsToAngles[association->transform],
			                 /*rotate about (default = center)*/ NULL,
			                 c_transformsToSDLRenderFlips[association->transform]);
			break;
		}

		if (mouseButtonState & SDL_BUTTON_LMASK && inventory[currentSelectedButtonIndex] &&
		    selectedCell->type != editButtons[currentSelectedButtonIndex])
		{
			// Give back resources
			for (int buttonIndex = 0; buttonIndex < ARRAY_SIZE(editButtons); ++buttonIndex)
			{
				if (selectedCell->type == editButtons[buttonIndex])
					inventory[buttonIndex] += 1;
			}

			// Make the placement
			inventory[currentSelectedButtonIndex] -= 1;
			memset(selectedCell, sizeof(GridCell), 0);
			selectedCell->type = editButtons[currentSelectedButtonIndex];
		}
	}
}

//Goal
//for now just a simple rect
//



static void renderGoal(SDL_Renderer* renderer, Camera* camera, SDL_Rect* goal){

}

void CheckGoalSatisfied(Vec2* PlayerPos, GridSpace* PlayerShip){

}

//
// Main
//

int main(int numArguments, char** arguments)
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	SDL_Window* window = NULL;
	int windowWidth = 1920;
	int windowHeight = 1080;
	if (!(sdlInitializeFor2d((&window), "Space Factory", windowWidth, windowHeight)))
	{
		fprintf(stderr, "Failed to initialize SDL\n");
		return 1;
	}

	// Initialize the hardware-accelerated 2D renderer
	// Note: I had to set the driver to -1 so that a compatible one is automatically chosen.
	// Otherwise, I get a window that doesn't vsync
	sdlList2dRenderDrivers();
	SDL_Renderer* renderer =
	    SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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
	                           // Refined fuel
	                           {'R', 1, 0, TextureTransform_None},
	                       },
	                       tileSheetTexture};

	// Make some grids
	GridSpace playerShipData = {0};
	playerShipData.width = 18;
	playerShipData.height = 7;
	GridCell playerShipCells[18 * 7] = {0};
	playerShipData.data = playerShipCells;
	GridSpace* playerShip = &playerShipData;
	{
		setGridSpaceFromString(playerShip,
		                       "#######d##########"
		                       "#......A.........#"
		                       "l<<<<<<f<<<<<<<<<c"
		                       "l<<<<<<<<<.V<<<<<c"
		                       "#........A.V.....#"
		                       "#........A<f>>>>>r"
		                       "#######u##########");

		renderGridSpaceText(playerShip);
	}
	RigidBody playerPhys = SpawnPlayerPhys();
	// snap the camera to the player postion
	Camera camera;
	camera.x = playerPhys.position.x - (windowWidth / 2) + (playerShipData.width * c_tileSize) / 2;
	camera.y = playerPhys.position.y - (windowHeight / 2) + (playerShipData.height * c_tileSize) / 2;
	camera.w = windowWidth;
	camera.h = windowHeight;

	// Make some objects
	for (int i = 0; i < 40; ++i)
	{
		Object* testObject = &objects[i];
		testObject->type = 'U';
		testObject->body.position.x = (float)(rand() % windowWidth) + 700;
		testObject->body.position.y = (float)(rand() % windowHeight);
		testObject->body.velocity.x = (float)((rand() % 50) - 25);
		testObject->body.velocity.y = (float)((rand() % 50) - 25);
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
			playerPhys.velocity.y +=
			    -shipThrust * controlEnginesInDirection(&playerShipData, 'u', true);
		}
		else
		{
			controlEnginesInDirection(&playerShipData, 'u', false);
		}
		if (currentKeyStates[SDL_SCANCODE_S] || currentKeyStates[SDL_SCANCODE_DOWN])
		{
			playerPhys.velocity.y +=
			    shipThrust * controlEnginesInDirection(&playerShipData, 'd', true);
		}
		else
		{
			controlEnginesInDirection(&playerShipData, 'd', false);
		}
		if (currentKeyStates[SDL_SCANCODE_A] || currentKeyStates[SDL_SCANCODE_LEFT])
		{
			playerPhys.velocity.x +=
			    -shipThrust * controlEnginesInDirection(&playerShipData, 'r', true);
		}
		else
		{
			controlEnginesInDirection(&playerShipData, 'r', false);
		}
		if (currentKeyStates[SDL_SCANCODE_D] || currentKeyStates[SDL_SCANCODE_RIGHT])
		{
			playerPhys.velocity.x +=
			    shipThrust * controlEnginesInDirection(&playerShipData, 'l', true);
		}
		else
		{
			controlEnginesInDirection(&playerShipData, 'l', false);
		}

		updateEngineFuel(&playerShipData, deltaTime);

		bool playerAtMaxVelocity = false;
		if (playerPhys.velocity.y > c_maxSpeed)
		{
			playerPhys.velocity.y = c_maxSpeed;
			playerAtMaxVelocity = true;
		}
		if (playerPhys.velocity.y < -c_maxSpeed)
		{
			playerPhys.velocity.y = -c_maxSpeed;
			playerAtMaxVelocity = true;
		}
		if (playerPhys.velocity.x > c_maxSpeed)
		{
			playerPhys.velocity.x = c_maxSpeed;
			playerAtMaxVelocity = true;
		}
		if (playerPhys.velocity.x < -c_maxSpeed)
		{
			playerPhys.velocity.x = -c_maxSpeed;
			playerAtMaxVelocity = true;
		}

		UpdatePhysics(&playerPhys, c_playerDrag, deltaTime);
		updateObjects(&playerPhys, &playerShipData, deltaTime);

		doFactory(playerShip, deltaTime);
		snapCameraToGrid(&camera, &playerPhys.position, playerShip, deltaTime);

		// Rendering
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		renderStarField(renderer, &camera);

		renderGridSpaceFromTileSheet(playerShip, playerPhys.position.x, playerPhys.position.y,
		                             renderer, &tileSheet, &camera);

		renderObjects(renderer, &tileSheet, &camera);

		// HUD
		{
			int playerVelocity = (int)(Magnitude(&playerPhys.velocity));
			renderNumber(renderer, &tileSheet, 100, 100, playerVelocity);
			renderText(renderer, &tileSheet, 100, 80, "VELOCITY");
			// This is a bit weird, but informs the player that they will just waste fuel if they
			// keep burning in that direction
			if (playerVelocity >= (int)c_maxSpeed)
				renderText(renderer, &tileSheet, 100, 120, "WARNING MAX VELOCITY REACHED");
		}

		Vec2 cameraPosition = {(float)camera.x, (float)camera.y};
		doEditUI(renderer, &tileSheet, windowWidth, windowHeight, cameraPosition,
		         playerPhys.position, &playerShipData);

		lastFrameNumTicks = SDL_GetPerformanceCounter();
		SDL_RenderPresent(renderer);
		SDL_UpdateWindowSurface(window);
		/* SDL_Delay(c_arbitraryDelayTimeMilliseconds); */
	}

	if (exitReason)
	{
		fprintf(stderr, "Exiting. Reason: %s\n", exitReason);
	}
	SDL_DestroyRenderer(renderer);
	sdlShutdown(window);

	return 0;
}
