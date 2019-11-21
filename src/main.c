#include <genesis.h>
#include "gfx.h"

typedef enum
{
    STATE_TITLE,
    STATE_MENU
} GameState;

void changeState(GameState newState);
GameState gamestate;
bool firstdraw = TRUE;

static void joyEvent(u16 joy, u16 changed, u16 state);

int main()
{
    JOY_setEventHandler(joyEvent);
    VDP_setPaletteColors(0, (u16*) palette_black, 64);

    changeState(STATE_TITLE);

    while (1)
    {
        VDP_waitVSync();
    }
    return 0;
}

void changeState(GameState newState)
{
    if (!firstdraw) VDP_resetScreen();
    else firstdraw = FALSE;
    gamestate = newState;
    u16 ind = TILE_USERINDEX;

    if (gamestate == STATE_TITLE)
    {
        VDP_drawImageEx(PLAN_A, &img_title, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, ind), 0, 0, TRUE, TRUE);
        VDP_setTextPriority(2);
        VDP_drawText("PRESS START", 28, 24);
    }
}

static void joyEvent(u16 joy, u16 changed, u16 state)
{
    if (gamestate == STATE_TITLE)
    {
        if (changed & state & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START))
        {
            changeState(STATE_MENU);
        }
    }
}
