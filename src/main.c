//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"

#include "rxi-ini\ini.h"
#include "damped_spring.h"

static int update(void* userdata);
static void init(PlaydateAPI* userdata);
const char* fontpath = "/Asterix.pft";



#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed
	if ( event == kEventInit )
	{
		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		init(pd);
		pd->system->setUpdateCallback(update, pd);
	}
	return 0;
}


// ---------------------------------------------------------
// .* Game Code *.
// ---------------------------------------------------------

// Core types and functions
// ---------------------------------------------------------
#define PI 3.141f

#undef max 
#define max(a, b) ((a) > (b) ? (a) : (b))
#undef min
#define min(a, b) ((a) > (b) ? (b) : (a))

#define arrLen(staticArray) (sizeof(staticArray)/sizeof(staticArray[0]))

typedef enum {
	tGround,
	tWall,
} TileType;

typedef struct {
	int x, y;
} Vec2i;

typedef struct {
	float x, y;
} Vec2;

// IMPORTANT: Hack allert! This function may break in futher releases of the Playdate SDK. See link below.
// Source (big oof): https://devforum.play.date/t/c-api-get-number-of-bitmaps-in-lcdbitmaptable/9134/5
static uint16_t GetBitmapTableCount(LCDBitmapTable* table) {
	return *(uint16_t*)table;
}

static void LoadBitmapTableFromFile(PlaydateAPI* pd, LCDBitmapTable** table, const char* path) {
	const char* err;
	*table = pd->graphics->loadBitmapTable(path, &err);
	if (table == NULL)
		pd->system->error("%s:%i Couldn't asset %s: %s", __FILE__, __LINE__, path, err);

}

static void LoadBitmapFromFile(PlaydateAPI* pd, LCDBitmap** bitmap, const char* path) {
	if (*bitmap != 0) {
		pd->graphics->freeBitmap(*bitmap);
	}
	
	const char* err;
	*bitmap = pd->graphics->loadBitmap(path, &err);
	if (*bitmap == NULL)
		pd->system->error("%s:%i Couldn't asset %s: %s", __FILE__, __LINE__, path, err);

}

LCDBitmap* ScaleBitmap(PlaydateAPI* pd, LCDBitmap* src, float scaleX, float scaleY) {
	int width = 0, height = 0, rowbytes = 0;
	uint8_t* mask = NULL;
	uint8_t* data = NULL;
	pd->graphics->getBitmapData(src, &width, &height, &rowbytes, &mask, &data);
	LCDBitmap* newBitmap = pd->graphics->newBitmap((int)((float)width * scaleX), (int)((float)height * scaleY), kColorClear);
	pd->graphics->pushContext(newBitmap);
	pd->graphics->drawScaledBitmap(src, 0, 0, scaleX, scaleY);
	pd->graphics->popContext();
	
	return newBitmap;
}

// Checks if a pointer is an element of an array.
// TODO: is size_t also the size of a memory adress on every platform?
int IsPtrInArray(void* ptr, void* firstElement, void* lastElement) {
	const size_t ptr_i	= (size_t) ptr;
	const size_t first	= (size_t) firstElement;
	const size_t last	= (size_t) lastElement;

	return ptr_i >= first && ptr_i < last;
}
// Vector math
// ---------------------------------------------------------
float Vec2Dot(Vec2 a, Vec2 b) {
	return a.x * b.x + a.y * b.y;
}

float Vec2Length(Vec2 v) {
	return sqrtf(Vec2Dot(v, v));
}

Vec2 Vec2Normalize(Vec2 v) {
	const float length = Vec2Length(v);
	if (length > 0.0f) {
		return (Vec2) { v.x / length, v.y / length };
	}

	return (Vec2) { 0.0f, 0.0f };
}

Vec2 Vec2Scale(Vec2 v, float s) {
	return (Vec2){ v.x * s, v.y * s };
}

Vec2 Vec2Add(Vec2 a, Vec2 b) {
	return (Vec2) { a.x + b.x, a.y + b.y };
}

Vec2 Vec2Sub(Vec2 a, Vec2 b) {
	return (Vec2) { a.x - b.x, a.y - b.y };
}


