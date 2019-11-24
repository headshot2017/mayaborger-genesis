#include <genesis.h>
#include "gfx.h"
#include "sprite.h"

#define ANIM_STAND      0
#define ANIM_RUN        1
#define ANIM_JUMP       2

#define ENT_BORGER      0
#define ENT_THINKER      1

typedef enum
{
    STATE_TITLE,
    STATE_MENU,
    STATE_INGAME,
    STATE_RESULTS
} GameState;

Sprite* spr_player;
bool pl_canmove;
fix32 pl_x;
fix32 pl_y;
fix32 pl_hspeed;
fix32 pl_vspeed;
fix32 pl_x_acc;
fix32 pl_gravity;

Sprite* spr_entity[64];
fix32 ent_x[64];
fix32 ent_y[64];
fix32 ent_vspeed[64];
fix32 ent_gravity[64];

void changeState(GameState newState);
void stateLoop();

static void joyEvent(u16 joy, u16 changed, u16 state);
static void updatePlayerPhys();
static void updatePlayerAnim();
static void updateEntities();
static void addEntity(s16 type);
bool boundingBox(s16 x1, s16 y1, s16 w1, s16 h1,  s16 x2, s16 y2, s16 w2, s16 h2)
{
    return x1 < x2+w2 && x2 < x1+w1 && y1 < y2+h2 && y2 < y1+h1;
}

GameState gamestate;
u8 spr_mayaskin;
u16 m_borger_score;
fix32 ground_y = FIX32(24*8);
const char *str_mayaskin[4] = {"Normal Maya", "Nerdy Maya", "Magician Maya", "Thief Maya"};

int main()
{
    JOY_setEventHandler(joyEvent);
    SPR_init();
    VDP_setPaletteColors(0, (u16*) palette_black, 64);

    spr_mayaskin = m_borger_score = 0;
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
        char msg[96];
        sprintf(msg, "P1 ate %d borgers", m_borger_score);
        VDP_clearText(1, 1, 40-1);
        VDP_drawText(msg, 1, 1);
        VDP_drawText("Time: 60", 40-1-8, 1);

        updatePlayerPhys();
        updatePlayerAnim();
        updateEntities();

        if (random() % 5000 >= 4900)
        {
            bool is_thinker = (random() % 500 >= 400);
            addEntity(is_thinker);
        }
    }
}

void changeState(GameState newState)
{
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

        spr_player = SPR_addSprite(&maya_sprite, 160-32, 224/2, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        SPR_setAnim(spr_player, ANIM_STAND);
        SPR_setHFlip(spr_player, TRUE);
        pl_canmove = TRUE;
        pl_gravity = pl_hspeed = pl_vspeed = pl_x_acc = FIX32(0);
        pl_x = FIX32(160-32);
        pl_y = FIX32(224/2);
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
    else if (gamestate == STATE_MENU)
    {
        if (changed & state & BUTTON_START) changeState(STATE_INGAME);
    }

    else if (gamestate == STATE_INGAME)
    {
        if (changed & state & (BUTTON_A | BUTTON_B | BUTTON_C) && pl_y >= ground_y-FIX32(62) && pl_canmove)
            pl_vspeed = FIX32(-6);
    }
}

static void updatePlayerPhys()
{
    u16 value = (pl_canmove) ? JOY_readJoypad(JOY_1) : 0;
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
    if (pl_x < FIX32(-16)) pl_x = FIX32(-16);
    if (pl_x > FIX32(320-48)) pl_x = FIX32(320-48);
    if (pl_y > ground_y-FIX32(62)) pl_y = ground_y-FIX32(62);

    SPR_setPosition(spr_player, fix32ToInt(pl_x), fix32ToInt(pl_y));
}

static void updatePlayerAnim()
{
    u16 value = (pl_canmove) ? JOY_readJoypad(JOY_1) : 0;
    if (value & BUTTON_LEFT) SPR_setHFlip(spr_player, FALSE);
    if (value & BUTTON_RIGHT) SPR_setHFlip(spr_player, TRUE);

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
        if (value & BUTTON_RIGHT || value & BUTTON_LEFT)
            SPR_setAnim(spr_player, ANIM_RUN);
        else
            SPR_setAnim(spr_player, ANIM_STAND);
    }
}

static void updateEntities()
{
    for (u16 i=0; i<64; i+=1)
    {
        if (spr_entity[i])
        {
            ent_vspeed[i] += ent_gravity[i];
            ent_y[i] += ent_vspeed[i];
            if (ent_y[i] >= ground_y-FIX32(24))
            {
                SPR_releaseSprite(spr_entity[i]);
                spr_entity[i] = NULL;
            }
            else if (boundingBox(fix32ToInt(pl_x), fix32ToInt(pl_y), 48, 48, fix32ToInt(ent_x[i]), fix32ToInt(ent_y[i]), 24, 24))
            {
                if (spr_entity[i]->animInd == ENT_BORGER)
                    m_borger_score++;
                else
                {
                    m_borger_score = 0;
                }

                SPR_releaseSprite(spr_entity[i]);
                spr_entity[i] = NULL;
                continue;
            }
            else
            {
                SPR_setPosition(spr_entity[i], fix32ToInt(ent_x[i]), fix32ToInt(ent_y[i]));
            }
        }
    }
}

static void addEntity(s16 type)
{
    s16 i = -1;
    for (s16 ii=0; ii<64; ii+=1)
    {
        if (!spr_entity[ii])
        {
            i = ii;
            break;
        }
    }

    if (i != -1)
    {
        ent_x[i] = FIX32(random() % (320-24));
        ent_y[i] = FIX32(-24);
        ent_vspeed[i] = FIX32(0);
        ent_gravity[i] = FIX32(0.02);

        spr_entity[i] = SPR_addSprite(&pickup_sprites, fix32ToInt(ent_x[i]), -24, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
        SPR_setAnim(spr_entity[i], type);
    }
}
