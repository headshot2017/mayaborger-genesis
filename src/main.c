#include <genesis.h>
#include "gfx.h"
#include "sprite.h"

#define ANIM_STAND      0
#define ANIM_RUN        1
#define ANIM_JUMP       2
#define ANIM_DIE        3
#define ANIM_GET_UP     4

#define ENT_BORGER      0
#define ENT_THINKER     1
#define ENT_ACCESSORY   2

typedef enum
{
    STATE_TITLE,
    STATE_MENU,
    STATE_INGAME,
    STATE_RESULTS
} GameState;

const SpriteDefinition* maya_sprites[2] = {&maya_sprite, &nerdymaya_sprite};
const SpriteDefinition* maya_sprite_accessory[2] = {NULL, &ema_glasses};
Sprite* spr_player;
bool pl_canmove;
u16 pl_deadtime;
s8 pl_dir;
fix32 pl_x;
fix32 pl_y;
fix32 pl_hspeed;
fix32 pl_vspeed;
fix32 pl_x_acc;
fix32 pl_gravity;

Sprite* spr_entity[64];
s16 ent_type[64];
fix32 ent_x[64];
fix32 ent_y[64];
fix32 ent_hspeed[64];
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
u16 m_timeleft;
fix32 ground_y = FIX32(24*8);
const char *str_mayaskin[4] = {"Normal Maya", "Nerdy Maya", "Magician Maya", "Thief Maya"};

