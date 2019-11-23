#include <genesis.h>
#include "gfx.h"
#include "sprite.h"

#define ANIM_STAND      0
#define ANIM_RUN        1
#define ANIM_JUMP       2

typedef enum
{
    STATE_TITLE,
    STATE_MENU,
    STATE_INGAME,
    STATE_RESULTS
} GameState;

Sprite* spr_player;
fix32 pl_x;
fix32 pl_y;
fix32 pl_hspeed;
fix32 pl_vspeed;
fix32 pl_x_acc;
fix32 pl_gravity;

Sprite* spr_entity[64];
fix32 ent_y[64];
fix32 ent_vspeed[64];
fix32 ent_gravity[64];

void changeState(GameState newState);
void stateLoop();

static void joyEvent(u16 joy, u16 changed, u16 state);
static void updatePlayerPhys();
static void updatePlayerAnim();
static void updateEntities();

GameState gamestate;
u8 spr_mayaskin;
fix32 ground_y = FIX32(24*8);
const char *str_mayaskin[4] = {"Normal Maya", "Nerdy Maya", "Magician Maya", "Thief Maya"};

int main()
{
    JOY_setEventHandler(joyEvent);
    SPR_init();
    VDP_setPaletteColors(0, (u16*) palette_black, 64);

    spr_mayaskin = 0;
    changeState(STATE_TITLE);

    while (1)
    {
        stateLoop();

        SPR_update();
        VDP_waitVSync();
    }
    return 0;
}

void stateLoop()
{
    if (gamestate == STATE_MENU)
    {
        VDP_clearText(6, 12, 32);
        VDP_drawText(str_mayaskin[spr_mayaskin], 6, 12);
    }
    else if (gamestate == STATE_INGAME)
    {
        updatePlayerPhys();
        updatePlayerAnim();
        updateEntities();
    }
}

void changeState(GameState newState)
{
    SYS_disableInts();
    VDP_clearPlan(PLAN_B, TRUE);
    VDP_clearTextArea(0, 0, 320/8, 224/8);

    u16 ind = TILE_USERINDEX;
    GameState oldState = gamestate;
    gamestate = newState;

    // destroy old state
    if (oldState == STATE_MENU)
    {
        SPR_releaseSprite(spr_player);
        spr_player = NULL;
    }

    // initialize new state
    if (newState == STATE_TITLE)
    {
        VDP_setTextPalette(PAL0);
        VDP_drawImageEx(PLAN_B, &img_title, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, ind), 0, 0, TRUE, TRUE);
        VDP_drawText("PRESS START", 27, 24);
    }
    else if (newState == STATE_MENU)
    {
        VDP_setPaletteColor(PAL0, 0xffff);
        VDP_setPalette(PAL1, palette_black);
        VDP_setTextPalette(PAL1);

        spr_player = SPR_addSprite(&maya_sprite, 320 - 128, 224/2 - 48, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        VDP_setPalette(PAL2, maya_sprite.palette->data);
        SPR_setAnim(spr_player, ANIM_RUN);
        SPR_setHFlip(spr_player, TRUE);

        VDP_drawText("Setup game", 2, 1);
        VDP_drawText("<     >", 24, 17);
        VDP_drawText("Press START to play", 38-19, 26);
    }

    else if (newState == STATE_INGAME)
    {
        VDP_setPaletteColor(PAL0, 0xffff);
        VDP_setPalette(PAL1, img_ground.palette->data);
        VDP_setPalette(PAL2, maya_sprite.palette->data);
        VDP_setPalette(PAL3, pickup_sprites.palette->data);
        VDP_setTextPalette(PAL1);
        VDP_drawImageEx(PLAN_B, &img_ground, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, ind), 0, fix32ToInt(ground_y)/8, TRUE, TRUE);

        VDP_drawText("P1 ate 0 borgers", 1, 1);

        spr_player = SPR_addSprite(&maya_sprite, 160-32, 224/2, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        SPR_setAnim(spr_player, ANIM_STAND);
        SPR_setHFlip(spr_player, TRUE);
        pl_gravity = pl_hspeed = pl_vspeed = pl_x_acc = FIX32(0);
        pl_x = FIX32(160-32);
        pl_y = FIX32(224/2);
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
    else if (gamestate == STATE_MENU)
    {
        if (changed & state & BUTTON_START) changeState(STATE_INGAME);
    }

    else if (gamestate == STATE_INGAME)
    {
        if (changed & state & (BUTTON_A | BUTTON_B | BUTTON_C) && pl_y >= ground_y-FIX32(62))
            pl_vspeed = FIX32(-6);
    }
}

static void updatePlayerPhys()
{
    u16 value = JOY_readJoypad(JOY_1);
    if (value & BUTTON_LEFT)
        pl_x_acc = FIX32(-0.25);
    else if (value & BUTTON_RIGHT)
        pl_x_acc = FIX32(0.25);
    else
    {
        if (pl_hspeed > 0) pl_x_acc = FIX32(-0.25);
        else if (pl_hspeed < 0) pl_x_acc = FIX32(0.25);
        else pl_x_acc = FIX32(0);
    }

    if (pl_y < ground_y-FIX32(62)) // above ground
    {
        pl_gravity = FIX32(0.2);
    }
    else // on ground
    {
        pl_gravity = FIX32(0);
    }

    pl_hspeed += pl_x_acc;
    pl_vspeed += pl_gravity;
    if (pl_hspeed > FIX32(4)) pl_hspeed = FIX32(4);
    if (pl_hspeed < FIX32(-4)) pl_hspeed = FIX32(-4);
    pl_x += pl_hspeed;
    pl_y += pl_vspeed;
    if (pl_y > ground_y-FIX32(62)) pl_y = ground_y-FIX32(62);

    SPR_setPosition(spr_player, fix32ToInt(pl_x), fix32ToInt(pl_y));
}

static void updatePlayerAnim()
{
    if (pl_hspeed < 0) SPR_setHFlip(spr_player, FALSE);
    if (pl_hspeed > 0) SPR_setHFlip(spr_player, TRUE);

    if (pl_y < ground_y-FIX32(62))
    {
        SPR_setAnim(spr_player, ANIM_JUMP);
        if (pl_vspeed >= FIX32(-1.2))
        {
            if (spr_player->frameInd >= 6) {SPR_setFrame(spr_player, 7);}
        }
        else {SPR_setFrame(spr_player, 0);}
    }

    else
    {
        if (pl_hspeed == FIX32(0))
            SPR_setAnim(spr_player, ANIM_STAND);
        else
            SPR_setAnim(spr_player, ANIM_RUN);
    }
}

static void updateEntities()
{

}
