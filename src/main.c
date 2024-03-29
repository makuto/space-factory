#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include "SDL.h"

// From Cakelisp
#include "SDL.cake.hpp"
#include "SpaceFactory.cake.hpp"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

//
// SpaceFactory.cake generates these
//

#ifndef NO_DATA_BUNDLE
#ifdef WINDOWS
extern unsigned char* startTilesheetBmp;
extern unsigned char* endTilesheetBmp;
extern unsigned char* startLogoBmp;
extern unsigned char* endLogoBmp;
#else
extern unsigned char _binary_assets_TileSheet_bmp_start;
extern unsigned char _binary_assets_TileSheet_bmp_end;
static unsigned char* startTilesheetBmp = (&_binary_assets_TileSheet_bmp_start);
static unsigned char* endTilesheetBmp = (&_binary_assets_TileSheet_bmp_end);
#endif
#endif

// Math
typedef struct Vec2
{
	float x;
	float y;
} Vec2;

typedef struct IVec2
{
	int x;
	int y;
} IVec2;

bool pointInFRect(Vec2* point, SDL_FRect* rect)
{
	return point->x >= rect->x && point->x < (rect->x + rect->w) && point->y >= rect->y &&
	       point->y < (rect->y + rect->h);
}

//
// Constants
//

const bool enableDeveloperOptions = true;

/* const int c_arbitraryDelayTimeMilliseconds = 10; */
const char c_tileSize = 32;

const int c_fontWidth = 7;
const int c_fontHeight = 10;
// Evidently you cannot actually do this in C
/* static const int c_scaledFontWidth = c_fontWidth * 2; */
const int c_scaledFontWidth = /*c_fontWidth * 2*/14;
/* static const int c_scaledFontHeight = c_fontHeight * 2; */
const int c_scaledFontHeight = /*c_fontHeight * 2*/20;

const float c_typeOutTime = 0.75f;

const float c_simulateUpdateRate = 1.f / 60.f;

// space
const int c_spaceSize = 10000;
const int c_spawnBuffer = /*c_spaceSize / 10*/1000;  // 10% margins

// goal
const int c_goalSize = /*c_tileSize * 5*/160;
const int c_goalMinimapScaleFactor = 2;

// On failure
const float c_timeToShowFailedOverlay = 0.25f;
const float c_timeToShowDamagedText = 3.f;
const unsigned int c_perCellDamageRoll = 30;
const unsigned char c_numSustainableDamagesBeforeGameOver = 2;

// minimap
const int c_miniMapSize = 400;
//
// Camera
//
typedef SDL_Rect Camera;
const float c_cameraEaseFactor = 5.f;
const bool c_enableCameraSmoothing = false;

// Ship
const float c_shipThrust = 300.f;
const float c_maxSpeed = 1500.f;
const float c_defaultStartFuel = 2.f;
const float c_fuelConsumptionRate = 1.f;

// Physics
float playerDrag = 0.f;
const float c_onFailurePlayerDrag = 0.1f;
const float c_objectDrag = 0.05f;

// These force transfer values fake Newton's Third Law of Motion (equal and opposite reactions) by
// using hard-coded values rather than F=MA. This gives us more control over the feel.

// Objects colliding with the ship will receive the player's velocity plus a bit more
const float c_objectForceTransfer = 1.2f;
// This will go straight to reducing player velocity, regardless of the object's velocity. By reducing the player's velocity, we give the asteriods a feeling of weight
const float c_shipObjectForceTransfer = 8.f;

// Factory
const int c_maxFuel = 5;
const unsigned char c_transitionThreshold = 128;
const unsigned char c_conveyorTransitionPerSecond = 250;
const unsigned char c_furnaceTransitionPerSecond = 100;

//
// Factory Cells
//
typedef struct EngineCell
{
	float fuel;
	bool firing;
} EngineCell;

//
// Grid
//

typedef struct GridCell
{
	unsigned char type;
	EngineCell engineCell;
} GridCell;

typedef struct GridSpace
{
	unsigned char width;
	unsigned char height;
	GridCell* data;
} GridSpace;

#define GridCellAt(gridSpace, x, y) ((gridSpace)->data[((y) * (gridSpace)->width) + (x)])

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
	// This is the number of distinct things that can be rendered from the tile sheet
	CharacterSheetCellAssociation associations[17];

	SDL_Texture* texture;
} TileSheet;

bool isEngineTile(unsigned char c)
{
	return c == 'u' || c == 'l' || c == 'r' || c == 'd';
}

static void renderGridSpaceFromTileSheet(SDL_Renderer* renderer, TileSheet* tileSheet,
                                         GridSpace* gridSpace, int originX, int originY,
                                         int cameraX, int cameraY)
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
				int screenX = originX + (cellX * c_tileSize) - cameraX;
				int screenY = originY + (cellY * c_tileSize) - cameraY;
				if (isEngineTile(tileToFind))
				{
					// if this is an engine tile, and its firing, swap the off sprite for the on
					// sprite, and draw the trail
					if (GridCellAt(gridSpace, cellX, cellY).engineCell.firing)
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
					SDL_SetRenderDrawColor(renderer, 102, 138, 158, 255);
					SDL_RenderDrawRect(renderer, &fuelMeterRect);
					float fuelPercentage =
					    (GridCellAt(gridSpace, cellX, cellY).engineCell.fuel) / c_maxFuel;
					if (meterWidth > meterHeight)
					{
						meterWidth *= fuelPercentage;
					}
					else
					{
						meterHeight *= fuelPercentage;
					}

					SDL_Rect fuelRect = {meterPosX + 1, meterPosY + 1, meterWidth, meterHeight};
					SDL_SetRenderDrawColor(renderer, 209, 193, 163, 255);
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
			writeHead->engineCell.fuel = c_defaultStartFuel;
			writeHead->engineCell.firing = false;
		}

		++writeHead;
	}
}

static void renderStarField(SDL_Renderer* renderer, Camera* camera, int windowWidth,
                            int windowHeight)
{
	static SDL_FRect stars[128] = {0};
	static SDL_FRect dynstars[128] = {0};
	static int starsSizeX = 0;
	static int starsSizeY = 0;
	if (starsSizeX != windowWidth || starsSizeY != windowHeight)
	{
		starsSizeX = windowWidth;
		starsSizeY = windowHeight;
		for (int i = 0; i < ARRAY_SIZE(stars); ++i)
		{
			stars[i].x = rand() % starsSizeX;
			stars[i].y = rand() % starsSizeY;
			stars[i].w = rand() % 5 + 1;
			stars[i].h = rand() % 5 + 1;
			dynstars[i].w = stars[i].w;
			dynstars[i].h = stars[i].h;
		}
	}

	for (int i = 0; i < ARRAY_SIZE(stars); ++i)
	{
		dynstars[i].x = stars[i].x - camera->x / 1000;
		dynstars[i].y = stars[i].y - camera->y / 1000;
	}

	SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
	SDL_RenderFillRectsF(renderer, dynstars, ARRAY_SIZE(dynstars));
}

//
// Physics
//

typedef struct RigidBody
{
	Vec2 position;
	Vec2 velocity;
} RigidBody;

bool objHittingGrid(RigidBody* gridPos, GridSpace* gridSheet, RigidBody* objPos)
{
	SDL_FRect playerBoundingBox = {
	    gridPos->position.x,
	    gridPos->position.y,
	    (float)((gridSheet->width) * c_tileSize),
	    (float)((gridSheet->height) * c_tileSize),
	};

	return (pointInFRect(&objPos->position, &playerBoundingBox));
}

// Collision Detection
IVec2 TileCoordinateHit(RigidBody* gridPos, GridSpace* gridSheet, RigidBody* objPos)
{
	assert(objHittingGrid(gridPos, gridSheet, objPos));

	float objX = objPos->position.x;
	float objY = objPos->position.y;
	float gridX = gridPos->position.x;
	float gridY = gridPos->position.y;

	float distToTop = abs(gridY - objY);
	float distToBottom = abs(gridY + gridSheet->height * c_tileSize - objY);
	float distToLeft = abs(gridX - objX);
	float distToRight = abs(gridX + gridSheet->width * c_tileSize - objX);
	int tileX;
	int tileY;
	if (distToTop < distToBottom && distToTop < distToLeft && distToTop < distToRight)
	{
		tileX = (int)(objX - gridX) / c_tileSize;
		tileY = 0;
		assert(tileX < gridSheet->width);
		assert(tileY < gridSheet->height);
	}
	else if (distToBottom < distToTop && distToBottom < distToLeft && distToBottom < distToRight)
	{
		tileX = (int)(objX - gridX) / c_tileSize;
		tileY = gridSheet->height - 1;
		assert(tileX < gridSheet->width);
		assert(tileY < gridSheet->height);
	}
	else if (distToLeft < distToTop && distToLeft < distToBottom && distToLeft < distToRight)
	{
		tileX = 0;
		tileY = (int)(objY - gridPos->position.y) / c_tileSize;
		assert(tileX < gridSheet->width);
		assert(tileY < gridSheet->height);
	}
	else
	{  // must be hitting the right side
		tileX = gridSheet->width - 1;
		tileY = (int)(objY - gridPos->position.y) / c_tileSize;
		assert(tileX < gridSheet->width);
		assert(tileY < gridSheet->height);
	}
	IVec2 result = {tileX, tileY};
	return result;
}

