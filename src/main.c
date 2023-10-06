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

static struct {
	int t;
	LCDBitmapTable* gifTest;
}assets;

typedef enum {
	tGround,
	tWall,
} TileType;

typedef struct {
	int x, y;
} Vec2i;

#define WORLD_TILE_COUNT 64
#define WORLD_TILE_PX 32
static struct {
	unsigned int frameCount;
	struct {
		Vec2i pos;
	}Player;
	
	TileType map[WORLD_TILE_COUNT][WORLD_TILE_COUNT];
}g;

// OOF: https://devforum.play.date/t/c-api-get-number-of-bitmaps-in-lcdbitmaptable/9134/5
static uint16_t GetBitmapTableCount(LCDBitmapTable* table) {
	return *(uint16_t*)table;
}

static void init(PlaydateAPI* pd)
{
	{
		const char* err;
		font = pd->graphics->loadFont(fontpath, &err);

		if (font == NULL)
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);
	}

	{
		const char* err;
		const char* gifPath = "Unbenannt-1.gif";
		assets.gifTest = pd->graphics->loadBitmapTable(gifPath, &err);
		if (assets.gifTest == NULL)
			pd->system->error("%s:%i Couldn't asset %s: %s", __FILE__, __LINE__, gifPath, err);
	}
	
	pd->display->setRefreshRate(50.0f);
}

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;
	
	//const Vec2i screenCenter = {
	//	pd->display->getWidth() / 2,
	//	pd->display->getHeight() / 2
	//};


	// Handle Input
	// ---------------------------------------------------------
	PDButtons buttonsCurrent, buttonsPushed, buttonsReleased;
	pd->system->getButtonState(&buttonsCurrent, &buttonsPushed, &buttonsReleased);
	
	if ((buttonsCurrent & kButtonLeft)	== kButtonLeft)		{ g.Player.pos.x++; }
	if ((buttonsCurrent & kButtonRight) == kButtonRight)	{ g.Player.pos.x--; }
	if ((buttonsCurrent & kButtonUp)	== kButtonUp)		{ g.Player.pos.y++; }
	if ((buttonsCurrent & kButtonDown)	== kButtonDown)		{ g.Player.pos.y--; }

	//const Vec2i cameraTarget = g.Player.pos;
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
	const int playerWidth = 14;
	const int playerHeight = 30;
	

	LCDBitmap* image = pd->graphics->newBitmap(32, 32, 0);

	pd->graphics->pushContext(image);
		pd->graphics->fillEllipse(0, 0, playerWidth, playerHeight, 0.0f, 0.0f, kColorBlack);
		pd->graphics->drawEllipse(0, 0, playerWidth, playerHeight, 1, 0.0f, 0.0f, kColorWhite);
	pd->graphics->popContext();
	LCDSprite* sprite = pd->sprite->newSprite();
	pd->sprite->setSize(sprite, 32, 32);
	pd->sprite->setImage(sprite, image, kBitmapUnflipped);
	pd->sprite->addSprite(sprite);
	pd->sprite->moveBy(sprite, 14, 14);
	pd->sprite->updateAndDrawSprites();
	//pd->sprite->drawSprites();

	pd->sprite->removeAllSprites();


	//pd->graphics->fillEllipse(screenCenter.x, screenCenter.y, 128, 128, 0.0f, 0.0f, kColorXOR);
	pd->system->drawFPS(0, 0);
	//int gifFrameCount = pd->graphics
	//uint16_t gifCount = GetBitmapTableCount(assets.gifTest);
	//pd->graphics->drawBitmap(pd->graphics->getTableBitmap(assets.gifTest, g.frameCount % gifCount), screenCenter.x, screenCenter.y, kBitmapUnflipped);

	//pd->graphics->setFont(font);
	//pd->graphics->drawText("HELLO", strlen("HELLO"), kUTF8Encoding, 10, 10);

	g.frameCount++;
	pd->sprite->freeSprite(sprite);
	pd->graphics->freeBitmap(image);
	return 1;
}