//NOTE-Julian: stolen from raylib!
static char* LoadTextFile(PlaydateAPI* pd, const char* fileName) {
	SDFile* file = pd->file->open(fileName, kFileRead);
	char* text = NULL;
	if (file != NULL)
	{
		// WARNING: When reading a file as 'text' file,
		// text mode causes carriage return-linefeed translation...
		// ...but using fseek() should return correct byte-offset
		//fseek(file, 0, SEEK_END);
		pd->file->seek(file, 0, 2);
		int size = (unsigned int)pd->file->tell(file);
		pd->file->seek(file, 0, 0);
		if (size > 0)
		{
			text = (char*)malloc((size + 1) * sizeof(char));
			if (text == NULL) {
				return text;//TODO
			}
			if (text != NULL)
			{
				int count = (unsigned int)pd->file->read(file, text, size);

				// WARNING: \r\n is converted to \n on reading, so,
				// read bytes count gets reduced by the number of lines
				if (count < size) text = realloc(text, count + 1);
				if (text == NULL) {
					return text; //TODO
				}

				// Zero-terminate the string
				text[count] = '\0';

				//pd->system->logToConsole("FILEIO: [%s] Text file loaded successfully", fileName);
			}
			else pd->system->logToConsole("FILEIO: [%s] Failed to allocated memory for file reading", fileName);
		}
		else pd->system->logToConsole("FILEIO: [%s] Failed to read text file", fileName);
		
		pd->file->close(file);
	}
	return text;
}

static float ini_get_float(ini_t* ini, char* section, char* key) {
	const char* value = ini_get(ini, section, key);
	return (float) strtol (value, 0, 10);
}

static LCDColor GetDithered(int x, int y, float value, LCDColor lowColor, LCDColor highColor) {
	float xf = (float)x;
	float yf = (float)y;
	(void)value;
	//note: 2x2 ordered dithering, ALU-based (omgthehorror)
	//float ijX = floorf(fmodf((float)x, 2.0f));
	//float ijY = floorf(fmodf((float)y, 2.0f));
	//
	//float idx = ijX + 2.0f * ijY;
	//float mx = (fabsf(idx - 0.0f) < .5f ? 1.f : .0f) * .75f;
	//float my = (fabsf(idx - 1.0f) < .5f ? 1.f : .0f) * .25f;
	//float mz = (fabsf(idx - 2.0f) < .5f ? 1.f : .0f) * .00f;
	//float mw = (fabsf(idx - 3.0f) < .5f ? 1.f : .0f) * .50f;
	//float d = mx + my + mz + mw;

	//const float phi = 1.61803f;
	const float phiX = 0.7548776662f;// 1.0f / phi;
	const float phiY = 0.56984029f;//1.0f / (phi * phi);

	float dither = fmodf(phiX * xf + phiY * yf, 1.0f);
	dither = dither * 2.0f - 1.0f;
	return dither + value > 1.0f ? lowColor : highColor;
}

// Item defs
// ---------------------------------------------------------
#define ITEM_COUNT 5
typedef enum {
	itCasette,
	itGameBoy,
	itPuzzle,
	itSandwich,
	itTeddy,
} ItemType;


typedef struct {
	LCDSprite* spriteWorld;
	LCDSprite* spriteUi;
	LCDBitmapTable* bitmap;
	LCDBitmap* bitmapUi;
	int collected;
} Item;

// Enemy defs
// ---------------------------------------------------------
#define ENEMY_TYPE_COUNT 2

typedef enum {
	etShear,
	//etComputerHead TODO: not implemented yet
} EmemyType;

typedef struct {
	int active;
	EmemyType type;
	LCDSprite* sprite;
} Enemy;

// Game Context
// ---------------------------------------------------------

const float lightFullRadius = 240.0f;
static struct {
	unsigned int frameCount;
	struct {
		LCDSprite* sprite;
	}Player;

	Item items[ITEM_COUNT];
	Enemy enemies[8];

	LCDFont* font;
	struct {
		LCDSprite* lightMask;
		LCDSprite* background;
		
	} sprites;

	struct {
		LCDBitmapTable* player[9 /*8 directions + 1 idle*/];
		LCDBitmapTable* enemies[ENEMY_TYPE_COUNT];
		LCDBitmapTable* background;
		LCDBitmap* lightMask;		
	} bitmaps;

