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

    changeState(STATE_TITLE);

    while (1)
    {
        VDP_waitVSync();
    }
    return 0;
}

void changeState(GameState newState)
{
    SYS_disableInts();

    if (firstdraw == FALSE) VDP_resetScreen();
    else firstdraw = FALSE;
    gamestate = newState;
    u16 ind = TILE_USERINDEX;

    if (gamestate == STATE_TITLE)
    {
        VDP_drawImageEx(PLAN_B, &img_title, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, ind), 0, 0, TRUE, TRUE);
        VDP_drawText("PRESS START", 27, 24);
    }
    else if (gamestate == STATE_MENU)
    {
        VDP_clearText(28, 24, 10);
        VDP_drawText("Select Maya sprite", 2, 2);
    }

    SYS_enableInts();
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
