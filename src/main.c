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

static int update(void* userdata);
static void init(PlaydateAPI* userdata);
const char* fontpath = "/Asterix.pft";

LCDFont* font = NULL;

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
// 
#define PI 3.141f

#undef max 
#define max(a, b) ((a) > (b) ? (a) : (b))
#undef min
#define min(a, b) ((a) > (b) ? (b) : (a))


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

float Vec2Length(Vec2 v) {
	return sqrtf(v.x * v.x + v.y * v.y);
}

Vec2 Vec2Normalized(Vec2 v) {
	float length = Vec2Length(v);
	if (length > 0.0f) {
		return (Vec2) { v.x / length, v.y / length };
	}

	return (Vec2) { 0.0f, 0.0f };
}

Vec2 Vec2Scale(Vec2 v, float s) {
	return (Vec2){ v.x * s, v.y * s };
}

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



#define WORLD_TILE_COUNT 64
#define WORLD_TILE_PX 32
static struct {
	unsigned int frameCount;
	struct {
		LCDSprite* sprite;
		Vec2 pos;
	}Player;
	struct {
		LCDSprite* lightMask;
		LCDSprite* background;
		
	} sprites;

	struct {
		LCDBitmapTable* player[9 /*8 directions + 1 idle*/];
		LCDBitmap* lightMask;
		LCDBitmap* backgroundTest;
	}bitmaps;

	struct {
		float lightExp;
		float lightRadius;
	}config;
}g;

static void reloadAssets(PlaydateAPI* pd) {
	// load ini
	char* content = LoadTextFile(pd, "config.ini");
	ini_t* config = ini_load_from_memory(content);
	// parse values
//	char test = ini_get(config, "light", "exp");
	g.config.lightExp = ini_get_float(config, "light", "exp");
	g.config.lightRadius = ini_get_float(config, "light", "radius");
	
	ini_free(config);
	free(content);
	// generate light mask 
	{
		const int height = 240, width = 400;
		pd->graphics->freeBitmap(g.bitmaps.lightMask);
		g.bitmaps.lightMask = pd->graphics->newBitmap(width, height, kColorBlack);
		pd->graphics->pushContext(g.bitmaps.lightMask);
		//for (int x = 0; x < width; x++) {
		//	for (int y = 0; y < height; y++)
		//	{
		//		float xf = (float)((x) - width / 2);
		//		float yf = (float)((y) - height / 2);
		//		float dist = sqrtf(xf * xf + yf * yf);
		//		dist /= g.config.lightRadius;
		//		dist = powf(dist, g.config.lightExp);
		//		LCDColor color = GetDithered(x, y , dist, kColorClear, kColorBlack);
		//		if (color != kColorClear) {
		//			pd->graphics->drawLine(x, y, x, y, 1, kColorBlack);
		//		}
		//	}
		//}
		pd->graphics->setDrawMode(kDrawModeWhiteTransparent);
		pd->graphics->fillEllipse(0, 0, height, height, 0.0f, 0.0f, kColorWhite);
		pd->graphics->popContext();

		pd->sprite->removeSprite(g.sprites.lightMask);
		pd->sprite->freeSprite(g.sprites.lightMask);

		g.sprites.lightMask = pd->sprite->newSprite();
		pd->sprite->setImage(g.sprites.lightMask, g.bitmaps.lightMask, kBitmapUnflipped);
		//pd->sprite->addSprite(g.sprites.lightMask);

		const Vec2 screenCenter = {
			pd->display->getWidth() / 2.0f,
			pd->display->getHeight() / 2.0f
		};
		pd->sprite->moveTo(g.sprites.lightMask, screenCenter.x, screenCenter.y);		
	}



	LoadBitmapFromFile(pd, &g.bitmaps.backgroundTest, "GameJam_Background.png");
	pd->sprite->removeSprite(g.sprites.background);
	pd->sprite->freeSprite(g.sprites.background);
	g.sprites.background = pd->sprite->newSprite();
	pd->sprite->setImage(g.sprites.background, g.bitmaps.backgroundTest, kBitmapUnflipped);
	pd->sprite->addSprite(g.sprites.background);
	pd->sprite->setZIndex(g.sprites.background, -16);
}

static void init(PlaydateAPI* pd)
{
	const int screenHeight = 240, screenWidth = 400;
	// Load assets
	// ---------------------------------------------------------
	{
		const char* err;
		font = pd->graphics->loadFont(fontpath, &err);

		if (font == NULL)
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);
	}

	//g.bitmaps.gifTest = LoadBitmapTableFromFile(pd, "Unbenannt-1.gif");
	
	pd->display->setRefreshRate(50.0f);

	// Initialize sprites
	// ---------------------------------------------------------
	//g.sprites.background = pd->sprite->newSprite();
	//pd->sprite->setImage(g.demoSprite, pd->graphics->getTableBitmap(g.bitmaps.gifTest, 0), kBitmapUnflipped);
	//pd->sprite->addSprite(g.demoSprite);

	// Initialize sprites
	// ---------------------------------------------------------
	LoadBitmapFromFile(pd, &g.bitmaps.backgroundTest, "GameJam_Background.png");
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
	pd->sprite->moveTo(g.Player.sprite, 200.0f, 120.0f);

	// setup light area 
	g.bitmaps.lightMask = pd->graphics->newBitmap(screenWidth, screenHeight, kColorBlack);
	g.sprites.lightMask = pd->sprite->newSprite();
	pd->sprite->setImage(g.sprites.lightMask, g.bitmaps.lightMask, kBitmapUnflipped);

	reloadAssets(pd);

}

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;
#ifdef _WINDLL
	reloadAssets(pd); //only hot reload during testing