int main()
{
    JOY_setEventHandler(joyEvent);
    SPR_init();
    VDP_setPaletteColors(0, (u16*) palette_black, 64);

    spr_mayaskin = m_borger_score = m_timeleft = pl_deadtime = 0;
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
        m_timeleft--;
        if (m_timeleft == 0)
        {
            changeState(STATE_RESULTS);
            return;
        }

        char msg[96];
        sprintf(msg, "P1 ate %d borgers", m_borger_score);
        VDP_clearText(1, 1, 40-1);
        VDP_drawText(msg, 1, 1);
        sprintf(msg, "Time: %d", m_timeleft/60+1);
        VDP_drawText(msg, 40-1-8, 1);

        updatePlayerPhys();
        updatePlayerAnim();
        updateEntities();

        if (random() % 5000 >= 4850)
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
    VDP_clearSprites();

    u16 ind = TILE_USERINDEX;
    char msg[96];
    GameState oldState = gamestate;
    gamestate = newState;

    // destroy old state
    if (oldState == STATE_MENU)
    {
        SPR_releaseSprite(spr_player);
        spr_player = NULL;
    }
    else if (oldState == STATE_INGAME)
    {
        SPR_releaseSprite(spr_player);
        spr_player = NULL;
        for (u16 i=0; i<64; i++)
        {
            if (spr_entity[i])
            {
                SPR_releaseSprite(spr_entity[i]);
                spr_entity[i] = NULL;
            }
        }
    }

    // initialize new state
    if (newState == STATE_TITLE)
    {
        VDP_drawImageEx(PLAN_B, &img_title, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, ind), 0, 0, TRUE, TRUE);
        VDP_setPalette(PAL1, palette_black);

        VDP_setTextPalette(PAL0);
        VDP_drawText("PRESS START", 27, 24);
        VDP_setTextPalette(PAL1);
        VDP_drawText("DEMO", 40-7, 7);
    }
    else if (newState == STATE_MENU)
    {
        VDP_setPaletteColor(PAL0, 0xffff);
        VDP_setPalette(PAL1, palette_black);
        VDP_setTextPalette(PAL1);

        spr_player = SPR_addSprite(maya_sprites[spr_mayaskin], 320 - 128, 224/2 - 48, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        VDP_setPalette(PAL2, maya_sprites[spr_mayaskin]->palette->data);
        SPR_setAnim(spr_player, ANIM_RUN);
        SPR_setHFlip(spr_player, TRUE);

        VDP_drawText("Setup game", 2, 1);
        VDP_drawText("How many borgers can you eat in", 4, 3);
        VDP_drawText("60 seconds?", 4, 4);
        VDP_drawText("<     >", 24, 17);
        VDP_drawText("Press START to play", 38-19, 26);
    }

    else if (newState == STATE_INGAME)
    {
        VDP_setPaletteColor(PAL0, 0xffff);
        VDP_setPalette(PAL1, img_ground.palette->data);
        VDP_setPalette(PAL2, maya_sprites[spr_mayaskin]->palette->data);
        VDP_setPalette(PAL3, pickup_sprites.palette->data);
        VDP_setTextPalette(PAL1);
        VDP_drawImageEx(PLAN_B, &img_ground, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, ind), 0, fix32ToInt(ground_y)/8, TRUE, TRUE);

        spr_player = SPR_addSprite(maya_sprites[spr_mayaskin], 160-32, 224/2, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
        SPR_setAnim(spr_player, ANIM_STAND);
        SPR_setHFlip(spr_player, TRUE);
        pl_dir = 1;
        pl_canmove = TRUE;
        pl_deadtime = m_borger_score = 0;
        pl_gravity = pl_hspeed = pl_vspeed = pl_x_acc = FIX32(0);
        pl_x = FIX32(160-32);
        pl_y = FIX32(224/2);
        m_timeleft = 60*60;
    }

    else if (newState == STATE_RESULTS)
    {
        sprintf(msg, "You ate %d borgers in 60 seconds!", m_borger_score);
        VDP_setPaletteColor(PAL0, 0xffff);
        VDP_setPalette(PAL1, palette_black);
        VDP_drawText(msg, 1, 1);
        VDP_drawText("Press any button to continue", 6, 26);
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

        if (changed & state & BUTTON_LEFT)
        {
            if (spr_mayaskin == 0) spr_mayaskin = 2;
            spr_mayaskin--;

            SPR_setDefinition(spr_player, maya_sprites[spr_mayaskin]);
            VDP_setPalette(PAL2, maya_sprites[spr_mayaskin]->palette->data);
            SPR_setAnim(spr_player, ANIM_RUN);
        }
        if (changed & state & BUTTON_RIGHT)
        {
            spr_mayaskin++;
            if (spr_mayaskin >= 2) spr_mayaskin = 0;

            SPR_setDefinition(spr_player, maya_sprites[spr_mayaskin]);
            VDP_setPalette(PAL2, maya_sprites[spr_mayaskin]->palette->data);
            SPR_setAnim(spr_player, ANIM_RUN);
        }
    }

    else if (gamestate == STATE_INGAME)
    {
        if (changed & state & (BUTTON_A | BUTTON_B | BUTTON_C) && pl_y >= ground_y-FIX32(62) && pl_canmove)
            pl_vspeed = FIX32(-6);
    }

    else if (gamestate == STATE_RESULTS)
    {
        if (changed & state & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START))
            changeState(STATE_MENU);
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
        if (pl_canmove)
        {
            if (pl_hspeed > 0) pl_x_acc = FIX32(-0.25);
            else if (pl_hspeed < 0) pl_x_acc = FIX32(0.25);
            else pl_x_acc = FIX32(0);
        }
        else
        {
            if (pl_y >= ground_y-FIX32(62))
            {
                if (pl_hspeed > 0) pl_x_acc = FIX32(-0.25);
                else if (pl_hspeed < 0) pl_x_acc = FIX32(0.25);
                else pl_x_acc = FIX32(0);
            }
            else pl_x_acc = FIX32(0);
        }
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
    if (value & BUTTON_LEFT) {SPR_setHFlip(spr_player, FALSE); pl_dir = -1;}
    if (value & BUTTON_RIGHT) {SPR_setHFlip(spr_player, TRUE); pl_dir = 1;}

    if (pl_canmove)
    {
        if (pl_y < ground_y-FIX32(62))
        {
            SPR_setAnim(spr_player, ANIM_JUMP);
            if (pl_vspeed >= FIX32(-1.2))
            {
                if (spr_player->frameInd >= 6) {SPR_setFrame(spr_player, 7);}
            }
            else SPR_setFrame(spr_player, 0);
        }

        else
        {
            if (value & BUTTON_RIGHT || value & BUTTON_LEFT)
                SPR_setAnim(spr_player, ANIM_RUN);
            else
                SPR_setAnim(spr_player, ANIM_STAND);
        }
    }

    else
    {
        if (spr_player->animInd == ANIM_DIE)
        {
            if (spr_player->frameInd >= spr_player->animation->length-2)
                SPR_setFrame(spr_player, spr_player->animation->length-2);

            if (pl_y >= ground_y-FIX32(62) && pl_hspeed == 0)
            {
                pl_deadtime--;
                if (!pl_deadtime)
                {
                    SPR_setAnim(spr_player, ANIM_GET_UP);
                }
            }
        }

        else if (spr_player->animInd == ANIM_GET_UP)
        {
            if (spr_player->frameInd >= spr_player->animation->length-2)
            {
                pl_canmove = TRUE; // the animation is set automatically afterwards
            }
        }
    }
}