	struct {
		float radius01;
		float radiusVel;
	} light;

	struct {
		LCDBitmap* bgBitmap;
		LCDSprite* bg;
	} ui;

	struct {
		float crankToLightMult;
	} config;


}g;

// Item functions
// ---------------------------------------------------------
void ResetItem(PlaydateAPI* pd, Item* item);
void LoadAndInitializeItem(PlaydateAPI* pd, Item* item, int16_t index, float placementX, float placementY, const char* path) {
	LoadBitmapTableFromFile(pd, &item->bitmap, path);
	// make downscaled version of sprite for UI
	item->bitmapUi = ScaleBitmap(pd, pd->graphics->getTableBitmap(item->bitmap, 0), 0.5f, 0.5f);

	// place and setup UI sprite
	item->spriteUi = pd->sprite->newSprite();
	pd->sprite->setImage(item->spriteUi, item->bitmapUi, kBitmapUnflipped);
	pd->sprite->moveTo(item->spriteUi, 16.0f + 32.0f * (float)index, 16.0f);
	pd->sprite->setIgnoresDrawOffset(item->spriteUi, 1);
	pd->sprite->setZIndex(item->spriteUi, 2100);

	// place and setup world Sprite
	item->spriteWorld = pd->sprite->newSprite();
	pd->sprite->setImage(item->spriteWorld, pd->graphics->getTableBitmap(item->bitmap, 0), kBitmapUnflipped);
	pd->sprite->moveTo(item->spriteWorld, placementX, placementY);

	// setup collision for world sprite 
	pd->sprite->setCollideRect(item->spriteWorld, (PDRect) { .width = 64.0f, .height = 64.0f });
	pd->sprite->collisionsEnabled(item->spriteWorld);

	// make sprite accessable 
	pd->sprite->setUserdata(item->spriteWorld, (void*)item);


	ResetItem(pd, item);
}

void UpdateItem(PlaydateAPI* pd, Item* item) {
	uint16_t animFrames = GetBitmapTableCount(item->bitmap);
	pd->sprite->setImage(item->spriteWorld, pd->graphics->getTableBitmap(item->bitmap, (g.frameCount / 4) % animFrames), kBitmapUnflipped);
}

void SetItemCollected(PlaydateAPI* pd, Item* item) {
	item->collected = 1;
	pd->sprite->removeSprite(item->spriteWorld);
	pd->sprite->addSprite(item->spriteUi);
}

void ResetItem(PlaydateAPI* pd, Item* item) {
	// remove from world (in case they where placed before)
	pd->sprite->removeSprite(item->spriteUi);
	pd->sprite->removeSprite(item->spriteWorld);
	item->collected = 0;

	// add sprite to world.
	pd->sprite->addSprite(item->spriteWorld);
}

int IsItem(void* itemPtr) {
	return IsPtrInArray(itemPtr, &g.items[0], &g.items[ITEM_COUNT]);
}

// Enemy functions
// ---------------------------------------------------------

void DisableEnemy(PlaydateAPI* pd, Enemy* em) {
	if (!em->active) {
		return;
	}
	em->active = 0;
	pd->sprite->removeSprite(em->sprite);
	pd->sprite->freeSprite(em->sprite);
}

void EnableEnemy(PlaydateAPI* pd, Enemy* em, EmemyType type) {
	if (em->active) {
		return; 
	}
	em->active = 1;
	em->type = type;

	// setup sprite
	em->sprite = pd->sprite->newSprite();
	LCDBitmap* firstFrame = pd->graphics->getTableBitmap(g.bitmaps.enemies[em->type], 0);
	if (em->type == etShear) {
		pd->sprite->setZIndex(em->sprite, 1001);
	}
	pd->sprite->setImage(em->sprite, firstFrame, kBitmapUnflipped);
	pd->sprite->addSprite(em->sprite);
	pd->sprite->setCollideRect(em->sprite, (PDRect) {.x = 16.0f, .width = 32.0f, .height = 64.0f });
	pd->sprite->collisionsEnabled(em->sprite);

	pd->sprite->setUserdata(em->sprite, (void*) em);
}

