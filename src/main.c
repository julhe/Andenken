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
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if ( event == kEventInit )
	{
		const char* err;
		font = pd->graphics->loadFont(fontpath, &err);
		
		if ( font == NULL )
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);

		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->display->setRefreshRate(50.0f);
		pd->system->setUpdateCallback(update, pd);
	}
	
	return 0;
}

// ---------------------------------------------------------
// .* Game Code *.
// ---------------------------------------------------------

typedef enum {
	tGround,
	tWall,
} TileType;

typedef struct {
	int x, y;
} Vec2i;

#define WORLD_TILE_COUNT 8
#define WORLD_TILE_PX 16
static struct {
	unsigned int frameCount;
	Vec2i cameraPos;

	TileType map[WORLD_TILE_COUNT][WORLD_TILE_COUNT];

}g;

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;
	
	pd->graphics->clear(kColorWhite); // TODO: remove clear 
	pd->graphics->setFont(font);

	PDButtons buttonsCurrent, buttonsPushed, buttonsReleased;
	pd->system->getButtonState(&buttonsCurrent, &buttonsPushed, &buttonsReleased);
	
	if ((buttonsCurrent & kButtonLeft) == kButtonLeft) { g.cameraPos.x++; }
	if ((buttonsCurrent & kButtonRight) == kButtonRight) { g.cameraPos.x--; }
	if ((buttonsCurrent & kButtonUp) == kButtonUp) { g.cameraPos.y++; }
	if ((buttonsCurrent & kButtonDown) == kButtonDown) { g.cameraPos.y--; }

	for (int x = 0; x < WORLD_TILE_COUNT; x++) {
		for (int y = 0; y < WORLD_TILE_COUNT; y++) {
			pd->graphics->fillRect(
				x * WORLD_TILE_PX + g.cameraPos.x, 
				y * WORLD_TILE_PX + g.cameraPos.y, 
				WORLD_TILE_PX, 
				WORLD_TILE_PX, 
				(x ^ y) % 2 );
		}
	}

	pd->system->drawFPS(0, 0);
	g.frameCount++;
	return 1;
}