float Magnitude(Vec2* vec)
{
	return sqrt(vec->x * vec->x + vec->y * vec->y);
}

void UpdatePhysics(RigidBody* object, float drag, float dt)
{
	// update via implicit euler integration
	object->velocity.x /= (1.f + (dt * drag));
	object->velocity.y /= (1.f + (dt * drag));

	object->position.x += object->velocity.x * dt;
	object->position.y += object->velocity.y * dt;

	if (object->position.x > c_spaceSize)
		object->position.x = 0;

	if (object->position.x < 0)
		object->position.x = c_spaceSize;

	if (object->position.y > c_spaceSize)
		object->position.y = 0;

	if (object->position.y < 0)
		object->position.y = c_spaceSize;
}

RigidBody SpawnPlayerPhys()
{
	RigidBody player;
	player.position.x = c_spaceSize / 2;
	player.position.y = c_spaceSize / 2;
	player.velocity.x = 0.f;
	player.velocity.y = 0.f;
	return player;
}

void damageShip(GridSpace* gridSpace)
{
	for (int cellY = 0; cellY < gridSpace->height; ++cellY)
	{
		for (int cellX = 0; cellX < gridSpace->width; ++cellX)
		{
			GridCell* currentCell = &GridCellAt(gridSpace, cellX, cellY);
			if (rand() % c_perCellDamageRoll == 1)
			{
				memset(currentCell, 0, sizeof(GridCell));
			}
		}
	}
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
const int c_numObjectsToCreate = 400;

void renderObjects(SDL_Renderer* renderer, TileSheet* tileSheet, Camera* camera,
                   Vec2 extrapolatedPlayerPosition, float extrapolateTime)
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

			Vec2 extrapolatedObjectPosition = currentObject->body.position;
			if (!currentObject->inFactory)
			{
				extrapolatedObjectPosition.x = currentObject->body.position.x +
				                               (currentObject->body.velocity.x * extrapolateTime) -
				                               c_tileSize / 2;

				extrapolatedObjectPosition.y = currentObject->body.position.y +
				                               (currentObject->body.velocity.y * extrapolateTime) -
				                               c_tileSize / 2;
			}
			else
			{
				extrapolatedObjectPosition.x =
				    (currentObject->tileX * c_tileSize) + extrapolatedPlayerPosition.x;
				extrapolatedObjectPosition.y =
				    (currentObject->tileY * c_tileSize) + extrapolatedPlayerPosition.y;
			}

			int textureX = association->column * c_tileSize;
			int textureY = association->row * c_tileSize;
			int screenX = extrapolatedObjectPosition.x - camera->x;
			int screenY = extrapolatedObjectPosition.y - camera->y;
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
			if (tileType == cell->type && cell->engineCell.fuel > 0)
			{
				cell->engineCell.firing = set;
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
			if (isEngineTile(cell->type) && cell->engineCell.firing)
			{
				cell->engineCell.fuel -= c_fuelConsumptionRate * deltaTime;

				if (cell->engineCell.fuel <= 0.f)
				{
					cell->engineCell.fuel = 0.f;
					cell->engineCell.firing = false;
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

static const TransitionDelta c_transitions[] = {{'R', -1, 0}, {'U', 0, 1},  {'D', 0, -1},
                                                {'L', 1, 0},  {'<', -1, 0}, {'V', 0, 1},
                                                {'A', 0, -1}, {'>', 1, 0}};
typedef struct TileDelta
{
	char x;
	char y;
	char conveyor;
} TileDelta;
// Useful to check all cardinal directions of a tile
static const TileDelta c_deltas[] = {{-1, 0, '<'}, {1, 0, '>'}, {0, -1, 'A'}, {0, 1, 'V'}};

// Pick a random outgoing conveyor and move the object onto it
static void conveyorAway(GridSpace* gridSpace, Object* objectToConveyor)
{
	for (int directionIndex = 0; directionIndex < ARRAY_SIZE(c_deltas); ++directionIndex)
	{
		char directionCellX = objectToConveyor->tileX + c_deltas[directionIndex].x;
		char directionCellY = objectToConveyor->tileY + c_deltas[directionIndex].y;

		// Don't allow out of bounds
		if (directionCellX < 0 || directionCellX >= gridSpace->width || directionCellY < 0 ||
		    directionCellY >= gridSpace->height)
			return;

		GridCell* currentCell = &GridCellAt(gridSpace, directionCellX, directionCellY);
		GridCell* cellTo = &GridCellAt(gridSpace, directionCellX, directionCellY);
		// TODO: Hack to "randomly" distribute objects in directions
		if (cellTo->type == c_deltas[directionIndex].conveyor && rand() % 4 == 0)
		{
			objectToConveyor->tileX = directionCellX;
			objectToConveyor->tileY = directionCellY;
			objectToConveyor->transition = 0;
			break;
		}
	}
}

static bool isIntake(char cellType)
{
	static const char intakes[] = {'L', 'R', 'U', 'D'};
	for (int i = 0; i < ARRAY_SIZE(intakes); ++i)
	{
		if (intakes[i] == cellType)
			return true;
	}
	return false;
}

void doFactory(GridSpace* gridSpace, float deltaTime)
{
	for (int cellY = 0; cellY < gridSpace->height; ++cellY)
	{
		for (int cellX = 0; cellX < gridSpace->width; ++cellX)
		{
			GridCell* cell = &GridCellAt(gridSpace, cellX, cellY);
			switch (cell->type)
			{
				// Destroy anything that touches empty spaces. Usually only from ship damage
				case 0:
				{
					for (int i = 0; i < ARRAY_SIZE(objects); ++i)
					{
						Object* currentObject = &objects[i];
						if (!currentObject->type || currentObject->tileX != cellX ||
						    currentObject->tileY != cellY || !currentObject->inFactory)
							continue;
						currentObject->type = 0;
					}
					break;
				}

				case 'L':
				case 'R':
				case 'U':
				case 'D':
				case '<':
				case 'V':
				case 'A':
				case '>':
				{
					for (int i = 0; i < ARRAY_SIZE(objects); ++i)
					{
						Object* currentObject = &objects[i];
						if (!currentObject->type || currentObject->tileX != cellX ||
						    currentObject->tileY != cellY || !currentObject->inFactory)
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
						    currentObject->tileY != cellY || !currentObject->inFactory)
							continue;

						// Move objects along which aren't unrefined the same speed as a conveyor
						if (currentObject->type == 'a')
							currentObject->transition += c_furnaceTransitionPerSecond * deltaTime;
						else
							currentObject->transition += c_conveyorTransitionPerSecond * deltaTime;
						if (currentObject->transition > c_transitionThreshold)
						{
							if (currentObject->type == 'a')
								currentObject->type = 'g';

							conveyorAway(gridSpace, currentObject);
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
						    currentObject->tileY != cellY || !currentObject->inFactory)
							continue;

						// Only refined objects will give fuel; everything else just gets destroyed
						if (currentObject->type == 'g')
							cell->engineCell.fuel += 1.f;
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
	int x = (int)position->x + ((grid->width * c_tileSize) / 2);
	int y = (int)position->y + ((grid->height * c_tileSize) / 2);

	/* if ((camera->x - x - camera->w / 2) > 10 || (camera->y - y - camera->h / 2) > 10) */
	/* { */
	/* 	camera->x = (x - camera->w / 2); */
	/* 	camera->y = (y - camera->h / 2); */
	/* } */

	int cameraOffsetX = (camera->w / 2);
	int cameraOffsetY = (camera->h / 2);
	int goalX = x;
	int goalY = y;
	camera->x += cameraOffsetX;
	camera->y += cameraOffsetY;
	// Ease in
	int deltaX = goalX - camera->x;
	int deltaY = goalY - camera->y;
	if (c_enableCameraSmoothing)
	{
		camera->x += deltaX * c_cameraEaseFactor * deltaTime;
		camera->y += deltaY * c_cameraEaseFactor * deltaTime;
	}
	else
	{
		camera->x += deltaX;
		camera->y += deltaY;
	}
	camera->x -= cameraOffsetX;
	camera->y -= cameraOffsetY;

	/* fprintf(stderr, "%f, %f (player: %f, %f) delta %f %f\n", camera->x, camera->y, position->x,
	 * position->y, deltaX, deltaY); */
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

		if (objHittingGrid(playerPhys, playerShipData, &currentObject->body))
		{
			IVec2 tileCoords = TileCoordinateHit(playerPhys, playerShipData, &currentObject->body);
			unsigned char shipTileX = (unsigned char)tileCoords.x;
			unsigned char shipTileY = (unsigned char)tileCoords.y;
			GridCell cell = GridCellAt(playerShipData, shipTileX, shipTileY);

			if (isIntake(cell.type))
			{
				currentObject->body.position.x = (shipTileX * c_tileSize) + playerPhys->position.x;
				currentObject->body.position.y = (shipTileY * c_tileSize) + playerPhys->position.y;
				currentObject->body.velocity.x = 0;
				currentObject->body.velocity.y = 0;
				currentObject->tileX = shipTileX;
				currentObject->tileY = shipTileY;
				currentObject->inFactory = true;
			}
			else  // collide with an edge of the ship, accounting for momentum
			{
				float* objX = &currentObject->body.position.x;
				float* objY = &currentObject->body.position.y;
				float* objVX = &currentObject->body.velocity.x;
				float* objVY = &currentObject->body.velocity.y;
				float plyVX = playerPhys->velocity.x;
				float plyVY = playerPhys->velocity.y;
				// if the asteroid hits an edge, move it to the outside of the ship,
				// then give it velocity
				if (shipTileX == 0)  // hit left side
				{
					if (*objX > playerPhys->position.x)
						*objX = playerPhys->position.x;

					if (plyVX <= 0)
					{
						*objVX = plyVX * c_objectForceTransfer;
						playerPhys->velocity.x += c_shipObjectForceTransfer;
					}
				}
				else if (shipTileX == playerShipData->width - 1)
				{  // hit right side

					if (*objX < playerPhys->position.x + playerShipData->width * c_tileSize)
						*objX = playerPhys->position.x + playerShipData->width * c_tileSize;

					if (plyVX >= 0)
					{
						*objVX = plyVX * c_objectForceTransfer;
						playerPhys->velocity.x -= c_shipObjectForceTransfer;
					}
				}
				else if (shipTileY == 0)
				{  // hit top
					if (*objY > playerPhys->position.y)
						*objY = playerPhys->position.y;
					*objY = playerPhys->position.y;
					if (plyVY <= 0)
					{
						*objVY = plyVY * c_objectForceTransfer;
						playerPhys->velocity.y += c_shipObjectForceTransfer;
					}
				}
				else if (shipTileY == playerShipData->height - 1)
				{  // hit bottom
					if (*objY < playerPhys->position.y + playerShipData->height * c_tileSize)
						*objY = playerPhys->position.y + playerShipData->height * c_tileSize;
					if (plyVY >= 0)
					{
						*objVY = plyVY * c_objectForceTransfer;
						playerPhys->velocity.y -= c_shipObjectForceTransfer;
					}
				}
			}
		};
	}
}

//
// Ship editing
//

static GridCell* pickGridCellFromWorldSpace(Vec2 gridSpaceWorldPosition, GridSpace* searchGridSpace,
                                            IVec2 pickWorldPosition,
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

static void renderText(SDL_Renderer* renderer, TileSheet* tileSheet, int x, int y, const char* text)
{
	const int c_fontStartX = 0;
	const int c_fontStartY = 99;
	const int c_numberStartX = 56;
	const int c_numberStartY = 114;
	const int c_charactersPerRow = 18;
	const int c_fontVerticalSpace = 5;
	int currentX = 0;
	int currentY = 0;
	for (const char* read = text; *read; ++read)
	{
		if (*read == '\n')
		{
			currentY += c_scaledFontHeight + c_fontVerticalSpace;
			currentX = 0;
			continue;
		}
		// Only uppercase
		bool isText = (*read >= 'A' && *read <= 'Z');
		bool isNumber = (*read >= '0' && *read <= '9');
		if (!isText && !isNumber)
		{
			currentX += c_scaledFontWidth;
			continue;
		}

		char index = *read - (isText ? 'A' : '0');
		int textureX = 0;
		if (isText)
			textureX = c_fontStartX + ((index % c_charactersPerRow) * c_fontWidth);
		else
			textureX = c_numberStartX + (index * c_fontWidth);
		int textureY = (isText ? c_fontStartY : c_numberStartX) +
		               ((index / c_charactersPerRow) * (c_fontHeight + c_fontVerticalSpace));
		if (isText)
			textureY = c_fontStartY +
			           ((index / c_charactersPerRow) * (c_fontHeight + c_fontVerticalSpace));
		else
			textureY = c_numberStartY;
		int screenX = currentX + x;
		int screenY = currentY + y;
		SDL_Rect sourceRectangle = {textureX, textureY, c_fontWidth, c_fontHeight};
		SDL_Rect destinationRectangle = {screenX, screenY, c_scaledFontWidth, c_scaledFontHeight};
		SDL_RenderCopy(renderer, tileSheet->texture, &sourceRectangle, &destinationRectangle);

		currentX += c_scaledFontWidth;
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
		int screenX = x + ((read - numberBuffer) * c_scaledFontWidth);
		int screenY = y;
		SDL_Rect sourceRectangle = {textureX, textureY, c_fontWidth, c_fontHeight};
		SDL_Rect destinationRectangle = {screenX, screenY, c_scaledFontWidth, c_scaledFontHeight};
		SDL_RenderCopy(renderer, tileSheet->texture, &sourceRectangle, &destinationRectangle);
	}
}

static void doEditUI(SDL_Renderer* renderer, TileSheet* tileSheet, int windowWidth,
                     int windowHeight, IVec2 cameraPosition, Vec2 gridSpaceWorldPosition,
                     GridSpace* editGridSpace, unsigned short* inventory, int inventorySize,
                     float* fuelPool)
{
	int mouseX = 0;
	int mouseY = 0;
	Uint32 mouseButtonState = SDL_GetMouseState(&mouseX, &mouseY);
	static char currentSelectedButtonIndex = 0;
	char editButtons[] = {'#', '.', '<', '>', 'A', 'V', 'f', 'L',
	                      'R', 'U', 'D', 'l', 'r', 'u', 'd'};
	const char* editButtonLabels[] = {"WALL", "FLOOR", "CONVEYOR LEFT", "CONVEYOR RIGHT",
	                                  "CONVEYOR UP", "CONVEYOR DOWN", "REFINERY",
	                                  // Intakes
	                                  "INTAKE LEFT", "INTAKE RIGHT", "INTAKE UP", "INTAKE DOWN",
	                                  // Engines
	                                  "ENGINE LEFT", "ENGINE RIGHT", "ENGINE UP", "ENGINE DOWN"};
	typedef enum PlacementRestriction
	{
		Restrict_None,
		Restrict_Inside,
		Restrict_EdgeAny,
		Restrict_EdgeLeft,
		Restrict_EdgeRight,
		Restrict_EdgeTop,
		Restrict_EdgeBottom,
	} PlacementRestriction;
	static const char* restrictionExplanation[] = {
	    /*Restrict_None=*/
	    "",
	    /*Restrict_Inside=*/
	    "MUST PLACE INSIDE",
	    /*Restrict_EdgeAny=*/
	    "MUST PLACE ON EDGE",
	    /*Restrict_EdgeLeft=*/
	    "MUST PLACE ON LEFT EDGE",
	    /*Restrict_EdgeRight=*/
	    "MUST PLACE ON RIGHT EDGE",
	    /*Restrict_EdgeTop=*/
	    "MUST PLACE ON TOP EDGE",
	    /*Restrict_EdgeBottom=*/"MUST PLACE ON BOTTOM EDGE"};
	static PlacementRestriction restrictions[] = {
	    /*WALL*/ Restrict_EdgeAny, /*FLOOR*/ Restrict_Inside, /*CONVEYOR LEFT*/ Restrict_Inside,
	    /*CONVEYOR RIGHT*/ Restrict_Inside,
	    /*CONVEYOR UP*/ Restrict_Inside, /*CONVEYOR DOWN*/ Restrict_Inside,
	    /*REFINERY*/ Restrict_Inside,
	    // Intakes
	    /*INTAKE LEFT*/ Restrict_EdgeLeft, /*INTAKE RIGHT*/ Restrict_EdgeRight,
	    /*INTAKE UP*/ Restrict_EdgeTop, /*INTAKE DOWN*/ Restrict_EdgeBottom,
	    // Engines
	    /*ENGINE LEFT*/ Restrict_EdgeLeft, /*ENGINE RIGHT*/ Restrict_EdgeRight,
	    /*ENGINE UP*/ Restrict_EdgeBottom, /*ENGINE DOWN*/ Restrict_EdgeTop};

	assert(inventorySize == ARRAY_SIZE(editButtons) &&
	       ARRAY_SIZE(editButtonLabels) == ARRAY_SIZE(editButtons) &&
	       ARRAY_SIZE(restrictions) == ARRAY_SIZE(editButtons) &&
	       "An array for the edit UI has gotten out of sync. Check that it has the same number of "
	       "elements as editButtons.");

	const int c_buttonMarginX = 22;
	int startButtonBarX =
	    ((windowWidth / 2) - ((ARRAY_SIZE(editButtons) * (c_tileSize + c_buttonMarginX)) / 2));
	const int buttonBarY = 32;
	const int c_numberMargin = 8;
	const int c_toolTipMargin = 28;

	renderText(renderer, tileSheet, startButtonBarX, buttonBarY - 25, "INVENTORY");

	renderText(renderer, tileSheet, startButtonBarX + 475, buttonBarY - 25, "FUEL IN RESERVE");
	renderNumber(renderer, tileSheet, startButtonBarX + 700, buttonBarY - 25,
	             (unsigned int)(*fuelPool * 10.f));

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
			int screenX = startButtonBarX + (buttonIndex * (c_tileSize + c_buttonMarginX));
			int screenY = buttonBarY;
			SDL_Rect sourceRectangle = {textureX, textureY, c_tileSize, c_tileSize};
			SDL_Rect destinationRectangle = {screenX, screenY, c_tileSize, c_tileSize};

			if (mouseX > destinationRectangle.x - (c_buttonMarginX / 2) &&
			    mouseX <= destinationRectangle.x + destinationRectangle.w + (c_buttonMarginX / 2) &&
			    mouseY >= destinationRectangle.y &&
			    mouseY <= destinationRectangle.y + destinationRectangle.h)
			{
				if (mouseButtonState & SDL_BUTTON_LMASK)
					currentSelectedButtonIndex = buttonIndex;

				drawOutlineRectangle(renderer, &destinationRectangle, mouseButtonState);

				renderText(renderer, tileSheet, screenX, screenY + c_tileSize + c_toolTipMargin,
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

			renderNumber(renderer, tileSheet, screenX, screenY + c_tileSize + c_numberMargin,
			             inventory[buttonIndex]);
			break;
		}
	}

	IVec2 pickWorldPosition = {mouseX + cameraPosition.x, mouseY + cameraPosition.y};
	unsigned char selectedCellX = 0;
	unsigned char selectedCellY = 0;
	GridCell* selectedCell = pickGridCellFromWorldSpace(
	    gridSpaceWorldPosition, editGridSpace, pickWorldPosition, &selectedCellX, &selectedCellY);
	if (selectedCell)
	{
		bool isValidPlacement = true;
		{
			switch (restrictions[currentSelectedButtonIndex])
			{
				case Restrict_None:
					break;
				case Restrict_Inside:
					if (selectedCellX == 0 || selectedCellX == editGridSpace->width - 1 ||
					    selectedCellY == 0 || selectedCellY == editGridSpace->height - 1)
						isValidPlacement = false;
					break;
				case Restrict_EdgeAny:
					if ((selectedCellX != 0 && selectedCellX != editGridSpace->width - 1) &&
					    (selectedCellY != 0 && selectedCellY != editGridSpace->height - 1))
						isValidPlacement = false;
					break;
				case Restrict_EdgeLeft:
					if (selectedCellX != 0)
						isValidPlacement = false;
					break;
				case Restrict_EdgeRight:
					if (selectedCellX != editGridSpace->width - 1)
						isValidPlacement = false;
					break;
				case Restrict_EdgeTop:
					if (selectedCellY != 0)
						isValidPlacement = false;
					break;
				case Restrict_EdgeBottom:
					if (selectedCellY != editGridSpace->height - 1)
						isValidPlacement = false;
					break;
			}

			if (inventory[currentSelectedButtonIndex] == 0)
				isValidPlacement = false;
		}

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

			if (!isValidPlacement)
			{
				SDL_SetRenderDrawColor(renderer, 245, 15, 15, 255);
				SDL_RenderDrawRect(renderer, &destinationRectangle);
				const char* explanation =
				    restrictionExplanation[restrictions[currentSelectedButtonIndex]];
				if (inventory[currentSelectedButtonIndex] == 0)
					explanation = "NONE LEFT";
				renderText(renderer, tileSheet, screenX, screenY + c_tileSize, explanation);
			}
			else
			{
				drawOutlineRectangle(renderer, &destinationRectangle, mouseButtonState);

				SDL_RenderCopyEx(renderer, tileSheet->texture, &sourceRectangle,
				                 &destinationRectangle,
				                 c_transformsToAngles[association->transform],
				                 /*rotate about (default = center)*/ NULL,
				                 c_transformsToSDLRenderFlips[association->transform]);
			}
			break;
		}

		if (isValidPlacement && mouseButtonState & SDL_BUTTON_LMASK &&
		    inventory[currentSelectedButtonIndex] &&
		    selectedCell->type != editButtons[currentSelectedButtonIndex])
		{
			// Give back resources
			for (int buttonIndex = 0; buttonIndex < ARRAY_SIZE(editButtons); ++buttonIndex)
			{
				if (selectedCell->type == editButtons[buttonIndex])
				{
					inventory[buttonIndex] += 1;
					if (isEngineTile(selectedCell->type))
						*fuelPool += selectedCell->engineCell.fuel;
					break;
				}
			}

			// Make the placement
			inventory[currentSelectedButtonIndex] -= 1;
			memset(selectedCell, 0, sizeof(GridCell));
			selectedCell->type = editButtons[currentSelectedButtonIndex];
			if (isEngineTile(selectedCell->type))
			{
				float fuelToAdd = *fuelPool >= c_defaultStartFuel ? c_defaultStartFuel : *fuelPool;
				if (fuelToAdd > 0.f)
				{
					selectedCell->engineCell.fuel = fuelToAdd;
					*fuelPool -= fuelToAdd;
				}
			}
		}
	}
}

static void renderMainMenu(SDL_Renderer* renderer, TileSheet* tileSheet, SDL_Texture* logoTexture)
{
	int windowWidth;
	int windowHeight;
	SDL_GetRendererOutputSize(renderer, &windowWidth, &windowHeight);

	const int c_logoWidth = 99;
	const int c_logoHeight = 57;
	SDL_Rect sourceRectangle = {0, 0, c_logoWidth, c_logoHeight};
	SDL_Rect destinationRectangle = {(windowWidth / 2) - ((c_logoWidth * 12) / 2), 100,
	                                 c_logoWidth * 12, c_logoHeight * 12};
	SDL_RenderCopy(renderer, logoTexture, &sourceRectangle, &destinationRectangle);

	int currentY = 800;
	const char* text = "A GAME MADE IN 8 DAYS BY";
	renderText(renderer, tileSheet, (windowWidth / 2) - ((strlen(text) * c_scaledFontWidth) / 2),
	           currentY, text);

	currentY += c_scaledFontHeight + 20;
	text = "MACOY MADSON    WILL CHAMBERS";
	renderText(renderer, tileSheet, (windowWidth / 2) - ((strlen(text) * c_scaledFontWidth) / 2),
	           currentY, text);

	currentY += c_scaledFontHeight + 50;
	text = "PRESS SPACE TO PLAY";
	renderText(renderer, tileSheet, (windowWidth / 2) - ((strlen(text) * c_scaledFontWidth) / 2),
	           currentY, text);

	currentY += c_scaledFontHeight + 50;
	text = "COPYRIGHT 2022  AVAILABLE UNDER TERMS OF GNU GENERAL PUBLIC LICENSE VERSION 3";
	renderText(renderer, tileSheet, (windowWidth / 2) - ((strlen(text) * c_scaledFontWidth) / 2),
	           currentY, text);
}

static void doTutorial(SDL_Renderer* renderer, TileSheet* tileSheet, int page)
{
	// No sci-fi game is complete without some cheese
	const char* tutorialText[] = {
	    "FREEDOM, CURIOSITY, SHARED DESTINY:\n"
	    "THE CONGLOMERATE IS NOT HAPPY WITH YOU SPREADING THESE IDEALS\n\n"
	    "THEIR SHIP IS TRACKING YOU\n\n\n"
	    "PRESS THE SPACE KEY TO CONTINUE",
	    "YOU MUST REFINE ASTEROIDS INTO FUEL\n"
	    "FUEL YOUR ENGINES TO MANEUVER THE SHIP\n\n"
	    "CUSTOMIZE YOUR FACTORY TO MAXIMIZE EFFICIENCY\n\n\n"
	    "PRESS THE SPACE KEY TO CONTINUE",
	    "EVERY SECOND COUNTS\n"
	    "REACH THE TARGET LOCATIONS IN TIME TO AVOID DETECTION\n\n"
	    "YOUR CREW DEPENDS ON YOU\n\n\n"
	    "PRESS THE SPACE KEY TO CONTINUE",
	};
	renderText(renderer, tileSheet, 200, 200, tutorialText[page]);
}

static void doEndScreenFailure(SDL_Renderer* renderer, TileSheet* tileSheet)
{
	const char* endScreenFailure =
	    "YOUR SHIP HAS BEEN DESTROYED\n\n"
	    "YOU AND YOUR CREW DRIFT INTO EMPTY SPACE\n\n"
	    "BUT YOU KNOW THIS IS ONLY THE BEGINNING\n"
	    "\n\n\n\n"
	    "THANK YOU FOR PLAYING\n\n"
	    "PRESS SPACE TO PLAY AGAIN\n\n\n\n"
	    "CREATED BY\n"
	    "MACOY MADSON\n"
	    "WILL CHAMBERS\n\n"
	    "COPYRIGHT 2022\n"
	    "AVAILABLE UNDER TERMS OF GNU GENERAL PUBLIC LICENSE VERSION 3\n";
	renderText(renderer, tileSheet, 400, 200, endScreenFailure);
}

static void doEndScreenSuccess(SDL_Renderer* renderer, TileSheet* tileSheet)
{
	const char* endScreenSuccess =
	    "YOU SUCCESSFULLY AVOIDED DESTRUCTION\n\n"
	    "YOUR EXHAUSTED CREW CELEBRATES\n\n"
	    "BUT YOU KNOW THIS IS ONLY THE BEGINNING\n"
	    "\n\n\n\n"
	    "THANK YOU FOR PLAYING\n\n"
	    "PRESS SPACE TO PLAY AGAIN\n\n\n\n"
	    "CREATED BY\n"
	    "MACOY MADSON\n"
	    "WILL CHAMBERS\n\n"
	    "COPYRIGHT 2022\n"
	    "AVAILABLE UNDER TERMS OF GNU GENERAL PUBLIC LICENSE VERSION 3\n";
	renderText(renderer, tileSheet, 200, 200, endScreenSuccess);
}

// Goal
// for now just a simple rect
//

typedef SDL_Rect Goal;

static void renderGoal(SDL_Renderer* renderer, Camera* camera, Goal* goal)
{
	SDL_SetRenderDrawColor(renderer, 84, 211, 115, 155);
	SDL_Rect goalVis = {(int)(goal->x - camera->x), (int)((float)goal->y - camera->y), (int)goal->w,
	                    (int)goal->h};
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_RenderFillRect(renderer, &goalVis);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
}

bool pointInRect(Vec2* point, SDL_Rect* rect)
{
	return point->x > rect->x && point->x < (rect->x + rect->w) && point->y > rect->y &&
	       point->y < (rect->y + rect->h);
}

bool CheckGoalSatisfied(Vec2* playerPos, GridSpace* playerShip, Goal* goal)
{
	// aligned box collision WILL BREAK IF WE EVER ROTATE ANYTHING
	Vec2 goalTL = {(float)goal->x, (float)goal->y};
	Vec2 goalTR = {(float)(goal->x + goal->w), (float)goal->y};
	Vec2 goalBL = {(float)goal->x, (float)(goal->y + goal->h)};
	Vec2 goalBR = {(float)(goal->x + goal->w), (float)(goal->y + goal->h)};

	int playerWidth = playerShip->width * c_tileSize;
	int playerHeight = playerShip->height * c_tileSize;

	Vec2 playerTR = {(playerPos->x + playerWidth), (float)playerPos->y};
	Vec2 playerBL = {(float)playerPos->x, (float)(playerPos->y + playerHeight)};
	Vec2 playerBR = {(float)(playerPos->x + playerWidth), (float)(playerPos->y + playerHeight)};

	SDL_Rect playerBoundingBox = {(int)playerPos->x, (int)playerPos->y, playerWidth, playerHeight};

	return pointInRect(&goalTL, &playerBoundingBox) || pointInRect(&goalTR, &playerBoundingBox) ||
	       pointInRect(&goalBL, &playerBoundingBox) || pointInRect(&goalBR, &playerBoundingBox) ||
	       pointInRect(playerPos, goal) || pointInRect(&playerTR, goal) ||
	       pointInRect(&playerBL, goal) || pointInRect(&playerBR, goal);
}

IVec2 toMiniMapCoordinates(float worldCoordX, float worldCoordY)
{
	float nWorldCoordX = worldCoordX / c_spaceSize;
	float nWorldCoordY = worldCoordY / c_spaceSize;
	IVec2 result = {(int)(nWorldCoordX * c_miniMapSize), (int)(nWorldCoordY * c_miniMapSize)};
	return result;
}

SDL_Rect scaleRectToMinimap(float x, float y, float w, float h)
{
	IVec2 miniMapXY = toMiniMapCoordinates(x, y);
	IVec2 miniMapWH = toMiniMapCoordinates(w, h);

	if (miniMapWH.x == 0)
		miniMapWH.x = 1;

	if (miniMapWH.y == 0)
		miniMapWH.y = 1;

	SDL_Rect result = {miniMapXY.x, miniMapXY.y, miniMapWH.x, miniMapWH.y};
	return result;
}

void renderMiniMap(SDL_Renderer* renderer, int windowWidth, int windowHeight, Vec2* playerPos,
                   GridSpace* playerShip, Goal* goal)
{
	const int miniMapMargin = 10;
	int miniMapX = windowWidth - c_miniMapSize - miniMapMargin;
	int miniMapY = windowHeight - c_miniMapSize - miniMapMargin;

	SDL_Rect miniPlayer =
	    scaleRectToMinimap(playerPos->x, playerPos->y, playerShip->width * c_tileSize,
	                       playerShip->height * c_tileSize);

	miniPlayer.x += miniMapX;
	miniPlayer.y += miniMapY;

	SDL_Rect miniMapBounds = {miniMapX, miniMapY, c_miniMapSize, c_miniMapSize};
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderFillRect(renderer, &miniMapBounds);
	SDL_SetRenderDrawColor(renderer, 82, 74, 63, 255);
	SDL_RenderDrawRect(renderer, &miniMapBounds);

	SDL_SetRenderDrawColor(renderer, 184, 98, 76, 255);
	SDL_RenderFillRect(renderer, &miniPlayer);

	if (goal)
	{
		SDL_Rect miniGoal = scaleRectToMinimap(goal->x, goal->y, goal->w * c_goalMinimapScaleFactor,
		                                       goal->h * c_goalMinimapScaleFactor);
		miniGoal.x += miniMapX;
		miniGoal.y += miniMapY;
		SDL_SetRenderDrawColor(renderer, 84, 211, 115, 255);
		SDL_RenderFillRect(renderer, &miniGoal);
	}

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

	for (int i = 0; i < ARRAY_SIZE(objects); i++)
	{
		Object* currentObject = &objects[i];
		if (!currentObject->type)
			continue;
		IVec2 miniMapObjPos =
		    toMiniMapCoordinates(currentObject->body.position.x, currentObject->body.position.y);
		miniMapObjPos.x += miniMapX;
		miniMapObjPos.y += miniMapY;
		SDL_Rect miniObj = {miniMapObjPos.x, miniMapObjPos.y, 4, 4};
		SDL_SetRenderDrawColor(renderer, 102, 138, 158, 255);
		SDL_RenderFillRect(renderer, &miniObj);
	}
}

static void renderFactoryGuide(SDL_Renderer* renderer, TileSheet* tileSheet)
{
	// Show a guide for ship construction
	GridSpace tutorialGrid = {0};
	tutorialGrid.width = 5;
	tutorialGrid.height = 3;
	// Max health
	GridCell tutorialGridCells[5 * 3] = {0};
	tutorialGrid.data = tutorialGridCells;

	int currentY = 650;
	const int addMargin = 20;
	renderText(renderer, tileSheet, 100, currentY, "USE THE MOUSE TO EDIT SHIP");
	currentY += 40;
	renderText(renderer, tileSheet, 100, currentY, "INTAKES MOVE ASTEROIDS THEY TOUCH INSIDE\n");
	tutorialGridCells[0].type = 'a';
	tutorialGridCells[1].type = 'L';
	tutorialGridCells[2].type = '>';
	renderGridSpaceFromTileSheet(renderer, tileSheet, &tutorialGrid, 120, currentY + 20 + 7, 0, 0);
	currentY += 20 + 32 + addMargin;
	renderText(renderer, tileSheet, 100, currentY, "FURNACES REFINE ASTEROIDS INTO FUEL\n");
	memset(tutorialGridCells, 0, sizeof(tutorialGridCells));
	/* tutorialGridCells[0].type = '>'; */
	/* tutorialGridCells[1].type = 'f'; */
	/* tutorialGridCells[2].type = '>'; */

	tutorialGridCells[0].type = 'a';
	tutorialGridCells[1].type = '>';
	tutorialGridCells[2].type = 'f';
	tutorialGridCells[3].type = '>';
	tutorialGridCells[4].type = 'g';
	renderGridSpaceFromTileSheet(renderer, tileSheet, &tutorialGrid, 120, currentY + 20 + 7, 0, 0);
	currentY += 20 + 32 + addMargin;
	renderText(renderer, tileSheet, 100, currentY,
	           "FURNACES OUTPUT TO RANDOM ADJACENT OUTGOING CONVEYORS\n");
	memset(tutorialGridCells, 0, sizeof(tutorialGridCells));
	GridCellAt(&tutorialGrid, 1, 1).type = 'f';
	GridCellAt(&tutorialGrid, 0, 1).type = '>';
	GridCellAt(&tutorialGrid, 1, 0).type = 'V';
	GridCellAt(&tutorialGrid, 1, 2).type = 'V';
	GridCellAt(&tutorialGrid, 2, 1).type = '>';
	renderGridSpaceFromTileSheet(renderer, tileSheet, &tutorialGrid, 120, currentY + 20 + 7, 0, 0);
	currentY += 20 + (32 * 3) + addMargin;
	renderText(renderer, tileSheet, 100, currentY, "ENGINES ONLY ACCEPT REFINED FUEL\n");
	memset(tutorialGridCells, 0, sizeof(tutorialGridCells));
	tutorialGridCells[0].type = 'L';
	tutorialGridCells[1].type = '>';
	tutorialGridCells[2].type = 'f';
	tutorialGridCells[3].type = '>';
	tutorialGridCells[4].type = 'r';
	renderGridSpaceFromTileSheet(renderer, tileSheet, &tutorialGrid, 120, currentY + 20 + 7, 0, 0);
}

//
//
// Main
//

static bool continuePressed()
{
	const Uint8* currentKeyStates = SDL_GetKeyboardState(NULL);
	if (currentKeyStates[SDL_SCANCODE_SPACE])
		return true;
	return false;
}

// Returns whether the game should be played
bool doMainMenu(SDL_Window* window, SDL_Renderer* renderer, TileSheet* tileSheet)
{
	SDL_Texture* logoTexture = NULL;
	{
#define NO_DATA_BUNDLE
#ifdef NO_DATA_BUNDLE
		SDL_Surface* logoSurface = SDL_LoadBMP("assets/SpaceFactoryLogo.bmp");
#else
		SDL_RWops* logoRWOps = SDL_RWFromMem(startLogoBmp, endLogoBmp - startLogoBmp);
		SDL_Surface* logoSurface = SDL_LoadBMP_RW(logoRWOps, /*freesrc=*/1);
#endif
		if (!logoSurface)
		{
			fprintf(stderr, "Failed to load logo\n");
			return 1;
		}
		// Use pure black as our chroma key
		SDL_SetColorKey(logoSurface, SDL_TRUE, SDL_MapRGB(logoSurface->format, 0, 0, 0));
		logoTexture = SDL_CreateTextureFromSurface(renderer, logoSurface);
		SDL_FreeSurface(logoSurface);
		if (!logoTexture)
		{
			sdlPrintError();
			return 1;
		}
	}

	bool spaceDown = false;
	bool leftMouseButtonDown = false;
	char state = 0;
	while (1)
	{
		SDL_Event event;
		while (SDL_PollEvent((&event)))
		{
			if ((event.type == SDL_QUIT))
			{
				return false;
			}
		}

		const Uint8* currentKeyStates = SDL_GetKeyboardState(NULL);
		if (currentKeyStates[SDL_SCANCODE_ESCAPE])
			return false;

		if (currentKeyStates[SDL_SCANCODE_SPACE])
		{
			if (!spaceDown)
				++state;
			spaceDown = true;
		}
		else
			spaceDown = false;

		int mouseX = 0;
		int mouseY = 0;
		Uint32 mouseButtonState = SDL_GetMouseState(&mouseX, &mouseY);
		if (mouseButtonState & SDL_BUTTON_LMASK)
		{
			if (!leftMouseButtonDown)
				++state;
			leftMouseButtonDown = true;
		}
		else
			leftMouseButtonDown = false;

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		switch (state)
		{
			case 0:
				renderMainMenu(renderer, tileSheet, logoTexture);
				break;
			case 1:
				doTutorial(renderer, tileSheet, 0);
				break;
			case 2:
				doTutorial(renderer, tileSheet, 1);
				break;
			case 3:
				doTutorial(renderer, tileSheet, 2);
				break;
			case 4:
				return true;
				break;
		}

		SDL_RenderPresent(renderer);
		SDL_UpdateWindowSurface(window);
	}
	return false;
}

static void addRenderDiagnostics(SDL_Renderer* renderer, float deltaTime,
                                 int numSimulationUpdatesThisFrame)
{
#define NUM_FRAME_TIMES 256
	// Not actually used, only the points are used currently
	static float s_frameTimes[NUM_FRAME_TIMES] = {0};
	static SDL_FPoint s_frameRateGraph[NUM_FRAME_TIMES] = {0};
	static SDL_FPoint s_simulationUpdateGraph[NUM_FRAME_TIMES] = {0};
	static int writeTimeIndex = 0;

	const int marginX = 5;
	const int graphWidth = 512;
	const int graphHeight = 512;

	s_frameTimes[writeTimeIndex] = deltaTime;
	s_frameRateGraph[writeTimeIndex].x =
	    (writeTimeIndex * (graphWidth / ARRAY_SIZE(s_frameTimes))) + marginX;
	/* s_frameRateGraph[writeTimeIndex].y = (deltaTime / (1.f / 60.f)) * graphHeight; */
	s_frameRateGraph[writeTimeIndex].y = (deltaTime * graphHeight) / (1.f / 60.f);
	s_simulationUpdateGraph[writeTimeIndex].x =
	    (writeTimeIndex * (graphWidth / ARRAY_SIZE(s_frameTimes))) + marginX;
	s_simulationUpdateGraph[writeTimeIndex].y = ((numSimulationUpdatesThisFrame * graphHeight) / 5);
	++writeTimeIndex;
	if (writeTimeIndex >= ARRAY_SIZE(s_frameTimes))
		writeTimeIndex = 0;

	SDL_SetRenderDrawColor(renderer, 10, 240, 10, 255);
	SDL_RenderDrawPointsF(renderer, s_frameRateGraph, ARRAY_SIZE(s_frameRateGraph));

	SDL_SetRenderDrawColor(renderer, 10, 240, 240, 255);
	SDL_RenderDrawPointsF(renderer, s_simulationUpdateGraph, ARRAY_SIZE(s_simulationUpdateGraph));

	SDL_SetRenderDrawColor(renderer, 240, 10, 10, 255);
	const SDL_Point sixtyHertzLine[] = {{marginX, graphHeight},
	                                    {graphWidth + marginX, graphHeight}};
	SDL_RenderDrawLines(renderer, sixtyHertzLine, ARRAY_SIZE(sixtyHertzLine));
}

typedef enum GameplayResult
{
	GameplayResult_ExitGame,
	GameplayResult_StartNewGame,
} GameplayResult;

GameplayResult doGameplay(SDL_Window* window, SDL_Renderer* renderer, TileSheet tileSheet)
{
	int windowWidth;
	int windowHeight;
	SDL_GetRendererOutputSize(renderer, &windowWidth, &windowHeight);

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
		                       "l<<<<<<f<<<<<<<<<R"
		                       "l<<<<<<<<<<f<<<<<R"
		                       "#..........V.....#"
		                       "#..........>>>>>>r"
		                       "#######u##########");

		renderGridSpaceText(playerShip);
	}
	RigidBody playerPhys = SpawnPlayerPhys();
	// snap the camera to the player postion
	Camera camera;
	camera.x = playerPhys.position.x - (windowWidth / 2) + (playerShipData.width * c_tileSize) / 2;
	camera.y =
	    playerPhys.position.y - (windowHeight / 2) + (playerShipData.height * c_tileSize) / 2;
	camera.w = windowWidth;
	camera.h = windowHeight;

	srand(time(NULL));

	Goal goal;
	goal.x = rand() % c_spaceSize;
	goal.y = rand() % c_spaceSize;
	goal.w = c_goalSize;
	goal.h = c_goalSize;

	typedef enum Objective
	{
		Objective_None,
		// This does some weird stuff like hide the HUD, so use with caution
		Objective_ShipConstruct,
		Objective_ReachGoalPoint,
	} Objective;
	typedef struct GamePhase
	{
		const char* prompt;
		int timeToCompleteSeconds;
		Objective objective;
	} GamePhase;
	GamePhase gamePhases[] = {
	    {"CONSTRUCT YOUR SHIP", 60 + 30, Objective_ShipConstruct},
	    {"ENEMY RADAR SIGNAL DETECTED", 10, Objective_None},
	    {"REACH GREEN AREA 1", 30, Objective_ReachGoalPoint},
	    {"REACH GREEN AREA 2", 25, Objective_ReachGoalPoint},
	    {"ENEMY RADAR IN COOLDOWN", 5, Objective_None},
	    {"REFIT YOUR SHIP", 30, Objective_None},
	    {"REACH GREEN AREA 3", 15, Objective_ReachGoalPoint},
	    {"REACH GREEN AREA 4", 10, Objective_ReachGoalPoint},
	    {"REACH GREEN AREA 5", 5, Objective_ReachGoalPoint},
	};
	int currentGamePhase = 0;
	const Uint64 performanceNumTicksPerSecond = SDL_GetPerformanceFrequency();
	Uint64 gamePhaseStartTicks = SDL_GetPerformanceCounter();
	int startPhaseSeconds = gamePhaseStartTicks / performanceNumTicksPerSecond;
	unsigned char numDamagesSustained = 0;
	float timeSinceFailedPhase = 0.f;
	float timeSinceFailedPhaseDamage = 0.f;

	// Make some objects
	memset(objects, 0, sizeof(objects));
	for (int i = 0; i < c_numObjectsToCreate; ++i)
	{
		Object* object = &objects[i];
		object->type = 'a';
		object->body.position.x = (float)(rand() % c_spaceSize);
		object->body.position.y = (float)(rand() % c_spaceSize);
		object->body.velocity.x = (float)((rand() % 50) - 25);
		object->body.velocity.y = (float)((rand() % 50) - 25);
	}

	// Player inventory (MUST MATCH size of editor buttons)
	unsigned short inventory[] = {/*'#'=*/100, /*'.'=*/999, /*'<'=*/100, /*'>'=*/100,
	                              /*'A'=*/100, /*'V'=*/100, /*'f'=*/8,   /*'L'=*/8,
	                              /*'R'=*/8,   /*'U'=*/8,   /*'D'=*/8,
	                              /*'l'=*/4,   /*'r'=*/4,   /*'u'=*/4,   /*'d'=*/4};

	// Enough to fuel two engines per side
	float constructionFuelPool = 4 * 2 * c_defaultStartFuel;

	// Main loop
	bool enableDebugUI = false;
	bool isPhaseSkipPressed = false;
	float accumulatedTime = 0.f;
	float startPromptTimeToTypeOut = 0.f;
	Uint64 lastFrameNumTicks = SDL_GetPerformanceCounter();
	const char* exitReason = NULL;
	while (!(exitReason))
	{
		Uint64 currentCounterTicks = SDL_GetPerformanceCounter();
		Uint64 frameDiffTicks = (currentCounterTicks - lastFrameNumTicks);
		lastFrameNumTicks = currentCounterTicks;
		float deltaTime = (frameDiffTicks / ((float)performanceNumTicksPerSecond));
		accumulatedTime += deltaTime;

		SDL_Event event;
		while (SDL_PollEvent((&event)))
		{
			if ((event.type == SDL_QUIT))
			{
				exitReason = "Window event";
			}
		}
		SDL_GetRendererOutputSize(renderer, &windowWidth, &windowHeight);
		camera.w = windowWidth;
		camera.h = windowHeight;
		const Uint8* currentKeyStates = SDL_GetKeyboardState(NULL);
		if (currentKeyStates[SDL_SCANCODE_ESCAPE])
		{
			exitReason = "Escape pressed";
		}

		// Developer options
		bool forceStartNewPhase = false;
		if (enableDeveloperOptions)
		{
			if (currentKeyStates[SDL_SCANCODE_F1])
				enableDebugUI = true;
			// Advance to next phase
			if (currentKeyStates[SDL_SCANCODE_F2])
			{
				if (!isPhaseSkipPressed)
					forceStartNewPhase = true;
				isPhaseSkipPressed = true;
			}
			else
				isPhaseSkipPressed = false;
		}

		int numSimulationUpdatesThisFrame = 0;
		/* accumulatedTime = c_simulateUpdateRate;// Fixed update */
		while (accumulatedTime >= c_simulateUpdateRate)
		{
			++numSimulationUpdatesThisFrame;
			float shipThrust = c_shipThrust * c_simulateUpdateRate;

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

			updateEngineFuel(&playerShipData, c_simulateUpdateRate);

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

			UpdatePhysics(&playerPhys,
			              numDamagesSustained <= c_numSustainableDamagesBeforeGameOver ?
			                  playerDrag :
			                  c_onFailurePlayerDrag,
			              c_simulateUpdateRate);
			updateObjects(&playerPhys, &playerShipData, c_simulateUpdateRate);

			doFactory(playerShip, c_simulateUpdateRate);
			accumulatedTime -= c_simulateUpdateRate;
		}
		/* fprintf(stderr, "%d\n", numSimulationUpdatesThisFrame); */

		// Rendering
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		Vec2 extrapolatedPlayerPosition;
		extrapolatedPlayerPosition.x =
		    playerPhys.position.x + (accumulatedTime * playerPhys.velocity.x);
		extrapolatedPlayerPosition.y =
		    playerPhys.position.y + (accumulatedTime * playerPhys.velocity.y);

		// Let the ship drift away on success or failure
		if (numDamagesSustained <= c_numSustainableDamagesBeforeGameOver &&
		    currentGamePhase < ARRAY_SIZE(gamePhases))
		{
			snapCameraToGrid(&camera, &extrapolatedPlayerPosition, playerShip, deltaTime);
		}

		renderStarField(renderer, &camera, windowWidth, windowHeight);

		// Note: SDL doesn't render at a subpixel level, so we cast away the floating point of the
		// camera to ensure our tiles will be at exact pixels. If we didn't do this, we would get
		// seams due to floating point inaccuracies.
		renderGridSpaceFromTileSheet(renderer, &tileSheet, playerShip, extrapolatedPlayerPosition.x,
		                             extrapolatedPlayerPosition.y, (int)camera.x, (int)camera.y);

		renderObjects(renderer, &tileSheet, &camera, extrapolatedPlayerPosition, accumulatedTime);

		// HUD
		if (numDamagesSustained > c_numSustainableDamagesBeforeGameOver)
		{
			doEndScreenFailure(renderer, &tileSheet);
			if (continuePressed())
				return GameplayResult_StartNewGame;
		}
		else if (currentGamePhase >= ARRAY_SIZE(gamePhases))
		{
			doEndScreenSuccess(renderer, &tileSheet);
			if (continuePressed())
				return GameplayResult_StartNewGame;
		}
		else
		{
			// Hide part of the hud during ship construction
			if (gamePhases[currentGamePhase].objective != Objective_ShipConstruct)
			{
				renderMiniMap(
				    renderer, windowWidth, windowHeight, &extrapolatedPlayerPosition, playerShip,
				    gamePhases[currentGamePhase].objective == Objective_ReachGoalPoint ? &goal :
				                                                                         NULL);

				int playerVelocity = (int)(Magnitude(&playerPhys.velocity));
				renderNumber(renderer, &tileSheet, 100, 100, playerVelocity);
				renderText(renderer, &tileSheet, 100, 80, "VELOCITY");
				// This is a bit weird, but informs the player that they will just waste fuel if
				// they keep burning in that direction
				if (playerVelocity >= (int)c_maxSpeed)
					renderText(renderer, &tileSheet, 100, 60, "WARNING   MAX VELOCITY REACHED");

				{
					renderText(renderer, &tileSheet, 100, 300 - 40, "SHIP ARMOR");
					char remainingHealth =
					    c_numSustainableDamagesBeforeGameOver - numDamagesSustained;
					if (remainingHealth)
					{
						GridSpace shipHealth = {0};
						shipHealth.width = c_numSustainableDamagesBeforeGameOver;
						shipHealth.height = 1;
						// Max health
						GridCell shipHealthCells[10] = {0};
						for (int i = 0; i < remainingHealth; ++i)
							shipHealthCells[i].type = '#';
						shipHealth.data = shipHealthCells;
						renderGridSpaceFromTileSheet(renderer, &tileSheet, &shipHealth, 100,
						                             300 - 20, 0, 0);
					}
					else
						renderText(renderer, &tileSheet, 100, 300 - 20, "NONE");
				}
			}
			else
			{
				renderFactoryGuide(renderer, &tileSheet);
			}

			if (currentGamePhase < ARRAY_SIZE(gamePhases))
			{
				bool startNewPhase = false;
				if (gamePhases[currentGamePhase].objective == Objective_ReachGoalPoint)
				{
					renderGoal(renderer, &camera, &goal);
					if (CheckGoalSatisfied(&extrapolatedPlayerPosition, playerShip, &goal))
						startNewPhase = true;
				}

				Uint64 currentGamePhaseTicks = SDL_GetPerformanceCounter();
				int currentSeconds = currentGamePhaseTicks / performanceNumTicksPerSecond;
				int secondsInCurrentPhase = currentSeconds - startPhaseSeconds;
				GamePhase* phase = &gamePhases[currentGamePhase];
				static const char* currentPrompt = NULL;
				char typeOutPrompt[256] = {0};
				if (currentPrompt != phase->prompt)
				{
					startPromptTimeToTypeOut = c_typeOutTime;
					currentPrompt = phase->prompt;
				}
				startPromptTimeToTypeOut -= deltaTime;
				if (startPromptTimeToTypeOut < 0.f)
					startPromptTimeToTypeOut = 0.f;
				char promptLength = strlen(currentPrompt);
				char typePromptLength =
				    (promptLength * (c_typeOutTime - startPromptTimeToTypeOut)) / c_typeOutTime;
				memset(typeOutPrompt, 0, sizeof(typeOutPrompt));
				for (int i = 0; i < typePromptLength; ++i)
				{
					typeOutPrompt[i] = currentPrompt[i];
				}
				renderText(renderer, &tileSheet,
				           (windowWidth / 2) - ((strlen(phase->prompt) * c_scaledFontWidth) / 2),
				           120, typeOutPrompt);
				int phaseTimeLeft = phase->timeToCompleteSeconds - secondsInCurrentPhase;
				if (phaseTimeLeft)
					renderNumber(renderer, &tileSheet, (windowWidth / 2) - c_fontWidth, 145,
					             phaseTimeLeft);

				bool failedPhase = false;
				if (secondsInCurrentPhase >= phase->timeToCompleteSeconds)
				{
					switch (gamePhases[currentGamePhase].objective)
					{
						case Objective_None:
							startNewPhase = true;
							break;
						case Objective_ShipConstruct:
							startNewPhase = true;
							break;
						case Objective_ReachGoalPoint:
							failedPhase = true;
							break;
					}
				}

				if (failedPhase)
				{
					timeSinceFailedPhase = c_timeToShowFailedOverlay;
					timeSinceFailedPhaseDamage = c_timeToShowDamagedText;
					startNewPhase = true;

					damageShip(playerShip);
					++numDamagesSustained;
				}

				if (timeSinceFailedPhaseDamage > 0.f)
				{
					timeSinceFailedPhaseDamage -= 1.f * deltaTime;
					if (timeSinceFailedPhaseDamage < 0.f)
						timeSinceFailedPhaseDamage = 0.f;

					renderText(renderer, &tileSheet, 100, 320, "DAMAGE DAMAGE DAMAGE");
					if (numDamagesSustained == c_numSustainableDamagesBeforeGameOver)
						renderText(renderer, &tileSheet, 100, 340,
						           "WE WILL NOT SURVIVE ANOTHER HIT");
				}

				if (startNewPhase || forceStartNewPhase)
				{
					startPhaseSeconds = currentSeconds;
					++currentGamePhase;
					if (gamePhases[currentGamePhase].objective == Objective_ReachGoalPoint)
					{
						goal.x = (rand() % (c_spaceSize - (c_spawnBuffer * 2))) + c_spawnBuffer;
						goal.y = (rand() % (c_spaceSize - (c_spawnBuffer * 2))) + c_spawnBuffer;
					}
				}
			}

			IVec2 cameraPosition = {(int)camera.x, (int)camera.y};
			doEditUI(renderer, &tileSheet, windowWidth, windowHeight, cameraPosition,
			         extrapolatedPlayerPosition, &playerShipData, inventory, ARRAY_SIZE(inventory),
			         &constructionFuelPool);
		}

		// Draw this even after the game is over
		if (timeSinceFailedPhase > 0.f)
		{
			timeSinceFailedPhase -= 1.f * deltaTime;
			if (timeSinceFailedPhase < 0.f)
				timeSinceFailedPhase = 0.f;
			SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
			SDL_SetRenderDrawColor(renderer, 255, 255, 255,
			                       (255 * timeSinceFailedPhase) / c_timeToShowFailedOverlay);
			SDL_Rect fullScreenRect = {0, 0, windowWidth, windowHeight};
			SDL_RenderFillRect(renderer, &fullScreenRect);
			SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
		}

		if (enableDebugUI)
			addRenderDiagnostics(renderer, deltaTime, numSimulationUpdatesThisFrame);

		SDL_RenderPresent(renderer);
		SDL_UpdateWindowSurface(window);
		/* SDL_Delay(c_arbitraryDelayTimeMilliseconds); */
	}

	if (exitReason)
	{
		fprintf(stderr, "Exiting. Reason: %s\n", exitReason);
	}
	return GameplayResult_ExitGame;
}

#ifdef WINDOWS
void SetDPIAware();
#endif

#ifdef WINDOWS
int WinMain(int numArguments, char** arguments)
#else
int main(int numArguments, char** arguments)
#endif
{
#ifdef WINDOWS
	SetDPIAware();
#endif

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
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
	{
		sdlPrintError();
		return 1;
	}

	// Exclusive fullscreen (for testing)
	/* SDL_SetWindowSize(window, 3840, 2160); */
	/* SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN); */

	// Set up bundled data
	initializeCakelisp();

	// Load tile sheet into texture
	SDL_Texture* tileSheetTexture = NULL;
	{
#define NO_DATA_BUNDLE
#ifdef NO_DATA_BUNDLE
		SDL_Surface* tileSheetSurface = SDL_LoadBMP("assets/TileSheet.bmp");
#else
		SDL_RWops* tileSheetRWOps =
		    SDL_RWFromMem(startTilesheetBmp, endTilesheetBmp - startTilesheetBmp);
		SDL_Surface* tileSheetSurface = SDL_LoadBMP_RW(tileSheetRWOps, /*freesrc=*/1);
#endif
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
	                           {'R', 2, 0, TextureTransform_None},
	                           // Intake from left
	                           {'L', 2, 0, TextureTransform_FlipHorizontal},
	                           // Intake from top
	                           {'U', 2, 0, TextureTransform_CounterClockwise90},
	                           // Intake from bottom
	                           {'D', 2, 0, TextureTransform_Clockwise90},
	                           // Engine to left (unpowered)
	                           {'l', 1, 1, TextureTransform_FlipHorizontal},
	                           {'r', 1, 1, TextureTransform_None},
	                           {'u', 1, 1, TextureTransform_Clockwise90},
	                           {'d', 1, 1, TextureTransform_CounterClockwise90},

	                           // Objects
	                           // Unrefined fuel (asteroid)
	                           {'a', 0, 3, TextureTransform_None},
	                           // Refined fuel
	                           {'g', 1, 0, TextureTransform_None},
	                       },
	                       tileSheetTexture};

	if (!doMainMenu(window, renderer, &tileSheet))
		return 0;

	GameplayResult result = GameplayResult_StartNewGame;
	while (result == GameplayResult_StartNewGame)
	{
		result = doGameplay(window, renderer, tileSheet);
	}

	SDL_DestroyRenderer(renderer);
	sdlShutdown(window);

	switch (result)
	{
		case GameplayResult_ExitGame:
			return 0;
		default:
			return 1;
	}
}