void UpdateEnemy(PlaydateAPI* pd, Enemy* em) {
	if (!em->active) {
		return;
	}

	Vec2 pos;
	pd->sprite->getPosition(em->sprite, &pos.x, &pos.y);
	Vec2 playerPos;
	pd->sprite->getPosition(g.Player.sprite, &playerPos.x, &playerPos.y);

	switch (em->type)
	{
	case etShear: {
		Vec2 vecToPlayer = Vec2Sub(playerPos, pos);
		float distToPlayer = Vec2Length(vecToPlayer);
		Vec2 dirToPlayer = Vec2Normalize(vecToPlayer);
		int inRadius = distToPlayer < (g.light.radius01 * lightFullRadius * 0.25f);
		if (!inRadius) {
			pd->sprite->moveBy(em->sprite, dirToPlayer.x, dirToPlayer.y);
		}

		uint16_t animFrameCount = GetBitmapTableCount(g.bitmaps.enemies[em->type]);
		LCDBitmap* currentFrame = pd->graphics->getTableBitmap(g.bitmaps.enemies[em->type], (g.frameCount / 4) % animFrameCount);
		pd->sprite->setImage(em->sprite, currentFrame, kBitmapUnflipped);

		break;
	}
	default:
		break;
	}


}



int IsEnemy(void* enemyPtr) {
	return IsPtrInArray(enemyPtr, &g.enemies[0], &g.enemies[arrLen(g.enemies)]);
}

// Config reloading (obsolete?)
// ---------------------------------------------------------
static void reloadAssets(PlaydateAPI* pd) {
	// load ini
	char* content = LoadTextFile(pd, "config.ini");
	ini_t* config = ini_load_from_memory(content);
	// parse values
	g.config.crankToLightMult = ini_get_float(config, "light", "crankToLightMult");
	//g.config.lightRadius = ini_get_float(config, "light", "radius");
	
	ini_free(config);
	free(content);
}