static void updateEntities()
{
    for (u16 i=0; i<64; i+=1)
    {
        if (spr_entity[i])
        {
            ent_vspeed[i] += ent_gravity[i];
            ent_x[i] += ent_hspeed[i];
            ent_y[i] += ent_vspeed[i];

            if (ent_type[i] == ENT_BORGER || ent_type[i] == ENT_THINKER)
            {
                if (ent_y[i] >= ground_y-FIX32(24))
                {
                    SPR_releaseSprite(spr_entity[i]);
                    spr_entity[i] = NULL;
                }
                else
                {
                    bool coll = FALSE;
                    if (boundingBox(fix32ToInt(pl_x)+8, fix32ToInt(pl_y), 44, 48, fix32ToInt(ent_x[i]), fix32ToInt(ent_y[i]), 24, 24) && spr_entity[i]->animInd == ENT_BORGER)
                    {
                        m_borger_score++;
                        coll = TRUE;
                    }
                    else if (boundingBox(fix32ToInt(pl_x)+6, fix32ToInt(pl_y), 40, 48, fix32ToInt(ent_x[i]), fix32ToInt(ent_y[i]), 8, 24) && spr_entity[i]->animInd == ENT_THINKER)
                    {
                        m_borger_score = 0;
                        coll = TRUE;
                        SPR_setAnim(spr_player, ANIM_DIE);
                        pl_canmove = FALSE;
                        pl_deadtime = 45;
                        if (spr_mayaskin > 0) addEntity(ENT_ACCESSORY);
                    }

                    if (coll)
                    {
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
            else if (ent_type[i] == ENT_ACCESSORY)
            {
                if (ent_y[i] >= ground_y-FIX32(spr_entity[i]->frame->h)) // bounce.
                {
                    ent_vspeed[i] = -ent_vspeed[i] + FIX32(1);
                    if (ent_vspeed[i] > FIX32(0))
                    {
                        ent_vspeed[i] = FIX32(0);
                        ent_y[i] = ground_y-FIX32(spr_entity[i]->frame->h);
                    }
                }

                if (ent_x[i] < FIX32(0 - spr_entity[i]->frame->w) || ent_x[i] > FIX32(320)) // off-screen. destroy.
                {
                    SPR_releaseSprite(spr_entity[i]);
                    spr_entity[i] = NULL;
                    continue;
                }
                else
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
        ent_type[i] = type;

        if (type == ENT_BORGER || type == ENT_THINKER)
        {
            ent_x[i] = FIX32(random() % (320-24));
            ent_y[i] = FIX32(-24);
            ent_vspeed[i] = ent_hspeed[i] = FIX32(0);
            ent_gravity[i] = FIX32(0.02);
            spr_entity[i] = SPR_addSprite(&pickup_sprites, fix32ToInt(ent_x[i]), -24, TILE_ATTR(PAL3, FALSE, FALSE, FALSE));
            SPR_setAnim(spr_entity[i], type);
        }
        else if (type == ENT_ACCESSORY)
        {
            ent_x[i] = pl_x+FIX32(32);
            ent_y[i] = pl_y+FIX32(8);
            ent_vspeed[i] = FIX32(-1);
            ent_hspeed[i] = FIX32(-pl_dir * 3);
            ent_gravity[i] = FIX32(0.25);
            spr_entity[i] = SPR_addSprite(maya_sprite_accessory[spr_mayaskin], fix32ToInt(ent_x[i]), fix32ToInt(ent_y[i]), TILE_ATTR(PAL2, FALSE, FALSE, (pl_dir > 0) ? TRUE : FALSE));
        }
    }
}