#endif
	//const Vec2i screenCenter = {
	//	pd->display->getWidth() / 2,
	//	pd->display->getHeight() / 2
	//};


	// Handle Input
	// ---------------------------------------------------------
	PDButtons buttonsCurrent, buttonsPushed, buttonsReleased;
	pd->system->getButtonState(&buttonsCurrent, &buttonsPushed, &buttonsReleased);

	// Player movement + 
	// ---------------------------------------------------------
	{
		Vec2 movement = { 0.0f, 0.0f };
		if ((buttonsCurrent & kButtonLeft) == kButtonLeft) { movement.x += 1.0f; }
		if ((buttonsCurrent & kButtonRight) == kButtonRight) { movement.x -= 1.0f; }
		if ((buttonsCurrent & kButtonUp) == kButtonUp) { movement.y += 1.0f; }
		if ((buttonsCurrent & kButtonDown) == kButtonDown) { movement.y -= 1.0f; }

		int directionMapedToAnimIndex = 3; // 3 == idle anim
		if (movement.x != 0.0f || movement.y != 0.0f) {

			float directionNormalized = atan2f(movement.y, movement.x) / PI; // / PI -> map from [-pi/2..pi/2] to [-1..1] //TODO: atan2 is not the best in case of perf...
			float directionMapedToAnimIndexF = (directionNormalized * 0.5f + 0.5f) * 8.0f; // map from [-1..1] to [0..1]
			directionMapedToAnimIndex = max(0, (int)directionMapedToAnimIndexF);
			directionMapedToAnimIndex = min(8, directionMapedToAnimIndex);
		}

		uint16_t animTableIndex = (uint16_t)directionMapedToAnimIndex;
		uint16_t animTableCount = GetBitmapTableCount(g.bitmaps.player[animTableIndex]);
		pd->sprite->setImage(
			g.Player.sprite,
			pd->graphics->getTableBitmap(g.bitmaps.player[animTableIndex], (g.frameCount / 4) % animTableCount),
			kBitmapUnflipped);

		movement = Vec2Normalized(movement);
		const float movementSpeed = 4.0f;
		movement = Vec2Scale(movement, movementSpeed);
		g.Player.pos.x += movement.x;
		g.Player.pos.y += movement.y;

	}

	// Light
	// ---------------------------------------------------------
	{
		pd->graphics->pushContext(g.bitmaps.lightMask);
		float lightFill = 1.0f;
		const float maxSize = 240.0f;
		int lightAreaSize = (int)(lightFill * maxSize);
		pd->graphics->setDrawMode(kDrawModeWhiteTransparent);
		pd->graphics->setBackgroundColor(kColorBlack);
		pd->graphics->fillEllipse(0, 0, lightAreaSize, lightAreaSize, 0.0f, 0.0f, kColorWhite);
		pd->graphics->popContext();
	}

	const Vec2 cameraTarget = g.Player.pos;
	pd->sprite->moveTo(g.sprites.background, cameraTarget.x, cameraTarget.y);
	// draw tiles
	// ---------------------------------------------------------
	
	// TODO: flatten loop for extra perf?
	//for (int x = 0; x < WORLD_TILE_COUNT; x++) {
	//	for (int y = 0; y < WORLD_TILE_COUNT; y++) {
	//		pd->graphics->fillRect(
	//			x * WORLD_TILE_PX + cameraTarget.x,
	//			y * WORLD_TILE_PX + cameraTarget.y,
	//			WORLD_TILE_PX, 
	//			WORLD_TILE_PX, 
	//			(x ^ y) % 2 );
	//	}
	//}

	// loop over entire screen
	//for (int x = 0; x < pd->display->getWidth(); x++) {
	//	for (int y = 0; y < pd->display->getHeight(); y++) {
	//		//pd->graphics->p
	//	}
	//}

	// draw player
	//const int playerWidth = 14;
	//const int playerHeight = 30;
	//

	//LCDBitmap* image = pd->graphics->newBitmap(32, 32, 0);

	//pd->graphics->pushContext(image);
	//	pd->graphics->fillEllipse(0, 0, playerWidth, playerHeight, 0.0f, 0.0f, kColorBlack);
	//	pd->graphics->drawEllipse(0, 0, playerWidth, playerHeight, 1, 0.0f, 0.0f, kColorWhite);
	//pd->graphics->popContext();
	//LCDSprite* sprite = pd->sprite->newSprite();
	//pd->sprite->setSize(sprite, 32, 32);
	//pd->sprite->setImage(sprite, image, kBitmapUnflipped);
	//pd->sprite->addSprite(sprite);
	//pd->sprite->moveBy(sprite, 10, 14);
	//


	//pd->graphics->fillEllipse(screenCenter.x, screenCenter.y, 128, 128, 0.0f, 0.0f, kColorXOR);

	//int gifFrameCount = pd->graphics
	//uint16_t gifCount = GetBitmapTableCount(g.assets.gifTest);
	//pd->graphics->drawBitmap(g.bitmaps.backgroundTest, g.Player.pos.x, g.Player.pos.y, kBitmapUnflipped);
	//pd->graphics->drawBitmap(g.bitmaps.lightMask, 0, 0, kBitmapUnflipped);
	//float scale = ((float) (g.frameCount % 40) / 40.0f);

	//pd->sprite->setSize(g.sprites.lightMask, scale * 400.0f, scale * 240.0f);

	
	//pd->graphics->drawBitmap(pd->graphics->getTableBitmap(g.assets.gifTest, g.frameCount % gifCount), screenCenter.x, screenCenter.y, kBitmapUnflipped);
	pd->sprite->updateAndDrawSprites();
	//pd->graphics->setFont(font);
//


//	pd->graphics->drawText(value, strlen(value), kUTF8Encoding, 10, 10);


	pd->system->drawFPS(0, 0);
	g.frameCount++;

	return 1;
}