static void init(PlaydateAPI* pd)
{
	const int screenHeight = 240, screenWidth = 400;
	// Load assets
	// ---------------------------------------------------------
	{
		const char* err;
		g.font = pd->graphics->loadFont(fontpath, &err);

		if (g.font == NULL)
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);
	}


	pd->display->setRefreshRate(50.0f);


	g.light.radius01 = .1f;
	// Initialize sprites
	// ---------------------------------------------------------
	LoadBitmapTableFromFile(pd, &g.bitmaps.player[0], "player/runDownRight.gif");
	LoadBitmapTableFromFile(pd, &g.bitmaps.player[1], "player/runDown.gif");
	LoadBitmapTableFromFile(pd, &g.bitmaps.player[2], "player/runDownLeft.gif");
	LoadBitmapTableFromFile(pd, &g.bitmaps.player[3], "player/idleFront.gif");
	LoadBitmapTableFromFile(pd, &g.bitmaps.player[4], "player/runSideLeft.gif");
	LoadBitmapTableFromFile(pd, &g.bitmaps.player[5], "player/runUpLeft.gif");
	LoadBitmapTableFromFile(pd, &g.bitmaps.player[6], "player/runUp.gif");
	LoadBitmapTableFromFile(pd, &g.bitmaps.player[7], "player/runUpRight.gif");
	LoadBitmapTableFromFile(pd, &g.bitmaps.player[8], "player/runSideRight.gif");

	// setup player sprite
	g.Player.sprite = pd->sprite->newSprite();
	pd->sprite->setImage(g.Player.sprite, pd->graphics->getTableBitmap(g.bitmaps.player[0], 0), kBitmapUnflipped);
	pd->sprite->addSprite(g.Player.sprite);
	pd->sprite->moveTo(g.Player.sprite, 200.0f, 120.0f); //move to center of screen
	pd->sprite->setCollideRect(g.Player.sprite, (PDRect) { .x = 16.0f, .width = 32.0f, .height = 64.0f });
	//pd->sprite->collisionsEnabled(g.Player.sprite);
	//pd->sprite->setUserdata(g.Player.sprite, (void*) (size_t) udIsPlayer);
	

	// setup light area 
	g.bitmaps.lightMask = pd->graphics->newBitmap(screenWidth, screenHeight, kColorClear);
	g.sprites.lightMask = pd->sprite->newSprite();
	pd->sprite->setImage(g.sprites.lightMask, g.bitmaps.lightMask, kBitmapUnflipped);

	pd->sprite->addSprite(g.sprites.lightMask);
	pd->sprite->moveTo(g.sprites.lightMask, 200.0f, 120.0f);
	pd->sprite->setIgnoresDrawOffset(g.sprites.lightMask, 1);
	pd->sprite->setZIndex(g.sprites.lightMask, 1000);
	// load background
	LoadBitmapTableFromFile(pd, &g.bitmaps.background, "backgroundTest.gif");
	g.sprites.background = pd->sprite->newSprite();
	pd->sprite->setImage(g.sprites.background, pd->graphics->getTableBitmap(g.bitmaps.background, 0), kBitmapUnflipped);
	pd->sprite->addSprite(g.sprites.background);
	pd->sprite->setZIndex(g.sprites.background, -16);


	// Items
	LoadAndInitializeItem(pd, &g.items[itCasette],	itCasette,		0200.0f, 0500.0f, "items/itemCasette.gif");
	LoadAndInitializeItem(pd, &g.items[itGameBoy],	itGameBoy,		0600.0f, 0100.0f, "items/itemGameBoy.gif");
	LoadAndInitializeItem(pd, &g.items[itPuzzle],	itPuzzle,		1200.0f, 0900.0f, "items/itemPuzzle.gif");
	LoadAndInitializeItem(pd, &g.items[itSandwich],	itSandwich,		0000.0f, 0000.0f, "items/itemSandwich.gif");
	LoadAndInitializeItem(pd, &g.items[itTeddy],	itTeddy,		0900.0f, 0500.0f, "items/itemTeddy.gif");

	//UI background
	const int uiBarHeught = 32;
	g.ui.bgBitmap = pd->graphics->newBitmap(400, uiBarHeught, kColorBlack);
	g.ui.bg = pd->sprite->newSprite();
	pd->sprite->setImage(g.ui.bg, g.ui.bgBitmap, kBitmapUnflipped);
	pd->sprite->addSprite(g.ui.bg);
	pd->sprite->moveTo(g.ui.bg, 200, uiBarHeught * 0.5f);
	pd->sprite->setIgnoresDrawOffset(g.ui.bg, 1);
	pd->sprite->setZIndex(g.ui.bg, 2001);

	//Enemy bitmaps
	LoadBitmapTableFromFile(pd, &g.bitmaps.enemies[etShear], "enemies/enemyShear.gif");

	EnableEnemy(pd, &g.enemies[0], etShear);
	reloadAssets(pd);
}

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;
#ifdef _WINDLL
	reloadAssets(pd); //only hot reload during testing
#endif

	// Handle Input
	// ---------------------------------------------------------
	PDButtons buttonsCurrent, buttonsPushed, buttonsReleased;
	pd->system->getButtonState(&buttonsCurrent, &buttonsPushed, &buttonsReleased);

	// Player movement + animation
	// ---------------------------------------------------------
	{
		Vec2 movement = { 0.0f, 0.0f };
		if ((buttonsCurrent & kButtonLeft) == kButtonLeft) { movement.x += 1.0f; }
		if ((buttonsCurrent & kButtonRight) == kButtonRight) { movement.x -= 1.0f; }
		if ((buttonsCurrent & kButtonUp) == kButtonUp) { movement.y += 1.0f; }
		if ((buttonsCurrent & kButtonDown) == kButtonDown) { movement.y -= 1.0f; }

		// compute animation direction index
		int directionMapedToAnimIndex = 3; // 3 == idle anim
		if (movement.x != 0.0f || movement.y != 0.0f) {

			float directionNormalized = atan2f(movement.y, movement.x) / PI; // / PI -> map from [-pi/2..pi/2] to [-1..1] //TODO: atan2 is not the best in case of perf...
			float directionMapedToAnimIndexF = (directionNormalized * 0.5f + 0.5f) * 8.0f; // map from [-1..1] to [0..1]
			directionMapedToAnimIndex = max(0, (int)directionMapedToAnimIndexF);
			directionMapedToAnimIndex = min(8, directionMapedToAnimIndex);
		}

		// set animation frame and index
		uint16_t animTableIndex = (uint16_t)directionMapedToAnimIndex;
		uint16_t animTableCount = GetBitmapTableCount(g.bitmaps.player[animTableIndex]);
		pd->sprite->setImage(
			g.Player.sprite,
			pd->graphics->getTableBitmap(g.bitmaps.player[animTableIndex], (g.frameCount / 4) % animTableCount),
			kBitmapUnflipped);

		movement = Vec2Normalize(movement);
		const float movementSpeed = 4.0f;
		movement = Vec2Scale(movement, movementSpeed);
		pd->sprite->moveBy(g.Player.sprite, -movement.x, -movement.y);
	}

	// Light
	// ---------------------------------------------------------
	{
		tDampedSpringMotionParams lightDamping = CalcDampedSpringMotionParams(1.0f / 50.0f, 0.25f, 1.0f);
		float rawLightFill = fabsf( pd->system->getCrankChange() * 0.08f);
		rawLightFill = max(32.0f / lightFullRadius, rawLightFill);
		UpdateDampedSpringMotion(&g.light.radius01, &g.light.radiusVel, rawLightFill, lightDamping);

		int lightAreaSize = (int)(g.light.radius01 * lightFullRadius);

		int x = 200 - lightAreaSize / 2;
		int y = 120 - lightAreaSize / 2;
		pd->graphics->pushContext(g.bitmaps.lightMask);
			pd->graphics->setDrawOffset(0, 0);
			pd->graphics->clear(kColorBlack);
			pd->graphics->fillEllipse(x, y, lightAreaSize, lightAreaSize, 0.0f, 0.0f, kColorClear);
		pd->graphics->popContext();
	}
	// Background Anim
	// ---------------------------------------------------------
	{
		uint16_t frameCount = GetBitmapTableCount(g.bitmaps.background);
		pd->sprite->setImage(g.sprites.background, pd->graphics->getTableBitmap(g.bitmaps.background, (g.frameCount / 4) % frameCount), kBitmapUnflipped);
	}

	// Items Anim
	// ---------------------------------------------------------
	// check player item collisions
	{
		Vec2 player;
		pd->sprite->getPosition(g.Player.sprite, &player.x, &player.y);
		Vec2 playerNew;
		int collisions = 0;
		SpriteCollisionInfo* c = pd->sprite->checkCollisions(g.Player.sprite, player.x, player.y, &playerNew.x, &playerNew.y, &collisions);
		if (collisions > 0) {
			SpriteCollisionInfo ci = c[0];
		
			void* ptr = pd->sprite->getUserdata(ci.other);

			//TODO make switch statement 
			
			if (IsItem(ptr)) {
				SetItemCollected(pd, ptr);
			} else if (IsEnemy(ptr)) {
				// YOU DED!
				pd->sprite->moveTo(g.Player.sprite, 0.0f, 0.0f);

				for (size_t i = 0; i < ITEM_COUNT; i++) {
					ResetItem(pd, &g.items[i]);
				}
			}
		}

		for (size_t i = 0; i < ITEM_COUNT; i++) {
			UpdateItem(pd, &g.items[i]);
		}
	}

	{
		for (size_t enemyIdx = 0; enemyIdx < arrLen(g.enemies); enemyIdx++) {
			UpdateEnemy(pd, &g.enemies[enemyIdx]);
		}
	}


	Vec2 cameraTarget = {0};
	pd->sprite->getPosition(g.Player.sprite, &cameraTarget.x, &cameraTarget.y);
	cameraTarget.x -= 200.0f;
	cameraTarget.y -= 120.0f;
	pd->graphics->setDrawOffset((int) -cameraTarget.x, (int) -cameraTarget.y);
	pd->sprite->updateAndDrawSprites();


	// debug info
	const int showDebugInfo = 1;
	if (showDebugInfo) {
		const char* buildMsg = "BUILD " __TIME__;
		pd->graphics->setDrawOffset(0, 0);
		pd->graphics->fillRect(0, 230, 400, 10, kColorWhite);
		pd->graphics->setFont(g.font);
		pd->graphics->drawText(buildMsg, strlen(buildMsg), kUTF8Encoding, 0, 230);
		pd->system->drawFPS(385, 230);
	}

	g.frameCount++;

	return 1;
}

