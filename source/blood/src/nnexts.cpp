//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 Nuke.YKT
Copyright (C) NoOne

This file is part of NBlood.

NBlood is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------


///////////////////////////////////////////////////////////////////
// This file provides modern features for mappers.
// For full documentation please visit http://cruo.bloodgame.ru/xxsystem
///////////////////////////////////////////////////////////////////


#ifdef NOONE_EXTENSIONS
#include <random>
#include "nnexts.h"
#include "nnextsif.h"
#include "nnextslaser.h"
#include "eventq.h"
#include "aicdud.h"
#include "triggers.h"
#include "sectorfx.h"
#include "globals.h"
#include "endgame.h"
#include "weapon.h"
#include "mmulti.h"
#include "view.h"
#include "tile.h"
#include "trig.h"
#include "sfx.h"
#include "seq.h"
#include "ai.h"
#include "mirrors.h"
#ifdef WITHKPLIB
#include "kplib.h"
#endif

#define kMaxPathSpriteSectors 512
#define kMaxLasers 2048

bool gAllowTrueRandom = false;
bool gEventRedirectsUsed = false;
signed char gCustomMusicStatus = -1;
static VIEWCAMERA gViewCam; // this one for view
static VIEWCAMERA gMeCam;   // this one for sounds

SPRITEMASS gSpriteMass[];   // cache for getSpriteMassBySize();

IDLIST gProxySpritesList(false);
IDLIST gSightSpritesList(false);
IDLIST gImpactSpritesList(false);
IDLIST gPhysSpritesList(false);
IDLIST gFlwSpritesList(false);

IDLIST* gPathSprList[kMaxPathSpriteSectors]; // this is a lists of sprites that sector makes move
uint16_t gPathSprIndex[kMaxSectors];
uint16_t gPathSprCount = 0;

LASER* gLaser[kMaxLasers];
uint16_t gLasersCount = 0;

#ifdef NNEXTS_USE_RES_SYS
bool gExternalFilesAdded = false;
EXTERNAL_FILES_LIST gExternFiles[] =
{
    { kCdudeFileNamePrefixWild, kCdudeFileExt },
};
#endif

// SPRITES_NEAR_SECTORS
// Intended for move sprites that is close to the outside walls with
// TranslateSector and/or zTranslateSector similar to Powerslave(Exhumed) way
// --------------------------------------------------------------------------
SPRINSECT gSprNSect;
// --------------------------------------------------------------------

// indicate if object is part of trigger
// sequence that contains event
// causer channel
OBJECT_STATUS1* gEvCauser = NULL;

short gEffectGenCallbacks[] = {
    
    kCallbackFXFlameLick,
    kCallbackFXFlareSpark,
    kCallbackFXFlareSparkLite,
    kCallbackFXZombieSpurt,
    kCallbackFXBloodSpurt,
    kCallbackFXArcSpark,
    kCallbackFXTeslaAlt,

};


TRPLAYERCTRL gPlayerCtrl[kMaxPlayers];
std::default_random_engine gStdRandom;

EXPLOSION_EXTRA gExplodeExtra[] =
{
    {3, 303, true},     {4, 304, false},    {4, 305, false},
    {9, 307, true},     {5, 307, true},     {4, 303, false},
    {4, 303, false},    {4, 303, false},
};

VECTORINFO_EXTRA gVectorInfoExtra[] = {
    1207,1207,      1001,1001,      4001,4002,
    431,431,        1002,1002,      359,359,
    521,521,        513,513,        499,499,
    9012,9014,      1101,1101,      1207,1207,
    499,495,        495,496,        9013,499,
    1307,1308,      499,499,        499,499,
    499,499,        499,499,        351,351,
    0,0,            357,499
};

MISSILEINFO_EXTRA gMissileInfoExtra[] = {
    1207, 1207,    false, false, false, false, false, true, false,     true,
    420, 420,      false, true, true, false, false, false, false,      true,
    471, 471,      false, false, false, false, false, false, true,     false,
    421, 421,      false, true, false, true, false, false, false,      false,
    1309, 351,     false, true, false, false, false, false, false,     true,
    480, 480,      false, true, false, true, false, false, false,      false,
    470, 470,      false, false, false, false, false, false, true,     true,
    489, 490,      false, false, false, false, false, true, false,     true,
    462, 351,      false, true, false, false, false, false, false,     true,
    1203, 172,     false, false, true, false, false, false, false,     true,
    0,0,           false, false, true, false, false, false, false,     true,
    1457, 249,     false, false, false, false, false, true, false,     true,
    480, 489,      false, true, false, true, false, false, false,      false,
    480, 489,      false, false, false, true, false, false, false,     false,
    480, 489,      false, false, false, true, false, false, false,     false,
    491, 491,      true, true, true, true, true, true, true,           true,
    520, 520,      false, false, false, false, false, true, false,     true,
    520, 520,      false, false, false, false, false, true, false,     true,
};

THINGINFO_EXTRA gThingInfoExtra[] = {
    true,   true,   true,   false,  false,
    false,  false,  false,  false,  false,
    false,  false,  false,  false,  false,
    true,   false,  false,  true,   true,
    true,   true,   false,  false,  false,
    false,  false,  true,   true,   true,
    true,   true,   true,   true,   true,
    true,
};

DUDEINFO_EXTRA gDudeInfoExtra[] = {
    
    { false,  false,  -1, -1, -1, -1, -1, -1 },     // 200
    { false,  false,  0, 9, 13, 13, 17, 14 },       // 201
    { false,  false,  0, 9, 13, 13, 17, 14 },       // 202
    { false,  true,   0, 8, 0, 8, -1, -1 },         // 203
    { false,  false,  0, 8, 0, 8, -1, -1 },         // 204
    { false,  true,   12, -1, 12, -1, -1, -1 },     // 205
    { true,   true,   0, 0, 0, 0, -1, -1 },         // 206
    { true,   false,  0, 0, 0, 0, -1, -1 },         // 207
    { false,  false,  0, 0, 0, 0, -1, -1 },         // 208
    { false,  false,  0, 0, 0, 0, -1, -1 },         // 209
    { true,   true,   0, 0, 0, 0, -1, -1 },         // 210
    { false,  true,   0, 8, 0, 8, -1, -1 },         // 211
    { false,  true,   0, 6, 0, 6, -1, -1 },         // 212
    { false,  true,   0, 7, 0, 7, -1, -1 },         // 213
    { false,  true,   0, 7, 0, 7, -1, -1 },         // 214
    { false,  true,   0, 7, 0, 7, -1, -1 },         // 215
    { false,  true,   0, 7, 0, 7, -1, -1 },         // 216
    { false,  true,   0, 9, 10, 10, -1, -1 },       // 217
    { false,  true,   0, 0, 0, 0, -1, -1 },         // 218
    { true,  false,   7, 7, 7, 7, -1, -1 },         // 219
    { false,  true,   0, 7, 0, 7, -1, -1 },         // 220
    { false,  false,  0, -1, 0, -1, -1, -1 },       // 221
    { false,  true,   0, -1, 0, -1, -1, -1 },       // 222
    { false,  false,  0, -1, 0, -1, -1, -1 },       // 223
    { false,  true,   0, -1, 0, -1, -1, -1 },       // 224
    { false,  false,  0, -1, 0, -1, -1, -1 },       // 225
    { false,  false,  -1, -1, -1, -1, -1, -1 },     // 226
    { false,  false,  0, 7, 0, 7, -1, -1 },         // 227
    { false,  false,  0, 7, 0, 7, -1, -1 },         // 228
    { false,  false,  0, 8, 0, 8, -1, -1 },         // 229
    { false,  false,  0, 9, 13, 13, 17, 14 },       // 230
    { false,  false,  0, 6, 9, 9, 10, 11 },         // 231
    { false,  false,  0, 6, 9, 9, 10, 11 },         // 232
    { false,  false,  0, 6, 9, 9, 10, 11 },         // 233
    { false,  false,  0, 6, 9, 9, 10, 11 },         // 234
    { false,  false,  0, 6, 9, 9, 10, 11 },         // 235
    { false,  false,  0, 6, 9, 9, 10, 11 },         // 236
    { false,  false,  0, 6, 9, 9, 10, 11 },         // 237
    { false,  false,  0, 6, 9, 9, 10, 11 },         // 238
    { false,  false,  3, 3, 3, 3, -1, -1 },         // 239
    { false,  false,  3, 3, 3, 3, -1, -1 },         // 240
    { false,  false,  3, 3, 3, 3, -1, -1 },         // 241
    { false,  false,  3, 3, 3, 3, -1, -1 },         // 242
    { false,  false,  -1, -1, -1, -1, -1, -1 },     // 243
    { false,  true,   10, -1, 10, -1, -1, -1 },     // 244
    { false,  true,   0, 6, 0, 6, -1, -1 },         // 245
    { false,  false,  0, 9, 13, 13, 17, 14 },       // 246
    { false,  false,  0, 9, 13, 13, 14, 14 },       // 247
    { false,  false,  0, 9, 13, 13, 14, 14 },       // 248
    { false,  false,  0, 9, 13, 13, 17, 14 },       // 249
    { false,  true,   0, 6, 8, 8, 10, 9 },          // 250
    { false,  true,   0, 8, 9, 9, 11, 10 },         // 251
    { false,  false,  -1, -1, -1, -1, -1, -1 },     // 252
    { false,  false,  -1, -1, -1, -1, -1, -1 },     // 253
    { false,  false,  0, 4, 0, 4, 0, 4 },           // 254 (seq offsets is dude AI state offsets here!)
    { false,  false,  -1, -1, -1, -1, -1, -1 },     // 255

};

struct GINPUTINFO
{
    int8_t valMin;
    int8_t valMax;
    int8_t valDef;
    int8_t valShl;
    uint8_t sync;
};

GINPUTINFO gInputInfo[] =
{
    { 0, 0,                              0, 0,  0x00 },     // 00 (-----) dummy
    { 0, 4,                              0, 9,  0x00 },     // 01 (-----) forward
    {-4, 0,                              0, 9,  0x00 },     // 02 (-----) backward
    {0,  4,                              0, 9,  0x00 },     // 03 (-----) strafe left
    {-4, 0,                              0, 9,  0x00 },     // 04 (-----) strafe right
    {0,  1,                              1, 0,  0x04 },     // 05 (--b--) jump
    {0,  1,                              1, 0,  0x04 },     // 06 (--b--) crouch
    {0,  1,                              1, 0,  0x04 },     // 07 (--b--) shoot1
    {0,  1,                              1, 0,  0x04 },     // 08 (--b--) shoot2
    {0,  1,                              1, 0,  0x01 },     // 09 (k----) action
    {0,  4,                              4, 20, 0x00 },     // 10 (-----) turn right
    {-4, 0,                              -4, 20, 0x00 },    // 11 (-----) turn left
    {0,  4,                              4, 16, 0x08 },     // 12 (---l-) look up
    {-4, 0,                              -4, 16, 0x08 },    // 13 (---l-) look down
    {0,  1,                              1, 0,  0x01 },     // 14 (k----) center
    {0,  1,                              1, 0,  0x01 },     // 15 (k----) holster
    {0,  LENGTH(PLAYER::hasWeapon)-1,    0, 0,  0x10 },     // 16 (----w) sel weap
    {0,  1,                              1, 0,  0x11 },     // 17 (----w) next weap
    {0,  1,                              1, 0,  0x11 },     // 18 (----w) prev weap
    {0,  4,                              0, 0,  0x03 },     // 19 (ku---) use item
    {0,  1,                              1, 0,  0x03 },     // 20 (ku---) next item
    {0,  1,                              1, 0,  0x03 },     // 21 (ku---) prev item
    {0,  1,                              1, 0,  0x01 },     // 22 (k----) pause
    {0,  1,                              1, 0,  0x01 },     // 23 (k----) restart
    {0,  1,                              1, 0,  0x01 },     // 24 (k----) quit
};

LASER* laserGet(spritetype* pSpr);

char idListProcessProxySprite(int32_t nSpr)
{
    int i, okDist;
    bool causerPart;

    spritetype* pSpr = &sprite[nSpr];
    if (!xspriRangeIsFine(pSpr->extra)) return kListREMOVE;
    else if (isOnRespawn(pSpr))
        return kListSKIP; // don't process

    XSPRITE* pXSpr = &xsprite[pSpr->extra];
    if (pXSpr->locked) return kListSKIP; // don't process
    else if (pXSpr->isTriggered || !pXSpr->Proximity) return kListREMOVE; // remove from the list
    else if (!pXSpr->Interrutable && pXSpr->state != pXSpr->restState) // just time out
        return kListSKIP;

    okDist = (pSpr->statnum == kStatDude) ? 96 : ClipLow(pSpr->clipdist * 3, 32);
    causerPart = isPartOfCauserScript(OBJ_SPRITE, nSpr);


    // only check players
    if (pXSpr->DudeLockout)
    {
        PLAYER* pPlayer;
        for (i = connecthead; i >= 0; i = connectpoint2[i])
        {
            pPlayer = &gPlayer[i];
            if (!xsprIsFine(pPlayer->pSprite) || pPlayer->pXSprite->health <= 0)
                continue;

            if (CheckProximity(pPlayer->pSprite, pSpr->x, pSpr->y, pSpr->z, pSpr->sectnum, okDist))
            {
                trTriggerSprite(nSpr, pXSpr, kCmdSpriteProximity, pPlayer->nSprite);
                if (!causerPart)
                    break; // no point to keep going
            }
        }
    }
    // check all dudes
    else
    {
        spritetype* pDude;
        for (i = headspritestat[kStatDude]; i >= 0; i = nextspritestat[i])
        {
            pDude = &sprite[i];
            if (!xsprIsFine(pDude) || xsprite[sprite[i].extra].health <= 0) continue;
            else if (CheckProximity(pDude, pSpr->x, pSpr->y, pSpr->z, pSpr->sectnum, okDist))
            {
                trTriggerSprite(nSpr, pXSpr, kCmdSpriteProximity, i);
                if (!causerPart)
                    break; // no point to keep going
            }
        }
    }
    
    return kListOK;
}

char idListProcessSightSprite(int32_t nSpr)
{
    static int z[3];
    int i, j;
    spritetype* pSpr = &sprite[nSpr];
    PLAYER* pPlayer; spritetype* pPlaySpr;
    bool causerPart;

    if (!xspriRangeIsFine(pSpr->extra)) return kListREMOVE;
    else if (isOnRespawn(pSpr))
        return kListSKIP; // don't process

    XSPRITE* pXSpr = &xsprite[pSpr->extra];
    if (pXSpr->locked)  return kListSKIP; // don't process
    else if (pXSpr->isTriggered || (!pXSpr->Sight && !pXSpr->unused3)) return kListREMOVE; // remove from the list
    else if (!pXSpr->Interrutable && pXSpr->state != pXSpr->restState)
        return kListSKIP; // just time out

    // sprite is drawn for one of players
    if ((pXSpr->unused3 & kTriggerSpriteScreen) && gGameOptions.nGameType == kGameTypeSinglePlayer && TestBitString(show2dsprite, nSpr))
    {
        pPlayer = gMe;
        if (xsprIsFine(pPlayer->pSprite) && pPlayer->pXSprite->health)
        {
            trTriggerSprite(nSpr, pXSpr, kCmdSpriteSight, pPlayer->nSprite);
            ClearBitString(show2dsprite, nSpr);
        }

        return kListOK;
    }

    causerPart = isPartOfCauserScript(OBJ_SPRITE, nSpr);

    // check players
    for (i = connecthead; i >= 0; i = connectpoint2[i])
    {
        pPlayer = &gPlayer[i];
        if (!xsprIsFine(pPlayer->pSprite) || pPlayer->pXSprite->health <= 0)
            continue;

        pPlaySpr = pPlayer->pSprite; z[0] = pPlaySpr->z; GetSpriteExtents(pPlaySpr, &z[1], &z[2]);
        for (j = 0; j < 3; j++)
        {
            if (cansee(pSpr->x, pSpr->y, pSpr->z, pSpr->sectnum, pPlaySpr->x, pPlaySpr->y, z[j], pPlaySpr->sectnum))
            {
                if (pXSpr->Sight)
                {
                    trTriggerSprite(nSpr, pXSpr, kCmdSpriteSight, pPlayer->nSprite);
                }
                else if (pXSpr->unused3 & kTriggerSpriteAim)
                {
                    bool vector = (pSpr->cstat & CSTAT_SPRITE_BLOCK_HITSCAN);
                    if (!vector)
                        pSpr->cstat |= CSTAT_SPRITE_BLOCK_HITSCAN;

                    HitScan(pPlaySpr, pPlayer->zWeapon, pPlayer->aim.dx, pPlayer->aim.dy, pPlayer->aim.dz, CLIPMASK0 | CLIPMASK1, 0);
                    if (gHitInfo.hitsprite == nSpr)
                        trTriggerSprite(nSpr, pXSpr, kCmdSpriteSight, pPlayer->nSprite);

                    if (!vector)
                        pSpr->cstat &= ~CSTAT_SPRITE_BLOCK_HITSCAN;
                }

                break;
            }
        }

        if (j < 3 && !causerPart)
            break; // no point to keep going
    }

    return kListOK;
}

char idListProcessPhysSprite(int32_t nSpr)
{
    spritetype* pSpr = &sprite[nSpr];
    if (!xspriRangeIsFine(pSpr->extra))
        return kListREMOVE;

    XSPRITE* pXSpr = &xsprite[pSpr->extra];
    if (!(pXSpr->physAttr & kPhysMove) && !(pXSpr->physAttr & kPhysGravity))
        return kListREMOVE;

    viewBackupSpriteLoc(nSpr, pSpr);
    
    XSECTOR* pXSector = (sector[pSpr->sectnum].extra >= 0) ? &xsector[sector[pSpr->sectnum].extra] : NULL;
    
    bool uwater = false;
    int mass = gSpriteMass[pSpr->extra].mass;
    int airVel = gSpriteMass[pSpr->extra].airVel;

    int top, bottom;
    GetSpriteExtents(pSpr, &top, &bottom);

    if (pXSector != NULL)
    {
        if ((uwater = pXSector->Underwater) != 0)
            airVel <<= 6;

        if (pXSector->panVel != 0 && getflorzofslope(pSpr->sectnum, pSpr->x, pSpr->y) <= bottom)
        {
            int angle = pXSector->panAngle; int speed = 0;
            if (pXSector->panAlways || pXSector->state || pXSector->busy)
            {
                speed = pXSector->panVel << 9;
                if (!pXSector->panAlways && pXSector->busy)
                    speed = mulscale16(speed, pXSector->busy);
            }

            if (sector[pSpr->sectnum].floorstat & 64)
                angle = (angle + GetWallAngle(sector[pSpr->sectnum].wallptr) + 512) & 2047;
            
            int dx = mulscale30(speed, Cos(angle));
            int dy = mulscale30(speed, Sin(angle));
            xvel[nSpr] += dx;
            yvel[nSpr] += dy;

        }
    }

    actAirDrag(pSpr, airVel);

    if (pXSpr->physAttr & kPhysDebrisTouch)
    {
        PLAYER* pPlayer = NULL;
        for (int a = connecthead; a != -1; a = connectpoint2[a])
        {
            pPlayer = &gPlayer[a];
            if ((gSpriteHit[pPlayer->pSprite->extra].hit & 0xc000) == 0xc000 && (gSpriteHit[pPlayer->pSprite->extra].hit & 0x3fff) == nSpr)
            {
                int nSpeed = approxDist(xvel[pPlayer->pSprite->index], yvel[pPlayer->pSprite->index]);
                nSpeed = ClipLow(nSpeed - mulscale6(nSpeed, mass), 0x9000 - (mass << 3));

                xvel[nSpr] += mulscale30(nSpeed, Cos(pPlayer->pSprite->ang));
                yvel[nSpr] += mulscale30(nSpeed, Sin(pPlayer->pSprite->ang));

                gSpriteHit[pSpr->extra].hit = pPlayer->pSprite->index | 0xc000;
            }
        }
    }

    if (pXSpr->physAttr & kPhysGravity) pXSpr->physAttr |= kPhysFalling;
    if ((pXSpr->physAttr & kPhysFalling) || xvel[nSpr] || yvel[nSpr] || zvel[nSpr] || velFloor[pSpr->sectnum] || velCeil[pSpr->sectnum])
        debrisMove(nSpr);

    if (xvel[nSpr] || yvel[nSpr])
        pXSpr->goalAng = getangle(xvel[nSpr], yvel[nSpr]) & 2047;

    int ang = pSpr->ang & 2047;
    if ((uwater = spriteIsUnderwater(pSpr)) == false)
        evKill(nSpr, 3, kCallbackEnemeyBubble);
    else if (Chance(0x1000 - mass))
    {
        if (zvel[nSpr] > 0x100)
            debrisBubble(nSpr);

        if (ang == pXSpr->goalAng)
        {
            pXSpr->goalAng = (pSpr->ang + Random3(kAng60)) & 2047;
            debrisBubble(nSpr);
        }
    }

    int angStep = ClipLow(mulscale8(1, ((abs(xvel[nSpr]) + abs(yvel[nSpr])) >> 5)), (uwater) ? 1 : 0);
    if (ang < pXSpr->goalAng)
        pSpr->ang = ClipHigh(ang + angStep, pXSpr->goalAng);
    else if (ang > pXSpr->goalAng)
        pSpr->ang = ClipLow(ang - angStep, pXSpr->goalAng);

    int nSector = pSpr->sectnum;
    int cz = getceilzofslope(nSector, pSpr->x, pSpr->y);
    int fz = getflorzofslope(nSector, pSpr->x, pSpr->y);

    GetSpriteExtents(pSpr, &top, &bottom);
    if (fz >= bottom && gLowerLink[nSector] < 0 && !(sector[nSector].ceilingstat & 0x1))
        pSpr->z += ClipLow(cz - top, 0);
    
    if (cz <= top && gUpperLink[nSector] < 0 && !(sector[nSector].floorstat & 0x1))
        pSpr->z += ClipHigh(fz - bottom, 0);

    return kListOK;
}

char dudeIsAlive(spritetype* pSpr)
{
    return (pSpr->statnum == kStatDude && xsprIsFine(pSpr) && xsprite[pSpr->extra].health);
}

int nnExtGetStartHealth(spritetype* pSpr)
{
    int nRetn = 0;
    if (IsDudeSprite(pSpr))
    {
        XSPRITE* pXSpr = xspriRangeIsFine(pSpr->extra) ? &xsprite[pSpr->extra] : NULL;
        if (IsPlayerSprite(pSpr))       nRetn = gPlayerTemplate[pSpr->type - kDudePlayer1].startHealth;
        else if (pXSpr && pXSpr->data4) nRetn = pXSpr->data4;
        else                            nRetn = getDudeInfo(pSpr->type)->startHealth;
    }

    return ClipHigh(nRetn << 4, 65535);
}

void followTarget(spritetype* pSpr, spritetype* pTarg, int nMaxAng)
{
    if (cansee(pSpr->x, pSpr->y, pSpr->z, pSpr->sectnum, pTarg->x, pTarg->y, pTarg->z, pTarg->sectnum))
    {
        int zt, zb;
        int dx = pTarg->x - pSpr->x;
        int dy = pTarg->y - pSpr->y;
        
        int nAng = getangle(dx, dy);
        int nDAng = klabs(((nAng + kAng180 - pSpr->ang) & kAngMask) - kAng180);
        
        if (nMaxAng && nDAng >= nMaxAng)
            return;
        
        GetSpriteExtents(pSpr, &zt, &zb);

        int dz = pTarg->z - zb;
        int zv = zvel[pSpr->index];
        int vAng = getVelocityAngle(pSpr);
        int dAng = (nAng - vAng) & kAngMask;
        RotatePoint(&xvel[pSpr->index], &yvel[pSpr->index], dAng, pSpr->x, pSpr->y);
        pSpr->ang = nAng;
        if ((pSpr->cstat & CSTAT_SPRITE_ALIGNMENT_MASK) == CSTAT_SPRITE_ALIGNMENT_WALL)
            pSpr->ang = (pSpr->ang + kAng90) & kAngMask;

        if (!(pSpr->flags & kPhysGravity))
        {
            if ((zv > 0 && pTarg->z < zb) || (zv < 0 && pTarg->z > pSpr->z))
                zvel[pSpr->index] = 0;
        }

        zvel[pSpr->index] += dz;
    }
}

char idListProcessFollowSprite(int32_t nSpr)
{
    spritetype* pSpr = &sprite[nSpr];
    if (!xspriRangeIsFine(pSpr->extra) || pSpr->owner < 0)
        return kListREMOVE;

    spritetype* pOwn = &sprite[pSpr->owner];
    if (!dudeIsAlive(pOwn))
        return kListREMOVE;
    
    XSPRITE* pXSpr = &xsprite[pSpr->extra];
    if (spriRangeIsFine(pXSpr->sysData1) && pXSpr->goalAng)
    {
        spritetype* pTarg = &sprite[pXSpr->sysData1];
        if (dudeIsAlive(pTarg))
        {
            if (IsPlayerSprite(pTarg))
            {
                PLAYER* pPlayer = &gPlayer[pTarg->type - kDudePlayer1];
                if (powerupCheck(pPlayer, kPwUpShadowCloak) > 0)
                    return kListSKIP;
            }

            followTarget(pSpr, pTarg, pXSpr->goalAng);
            return kListOK;
        }
    }

    return kListREMOVE;
}

// for actor.cpp
//-------------------------------------------------------------------------

spritetype* nnExtSpawnDude(XSPRITE* pXSource, spritetype* pSprite, short nType, int a3, int a4)
{

    spritetype* pDude = NULL;
    spritetype* pSource = &sprite[pXSource->reference];
    if (nType < kDudeBase || nType >= kDudeMax || (pDude = actSpawnSprite(pSprite, kStatDude)) == NULL)
        return NULL;

    XSPRITE* pXDude = &xsprite[pDude->extra];

    int angle = pSprite->ang;
    int x, y, z = a4 + pSprite->z;
    if (a3 < 0)
    {
        x = pSprite->x;
        y = pSprite->y;
    } else
    {
        x = pSprite->x + mulscale30r(Cos(angle), a3);
        y = pSprite->y + mulscale30r(Sin(angle), a3);
    }

    vec3_t pos = { x, y, z };
    setsprite(pDude->index, &pos);

    pDude->type = nType;
    pDude->ang = angle;

    pDude->cstat |= 0x1101;
    pDude->clipdist = getDudeInfo(nType)->clipdist;

    pXDude->respawn = 1;
    pXDude->health = getDudeInfo(nType)->startHealth << 4;

    if (gSysRes.Lookup(getDudeInfo(nType)->seqStartID, "SEQ"))
        seqSpawn(getDudeInfo(nType)->seqStartID, 3, pDude->extra, -1);

    // add a way to inherit some values of spawner by dude.
    if (pSource->flags & kModernTypeFlag1) {

        //inherit pal?
        if (pDude->pal <= 0)
            pDude->pal = pSource->pal;

        // inherit spawn sprite trigger settings, so designer can count monsters.
        pXDude->txID = pXSource->txID;
        pXDude->command = pXSource->command;
        pXDude->triggerOn = pXSource->triggerOn;
        pXDude->triggerOff = pXSource->triggerOff;

        // inherit drop items
        pXDude->dropMsg = pXSource->dropMsg;

        // inherit dude flags
        pXDude->dudeDeaf = pXSource->dudeDeaf;
        pXDude->dudeGuard = pXSource->dudeGuard;
        pXDude->dudeAmbush = pXSource->dudeAmbush;
        pXDude->dudeFlag4 = pXSource->dudeFlag4;
        pXDude->unused1 = pXSource->unused1;

    }

    aiInitSprite(pDude);

    gKillMgr.AddCount(pDude);

    bool burning = IsBurningDude(pDude);
    if (burning) {
        pXDude->burnTime = 10;
        pXDude->target = -1;
    }

    if ((burning || (pSource->flags & kModernTypeFlag3)) && !pXDude->dudeFlag4)
        aiActivateDude(pDude, pXDude);

    return pDude;
}

// unlike vanilla, this one won't impact missile for no reason...
spritetype* nnExtFireMissile(spritetype* pSpr, int a2, int a3, int a4, int a5, int a6, int nType)
{
    HITINFO* pHit = &gHitInfo; spritetype* pShot;
    MissileType* pInfo = &missileInfo[nType - kMissileBase];
    int nClip = pInfo->clipDist + pSpr->clipdist; int nHit;
    int x = pSpr->x + mulscale28(nClip, Cos(pSpr->ang));
    int y = pSpr->y + mulscale28(nClip, Sin(pSpr->ang));
    int z = pSpr->z;

    nnExtOffsetPos(a2, 0, a3, pSpr->ang, &x, &y, &z);
    if ((nHit = HitScan(pSpr, z, x - pSpr->x, y - pSpr->y, 0, CLIPMASK0, nClip)) >= 0)
    {
        switch (nHit)
        {
            case 0:
            case 3:
                if (pHit->hitsprite >= 0)
                {
                    switch (sprite[pHit->hitsprite].statnum)
                    {
                        case kStatProjectile:
                            nHit = -1;
                            break;
                        default:
                            break;
                    }
                }

                if (nHit >= 0)
                {
                    x = pHit->hitx - mulscale30(Cos(pSpr->ang), 16);
                    y = pHit->hity - mulscale30(Sin(pSpr->ang), 16);
                }
                break;
            default:
                x = pHit->hitx - mulscale28(pInfo->clipDist << 1, Cos(pSpr->ang));
                y = pHit->hity - mulscale28(pInfo->clipDist << 1, Sin(pSpr->ang));
                break;
        }
    }

    if ((pShot = actSpawnSprite(pSpr->sectnum, x, y, z, kStatProjectile, true)) != NULL)
    {
        XSPRITE* pXShot = &xsprite[pShot->extra];
        
        pShot->type     = nType;
        pShot->shade    = pInfo->shade;
        pShot->pal      = 0;
        pShot->clipdist = pInfo->clipDist;
        pShot->flags    = kPhysMove;
        pShot->xrepeat  = pInfo->xrepeat;
        pShot->yrepeat  = pInfo->yrepeat;
        pShot->picnum   = pInfo->picnum;
        pShot->ang      = (pSpr->ang + pInfo->angleOfs) & kAngMask;

        xvel[pShot->index] = mulscale14(pInfo->velocity, a4);
        yvel[pShot->index] = mulscale14(pInfo->velocity, a5);
        zvel[pShot->index] = mulscale14(pInfo->velocity, a6);

        SetBitString(show2dsprite, pShot->index);
        actBuildMissile(pShot, pShot->extra, pShot->index);
        actPropagateSpriteOwner(pShot, pSpr);
        
        pShot->cstat &= ~CSTAT_SPRITE_BLOCK; // projectiles don't even need this
        pXShot->target = -1;

        if (nHit >= 0)
        {
            actImpactMissile(pShot, nHit);
            return NULL;
        }

        evPost(pShot->index, OBJ_SPRITE, 600, kCallbackRemove);
    }

    return pShot;
}

spritetype* nnExtSpawnDude(spritetype* pSrc, int nType, int x, int y, int z)
{
    spritetype* pSpawn = actSpawnSprite(pSrc, kStatDude);
    if (!pSpawn)
        return NULL;

    pSpawn->type        = nType;
    pSpawn->inittype    = pSpawn->type;
    pSpawn->ang         = pSrc->ang;
    pSpawn->cstat       |= 0x1101;
    pSpawn->owner       = pSrc->index;
    pSpawn->clipdist    = getDudeInfo(nType)->clipdist;

    vec3_t pos = { x, y, z };
    setsprite(pSpawn->index, &pos);

    XSPRITE* pXSpawn    = &xsprite[pSpawn->extra];
    pXSpawn->health     = nnExtDudeStartHealth(pSpawn, pXSpawn->data4);
    pXSpawn->respawn    = 1;
    return pSpawn;
}

bool nnExtIsImmune(spritetype* pSprite, int dmgType, int minScale) {

    if (dmgType >= kDmgFall && dmgType < kDmgMax && pSprite->extra >= 0 && xsprite[pSprite->extra].locked != 1)
    {
        if (pSprite->type >= kThingBase && pSprite->type < kThingMax)
            return (thingInfo[pSprite->type - kThingBase].dmgControl[dmgType] <= minScale);
        else if (IsDudeSprite(pSprite))
        {
            if (IsPlayerSprite(pSprite)) return (gPlayer[pSprite->type - kDudePlayer1].damageControl[dmgType]);
            else if (pSprite->type == kDudeModernCustom) return (cdudeGet(pSprite->index)->GetDamage(-1, dmgType) <= minScale);
            else return (getDudeInfo(pSprite->type)->curDamage[dmgType] <= minScale);
        }
    }

    return true;
}

bool nnExtEraseModernStuff(spritetype* pSprite, XSPRITE* pXSprite) {
    
    bool erased = false;
    switch (pSprite->type) {
        // erase all modern types if the map is not extended
        case kModernCustomDudeSpawn:
        case kModernRandomTX:
        case kModernSequentialTX:
        case kModernSeqSpawner:
        case kModernObjPropertiesChanger:
        case kModernObjPicnumChanger:
        case kModernObjSizeChanger:
        case kModernDudeTargetChanger:
        case kModernSectorFXChanger:
        case kModernObjDataChanger:
        case kModernSpriteDamager:
        case kModernObjDataAccumulator:
        case kModernEffectSpawner:
        case kModernWindGenerator:
        case kModernPlayerControl:
        case kModernCondition:
        case kModernConditionFalse:
        case kModernSlopeChanger:
        case kModernPatrolRegion:
            pSprite->type = kSpriteDecoration;
            erased = true;
            break;
        case kItemModernMapLevel:
        case kDudeModernCustom:
        case kModernThingTNTProx:
        case kModernThingEnemyLifeLeech:
            pSprite->type = kSpriteDecoration;
            changespritestat(pSprite->index, kStatDecoration);
            erased = true;
            break;
        // also erase some modernized vanilla types which was not active
        case kMarkerWarpDest:
            if (pSprite->statnum == kStatMarker) break;
            pSprite->type = kSpriteDecoration;
            erased = true;
            break;
    }

    if (pXSprite->Sight) {
        pXSprite->Sight = false; // it does not work in vanilla at all
        erased = true;
    }

    if (pXSprite->Proximity) {
        // proximity works only for things and dudes in vanilla
        switch (pSprite->statnum) {
            case kStatThing:
            case kStatDude:
                break;
            default:
                pXSprite->Proximity = false;
                erased = true;
                break;
        }
    }

    return erased;
}

void nnExtTriggerObject(int objType, int objIndex, int command, int causerID) {
    switch (objType) {
        case OBJ_SECTOR:
            if (!xsectRangeIsFine(sector[objIndex].extra)) break;
            trTriggerSector(objIndex, &xsector[sector[objIndex].extra], command, causerID);
            break;
        case OBJ_WALL:
            if (!xwallRangeIsFine(wall[objIndex].extra)) break;
            trTriggerWall(objIndex, &xwall[wall[objIndex].extra], command, causerID);
            break;
        case OBJ_SPRITE:
            if (!xspriRangeIsFine(sprite[objIndex].extra)) break;
            trTriggerSprite(objIndex, &xsprite[sprite[objIndex].extra], command, causerID);
            break;
    }

    return;
}

void nnExtResetGlobals()
{
    gAllowTrueRandom = gEventRedirectsUsed = false;

    // clear lists
    gProxySpritesList.Free();       gSightSpritesList.Free();
    gImpactSpritesList.Free();      gPhysSpritesList.Free();
    gFlwSpritesList.Free();

    // free all custom dudes
    cdudeFree();

    // free all condition trackers
    conditionsTrackingClear();

    // clear sprite mass cache
    memset(gSpriteMass, 0, sizeof(gSpriteMass));

    lasersClear();

    if (gViewCam.active) // reset view camera to the player
        useViewCamera(NULL, NULL, NULL, &gPlayer[myconnectindex]);

    gCustomMusicStatus = -1; // reset music to the INI definition
}

void nnExtPlayerReset(PLAYER* pPlayer)
{
    // reset QAV scene
    QAVSCENE* pQavScene = &gPlayerCtrl[pPlayer->nPlayer].qavScene;
    pQavScene->index = pQavScene->dummy = pPlayer->sceneQav = -1;
    pQavScene->qavResrc = NULL;

    if (pPlayer->pSprite && pPlayer->pXSprite)
    {
        pPlayer->pXSprite->sysData1 = 0;
        pPlayer->pXSprite->aiState = NULL;
        pPlayer->pXSprite->dudeFlag4 = 0;
    }

    if (gViewCam.active) // reset view camera to the player
        useViewCamera(NULL, NULL, NULL, &gPlayer[myconnectindex]);
}

void getSectorWalls(int nSect, int* swal, int* ewal)
{
    *swal = sector[nSect].wallptr;
    *ewal = *swal + sector[nSect].wallnum - 1;
}

bool isMultiTx(short nSpr)
{
    int i, j = 0;
    if (sprite[nSpr].statnum < kStatFree && (sprite[nSpr].type == 25 || sprite[nSpr].type == 26))
    {
        for (i = 0; i < 4; i++)
            if (rngok(getDataFieldOfObject(OBJ_SPRITE, nSpr, i + 1), 1, kChannelUserMax)) j++;
    }

    return (j > 0);
}

bool multiTxGetRange(int nSpr, int out[4])
{
    int j;
    XSPRITE* pXSpr = &xsprite[sprite[nSpr].extra];
    for (j = 0; j < 4; j++) out[j] = 0;

    if (rngok(pXSpr->data1, 1, kChannelUserMax) && pXSpr->data2 == 0 && pXSpr->data3 == 0 && rngok(pXSpr->data4, 1, kChannelUserMax))
    {
        if (pXSpr->data1 > pXSpr->data4)
        {
            j = pXSpr->data4;
            pXSpr->data4 = pXSpr->data1;
            pXSpr->data1 = j;
        }

        out[0] = pXSpr->data1;
        out[1] = pXSpr->data4;
        return true; // ranged
    }

    for (j = 0; j < 4; j++) out[j] = getDataFieldOfObject(OBJ_SPRITE, nSpr, j + 1);
    return false; // normal
}

bool multiTxPointsRx(int rx, short nSpr)
{
    int j; int txrng[4];

    // ranged
    if (multiTxGetRange(nSpr, txrng))
        return (rngok(rx, txrng[0], txrng[1] + 1));


    // normal
    for (j = 0; j < 4; j++)
    {
        if (rx == txrng[j])
            return true;
    }

    return false;

}

int collectObjectsByChannel(int nChannel, bool rx, OBJECT_LIST* pOut, char flags)
{
    bool ok, link = false, unlink = false;
    int i = numsectors;
    int c = 0, j, s, e, f;

    //return 0;

    switch (flags & 0x30)
    {
    case 0x10:	link = true;    break;
    case 0x20:	unlink = true;    break;
    }

    while (--i >= 0)
    {
        getSectorWalls(i, &s, &e);
        for (j = s; j <= e; j++)
        {
            if (wall[j].extra > 0)
            {
                XWALL* pXObj = &xwall[wall[j].extra];
                if ((rx && pXObj->rxID == nChannel) || (!rx && pXObj->txID == nChannel))
                {
                    if (link || unlink)
                    {
                        ok = ((f = collectObjectsByChannel(nChannel, !rx, NULL, 0)) > 0 && (f != 1 || pXObj->rxID != pXObj->txID));
                        if ((!ok && link) || (ok && unlink))
                            continue;
                    }

                    if (pOut)
                        pOut->Add(OBJ_WALL, j);

                    c++;
                }
            }
        }

        for (j = headspritesect[i]; j >= 0; j = nextspritesect[j])
        {
            if (sprite[j].extra > 0)
            {
                XSPRITE* pXObj = &xsprite[sprite[j].extra];
                if ((rx && pXObj->rxID == nChannel) || (!rx && ((isMultiTx(j) && multiTxPointsRx(nChannel, j)) || pXObj->txID == nChannel)))
                {
                    if (link || unlink)
                    {
                        ok = ((f = collectObjectsByChannel(nChannel, !rx, NULL, 0)) > 0 && (f != 1 || pXObj->rxID != pXObj->txID));
                        if ((!ok && link) || (ok && unlink))
                            continue;
                    }

                    if (pOut)
                        pOut->Add(OBJ_SPRITE, j);

                    c++;
                }
            }
        }

        if (sector[i].extra > 0)
        {
            XSECTOR* pXObj = &xsector[sector[i].extra];
            if ((rx && pXObj->rxID == nChannel) || (!rx && pXObj->txID == nChannel))
            {
                if (link || unlink)
                {
                    ok = ((f = collectObjectsByChannel(nChannel, !rx, NULL, 0)) > 0 && (f != 1 || pXObj->rxID != pXObj->txID));
                    if ((!ok && link) || (ok && unlink))
                        continue;
                }

                if (pOut)
                    pOut->Add(OBJ_SECTOR, i);

                c++;
            }
        }
    }

    return c;
}

int getChannelOf(int objType, int objIdx, bool rx)
{
    switch (objType)
    {
        case OBJ_WALL:      return (rx) ? xwall[wall[objIdx].extra].rxID     : xwall[wall[objIdx].extra].txID;
        case OBJ_SPRITE:    return (rx) ? xsprite[sprite[objIdx].extra].rxID : xsprite[sprite[objIdx].extra].txID;
        case OBJ_SECTOR:    return (rx) ? xsector[sector[objIdx].extra].rxID : xsector[sector[objIdx].extra].txID;
    }

    return -1;
}

int collectBranchByChannel(int nChannelA, bool rx, OBJECT_LIST* pOut)
{
    int i = 0, l, nChannelB;
    OBJECT_LIST objects; OBJECT* pObj;

    collectObjectsByChannel(nChannelA, rx, &objects, 0);
    pObj = objects.Ptr(); l = objects.Length();

    while (i < l)
    {
        if (pOut->Find(pObj->type, pObj->index) < 0)
        {
            pOut->Add(pObj->type, pObj->index);
            if ((nChannelB = getChannelOf(pObj->type, pObj->index, true)) > 0)
                collectBranchByChannel(nChannelB, false, pOut);
        }

        pObj++;
        i++;
    }

    return l;
}

void nnExtInitCauserTable()
{
    OBJECT_LIST objects; OBJECT* pObj;
    int t = sizeof(OBJECT_STATUS1) * (OBJ_SECTOR + 1);
    collectBranchByChannel(kChannelEventCauser, false, &objects);

    if (!gEvCauser)
        gEvCauser = (OBJECT_STATUS1*)Bmalloc(t);

    memset(gEvCauser, 0, t);

    pObj = objects.Ptr(); t = objects.Length();
    while (--t >= 0)
    {
        if (getChannelOf(pObj->type, pObj->index, false) > 0)
            gEvCauser[pObj->type].id[pObj->index].ok = 1;

        pObj++;
    }
}


void nnExtInitSprite(int nSpr, bool bSaveLoad)
{
    int i;
    spritetype* pSpr = &sprite[nSpr];
    // FIXME: the extra < 0 check prevents crashes when loading some savegames, for example
    //        due to `XSPRITE* pXSpr = &xsprite[pSpr->extra]` accessing invalid memory,
    //        or due to that memory containing a value != 0 at pXspr->physAddr and then
    //        getSpriteMassBySize() throwing an error because pSpr->extra < 0..
    //        However, it maybe hides real issues that should be investigated, like
    //        when/why is extra -1 here?!
    if ((pSpr->flags & kHitagFree) || pSpr->extra < 0)
        return;

    XSPRITE* pXSpr = &xsprite[pSpr->extra];

    switch (pSpr->type)
    {
        case kModernRandomTX:
        case kModernSequentialTX:
            if (pXSpr->command == kCmdLink) gEventRedirectsUsed = true;
            break;
        case kDudeModernCustom:
            if (bSaveLoad && pSpr->statnum != kStatInactive)
            {
                pXSpr->data3 = pXSpr->sysData1; // move sndStartId back from sysData1 to data3
                if (pXSpr->health <= 0)         // it's a dying dude
                {
                    ChangeSpriteStat(pSpr->index, kStatThing);
                    pSpr->type = kThingBloodChunks;
                    
                    // don't let pending dude to be destroyed
                    if (pSpr->flags & kHitagRespawn)
                        pXSpr->isTriggered = 1;
                    else
                        DudeToGibCallback1(pSpr->index, pSpr->extra);
                }
            }
            break;
    }

    // init after loading save file
    if (bSaveLoad)
    {
        // add in list of physics affected sprites
        if (pXSpr->physAttr != 0)
        {
            gPhysSpritesList.Add(pSpr->index);
            getSpriteMassBySize(pSpr); // create mass cache
        }
    }
    else
    {
        // auto set going On and going Off if both are empty
        if (pXSpr->txID && !pXSpr->triggerOn && !pXSpr->triggerOff)
            pXSpr->triggerOn = pXSpr->triggerOff = true;

        // copy custom start health to avoid overwrite by kThingBloodChunks
        if (IsDudeSprite(pSpr))
            pXSpr->sysData2 = pXSpr->data4;

        switch (pSpr->type)
        {
            case kGenBubble:
            case kGenBubbleMulti:
                // convert in effect gen
                pSpr->type = kModernEffectSpawner;
                pSpr->flags = 0;
                pXSpr->data2 = (pSpr->type == kGenBubble) ? FX_23 : FX_26;
                pXSpr->data3 = 0;
                pXSpr->data4 = 0;
                break;
            case kThingObjectExplode:
            case kThingObjectGib:
                // copy flags so 32 can be used
                pXSpr->sysData1 = pSpr->flags;
                pSpr->flags = 0;
                break;
            case kModernRandomTX:
            case kModernSequentialTX:
                if (pXSpr->command != kCmdLink) break;
                // add statnum for faster redirects search
                ChangeSpriteStat(pSpr->index, kStatModernEventRedirector);
                break;
            case kModernWindGenerator:
                pSpr->cstat &= ~CSTAT_SPRITE_BLOCK;
                break;
            case kModernDudeTargetChanger:
            case kModernObjDataAccumulator:
            case kModernRandom:
            case kModernRandom2:
            case kModernPatrolRegion:
                pSpr->cstat &= ~CSTAT_SPRITE_BLOCK;
                pSpr->cstat |= CSTAT_SPRITE_INVISIBLE;
                switch (pSpr->type) {
                        // regions for patrolling enemies
                    case kModernPatrolRegion:
                        ChangeSpriteStat(pSpr->index, kStatModernPatrolRegion);
                        break;
                        // add statnum for faster dude searching
                    case kModernDudeTargetChanger:
                        ChangeSpriteStat(pSpr->index, kStatModernDudeTargetChanger);
                        if (pXSpr->busyTime <= 0) pXSpr->busyTime = 5;
                        pXSpr->command = kCmdLink;
                        break;
                        // remove kStatItem status from random item generators
                    case kModernRandom:
                    case kModernRandom2:
                        ChangeSpriteStat(pSpr->index, kStatDecoration);
                        pXSpr->sysData1 = pXSpr->command; // save the command so spawned item can inherit it
                        pXSpr->command = kCmdLink;  // generator itself can't send commands
                        break;
                }
                break;
            case kModernThingTNTProx:
                pXSpr->Proximity = true;
                break;
            case kDudePodMother:
            case kDudeTentacleMother:
                pXSpr->state = 1;
                break;
            case kModernPlayerControl:
                switch (pXSpr->command)
                {
                    case kCmdLink:
                        if (pXSpr->data1 && !rngok(pXSpr->data1, 1, kMaxPlayers + 1))
                            ThrowError("\nPlayer Control (SPRITE #%d):\nPlayer out of a range (data1 = %d)", pSpr->index, pXSpr->data1);

                        if (pXSpr->rxID && pXSpr->rxID != kChannelLevelStart)
                            ThrowError("\nPlayer Control (SPRITE #%d) with Link command should have no RX ID!", pSpr->index, pXSpr->data1);

                        if (pXSpr->txID && pXSpr->txID < kChannelUser)
                            ThrowError("\nPlayer Control (SPRITE #%d):\nTX ID should be in range of %d and %d!", pSpr->index, kChannelUser, kChannelMax);

                        // only one linker per player allowed
                        for (i = headspritestat[kStatModernPlayerLinker]; i >= 0; i = nextspritestat[i])
                        {
                            XSPRITE* pXCtrl = &xsprite[sprite[i].extra];
                            if (pXSpr->data1 == pXCtrl->data1)
                                ThrowError("\nPlayer Control (SPRITE #%d):\nPlayer %d already linked with different player control sprite #%d!", pSpr->index, pXSpr->data1, i);
                        }
                        pXSpr->sysData1 = -1;
                        pSpr->cstat &= ~CSTAT_SPRITE_BLOCK;
                        ChangeSpriteStat(pSpr->index, kStatModernPlayerLinker);
                        break;
                    case 67: // play qav animation
                        if (pXSpr->txID >= kChannelUser && !pXSpr->waitTime) pXSpr->waitTime = 1;
                        ChangeSpriteStat(pSpr->index, kStatModernQavScene);
                        break;
                }
                break;
            case kModernCondition:
            case kModernConditionFalse:
                if (pXSpr->busyTime > 0)
                {
                    pXSpr->busy = pXSpr->busyTime;
                    if (pXSpr->waitTime > 0)
                    {
                        pXSpr->busy += EVTIME2TICKS(pXSpr->waitTime); pXSpr->waitTime = 0;
                        consoleSysMsg("Summing busyTime and waitTime for tracking condition #%d, RX ID %d. Result = %d ticks", pSpr->index, pXSpr->rxID, pXSpr->busyTime);
                    }
                }

                pXSpr->Decoupled    = false; // must go through operateSprite always
                pXSpr->Sight        = pXSpr->Impact    = pXSpr->Touch  = false;
                pXSpr->Proximity    = pXSpr->Push      = pXSpr->Vector = false;
                pXSpr->state        = pXSpr->restState = 0;

                if (gModernMap == 2 && pXSpr->triggerOn && !pXSpr->triggerOff)
                    pSpr->flags |= kModernTypeFlag64;
                else
                    pSpr->flags &= ~kModernTypeFlag64;

                pXSpr->triggerOn    = pXSpr->triggerOff = false;
                pXSpr->targetX      = pXSpr->targetY    = pXSpr->targetZ = pXSpr->target = pXSpr->sysData2 = -1;
                ChangeSpriteStat(pSpr->index, kStatModernCondition);
                pSpr->cstat |= CSTAT_SPRITE_INVISIBLE;
                break;
        }

        // the following trigger flags are senseless to have together
        if ((pXSpr->Touch && (pXSpr->Proximity || pXSpr->Sight) && pXSpr->DudeLockout)
            || (pXSpr->Touch && pXSpr->Proximity && !pXSpr->Sight)) pXSpr->Touch = false;

        if (pXSpr->Proximity && pXSpr->Sight && pXSpr->DudeLockout)
            pXSpr->Proximity = false;

        // very quick fix for floor sprites with Touch trigger flag if their Z is equals sector floorz / ceilgz
        if (pSpr->sectnum >= 0 && pXSpr->Touch && (pSpr->cstat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_FLOOR)
        {
            if (pSpr->z == sector[pSpr->sectnum].floorz) pSpr->z--;
            else if (pSpr->z == sector[pSpr->sectnum].ceilingz)
                pSpr->z++;
        }
    }

    // make Proximity flag work not just for dudes and things...
    if (pXSpr->Proximity)
    {
        switch (pSpr->statnum)
        {
            case kStatFX:           case kStatExplosion:            case kStatItem:
            case kStatPurge:        case kStatSpares:               case kStatFlare:
            case kStatInactive:     case kStatFree:                 case kStatMarker:
            case kStatThing:        case kStatDude:                 case kStatModernPlayerLinker:
                break;
            default:
                gProxySpritesList.Add(pSpr->index);
                break;
        }
    }

    // make Sight, Screen, Aim flags work not just for dudes and things...
    if (pXSpr->Sight || pXSpr->unused3)
    {
        switch (pSpr->statnum)
        {
            case kStatFX:           case kStatExplosion:            case kStatItem:
            case kStatPurge:        case kStatSpares:               case kStatFlare:
            case kStatInactive:     case kStatFree:                 case kStatMarker:
            case kStatModernPlayerLinker:
                break;
            default:
                gSightSpritesList.Add(pSpr->index);
                break;
        }
    }

    // make Impact flag work for sprites that affected by explosions...
    if (pXSpr->Impact)
    {
        switch (pSpr->statnum)
        {
            case kStatFX:           case kStatExplosion:            case kStatItem:
            case kStatPurge:        case kStatSpares:               case kStatFlare:
            case kStatInactive:     case kStatFree:                 case kStatMarker:
            case kStatModernPlayerLinker:
                break;
            default:
                gImpactSpritesList.Add(pSpr->index);
                break;
        }
    }
}

void nnExtInitModernStuff(bool bSaveLoad) {
    
    int i, j;
    nnExtResetGlobals();

    // initialize super xsprites lists
    gProxySpritesList.Init(kListEndDefault,  kMaxSuperXSprites);     gSightSpritesList.Init(kListEndDefault, kMaxSuperXSprites);
    gImpactSpritesList.Init(kListEndDefault, kMaxSuperXSprites);     gPhysSpritesList.Init(kListEndDefault,  kMaxSuperXSprites);
    gFlwSpritesList.Init(kListEndDefault, kMaxSuperXSprites);

    // use true random only for single player mode, otherwise use Blood's default one.
    if (gGameOptions.nGameType == kGameTypeSinglePlayer)
    {
        i = kMaxRandomizeRetries;
        gStdRandom.seed(std::random_device()());

        // since true random is not working if compiled with old mingw versions, we should
        // check if it works in game and if not - switch to using in-game random function.
        while (--i >= 0)
        {
            std::uniform_int_distribution<int> dist_a_b(0, 100);
            if (dist_a_b(gStdRandom) == 0) continue;
            gAllowTrueRandom = true;
            break;
        }
    }

    consoleSysMsg("STD randomness %s available!", (gAllowTrueRandom) ? "is" : "is not");
    
    i = numsectors;
    while (--i >= 0)
    {
        // initialize sprites
        for (j = headspritesect[i]; j >= 0; j = nextspritesect[j])
            nnExtInitSprite(j, bSaveLoad);
    }

    // prepare event causer sequence table
    nnExtInitCauserTable();

    // prepare conditions for use
    if (gStatCount[kStatModernCondition])
        conditionsInit(bSaveLoad);

    // prepare custom dudes array
    cdudeAlloc();

#ifdef NNEXTS_USE_RES_SYS
    if (!gExternalFilesAdded)
    {
        // add external files from the mod directory
        // to the resource system (cdudes scripts etc)

        char modDir[BMAX_PATH] = "./";
        if (g_modDir[0] != '/' || g_modDir[1] != 0)
            strcpy(modDir, g_modDir);

        i = nnExtResAddExternalFiles(&gSysRes, modDir, gExternFiles, LENGTH(gExternFiles));
        consoleSysMsg("There is %d extra external files added in total.", i);
        gExternalFilesAdded = true;
    }
#endif

    // init lasers
    lasersInit();
}


// The following functions required for random event features
//-------------------------
int nnExtRandom(int a, int b) {
    if (!gAllowTrueRandom) return Random(((b + 1) - a)) + a;
    // used for better randomness in single player
    std::uniform_int_distribution<int> dist_a_b(a, b);
    return dist_a_b(gStdRandom);
}

int GetDataVal(spritetype* pSprite, int data) {
    dassert(xspriRangeIsFine(pSprite->extra));
    
    switch (data) {
        case 0: return xsprite[pSprite->extra].data1;
        case 1: return xsprite[pSprite->extra].data2;
        case 2: return xsprite[pSprite->extra].data3;
        case 3: return xsprite[pSprite->extra].data4;
    }

    return -1;
}

// tries to get random data field of sprite
int randomGetDataValue(XSPRITE* pXSprite, int randType) {
    if (pXSprite == NULL) return -1;
    int random = 0; int bad = 0; int maxRetries = kMaxRandomizeRetries;

    int rData[4];
    rData[0] = pXSprite->data1; rData[2] = pXSprite->data3;
    rData[1] = pXSprite->data2; rData[3] = pXSprite->data4;
    // randomize only in case if at least 2 data fields fits.
    for (int i = 0; i < 4; i++) {
        switch (randType) {
        case kRandomizeItem:
            if (rData[i] >= kItemWeaponBase && rData[i] < kItemMax) break;
            else bad++;
            break;
        case kRandomizeDude:
            if (rData[i] >= kDudeBase && rData[i] < kDudeMax) break;
            else bad++;
            break;
        case kRandomizeTX:
            if (rData[i] > kChannelZero && rData[i] < kChannelUserMax) break;
            else bad++;
            break;
        default:
            bad++;
            break;
        }
    }

    if (bad < 3) {
        // try randomize few times
        while (maxRetries > 0) {
            random = nnExtRandom(0, 3);
            if (rData[random] > 0) return rData[random];
            else maxRetries--;
        }
    }

    return -1;
}

// this function drops random item using random pickup generator(s)
spritetype* randomDropPickupObject(spritetype* pSource, short prevItem) {
    spritetype* pSprite2 = NULL; int selected = -1; int maxRetries = 9;
    if (xspriRangeIsFine(pSource->extra)) {
        XSPRITE* pXSource = &xsprite[pSource->extra];
        while ((selected = randomGetDataValue(pXSource, kRandomizeItem)) == prevItem) if (maxRetries-- <= 0) break;
        if (selected > 0) {
            pSprite2 = actDropObject(pSource, selected);
            if (pSprite2 != NULL) {

                pXSource->dropMsg = pSprite2->type; // store dropped item type in dropMsg
                pSprite2->x = pSource->x;
                pSprite2->y = pSource->y;
                pSprite2->z = pSource->z;

                if ((pSource->flags & kModernTypeFlag1) && (pXSource->txID > 0 || (pXSource->txID != 3 && pXSource->lockMsg > 0)) &&
                    dbInsertXSprite(pSprite2->index) > 0) {

                    XSPRITE* pXSprite2 = &xsprite[pSprite2->extra];

                    // inherit spawn sprite trigger settings, so designer can send command when item picked up.
                    pXSprite2->txID = pXSource->txID;
                    pXSprite2->command = pXSource->sysData1;
                    pXSprite2->triggerOn = pXSource->triggerOn;
                    pXSprite2->triggerOff = pXSource->triggerOff;

                    pXSprite2->Pickup = true;

                }
            }
        }
    }
    return pSprite2;
}

// this function spawns random dude using dudeSpawn
spritetype* randomSpawnDude(XSPRITE* pXSource, spritetype* pSprite, int a3, int a4) {
    
    UNREFERENCED_PARAMETER(a4);

    spritetype* pSprite2 = NULL; int selected = -1;
    spritetype* pSource = &sprite[pXSource->reference];
    
    if (xspriRangeIsFine(pSource->extra)) {
        XSPRITE* pXSource = &xsprite[pSource->extra];
        if ((selected = randomGetDataValue(pXSource, kRandomizeDude)) > 0)
            pSprite2 = nnExtSpawnDude(pXSource, pSprite, selected, a3, 0);
    }

    return pSprite2;
}

//-------------------------
void nnExtProcessSuperSprites()
{
    conditionsTrackingProcess();							            // process tracking conditions
    playerProcessAiState();                                             // process players controlled by AI
    lasersProcess();                                                    // process lasers

    gProxySpritesList.Process(idListProcessProxySprite, true);          // process additional proximity sprites
    gSightSpritesList.Process(idListProcessSightSprite, true);          // process sight sprites (for players only)
    gPhysSpritesList.Process(idListProcessPhysSprite,   true);          // process Debris sprites for movement
    gFlwSpritesList.Process(idListProcessFollowSprite,  true);          // process projectiles and things for following target

    if (gViewCam.active)
    {
        if (!gViewCam.drawToTile)
        {
            if (gViewMode == 4)
            {
                // show 2d map relative to player
                gView = gMe = &gPlayer[gViewIndex];
            }
            else if (gView >= &gPlayer[0] && gView < &gPlayer[kMaxPlayers])
            {
                if (gView->nPlayer == gViewCam.nPlayer) // switch back to camera view
                    gView = &gViewCam, gMe = &gMeCam;
            }
        }

        if (!xsprIsFine(gViewCam.pSprite))
        {
            spritetype* pCSrc = &sprite[gViewCam.nCamSrc];
            XSPRITE* pXCSrc = &xsprite[pCSrc->extra];
            
            gViewCam.nSprite = gMeCam.nSprite = pCSrc->index;
            gViewCam.pSprite = gMeCam.pSprite = pCSrc;
            gViewCam.pXSprite = gMeCam.pXSprite = pXCSrc;
        }

        processViewCamera(&gViewCam);
    }
}

// this function plays sound predefined in missile info
void sfxPlayMissileSound(spritetype* pSprite, int missileId) {
    MISSILEINFO_EXTRA* pMissType = &gMissileInfoExtra[missileId - kMissileBase];
    sfxPlay3DSound(pSprite, Chance(0x5000) ? pMissType->fireSound[0] : pMissType->fireSound[1], -1, 0);
}

// this function plays sound predefined in vector info
void sfxPlayVectorSound(spritetype* pSprite, int vectorId) {
    VECTORINFO_EXTRA* pVectorData = &gVectorInfoExtra[vectorId];
    sfxPlay3DSound(pSprite, Chance(0x5000) ? pVectorData->fireSound[0] : pVectorData->fireSound[1], -1, 0);
}

int getSpriteMassBySize(spritetype* pSprite) {
    int mass = 0; int seqId = -1; int clipDist = pSprite->clipdist; Seq* pSeq = NULL;
    if (pSprite->extra < 0) {
        ThrowError("getSpriteMassBySize: pSprite->extra < 0");

    }

    SPRITEMASS* cached = &gSpriteMass[pSprite->extra];
    if (IsDudeSprite(pSprite))
    {
        switch (pSprite->type)
        {
            case kDudeModernCustom:
            case kDudePodMother: //no seq
                break;
            default:
                seqId = getDudeInfo(pSprite->type)->seqStartID;
                break;
        }
    }
    else
    {
        seqId = seqGetID(3, pSprite->extra);
    }

    if (((seqId >= 0 && seqId == cached->seqId) || pSprite->picnum == cached->picnum) && pSprite->xrepeat == cached->xrepeat &&
        pSprite->yrepeat == cached->yrepeat && clipDist == cached->clipdist) {
        return cached->mass;
    }

    short picnum = pSprite->picnum;
    short massDiv = 30;  short addMul = 2; short subMul = 2;

    if (seqId >= 0) {
        DICTNODE* hSeq = gSysRes.Lookup(seqId, "SEQ");
        if (hSeq)
        {
            pSeq = (Seq*)gSysRes.Load(hSeq);
            picnum = seqGetTile(&pSeq->frames[0]);
        } else
            picnum = pSprite->picnum;
    }

    clipDist = ClipLow(pSprite->clipdist, 1);
    short x = tilesiz[picnum].x;        short y = tilesiz[picnum].y;
    short xrepeat = pSprite->xrepeat; 	short yrepeat = pSprite->yrepeat;

    // take surface type into account
    switch (tileGetSurfType(pSprite->index, 0xc000)) {
        case 1:  massDiv = 16; break; // stone
        case 2:  massDiv = 18; break; // metal
        case 3:  massDiv = 21; break; // wood
        case 4:  massDiv = 25; break; // flesh
        case 5:  massDiv = 28; break; // water
        case 6:  massDiv = 26; break; // dirt
        case 7:  massDiv = 27; break; // clay
        case 8:  massDiv = 35; break; // snow
        case 9:  massDiv = 22; break; // ice
        case 10: massDiv = 37; break; // leaves
        case 11: massDiv = 33; break; // cloth
        case 12: massDiv = 36; break; // plant
        case 13: massDiv = 24; break; // goo
        case 14: massDiv = 23; break; // lava
    }

    mass = ((x + y) * (clipDist / 2)) / massDiv;

    if (xrepeat > 64) mass += ((xrepeat - 64) * addMul);
    else if (xrepeat < 64 && mass > 0) {
        for (int i = 64 - xrepeat; i > 0; i--) {
            if ((mass -= subMul) <= 100 && subMul-- <= 1) {
                mass -= i;
                break;
            }
        }
    }

    if (yrepeat > 64) mass += ((yrepeat - 64) * addMul);
    else if (yrepeat < 64 && mass > 0) {
        for (int i = 64 - yrepeat; i > 0; i--) {
            if ((mass -= subMul) <= 100 && subMul-- <= 1) {
                mass -= i;
                break;
            }
        }
    }

    if (mass <= 0) cached->mass = 1 + Random(10);
    else cached->mass = ClipRange(mass, 1, 65535);

    cached->airVel = ClipRange(400 - cached->mass, 32, 400);
    cached->fraction = ClipRange(60000 - (cached->mass << 7), 8192, 60000);

    cached->xrepeat = pSprite->xrepeat;             cached->yrepeat = pSprite->yrepeat;
    cached->picnum = pSprite->picnum;               cached->seqId = seqId;
    cached->clipdist = pSprite->clipdist;

    return cached->mass;
}

void debrisConcuss(int nOwner, int nDebris, int x, int y, int z, int dmg)
{
    bool thing;
    int dx, dy, dz, size, t;
    spritetype* pSpr = &sprite[nDebris];
    if (pSpr->extra <= 0)
        return;

    XSPRITE* pXSpr = &xsprite[pSpr->extra];

    dx = pSpr->x - x; dy = pSpr->y - y; dz = (pSpr->z - z) >> 4;
    dmg = scale(0x40000, dmg, 0x40000 + dx * dx + dy * dy + dz * dz);
    size = (tilesiz[pSpr->picnum].x * pSpr->xrepeat * tilesiz[pSpr->picnum].y * pSpr->yrepeat) >> 1;
    thing = (pSpr->type >= kThingBase && pSpr->type < kThingMax);

    if (pXSpr->physAttr & kPhysDebrisExplode)
    {
        if (gSpriteMass[pSpr->extra].mass > 0)
        {
            t = scale(dmg, size, gSpriteMass[pSpr->extra].mass);
            xvel[pSpr->index] += mulscale16(t, dx);
            yvel[pSpr->index] += mulscale16(t, dy);
            zvel[pSpr->index] += mulscale16(t, dz);
        }

        if (thing)
            pSpr->statnum = kStatThing; // temporary change statnum property
    }

    actDamageSprite(nOwner, pSpr, kDamageExplode, dmg);

    if (thing)
        pSpr->statnum = kStatDecoration; // return statnum property back
    
}

void debrisBubble(int nSprite) {
    
    spritetype* pSprite = &sprite[nSprite];
    
    int top, bottom;
    GetSpriteExtents(pSprite, &top, &bottom);
    for (unsigned int i = 0; i < 1 + Random(5); i++) {
        
        int nDist = (pSprite->xrepeat * (tilesiz[pSprite->picnum].x >> 1)) >> 2;
        int nAngle = Random(2048);
        int x = pSprite->x + mulscale30(nDist, Cos(nAngle));
        int y = pSprite->y + mulscale30(nDist, Sin(nAngle));
        int z = bottom - Random(bottom - top);
        spritetype* pFX = gFX.fxSpawn((FX_ID)(FX_23 + Random(3)), pSprite->sectnum, x, y, z);
        if (pFX) {
            xvel[pFX->index] = xvel[nSprite] + Random2(0x1aaaa);
            yvel[pFX->index] = yvel[nSprite] + Random2(0x1aaaa);
            zvel[pFX->index] = zvel[nSprite] + Random2(0x1aaaa);
        }

    }
    
    if (Chance(0x2000))
        evPost(nSprite, 3, 0, kCallbackEnemeyBubble);
}

void debrisMove(int nSpr)
{
    spritetype* pSpr = &sprite[nSpr]; XSPRITE* pXSpr = &xsprite[pSpr->extra];
    int nSect = pSpr->sectnum; int nXSpr = pSpr->extra;

    int top, bottom, i;
    GetSpriteExtents(pSpr, &top, &bottom);

    int moveHit = 0;
    int floorDist = (bottom - pSpr->z) >> 2;
    int ceilDist = (pSpr->z - top) >> 2;
    int clipDist = pSpr->clipdist << 2;
    int mass = gSpriteMass[nXSpr].mass;

    bool uwater = false;
    int tmpFraction = gSpriteMass[pSpr->extra].fraction;
    if (sector[nSect].extra >= 0 && xsector[sector[nSect].extra].Underwater)
    {
        tmpFraction >>= 1;
        uwater = true;
    }

    if (xvel[nSpr] || yvel[nSpr])
    {
        short oldcstat = pSpr->cstat;
        pSpr->cstat &= ~(CSTAT_SPRITE_BLOCK | CSTAT_SPRITE_BLOCK_HITSCAN);
        moveHit = gSpriteHit[nXSpr].hit = ClipMove((int*)&pSpr->x, (int*)&pSpr->y, (int*)&pSpr->z, &nSect, xvel[nSpr] >> 12,
            yvel[nSpr] >> 12, clipDist, ceilDist, floorDist, CLIPMASK0);

        pSpr->cstat = oldcstat;
        if (pSpr->sectnum != nSect)
        {
            if (!sectRangeIsFine(nSect))
                return;
            
            ChangeSpriteSect(nSpr, nSect);
        }

        if (sector[nSect].type >= kSectorPath && sector[nSect].type <= kSectorRotate)
        {
            short nSect2 = nSect;
            if (pushmove_old(&pSpr->x, &pSpr->y, &pSpr->z, &nSect2, clipDist, ceilDist, floorDist, CLIPMASK0) != -1)
                nSect = nSect2;
        }

        if ((gSpriteHit[nXSpr].hit & 0xc000) == 0x8000)
        {
            i = moveHit = gSpriteHit[nXSpr].hit & 0x3fff;
            actWallBounceVector((int*)&xvel[nSect], (int*)&yvel[nSpr], i, tmpFraction);
        }

    }
    
    if (nSect < 0 && !FindSector(pSpr->x, pSpr->y, pSpr->z, &nSect)) 
        return;
    else if (pSpr->sectnum != nSect)
    {
        ChangeSpriteSect(nSpr, nSect);
        nSect = pSpr->sectnum;
    }

    if (sector[nSect].extra > 0)
        uwater = xsector[sector[nSect].extra].Underwater;

    if (zvel[nSpr])
        pSpr->z += zvel[nSpr] >> 8;

    int ceilZ, ceilHit, floorZ, floorHit;
    GetZRange(pSpr, &ceilZ, &ceilHit, &floorZ, &floorHit, clipDist, CLIPMASK0, PARALLAXCLIP_CEILING | PARALLAXCLIP_FLOOR);
    GetSpriteExtents(pSpr, &top, &bottom);

    if ((pXSpr->physAttr & kPhysDebrisSwim) && uwater)
    {
        int vc = 0;
        int cz = getceilzofslope(nSect, pSpr->x, pSpr->y);
        int fz = getflorzofslope(nSect, pSpr->x, pSpr->y);
        int div = ClipLow(bottom - top, 1);

        if (gLowerLink[nSect] >= 0)
            cz += (cz < 0) ? 0x500 : -0x500;
        
        if (top > cz && (!(pXSpr->physAttr & kPhysDebrisFloat) || fz <= bottom << 2))
            zvel[nSpr] -= divscale8((bottom - ceilZ) >> 6, mass);

        if (fz < bottom)
            vc = 58254 + ((bottom - fz) * -80099) / div;

        if (vc)
        {
            pSpr->z += ((vc << 2) >> 1) >> 8;
            zvel[nSpr] += vc;
        }

    }
    else if ((pXSpr->physAttr & kPhysGravity) && bottom < floorZ)
    {
        pSpr->z += 455;
        zvel[nSpr] += 58254;
    }

    if ((i = CheckLink(pSpr)) != 0)
    {
        GetZRange(pSpr, &ceilZ, &ceilHit, &floorZ, &floorHit, clipDist, CLIPMASK0, PARALLAXCLIP_CEILING | PARALLAXCLIP_FLOOR);
        if (!(pSpr->cstat & CSTAT_SPRITE_INVISIBLE))
        {
            switch (i)
            {
                case kMarkerUpWater:
                case kMarkerUpGoo:
                    int pitch = (150000 - (gSpriteMass[pSpr->extra].mass << 9)) + Random3(8192);
                    sfxPlay3DSoundCP(pSpr, 720, -1, 0, pitch, 75 - Random(40));
                    if (!spriteIsUnderwater(pSpr))
                    {
                        evKill(pSpr->index, 3, kCallbackEnemeyBubble);
                    }
                    else
                    {
                        evPost(pSpr->index, 3, 0, kCallbackEnemeyBubble);
                        for (int i = 2; i <= 5; i++)
                        {
                            if (Chance(0x5000 * i))
                                evPost(pSpr->index, 3, Random(5), kCallbackEnemeyBubble);
                        }
                    }
                    break;
            }
        }
    }

    GetSpriteExtents(pSpr, &top, &bottom);

    if (floorZ <= bottom)
    {
        gSpriteHit[nXSpr].florhit = floorHit;
        int v30 = zvel[nSpr] - velFloor[pSpr->sectnum];

        if (v30 > 0)
        {
            pXSpr->physAttr |= kPhysFalling;
            actFloorBounceVector((int*)&xvel[nSpr], (int*)&yvel[nSpr], (int*)&v30, pSpr->sectnum, tmpFraction);
            zvel[nSpr] = v30;

            if (klabs(zvel[nSpr]) < 0x10000)
            {
                zvel[nSpr] = velFloor[pSpr->sectnum];
                pXSpr->physAttr &= ~kPhysFalling;
            }

            moveHit = floorHit;
            spritetype* pFX = NULL; spritetype* pFX2 = NULL;
            switch (tileGetSurfType(floorHit))
            {
                case kSurfLava:
                    if ((pFX = gFX.fxSpawn(FX_10, pSpr->sectnum, pSpr->x, pSpr->y, floorZ)) == NULL) break;
                    for (i = 0; i < 7; i++)
                    {
                        if ((pFX2 = gFX.fxSpawn(FX_14, pFX->sectnum, pFX->x, pFX->y, pFX->z)) == NULL) continue;
                        xvel[pFX2->index] = Random2(0x6aaaa);
                        yvel[pFX2->index] = Random2(0x6aaaa);
                        zvel[pFX2->index] = -Random(0xd5555);
                    }
                    break;
                case kSurfWater:
                    gFX.fxSpawn(FX_9, pSpr->sectnum, pSpr->x, pSpr->y, floorZ);
                    break;
            }
        }
        else if (zvel[nSpr] == 0)
        {
            pXSpr->physAttr &= ~kPhysFalling;
        }

    }
    else
    {
        gSpriteHit[nXSpr].florhit = 0;
        if (pXSpr->physAttr & kPhysGravity)
            pXSpr->physAttr |= kPhysFalling;
    }

    if (top <= ceilZ)
    {
        gSpriteHit[nXSpr].ceilhit = moveHit = ceilHit;
        pSpr->z += ClipLow(ceilZ - top, 0);
        if (zvel[nSpr] <= 0 && (pXSpr->physAttr & kPhysFalling))
            zvel[nSpr] = mulscale16(-zvel[nSpr], 0x2000);
    }
    else
    {
        gSpriteHit[nXSpr].ceilhit = 0;
        GetSpriteExtents(pSpr, &top, &bottom);
    }

    if (moveHit && pXSpr->Impact && !pXSpr->locked && !pXSpr->isTriggered && (pXSpr->state == pXSpr->restState || pXSpr->Interrutable))
    {
        if (IsThingSprite(pSpr))
            ChangeSpriteStat(nSpr, kStatThing);

        trTriggerSprite(nSpr, pXSpr, kCmdToggle, nSpr);
    }

    if (!xvel[nSpr] && !yvel[nSpr]) return;
    else if ((floorHit & 0xc000) == 0xc000)
    {

        int nHSpr = floorHit & 0x3fff;
        if ((sprite[nHSpr].cstat & 0x30) == 0)
        {
            xvel[nSpr] += mulscale2(4, pSpr->x - sprite[nHSpr].x);
            yvel[nSpr] += mulscale2(4, pSpr->y - sprite[nHSpr].y);
            return;
        }
    }

    pXSpr->height = ClipLow(floorZ - bottom, 0) >> 8;
    if (uwater || pXSpr->height >= 0x100)
        return;

    int nDrag = 0x2a00;
    if (pXSpr->height > 0)
        nDrag -= scale(nDrag, pXSpr->height, 0x100);

    xvel[nSpr] -= mulscale16r(xvel[nSpr], nDrag);
    yvel[nSpr] -= mulscale16r(yvel[nSpr], nDrag);
    if (approxDist(xvel[nSpr], yvel[nSpr]) < 0x1000)
        xvel[nSpr] = yvel[nSpr] = 0;

}



bool ceilIsTooLow(spritetype* pSprite) {
    if (pSprite != NULL) {

        sectortype* pSector = &sector[pSprite->sectnum];
        int a = pSector->ceilingz - pSector->floorz;
        int top, bottom;
        GetSpriteExtents(pSprite, &top, &bottom);
        int b = top - bottom;
        if (a > b) return true;
    }

    return false;
}

void aiSetGenIdleState(spritetype* pSprite, XSPRITE* pXSprite) {
    switch (pSprite->type) {
    case kDudeModernCustom:
        cdudeGet(pSprite)->NewState(kCdudeStateGenIdle);
        break;
    default:
        aiNewState(pSprite, pXSprite, &genIdle);
        break;
    }
}

// this function stops wind on all TX sectors affected by WindGen after it goes off state.
void windGenStopWindOnSectors(XSPRITE* pXSource) {
    spritetype* pSource = &sprite[pXSource->reference];
    if (pXSource->txID <= 0 && xsectRangeIsFine(sector[pSource->sectnum].extra)) {
        xsector[sector[pSource->sectnum].extra].windVel = 0;
        return;
    }

    for (int i = bucketHead[pXSource->txID]; i < bucketHead[pXSource->txID + 1]; i++) {
        if (rxBucket[i].type != OBJ_SECTOR) continue;
        XSECTOR* pXSector = &xsector[sector[rxBucket[i].index].extra];
        if ((pXSector->state == 1 && !pXSector->windAlways)
            || ((pSource->flags & kModernTypeFlag1) && !(pSource->flags & kModernTypeFlag2))) {
                pXSector->windVel = 0;
        }
    }
    
    // check redirected TX buckets
    int rx = -1; XSPRITE* pXRedir = NULL;
    while ((pXRedir = evrListRedirectors(OBJ_SPRITE, sprite[pXSource->reference].extra, pXRedir, &rx)) != NULL) {
        for (int i = bucketHead[rx]; i < bucketHead[rx + 1]; i++) {
            if (rxBucket[i].type != OBJ_SECTOR) continue;
            XSECTOR* pXSector = &xsector[sector[rxBucket[i].index].extra];
            if ((pXSector->state == 1 && !pXSector->windAlways) || (pSource->flags & kModernTypeFlag2))
                pXSector->windVel = 0;
        }
    }
}


void trPlayerCtrlStartScene(XSPRITE* pXSource, PLAYER* pPlayer, bool force) {

    int nSource = pXSource->reference; TRPLAYERCTRL* pCtrl = &gPlayerCtrl[pPlayer->nPlayer];

    if (pCtrl->qavScene.index >= 0 && !force) return;

    QAV* pQav = playerQavSceneLoad(pXSource->data2);
    if (pQav != NULL) {

        // save current weapon
        pXSource->dropMsg = pPlayer->curWeapon;

        short nIndex = pCtrl->qavScene.index;
        if (nIndex > -1 && nIndex != nSource && sprite[nIndex].extra >= 0)
            pXSource->dropMsg = xsprite[sprite[nIndex].extra].dropMsg;

        if (nIndex < 0)
            WeaponLower(pPlayer);

        pXSource->sysData1 = ClipLow((pQav->at10 * pXSource->waitTime) / 4, 0); // how many times animation should be played

        pCtrl->qavScene.index = nSource;
        pCtrl->qavScene.qavResrc = pQav;
        pCtrl->qavScene.dummy = kCauserGame;

        pCtrl->qavScene.qavResrc->Preload();

        pPlayer->sceneQav = pXSource->data2;
        pPlayer->weaponTimer = pCtrl->qavScene.qavResrc->at10;
        pPlayer->qavCallback = (pXSource->data3 > 0) ? ClipRange(pXSource->data3 - 1, 0, 32) : -1;
        pPlayer->qavLoop = false;
    }

}

void trPlayerCtrlStopScene(PLAYER* pPlayer) {

    TRPLAYERCTRL* pCtrl = &gPlayerCtrl[pPlayer->nPlayer];
    int scnIndex = pCtrl->qavScene.index; XSPRITE* pXSource = NULL;
    if (spriRangeIsFine(scnIndex)) {
        pXSource = &xsprite[sprite[scnIndex].extra];
        pXSource->sysData1 = 0;
    }

    if (pCtrl->qavScene.index >= 0) {
        pCtrl->qavScene.index = -1;
        pCtrl->qavScene.qavResrc = NULL;
        pPlayer->sceneQav = -1;

        // restore weapon
        if (pPlayer->pXSprite->health > 0) {
            int oldWeapon = (pXSource && pXSource->dropMsg != 0) ? pXSource->dropMsg : 1;
            pPlayer->input.newWeapon = pPlayer->curWeapon = oldWeapon;
            WeaponRaise(pPlayer);
        }
    }

}

void trPlayerCtrlLink(XSPRITE* pXSource, PLAYER* pPlayer, bool checkCondition) {

    // save player's sprite index to let the tracking condition know it after savegame loading...
    pXSource->sysData1                  = pPlayer->nSprite;
    
    pPlayer->pXSprite->txID             = pXSource->txID;
    pPlayer->pXSprite->command          = kCmdToggle;
    pPlayer->pXSprite->triggerOn        = pXSource->triggerOn;
    pPlayer->pXSprite->triggerOff       = pXSource->triggerOff;
    pPlayer->pXSprite->busyTime         = pXSource->busyTime;
    pPlayer->pXSprite->waitTime         = pXSource->waitTime;
    pPlayer->pXSprite->restState        = pXSource->restState;

    pPlayer->pXSprite->Push             = pXSource->Push;
    pPlayer->pXSprite->Impact           = pXSource->Impact;
    pPlayer->pXSprite->Vector           = pXSource->Vector;
    pPlayer->pXSprite->Touch            = pXSource->Touch;
    pPlayer->pXSprite->Sight            = pXSource->Sight;
    pPlayer->pXSprite->Proximity        = pXSource->Proximity;

    pPlayer->pXSprite->Decoupled        = pXSource->Decoupled;
    pPlayer->pXSprite->Interrutable     = pXSource->Interrutable;
    pPlayer->pXSprite->DudeLockout      = pXSource->DudeLockout;

    pPlayer->pXSprite->data1            = pXSource->data1;
    pPlayer->pXSprite->data2            = pXSource->data2;
    pPlayer->pXSprite->data3            = pXSource->data3;
    pPlayer->pXSprite->data4            = pXSource->data4;

    pPlayer->pXSprite->key              = pXSource->key;
    pPlayer->pXSprite->dropMsg          = pXSource->dropMsg;

    pPlayer->pXSprite->dudeAmbush       = pXSource->dudeAmbush;
    pPlayer->pXSprite->dudeGuard        = pXSource->dudeGuard;
    pPlayer->pXSprite->dudeDeaf         = pXSource->dudeDeaf;
    pPlayer->pXSprite->dudeFlag4        = pXSource->dudeFlag4;
    pPlayer->pXSprite->unused1          = pXSource->unused1;

    // let's check if there is tracking condition expecting objects with this TX id
    if (checkCondition && pXSource->txID >= kChannelUser)
        conditionsLinkPlayer(pXSource, pPlayer);
}

void trPlayerCtrlSetRace(XSPRITE* pXSource, PLAYER* pPlayer) {
    playerSetRace(pPlayer, pXSource->data2);
    switch (pPlayer->lifeMode) {
        case kModeHuman:
        case kModeBeast:
            playerSizeReset(pPlayer);
            break;
        case kModeHumanShrink:
            playerSizeShrink(pPlayer, 2);
            break;
        case kModeHumanGrown:
            playerSizeGrow(pPlayer, 2);
            break;
    }
}

void trPlayerCtrlSetMoveSpeed(XSPRITE* pXSource, PLAYER* pPlayer) {
    
    int speed = ClipRange(pXSource->data2, 0, 500);
    for (int i = 0; i < kModeMax; i++) {
        for (int a = 0; a < kPostureMax; a++) {
            POSTURE* curPosture = &pPlayer->pPosture[i][a]; POSTURE* defPosture = &gPostureDefaults[i][a];
            curPosture->frontAccel = (defPosture->frontAccel * speed) / kPercFull;
            curPosture->sideAccel = (defPosture->sideAccel * speed) / kPercFull;
            curPosture->backAccel = (defPosture->backAccel * speed) / kPercFull;
        }
    }
}

void trPlayerCtrlSetJumpHeight(XSPRITE* pXSource, PLAYER* pPlayer) {
    
    int jump = ClipRange(pXSource->data3, 0, 500);
    for (int i = 0; i < kModeMax; i++) {
        POSTURE* curPosture = &pPlayer->pPosture[i][kPostureStand]; POSTURE* defPosture = &gPostureDefaults[i][kPostureStand];
        curPosture->normalJumpZ = (defPosture->normalJumpZ * jump) / kPercFull;
        curPosture->pwupJumpZ = (defPosture->pwupJumpZ * jump) / kPercFull;
    }
}

void trPlayerCtrlSetScreenEffect(XSPRITE* pXSource, PLAYER* pPlayer) {
    
    int eff = ClipLow(pXSource->data2, 0); int time = (eff > 0) ? pXSource->data3 : 0;
    switch (eff) {
        case 0: // clear all
        case 1: // tilting
            pPlayer->tiltEffect = ClipRange(time, 0, 220);
            if (eff) break;
            fallthrough__;
        case 2: // pain
            pPlayer->painEffect = ClipRange(time, 0, 2048);
            if (eff) break;
            fallthrough__;
        case 3: // blind
            pPlayer->blindEffect = ClipRange(time, 0, 2048);
            if (eff) break;
            fallthrough__;
        case 4: // pickup
            pPlayer->pickupEffect = ClipRange(time, 0, 2048);
            if (eff) break;
            fallthrough__;
        case 5: // quakeEffect
            pPlayer->quakeEffect = ClipRange(time, 0, 2048);
            if (eff) break;
            fallthrough__;
        case 6: // visibility
            pPlayer->visibility = ClipRange(time, 0, 2048);
            if (eff) break;
            fallthrough__;
        case 7: // delirium
            pPlayer->pwUpTime[kPwUpDeliriumShroom] = ClipHigh(time << 1, 432000);
            break;
    }

}

void trPlayerCtrlSetLookAngle(XSPRITE* pXSource, PLAYER* pPlayer) {
    
    CONSTEXPR int upAngle = 289; CONSTEXPR int downAngle = -347;
    CONSTEXPR double lookStepUp = 4.0 * upAngle / 60.0;
    CONSTEXPR double lookStepDown = -4.0 * downAngle / 60.0;

    int look = pXSource->data2 << 5;
    if (look > 0) pPlayer->q16look = fix16_min(mulscale8(F16(lookStepUp), look), F16(upAngle));
    else if (look < 0) pPlayer->q16look = -fix16_max(mulscale8(F16(lookStepDown), abs(look)), F16(downAngle));
    else pPlayer->q16look = 0;

}

void trPlayerCtrlEraseStuff(XSPRITE* pXSource, PLAYER* pPlayer) {
    
    switch (pXSource->data2) {
        case 0: // erase all
            fallthrough__;
        case 1: // erase weapons
            WeaponLower(pPlayer);

            for (int i = 0; i < kWeaponMax; i++) {
                pPlayer->hasWeapon[i] = false;
                // also erase ammo
                if (i < 12) pPlayer->ammoCount[i] = 0;
            }

            pPlayer->hasWeapon[kWeaponPitchfork] = true;
            pPlayer->curWeapon = kWeaponNone;
            pPlayer->nextWeapon = kWeaponPitchfork;

            WeaponRaise(pPlayer);
            if (pXSource->data2) break;
            fallthrough__;
        case 2: // erase all armor
            for (int i = 0; i < 3; i++) pPlayer->armor[i] = 0;
            if (pXSource->data2) break;
            fallthrough__;
        case 3: // erase all pack items
            for (int i = 0; i < 5; i++) {
                pPlayer->packSlots[i].isActive = false;
                pPlayer->packSlots[i].curAmount = 0;
            }
            pPlayer->packItemId = -1;
            if (pXSource->data2) break;
            fallthrough__;
        case 4: // erase all keys
            for (int i = 0; i < 8; i++) pPlayer->hasKey[i] = false;
            if (pXSource->data2) break;
            fallthrough__;
        case 5: // erase powerups
            for (int i = 0; i < kMaxPowerUps; i++) pPlayer->pwUpTime[i] = 0;
            break;
    }
}

void trPlayerCtrlGiveStuff(XSPRITE* pXSource, PLAYER* pPlayer, TRPLAYERCTRL* pCtrl) {
    
    int weapon = pXSource->data3;
    switch (pXSource->data2) {
        case 1: // give N weapon and default ammo for it
        case 2: // give just N ammo for selected weapon
            if (weapon <= kWeaponNone || weapon > kWeaponBeast) {
                consoleSysMsg("Weapon #%d is out of a weapons range!");
                break;
            } else if (pXSource->data2 == 2 && pXSource->data4 == 0) {
                consoleSysMsg("Zero ammo for weapon #%d is specyfied!");
                break;
            }
            switch (weapon) {
                case 11: // remote bomb 
                case 12: // prox bomb
                    pPlayer->hasWeapon[weapon] = true;
                    weapon--;
                    pPlayer->ammoCount[weapon] = ClipHigh(pPlayer->ammoCount[weapon] + ((pXSource->data2 == 2) ? pXSource->data4 : 1), gAmmoInfo[weapon].max);
                    weapon++;
                    break;
                default:
                    for (int i = 0; i < 11; i++) {
                        if (gWeaponItemData[i].type != weapon) continue;

                        WEAPONITEMDATA* pWeaponData = &gWeaponItemData[i]; int nAmmoType = pWeaponData->ammoType;
                        switch (pXSource->data2) {
                            case 1:
                                pPlayer->hasWeapon[weapon] = true;
                                if (pPlayer->ammoCount[nAmmoType] >= pWeaponData->count) break;
                                pPlayer->ammoCount[nAmmoType] = ClipHigh(pPlayer->ammoCount[nAmmoType] + pWeaponData->count, gAmmoInfo[nAmmoType].max);
                                break;
                            case 2:
                                pPlayer->ammoCount[nAmmoType] = ClipHigh(pPlayer->ammoCount[nAmmoType] + pXSource->data4, gAmmoInfo[nAmmoType].max);
                                break;
                        }
                        break;
                    }
                    break;
            }
            if (pPlayer->hasWeapon[weapon] && pXSource->data4 == 0) { // switch on it
                pPlayer->nextWeapon = kWeaponNone;

                if (pPlayer->sceneQav >= 0 && spriRangeIsFine(pCtrl->qavScene.index)) {
                    XSPRITE* pXScene = &xsprite[sprite[pCtrl->qavScene.index].extra];
                    pXScene->dropMsg = weapon;
                } else if (pPlayer->curWeapon != weapon) {
                    pPlayer->input.newWeapon = weapon;
                    WeaponRaise(pPlayer);
                }
            }
            break;
    }
}

void trPlayerCtrlUsePackItem(XSPRITE* pXSource, PLAYER* pPlayer, int evCmd) {
    unsigned int invItem = pXSource->data2 - 1;
    switch (evCmd) {
        case kCmdOn:
            if (!pPlayer->packSlots[invItem].isActive) packUseItem(pPlayer, invItem);
            break;
        case kCmdOff:
            if (pPlayer->packSlots[invItem].isActive) packUseItem(pPlayer, invItem);
            break;
        default:
            packUseItem(pPlayer, invItem);
            break;
    }

    switch (pXSource->data4) {
        case 2: // both
        case 0: // switch on it
            if (pPlayer->packSlots[invItem].curAmount > 0) pPlayer->packItemId = invItem;
            if (!pXSource->data4) break;
            fallthrough__;
        case 1: // force remove after use
            pPlayer->packSlots[invItem].isActive = false;
            pPlayer->packSlots[invItem].curAmount = 0;
            break;
    }
}

void trPlayerCtrlUsePowerup(XSPRITE* pXSource, PLAYER* pPlayer, int evCmd) {

    UNREFERENCED_PARAMETER(evCmd);

    spritetype* pSource = &sprite[pXSource->reference];
    bool relative = (pSource->flags & kModernTypeFlag1);

    int nPower = (kMinAllowedPowerup + pXSource->data2) - 1;
    int nTime = ClipRange(abs(pXSource->data3) * 100, -gPowerUpInfo[nPower].maxTime, gPowerUpInfo[nPower].maxTime);
    if (pXSource->data3 < 0)
        nTime = -nTime;

    
    if (pPlayer->pwUpTime[nPower]) {
       if (!relative && nTime <= 0)
           powerupDeactivate(pPlayer, nPower);

    }

    if (nTime != 0) {
        
        if (pPlayer->pwUpTime[nPower] <= 0)
            powerupActivate(pPlayer, nPower);  // MUST activate first for powerups like kPwUpDeathMask
        
        // ...so we able to change time amount
        if (relative) pPlayer->pwUpTime[nPower] += nTime;
        else pPlayer->pwUpTime[nPower] = nTime;
    }

    if (pPlayer->pwUpTime[nPower] <= 0)
        powerupDeactivate(pPlayer, nPower);

    return;

}

void trPlayerCtrlSendInput(XSPRITE* pXSrc, PLAYER* pPlay)
{
    GINPUT* p = &gInput; GINPUTINFO* pInfo;
    int nVal;

    if (!rngok(pXSrc->data2, 0, LENGTH(gInputInfo)))
        return;

    pInfo = &gInputInfo[pXSrc->data2];
    nVal = pXSrc->data3;

    if (pInfo->valMin > 0 && nVal < 0) nVal = klabs(nVal);
    if (pInfo->valMin < 0 && nVal > 0) nVal = -nVal;
    if (irngok(nVal, pInfo->valMin, pInfo->valMax))
    {
        spritetype* pSrc = &sprite[pXSrc->reference];

        if ((pSrc->flags & kModernTypeFlag1) == 0)
        {
            if (nVal == 0)
                nVal = pInfo->valDef;
        }

        if (pSrc->flags & kModernTypeFlag2)
            Bmemset(p, 0, sizeof(*p));
        
        nVal <<= pInfo->valShl;
    
        switch (pXSrc->data2)
        {
            case 1:
            case 2:
            case 3:
            case 4:
                if (nVal == 0)
                {
                    GINPUT* p2 = &gFifoInput[(gNetFifoTail - 1) & 255][pPlay->nPlayer];
                    nVal = (pXSrc->data2 == 1 || pXSrc->data2 == 3) ? 1024 : -1024;
                    nVal <<= p2->syncFlags.run;
                }
                (pXSrc->data2 < 3) ? p->forward = nVal : p->strafe = nVal;
                break;
            case 5: p->buttonFlags.jump = nVal;                     break;
            case 6: p->buttonFlags.crouch = nVal;                   break;
            case 7: p->buttonFlags.shoot = nVal;                    break;
            case 8: p->buttonFlags.shoot2 = nVal;                   break;
            case 9: p->keyFlags.action = nVal;                      break;
            case 10: case 11: p->q16turn = nVal;                    break;
            case 12: case 13: p->q16mlook = nVal;                   break;
            case 14: p->keyFlags.lookCenter = nVal;                 break;
            case 15:
                if (numplayers == 1) pPlay->lastWeapon = pPlay->curWeapon;
                p->keyFlags.holsterWeapon = nVal;
                break;
            case 16:
                p->newWeapon = (nVal) ? nVal : pPlay->lastWeapon;
                break;
            case 17: p->keyFlags.nextWeapon = nVal;                 break;
            case 18: p->keyFlags.prevWeapon = nVal;                 break;
            case 19:
                switch (nVal)
                {
                    case 0: p->keyFlags.useItem = 1;                break;
                    case 1: p->useFlags.useBeastVision = 1;         break;
                    case 2: p->useFlags.useCrystalBall = 1;         break;
                    case 3: p->useFlags.useJumpBoots = 1;           break;
                    case 4: p->useFlags.useMedKit = 1;              break;
                }
                break;
            case 20: p->keyFlags.nextItem = nVal;                   break;
            case 21: p->keyFlags.prevItem = nVal;                   break;
            case 22: p->keyFlags.pause = nVal;                      break;
            case 23: p->keyFlags.restart = nVal;                    break;
            case 24: p->keyFlags.quit = nVal;                       break;
        }

        if (numplayers > 1)
        {
            if (pInfo->sync)
            {
                if (pInfo->sync & 0x01) p->syncFlags.keyChange      = 1;
                if (pInfo->sync & 0x02) p->syncFlags.useChange      = 1;
                if (pInfo->sync & 0x04) p->syncFlags.buttonChange   = 1;
                if (pInfo->sync & 0x08) p->syncFlags.mlookChange    = 1;
                if (pInfo->sync & 0x10) p->syncFlags.weaponChange   = 1;
            }

            Bmemcpy(&gNetInput, p, sizeof(*p));
        }
    }
}

void useObjResizer(XSPRITE* pXSource, short objType, int objIndex) {
    switch (objType) {
        case OBJ_SECTOR:
        if (valueIsBetween(pXSource->data1, -1, 32767))
            sector[objIndex].floorxpanning = ClipRange(pXSource->data1, 0, 255);

        if (valueIsBetween(pXSource->data2, -1, 32767))
            sector[objIndex].floorypanning = ClipRange(pXSource->data2, 0, 255);

        if (valueIsBetween(pXSource->data3, -1, 32767))
            sector[objIndex].ceilingxpanning = ClipRange(pXSource->data3, 0, 255);

        if (valueIsBetween(pXSource->data4, -1, 65535))
            sector[objIndex].ceilingypanning = ClipRange(pXSource->data4, 0, 255);
        break;
        case OBJ_SPRITE:
        // resize by seq scaling
            if (sprite[pXSource->reference].flags & kModernTypeFlag1)
            {
                if (valueIsBetween(pXSource->data1, -255, 32767))
                {
                int mulDiv = (valueIsBetween(pXSource->data2, 0, 257)) ? pXSource->data2 : 256;
                if (pXSource->data1 > 0) xsprite[sprite[objIndex].extra].scale = mulDiv * ClipHigh(pXSource->data1, 25);
                else if (pXSource->data1 < 0) xsprite[sprite[objIndex].extra].scale = mulDiv / ClipHigh(abs(pXSource->data1), 25);
                else xsprite[sprite[objIndex].extra].scale = 0;
            }
            }
        // resize by repeats
            else
            {
                if (valueIsBetween(pXSource->data1, -1, 32767))
                sprite[objIndex].xrepeat = ClipRange(pXSource->data1, 0, 255);
            
                if (valueIsBetween(pXSource->data2, -1, 32767))
                sprite[objIndex].yrepeat = ClipRange(pXSource->data2, 0, 255);
        }

        if (valueIsBetween(pXSource->data3, -1, 32767))
            sprite[objIndex].xoffset = ClipRange(pXSource->data3, 0, 255);

        if (valueIsBetween(pXSource->data4, -1, 65535))
            sprite[objIndex].yoffset = ClipRange(pXSource->data4, 0, 255);
        break;
    case OBJ_WALL:
        if (valueIsBetween(pXSource->data1, -1, 32767))
            wall[objIndex].xrepeat = ClipRange(pXSource->data1, 0, 255);

        if (valueIsBetween(pXSource->data2, -1, 32767))
            wall[objIndex].yrepeat = ClipRange(pXSource->data2, 0, 255);

        if (valueIsBetween(pXSource->data3, -1, 32767))
            wall[objIndex].xpanning = ClipRange(pXSource->data3, 0, 255);

        if (valueIsBetween(pXSource->data4, -1, 65535))
            wall[objIndex].ypanning = ClipRange(pXSource->data4, 0, 255);
        break;
    }

}

void usePropertiesChanger(XSPRITE* pXSource, short objType, int objIndex) {

    spritetype* pSource = &sprite[pXSource->reference];
    bool flag1 = (pSource->flags & kModernTypeFlag1);
    bool flag2 = (pSource->flags & kModernTypeFlag2);
    bool flag4 = (pSource->flags & kModernTypeFlag4);

    short data3 = (short)pXSource->data3;
    short data4 = (short)pXSource->data4;
    int nSector;

    switch (objType) {
        case OBJ_WALL:
        {
            walltype* pWall = &wall[objIndex]; int old = -1;

            // data1 = set this wall as first wall of sector or as sector.alignto wall
            if (valueIsBetween(pXSource->data1, -1, 32767))
            {
                if ((nSector = sectorofwall(objIndex)) >= 0)
                {
                    switch (pXSource->data1) {
                        case 1:
                            setfirstwall(nSector, objIndex);
                            /// !!! correct?
                            if (xwallRangeIsFine(wall[objIndex].extra)) break;
                            dbInsertXWall(objIndex);
                            break;
                        case 2:
                            qsector_filler[nSector] = ClipLow(objIndex - sector[nSector].wallptr, 0);
                            break;
                    }
                }
            }
            
            // data3 = set wall hitag
            if (valueIsBetween(data3, -1, 32767))
            {
                // relative
                if (flag1)
                {
                    if (flag4) pWall->hitag &= ~data3;
                    else pWall->hitag |= data3;
                }
                else pWall->hitag = data3; // absolute
            }

            // data4 = set wall cstat
            if (valueIsBetween(pXSource->data4, -1, 65535))
            {
                old = pWall->cstat;

                // relative
                if (flag1)
                {
                    if (flag4) pWall->cstat &= ~data4;
                    else pWall->cstat |= data4;
                }
                // absolute
                else
                {
                    pWall->cstat = data4;
                    if (!flag2)
                    {
                        // check for exceptions
                        if ((old & 0x2) && !(pWall->cstat & 0x2)) pWall->cstat |= 0x2; // kWallBottomSwap
                        if ((old & 0x4) && !(pWall->cstat & 0x4)) pWall->cstat |= 0x4; // kWallBottomOrg, kWallOutsideOrg
                        if ((old & 0x20) && !(pWall->cstat & 0x20)) pWall->cstat |= 0x20; // kWallOneWay

                        if ((old & 0x4000) && !(pWall->cstat & 0x4000)) pWall->cstat |= 0x4000; // kWallMoveForward
                        else if ((old & 0x8000) && !(pWall->cstat & 0x8000)) pWall->cstat |= 0x8000; // kWallMoveBackward
                    }
                }
            }
        }
        break;
        case OBJ_SPRITE:
        {
            spritetype* pSprite = &sprite[objIndex]; bool thing2debris = false;
            XSPRITE* pXSprite = &xsprite[pSprite->extra]; int old = -1;

            // data3 = set sprite hitag
            if (valueIsBetween(data3, -1, 32767))
            {
                old = pSprite->flags;
                
                // relative
                if (flag1)
                {
                    if (flag4) pSprite->flags &= ~data3; 
                    else pSprite->flags |= data3;
                }
                // absolute
                else
                {
                    pSprite->flags = data3;
                }
                
                // handle exceptions (check always because they are system)
                if ((old & kHitagFree) && !(pSprite->flags & kHitagFree)) pSprite->flags |= kHitagFree;
                if ((old & kHitagRespawn) && !(pSprite->flags & kHitagRespawn)) pSprite->flags |= kHitagRespawn;

                // prepare things for different (debris) physics.
                thing2debris = (pSprite->statnum == kStatThing);
            }

            // data2 = sprite physics settings
            if (valueIsBetween(pXSource->data2, -1, 32767) || thing2debris) {
                switch (pSprite->statnum) {
                case kStatDude: // dudes already treating in game
                case kStatFree:
                case kStatMarker:
                case kStatPathMarker:
                    break;
                default:
                    // store physics attributes in xsprite to avoid setting hitag for modern types!
                    int flags = (pXSprite->physAttr != 0) ? pXSprite->physAttr : 0;
                    int oldFlags = flags;

                    if (thing2debris)
                    {
                        // converting thing to debris
                        if ((pSprite->flags & kPhysMove) != 0) flags |= kPhysMove;
                        else flags &= ~kPhysMove;

                        if ((pSprite->flags & kPhysGravity) != 0) flags |= (kPhysGravity | kPhysFalling);
                        else flags &= ~(kPhysGravity | kPhysFalling);

                        pSprite->flags &= ~(kPhysMove | kPhysGravity | kPhysFalling);
                        xvel[objIndex] = yvel[objIndex] = zvel[objIndex] = 0;
                        pXSprite->restState = pXSprite->state;
                    }
                    else
                    {
                        static char digits[6];
                        memset(digits, 0, sizeof(digits));
                        Bsprintf(digits, "%d", pXSource->data2);
                        for (unsigned int i = 0; i < sizeof(digits); i++)
                            digits[i] = (digits[i] >= 48 && digits[i] <= 57) ? (digits[i] - 57) + 9 : 0;

                        // first digit of data2: set main physics attributes
                        switch (digits[0]) {
                            case 0:
                                flags &= ~kPhysMove;
                                flags &= ~(kPhysGravity | kPhysFalling);
                                break;
                            case 1:
                                flags |= kPhysMove;
                                flags &= ~(kPhysGravity | kPhysFalling);
                                break;
                            case 2:
                                flags &= ~kPhysMove;
                                flags |= (kPhysGravity | kPhysFalling);
                                break;
                            case 3:
                                flags |= kPhysMove;
                                flags |= (kPhysGravity | kPhysFalling);
                                break;
                        }

                        // second digit of data2: touch physics flags
                        switch (digits[1]) {
                            case 0:
                                flags &= ~kPhysDebrisTouch;
                                break;
                            case 1:
                                flags |= kPhysDebrisTouch;
                                break;
                        }

                        // third digit of data2: weapon physics flags
                        switch (digits[2]) {
                            case 0:
                                flags &= ~kPhysDebrisVector;
                                flags &= ~kPhysDebrisExplode;
                                break;
                            case 1:
                                flags |= kPhysDebrisVector;
                                flags &= ~kPhysDebrisExplode;
                                break;
                            case 2:
                                flags &= ~kPhysDebrisVector;
                                flags |= kPhysDebrisExplode;
                                break;
                            case 3:
                                flags |= kPhysDebrisVector;
                                flags |= kPhysDebrisExplode;
                                break;
                        }

                        // fourth digit of data2: swimming / flying physics flags
                        switch (digits[3]) {
                            case 0:
                                flags &= ~kPhysDebrisSwim;
                                flags &= ~kPhysDebrisFly;
                                flags &= ~kPhysDebrisFloat;
                                break;
                            case 1:
                                flags |= kPhysDebrisSwim;
                                flags &= ~kPhysDebrisFly;
                                flags &= ~kPhysDebrisFloat;
                                break;
                            case 2:
                                flags |= kPhysDebrisSwim;
                                flags |= kPhysDebrisFloat;
                                flags &= ~kPhysDebrisFly;
                                break;
                            case 3:
                                flags |= kPhysDebrisFly;
                                flags &= ~kPhysDebrisSwim;
                                flags &= ~kPhysDebrisFloat;
                                break;
                            case 4:
                                flags |= kPhysDebrisFly;
                                flags |= kPhysDebrisFloat;
                                flags &= ~kPhysDebrisSwim;
                                break;
                            case 5:
                                flags |= kPhysDebrisSwim;
                                flags |= kPhysDebrisFly;
                                flags &= ~kPhysDebrisFloat;
                                break;
                            case 6:
                                flags |= kPhysDebrisSwim;
                                flags |= kPhysDebrisFly;
                                flags |= kPhysDebrisFloat;
                                break;
                        }

                    }

                    bool exists = gPhysSpritesList.Exists(objIndex); // check if there is no sprite in list

                    // adding physics sprite in list
                    if ((flags & kPhysGravity) != 0 || (flags & kPhysMove) != 0)
                    {
                        if (oldFlags == 0)
                            xvel[objIndex] = yvel[objIndex] = zvel[objIndex] = 0;

                        pXSprite->physAttr = flags; // update physics attributes
                        
                        if (!exists)
                        {
                            // allow things to became debris, so they use different physics...
                            if (pSprite->statnum == kStatThing) ChangeSpriteStat(objIndex, 0);

                            // set random goal ang for swimming so they start turning
                            if ((flags & kPhysDebrisSwim) && !xvel[objIndex] && !yvel[objIndex] && !zvel[objIndex])
                                pXSprite->goalAng = (pSprite->ang + Random3(kAng45)) & kAngMask;

                            if (pXSprite->physAttr & kPhysDebrisVector)
                                pSprite->cstat |= CSTAT_SPRITE_BLOCK_HITSCAN;

                            gPhysSpritesList.Add(objIndex);
                            getSpriteMassBySize(pSprite); // create physics cache
                        }
                    }
                    // removing physics from sprite in list
                    else if (exists)
                    {
                        pXSprite->physAttr = flags;
                        xvel[objIndex] = yvel[objIndex] = zvel[objIndex] = 0;
                        if (IsThingSprite(pSprite))
                            ChangeSpriteStat(objIndex, kStatThing);  // if it was a thing - restore statnum

                        gPhysSpritesList.Remove(objIndex);
                    }
                    break;
                }
            }

            // data4 = sprite cstat
            if (valueIsBetween(pXSource->data4, -1, 65535))
            {
                old = pSprite->cstat;
                
                // relative
                if (flag1)
                {
                    if (flag4) pSprite->cstat &= ~data4;
                    else pSprite->cstat |= data4;
                }
                // absolute
                else
                {
                    pSprite->cstat = data4;
                    if (!flag2)
                    {
                        // check exceptions
                        if ((old & 0x1000) && !(pSprite->cstat & 0x1000)) pSprite->cstat |= 0x1000; //kSpritePushable
                        if ((old & 0x80) && !(pSprite->cstat & 0x80)) pSprite->cstat |= 0x80; // kSpriteOriginAlign

                        if ((old & 0x2000) && !(pSprite->cstat & 0x2000)) pSprite->cstat |= 0x2000; // kSpriteMoveForward, kSpriteMoveFloor
                        else if ((old & 0x4000) && !(pSprite->cstat & 0x4000)) pSprite->cstat |= 0x4000; // kSpriteMoveReverse, kSpriteMoveCeiling
                    }
                }
            }
        }
        break;
        case OBJ_SECTOR:
        {
            sectortype* pSector = &sector[objIndex];
            XSECTOR* pXSector = &xsector[sector[objIndex].extra];

            // data1 = sector underwater status and depth level
            if (pXSource->data1 >= 0 && pXSource->data1 < 2)
            {
                pXSector->Underwater = (pXSource->data1) ? true : false;

                spritetype* pLower = (gLowerLink[objIndex] >= 0) ? &sprite[gLowerLink[objIndex]] : NULL;
                XSPRITE* pXLower = NULL; spritetype* pUpper = NULL; XSPRITE* pXUpper = NULL;
                
                if (pLower) {
                    
                    pXLower = &xsprite[pLower->extra];

                    // must be sure we found exact same upper link
                    for (int i = 0; i < kMaxSectors; i++) {
                        if (gUpperLink[i] < 0 || xsprite[sprite[gUpperLink[i]].extra].data1 != pXLower->data1) continue;
                        pUpper = &sprite[gUpperLink[i]]; pXUpper = &xsprite[pUpper->extra];
                        break;
                    }
                }
                
                // treat sectors that have links, so warp can detect underwater status properly
                if (pLower) {
                    if (pXSector->Underwater) {
                        switch (pLower->type) {
                            case kMarkerLowStack:
                            case kMarkerLowLink:
                                pXLower->sysData1 = pLower->type;
                                pLower->type = kMarkerLowWater;
                                break;
                            default:
                                pXLower->sysData1 = (IsRorSector(objIndex, OBJ_CEILING)) ? kMarkerLowStack : kMarkerLowLink;
                                break;
                        }
                    }
                    else if (pXLower->sysData1 > 0) pLower->type = pXLower->sysData1;
                    else if (!IsRorSector(objIndex, OBJ_CEILING)) pLower->type = kMarkerLowLink;
                    else pLower->type = kMarkerLowStack;
                }

                if (pUpper) {
                    if (pXSector->Underwater) {
                        switch (pUpper->type) {
                            case kMarkerUpStack:
                            case kMarkerUpLink:
                                pXUpper->sysData1 = pUpper->type;
                                pUpper->type = kMarkerUpWater;
                                break;
                            default:
                                pXLower->sysData1 = (IsRorSector(objIndex, OBJ_FLOOR)) ? kMarkerUpStack : kMarkerUpLink;
                                break;
                        }
                    }
                    else if (pXUpper->sysData1 > 0) pUpper->type = pXUpper->sysData1;
                    else if (!IsRorSector(objIndex, OBJ_FLOOR)) pUpper->type = kMarkerUpLink;
                    else pUpper->type = kMarkerUpStack;
                }

                // search for dudes in this sector and change their underwater status
                for (int nSprite = headspritesect[objIndex]; nSprite >= 0; nSprite = nextspritesect[nSprite]) {

                    spritetype* pSpr = &sprite[nSprite];
                    if (pSpr->statnum != kStatDude || !IsDudeSprite(pSpr) || !xspriRangeIsFine(pSpr->extra))
                        continue;

                    PLAYER* pPlayer = getPlayerById(pSpr->type);
                    if (pXSector->Underwater) {
                        if (pLower)
                            xsprite[pSpr->extra].medium = (pLower->type == kMarkerUpGoo) ? kMediumGoo : kMediumWater;

                        if (pPlayer) {
                            int waterPal = kMediumWater;
                            if (pLower) {
                                if (pXLower->data2 > 0) waterPal = pXLower->data2;
                                else if (pLower->type == kMarkerUpGoo) waterPal = kMediumGoo;
                            }

                            pPlayer->nWaterPal = waterPal;
                            pPlayer->posture = kPostureSwim;
                            pPlayer->pXSprite->burnTime = 0;
                        }

                    } else {

                        xsprite[pSpr->extra].medium = kMediumNormal;
                        if (pPlayer) {
                            pPlayer->posture = (!pPlayer->input.buttonFlags.crouch) ? kPostureStand : kPostureCrouch;
                            pPlayer->nWaterPal = 0;
                        }

                    }
                }
            }
            else if (pXSource->data1 > 9) pXSector->Depth = 7;
            else if (pXSource->data1 > 1) pXSector->Depth = pXSource->data1 - 2;


            // data2 = sector visibility
            if (valueIsBetween(pXSource->data2, -1, 32767))
                pSector->visibility = ClipRange(pXSource->data2, 0 , 234);

            // data3 = sector ceil cstat
            if (valueIsBetween(pXSource->data3, -1, 32767))
            {
                // relative
                if (flag1)
                {
                    if (flag4) pSector->ceilingstat &= ~data3;
                    else pSector->ceilingstat |= data3;
                }
                // absolute
                else pSector->ceilingstat = data3;
            }

            // data4 = sector floor cstat
            if (valueIsBetween(pXSource->data4, -1, 65535))
            {
                // relative
                if (flag1)
                {
                    if (flag4) pSector->floorstat &= ~data4;
                    else pSector->floorstat |= data4;
                }
                // absolute
                else pSector->floorstat = data4;
            }
        }
        break;
        // no TX id
        case -1:
            // data2 = global visibility
            if (valueIsBetween(pXSource->data2, -1, 32767))
                gVisibility = ClipRange(pXSource->data2, 0, 4096);
        break;
    }

}

spritetype* getCauser(int nSpr) { return (nSpr != kCauserGame) ? &sprite[nSpr] : NULL; }

void useVelocityChanger(XSPRITE* pXSource, int causerID, short objType, int objIndex)
{
    #define kVelShift       8
    #define kScaleVal       0x10000
    
    int t, r = 0, nAng, vAng;
    int xv = 0, yv = 0, zv = 0;

    spritetype* pCauser;
    spritetype* pSource = &sprite[pXSource->reference];
    bool relative   = (pSource->flags & kModernTypeFlag1);
    bool toDstAng   = (pSource->flags & kModernTypeFlag2);
    bool toSrcAng   = (pSource->flags & kModernTypeFlag4);
    bool toRndAng   = (pSource->flags & kModernTypeFlag8);
    bool chgDstAng  = !(pSource->flags & kModernTypeFlag16);
    bool toRelAng   = (pSource->flags & kModernTypeFlag64);
    bool toEvnAng   = (toDstAng && toSrcAng && (pCauser = getCauser(causerID)) != NULL);
    bool toAng      = (toRelAng || toDstAng || toSrcAng || toEvnAng || toRndAng);
    bool toAng180   = (toRndAng && (toDstAng || toSrcAng || toEvnAng));

    if (objType == OBJ_SPRITE)
    {
        spritetype* pSpr = &sprite[objIndex];
        if ((r = mulscale14(pXSource->data4 << kVelShift, kScaleVal)) != 0)
            r = nnExtRandom(-r, r);

        if (valueIsBetween(pXSource->data3, -32767, 32767))
        {
            if ((zv = mulscale14(pXSource->data3 << kVelShift, kScaleVal)) != 0)
                zv += r;
        }
        else if (!relative)
        {
            zv = zvel[pSpr->index];
        }

        if (!toAng)
        {
            if (valueIsBetween(pXSource->data1, -32767, 32767))
            {
                if ((xv = mulscale14(pXSource->data1 << kVelShift, kScaleVal)) != 0)
                    xv += r;
            }
            else if (!relative)
            {
                xv = xvel[pSpr->index];
            }

            if (valueIsBetween(pXSource->data2, -32767, 32767))
            {
                if ((yv = mulscale14(pXSource->data2 << kVelShift, kScaleVal)) != 0)
                    yv += r;
            }
            else if (!relative)
            {
                yv = yvel[pSpr->index];
            }
        }
        else
        {
            if (toEvnAng)       nAng = pCauser->ang;
            else if (toSrcAng)  nAng = pSource->ang;
            else                nAng = pSpr->ang;

            if (toRelAng)
               nAng += pSource->ang;

            nAng = nAng & kAngMask;

            if (!toAng180 && toRndAng)
            {
                t = nAng;
                while (t == nAng)
                    nAng = nnExtRandom(0, kAng360);
            }

            if (chgDstAng)
                changeSpriteAngle(pSpr, nAng);

            if ((t = (pXSource->data1 << kVelShift)) != 0)
                t += r;

            xv = mulscale14(t, Cos(nAng) >> 16);
            yv = mulscale14(t, Sin(nAng) >> 16);
        }

        if (pXSource->physAttr)
        {
            t = 1;
            switch (pSpr->statnum) {
                case kStatThing:
                    break;
                case kStatFX:
                    t = 0;
                    fallthrough__;
                case kStatDude:
                case kStatProjectile:
                    if (pXSource->physAttr & kPhysMove)     pSpr->flags |= kPhysMove;     else pSpr->flags  &= ~kPhysMove;
                    if (pXSource->physAttr & kPhysGravity)  pSpr->flags |= kPhysGravity;  else pSpr->flags  &= ~kPhysGravity;
                    if (pXSource->physAttr & kPhysFalling)  pSpr->flags |= kPhysFalling;  else pSpr->flags  &= ~kPhysFalling;
                    break;
            }

            // debris physics for sprites that is allowed
            if (t)
            {
                if (pSpr->extra <= 0)
                    dbInsertXSprite(pSpr->index);

                if (pSpr->statnum == kStatThing)
                {
                    pSpr->flags &= ~(kPhysMove | kPhysGravity | kPhysFalling);
                    ChangeSpriteStat(pSpr->index, 0);
                }

                XSPRITE* pXSpr = &xsprite[pSpr->extra];
                pXSpr->physAttr = pXSource->physAttr;
                if (!gPhysSpritesList.Exists(pSpr->index))
                {
                    getSpriteMassBySize(pSpr);
                    gPhysSpritesList.Add(pSpr->index);
                }
            }
        }

        if (relative)
        {
            xvel[pSpr->index] += xv;
            yvel[pSpr->index] += yv;
            zvel[pSpr->index] += zv;
        }
        else
        {
            xvel[pSpr->index] = xv;
            yvel[pSpr->index] = yv;
            zvel[pSpr->index] = zv;
        }

        vAng = getVelocityAngle(pSpr);

        if (toAng)
        {
            if (toAng180)
                RotatePoint(&xvel[pSpr->index], &yvel[pSpr->index], kAng180, pSpr->x, pSpr->y);
            else
                RotatePoint(&xvel[pSpr->index], &yvel[pSpr->index], (nAng - vAng) & kAngMask, pSpr->x, pSpr->y);


            vAng = getVelocityAngle(pSpr);
        }

        if (chgDstAng)
            changeSpriteAngle(pSpr, vAng);

        if (pSpr->owner >= 0)
        {
            // hack to make player projectiles damage it's owner
            if (pSpr->statnum == kStatProjectile && (t = actOwnerIdToSpriteId(pSpr->owner)) >= 0 && IsPlayerSprite(&sprite[t]))
                actPropagateSpriteOwner(pSpr, pSource);
        }

        viewCorrectPrediction();

        //if (pXSource->rxID == 157)
        //viewSetSystemMessage("%d: %d  /  %d  /  %d, C: %d", pSpr->sectnum, xvel[pSpr->index], yvel[pSpr->index], zvel[pSpr->index], sprite[causerID].type);
    }
    else if (objType == OBJ_SECTOR)
    {
        for (t = headspritesect[objIndex]; t >= 0; t = nextspritesect[t])
        {
            useVelocityChanger(pXSource, causerID, OBJ_SPRITE, t);
        }
    }
}

void useTeleportTarget(XSPRITE* pXSource, spritetype* pSprite) {
    spritetype* pSource = &sprite[pXSource->reference]; PLAYER* pPlayer = getPlayerById(pSprite->type);
    XSECTOR* pXSector = (sector[pSource->sectnum].extra >= 0) ? &xsector[sector[pSource->sectnum].extra] : NULL;
    bool isDude = (!pPlayer && IsDudeSprite(pSprite));

    if (pSprite->sectnum != pSource->sectnum)
        ChangeSpriteSect(pSprite->index, pSource->sectnum);

    pSprite->x = pSource->x; pSprite->y = pSource->y;
    int zTop, zBot; GetSpriteExtents(pSource, &zTop, &zBot);
    pSprite->z = zBot;

    clampSprite(pSprite, 0x01);

    if (pSource->flags & kModernTypeFlag1) // force telefrag
        TeleFrag(pSprite->index, pSource->sectnum);

    if (pSprite->flags & kPhysGravity)
        pSprite->flags |= kPhysFalling;

    if (pXSector)
    {
        if (pXSector->Enter && (pPlayer || (isDude && !pXSector->dudeLockout)))
            trTriggerSector(pSource->sectnum, pXSector, kCmdSectorEnter, pSprite->index);

        if (pXSector->Underwater)
        {
            spritetype* pLink = (gLowerLink[pSource->sectnum] >= 0) ? &sprite[gLowerLink[pSource->sectnum]] : NULL;
            if (pLink)
            {
                // must be sure we found exact same upper link
                for (int i = 0; i < kMaxSectors; i++)
                {
                    if (gUpperLink[i] < 0 || xsprite[sprite[gUpperLink[i]].extra].data1 != xsprite[pLink->extra].data1) continue;
                    pLink = &sprite[gUpperLink[i]];
                    break;
                }
            }

            if (pLink)
                xsprite[pSprite->extra].medium = (pLink->type == kMarkerUpGoo) ? kMediumGoo : kMediumWater;

            if (pPlayer)
            {
                int waterPal = kMediumWater;
                if (pLink)
                {
                    if (xsprite[pLink->extra].data2 > 0) waterPal = xsprite[pLink->extra].data2;
                    else if (pLink->type == kMarkerUpGoo) waterPal = kMediumGoo;
                }

                pPlayer->nWaterPal = waterPal;
                pPlayer->posture = kPostureSwim;
                pPlayer->pXSprite->burnTime = 0;
            }
        }
        else
        {
            xsprite[pSprite->extra].medium = kMediumNormal;
            if (pPlayer)
            {
                pPlayer->posture = (!pPlayer->input.buttonFlags.crouch) ? kPostureStand : kPostureCrouch;
                pPlayer->nWaterPal = 0;
            }
        }
    }

    if (pSprite->statnum == kStatDude && IsDudeSprite(pSprite) && !IsPlayerSprite(pSprite))
    {
        XSPRITE* pXDude = &xsprite[pSprite->extra];
        int x = pXDude->targetX; int y = pXDude->targetY; int z = pXDude->targetZ;
        int target = pXDude->target;
        
        aiInitSprite(pSprite);
        if (spriRangeIsFine(target) && IsDudeSprite(&sprite[target]))
        {
            pXDude->targetX = x; pXDude->targetY = y; pXDude->targetZ = z;
            pXDude->target = target; aiActivateDude(pSprite, pXDude);
        }
    }

    if (pXSource->data3 == 1)
    {
        // reset velocity
        xvel[pSprite->index] = yvel[pSprite->index] = zvel[pSprite->index] = 0;
    }
    else if (pXSource->data3 > 0)
    {
        // change movement direction according source angle
        if (pXSource->data3 & kModernTypeFlag2)
        {
            int vAng = getVelocityAngle(pSprite);
            RotatePoint(&xvel[pSprite->index], &yvel[pSprite->index], (pSource->ang - vAng) & kAngMask, pSprite->x, pSprite->y);
        }

        if (pXSource->data3 & kModernTypeFlag4)
            zvel[pSprite->index] = 0;
    }
    
    if (pXSource->data2 == 1)
        changeSpriteAngle(pSprite, pSource->ang);

    viewBackupSpriteLoc(pSprite->index, pSprite);

    if (pXSource->data4 > 0)
        sfxPlay3DSound(pSource, pXSource->data4, -1, 0);

    if (pPlayer)
    {
        playerResetInertia(pPlayer);
        if (pXSource->data2 == 1)
            pPlayer->zViewVel = pPlayer->zWeaponVel = 0;
        if (pPlayer == gMe) // if player is listener, update ear position/reset ear velocity so audio pitch of surrounding sfx does not freak out when teleporting player
            sfxResetListener();
    }
}

void effectGenPropagateAppearance(spritetype* pSrc, spritetype* pDest, spritetype* pFx)
{
    if (pSrc->flags & kModernTypeFlag1)
    {
        pFx->pal = pSrc->pal;
        pFx->xoffset = pSrc->xoffset;
        pFx->yoffset = pSrc->yoffset;
        pFx->xrepeat = pSrc->xrepeat;
        pFx->yrepeat = pSrc->yrepeat;
        pFx->shade = pSrc->shade;
    }

    if (pSrc->flags & kModernTypeFlag2)
    {
        pFx->cstat = pSrc->cstat;
        if (pFx->cstat & CSTAT_SPRITE_INVISIBLE)
            pFx->cstat &= ~CSTAT_SPRITE_INVISIBLE;
    }

    if (pSrc->flags & kModernTypeFlag8)
        pFx->ang = pDest->ang;
    else if (pSrc->flags & kModernTypeFlag4)
        pFx->ang = pSrc->ang;

    if (pSrc->flags & kModernTypeFlag16)
        pFx->picnum = pSrc->picnum;
}

void useEffectGen(XSPRITE* pXSource, spritetype* pSpr)
{
    int pos, top, bottom;
    int fxId = (pXSource->data3 <= 0) ? pXSource->data2 : pXSource->data2 + Random(pXSource->data3 + 1);
    spritetype* pSource = &sprite[pXSource->reference];
    spritetype* pFx = NULL;
    
    if (pSpr == NULL)
        pSpr = pSource;

    if (pSpr->sectnum < 0)
        return;

    GetSpriteExtents(pSpr, &top, &bottom);

    if (rngok(fxId, kEffectGenCallbackBase, kEffectGenCallbackBase + LENGTH(gEffectGenCallbacks)))
    {
        fxId = gEffectGenCallbacks[fxId - kEffectGenCallbackBase];
        evKill(pSpr->index, OBJ_SPRITE, (CALLBACK_ID)fxId);
        evPost(pSpr->index, OBJ_SPRITE, 0, (CALLBACK_ID)fxId);
    }
    else if (valueIsBetween(fxId, 0, kFXMax))
    {
        // select where exactly effect should be spawned
        switch (pXSource->data4)
        {
            case 1:
                pos = bottom;
                break;
            case 2: // middle
                pos = top + ((bottom-top) >> 1);
                break;
            case 3:
            case 4:
                if (sectRangeIsFine(pSpr->sectnum))
                {
                    if (pXSource->data4 == 3)
                        pos = getflorzofslope(pSpr->sectnum, pSpr->x, pSpr->y);
                    else
                        pos = getceilzofslope(pSpr->sectnum, pSpr->x, pSpr->y);

                    break;
                }
                fallthrough__;
            default:
                pos = top;
                break;
        }
        
        if ((pFx = gFX.fxSpawn((FX_ID)fxId, pSpr->sectnum, pSpr->x, pSpr->y, pos)) != NULL)
        {
            pFx->owner = pSource->index;
            effectGenPropagateAppearance(pSource, pSpr, pFx);
            if (pFx->cstat & CSTAT_SPRITE_ONE_SIDED)
                pFx->cstat &= ~CSTAT_SPRITE_ONE_SIDED;
        }
    }
}


void useSectorWindGen(XSPRITE* pXSource, sectortype* pSector) {

    spritetype* pSource = &sprite[pXSource->reference];
    XSECTOR* pXSector = NULL; int nXSector = 0;
    
    if (pSector != NULL) {
        pXSector = &xsector[pSector->extra];
        nXSector = sector[pXSector->reference].extra;
    } else if (xsectRangeIsFine(sector[pSource->sectnum].extra)) {
        pXSector = &xsector[sector[pSource->sectnum].extra];
        nXSector = sector[pXSector->reference].extra;
    } else {
        nXSector = dbInsertXSector(pSource->sectnum);
        pXSector = &xsector[nXSector]; pXSector->windAlways = 1;
    }

    int windVel = ClipRange(pXSource->data2, 0, 32767);
    if ((pXSource->data1 & 0x0001))
        windVel = nnExtRandom(0, windVel);
    

    pXSector->windVel = windVel;
    if ((pSource->flags & kModernTypeFlag1))
        pXSector->panAlways = pXSector->windAlways = 1;

    int ang = pSource->ang;
    if (pXSource->data4 <= 0) {
        if ((pXSource->data1 & 0x0002)) {
            while (pSource->ang == ang)
                pSource->ang = nnExtRandom(-kAng360, kAng360) & 2047;
        }
    }
    else if (pSource->cstat & 0x2000) pSource->ang += pXSource->data4;
    else if (pSource->cstat & 0x4000) pSource->ang -= pXSource->data4;
    else if (pXSource->sysData1 == 0) {
        if ((ang += pXSource->data4) >= kAng180) pXSource->sysData1 = 1;
        pSource->ang = ClipHigh(ang, kAng180);
    } else {
        if ((ang -= pXSource->data4) <= -kAng180) pXSource->sysData1 = 0;
        pSource->ang = ClipLow(ang, -kAng180);
    }

    pXSector->windAng = pSource->ang;

    if (pXSource->data3 > 0 && pXSource->data3 < 4) {
        switch (pXSource->data3) {
        case 1:
            pXSector->panFloor = true;
            pXSector->panCeiling = false;
            break;
        case 2:
            pXSector->panFloor = false;
            pXSector->panCeiling = true;
            break;
        case 3:
            pXSector->panFloor = true;
            pXSector->panCeiling = true;
            break;
        }

        short oldPan = pXSector->panVel;
        pXSector->panAngle = pXSector->windAng;
        pXSector->panVel = pXSector->windVel;

        // add to panList if panVel was set to 0 previously
        if (oldPan == 0 && pXSector->panVel != 0 && panCount < kMaxXSectors) {

            int i;
            for (i = 0; i < panCount; i++) {
                if (panList[i] != nXSector) continue;
                break;
            }

            if (i == panCount)
                panList[panCount++] = nXSector;
        }

    }
}

void useSpriteDamager(XSPRITE* pXSource, int objType, int objIndex) {

    spritetype* pSource = &sprite[pXSource->reference];
    sectortype* pSector = &sector[pSource->sectnum];

    int top, bottom, i;
    bool floor, ceil, wall, enter;

    switch (objType) {
        case OBJ_SPRITE:
            damageSprites(pXSource, &sprite[objIndex]);
            break;
        case OBJ_SECTOR:
            GetSpriteExtents(pSource, &top, &bottom);
            floor = (bottom >= pSector->floorz);    ceil = (top <= pSector->ceilingz);
            wall = (pSource->cstat & 0x10);         enter = (!floor && !ceil && !wall);
            for (i = headspritesect[objIndex]; i != -1; i = nextspritesect[i]) {
                if (!IsDudeSprite(&sprite[i]) || !xspriRangeIsFine(sprite[i].extra))
                    continue;
                else if (enter)
                    damageSprites(pXSource, &sprite[i]);
                else if (floor && (gSpriteHit[sprite[i].extra].florhit & 0xc000) == 0x4000 && (gSpriteHit[sprite[i].extra].florhit & 0x3fff) == objIndex)
                    damageSprites(pXSource, &sprite[i]);
                else if (ceil && (gSpriteHit[sprite[i].extra].ceilhit & 0xc000) == 0x4000 && (gSpriteHit[sprite[i].extra].ceilhit & 0x3fff) == objIndex)
                    damageSprites(pXSource, &sprite[i]);
                else if (wall && (gSpriteHit[sprite[i].extra].hit & 0xc000) == 0x8000 && sectorofwall(gSpriteHit[sprite[i].extra].hit & 0x3fff) == objIndex)
                    damageSprites(pXSource, &sprite[i]);
            }
            break;
        case -1:
            for (i = headspritestat[kStatDude]; i != -1; i = nextspritestat[i]) {
                if (sprite[i].statnum != kStatDude) continue;
                switch (pXSource->data1) {
                    case 667:
                        if (IsPlayerSprite(&sprite[i])) continue;
                        damageSprites(pXSource, &sprite[i]);
                        break;
                    case 668:
                        if (!IsPlayerSprite(&sprite[i])) continue;
                        damageSprites(pXSource, &sprite[i]);
                        break;
                    default:
                        damageSprites(pXSource, &sprite[i]);
                        break;
                }
            }
            break;
    }
}

void damageSprites(XSPRITE* pXSource, spritetype* pSpr)
{
    if (!IsDudeSprite(pSpr) || !xsprIsFine(pSpr) || pXSource->data3 < 0)
        return;

    XSPRITE* pXSpr = &xsprite[pSpr->extra];
    if (pXSpr->health <= 0 || pXSpr->locked)
        return;
    
    PLAYER* pPlayer = getPlayerById(pSpr->type);
    if (pPlayer && (powerupCheck(pPlayer, kPwUpDeathMask) || pPlayer->godMode))
        return;
    
    int health = 0;
    int dmgType = (pXSource->data2 >= kDmgFall) ? ClipHigh(pXSource->data2, kDmgElectric) : -1;
    int dmg = pXSpr->health; int armor[sizeof(gPlayer[0].armor)];

    spritetype* pSource = &sprite[pXSource->reference];
    bool immune         = (dmgType >= 0 && nnExtIsImmune(pSpr, dmgType, 0));
    bool showEffects    = !(pSource->flags & kModernTypeFlag2); // show it by default
    bool setHealth      = (!(pSource->flags & kModernTypeFlag8) || immune);
    bool forceRecoil    = (pSource->flags & kModernTypeFlag4);
    bool death          = false;
    

    if (pXSource->data3 > 0)
    {
        if (pSource->flags & kModernTypeFlag1)      dmg = ClipHigh(pXSource->data3 << 1, 65535);
        else if (pXSpr->sysData2 > 0)               dmg = (ClipHigh(pXSpr->sysData2 << 4, 65535) * pXSource->data3) / kPercFull;
        else                                        dmg = ((getDudeInfo(pSpr->type)->startHealth << 4) * pXSource->data3) / kPercFull;
        
        health = ClipLow(pXSpr->health - dmg, 0);
    }

    death = (health <= 0);

    if (!death)
    {
        if (dmgType >= kDmgFall && !immune)
        {
            if (pPlayer)
            {
                playerDamageArmor(pPlayer, (DAMAGE_TYPE)dmgType, dmg);

                memcpy(armor, pPlayer->armor, sizeof(armor)); memset(pPlayer->armor, 0, sizeof(armor));
                actDamageSprite(pSource->index, pSpr, (DAMAGE_TYPE)dmgType, dmg); // we need clear damage (no armor)
                memcpy(pPlayer->armor, armor, sizeof(armor));
            }
            else
            {
                actDamageSprite(pSource->index, pSpr, (DAMAGE_TYPE)dmgType, dmg);
            }
        }

        // check again if dude still alive
        if (IsDudeSprite(pSpr) && xsprIsFine(pSpr))
        {
            if (pXSpr->health <= 0)
            {
                if (dmgType < 0)
                    death = true;
                else
                    return;
            }
            else if (IsBurningDude(pSpr))
            {
                if (!rngok(pXSpr->burnTime, 1, 1200))
                    actBurnSprite(pSource->index, pXSpr, 1200);

                return;
            }
            else if (setHealth)
            {
                pXSpr->health = health;
            }

            // still may going to death 
            if (!death)
            {
                if (showEffects)
                {
                    switch (dmgType)
                    {
                        case kDmgBurn:
                            if (!rngok(pXSpr->burnTime, 1, 1200))
                            {
                                actBurnSprite(pSource->index, pXSpr, ClipLow(dmg >> 1, 128));
                                evKill(pSpr->index, OBJ_SPRITE, kCallbackFXFlameLick);
                                evPost(pSpr->index, OBJ_SPRITE, 0, kCallbackFXFlameLick); // show flames
                            }
                            break;
                        case kDmgElectric:
                            forceRecoil = true; // show tesla recoil animation
                            break;
                        case kDmgBullet:
                            evKill(pSpr->index, OBJ_SPRITE, kCallbackFXBloodSpurt);
                            for (int i = 0; i < 3; i++)
                            {
                                if (Chance(0x10000 >> i))
                                    fxSpawnBlood(pSpr, 0);
                            }
                            break;
                        case kDmgChoke:
                            if (!pPlayer || !Chance(0x2000)) break;
                            else pPlayer->blindEffect += ClipHigh(dmg << 2, 128);
                            break;
                    }
                }

                if (forceRecoil && !pPlayer)
                {
                    pXSpr->data3 = 32767; // to be sure dude will play the animation
                    gDudeExtra[pSpr->extra].teslaHit = (dmgType == kDmgElectric);
                    if (pXSpr->aiState->stateType != kAiStateRecoil)
                        RecoilDude(pSpr, pXSpr);
                }
            }
        }
        else
        {
            return;
        }
    }

    if (death)
    {
        if (dmgType < 0 || immune)
            dmgType = kDmgBullet;

        if (pPlayer)
        {
            playerDamageSprite(pSource->index, pPlayer, (DAMAGE_TYPE)dmgType, dmg);
        }
        else
        {
            switch (dmgType)
            {
                case kDmgExplode:
                    break;
                case kDmgBurn:
                    if (!IsBurningDude(pSpr) || pXSpr->burnTime <= 0) break;
                    return;
                default:
                    pXSpr->health = 0x1000; // so it wont turn into gib immediately
                    break;
            }

            actKillDude(pSource->index, pSpr, (DAMAGE_TYPE)dmgType, dmg);
        }
    }
}

void useSeqSpawnerGen(XSPRITE* pXSource, int objType, int index) {

    if (pXSource->data2 > 0 && !gSysRes.Lookup(pXSource->data2, "SEQ")) {
        consoleSysMsg("Missing sequence #%d", pXSource->data2);
        return;
    }

    spritetype* pSource = &sprite[pXSource->reference];
    switch (objType) {
        case OBJ_SECTOR:
            if (pXSource->data2 <= 0) {
                if (pXSource->data3 == 3 || pXSource->data3 == 1)
                    seqKill(2, sector[index].extra);
                if (pXSource->data3 == 3 || pXSource->data3 == 2)
                    seqKill(1, sector[index].extra);
            } else {
                if (pXSource->data3 == 3 || pXSource->data3 == 1)
                    seqSpawn(pXSource->data2, 2, sector[index].extra, -1);
                if (pXSource->data3 == 3 || pXSource->data3 == 2)
                    seqSpawn(pXSource->data2, 1, sector[index].extra, -1);
            }
            return;
        case OBJ_WALL:
            if (pXSource->data2 <= 0) {
                if (pXSource->data3 == 3 || pXSource->data3 == 1)
                    seqKill(0, wall[index].extra);
                if ((pXSource->data3 == 3 || pXSource->data3 == 2) && (wall[index].cstat & CSTAT_WALL_MASKED))
                    seqKill(4, wall[index].extra);
            } else {

                if (pXSource->data3 == 3 || pXSource->data3 == 1)
                    seqSpawn(pXSource->data2, 0, wall[index].extra, -1);
                if (pXSource->data3 == 3 || pXSource->data3 == 2) {

                    if (wall[index].nextwall < 0) {
                        if (pXSource->data3 == 3)
                            seqSpawn(pXSource->data2, 0, wall[index].extra, -1);

                    } else {
                        if (!(wall[index].cstat & CSTAT_WALL_MASKED))
                            wall[index].cstat |= CSTAT_WALL_MASKED;

                        seqSpawn(pXSource->data2, 4, wall[index].extra, -1);
                    }
                }

                if (pXSource->data4 > 0) {

                    int cx, cy, cz;
                    cx = (wall[index].x + wall[wall[index].point2].x) >> 1;
                    cy = (wall[index].y + wall[wall[index].point2].y) >> 1;
                    int nSector = sectorofwall(index);
                    int32_t ceilZ, floorZ;
                    getzsofslope(nSector, cx, cy, &ceilZ, &floorZ);
                    int32_t ceilZ2, floorZ2;
                    getzsofslope(wall[index].nextsector, cx, cy, &ceilZ2, &floorZ2);
                    ceilZ = ClipLow(ceilZ, ceilZ2);
                    floorZ = ClipHigh(floorZ, floorZ2);
                    cz = (ceilZ + floorZ) >> 1;

                    sfxPlay3DSound(cx, cy, cz, pXSource->data4, nSector);

                }

            }
            return;
        case OBJ_SPRITE:
            
            if (pXSource->data2 <= 0) seqKill(3, sprite[index].extra);
            else if (sectRangeIsFine(sprite[index].sectnum))
            {
                if (pXSource->data3 > 0)
                {
                    int nSprite = InsertSprite(sprite[index].sectnum, kStatDecoration);
                    int top, bottom; GetSpriteExtents(&sprite[index], &top, &bottom);
                    sprite[nSprite].x = sprite[index].x;
                    sprite[nSprite].y = sprite[index].y;
                    switch (pXSource->data3) {
                        default:
                            sprite[nSprite].z = sprite[index].z;
                            break;
                        case 2:
                            sprite[nSprite].z = bottom;
                            break;
                        case 3:
                            sprite[nSprite].z = top;
                            break;
                        case 4:
                            sprite[nSprite].z = top + ((bottom-top) >> 1);
                            break;
                        case 5:
                        case 6:
                            if (!sectRangeIsFine(sprite[index].sectnum)) sprite[nSprite].z = top;
                            else if (pXSource->data3 == 5) sprite[nSprite].z = getflorzofslope(sprite[nSprite].sectnum, sprite[nSprite].x, sprite[nSprite].y);
                            else sprite[nSprite].z = getceilzofslope(sprite[nSprite].sectnum, sprite[nSprite].x, sprite[nSprite].y);
                            break;
                    }
                        
                    if (nSprite >= 0)
                    {
                        int nXSprite = dbInsertXSprite(nSprite);
                        seqSpawn(pXSource->data2, 3, nXSprite, -1);
                        if (pSource->flags & kModernTypeFlag1)
                        {
                            sprite[nSprite].pal = pSource->pal;
                            sprite[nSprite].shade = pSource->shade;
                            sprite[nSprite].xrepeat = pSource->xrepeat;
                            sprite[nSprite].yrepeat = pSource->yrepeat;
                            sprite[nSprite].xoffset = pSource->xoffset;
                            sprite[nSprite].yoffset = pSource->yoffset;
                        }

                        if (pSource->flags & kModernTypeFlag2)
                        {
                            sprite[nSprite].cstat |= pSource->cstat;
                        }

                        if (pSource->flags & kModernTypeFlag4)
                        {
                            sprite[nSprite].ang = pSource->ang;
                        }

                        // should be: the more is seqs, the shorter is timer
                        evPost(nSprite, OBJ_SPRITE, 1000, kCallbackRemove);
                    }
                } else
                {
                    seqSpawn(pXSource->data2, 3, sprite[index].extra, -1);
                }
                    
                if (pXSource->data4 > 0)
                    sfxPlay3DSound(&sprite[index], pXSource->data4, -1, 0);
            }
            return;
    }
}

void setupViewCamera(VIEWCAMERA* pCam, PLAYER* pPlay, spritetype* pDest, XSPRITE* pXDest)
{
    pCam->sceneQav  = -1;
    pCam->fraggerId = -1;
    pCam->weaponQav = -1;

    pCam->nSprite   = pDest->index;
    pCam->nPlayer   = pPlay->nPlayer;
    pCam->pSprite   = pDest;
    pCam->pXSprite  = pXDest;
    pCam->pDudeInfo = pPlay->pDudeInfo;
}

char isCameraSprite(spritetype* pSpr)
{
    return (gViewCam.active && gViewCam.pSprite == pSpr);
}

void useViewCamera(XSPRITE* pXSrc, spritetype* pCSpr, spritetype* pTSpr, PLAYER* pPlay)
{
    spritetype* pSrc;
    XSPRITE* pXCSpr;

    if (gViewCam.active)
    {
        gView = gMe = pPlay;
        
        if (gViewCam.drawToTile == 0)
        {
            if (gViewSize != gViewCam.viewSize)
                viewResizeView(gViewCam.viewSize);

            gAimReticle = gViewCam.aimReticle;
            gViewPos = (VIEWPOS)gViewCam.viewPos;
        }

        gViewCam.drawToTile = 0;
        gViewCam.nTrackSpr = -1;
        gViewCam.active = 0;
    }

    if (pXSrc && pCSpr && xsprIsFine(pCSpr) && pPlay->pSprite != pCSpr)
    {
        pSrc = &sprite[pXSrc->reference];
        pXCSpr = &xsprite[pCSpr->extra];

        gViewCam.viewSize = gViewSize;
        gViewCam.aimReticle = gAimReticle;
        gViewCam.viewPos = gViewPos;
        gViewCam.drawToTile = (pSrc->flags & 0x8000) != 0;

        if (gViewCam.drawToTile == 0 && gViewMode != 4)
        {
            gView = &gViewCam; gMe = &gMeCam;
            
            if (gViewSize) viewResizeView(0);
            gViewPos = (pSrc->flags & kModernTypeFlag16) ? VIEWPOS_1 : VIEWPOS_0;
            gAimReticle = 0;
        }

        setupViewCamera(&gViewCam, pPlay, pCSpr, pXCSpr);
        setupViewCamera(&gMeCam, pPlay, pCSpr, pXCSpr);

        gViewCam.nCamSrc = pSrc->index;
        gViewCam.nTrackSpr = -1;
        if ((pSrc->flags & kModernTypeFlag1) && pTSpr)
            gViewCam.nTrackSpr = pTSpr->index;

        gViewCam.active = 1;
    }
}

void processViewCamera(VIEWCAMERA* pCam)
{
    PLAYER* pPlay = &gPlayer[pCam->nPlayer];
    spritetype* pCSrc = &sprite[pCam->nCamSrc];
    XSPRITE* pXCSrc = &xsprite[pCSrc->extra];
    spritetype* pCSpr; LOCATION* pLoc;
    int nHeigh;

    pCSpr = pCam->pSprite;

    if (IsCustomDude(pCSpr))        nHeigh = cdudeGet(pCSpr)->eyeHeight;
    else if (IsDudeSprite(pCSpr))   nHeigh = (getDudeInfo(pCSpr->type)->eyeHeight * pCSpr->yrepeat) << 2;
    else                            nHeigh = 0;

    viewBackupSpriteLoc(pCSpr->index, pCSpr); // only if doesn't have backup already
    pLoc = &gPrevSpriteLoc[pCSpr->index]; // use prev loc (for interpolation)

    if (gViewInterpolate)
    {
        pCam->ix = pLoc->x;
        pCam->iy = pLoc->y;
        pCam->iz = pCam->zView;

        pCam->ia = pCam->viewAng;
        pCam->ih = pCam->viewSlope;
        pCam->it = pCam->scrAng;
    }

    pCam->zView         = pLoc->z - nHeigh;
    pCam->viewAng       = pLoc->ang & kAngMask;
    pCam->viewSlope     = pXCSrc->data2;
    pCam->scrAng        = pXCSrc->data3 & kAngMask;
    pCam->isUnderwater  = spriteIsUnderwater(pCSrc);
    pCam->drawToTile    = (pCSrc->flags & 0x8000) != 0;

    if (pCam->nCamSpr != pCSpr->index)
    {
        pCam->nCamSpr = pCSpr->index;
        
        if (gViewInterpolate)
        {
            pCam->iz = pCam->zView;
            pCam->ia = pCam->viewAng;
            pCam->ih = pCam->viewSlope;
            pCam->it = pCam->scrAng;
        }
    }

    if ((pCSrc->flags & kModernTypeFlag1) && pCam->nTrackSpr >= 0)
    {
        // Track this sprite
        spritetype* pTSpr = &sprite[pCam->nTrackSpr];

        if (pTSpr->statnum < kMaxStatus)
        {
            int dx = pTSpr->x - pCSpr->x;
            int dy = pTSpr->y - pCSpr->y;
            int nDist = approxDist(dx, dy);
            int nAng  = getangle(dx, dy);
            int t;

            if ((pCSrc->flags & kModernTypeFlag2) == 0)
                pCam->viewAng = nAng;

            if ((pCSrc->flags & kModernTypeFlag4) == 0)
            {
                pCam->viewSlope = divscale10((pCam->zView - pTSpr->z) >> 8, nDist);
                if (pXCSrc->data2)
                    t = klabs(pXCSrc->data2), pCam->viewSlope = ClipRange(pCam->viewSlope, -t, t);
            }

            if ((pCSrc->flags & kModernTypeFlag8) != 0)
            {
                pCam->scrAng = DANGLE(nAng, pCSpr->ang);
                if (pXCSrc->data3)
                    t = klabs(pXCSrc->data3), pCam->scrAng = ClipRange(pCam->scrAng, -t, t);

                pCam->scrAng = pCam->scrAng & kAngMask;
            }
        }
    }
    
    if ((pCSrc->flags & kModernTypeFlag64) != 0)
    {
        pCam->flashEffect                   = pPlay->flashEffect;
        pCam->flickerEffect                 = pPlay->flickerEffect;
        pCam->visibility                    = pPlay->visibility;
        pCam->painEffect                    = pPlay->painEffect;
        pCam->blindEffect                   = pPlay->blindEffect;
        pCam->chokeEffect                   = pPlay->chokeEffect;
        pCam->tiltEffect                    = pPlay->tiltEffect;
        pCam->pickupEffect                  = pPlay->pickupEffect;
        pCam->pwUpTime[kPwUpDeliriumShroom] = pPlay->pwUpTime[kPwUpDeliriumShroom];
    }

    if (pCam->drawToTile)
    {
        #define kCamTileRes 512

        static char ror_status[kMaxMirrors];
        int nTile = 4080;
        int t, i;

        fix16_t q16Ang   = fix16_from_int(pCam->viewAng);
        fix16_t q16Slope = fix16_from_int(pCam->viewSlope);

        if (tilesiz[nTile].x != kCamTileRes || tilesiz[nTile].y != kCamTileRes)
            tileDelete(nTile), tileCreate(nTile, kCamTileRes, kCamTileRes);

        if (!waloff[nTile])
            tileLoad(nTile);

        if (waloff[nTile])
        {
            int16_t s = pCSpr->sectnum;
            int32_t x = pCSpr->x, y = pCSpr->y, z = pCam->zView;

            renderSetTarget(nTile, kCamTileRes, kCamTileRes);

            if (pCSrc->flags & kModernTypeFlag16)
            {
                GetSpriteExtents(pCSpr, &z, &t);
                nnExtOffsetPos(0, -1024, 0, pCam->viewAng, &x, &y, NULL);
                updatesectorz(x, y, z, &s);
                if (s < 0)
                    s = pCSpr->sectnum;
            }

            t = 16; i = 1;
            while (i >= 0 && t >= 0)
            {
                i = mirrorcnt;
                while (--i >= 0) ror_status[i] = ROR_TestGotPic(i);

                DrawMirrors(x, y, z, q16Ang, q16Slope, gInterpolate, -1);

                yax_preparedrawrooms();
                drawrooms(x, y, z, pCam->viewAng, pCam->viewSlope, s);
                yax_drawrooms(viewProcessSprites, s, 0, gInterpolate);

                i = mirrorcnt;
                while (--i >= 0 && ror_status[i] == ROR_TestGotPic(i));

                viewProcessSprites(x, y, z, pCam->viewAng, gInterpolate);
                renderDrawMasks();
            }

            renderRestoreTarget();
            squarerotatetile(nTile);

            i = nTile; nTile = pCSrc->picnum;
            if (i != nTile)
            {
                if (tilesiz[nTile].x != kCamTileRes || tilesiz[nTile].y != kCamTileRes)
                    tileDelete(nTile), tileCreate(nTile, kCamTileRes, kCamTileRes);

                if (!waloff[nTile])
                    tileLoad(nTile);

                if (waloff[nTile])
                    Bmemcpy((void*)waloff[nTile], (void*)waloff[i], kCamTileRes * kCamTileRes);

#ifdef USE_OPENGL
                if (videoGetRenderMode() != REND_CLASSIC)
                    gltexinvalidatetype(INVALIDATE_ART); // need update after tile copying?
#endif
            }
        }
    }
    else
    {
        if (gViewSize) viewResizeView(0);
        gViewPos = (pCSrc->flags & kModernTypeFlag16) ? VIEWPOS_1 : VIEWPOS_0;
        gAimReticle = 0;
    }
}

bool valueIsBetween(int val, int min, int max) {
    return (val > min && val < max);
}

char modernTypeSetSpriteState(int nSprite, XSPRITE* pXSprite, int nState, int causerID) {
    if ((pXSprite->busy & 0xffff) == 0 && pXSprite->state == nState)
        return 0;

    pXSprite->busy = nState << 16; pXSprite->state = nState;
    
    evKill(nSprite, 3, causerID);
    if (pXSprite->restState != nState && pXSprite->waitTime > 0)
        evPost(nSprite, 3, (pXSprite->waitTime * 120) / 10, pXSprite->restState ? kCmdOn : kCmdOff, causerID);

    if (pXSprite->txID != 0 && ((pXSprite->triggerOn && pXSprite->state) || (pXSprite->triggerOff && !pXSprite->state)))
        modernTypeSendCommand(nSprite, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);

    return 1;
}

void modernTypeSendCommand(int nSprite, int destChannel, COMMAND_ID command, int causerID) {
    switch (command) {
    case kCmdLink:
        evSend(nSprite, 3, destChannel, kCmdModernUse, causerID); // just send command to change properties
        return;
    case kCmdUnlock:
        evSend(nSprite, 3, destChannel, command, causerID); // send normal command first
        evSend(nSprite, 3, destChannel, kCmdModernUse, causerID);  // then send command to change properties
        return;
    default:
        evSend(nSprite, 3, destChannel, kCmdModernUse, causerID); // send first command to change properties
        evSend(nSprite, 3, destChannel, command, causerID); // then send normal command
        return;
    }
}

// this function used by various new modern types.
void modernTypeTrigger(int destObjType, int destObjIndex, EVENT event) {

    if (event.type != OBJ_SPRITE) return;
    spritetype* pSource = &sprite[event.index];

    if (!xspriRangeIsFine(pSource->extra)) return;
    XSPRITE* pXSource = &xsprite[pSource->extra];

    switch (destObjType) {
        case OBJ_SECTOR:
            if (!xsectRangeIsFine(sector[destObjIndex].extra)) return;
            break;
        case OBJ_WALL:
            if (!xwallRangeIsFine(wall[destObjIndex].extra)) return;
            break;
        case OBJ_SPRITE:
            if (!xspriRangeIsFine(sprite[destObjIndex].extra)) return;
            else if (sprite[destObjIndex].flags & kHitagFree) return;

            // allow redirect events received from some modern types.
            // example: it allows to spawn FX effect if event was received from kModernEffectGen
            // on many TX channels instead of just one.
            switch (sprite[destObjIndex].type) {
                case kModernRandomTX:
                case kModernSequentialTX:
                    spritetype* pSpr = &sprite[destObjIndex]; XSPRITE* pXSpr = &xsprite[pSpr->extra];
                    if (pXSpr->command != kCmdLink || pXSpr->locked) break; // no redirect mode detected
                    switch (pSpr->type) {
                        case kModernRandomTX:
                            useRandomTx(pXSpr, (COMMAND_ID)pXSource->command, false, event.causer); // set random TX id
                            break;
                        case kModernSequentialTX:
                            if (pSpr->flags & kModernTypeFlag1) {
                                seqTxSendCmdAll(pXSpr, pSource->index, (COMMAND_ID)pXSource->command, true, event.causer);
                                return;
                            }
                            useSequentialTx(pXSpr, (COMMAND_ID)pXSource->command, false, event.causer); // set next TX id
                            break;
                    }
                    if (pXSpr->txID <= 0 || pXSpr->txID >= kChannelUserMax) return;
                    modernTypeSendCommand(pSource->index, pXSpr->txID, (COMMAND_ID)pXSource->command, event.causer);
                    return;
            }
            break;
        default:
            return;
    }

    switch (pSource->type) {
        case kThingDripBlood:
        case kThingDripWater:
            if (destObjType != OBJ_SPRITE) break;
            useDripGenerator(pXSource, &sprite[destObjIndex]);
            break;
        // spawn gibs with extended settings
        case kThingObjectExplode:
        case kThingObjectGib:
            if (destObjType != OBJ_SPRITE) break;
            useGibObject(pXSource, &sprite[destObjIndex]);
            break;
        // allows teleport any sprite from any location to the source destination
        case kMarkerWarpDest:
            if (destObjType != OBJ_SPRITE) break;
            useTeleportTarget(pXSource, &sprite[destObjIndex]);
            break;
        // changes slope of sprite or sector
        case kModernSlopeChanger:
            switch (destObjType) {
                case OBJ_SPRITE:
                case OBJ_SECTOR:
                    useSlopeChanger(pXSource, destObjType, destObjIndex);
                    break;
            }
            break;
        case kModernSpriteDamager:
        // damages xsprite via TX ID or xsprites in a sector
            switch (destObjType) {
                case OBJ_SPRITE:
                case OBJ_SECTOR:
                    useSpriteDamager(pXSource, destObjType, destObjIndex);
                    break;
            }
            break;
        // can spawn any effect passed in data2 on it's or txID sprite
        case kModernEffectSpawner:
            if (destObjType != OBJ_SPRITE) break;
            useEffectGen(pXSource, &sprite[destObjIndex]);
            break;
        // takes data2 as SEQ ID and spawns it on it's or TX ID object
        case kModernSeqSpawner:
            useSeqSpawnerGen(pXSource, destObjType, destObjIndex);
            break;
        // creates wind on TX ID sector
        case kModernWindGenerator:
            if (destObjType != OBJ_SECTOR || pXSource->data2 < 0) break;
            useSectorWindGen(pXSource, &sector[destObjIndex]);
            break;
        // size and pan changer of sprite/wall/sector via TX ID
        case kModernObjSizeChanger:
            useObjResizer(pXSource, destObjType, destObjIndex);
            break;
        // iterate data filed value of destination object
        case kModernObjDataAccumulator:
            useIncDecGen(pXSource, destObjType, destObjIndex);
            break;
        // change data field value of destination object
        case kModernObjDataChanger:
            useDataChanger(pXSource, destObjType, destObjIndex);
            break;
        // change sector lighting dynamically
        case kModernSectorFXChanger:
            if (destObjType != OBJ_SECTOR) break;
            useSectorLigthChanger(pXSource, &sector[destObjIndex]);
            break;
        // change target of dudes and make it fight
        case kModernDudeTargetChanger:
            if (destObjType != OBJ_SPRITE) break;
            useTargetChanger(pXSource, &sprite[destObjIndex]);
            break;
        // change picture and palette of TX ID object
        case kModernObjPicnumChanger:
            usePictureChanger(pXSource, destObjType, destObjIndex);
            break;
        // change various properties
        case kModernObjPropertiesChanger:
            usePropertiesChanger(pXSource, destObjType, destObjIndex);
            break;
        // change velocity of the sprite
        case kModernVelocityChanger:
            switch (destObjType) {
                case OBJ_SPRITE:
                case OBJ_SECTOR:
                    useVelocityChanger(pXSource, event.causer, destObjType, destObjIndex);
                    break;
            }
            break;
        // updated vanilla sound gen that now allows to play sounds on TX ID sprites
        case kGenModernSound:
            if (destObjType != OBJ_SPRITE) break;
            useSoundGen(pXSource, &sprite[destObjIndex]);
            break;
        // updated ecto skull gen that allows to fire missile from TX ID sprites
        case kGenModernMissileUniversal:
            if (destObjType != OBJ_SPRITE) break;
            useUniMissileGen(pXSource, &sprite[destObjIndex]);
            break;
        // spawn enemies on TX ID sprites
        case kMarkerDudeSpawn:
            if (destObjType != OBJ_SPRITE) break;
            useDudeSpawn(pXSource, &sprite[destObjIndex]);
            break;
         // spawn custom dude on TX ID sprites
        case kModernCustomDudeSpawn:
            if (destObjType != OBJ_SPRITE) break;
            useCustomDudeSpawn(pXSource, &sprite[destObjIndex]);
            break;
    }
}

// the following functions required for kModernDudeTargetChanger
//---------------------------------------
spritetype* aiFightGetTargetInRange(spritetype* pSprite, int minDist, int maxDist, short data, short teamMode) {
    DUDEINFO* pDudeInfo = getDudeInfo(pSprite->type); XSPRITE* pXSprite = &xsprite[pSprite->extra];
    spritetype* pTarget = NULL; XSPRITE* pXTarget = NULL; spritetype* cTarget = NULL;
    for (int nSprite = headspritestat[kStatDude]; nSprite >= 0; nSprite = nextspritestat[nSprite]) {
        pTarget = &sprite[nSprite];  pXTarget = &xsprite[pTarget->extra];
        if (!aiFightDudeCanSeeTarget(pXSprite, pDudeInfo, pTarget)) continue;

        int dist = aiFightGetTargetDist(pSprite, pDudeInfo, pTarget);
        if (dist < minDist || dist > maxDist) continue;
        else if (pXSprite->target == pTarget->index) return pTarget;
        else if (!IsDudeSprite(pTarget) || pTarget->index == pSprite->index || IsPlayerSprite(pTarget)) continue;
        else if (IsBurningDude(pTarget) || !IsKillableDude(pTarget) || pTarget->owner == pSprite->index) continue;
        else if ((teamMode == 1 && aiFightIsMateOf(pXSprite, pXTarget)) || aiFightMatesHaveSameTarget(pXSprite, pTarget, 1)) continue;
        else if (data == 666 || pXTarget->data1 == data) {

            if (pXSprite->target > 0) {
                cTarget = &sprite[pXSprite->target];
                int fineDist1 = aiFightGetFineTargetDist(pSprite, cTarget);
                int fineDist2 = aiFightGetFineTargetDist(pSprite, pTarget);
                if (fineDist1 < fineDist2)
                    continue;
            }
            return pTarget;
        }
    }

    return NULL;
}

spritetype* aiFightTargetIsPlayer(XSPRITE* pXSprite) {

    if (pXSprite->target >= 0) {
        if (IsPlayerSprite(&sprite[pXSprite->target]))
            return &sprite[pXSprite->target];
    }

    return NULL;
}
spritetype* aiFightGetMateTargets(XSPRITE* pXSprite) {
    int rx = pXSprite->rxID; spritetype* pMate = NULL; XSPRITE* pXMate = NULL;

    for (int i = bucketHead[rx]; i < bucketHead[rx + 1]; i++) {
        if (rxBucket[i].type == OBJ_SPRITE) {
            pMate = &sprite[rxBucket[i].index];
            if (pMate->extra < 0 || pMate->index == sprite[pXSprite->reference].index || !IsDudeSprite(pMate))
                continue;

            pXMate = &xsprite[pMate->extra];
            if (pXMate->target > -1) {
                if (!IsPlayerSprite(&sprite[pXMate->target]))
                    return &sprite[pXMate->target];
            }

        }
    }

    return NULL;
}

bool aiFightMatesHaveSameTarget(XSPRITE* pXLeader, spritetype* pTarget, int allow) {
    int rx = pXLeader->rxID; spritetype* pMate = NULL; XSPRITE* pXMate = NULL;

    for (int i = bucketHead[rx]; i < bucketHead[rx + 1]; i++) {

        if (rxBucket[i].type != OBJ_SPRITE)
            continue;

        pMate = &sprite[rxBucket[i].index];
        if (pMate->extra < 0 || pMate->index == sprite[pXLeader->reference].index || !IsDudeSprite(pMate))
            continue;

        pXMate = &xsprite[pMate->extra];
        if (pXMate->target == pTarget->index && allow-- <= 0)
            return true;
    }

    return false;

}

bool aiFightDudeCanSeeTarget(XSPRITE* pXDude, DUDEINFO* pDudeInfo, spritetype* pTarget) {
    spritetype* pDude = &sprite[pXDude->reference];
    int dx = pTarget->x - pDude->x; int dy = pTarget->y - pDude->y;

    // check target
    if (approxDist(dx, dy) < pDudeInfo->seeDist) {
        int eyeAboveZ = pDudeInfo->eyeHeight * pDude->yrepeat << 2;

        // is there a line of sight to the target?
        if (cansee(pDude->x, pDude->y, pDude->z, pDude->sectnum, pTarget->x, pTarget->y, pTarget->z - eyeAboveZ, pTarget->sectnum)) {
            /*int nAngle = getangle(dx, dy);
            int losAngle = ((1024 + nAngle - pDude->ang) & 2047) - 1024;

            // is the target visible?
            if (klabs(losAngle) < 2048) // 360 deg periphery here*/
            return true;
        }

    }

    return false;

}

// this function required if monsters in genIdle ai state. It wakes up monsters
// when kModernDudeTargetChanger goes to off state, so they won't ignore the world.
void aiFightActivateDudes(int rx) {
    for (int i = bucketHead[rx]; i < bucketHead[rx + 1]; i++) {
        if (rxBucket[i].type != OBJ_SPRITE) continue;
        spritetype* pDude = &sprite[rxBucket[i].index]; XSPRITE* pXDude = &xsprite[pDude->extra];
        if (!IsDudeSprite(pDude) || pXDude->aiState->stateType != kAiStateGenIdle) continue;
        aiInitSprite(pDude);
    }
}


// this function sets target to -1 for all dudes that hunting for nSprite
void aiFightFreeTargets(int nSprite) {
    for (int nTarget = headspritestat[kStatDude]; nTarget >= 0; nTarget = nextspritestat[nTarget]) {
        if (!IsDudeSprite(&sprite[nTarget]) || sprite[nTarget].extra < 0) continue;
        else if (xsprite[sprite[nTarget].extra].target == nSprite)
            aiSetTarget(&xsprite[sprite[nTarget].extra], sprite[nTarget].x, sprite[nTarget].y, sprite[nTarget].z);
    }

    return;
}

// this function sets target to -1 for all targets that hunting for dudes affected by selected kModernDudeTargetChanger
void aiFightFreeAllTargets(XSPRITE* pXSource) {
    if (pXSource->txID <= 0) return;
    for (int i = bucketHead[pXSource->txID]; i < bucketHead[pXSource->txID + 1]; i++) {
        if (rxBucket[i].type == OBJ_SPRITE && sprite[rxBucket[i].index].extra >= 0)
            aiFightFreeTargets(rxBucket[i].index);
    }

    return;
}


bool aiFightDudeIsAffected(XSPRITE* pXDude) {
    if (pXDude->rxID <= 0 || pXDude->locked == 1) return false;
    for (int nSprite = headspritestat[kStatModernDudeTargetChanger]; nSprite >= 0; nSprite = nextspritestat[nSprite]) {
        XSPRITE* pXSprite = (sprite[nSprite].extra >= 0) ? &xsprite[sprite[nSprite].extra] : NULL;
        if (pXSprite == NULL || pXSprite->txID <= 0 || pXSprite->state != 1) continue;
        for (int i = bucketHead[pXSprite->txID]; i < bucketHead[pXSprite->txID + 1]; i++) {
            if (rxBucket[i].type != OBJ_SPRITE) continue;

            spritetype* pSprite = &sprite[rxBucket[i].index];
            if (pSprite->extra < 0 || !IsDudeSprite(pSprite)) continue;
            else if (pSprite->index == sprite[pXDude->reference].index) return true;
        }
    }
    return false;
}

bool aiFightIsMateOf(XSPRITE* pXDude, XSPRITE* pXSprite) {
    return (pXDude->rxID == pXSprite->rxID);
}

// this function tells if there any dude found for kModernDudeTargetChanger
bool aiFightGetDudesForBattle(XSPRITE* pXSprite) {
    
    for (int i = bucketHead[pXSprite->txID]; i < bucketHead[pXSprite->txID + 1]; i++) {
        if (rxBucket[i].type != OBJ_SPRITE) continue;
        else if (IsDudeSprite(&sprite[rxBucket[i].index]) &&
            xsprite[sprite[rxBucket[i].index].extra].health > 0) return true;
    }

    // check redirected TX buckets
    int rx = -1; XSPRITE* pXRedir = NULL;
    while ((pXRedir = evrListRedirectors(OBJ_SPRITE, sprite[pXSprite->reference].extra, pXRedir, &rx)) != NULL) {
        for (int i = bucketHead[rx]; i < bucketHead[rx + 1]; i++) {
            if (rxBucket[i].type != OBJ_SPRITE) continue;
            else if (IsDudeSprite(&sprite[rxBucket[i].index]) &&
                xsprite[sprite[rxBucket[i].index].extra].health > 0) return true;
        }
    }
    return false;
}

void aiFightAlarmDudesInSight(spritetype* pSprite, int max) {
    spritetype* pDude = NULL; XSPRITE* pXDude = NULL;
    XSPRITE* pXSprite = &xsprite[pSprite->extra];
    DUDEINFO* pDudeInfo = getDudeInfo(pSprite->type);
    for (int nSprite = headspritestat[kStatDude]; nSprite >= 0; nSprite = nextspritestat[nSprite]) {
        pDude = &sprite[nSprite];
        if (pDude->index == pSprite->index || !IsDudeSprite(pDude) || pDude->extra < 0)
            continue;
        pXDude = &xsprite[pDude->extra];
        if (aiFightDudeCanSeeTarget(pXSprite, pDudeInfo, pDude)) {
            if (pXDude->target != -1 || pXDude->rxID > 0)
                continue;

            aiSetTarget(pXDude, pDude->x, pDude->y, pDude->z);
            aiActivateDude(pDude, pXDude);
            if (max-- < 1)
                break;
        }
    }
}

bool aiFightUnitCanFly(spritetype* pDude)
{
    return (IsDudeSprite(pDude) && gDudeInfoExtra[pDude->type - kDudeBase].flying);
}

bool aiFightIsMeleeUnit(spritetype* pDude)
{
    return (IsDudeSprite(pDude) && gDudeInfoExtra[pDude->type - kDudeBase].melee);
}

int aiFightGetTargetDist(spritetype* pSprite, DUDEINFO* pDudeInfo, spritetype* pTarget) {
    int x = pTarget->x; int y = pTarget->y;
    int dx = x - pSprite->x; int dy = y - pSprite->y;

    int dist = approxDist(dx, dy);
    if (dist <= pDudeInfo->meleeDist) return 0;
    if (dist >= pDudeInfo->seeDist) return 13;
    if (dist <= pDudeInfo->seeDist / 12) return 1;
    if (dist <= pDudeInfo->seeDist / 11) return 2;
    if (dist <= pDudeInfo->seeDist / 10) return 3;
    if (dist <= pDudeInfo->seeDist / 9) return 4;
    if (dist <= pDudeInfo->seeDist / 8) return 5;
    if (dist <= pDudeInfo->seeDist / 7) return 6;
    if (dist <= pDudeInfo->seeDist / 6) return 7;
    if (dist <= pDudeInfo->seeDist / 5) return 8;
    if (dist <= pDudeInfo->seeDist / 4) return 9;
    if (dist <= pDudeInfo->seeDist / 3) return 10;
    if (dist <= pDudeInfo->seeDist / 2) return 11;
    return 12;
}

int aiFightGetFineTargetDist(spritetype* pSprite, spritetype* pTarget) {
    int x = pTarget->x; int y = pTarget->y;
    int dx = x - pSprite->x; int dy = y - pSprite->y;

    int dist = approxDist(dx, dy);
    return dist;
}

int sectorInMotion(int nSector) {
 
    for (int i = 0; i < kMaxBusyCount; i++) {
        if (gBusy->at0 == nSector) return i;
    }

    return -1;
}

void sectorKillSounds(int nSector) {
    for (int nSprite = headspritesect[nSector]; nSprite >= 0; nSprite = nextspritesect[nSprite]) {
        if (sprite[nSprite].type != kSoundSector) continue;
        sfxKill3DSound(&sprite[nSprite]);
    }
}

void sectorPauseMotion(int nSector, int causerID) {

    if (!xsectRangeIsFine(sector[nSector].extra)) return;
    XSECTOR* pXSector = &xsector[sector[nSector].extra];
    pXSector->unused1 = 1;
    
    evKill(nSector, OBJ_SECTOR, causerID);

    sectorKillSounds(nSector);
    if ((pXSector->busy == 0 && !pXSector->state) || (pXSector->busy == 65536 && pXSector->state))
        SectorEndSound(nSector, xsector[sector[nSector].extra].state);
    
    return;
}

void sectorContinueMotion(int nSector, EVENT event) {
    
    if (!xsectRangeIsFine(sector[nSector].extra)) return;
    else if (gBusyCount >= kMaxBusyCount) {
        consoleSysMsg("Failed to continue motion for sector #%d. Max (%d) busy objects count reached!", nSector, kMaxBusyCount);
        return;
    }

    XSECTOR* pXSector = &xsector[sector[nSector].extra];
    pXSector->unused1 = 0;
    
    int busyTimeA = pXSector->busyTimeA;    int waitTimeA = pXSector->waitTimeA;
    int busyTimeB = pXSector->busyTimeB;    int waitTimeB = pXSector->waitTimeB;
    switch(sector[nSector].type)
    {
        case kSectorPath:
        case kModernSectorPathSprite:
            if (!spriRangeIsFine(pXSector->marker0)) return;
            busyTimeA = busyTimeB = xsprite[sprite[pXSector->marker0].extra].busyTime;
            waitTimeA = waitTimeB = xsprite[sprite[pXSector->marker0].extra].waitTime;
            break;
    }
    
    if (!pXSector->interruptable && event.cmd != kCmdSectorMotionContinue
        && ((!pXSector->state && pXSector->busy) || (pXSector->state && pXSector->busy != 65536))) {
            
            event.cmd = kCmdSectorMotionContinue;

    } else if (event.cmd == kCmdToggle) {
        
        event.cmd = (pXSector->state) ? kCmdOn : kCmdOff;

    }

    //viewSetSystemMessage("%d / %d", pXSector->busy, pXSector->state);

    int nDelta = 1;
    switch (event.cmd) {
        case kCmdOff:
            if (pXSector->busy == 0) {
                if (pXSector->reTriggerB && waitTimeB) evPost(nSector, OBJ_SECTOR, (waitTimeB * 120) / 10, kCmdOff, event.causer);
                return;
            }
            pXSector->state = 1;
            nDelta = 65536 / ClipLow((busyTimeB * 120) / 10, 1);
            break;
        case kCmdOn:
            if (pXSector->busy == 65536) {
                if (pXSector->reTriggerA && waitTimeA) evPost(nSector, OBJ_SECTOR, (waitTimeA * 120) / 10, kCmdOn, event.causer);
                return;
            }
            pXSector->state = 0;
            nDelta = 65536 / ClipLow((busyTimeA * 120) / 10, 1);
            break;
        case kCmdSectorMotionContinue:
            nDelta = 65536 / ClipLow((((pXSector->state) ? busyTimeB : busyTimeA) * 120) / 10, 1);
            break;
    }

    //bool crush = pXSector->Crush;
    int busyFunc = BUSYID_0;
    switch (sector[nSector].type) {
        case kSectorZMotion:
            busyFunc = BUSYID_2;
            break;
        case kSectorZMotionSprite:
            busyFunc = BUSYID_1;
            break;
        case kSectorSlideMarked:
        case kSectorSlide:
            busyFunc = BUSYID_3;
            break;
        case kSectorRotateMarked:
        case kSectorRotate:
            busyFunc = BUSYID_4;
            break;
        case kSectorRotateStep:
            busyFunc = BUSYID_5;
            break;
        case kSectorPath:
            busyFunc = BUSYID_7;
            break;
        case kModernSectorPathSprite:
            busyFunc = BUSY_PATHSPRITE;
            break;
        default:
            ThrowError("Unsupported sector type %d", sector[nSector].type);
            break;
    }

    SectorStartSound(nSector, pXSector->state);
    nDelta = (pXSector->state) ? -nDelta : nDelta;
    gBusy[gBusyCount].at0 = nSector;
    gBusy[gBusyCount].at4 = nDelta;
    gBusy[gBusyCount].at8 = pXSector->busy;
    gBusy[gBusyCount].atc = (BUSYID)busyFunc;
    gBusyCount++;
    return;

}

bool modernTypeOperateSector(int nSector, sectortype* pSector, XSECTOR* pXSector, EVENT event) {

    if (event.cmd >= kCmdLock && event.cmd <= kCmdToggleLock) {
        
        switch (event.cmd) {
            case kCmdLock:
                pXSector->locked = 1;
                break;
            case kCmdUnlock:
                pXSector->locked = 0;
                break;
            case kCmdToggleLock:
                pXSector->locked = pXSector->locked ^ 1;
                break;
        }

        switch (pSector->type) {
            case kSectorCounter:
                if (pXSector->locked != 1) break;
                SetSectorState(nSector, pXSector, 0, event.causer);
                evPost(nSector, 6, 0, kCallbackCounterCheck);
                break;
        }

        return true;
    
    // continue motion of the paused sector
    } else if (pXSector->unused1) {
        
        switch (event.cmd) {
            case kCmdOff:
            case kCmdOn:
            case kCmdToggle:
            case kCmdSectorMotionContinue:
                sectorContinueMotion(nSector, event);
                return true;
        }
    
    // pause motion of the sector
    } else if (event.cmd == kCmdSectorMotionPause) {
        
        sectorPauseMotion(nSector, event.causer);
        return true;

    }

    switch (pSector->type)
    {
		case kModernSectorPathSprite:
			pathSpriteOperate(nSector, pXSector, event);
			return true;
    }

    return false;

}

PLAYER* helperOperateCheckPlayer(XSPRITE* pXSpr, EVENT* pEvn, int* nCompare)
{
    int nID; char anyPlayer = 0;
    PLAYER* pPlay;

    if ((pXSpr->txID == kChannelEventCauser || pXSpr->data1 == 0) && pEvn->causer < kCauserGame && IsPlayerSprite(&sprite[pEvn->causer]))
        nID = sprite[pEvn->causer].type;
    else if (irngok(pXSpr->txID, kChannelPlayer0, kChannelPlayer7))
        nID = pXSpr->txID - kChannelPlayer0 + 1;
    else if (pXSpr->data1 > 0)      nID = pXSpr->data1;
    else if (gViewCam.active)       nID = gViewCam.nPlayer+1, anyPlayer = 1;
    else                            nID = gMe->nPlayer+1, anyPlayer = 1;

    pPlay = getPlayerById(nID);

    if (nCompare)
    {
        if (anyPlayer)
            *nCompare = kMaxPlayers;
        else if (pPlay && (pPlay == gMe || (gViewCam.active && gViewCam.nPlayer == pPlay->nPlayer)))
            *nCompare = pPlay->nPlayer;
        else
            *nCompare = -1;
    }

    return pPlay;
}

bool modernTypeOperateSprite(int nSprite, spritetype* pSprite, XSPRITE* pXSprite, EVENT event) {

    int causerID = event.causer;

    if (event.cmd >= kCmdLock && event.cmd <= kCmdToggleLock) {
        switch (event.cmd) {
            case kCmdLock:
                pXSprite->locked = 1;
                break;
            case kCmdUnlock:
                pXSprite->locked = 0;
                break;
            case kCmdToggleLock:
                pXSprite->locked = pXSprite->locked ^ 1;
                break;
        }

        switch (pSprite->type) {
            case kModernCondition:
            case kModernConditionFalse:
            #ifdef CONDITIONS_USE_BUBBLE_ACTION
                switch (event.cmd)
                {
                    case kCmdLock:
                    case kCmdUnlock:
                        conditionsBubble(pXSprite, conditionsSetIsLocked, pXSprite->locked); // same action for whole branch
                        break;
                    default:
                        conditionsSetIsLocked(pXSprite, pXSprite->locked);
                        break;
                }
            #else
                conditionsSetIsLocked(pXSprite, pXSprite->locked);
            #endif
                break;
            case kModernEffectSpawner:
                if (!pXSprite->locked) break;
                killEffectGenCallbacks(pXSprite);
                break;
        }
       
        return true;
    }
    else if (event.cmd == kCmdDudeFlagsSet)
    {
        if (event.type != OBJ_SPRITE)
        {
            viewSetSystemMessage("Only sprites could use command #%d", event.cmd);
            return true;
        }
        else if (xspriRangeIsFine(sprite[event.index].extra))
        {
            spritetype* pSrc = &sprite[event.index];
            XSPRITE* pXSrc = &xsprite[pSrc->extra];
            
            // copy dude flags from the source to destination sprite
            aiPatrolFlagsMgr(pSrc, pXSrc, pSprite, pXSprite, true, false);
        }
    }

    if (pSprite->statnum == kStatDude && IsDudeSprite(pSprite)) {

        switch (event.cmd) {
            case kCmdOff:
                if (pXSprite->state) SetSpriteState(nSprite, pXSprite, 0, causerID);
                break;
            case kCmdOn:
                if (!pXSprite->state) SetSpriteState(nSprite, pXSprite, 1, causerID);
                if (!IsDudeSprite(pSprite) || IsPlayerSprite(pSprite) || pXSprite->health <= 0) break;
                else if (pXSprite->aiState->stateType >= kAiStatePatrolBase && pXSprite->aiState->stateType < kAiStatePatrolMax)
                    break;

                
                switch (pXSprite->aiState->stateType) {
                    case kAiStateIdle:
                    case kAiStateGenIdle:
                        aiActivateDude(pSprite, pXSprite);
                        break;
                }
                break;
            case kCmdDudeFlagsSet:
                if (!dudeIsAlive(pSprite)) break;
                else aiPatrolFlagsMgr(&sprite[event.index], &xsprite[sprite[event.index].extra], pSprite, pXSprite, false, true); // initialize patrol dude with possible new flags
                break;
            default:
                if (!pXSprite->state) evPost(nSprite, OBJ_SPRITE, 0, kCmdOn, causerID);
                else evPost(nSprite, OBJ_SPRITE, 0, kCmdOff, causerID);
                break;
        }

        return true;
    }

    switch (pSprite->type) {
        default:
            return false; // no modern type found to work with, go normal OperateSprite();
        case kThingBloodBits:
        case kThingBloodChunks:
            // dude to thing morphing causing a lot of problems since it continues receiving commands after dude is dead.
            // this leads to weird stuff like exploding with gargoyle gib or corpse disappearing immediately.
            // let's allow only specific commands here to avoid this.
            if (pSprite->inittype < kDudeBase || pSprite->inittype >= kDudeMax) return false;
            else if (event.cmd != kCmdToggle && event.cmd != kCmdOff && event.cmd != kCmdSpriteImpact) return true;
            DudeToGibCallback1(nSprite, pSprite->extra); // set proper gib type just in case DATAs was changed from the outside.
            return false;
        case kThingObjectGib:
        case kThingObjectExplode:
            switch (event.cmd)
            {
                case kCmdOff:
                case kCmdOn:
                    if (!SetSpriteState(nSprite, pXSprite, event.cmd, causerID)) return true;
                    break;
                default:
                    if (!SetSpriteState(nSprite, pXSprite, pXSprite->state ^ 1, causerID)) return true;
                    break;
            }
            if (!(pXSprite->sysData1 & kModernTypeFlag128)) useGibObject(pXSprite, pSprite);
            else if (pXSprite->txID) modernTypeSetSpriteState(nSprite, pXSprite, pXSprite->state ^ 1, causerID);
            return true;
		case kModernLaserGen:
            switch (event.cmd)
			{
                case kCmdOff:
					if (pXSprite->state == 1) laserGet(pSprite)->SetState(0);
                    break;
                case kCmdOn:
                    if (pXSprite->state == 0) laserGet(pSprite)->SetState(1);
                    break;
                default:
					laserGet(pSprite)->SetState(pXSprite->state ^ 1);
                    break;
            }
			
			if (pXSprite->state && pXSprite->waitTime)
			{
				evKill(nSprite, OBJ_SPRITE);
				evPost(nSprite, OBJ_SPRITE, EVTIME2TICKS(pXSprite->waitTime), kCmdOff, event.causer);
			}
			return true;
        case kModernCondition:
        case kModernConditionFalse:
            if (!pXSprite->isTriggered) useCondition(pSprite, pXSprite, &event);
            return true;
        // add spawn random dude feature - works only if at least 2 data fields are not empty.
        case kMarkerDudeSpawn:
            if (!gGameOptions.nMonsterSettings) return true;
            else if (!(pSprite->flags & kModernTypeFlag4)) useDudeSpawn(pXSprite, pSprite);
            else if (pXSprite->txID) evSend(nSprite, OBJ_SPRITE, pXSprite->txID, kCmdModernUse, causerID);
            return true;
        case kModernCustomDudeSpawn:
            if (!gGameOptions.nMonsterSettings) return true;
            else if (!(pSprite->flags & kModernTypeFlag4)) useCustomDudeSpawn(pXSprite, pSprite);
            else if (pXSprite->txID) evSend(nSprite, OBJ_SPRITE, pXSprite->txID, kCmdModernUse, causerID);
            return true;
        case kModernRandomTX: // random Event Switch takes random data field and uses it as TX ID
        case kModernSequentialTX: // sequential Switch takes values from data fields starting from data1 and uses it as TX ID
            if (pXSprite->command == kCmdLink) return true; // work as event redirector
            switch (pSprite->type) {
                case kModernRandomTX:
                    useRandomTx(pXSprite, (COMMAND_ID)pXSprite->command, true, event.causer);
                    break;
                case kModernSequentialTX:
                    if (!(pSprite->flags & kModernTypeFlag1)) useSequentialTx(pXSprite, (COMMAND_ID)pXSprite->command, true, causerID);
                    else seqTxSendCmdAll(pXSprite, pSprite->index, (COMMAND_ID)pXSprite->command, false, causerID);
                    break;
            }
            return true;
        case kModernSpriteDamager:
            switch (event.cmd) {
                case kCmdOff:
                    SetSpriteState(nSprite, pXSprite, 0, causerID);
                    break;
                case kCmdOn:
                    SetSpriteState(nSprite, pXSprite, 1, causerID);
                    fallthrough__;
                case kCmdRepeat:
                    if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                    else if (pXSprite->txID > 0) modernTypeSendCommand(nSprite, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                    else if (pXSprite->data1 == 0 && sectRangeIsFine(pSprite->sectnum)) useSpriteDamager(pXSprite, OBJ_SECTOR, pSprite->sectnum);
                    else if (pXSprite->data1 >= 666 && pXSprite->data1 < 669) useSpriteDamager(pXSprite, -1, -1);
                    else
                    {
                        PLAYER* pPlayer = getPlayerById(pXSprite->data1);
                        if (pPlayer != NULL)
                            useSpriteDamager(pXSprite, OBJ_SPRITE, pPlayer->pSprite->index);
                    }

                    if (pXSprite->busyTime)
                        evPost(nSprite, 3, pXSprite->busyTime, kCmdRepeat, causerID);
                    break;
                default:
                    evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                    break;
            }
            return true;
        case kMarkerWarpDest:
            if (pXSprite->txID <= 0)
            {
                PLAYER* pPlayer = getPlayerById(pXSprite->data1);
                if (pPlayer != NULL && SetSpriteState(nSprite, pXSprite, pXSprite->state ^ 1, causerID) == 1)
                    useTeleportTarget(pXSprite, pPlayer->pSprite);
                return true;
            }
            fallthrough__;
        case kModernObjPropertiesChanger:
            if (pXSprite->txID <= 0)
            {
                if (SetSpriteState(nSprite, pXSprite, pXSprite->state ^ 1, causerID) == 1)
                    usePropertiesChanger(pXSprite, -1, -1);
                return true;
            }
            fallthrough__;
        case kModernSectorFXChanger:
            if (pXSprite->txID <= 0)
            {
                if (SetSpriteState(nSprite, pXSprite, pXSprite->state ^ 1, causerID) == 1)
                    useSectorLigthChanger(pXSprite, NULL);
                return true;
            }
            fallthrough__;
        case kModernSlopeChanger:
        case kModernObjSizeChanger:
        case kModernObjPicnumChanger:
        case kModernObjDataChanger:
            modernTypeSetSpriteState(nSprite, pXSprite, pXSprite->state ^ 1, causerID);
            return true;
        case kModernSeqSpawner:
        case kModernEffectSpawner:
            switch (event.cmd) {
            case kCmdOff:
                if (pXSprite->state == 1)
                {
                    SetSpriteState(nSprite, pXSprite, 0, causerID);
                    if (pSprite->type == kModernEffectSpawner)
                        killEffectGenCallbacks(pXSprite);
                }
                break;
            case kCmdOn:
                evKill(nSprite, OBJ_SPRITE, causerID);
                SetSpriteState(nSprite, pXSprite, 1, causerID);
                if (pSprite->type == kModernSeqSpawner && pXSprite->txID >= kChannelUser
                    && (pSprite->flags & kModernTypeFlag16) == 0) seqSpawnerOffSameTx(pXSprite);
                fallthrough__;
            case kCmdRepeat:
                if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                else if (pXSprite->txID > 0) modernTypeSendCommand(nSprite, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                else if (pSprite->type == kModernSeqSpawner) useSeqSpawnerGen(pXSprite, 3, pSprite->index);
                else useEffectGen(pXSprite, NULL);

                if (pXSprite->busyTime > 0)
                    evPost(nSprite, 3, ClipLow((int(pXSprite->busyTime) + Random2(pXSprite->data1)) * 120 / 10, 0), kCmdRepeat, causerID);
                break;
            default:
                evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                break;
            }
            return true;
        case kModernWindGenerator:
            switch (event.cmd) {
                case kCmdOff:
                    SetSpriteState(nSprite, pXSprite, 0, causerID);
                    windGenStopWindOnSectors(pXSprite);
                    break;
                case kCmdOn:
                    evKill(nSprite, OBJ_SPRITE);
                    SetSpriteState(nSprite, pXSprite, 1, causerID);
                    fallthrough__;
                case kCmdRepeat:
                    if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                    else if (pXSprite->txID > 0) modernTypeSendCommand(nSprite, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                    else useSectorWindGen(pXSprite, NULL);

                    if (pXSprite->busyTime)
                        evPost(nSprite, 3, pXSprite->busyTime, kCmdRepeat, causerID);
                    break;
                default:
                    evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                    break;
            }
            return true;
        case kModernVelocityChanger:
            switch (event.cmd) {
                case kCmdOff:
                    SetSpriteState(nSprite, pXSprite, 0, causerID);
                    break;
                case kCmdOn:
                    evKill(nSprite, OBJ_SPRITE, causerID);
                    SetSpriteState(nSprite, pXSprite, 1, causerID);
                    fallthrough__;
                case kCmdRepeat:
                    if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                    else if (pXSprite->txID > 0) modernTypeSendCommand(nSprite, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                    else useVelocityChanger(pXSprite, causerID, OBJ_SECTOR, pSprite->sectnum);
                    if (pXSprite->busyTime)
                        evPost(nSprite, 3, pXSprite->busyTime, kCmdRepeat, causerID);
                    break;
                default:
                    evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                    break;
            }
            return true;
        case kModernDudeTargetChanger:

            // this one is required if data4 of generator was dynamically changed
            // it turns monsters in normal idle state instead of genIdle, so they not ignore the world.
            if (pXSprite->dropMsg == 3 && 3 != pXSprite->data4)
                aiFightActivateDudes(pXSprite->txID);

            switch (event.cmd) {
                case kCmdOff:
                    if (pXSprite->data4 == 3) aiFightActivateDudes(pXSprite->txID);
                    SetSpriteState(nSprite, pXSprite, 0, causerID);
                    break;
                case kCmdOn:
                    evKill(nSprite, OBJ_SPRITE);
                    SetSpriteState(nSprite, pXSprite, 1, causerID);
                    fallthrough__;
                case kCmdRepeat:
                    if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                    else if (pXSprite->txID <= 0 || !aiFightGetDudesForBattle(pXSprite))
                    {
                        aiFightFreeAllTargets(pXSprite);
                        evPost(nSprite, 3, 0, kCmdOff, causerID);
                        break;
                    }
                    else
                    {
                        modernTypeSendCommand(nSprite, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                    }

                    if (pXSprite->busyTime)
                        evPost(nSprite, 3, pXSprite->busyTime, kCmdRepeat, causerID);
                    break;
                default:
                    evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                    break;
            }
            pXSprite->dropMsg = pXSprite->data4;
            return true;
        case kGenDripWater:
        case kGenDripBlood:
            switch (event.cmd) {
                case kCmdOff:
                    SetSpriteState(nSprite, pXSprite, 0, causerID);
                    break;
                case kCmdOn:
                    evKill(nSprite, OBJ_SPRITE, causerID);
                    SetSpriteState(nSprite, pXSprite, 1, causerID);
                    fallthrough__;
                case kCmdRepeat:
                    if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                    else if (pXSprite->txID) modernTypeSendCommand(nSprite, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                    else useDripGenerator(pXSprite, pSprite);
                    if (pXSprite->busyTime)
                        evPost(nSprite, 3, 120 * (Random2(pXSprite->data1) + pXSprite->busyTime) / 10, kCmdRepeat, causerID);
                    break;
                default:
                    evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                    break;
            }
            return true;
        case kGenMissileFireball:
            // true  = do not repeat if disabled (set as operated here)
            // false = set as NOT operated here (opertate in vanilla code)
            return (event.cmd == kCmdRepeat && !pXSprite->state);
        case kGenTrigger:
            if (!(pSprite->flags & kModernTypeFlag1))
            {
                // true  = do not repeat if disabled (set as operated here)
                // false = set as NOT operated here (opertate in vanilla code)
                return (event.cmd == kCmdRepeat && !pXSprite->state);
            }
            switch (event.cmd) { // work as fast generator
                case kCmdOff:
                    SetSpriteState(nSprite, pXSprite, 0, causerID);
                    break;
                case kCmdOn:
                    evKill(nSprite, OBJ_SPRITE, causerID);
                    SetSpriteState(nSprite, pXSprite, 1, causerID);
                    fallthrough__;
                case kCmdRepeat:
                    if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                    else if (pXSprite->txID) evSend(nSprite, 3, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                    if (pXSprite->busyTime)
                        evPost(nSprite, 3, pXSprite->busyTime, kCmdRepeat, causerID);
                    break;
                default:
                    evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                    break;
            }
            return true;
        case kModernObjDataAccumulator:
            switch (event.cmd) {
                case kCmdOff:
                    SetSpriteState(nSprite, pXSprite, 0, causerID);
                    break;
                case kCmdOn:
                    evKill(nSprite, OBJ_SPRITE, causerID);
                    SetSpriteState(nSprite, pXSprite, 1, causerID);
                    fallthrough__;
                case kCmdRepeat:
                    if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                    else if (pSprite->flags == kModernTypeFlag0 && incDecGoalValueIsReached(pXSprite))
                    {
                        // force OFF after *all* TX objects reach the goal value
                        evPost(nSprite, 3, 0, kCmdOff, causerID);
                        break;
                    }
                    
                    modernTypeSendCommand(nSprite, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                    if (pXSprite->busyTime)
                        evPost(nSprite, 3, pXSprite->busyTime, kCmdRepeat, causerID);
                    break;
                default:
                    evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                    break;
            }
            return true;
        case kModernRandom:
        case kModernRandom2:
            switch (event.cmd) {
                case kCmdOff:
                    SetSpriteState(nSprite, pXSprite, 0, causerID);
                    break;
                case kCmdOn:
                    evKill(nSprite, OBJ_SPRITE, causerID);
                    SetSpriteState(nSprite, pXSprite, 1, causerID);
                    fallthrough__;
                case kCmdRepeat:
                    if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                    else useRandomItemGen(pSprite, pXSprite);
                    if (pXSprite->busyTime)
                        evPost(nSprite, 3, (120 * pXSprite->busyTime) / 10, kCmdRepeat, causerID);
                    break;
                default:
                    evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                    break;
            }
            return true;
        case kModernThingTNTProx:
            if (pSprite->statnum != kStatRespawn) {
                switch (event.cmd) {
                case kCmdSpriteProximity:
                    if (pXSprite->state) break;
                    sfxPlay3DSound(pSprite, 452, 0, 0);
                    evPost(nSprite, 3, 30, kCmdOff, causerID);
                    pXSprite->state = 1;
                    fallthrough__;
                case kCmdOn:
                    sfxPlay3DSound(pSprite, 451, 0, 0);
                    pXSprite->Proximity = 1;
                    break;
                default:
                    actExplodeSprite(pSprite);
                    break;
                }
            }
            return true;
        case kModernThingEnemyLifeLeech:
            cdudeLeechOperate(pSprite, pXSprite);
            return true;
        case kModernPlayerControl:
        {
            PLAYER *pPlayer;
            int nCompare, nCmd, i;

            nCmd = ((event.cmd >= kCmdNumberic) ? event.cmd : pXSprite->command) - kCmdNumberic;
            if (nCmd < 0)
                return true;

            if ((pPlayer = helperOperateCheckPlayer(pXSprite, &event, &nCompare)) == NULL)
                return true;

            if (!irngok(nCmd, 3, 4) && nCmd != 20
                && !modernTypeSetSpriteState(nSprite, pXSprite, pXSprite->state ^ 1, causerID))
                    return true;

            if (pPlayer->pXSprite->health <= 0 && !irngok(nCmd, 19, 20))
                return true;

            TRPLAYERCTRL* pCtrl = &gPlayerCtrl[pPlayer->nPlayer];

            switch (nCmd)
            {
                case 0: // 64 (player life form)
                    if (pXSprite->data2 < kModeHuman || pXSprite->data2 > kModeHumanGrown) break;
                    else trPlayerCtrlSetRace(pXSprite, pPlayer);
                    break;
                case 1: // 65 (move speed and jump height)
                    // player movement speed (for all races and postures)
                    if (valueIsBetween(pXSprite->data2, -1, 32767))
                        trPlayerCtrlSetMoveSpeed(pXSprite, pPlayer);

                    // player jump height (for all races and stand posture only)
                    if (valueIsBetween(pXSprite->data3, -1, 32767))
                        trPlayerCtrlSetJumpHeight(pXSprite, pPlayer);
                    break;
                case 2: // 66 (player screen effects)
                    if (pXSprite->data3 < 0) break;
                    else trPlayerCtrlSetScreenEffect(pXSprite, pPlayer);
                    break;
                case 3: // 67 (start playing qav scene)
                    trPlayerCtrlStartScene(pXSprite, pPlayer, (pXSprite->data4 == 1) ? true : false);
                    break;
                case 4: // 68 (stop playing qav scene)
                    if (pXSprite->data2 > 0 && pXSprite->data2 != pPlayer->sceneQav) break;
                    else trPlayerCtrlStopScene(pPlayer);
                    break;
                case 5: // 69 (set player look angle, TO-DO: if tx > 0, take a look on TX ID sprite)
                    //data4 is reserved
                    if (pXSprite->data4 != 0) break;
                    else if (valueIsBetween(pXSprite->data2, -128, 128))
                        trPlayerCtrlSetLookAngle(pXSprite, pPlayer);
                    break;
                case 6: // 70 (erase player stuff...)
                    if (pXSprite->data2 < 0) break;
                    else trPlayerCtrlEraseStuff(pXSprite, pPlayer);
                    break;
                case 7: // 71 (give something to player...)
                    if (pXSprite->data2 <= 0) break;
                    else trPlayerCtrlGiveStuff(pXSprite, pPlayer, pCtrl);
                    break;
                case 8: // 72 (use inventory item)
                    if (pXSprite->data2 < 1 || pXSprite->data2 > 5) break;
                    else trPlayerCtrlUsePackItem(pXSprite, pPlayer, event.cmd);
                    break;
                case 9: // 73 (set player's sprite angle, TO-DO: if tx > 0, take a look on TX ID sprite)
                    //data4 is reserved
                    if (pXSprite->data4 != 0) break;
                    else if (pSprite->flags & kModernTypeFlag1) pPlayer->q16ang = fix16_from_int(pSprite->ang);
                    else if (valueIsBetween(pXSprite->data2, -kAng360, kAng360)) pPlayer->q16ang = fix16_from_int(pXSprite->data2);
                    break;
                case 10: // 74 (de)activate powerup
                    if (pXSprite->data2 <= 0 || pXSprite->data2 > (kMaxAllowedPowerup - (kMinAllowedPowerup << 1) + 1)) break;
                    trPlayerCtrlUsePowerup(pXSprite, pPlayer, event.cmd);
                    break;
                case 19: // 83: set user input status
                    for (i = connecthead; i >= 0; i = connectpoint2[i])
                    {
                        PLAYER* pOther = &gPlayer[i];
                        if (nCompare == kMaxPlayers || pPlayer == pOther)
                        {
                            if (pXSprite->data2 & kInputDisableAll)
                                pOther->pXSprite->sysData1 |= kInputDisableAll;
                            else
                                pOther->pXSprite->sysData1 &= ~kInputDisableAll;

                            if (pXSprite->data2 & kInputProcessNone)
                                pOther->pXSprite->sysData1 |= kInputProcessNone;
                            else
                                pOther->pXSprite->sysData1 &= ~kInputProcessNone;
                        }
                    }
                    break;
                case 20: // 84: send input
                    switch (event.cmd)
                    {
                        case kCmdOff:
                            if (pXSprite->state)
                            {
                                evKill(nSprite, OBJ_SPRITE, causerID);
                                pXSprite->state = 0;

                                if (pXSprite->triggerOff)
                                {
                                    if (pXSprite->busyTime <= 0
                                        && (nCompare == kMaxPlayers || nCompare == pPlayer->nPlayer))
                                            trPlayerCtrlSendInput(pXSprite, pPlayer);

                                    if (pXSprite->txID)
                                        evSend(pSprite->index, OBJ_SPRITE, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                                }
                            }
                            break;
                        case kCmdOn:
                            if (!pXSprite->state)
                            {
                                evKill(nSprite, OBJ_SPRITE, causerID);
                                pXSprite->state = 1;

                                if (pXSprite->waitTime)
                                    evPost(nSprite, OBJ_SPRITE, EVTIME2TICKS(pXSprite->waitTime), kCmdOff, causerID);
                            }
                            fallthrough__;
                        case kCmdRepeat:
                            if (nCompare == kMaxPlayers || nCompare == pPlayer->nPlayer)
                                trPlayerCtrlSendInput(pXSprite, pPlayer);

                            if (pXSprite->busyTime)
                                evPost(nSprite, OBJ_SPRITE, pXSprite->busyTime, kCmdRepeat, causerID);

                            if (!pXSprite->waitTime && pXSprite->triggerOn && pXSprite->txID)
                                evSend(pSprite->index, OBJ_SPRITE, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                            break;
                        default:
                            evPost(nSprite, OBJ_SPRITE, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                            break;
                    }
                    break;

            }
        }
        return true;
        case kModernViewCamera:
        {
            int nCompare, i, t;
            spritetype *pCSpr = NULL, *pTSpr = NULL, *pSpr = NULL;
            PLAYER* pPlayer;

            if ((pPlayer = helperOperateCheckPlayer(pXSprite, &event, &nCompare)) == NULL)
                return true;

            switch (event.cmd)
            {
                case kCmdOn: // set view camera
                    
                    // Lookup in TX ID directy to
                    // allow track or attach for
                    // locked sprites
        
                    pCSpr = pSprite;
                    pTSpr = pPlayer->pSprite;

                    if (irngok(pXSprite->txID, kChannelPlayer0, kChannelPlayer7))
                    {
                        PLAYER* pPlayer2;
                        t = pXSprite->txID - kChannelPlayer0 + 1;
                        if ((pPlayer2 = getPlayerById(t)) != NULL)
                            pSpr = pPlayer2->pSprite;
                    }
                    else if (pXSprite->txID == kChannelEventCauser)
                    {
                        if (spriRangeIsFine(event.causer)
                            && getPlayerById(sprite[event.causer].type) != pPlayer)
                                pSpr = &sprite[event.causer];
                    }
                    else if (pXSprite->txID >= kChannelUser)
                    {
                        t = pXSprite->txID;
                        for (i = bucketHead[t]; i < bucketHead[t + 1]; i++)
                        {
                            if (rxBucket[i].type != OBJ_SPRITE)
                                continue;
                            
                            pSpr = &sprite[rxBucket[i].index];
                            if (Chance(0x8000))
                                break;
                        }
                    }

                    if (pSpr != NULL)
                    {
                        if ((pSprite->flags & kModernTypeFlag1) && (pSprite->flags & kModernTypeFlag128))
                            pTSpr = pSpr; // track it
                        else
                            pCSpr = pSpr; // attach camera on it
                    }

                    fallthrough__;
                case kCmdOff: // unset view camera
                    if (nCompare == kMaxPlayers || nCompare == pPlayer->nPlayer)
                        useViewCamera(pXSprite, pCSpr, pTSpr, pPlayer);
                    break;
                default:
                    evPost(pSprite->index, OBJ_SPRITE, 0, (COMMAND_ID)(gViewCam.active == 0), causerID);
                    break;
            }
        }
        return true;
        case kModernMusicChanger:
        {
            DICTNODE* hRes;
            PLAYER* pPlayer; int nCompare;
            GAMEOPTIONS* pOpt = &gGameOptions;
            LEVELINFO* pLev = &gEpisodeInfo[pOpt->nEpisode].levelsInfo[pOpt->nLevel];

            if ((pPlayer = helperOperateCheckPlayer(pXSprite, &event, &nCompare)) == NULL)
                return true;

            if (nCompare != kMaxPlayers && nCompare != pPlayer->nPlayer)
                return true;

            switch (event.cmd)
            {
                case kCmdOn:
                    if (pXSprite->data2 > 0
                        && (hRes = gSoundRes.Lookup(pXSprite->data2, "MID")) != NULL)
                            Bstrcpy(pOpt->zLevelSong, hRes->name);
                    else
                        Bstrcpy(pOpt->zLevelSong, pLev->Song);

                    gCustomMusicStatus = 1;
                    if (pSprite->flags & kModernTypeFlag1)
                        gCustomMusicStatus = 2;

                    sndPlaySong(pOpt->zLevelSong, gCustomMusicStatus == 1);
                    break;
                case kCmdOff:
                    Bstrcpy(pOpt->zLevelSong, pLev->Song);
                    gCustomMusicStatus = 0;
                    sndStopSong();
                    break;
                default:
                    evPost(pSprite->index, OBJ_SPRITE, 0, (COMMAND_ID)(gCustomMusicStatus <= 0), causerID);
                    break;
            }
        }
        return true;
        case kGenModernSound:
            switch (event.cmd) {
            case kCmdOff:
                SetSpriteState(nSprite, pXSprite, 0, causerID);
                break;
            case kCmdOn:
                evKill(nSprite, OBJ_SPRITE, causerID);
                SetSpriteState(nSprite, pXSprite, 1, causerID);
                fallthrough__;
            case kCmdRepeat:
                if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                else if (pXSprite->txID)  modernTypeSendCommand(nSprite, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                else useSoundGen(pXSprite, pSprite);
                
                if (pXSprite->busyTime)
                    evPost(nSprite, 3, (120 * pXSprite->busyTime) / 10, kCmdRepeat, causerID);
                break;
            default:
                evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                break;
            }
            return true;
        case kGenModernMissileUniversal:
            switch (event.cmd) {
                case kCmdOff:
                    SetSpriteState(nSprite, pXSprite, 0, causerID);
                    break;
                case kCmdOn:
                    evKill(nSprite, OBJ_SPRITE, causerID);
                    SetSpriteState(nSprite, pXSprite, 1, causerID);
                    fallthrough__;
                case kCmdRepeat:
                    if (event.cmd == kCmdRepeat && !pXSprite->state) break;
                    else if (pXSprite->txID)  modernTypeSendCommand(nSprite, pXSprite->txID, (COMMAND_ID)pXSprite->command, causerID);
                    else useUniMissileGen(pXSprite, pSprite);
                    
                    if (pXSprite->busyTime)
                        evPost(nSprite, 3, (120 * pXSprite->busyTime) / 10, kCmdRepeat, causerID);

                    break;
                default:
                    evPost(nSprite, 3, 0, (COMMAND_ID)(pXSprite->state ^ 1), causerID);
                    break;
            }
            return true;
    }
}

bool modernTypeOperateWall(int nWall, walltype* pWall, XWALL* pXWall, EVENT event) {
    
    switch (pWall->type) {
        case kSwitchOneWay:
            switch (event.cmd) {
                case kCmdOff:
                    SetWallState(nWall, pXWall, 0, event.causer);
                    break;
                case kCmdOn:
                    SetWallState(nWall, pXWall, 1, event.causer);
                    break;
                default:
                    SetWallState(nWall, pXWall, pXWall->restState ^ 1, event.causer);
                    break;
            }
            return true;
        default:
            return false; // no modern type found to work with, go normal OperateWall();
    }
    
}

bool txIsRanged(XSPRITE* pXSource) {
    if (pXSource->data1 > 0 && pXSource->data2 <= 0 && pXSource->data3 <= 0 && pXSource->data4 > 0) {
        if (pXSource->data1 > pXSource->data4) {
            // data1 must be less than data4
            int tmp = pXSource->data1; pXSource->data1 = pXSource->data4;
            pXSource->data4 = tmp;
        }
        return true;
    }
    return false;
}

void seqTxSendCmdAll(XSPRITE* pXSource, int nIndex, COMMAND_ID cmd, bool modernSend, int causerID) {
    
    bool ranged = txIsRanged(pXSource);
    if (ranged) {
        for (pXSource->txID = pXSource->data1; pXSource->txID <= pXSource->data4; pXSource->txID++) {
            if (pXSource->txID <= 0 || pXSource->txID >= kChannelUserMax) continue;
            else if (!modernSend) evSend(nIndex, 3, pXSource->txID, cmd, causerID);
            else modernTypeSendCommand(nIndex, pXSource->txID, cmd, causerID);
        }
    } else {
        for (int i = 0; i <= 3; i++) {
            pXSource->txID = GetDataVal(&sprite[pXSource->reference], i);
            if (pXSource->txID <= 0 || pXSource->txID >= kChannelUserMax) continue;
            else if (!modernSend) evSend(nIndex, 3, pXSource->txID, cmd, causerID);
            else modernTypeSendCommand(nIndex, pXSource->txID, cmd, causerID);
        }
    }
    
    pXSource->txID = pXSource->sysData1 = 0;
    return;
}

void useRandomTx(XSPRITE* pXSource, COMMAND_ID cmd, bool setState, int causerID) {
    
    UNREFERENCED_PARAMETER(cmd);
    
    spritetype* pSource = &sprite[pXSource->reference];
    int tx = 0; int maxRetries = kMaxRandomizeRetries;
    
    if (txIsRanged(pXSource)) {
        while (maxRetries-- >= 0) {
            if ((tx = nnExtRandom(pXSource->data1, pXSource->data4)) != pXSource->txID)
                break;
        }
    } else {
        while (maxRetries-- >= 0) {
            if ((tx = randomGetDataValue(pXSource, kRandomizeTX)) > 0 && tx != pXSource->txID)
                break;
        }
    }

    pXSource->txID = (tx > 0 && tx < kChannelUserMax) ? tx : 0;
    if (setState)
        SetSpriteState(pSource->index, pXSource, pXSource->state ^ 1, causerID);
        //evSend(pSource->index, OBJ_SPRITE, pXSource->txID, (COMMAND_ID)pXSource->command);
}

void useSequentialTx(XSPRITE* pXSource, COMMAND_ID cmd, bool setState, int causerID) {
    
    spritetype* pSource = &sprite[pXSource->reference];
    bool range = txIsRanged(pXSource); int cnt = 3; int tx = 0;

    if (range) {
        
        // make sure sysData is correct as we store current index of TX ID here.
        if (pXSource->sysData1 < pXSource->data1) pXSource->sysData1 = pXSource->data1;
        else if (pXSource->sysData1 > pXSource->data4) pXSource->sysData1 = pXSource->data4;

    } else {
        
        // make sure sysData is correct as we store current index of data field here.
        if (pXSource->sysData1 > 3) pXSource->sysData1 = 0;
        else if (pXSource->sysData1 < 0) pXSource->sysData1 = 3;

    }

    switch (cmd) {
        case kCmdOff:
            if (!range) {
                while (cnt-- >= 0) { // skip empty data fields
                    if (pXSource->sysData1-- < 0) pXSource->sysData1 = 3;
                    if ((tx = GetDataVal(pSource, pXSource->sysData1)) <= 0) continue;
                    else break;
                }
            } else {
                if (--pXSource->sysData1 < pXSource->data1) pXSource->sysData1 = pXSource->data4;
                tx = pXSource->sysData1;
            }
            break;
        default:
            if (!range) {
                while (cnt-- >= 0) { // skip empty data fields
                    if (pXSource->sysData1 > 3) pXSource->sysData1 = 0;
                    if ((tx = GetDataVal(pSource, pXSource->sysData1++)) <= 0) continue;
                    else break;
                }
            } else {
                tx = pXSource->sysData1;
                if (pXSource->sysData1 >= pXSource->data4) {
                    pXSource->sysData1 = pXSource->data1;
                    break;
                }
                pXSource->sysData1++;
            }
            break;
    }

    pXSource->txID = (tx > 0 && tx < kChannelUserMax) ? tx : 0;
    if (setState)
        SetSpriteState(pSource->index, pXSource, pXSource->state ^ 1, causerID);
        //evSend(pSource->index, OBJ_SPRITE, pXSource->txID, (COMMAND_ID)pXSource->command);

}

void useRandomItemGen(spritetype* pSource, XSPRITE* pXSource) {
    // let's first search for previously dropped items and remove it
    if (pXSource->dropMsg > 0) {
        for (short nItem = headspritestat[kStatItem]; nItem >= 0; nItem = nextspritestat[nItem]) {
            spritetype* pItem = &sprite[nItem];
            if ((unsigned int)pItem->type == pXSource->dropMsg && pItem->x == pSource->x && pItem->y == pSource->y && pItem->z == pSource->z) {
                gFX.fxSpawn((FX_ID)29, pSource->sectnum, pSource->x, pSource->y, pSource->z);
                pItem->type = kSpriteDecoration;
                actPostSprite(nItem, kStatFree);
                break;
            }
        }
    }

    // then drop item
    spritetype* pDrop = randomDropPickupObject(pSource, pXSource->dropMsg);
    

    if (pDrop != NULL)
    {
        clampSprite(pDrop);

        // check if generator affected by physics
        if (gPhysSpritesList.Exists(pSource->index) && (pDrop->extra > 0 || dbInsertXSprite(pDrop->index) > 0))
        {
            xsprite[pDrop->extra].physAttr |= (kPhysMove | kPhysGravity | kPhysFalling); // must fall always
            pDrop->cstat &= ~CSTAT_SPRITE_BLOCK;
            gPhysSpritesList.AddIfNotExists(pDrop->index);
            getSpriteMassBySize(pDrop); // create mass cache
        }
    }

}

void useUniMissileGen(XSPRITE* pXSource, spritetype* pSprite) {

    int dx = 0, dy = 0, dz = 0;
    spritetype* pSource = &sprite[pXSource->reference];
    if (pSprite == NULL)
        pSprite = pSource;

    if (pXSource->data1 < kMissileBase || pXSource->data1 >= kMissileMax)
        return;

    if (pSprite->cstat & 32) {
        if (pSprite->cstat & 8) dz = 0x4000;
        else dz = -0x4000;
    } else {
        dx = Cos(pSprite->ang) >> 16;
        dy = Sin(pSprite->ang) >> 16;
        dz = pXSource->data3 << 6; // add slope controlling
        if (dz > 0x10000) dz = 0x10000;
        else if (dz < -0x10000) dz = -0x10000;
    }

    spritetype* pMissile = NULL;
    if ((pMissile = actFireMissile(pSprite, 0, 0, dx, dy, dz, pXSource->data1)) != NULL) {

        int from; // inherit some properties of the generator
        if ((from = (pSource->flags & kModernTypeFlag3)) > 0) {

            
            int canInherit = 0xF;
            if (xspriRangeIsFine(pMissile->extra) && seqGetStatus(OBJ_SPRITE, pMissile->extra) >= 0) {
                
                canInherit &= ~0x8;
               
                SEQINST* pInst = GetInstance(OBJ_SPRITE, pMissile->extra); Seq* pSeq = pInst->pSequence;
                for (int i = 0; i < pSeq->nFrames; i++) {
                    if ((canInherit & 0x4) && pSeq->frames[i].pal != 0) canInherit &= ~0x4;
                    if ((canInherit & 0x2) && pSeq->frames[i].xrepeat != 0) canInherit &= ~0x2;
                    if ((canInherit & 0x1) && pSeq->frames[i].yrepeat != 0) canInherit &= ~0x1;
                }


        }

            if (canInherit != 0) {
                
                if (canInherit & 0x2)
                    pMissile->xrepeat = (from == kModernTypeFlag1) ? pSource->xrepeat : pSprite->xrepeat;
                
                if (canInherit & 0x1)
                    pMissile->yrepeat = (from == kModernTypeFlag1) ? pSource->yrepeat : pSprite->yrepeat;

                if (canInherit & 0x4)
                    pMissile->pal = (from == kModernTypeFlag1) ? pSource->pal : pSprite->pal;
                
                if (canInherit & 0x8)
                    pMissile->shade = (from == kModernTypeFlag1) ? pSource->shade : pSprite->shade;

            }

        }

        // add velocity controlling
        if (pXSource->data2 > 0) {

            int velocity = pXSource->data2 << 12;
            xvel[pMissile->index] = mulscale14(velocity, dx);
            yvel[pMissile->index] = mulscale14(velocity, dy);
            zvel[pMissile->index] = mulscale14(velocity, dz);

        }

        // add bursting for missiles
        if (pMissile->type != kMissileFlareAlt && pXSource->data4 > 0)
            evPost(pMissile->index, 3, ClipHigh(pXSource->data4, 500), kCallbackMissileBurst);

    }

}

void useSoundGen(XSPRITE* pXSource, spritetype* pSprite) {
    //spritetype* pSource = &sprite[pXSource->reference];
    int pitch = pXSource->data4 << 1; if (pitch < 2000) pitch = 0;
    sfxPlay3DSoundCP(pSprite, pXSource->data2, -1, 0, pitch, ClipRange(pXSource->data3, -1, 255));
}

void useIncDecGen(XSPRITE* pXSource, short objType, int objIndex) {
    char buffer[5]; int data = -65535; short tmp = 0; int dataIndex = 0;
    Bsprintf(buffer, "%d", abs(pXSource->data1)); int len = Bstrlen(buffer);
    
    for (int i = 0; i < len; i++) {
        dataIndex = (buffer[i] - 52) + 4;
        if ((data = getDataFieldOfObject(objType, objIndex, dataIndex)) == -65535) {
            consoleSysMsg("\nWrong index of data (%c) for IncDec Gen #%d! Only 1, 2, 3 and 4 indexes allowed!\n", buffer[i], objIndex);
            continue;
        }
        spritetype* pSource = &sprite[pXSource->reference];
        
        if (pXSource->data2 < pXSource->data3) {

            data = ClipRange(data, pXSource->data2, pXSource->data3);
            if ((data += pXSource->data4) >= pXSource->data3) {
                switch (pSource->flags) {
                case kModernTypeFlag0:
                case kModernTypeFlag1:
                    if (data > pXSource->data3) data = pXSource->data3;
                    break;
                case kModernTypeFlag2:
                    if (data > pXSource->data3) data = pXSource->data3;
                    if (!incDecGoalValueIsReached(pXSource)) break;
                    tmp = pXSource->data3;
                    pXSource->data3 = pXSource->data2;
                    pXSource->data2 = tmp;
                    break;
                case kModernTypeFlag3:
                    if (data > pXSource->data3) data = pXSource->data2;
                    break;
                }
            }

        } else if (pXSource->data2 > pXSource->data3) {

            data = ClipRange(data, pXSource->data3, pXSource->data2);
            if ((data -= pXSource->data4) <= pXSource->data3) {
                switch (pSource->flags) {
                case kModernTypeFlag0:
                case kModernTypeFlag1:
                    if (data < pXSource->data3) data = pXSource->data3;
                    break;
                case kModernTypeFlag2:
                    if (data < pXSource->data3) data = pXSource->data3;
                    if (!incDecGoalValueIsReached(pXSource)) break;
                    tmp = pXSource->data3;
                    pXSource->data3 = pXSource->data2;
                    pXSource->data2 = tmp;
                    break;
                case kModernTypeFlag3:
                    if (data < pXSource->data3) data = pXSource->data2;
                    break;
                }
            }
        }

        pXSource->sysData1 = data;
        setDataValueOfObject(objType, objIndex, dataIndex, data);
    }

}


void sprite2sectorSlope(spritetype* pSpr, sectortype* pSect, char rel, bool forcez) {

    int slope = 0, z = 0;
    switch (rel)
    {
        default:
            z = getflorzofslope(pSpr->sectnum, pSpr->x, pSpr->y);
            if ((pSpr->cstat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_FLOOR && pSpr->extra > 0 && xsprite[pSpr->extra].Touch) z--;
            slope = pSect->floorheinum;
            break;
        case 1:
            z = getceilzofslope(pSpr->sectnum, pSpr->x, pSpr->y);
            if ((pSpr->cstat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_FLOOR && pSpr->extra > 0 && xsprite[pSpr->extra].Touch) z++;
            slope = pSect->ceilingheinum;
            break;
    }

    spriteSetSlope(pSpr->index, slope);
    if (forcez)
        pSpr->z = z;
}

void useSlopeChanger(XSPRITE* pXSource, int objType, int objIndex) {

    int slope, oslope, i;
    spritetype* pSource = &sprite[pXSource->reference];
    bool flag2 = (pSource->flags & kModernTypeFlag2);

    if (pSource->flags & kModernTypeFlag1) slope = ClipRange(pXSource->data2, -32767, 32767);
    else slope = (32767 / kPercFull) * ClipRange(pXSource->data2, -kPercFull, kPercFull);

    if (objType == OBJ_SECTOR)
    {
        sectortype* pSect = &sector[objIndex];

        switch (pXSource->data1)
        {
            case 2:
            case 0:
                if (slope == 0) pSect->floorstat &= ~0x0002;
                else if (!(pSect->floorstat & 0x0002))
                    pSect->floorstat |= 0x0002;

                // just set floor slopew
                if (flag2)
                {
                    pSect->floorheinum = slope;
                }
                else
                {
                    // force closest floor aligned sprites to inherit slope of the sector's floor
                    for (i = headspritesect[objIndex], oslope = pSect->floorheinum; i != -1; i = nextspritesect[i])
                    {
                        if ((sprite[i].cstat & CSTAT_SPRITE_ALIGNMENT) != CSTAT_SPRITE_ALIGNMENT_SLOPE) continue;
                        else if (getflorzofslope(objIndex, sprite[i].x, sprite[i].y) - kSlopeDist <= sprite[i].z)
                        {
                            sprite2sectorSlope(&sprite[i], &sector[objIndex], 0, true);

                            // set new slope of floor
                            pSect->floorheinum = slope;

                            // force sloped sprites to be on floor slope z
                            sprite2sectorSlope(&sprite[i], &sector[objIndex], 0, true);

                            // restore old slope for next sprite
                            pSect->floorheinum = oslope;
                        }
                    }

                    // finally set new slope of floor
                    pSect->floorheinum = slope;
                }

                if (pXSource->data1 == 0) break;
                fallthrough__;
            case 1:
                if (slope == 0) pSect->ceilingstat &= ~0x0002;
                else if (!(pSect->ceilingstat & 0x0002))
                    pSect->ceilingstat |= 0x0002;

                // just set ceiling slope
                if (flag2)
                {
                    pSect->ceilingheinum = slope;
                }
                else
                {
                    // force closest floor aligned sprites to inherit slope of the sector's ceiling
                    for (i = headspritesect[objIndex], oslope = pSect->ceilingheinum; i != -1; i = nextspritesect[i])
                    {
                        if ((sprite[i].cstat & CSTAT_SPRITE_ALIGNMENT) != CSTAT_SPRITE_ALIGNMENT_SLOPE) continue;
                        else if (getceilzofslope(objIndex, sprite[i].x, sprite[i].y) + kSlopeDist >= sprite[i].z)
                        {
                            sprite2sectorSlope(&sprite[i], &sector[objIndex], 1, true);

                            // set new slope of ceiling
                            pSect->ceilingheinum = slope;

                            // force sloped sprites to be on ceiling slope z
                            sprite2sectorSlope(&sprite[i], &sector[objIndex], 1, true);

                            // restore old slope for next sprite
                            pSect->ceilingheinum = oslope;
                        }
                    }

                    // finally set new slope of ceiling
                    pSect->ceilingheinum = slope;
                }
                break;
        }

        // let's give a little impulse to the physics sprites...
        for (i = headspritesect[objIndex]; i != -1; i = nextspritesect[i])
        {
            if (sprite[i].extra > 0 && xsprite[sprite[i].extra].physAttr > 0)
            {
                xsprite[sprite[i].extra].physAttr |= kPhysFalling;
                zvel[i]++;
            }
            else if ((sprite[i].statnum == kStatThing || sprite[i].statnum == kStatDude) && (sprite[i].flags & kPhysGravity))
            {
                sprite[i].flags |= kPhysFalling;
                zvel[i]++;
            }
        }
    }
    else if (objType == OBJ_SPRITE)
    {
        spritetype* pSpr = &sprite[objIndex];
        if ((pSpr->cstat & CSTAT_SPRITE_ALIGNMENT) != CSTAT_SPRITE_ALIGNMENT_FLOOR) pSpr->cstat |= CSTAT_SPRITE_ALIGNMENT_FLOOR;
        if ((pSpr->cstat & CSTAT_SPRITE_ALIGNMENT) != CSTAT_SPRITE_ALIGNMENT_SLOPE)
            pSpr->cstat |= CSTAT_SPRITE_ALIGNMENT_SLOPE;

        switch (pXSource->data4)
        {
            case 1:
            case 2:
            case 3:
                if (!sectRangeIsFine(pSpr->sectnum)) break;
                switch (pXSource->data4)
                {
                    case 1: sprite2sectorSlope(pSpr, &sector[pSpr->sectnum], 0, flag2); break;
                    case 2: sprite2sectorSlope(pSpr, &sector[pSpr->sectnum], 1, flag2); break;
                    case 3:
                        if (getflorzofslope(pSpr->sectnum, pSpr->x, pSpr->y) - kSlopeDist <= pSpr->z) sprite2sectorSlope(pSpr, &sector[pSpr->sectnum], 0, flag2);
                        if (getceilzofslope(pSpr->sectnum, pSpr->x, pSpr->y) + kSlopeDist >= pSpr->z) sprite2sectorSlope(pSpr, &sector[pSpr->sectnum], 1, flag2);
                        break;
                }
                break;
            default:
                spriteSetSlope(objIndex, slope);
                break;
        }
    }
}

void useDataChanger(XSPRITE* pXSource, int objType, int objIndex) {
    
    spritetype* pSource = &sprite[pXSource->reference];
    bool flag1 = (pSource->flags & kModernTypeFlag1);
    
    switch (objType) {
        case OBJ_SECTOR:
            if (flag1 || valueIsBetween(pXSource->data1, -1, 32767)) setDataValueOfObject(objType, objIndex, 1, pXSource->data1);
            break;
        case OBJ_SPRITE:
            if (flag1 || valueIsBetween(pXSource->data1, -1, 32767)) setDataValueOfObject(objType, objIndex, 1, pXSource->data1);
            if (flag1 || valueIsBetween(pXSource->data2, -1, 32767)) setDataValueOfObject(objType, objIndex, 2, pXSource->data2);
            if (flag1 || valueIsBetween(pXSource->data3, -1, 32767)) setDataValueOfObject(objType, objIndex, 3, pXSource->data3);
            if (flag1 || valueIsBetween(pXSource->data4, -1, 65535)) setDataValueOfObject(objType, objIndex, 4, pXSource->data4);
            break;
        case OBJ_WALL:
            if (flag1 || valueIsBetween(pXSource->data1, -1, 32767)) setDataValueOfObject(objType, objIndex, 1, pXSource->data1);
            break;
    }
}

void useSectorLigthChanger(XSPRITE* pXSource, sectortype* pSect) {
    
    XSECTOR* pXSector = NULL;
    spritetype* pSource = &sprite[pXSource->reference];
    bool relative = (pSource->flags & kModernTypeFlag16);
    int i, s, e, nExtra;
    
    if (pSect != NULL)
    {
        pXSector = &xsector[pSect->extra];
        nExtra = sector[pXSector->reference].extra;
    }
    else if (xsectRangeIsFine(sector[pSource->sectnum].extra))
    {
        pSect = &sector[pSource->sectnum];
        pXSector = &xsector[pSect->extra];
        nExtra = pSect->extra;
    }
    else
    {
        pSect = &sector[pSource->sectnum];
        nExtra = dbInsertXSector(pSource->sectnum);
        pXSector = &xsector[nExtra];
    }
    
    char paintNow = (pXSector->shadeAlways || pXSector->busy); // i don't understand why just xsector pal is not working
    int cstat = pSource->cstat;
    char status = 0x0;

    getSectorWalls(pXSector->reference, &s, &e);

    if (valueIsBetween(pXSource->data1, -1, 32767))
    {
        if (relative)
            pXSector->wave = ClipHigh(pXSector->wave + pXSource->data1, 11);
        else
            pXSector->wave = ClipHigh(pXSource->data1, 11);
    }

    if (valueIsBetween(pXSource->data2, -128, 128))
    {
        if (relative)
            pXSector->amplitude = ClipRange(pXSector->amplitude + pXSource->data2, -127, 127);
        else
            pXSector->amplitude = pXSource->data2;
    }

    if (valueIsBetween(pXSource->data3, -1, 32767))
    {
        if (relative)
            pXSector->freq = ClipHigh(pXSector->freq + pXSource->data3, 255);
        else
            pXSector->freq = ClipHigh(pXSource->data3, 255);
    }

    if (valueIsBetween(pXSource->data4, -1, 65535))
    {
        if (relative)
            pXSector->phase = ClipHigh(pXSector->phase + pXSource->data4, 255);
        else
            pXSector->phase = ClipHigh(pXSource->data4, 255);
    }

    if (pSource->flags)
    {
        if (pSource->flags != kModernTypeFlag1)
        {
            pXSector->shadeAlways = (pSource->flags & kModernTypeFlag1) ? true : false;
            pXSector->shadeFloor = (pSource->flags & kModernTypeFlag2) ? true : false;
            pXSector->shadeCeiling = (pSource->flags & kModernTypeFlag4) ? true : false;
            pXSector->shadeWalls = (pSource->flags & kModernTypeFlag8) ? true : false;
            pXSector->color = (pSource->pal) ? true : false;

            if ((cstat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_FLOOR)
            {
                if (pXSector->color)
                {
                    // !!! xsector pal bits must be extended
                    if ((cstat & CSTAT_SPRITE_ONE_SIDED) == 0)  status = 0x03;
                    else if (cstat & CSTAT_SPRITE_YFLIP)        status = 0x02;
                    else                                        status = 0x01;
                }

                if (pXSector->shadeFloor && (status & 0x01))
                {
                    if (paintNow)
                    {
                        if (!pXSector->shade)
                            pXSector->floorpal = pSect->floorpal;

                        pSect->floorpal = pSource->pal;
                    }
                    else
                        pXSector->floorpal = pSource->pal;
                }
                else
                    status &= ~0x01;

                if (pXSector->shadeCeiling && (status & 0x02))
                {
                    if (paintNow)
                    {
                        if (!pXSector->shade)
                            pXSector->ceilpal = pSect->ceilingpal;

                        pSect->ceilingpal = pSource->pal;

                    }
                    else
                        pSect->ceilingpal = pSource->pal;
                }
                else
                    status &= ~0x02;
            }

            if (paintNow && pXSector->shade)
            {
                if ((status & 0x01) == 0)
                {
                    pSect->floorpal = pXSector->floorpal;
                    if (pXSector->shadeWalls)
                        for (i = s; i <= e; i++) wall[i].pal = pSect->floorpal;
                }

                if ((status & 0x02) == 0)
                    pSect->ceilingpal = pXSector->ceilpal;
            }
        }
        else
        {
            pXSector->shadeAlways = true;
        }
    }

    // add to shadeList
    i = shadeCount;
    while (--i >= 0 && shadeList[i] != nExtra);
    if (i < 0)
        shadeList[shadeCount++] = nExtra;
    
}

void useTargetChanger(XSPRITE* pXSource, spritetype* pSprite) {
    
    
    if (!IsDudeSprite(pSprite) || pSprite->statnum != kStatDude) {
        switch (pSprite->type) { // can be dead dude turned in gib
            // make current target and all other dudes not attack this dude anymore
        case kThingBloodBits:
        case kThingBloodChunks:
            aiFightFreeTargets(pSprite->index);
            return;
        default:
            return;
        }
    }
    
    
    //spritetype* pSource = &sprite[pXSource->reference];
    
    XSPRITE* pXSprite = &xsprite[pSprite->extra];
    spritetype* pTarget = NULL; XSPRITE* pXTarget = NULL; int receiveHp = 33 + Random(33);
    DUDEINFO* pDudeInfo = getDudeInfo(pSprite->type); int matesPerEnemy = 1;

    // dude is burning?
    if (pXSprite->burnTime > 0 && spriRangeIsFine(pXSprite->burnSource)) {

        if (IsBurningDude(pSprite)) return;
        else {
            spritetype* pBurnSource = &sprite[pXSprite->burnSource];
            if (pBurnSource->extra >= 0) {
                if (pXSource->data2 == 1 && aiFightIsMateOf(pXSprite, &xsprite[pBurnSource->extra])) {
                    pXSprite->burnTime = 0;
                    
                    // heal dude a bit in case of friendly fire
                    int startHp = (pXSprite->sysData2 > 0) ? ClipRange(pXSprite->sysData2 << 4, 1, 65535) : pDudeInfo->startHealth << 4;
                    if (pXSprite->health < startHp) actHealDude(pXSprite, receiveHp, startHp);
                } else if (xsprite[pBurnSource->extra].health <= 0) {
                    pXSprite->burnTime = 0;
                }
            }
        }
    }

    spritetype* pPlayer = aiFightTargetIsPlayer(pXSprite);
    // special handling for player(s) if target changer data4 > 2.
    if (pPlayer != NULL)
    {
        if (pXSource->data4 == 3)
        {
            aiSetTarget(pXSprite, pSprite->x, pSprite->y, pSprite->z);
            aiSetGenIdleState(pSprite, pXSprite);
            if (pSprite->type == kDudeModernCustom)
                cdudeGet(pSprite)->LeechPickup();
        }
        else if (pXSource->data4 == 4)
        {
            aiSetTarget(pXSprite, pPlayer->x, pPlayer->y, pPlayer->z);
            if (pSprite->type == kDudeModernCustom)
                cdudeGet(pSprite)->LeechPickup();
        }
    }

    int maxAlarmDudes = 8 + Random(8);
    if (pXSprite->target > -1 && sprite[pXSprite->target].extra > -1 && pPlayer == NULL) {
        pTarget = &sprite[pXSprite->target]; pXTarget = &xsprite[pTarget->extra];

        if (aiFightUnitCanFly(pSprite) && aiFightIsMeleeUnit(pTarget) && !aiFightUnitCanFly(pTarget))
            pSprite->flags |= 0x0002;
        else if (aiFightUnitCanFly(pSprite))
            pSprite->flags &= ~0x0002;

        if (!IsDudeSprite(pTarget) || pXTarget->health < 1 || !aiFightDudeCanSeeTarget(pXSprite, pDudeInfo, pTarget)) {
            aiSetTarget(pXSprite, pSprite->x, pSprite->y, pSprite->z);
        }
        // dude attack or attacked by target that does not fit by data id?
        else if (pXSource->data1 != 666 && pXTarget->data1 != pXSource->data1) {
            if (aiFightDudeIsAffected(pXTarget)) {

                // force stop attack target
                aiSetTarget(pXSprite, pSprite->x, pSprite->y, pSprite->z);
                if (pXSprite->burnSource == pTarget->index) {
                    pXSprite->burnTime = 0;
                    pXSprite->burnSource = -1;
                }

                // force stop attack dude
                aiSetTarget(pXTarget, pTarget->x, pTarget->y, pTarget->z);
                if (pXTarget->burnSource == pSprite->index) {
                    pXTarget->burnTime = 0;
                    pXTarget->burnSource = -1;
                }
            }

        }
        else if (pXSource->data2 == 1 && aiFightIsMateOf(pXSprite, pXTarget)) {
            spritetype* pMate = pTarget; XSPRITE* pXMate = pXTarget;

            // heal dude
            int startHp = (pXSprite->sysData2 > 0) ? ClipRange(pXSprite->sysData2 << 4, 1, 65535) : pDudeInfo->startHealth << 4;
            if (pXSprite->health < startHp) actHealDude(pXSprite, receiveHp, startHp);

            // heal mate
            startHp = (pXMate->sysData2 > 0) ? ClipRange(pXMate->sysData2 << 4, 1, 65535) : getDudeInfo(pMate->type)->startHealth << 4;
            if (pXMate->health < startHp) actHealDude(pXMate, receiveHp, startHp);

            if (pXMate->target > -1 && sprite[pXMate->target].extra >= 0) {
                pTarget = &sprite[pXMate->target];
                // force mate stop attack dude, if he does
                if (pXMate->target == pSprite->index) {
                    aiSetTarget(pXMate, pMate->x, pMate->y, pMate->z);
                } else if (!aiFightIsMateOf(pXSprite, &xsprite[pTarget->extra])) {
                    // force dude to attack same target that mate have
                    aiSetTarget(pXSprite, pTarget->index);
                    return;

                } else {
                    // force mate to stop attack another mate
                    aiSetTarget(pXMate, pMate->x, pMate->y, pMate->z);
                }
            }

            // force dude stop attack mate, if target was not changed previously
            if (pXSprite->target == pMate->index)
                aiSetTarget(pXSprite, pSprite->x, pSprite->y, pSprite->z);


        }
        // check if targets aims player then force this target to fight with dude
        else if (aiFightTargetIsPlayer(pXTarget) != NULL) {
            aiSetTarget(pXTarget, pSprite->index);
        }

        int mDist = 3; if (aiFightIsMeleeUnit(pSprite)) mDist = 2;
        if (pXSprite->target >= 0 && aiFightGetTargetDist(pSprite, pDudeInfo, &sprite[pXSprite->target]) < mDist) {
            if (!isActive(pSprite->index)) aiActivateDude(pSprite, pXSprite);
            return;
        }
        // lets try to look for target that fits better by distance
        else if (((int)gFrameClock & 256) != 0 && (pXSprite->target < 0 || aiFightGetTargetDist(pSprite, pDudeInfo, pTarget) >= mDist)) {
            pTarget = aiFightGetTargetInRange(pSprite, 0, mDist, pXSource->data1, pXSource->data2);
            if (pTarget != NULL) {
                pXTarget = &xsprite[pTarget->extra];

                // Make prev target not aim in dude
                if (pXSprite->target > -1) {
                    spritetype* prvTarget = &sprite[pXSprite->target];
                    aiSetTarget(&xsprite[prvTarget->extra], prvTarget->x, prvTarget->y, prvTarget->z);
                    if (!isActive(pTarget->index))
                        aiActivateDude(pTarget, pXTarget);
                }

                // Change target for dude
                aiSetTarget(pXSprite, pTarget->index);
                if (!isActive(pSprite->index))
                    aiActivateDude(pSprite, pXSprite);

                // ...and change target of target to dude to force it fight
                if (pXSource->data3 > 0 && pXTarget->target != pSprite->index) {
                    aiSetTarget(pXTarget, pSprite->index);
                    if (!isActive(pTarget->index))
                        aiActivateDude(pTarget, pXTarget);
                }

                return;
            }
        }
    }
    
    if ((pXSprite->target < 0 || pPlayer != NULL) && ((int)gFrameClock & 32) != 0) {
        // try find first target that dude can see
        for (int nSprite = headspritestat[kStatDude]; nSprite >= 0; nSprite = nextspritestat[nSprite]) {
            
            pTarget = &sprite[nSprite]; pXTarget = &xsprite[pTarget->extra];

            if (pXTarget->target == pSprite->index) {
                aiSetTarget(pXSprite, pTarget->index);
                return;
            }

            // skip non-dudes and players
            if (!IsDudeSprite(pTarget) || (IsPlayerSprite(pTarget) && pXSource->data4 > 0) || pTarget->owner == pSprite->index) continue;
            // avoid self aiming, those who dude can't see, and those who dude own
            else if (!aiFightDudeCanSeeTarget(pXSprite, pDudeInfo, pTarget) || pSprite->index == pTarget->index) continue;
            // if Target Changer have data1 = 666, everyone can be target, except AI team mates.
            else if (pXSource->data1 != 666 && pXSource->data1 != pXTarget->data1) continue;
            // don't attack immortal, burning dudes and mates
            if (IsBurningDude(pTarget) || !IsKillableDude(pTarget) || (pXSource->data2 == 1 && aiFightIsMateOf(pXSprite, pXTarget)))
                continue;

            if (pXSource->data2 == 0 || (pXSource->data2 == 1 && !aiFightMatesHaveSameTarget(pXSprite, pTarget, matesPerEnemy))) {

                // Change target for dude
                aiSetTarget(pXSprite, pTarget->index);
                if (!isActive(pSprite->index))
                    aiActivateDude(pSprite, pXSprite);

                // ...and change target of target to dude to force it fight
                if (pXSource->data3 > 0 && pXTarget->target != pSprite->index) {
                    aiSetTarget(pXTarget, pSprite->index);
                    if (pPlayer == NULL && !isActive(pTarget->index))
                        aiActivateDude(pTarget, pXTarget);

                    if (pXSource->data3 == 2)
                        aiFightAlarmDudesInSight(pTarget, maxAlarmDudes);
                }
                
                return;
            }
            
            break;
        }
    }

    // got no target - let's ask mates if they have targets
    if ((pXSprite->target < 0 || pPlayer != NULL) && pXSource->data2 == 1 && ((int)gFrameClock & 64) != 0) {
        spritetype* pMateTarget = NULL;
        if ((pMateTarget = aiFightGetMateTargets(pXSprite)) != NULL && pMateTarget->extra > 0) {
            XSPRITE* pXMateTarget = &xsprite[pMateTarget->extra];
            if (aiFightDudeCanSeeTarget(pXSprite, pDudeInfo, pMateTarget)) {
                if (pXMateTarget->target < 0) {
                    aiSetTarget(pXMateTarget, pSprite->index);
                    if (IsDudeSprite(pMateTarget) && !isActive(pMateTarget->index))
                        aiActivateDude(pMateTarget, pXMateTarget);
                }

                aiSetTarget(pXSprite, pMateTarget->index);
                if (!isActive(pSprite->index))
                    aiActivateDude(pSprite, pXSprite);
                return;

                // try walk in mate direction in case if not see the target
            } else if (pXMateTarget->target >= 0 && aiFightDudeCanSeeTarget(pXSprite, pDudeInfo, &sprite[pXMateTarget->target])) {
                spritetype* pMate = &sprite[pXMateTarget->target];
                pXSprite->target = pMateTarget->index;
                pXSprite->targetX = pMate->x;
                pXSprite->targetY = pMate->y;
                pXSprite->targetZ = pMate->z;
                if (!isActive(pSprite->index))
                    aiActivateDude(pSprite, pXSprite);
                return;
            }
        }
    }
}

void usePictureChanger(XSPRITE* pXSource, int objType, int objIndex) {
    
    //spritetype* pSource = &sprite[pXSource->reference];
    
    switch (objType) {
        case OBJ_SECTOR:
            if (valueIsBetween(pXSource->data1, -1, 32767))
                sector[objIndex].floorpicnum = pXSource->data1;

            if (valueIsBetween(pXSource->data2, -1, 32767))
                sector[objIndex].ceilingpicnum = pXSource->data2;

            if (valueIsBetween(pXSource->data3, -1, 32767))
                sector[objIndex].floorpal = pXSource->data3;

            if (valueIsBetween(pXSource->data4, -1, 65535))
                sector[objIndex].ceilingpal = pXSource->data4;
            break;
        case OBJ_SPRITE:
            if (valueIsBetween(pXSource->data1, -1, 32767))
                sprite[objIndex].picnum = pXSource->data1;

            if (pXSource->data2 >= 0) sprite[objIndex].shade = (pXSource->data2 > 127) ? 127 : pXSource->data2;
            else if (pXSource->data2 < -1) sprite[objIndex].shade = (pXSource->data2 < -127) ? -127 : pXSource->data2;

            if (valueIsBetween(pXSource->data3, -1, 32767))
                sprite[objIndex].pal = pXSource->data3;
            break;
        case OBJ_WALL:
            if (valueIsBetween(pXSource->data1, -1, 32767))
                wall[objIndex].picnum = pXSource->data1;

            if (valueIsBetween(pXSource->data2, -1, 32767))
                wall[objIndex].overpicnum = pXSource->data2;

            if (valueIsBetween(pXSource->data3, -1, 32767))
                wall[objIndex].pal = pXSource->data3;
            break;
    }
}

void useCustomDudeSpawn(XSPRITE* pXSource, spritetype* pSprite) {

    cdudeSpawn(pXSource, pSprite, pSprite->clipdist << 1);

}

void useDripGenerator(XSPRITE* pXSource, spritetype* pSprite)
{
    int top, bottom;
    GetSpriteExtents(pSprite, &top, &bottom);
    spritetype* pThing = actSpawnThing(pSprite->sectnum, pSprite->x, pSprite->y, bottom, (pSprite->type == kGenDripWater) ? kThingDripWater : kThingDripBlood);
    actPropagateSpriteOwner(pThing, pSprite);
    if (pXSource->data4)
        zvel[pThing->index] = mulscale8(0x10000, pXSource->data4);
}

void useDudeSpawn(XSPRITE* pXSource, spritetype* pSprite) {

    if (randomSpawnDude(pXSource, pSprite, pSprite->clipdist << 1, 0) == NULL)
        nnExtSpawnDude(pXSource, pSprite, pXSource->data1, pSprite->clipdist << 1, 0);
}

bool seqForceOverride(spritetype* pSpr, SEQINST* pInst, bool ovrPic, bool ovrPal, bool ovrShd, bool ovrRep, bool ovrCst, bool setTimer)
{
    bool xrp, yrp, plu; 
    bool killSeq = (ovrPic || ovrShd);
    Seq* pSeq;

    UNREFERENCED_PARAMETER(ovrCst);

    if (pInst && ((pSeq = pInst->pSequence) != NULL))
    {
        if (!killSeq)
        {
            seqCanOverride(pSeq, 1, &xrp, &yrp, &plu);
            killSeq = (ovrRep && (!xrp || !yrp));
            if (!killSeq)
                killSeq = (ovrPal && !plu);
        }

        if (killSeq)
        {
            if (setTimer && (pSeq->flags & 2)) // remove when done flag
            {
                evKill(pSpr->index, OBJ_SPRITE, (CALLBACK_ID)kCallbackRemove);
                evPost(pSpr->index, OBJ_SPRITE, pInst->timeCount * kTicsPerFrame, (CALLBACK_ID)kCallbackRemove); // post remove with new time
            }

            seqKill(OBJ_SPRITE, pSpr->extra);
            return true;
        }
    }
    
    return false;
}

void gibPropagateAppearance(XSPRITE* pXSrc, spritetype* pFrom, spritetype* pDest)
{
    int t, flags = pXSrc->sysData1;
    bool xrp = true, yrp = true, plu = true;
    bool setPal = (flags & kModernTypeFlag1);
    bool setShd = (flags & kModernTypeFlag2);
    bool setRep = (flags & kModernTypeFlag4);
    bool setPic = (flags & kModernTypeFlag8);
    bool setCst = (flags & kModernTypeFlag16);
    bool setAng = (flags & kModernTypeFlag32);
    bool killSeq = (setPic || setShd);

    
    if (pDest->extra) // check and kill seq animation so sprites can inherit appearance
        seqForceOverride(pDest, GetInstance(OBJ_SPRITE, pDest->extra), setPic, setPal, setShd, setRep, setCst, !pXSrc->busyTime);

    
    if (pDest->extra > 0 && (t = seqGetID(OBJ_SPRITE, pDest->extra)) >= 0)
    {
        Seq* pSeq = (Seq*)gSysRes.Load(gSysRes.Lookup(t, "SEQ"));
        if (pSeq)
        {
            if (!killSeq)
            {
                seqCanOverride(pSeq, 1, &xrp, &yrp, &plu);
                killSeq = (setRep && (!xrp || !yrp));
                if (!killSeq)
                    killSeq = (setPal && !plu);
            }

            if (killSeq)
            {
                if (!pXSrc->busyTime && (pSeq->flags & 2)) // remove when done flag
                {
                    SEQINST* pInst = GetInstance(OBJ_SPRITE, pDest->extra);
                    evKill(pDest->index, OBJ_SPRITE, (CALLBACK_ID)kCallbackRemove);
                    evPost(pDest->index, OBJ_SPRITE, pInst->timeCount * kTicsPerFrame, (CALLBACK_ID)kCallbackRemove); // post remove with new time
                }

                seqKill(OBJ_SPRITE, pDest->extra);
            }
        }
    }

    // override remove timer
    if (pXSrc->busyTime)
    {
        evKill(pDest->index, OBJ_SPRITE, (CALLBACK_ID)kCallbackRemove);
        evPost(pDest->index, OBJ_SPRITE, EVTIME2TICKS(pXSrc->busyTime), (CALLBACK_ID)kCallbackRemove); // post remove with new time
    }

    if (setPal)
        pDest->pal = pFrom->pal;

    if (setShd)
        pDest->shade = pFrom->shade;

    if (setRep)
    {
        pDest->xrepeat = pFrom->xrepeat;
        pDest->yrepeat = pFrom->yrepeat;
    }

    if (setPic)
        pDest->picnum = pFrom->picnum;

    if (setCst)
    {
        pDest->cstat = pFrom->cstat;
        if (pDest->cstat & CSTAT_SPRITE_INVISIBLE)
            pDest->cstat &= ~CSTAT_SPRITE_INVISIBLE;
    }

    if (setAng)
    {
        pDest->ang = pFrom->ang;
    }
    else
    {
        t = pDest->ang;
        while(t == pDest->ang)
            pDest->ang = Random2(kAng360) & kAngMask;
    }
}

void useGibObject(XSPRITE* pXSource, spritetype* pSpr)
{
    spritetype* pEff;
    spritetype* pSource = &sprite[pXSource->reference];

    if (!pSpr)
        pSpr = pSource;

    static int a[3];
    int e, i = pSpr->index;
    a[0] = ClipRange(pXSource->data1, 0, 31);
    a[1] = ClipRange(pXSource->data2, 0, 31);
    a[2] = ClipRange(pXSource->data3, 0, 31);

    e = (pXSource->burnTime || mapRev2()) ? 3 : 2;

    bool fromSrc = !(pXSource->sysData1 & kModernTypeFlag64);
    bool toDest = ((pXSource->sysData1 & kModernTypeFlag128) && pXSource->txID);
    bool remove = (!toDest && !(pSource->cstat & CSTAT_SPRITE_INVISIBLE));

    if (!pXSource->sysData1)
    {
        i = 0;
        while (i < e)   // just create new sprites
        {
            if (a[i])
                GibSprite(pSpr, (GIBTYPE)(a[i] - 1), NULL, NULL);

            i++;
        }
    }
    else
    {
        IDLIST fxList(true);

        // i don't think there is other easy way to know
        // which sprites belongs to current objects, so...

        // collect all potential effect sprites in this sector
        for (i = headspritesect[pSpr->sectnum]; i >= 0; i = nextspritesect[i])
        {
            pEff = &sprite[i];
            if (pEff->owner == pSource->index || (pEff->flags & kHitagFree)) continue;
            else if (pEff->statnum == kStatFX || pEff->statnum == kStatThing)
                fxList.Add(i);
        }

        i = 0;
        while (i < e) // create new sprites
        {
            if (a[i])
                GibSprite(pSpr, (GIBTYPE)(a[i] - 1), NULL, NULL);

            i++;
        }

        // propagate only to new sprites
        for (i = headspritesect[pSpr->sectnum]; i >= 0; i = nextspritesect[i])
        {
            pEff = &sprite[i];
            if (pEff->flags & kHitagFree)  continue;
            else if (pEff->statnum != kStatFX && pEff->statnum != kStatThing) continue;
            else if (pEff->owner != pSource->index && !fxList.Exists(i))
            {
                gibPropagateAppearance(pXSource, (fromSrc) ? pSource : pSpr, &sprite[i]);
                pEff->owner = pSource->index;
            }
        }

        //viewSetSystemMessage("%d / %d / %d", fxList.Length(), fxList.SizeOf(), gStatCount[kStatFX]);
        fxList.Free();
    }

    if (pXSource->data4 > 0)
        sfxPlay3DSound(pSpr->x, pSpr->y, pSpr->z, pXSource->data4, pSpr->sectnum);

    if (pXSource->dropMsg > 0)
        actDropObject(pSource, pXSource->dropMsg);

    if (remove)
        actPostSprite(pSource->index, kStatFree);
}

//---------------------------------------

// player related
void playerProcessAiState(void)
{
    PLAYER* pPlay; spritetype* pSpr; XSPRITE* pXSpr;
    int i;

    for (i = connecthead; i >= 0; i = connectpoint2[i])
    {
        pPlay = &gPlayer[i];
        if (!spriRangeIsFine(pPlay->nSprite))
            continue;

        pSpr = &sprite[pPlay->nSprite];
        if (!xspriRangeIsFine(pSpr->extra))
            continue;

        pXSpr = &xsprite[pSpr->extra];
        pXSpr->stateTimer = ClipLow(pXSpr->stateTimer-kTicsPerFrame, 0);

        if (pXSpr->aiState && pXSpr->aiState->moveFunc)
            pXSpr->aiState->moveFunc(pSpr, pXSpr);

        if (pXSpr->aiState
            && pXSpr->aiState->thinkFunc && (gFrame & 3) == (pSpr->index & 3))
                pXSpr->aiState->thinkFunc(pSpr, pXSpr);
    }
}

QAV* playerQavSceneLoad(int qavId) {
    QAV* pQav = NULL; DICTNODE* hQav = gSysRes.Lookup(qavId, "QAV");

    if (hQav) pQav = (QAV*)gSysRes.Lock(hQav);
    else viewSetSystemMessage("Failed to load QAV animation #%d", qavId);

    return pQav;
}

void playerQavSceneProcess(PLAYER* pPlayer, QAVSCENE* pQavScene) {
    int nIndex = pQavScene->index;
    if (xspriRangeIsFine(sprite[nIndex].extra)) {
        
        XSPRITE* pXSprite = &xsprite[sprite[nIndex].extra];
        if (pXSprite->waitTime > 0 && --pXSprite->sysData1 <= 0) {
            if (pXSprite->txID >= kChannelUser) {
                
                XSPRITE* pXSpr = NULL;
                for (int i = bucketHead[pXSprite->txID]; i < bucketHead[pXSprite->txID + 1]; i++) {
                    if (rxBucket[i].type == OBJ_SPRITE) {
                        
                        spritetype* pSpr = &sprite[rxBucket[i].index];
                        if (pSpr->index == nIndex || !xspriRangeIsFine(pSpr->extra))
                            continue;

                        pXSpr = &xsprite[pSpr->extra];
                        if (pSpr->type == kModernPlayerControl && pXSpr->command == 67) {
                            if (pXSpr->data2 == pXSprite->data2 || pXSpr->locked) continue;
                            else trPlayerCtrlStartScene(pXSpr, pPlayer, true);
                            return;
                        }

                    }

                    nnExtTriggerObject(rxBucket[i].type, rxBucket[i].index, pXSprite->command, pPlayer->nSprite);

                }
            } //else {
                
                trPlayerCtrlStopScene(pPlayer);

            //}

        } else {
            
            playerQavScenePlay(pPlayer);
            pPlayer->weaponTimer = ClipLow(pPlayer->weaponTimer -= kTicsPerFrame, 0);

        }
    } else {
        
        pQavScene->index = pPlayer->sceneQav = -1;
        pQavScene->qavResrc = NULL;
    }
}

void playerQavSceneDraw(PLAYER* pPlayer, int a2, int x, int y, int a5) {
    if (pPlayer == NULL || pPlayer->sceneQav == -1) return;

    QAVSCENE* pQavScene = &gPlayerCtrl[pPlayer->nPlayer].qavScene;
    spritetype* pSprite = &sprite[pQavScene->index];

    if (pQavScene->qavResrc != NULL) {

        QAV* pQAV = pQavScene->qavResrc;
        int v4 = (pPlayer->weaponTimer == 0) ? (int)totalclock % pQAV->at10 : pQAV->at10 - pPlayer->weaponTimer;

        int flags = 2 | kQavOrientationQ16; int nInv = powerupCheck(pPlayer, kPwUpShadowCloak);
        if (nInv >= 120 * 8 || (nInv != 0 && ((int)totalclock & 32))) {
            a2 = -128; flags |= 1;
        }

        // draw as weapon
        if (!(pSprite->flags & kModernTypeFlag1)) {

            pQAV->x = x; pQAV->y = y;
            pQAV->Draw(v4, flags, a2, a5);

            // draw fullscreen (currently 4:3 only)
        } else {

            int wx1 = windowxy1.x, wy1 = windowxy1.y, wx2 = windowxy2.x, wy2 = windowxy2.y;

            windowxy2.x = xdim - 1; windowxy2.y = ydim - 1;
            windowxy1.x = windowxy1.y = 0;

            pQAV->Draw(v4, flags, a2, a5);

            windowxy1.x = wx1; windowxy1.y = wy1;
            windowxy2.x = wx2; windowxy2.y = wy2;

        }

    }

}

void playerQavScenePlay(PLAYER* pPlayer) {
    if (pPlayer == NULL) return;
    
    QAVSCENE* pQavScene = &gPlayerCtrl[pPlayer->nPlayer].qavScene;
    if (pPlayer->sceneQav == -1 && pQavScene->index >= 0)
        pPlayer->sceneQav = xsprite[sprite[pQavScene->index].extra].data2;

    if (pQavScene->qavResrc != NULL) {
        QAV* pQAV = pQavScene->qavResrc;
        pQAV->nSprite = pPlayer->pSprite->index;
        int nTicks = pQAV->at10 - pPlayer->weaponTimer;
        pQAV->Play(nTicks - kTicsPerFrame, nTicks, pPlayer->qavCallback, pPlayer);
    }
}

bool playerSizeShrink(PLAYER* pPlayer, int divider) {
    pPlayer->pXSprite->scale = 256 / divider;
    playerSetRace(pPlayer, kModeHumanShrink);
    return true;
}

bool playerSizeGrow(PLAYER* pPlayer, int multiplier) {
    pPlayer->pXSprite->scale = 256 * multiplier;
    playerSetRace(pPlayer, kModeHumanGrown);
    return true;
}

bool playerSizeReset(PLAYER* pPlayer) {
    playerSetRace(pPlayer, kModeHuman);
    pPlayer->pXSprite->scale = 0;
    return true;
}

void playerDeactivateShrooms(PLAYER* pPlayer) {
    powerupDeactivate(pPlayer, kPwUpGrowShroom);
    pPlayer->pwUpTime[kPwUpGrowShroom] = 0;

    powerupDeactivate(pPlayer, kPwUpShrinkShroom);
    pPlayer->pwUpTime[kPwUpShrinkShroom] = 0;
}



PLAYER* getPlayerById(short id) {

    // relative to connected players
    if (id >= 1 && id <= kMaxPlayers) {
        id = id - 1;
        for (int i = connecthead; i >= 0; i = connectpoint2[i]) {
            if (id == gPlayer[i].nPlayer)
                return &gPlayer[i];
        }

    // absolute sprite type
    } else if (id >= kDudePlayer1 && id <= kDudePlayer8) {
        for (int i = connecthead; i >= 0; i = connectpoint2[i]) {
            if (gPlayer[i].pSprite && id == gPlayer[i].pSprite->type)
                return &gPlayer[i];
        }
    }

    //viewSetSystemMessage("There is no player id #%d", id);
    return NULL;
}

// misc functions
bool IsBurningDude(spritetype* pSprite) {
    if (pSprite == NULL) return false;
    switch (pSprite->type) {
    case kDudeBurningInnocent:
    case kDudeBurningCultist:
    case kDudeBurningZombieAxe:
    case kDudeBurningZombieButcher:
    case kDudeBurningTinyCaleb:
    case kDudeBurningBeast:
        return true;
    case kDudeModernCustom:
        CUSTOMDUDE* pDude = cdudeGet(pSprite);
        return (pDude->StatusTest(kCdudeStatusBurning));
    }

    return false;
}

bool IsKillableDude(spritetype* pSprite) {
    switch (pSprite->type) {
    case kDudeGargoyleStatueFlesh:
    case kDudeGargoyleStatueStone:
        return false;
    default:
        if (!IsDudeSprite(pSprite) || xsprite[pSprite->extra].locked == 1) return false;
        return true;
    }
}

bool isGrown(spritetype* pSprite) {
    if (powerupCheck(&gPlayer[pSprite->type - kDudePlayer1], kPwUpGrowShroom) > 0) return true;
    else if (pSprite->extra >= 0 && xsprite[pSprite->extra].scale >= 512) return true;
    else return false;
}

bool isShrinked(spritetype* pSprite) {
    if (powerupCheck(&gPlayer[pSprite->type - kDudePlayer1], kPwUpShrinkShroom) > 0) return true;
    else if (pSprite->extra >= 0 && xsprite[pSprite->extra].scale > 0 && xsprite[pSprite->extra].scale <= 128) return true;
    else return false;
}

bool isActive(int nSprite) {
    if (sprite[nSprite].extra < 0 || sprite[nSprite].extra >= kMaxXSprites)
        return false;

    XSPRITE* pXDude = &xsprite[sprite[nSprite].extra];
    if (!pXDude->aiState)
        return false;

    switch (pXDude->aiState->stateType) {
    case kAiStateIdle:
    case kAiStateGenIdle:
    case kAiStateSearch:
    case kAiStateMove:
    case kAiStateOther:
        return false;
    default:
        return true;
    }
}

int getDataFieldOfObject(int objType, int objIndex, int dataIndex) {
    int data = -65535;
    switch (objType) {
        case OBJ_SPRITE:
            switch (dataIndex) {
                case 1: return xsprite[sprite[objIndex].extra].data1;
                case 2: return xsprite[sprite[objIndex].extra].data2;
                case 3:
                    switch (sprite[objIndex].type) {
                        case kDudeModernCustom: return xsprite[sprite[objIndex].extra].sysData1;
                        default: return xsprite[sprite[objIndex].extra].data3;
                    }
                case 4:return xsprite[sprite[objIndex].extra].data4;
                default: return data;
            }
        case OBJ_SECTOR: return xsector[sector[objIndex].extra].data;
        case OBJ_WALL: return xwall[wall[objIndex].extra].data;
        default: return data;
    }
}

bool setDataValueOfObject(int objType, int objIndex, int dataIndex, int value) {
    switch (objType) {
        case OBJ_SPRITE:
        {
            spritetype* pSpr = &sprite[objIndex];
            XSPRITE* pXSpr = &xsprite[sprite[objIndex].extra];

            CUSTOMDUDE* pDude = NULL;
            if (IsDudeSprite(pSpr))
            {
                if (pXSpr->health <= 0)
                    return true;

                if (IsCustomDude(pSpr))
                    pDude = cdudeGet(pSpr->index);
            }
            else
            {
            // exceptions
                switch (pSpr->type)
                {
                case kThingBloodBits:
                case kThingBloodChunks:
                case kThingZombieHead:
                    return true;
                }
            }

            switch (dataIndex) {
                case 1:
                    pXSpr->data1 = value;
                    switch (pSpr->type)
                    {
                        case kSwitchCombo:
                            if (value == pXSpr->data2) SetSpriteState(objIndex, pXSpr, 1, kCauserGame);
                            else SetSpriteState(objIndex, pXSpr, 0, kCauserGame);
                            return true;
                    }

                    if (pDude)
                        CUSTOMDUDE_SETUP::Setup(pDude);

                    return true;
                case 2:
                    pXSpr->data2 = value;
                    if (pDude && pDude->version == 1)
                        CUSTOMDUDE_SETUP::Setup(pDude);
                    return true;
                case 3:
                    pXSpr->data3 = value;
                    if (pDude && pDude->version == 1)
                    {
                        pXSpr->sysData1 = value;
                        CUSTOMDUDE_SETUP::Setup(pDude);
                    }
                    return true;
                case 4:
                    pXSpr->data4 = value;
                    return true;
                default:
                    return false;
            }
        }
        case OBJ_SECTOR:
            xsector[sector[objIndex].extra].data = value;
            return true;
        case OBJ_WALL:
            xwall[wall[objIndex].extra].data = value;
            return true;
        default:
            return false;
    }
}

int listTx(XSPRITE* pXRedir, int tx) {
    if (txIsRanged(pXRedir)) {
        if (tx == -1) tx = pXRedir->data1;
        else if (tx < pXRedir->data4) tx++;
        else tx = -1;
    } else {
        if (tx == -1) {
            for (int i = 0; i <= 3; i++) {
                if ((tx = GetDataVal(&sprite[pXRedir->reference], i)) <= 0) continue;
                else return tx;
            }
        } else {
            int saved = tx; bool savedFound = false;
            for (int i = 0; i <= 3; i++) {
                tx = GetDataVal(&sprite[pXRedir->reference], i);
                if (savedFound && tx > 0) return tx;
                else if (tx != saved) continue;
                else savedFound = true;
            }
        }
        
        tx = -1;
    }

    return tx;
}

XSPRITE* evrIsRedirector(int nSprite) {
    if (spriRangeIsFine(nSprite)) {
        switch (sprite[nSprite].type) {
        case kModernRandomTX:
        case kModernSequentialTX:
            if (xspriRangeIsFine(sprite[nSprite].extra) && xsprite[sprite[nSprite].extra].command == kCmdLink
                && !xsprite[sprite[nSprite].extra].locked) return &xsprite[sprite[nSprite].extra];
            break;
        }
    }

    return NULL;
}

XSPRITE* evrListRedirectors(int objType, int objXIndex, XSPRITE* pXRedir, int* tx) {
    if (!gEventRedirectsUsed) return NULL;
    else if (pXRedir && (*tx = listTx(pXRedir, *tx)) != -1)
        return pXRedir;

    int id = 0;
    switch (objType) {
        case OBJ_SECTOR:
            if (!xsectRangeIsFine(objXIndex)) return NULL;
            id = xsector[objXIndex].txID;
            break;
        case OBJ_SPRITE:
            if (!xspriRangeIsFine(objXIndex)) return NULL;
            id = xsprite[objXIndex].txID;
            break;
        case OBJ_WALL:
            if (!xwallRangeIsFine(objXIndex)) return NULL;
            id = xwall[objXIndex].txID;
            break;
        default:
            return NULL;
    }

    int nIndex = (pXRedir) ? pXRedir->reference : -1; bool prevFound = false;
    for (int i = bucketHead[id]; i < bucketHead[id + 1]; i++) {
        if (rxBucket[i].type != OBJ_SPRITE) continue;
        XSPRITE* pXSpr = evrIsRedirector(rxBucket[i].index);
        if (!pXSpr) continue;
        else if (prevFound || nIndex == -1) { *tx = listTx(pXSpr, *tx); return pXSpr; }
        else if (nIndex != pXSpr->reference) continue;
        else prevFound = true;
    }

    *tx = -1;
    return NULL;
}

// this function checks if all TX objects have the same value
bool incDecGoalValueIsReached(XSPRITE* pXSprite) {
    
    if (pXSprite->data3 != pXSprite->sysData1) return false;
    char buffer[5]; Bsprintf(buffer, "%d", abs(pXSprite->data1)); int len = Bstrlen(buffer); int rx = -1;
    for (int i = bucketHead[pXSprite->txID]; i < bucketHead[pXSprite->txID + 1]; i++) {
        if (rxBucket[i].type == OBJ_SPRITE && evrIsRedirector(rxBucket[i].index)) continue;
        for (int a = 0; a < len; a++) {
            if (getDataFieldOfObject(rxBucket[i].type, rxBucket[i].index, (buffer[a] - 52) + 4) != pXSprite->data3)
                return false;
        }
    }

    XSPRITE* pXRedir = NULL; // check redirected TX buckets
    while ((pXRedir = evrListRedirectors(OBJ_SPRITE, sprite[pXSprite->reference].extra, pXRedir, &rx)) != NULL) {
        for (int i = bucketHead[rx]; i < bucketHead[rx + 1]; i++) {
            for (int a = 0; a < len; a++) {
                if (getDataFieldOfObject(rxBucket[i].type, rxBucket[i].index, (buffer[a] - 52) + 4) != pXSprite->data3)
                    return false;
            }
        }
    }
    
    return true;
}

void seqSpawnerOffSameTx(XSPRITE* pXSource) {

    int i, j;
    XSPRITE* pXSpr; spritetype* pSpr;

    for (i = 0; i < numsectors; i++)
    {
        for (j = headspritesect[i]; j >= 0; j = nextspritesect[j])
        {
            pSpr = &sprite[j];
            if (pSpr->type != kModernSeqSpawner)
                continue;

            pXSpr = (xspriRangeIsFine(pSpr->extra)) ? &xsprite[pSpr->extra] : NULL;
            if (!pXSpr || pXSpr->reference == pXSource->reference || !pXSpr->state || pXSpr->txID != pXSource->txID)
                continue;

            evKill(j, OBJ_SPRITE);
            pXSpr->state = 0;
        }
    }
}

// this function can be called via sending numbered command to TX kChannelModernEndLevelCustom
// it allows to set custom next level instead of taking it from INI file.
void levelEndLevelCustom(int nLevel) {

    gGameOptions.uGameFlags |= kGameFlagContinuing;

    if (!rngok(nLevel, 0, kMaxLevels))
    {
        gGameOptions.uGameFlags |= kGameFlagEnding;
        gGameOptions.nLevel = 0;
        return;
    }

    gNextLevel = nLevel;
}

void callbackUniMissileBurst(int nSprite) // 22
{
    dassert(nSprite >= 0 && nSprite < kMaxSprites);
    if (sprite[nSprite].statnum != kStatProjectile) return;
    spritetype* pSprite = &sprite[nSprite];
    int nAngle = getangle(xvel[nSprite], yvel[nSprite]);
    int nRadius = 0x55555;

    for (int i = 0; i < 8; i++)
    {
        spritetype* pBurst = actSpawnSprite(pSprite, 5);

        pBurst->type = pSprite->type;
        pBurst->shade = pSprite->shade;
        pBurst->picnum = pSprite->picnum;

        pBurst->cstat = pSprite->cstat;
        if ((pBurst->cstat & CSTAT_SPRITE_BLOCK)) {
            pBurst->cstat &= ~CSTAT_SPRITE_BLOCK; // we don't want missiles impact each other
            evPost(pBurst->index, 3, 100, kCallbackMissileSpriteBlock); // so set blocking flag a bit later
        }

        pBurst->pal = pSprite->pal;
        pBurst->clipdist = pSprite->clipdist / 4;
        pBurst->flags = pSprite->flags;
        pBurst->xrepeat = pSprite->xrepeat / 2;
        pBurst->yrepeat = pSprite->yrepeat / 2;
        pBurst->ang = ((pSprite->ang + missileInfo[pSprite->type - kMissileBase].angleOfs) & 2047);
        pBurst->owner = pSprite->owner;

        actBuildMissile(pBurst, pBurst->extra, pSprite->index);

        int nAngle2 = (i << 11) / 8;
        int dx = 0;
        int dy = mulscale30r(nRadius, Sin(nAngle2));
        int dz = mulscale30r(nRadius, -Cos(nAngle2));
        if (i & 1)
        {
            dy >>= 1;
            dz >>= 1;
        }
        RotateVector(&dx, &dy, nAngle);
        xvel[pBurst->index] += dx;
        yvel[pBurst->index] += dy;
        zvel[pBurst->index] += dz;
        evPost(pBurst->index, 3, 960, kCallbackRemove);
    }
    evPost(nSprite, 3, 0, kCallbackRemove);
}


void callbackMakeMissileBlocking(int nSprite) // 23
{
    dassert(nSprite >= 0 && nSprite < kMaxSprites);
    if (sprite[nSprite].statnum != kStatProjectile) return;
    sprite[nSprite].cstat |= CSTAT_SPRITE_BLOCK;
}

void clampSprite(spritetype* pSprite, int which) {

    int zTop, zBot;
    if (pSprite->sectnum >= 0 && pSprite->sectnum < kMaxSectors) {

        GetSpriteExtents(pSprite, &zTop, &zBot);
        if (which & 0x01)
            pSprite->z += ClipHigh(getflorzofslope(pSprite->sectnum, pSprite->x, pSprite->y) - zBot, 0);
        if (which & 0x02)
            pSprite->z += ClipLow(getceilzofslope(pSprite->sectnum, pSprite->x, pSprite->y) - zTop, 0);

    }

}

void killEvents(int nRx, int nCmd)
{
    int i;
    for (i = bucketHead[nRx]; i < bucketHead[nRx + 1]; i++)
    {
        if (nCmd == kCmdEventKillFull)
            evKill(rxBucket[i].index, rxBucket[i].type);
    }
}

void triggerTouchSprite(spritetype* pSprite, int nHSprite)
{
    if (spriRangeIsFine(nHSprite) && xspriRangeIsFine(sprite[nHSprite].extra))
    {
        XSPRITE* pXHSprite = &xsprite[sprite[nHSprite].extra];
        if (pXHSprite->Touch && !pXHSprite->isTriggered && (!pXHSprite->DudeLockout || IsPlayerSprite(pSprite)))
            trTriggerSprite(nHSprite, pXHSprite, kCmdSpriteTouch, pSprite->index);

        // enough to reset gSpriteHit values
        xvel[pSprite->index] += 5;
    }
}

void triggerTouchWall(spritetype* pSprite, int nHWall)
{
    if (wallRangeIsFine(nHWall) && xwallRangeIsFine(wall[nHWall].extra))
    {
        XWALL* pXHWall = &xwall[wall[nHWall].extra];
        if (pXHWall->triggerTouch && !pXHWall->isTriggered && (!pXHWall->dudeLockout || IsPlayerSprite(pSprite)))
            trTriggerWall(nHWall, pXHWall, kCmdWallTouch, pSprite->index);

        // enough to reset gSpriteHit values
        xvel[pSprite->index] += 5;
    }
}

void changeSpriteAngle(spritetype* pSpr, int nAng)
{
    if (!IsDudeSprite(pSpr))
        pSpr->ang = nAng;
    else
    {
        PLAYER* pPlayer = getPlayerById(pSpr->type);
        if (pPlayer)
            pPlayer->q16ang = fix16_from_int(nAng);
        else
        {
            pSpr->ang = nAng;
            if (xsprIsFine(pSpr))
                xsprite[pSpr->extra].goalAng = pSpr->ang;
        }
    }
}

int getVelocityAngle(spritetype* pSpr)
{
    return getangle(xvel[pSpr->index] >> 12, yvel[pSpr->index] >> 12);
}

void killEffectGenCallbacks(int oId, int oType = OBJ_SPRITE)
{
    int l = sizeof(gEffectGenCallbacks) / sizeof(gEffectGenCallbacks[0]);
    while (--l >= 0)
        evKill(oId, oType, (CALLBACK_ID)gEffectGenCallbacks[l]);
}

void killEffectGenCallbacks(XSPRITE* pXSource)
{
    int i, j;
    if (pXSource->data2 < kEffectGenCallbackBase)
        return;

    switch (pXSource->txID)
    {
        case kChannelZero:  // self 
            killEffectGenCallbacks(pXSource->reference);
            break;
        case kChannelAllPlayers: // player sprites
            for (i = connecthead; i >= 0; i = connectpoint2[i])
            {
                if (gPlayer[i].nSprite >= 0)
                    killEffectGenCallbacks(gPlayer[i].nSprite);
            }
            break;
        case kChannelEventCauser: // worst case...
            for (i = 0; i < numsectors; i++)
            {
                for (j = headspritesect[i]; j >= 0; j = nextspritesect[j])
                    killEffectGenCallbacks(j);
            }
            break;
        default:
            if (pXSource->txID >= kChannelUser) // TX ID scope
            {
                for (i = bucketHead[pXSource->txID]; i < bucketHead[pXSource->txID + 1]; i++)
                {
                    if (rxBucket[i].type == OBJ_SPRITE)
                        killEffectGenCallbacks(rxBucket[i].index);
                }
            }
            else if (rngok(pXSource->txID, kChannelPlayer0, kChannelPlayer0 + kMaxPlayers - 1))
            {
                // player sprites
                PLAYER* pPlayer = getPlayerById(pXSource->txID - kChannelPlayer0);
                if (pPlayer && pPlayer->nSprite >= 0)
                    killEffectGenCallbacks(pPlayer->nSprite);
            }
            break;

    }
}

bool xsprIsFine(spritetype* pSpr)
{
    if (pSpr && xspriRangeIsFine(pSpr->extra) && !(pSpr->flags & kHitagFree))
    {
        if (!(pSpr->flags & kHitagRespawn) || (pSpr->statnum != kStatThing && pSpr->statnum != kStatDude))
            return true;
    }

    return false;
}

void nnExtOffsetPos(int oX, int oY, int oZ, int nAng, int* x, int* y, int* z)
{
    // left, right
    if (oX)
    {
        if (x) *x -= mulscale30(oX, Cos(nAng + kAng90));
        if (y) *y -= mulscale30(oX, Sin(nAng + kAng90));
    }

    // forward, backward
    if (oY)
    {
        if (x) *x += mulscale30r(Cos(nAng), oY);
        if (y) *y += mulscale30r(Sin(nAng), oY);
    }

    // top, bottom
    if (oZ && z)
        *z += oZ;

}

char nnExtOffsetSprite(spritetype* pSpr, int oX, int oY, int oZ)
{
    int x = pSpr->x;    int z = pSpr->z;
    int y = pSpr->y;    int s = pSpr->sectnum;

    nnExtOffsetPos(oX, oY, oZ, pSpr->ang, &x, &y, &z);

    if (s >= 0)
    {
        if (FindSector(x, y, z, &s) || FindSector(x, y, &s))
        {
            if (s != pSpr->sectnum)
                ChangeSpriteSect(pSpr->index, s);

            pSpr->x = x;
            pSpr->y = y;
            pSpr->z = z;

            return true;
        }
    }

    return false;
}



void nnExtScaleVelocity(spritetype* pSpr, int nVel, int dx, int dy, int dz, char which)
{
    if (which & 0x01)
    {
        xvel[pSpr->index] = mulscale14(nVel, dx);
        yvel[pSpr->index] = mulscale14(nVel, dy);
    }

    if (which & 0x02)
    {
        zvel[pSpr->index] = mulscale14(nVel, dz);
    }
}


void nnExtScaleVelocityRel(spritetype* pSpr, int nVel, int dx, int dy, int dz, char which)
{
    if (which & 0x01)
    {
        xvel[pSpr->index] += mulscale14(nVel, dx);
        yvel[pSpr->index] += mulscale14(nVel, dy);
    }

    if (which & 0x02)
    {
        zvel[pSpr->index] += mulscale14(nVel, dz);
    }
}

#ifdef NNEXTS_USE_RES_SYS
int nnExtResAddExternalFiles(Resource* pIn, const char* pPath, EXTERNAL_FILES_LIST* pList, int nLen)
{
    char match[BMAX_PATH], dir[BMAX_PATH], name[BMAX_PATH], ext[BMAX_PATH];
    EXTERNAL_FILES_LIST* ptr; BDIR* pDir; Bdirent* pEntry;
    int i, nRetn = 0;
    char* pMatch;

    for (i = 0; i < nLen; i++)
    {
        ptr = &pList[i];

        Bmemset(match, 0, sizeof(match)); pMatch = match;
        pMatch += Bsprintf(pMatch, "%s",  (ptr->name) ? ptr->name : "*");
        pMatch += Bsprintf(pMatch, ".%s", (ptr->ext) ? ptr->ext   : "*");

        if ((pDir = Bopendir(pPath)) != NULL)
        {
            while ((pEntry = Breaddir(pDir)) != NULL)
            {
                if (Bwildmatch(pEntry->name, match))
                {
                    SplitPath(pEntry->name, dir, name, ext);
                    pIn->AddExternalResource(name, ext);
                    nRetn++;
                }
            }

            Bclosedir(pDir);
        }
    }

    return nRetn;
}

DICTNODE* nnExtResFileSearch(Resource* pIn, const char* pName, const char* pExt, char external)
{
    int nLenA = Bstrlen(pName), nLenB;
    int i = pIn->count;
    DICTNODE* pFile;

    while(--i >= 0)  // from bottom we can meet it faster
    {
        pFile = &pIn->dict[i];
        if ((external && (pFile->flags & DICT_EXTERNAL)) || !external)
        {
            if (Bstrcasecmp(pFile->type, pExt) == 0 && Bstrncasecmp(pName, pFile->name, nLenA) == 0)
            {
                nLenB = Bstrlen(pFile->name);
                if (nLenB == nLenA || (nLenB > nLenA && pFile->name[nLenA] == '_'))
                {
                    //consoleSysMsg("FILE: %s / %s.%s", pFile->name, pName, pExt);
                    return pFile;
                }
            }
        }
    }

    return NULL;
}
#endif

void nnExtSprScaleSet(spritetype* pSpr, int nScale)
{
    pSpr->xrepeat = ClipRange(mulscale8(pSpr->xrepeat, nScale), 0, 255);
    pSpr->yrepeat = ClipRange(mulscale8(pSpr->yrepeat, nScale), 0, 255);
}

void nnExtCoSin(int nAng, int* x, int* y, int nShift)
{
    *x = Cos(nAng) >> nShift;
    *y = Sin(nAng) >> nShift;
}

bool isUnderwaterSector(XSECTOR* pXSect) { return pXSect->Underwater; }
bool isUnderwaterSector(sectortype* pSect) { return (pSect->extra > 0 && isUnderwaterSector(&xsector[pSect->extra])); }
bool isUnderwaterSector(int nSector) { return isUnderwaterSector(&sector[nSector]); }

bool isMovableSector(int nType)
{
    return (nType && nType != kSectorDamage && nType != kSectorTeleport && nType != kSectorCounter);
}

bool isMovableSector(sectortype* pSect)
{
    if (isMovableSector(pSect->type) && pSect->extra > 0)
    {
        XSECTOR* pXSect = &xsector[pSect->extra];
        return (pXSect->busy && !pXSect->unused1);
    }

    return false;
}

char spriteIsUnderwater(spritetype* pSprite, char oldWay)
{
    return ((pSprite->sectnum >= 0 && (sector[pSprite->sectnum].extra >= 0 && xsector[sector[pSprite->sectnum].extra].Underwater))
        || (oldWay && (xsprite[pSprite->extra].medium == kMediumWater || xsprite[pSprite->extra].medium == kMediumGoo)));
}

int getSpritesNearWalls(int nSrcSect, int* spriOut, int nMax, int nDist)
{
    int i, j, c = 0, nWall, nSect, swal, ewal;
    int xi, yi, wx, wy, lx, ly, sx, sy, qx, qy, num, den;
    int* skip = (int*)Bmalloc(sizeof(int) * kMaxSprites);
    if (!skip)
        return c;

    Bmemset(skip, 0, sizeof(int) * kMaxSprites);
    swal = sector[nSrcSect].wallptr;
    ewal = swal + sector[nSrcSect].wallnum - 1;

    for (i = swal; i <= ewal; i++)
    {
        nSect = wall[i].nextsector;
        if (nSect < 0)
            continue;

        nWall = i;
        wx = wall[nWall].x;	wy = wall[nWall].y;
        lx = wall[wall[nWall].point2].x - wx;
        ly = wall[wall[nWall].point2].y - wy;

        for (j = headspritesect[nSect]; j >= 0; j = nextspritesect[j])
        {
            if (skip[j])
                continue;

            sx = sprite[j].x;	qx = sx - wx;
            sy = sprite[j].y;	qy = sy - wy;
            num = dmulscale4(qx, lx, qy, ly);
            den = dmulscale4(lx, lx, ly, ly);

            if (num > 0 && num < den)
            {
                xi = wx + scale(lx, num, den);
                yi = wy + scale(ly, num, den);
                if (approxDist(xi - sx, yi - sy) <= nDist)
                {
                    skip[j] = 1; spriOut[c] = j;
                    if (++c == nMax)
                    {
                        Bfree(skip);
                        return c;
                    }
                }
            }
        }
    }
    
    Bfree(skip);
    return c;
}

int getDigitFromValue(int nVal, int nOffs)
{
    char t[16];
    int l = Bsprintf(t, "%d", abs(nVal));
    if (nOffs < l && rngok(t[nOffs], 48, 58))
        return t[nOffs] - 48;

    return -1;
}

bool isOnRespawn(spritetype* pSpr)
{
    if (pSpr->flags & kHitagRespawn)
    {
        switch (pSpr->statnum)
        {
            case kStatDude:
            case kStatThing:
            case kStatItem:
                return true;
        }
    }

    return false;
}

bool seqCanOverride(Seq* pSeq, int nFrame, bool* xrp, bool* yrp, bool* plu)
{
    SEQFRAME* pFrame;
    *xrp = *yrp = *plu = true;
    if (!pSeq)
        return true;

    while (nFrame < pSeq->nFrames)
    {
        pFrame = &pSeq->frames[nFrame++];
        if (pFrame->xrepeat) *xrp = false;
        if (pFrame->yrepeat) *yrp = false;
        if (pFrame->pal)     *plu = false;
        // TO DO: ...add check for cstat
    }

    return (*xrp && *yrp && *plu);
}

void getRxBucket(int nChannel, int* nStart, int* nEnd, RXBUCKET** pRx)
{
    *nStart = bucketHead[nChannel];
    *nEnd = bucketHead[nChannel + 1];

    if (pRx)
        *pRx = &rxBucket[*nStart];

}

int nnExtGibSprite(spritetype* pSpr, IDLIST* pOut, GIBTYPE nGibType, CGibPosition* pPos, CGibVelocity* pVel)
{
    int i;
    spritetype* pEff;
    int nSect = pSpr->sectnum;
    int nSectBackup = nSect;

    if (nSect < 0)
        return 0;

    if (pPos && FindSector(pPos->x, pPos->y, &nSect))
        pSpr->sectnum = nSect;

    // collect all potential effect sprites in this sector
    for (i = headspritesect[pSpr->sectnum]; i >= 0; i = nextspritesect[i])
    {
        pEff = &sprite[i];
        if (pEff->owner == pSpr->index || (pEff->flags & kHitagFree)) continue;
        else if (pEff->statnum == kStatFX || pEff->statnum == kStatThing)
            pOut->Add(pEff->index);
    }

    // create new sprites
    GibSprite(pSpr, (GIBTYPE)nGibType, pPos, pVel);

    // only new sprites must left
    for (i = headspritesect[pSpr->sectnum]; i >= 0; i = nextspritesect[i])
    {
        pEff = &sprite[i];
        if (pEff->flags & kHitagFree)  continue;
        else if (pEff->statnum != kStatFX && pEff->statnum != kStatThing) continue;
        else if (pEff->owner != pSpr->index)
        {
            if (pOut->Exists(pEff->index)) pOut->Remove(pEff->index);
            else pOut->Add(pEff->index);
        }
    }

    if (pPos)
        pSpr->sectnum = nSectBackup;

    return pOut->Length();
    
}

int nnExtDudeStartHealth(spritetype* pSpr, int nHealth)
{
    if (nHealth)
        return ClipRange(nHealth << 4, 0, 65535);

    return getDudeInfo(pSpr->type)->startHealth << 4;
}

void nnExtFixDudeDrag(spritetype* pSpr, int nDrag)
{
    int fz, zt, zb, d;
    if (xvel[pSpr->index] || yvel[pSpr->index])
    {
        fz = getflorzofslope(pSpr->sectnum, pSpr->x, pSpr->y);
        GetSpriteExtents(pSpr,&zt,&zb);
        
        if (((fz - zb) >> 8) >= 0x100)
        {
            d = gDudeDrag - scale(gDudeDrag, nDrag, 0x100);
            xvel[pSpr->index] -= mulscale16r(xvel[pSpr->index], d);
            yvel[pSpr->index] -= mulscale16r(yvel[pSpr->index], d);
            if (approxDist(xvel[pSpr->index], yvel[pSpr->index]) < 0x1000)
                xvel[pSpr->index] = yvel[pSpr->index] = 0;
        }
    }
}

char IsPhysicsSprite(spritetype* pSpr)
{
    if (pSpr->extra > 0)
    {
        if (xsprite[pSpr->extra].physAttr)
            return 2;

        if (pSpr->statnum == kStatThing || pSpr->statnum == kStatDude)
            if (pSpr->flags & (kPhysMove | kPhysGravity))
                return 1;
    }

    return 0;
}

void nnExtTrInit()
{
    gSprNSect.Init(); // collect sprites near outside walls
    pathSpriteInit();
}

// Functions to handle lasers
///////////////////////////////////////////////////
void lasersInit()
{
    spritetype* pSpr; LASER* pLaser;
    int i = numsectors;
    int j;

    lasersClear();
    LASER::GenerateTiles(0);
    LASER::GenerateTables();

    while(--i >= 0)
    {
        for (j = headspritesect[i]; j >= 0; j = nextspritesect[j])
        {
            pSpr = &sprite[j];
            if (pSpr->type != kModernLaserGen)
                continue;

            dassert(gLasersCount < kMaxLasers);

            if (pSpr->extra <= 0)
                dbInsertXSprite(pSpr->index);

            gLaser[gLasersCount] = new LASER(pSpr);

            pLaser = gLaser[gLasersCount];
            pLaser->pXOwn->sysData1 = gLasersCount++;
        }
    }
}

void lasersClear()
{
    int i = gLasersCount;
    while(--i >= 0)
        DELETE_AND_NULL(gLaser[i]);

    gLasersCount = 0;
}

void lasersProcess()
{
    int i = gLasersCount;
    LASER* pLaser;

    while(--i >= 0)
    {
        pLaser = gLaser[i];
        if (pLaser->pXOwn->state)
            pLaser->Process();
    }
}

void lasersProcessView3D(int camx, int camy, int camz, int cama, int cams, int camh)
{
    int i = numsectors, j;
    char cameraSet = 0;
    LASER* pLaser;

    while(--i >= 0)
    {
        if (!TestBitString(gotsector, i))
                continue;

        if (!cameraSet)
        {
            LASER::cam.x = camx;
            LASER::cam.y = camy;
            LASER::cam.z = camz;
            LASER::cam.s = cams;
            LASER::cam.a = cama;
            LASER::cam.h = camh;
            cameraSet = 1;
        }

        j = gLasersCount;
        while(--j >= 0)
        {
            pLaser = gLaser[j];
            if (!pLaser->pXOwn->state) continue;
            else if (!pLaser->RayShow(i))
                return;
        }
    }
}

LASER* laserGet(spritetype* pSpr)
{
    dassert(pSpr->extra > 0);
    return gLaser[xsprite[pSpr->extra].sysData1];
}

void laserSave(LoadSave* pSave)
{
    int32_t i = gLasersCount;
    LASER* pLaser;

    // total number of lasers
    pSave->Write(&i, sizeof(i));

    for (i = 0; i < gLasersCount; i++)
    {
        pLaser = gLaser[i];

        // save oldscan hitsprite to avoid triggering it again after loading a save
        pSave->Write(&pLaser->oldscan.hitsprite, sizeof(pLaser->oldscan.hitsprite));
    }
}

void laserLoad(LoadSave* pSave)
{
    LASER* pLaser;
    int32_t i, t;

    // total number of lasers
    pSave->Read(&t, sizeof(t));
    dassert(t == gLasersCount);

    for (i = 0; i < t; i++)
    {
        pLaser = gLaser[i];

        // restore oldscan hitsprite
        pSave->Read(&pLaser->oldscan.hitsprite, sizeof(pLaser->oldscan.hitsprite));
    }
}


// Functions to handle path sectors
///////////////////////////////////////////////////

void pathSpriteClear()
{
    int i = gPathSprCount;
    while(--i >= 0)
    {
        if (gPathSprList[i])
            DELETE_AND_NULL(gPathSprList[i]);
    }

    gPathSprCount = 0;
}

void pathSpriteInit()
{
    XSPRITE *pXFirst = NULL, *pXNext, *pXPrev;
    sectortype* pSect; XSECTOR* pXSect;
    int i = numsectors, j, dz;
    IDLIST* pList;

    pathSpriteClear();

    while(--i >= 0)
    {
        pSect = &sector[i];
        if (pSect->type != kModernSectorPathSprite)
            continue;

       dassert(gPathSprCount < kMaxPathSpriteSectors);

        pXSect = &xsector[pSect->extra];
        gPathSprList[gPathSprCount] = new IDLIST(true);
        pList = gPathSprList[gPathSprCount];

        for (j = headspritesect[i]; j >= 0; j = nextspritesect[j])
        {
            spritetype *pSpr = &sprite[j];
            if (pSpr->statnum == kStatMarker || pSpr->statnum == kStatPathMarker)
                continue;

            pList->Add(j);
        }

        if ((pXFirst = pathSectFindNextMarker(pXSect)) != NULL)
        {
            dz = pSect->floorz - sprite[pXFirst->reference].z;
            pXNext = pXFirst;

            do
            {
                // offset all markers relative to first marker
                // so sprites won't get weird positions
                // on sector activation

                sprite[pXNext->reference].z += dz;
                pXPrev = pXNext;
            }
            while((pXNext = pathSectFindNextMarker(pXSect, pXNext)) != NULL && pXNext != pXPrev && pXNext != pXFirst);

            pXSect->marker0     = pXFirst->reference;
            basePath[i]         = pXFirst->reference;
            gPathSprIndex[i]    = gPathSprCount++;

            if (pXSect->state)
                evPost(i, 6, 0, kCmdOn, kCauserGame);
        }
        else
        {
            ThrowError("Unable to find path marker with id #%d for path sprite sector #%d", pXSect->data, i);
            DELETE_AND_NULL(gPathSprList[gPathSprCount]);
        }
    }
}

void pathSpriteOperate(unsigned int nSect, XSECTOR *pXSect, EVENT event)
{
    spritetype* pFirst  = &sprite[basePath[nSect]];
    XSPRITE* pXFirst    = &xsprite[pFirst->extra];
    spritetype* pThis   = &sprite[pXSect->marker0];
    XSPRITE* pXThis     = &xsprite[pThis->extra];
    XSPRITE* pXNext     = NULL;

    if (event.cmd >= kCmdNumberic)
    {
        switch (event.cmd - kCmdNumberic)
        {
            case kCmdOff:       pXFirst->sysData4  = 0;  break;
            case kCmdOn:        pXFirst->sysData4  = 1;  break;
            case kCmdToggle:    pXFirst->sysData4 ^= 1;  break;
        }
    }

    if ((pXNext = pathSectFindNextMarker(pXSect, pXThis, pXFirst->sysData4 == 0)) == NULL)
    {
        ThrowError("Unable to find path marker with id #%d for path sector #%d", pXSect->data, nSect);
        return;
    }

    if (!pXSect->state && pXSect->triggerOff)
    {
        trTriggerSprite(pThis->index, pXThis, pXSect->command, kCauserGame); // trigger marker at departure
    }

    pXSect->state   = 0;
    pXSect->busy    = 0;

    if (pXNext != pXThis)
    {
        pXSect->marker1 = pXNext->reference;

        switch (event.cmd)
        {
            case kCmdOn:
                AddBusy(nSect, BUSY_PATHSPRITE, 65536 / ClipLow(EVTIME2TICKS(pXThis->busyTime), 1));
                SectorStartSound(nSect, 1);
                if (pXThis->data3)
                    PathSound(nSect, pXThis->data3);

                break;
        }
    }
}

void pathSpriteFixSector(spritetype* pSpr)
{
    int s;

    if (pSpr->type == kSoundSector)
    {
        // we don't change sector for this because
        // it seems like it doesn't matter
        // for sound

        return;
    }

    s = pSpr->sectnum;
    if (!FindSector(pSpr->x, pSpr->y, pSpr->z, &s))     // try 3D first
        if (!FindSector(pSpr->x, pSpr->y, &s))          // then 2D
            return;

    if (s != pSpr->sectnum)
        ChangeSpriteSect(pSpr->index, s);
}

char pathSpriteStaysOnSprite(IDLIST* pList, spritetype* pSpr)
{
    if (pSpr->extra > 0)
    {
        int nHit = gSpriteHit[pSpr->extra].florhit;
        if ((nHit & 0xC000) != 0xC000)
            return 0;

        int nSpr = nHit & 0x3FFF;
        if (pList->Exists(nSpr))
        {
            if (sprite[nSpr].cstat & 0x4000)
                return 2;

            return 1;
        }
    }

    return 0;
}

inline IDLIST* pathSpriteGetList(int nSect)
{
    return gPathSprList[gPathSprIndex[nSect]];
}

void pathSpriteTranslate(int nSect, int nBusyA, int nBusyB, int fX, int fY, int cX, int cY, int cZ, int cA, int nX, int nY, int nZ, int nA)
{
    XSECTOR *pXSect = &xsector[sector[nSect].extra];
    IDLIST* pList = pathSpriteGetList(nSect);
    spritetype *pSpr;
    int nSpr, x, y;
    int32_t* p;
    char r;

    int x1 = interpolate(cX, nX, nBusyA);
    int x2 = interpolate(cX, nX, nBusyB);
    int dx = x2 - x1;

    int y1 = interpolate(cY, nY, nBusyA);
    int y2 = interpolate(cY, nY, nBusyB);
    int dy = y2 - y1;

    int a1 = interpolate(cA, nA, nBusyA);
    int a2 = interpolate(cA, nA, nBusyB);
    int da = a2 - a1;

    int dz = nZ - cZ;
    int oz;

    if (dz)
    {
        oz = baseFloor[nSect];
        baseFloor[nSect] = cZ + mulscale16(dz, nBusyB);
        dz = baseFloor[nSect] - oz;
        velFloor[nSect] += (dz<<8);
    }

    for (p = pList->First(); *p >= 0; p++)
    {
        nSpr = *p;
        pSpr = &sprite[nSpr];
        x = baseSprite[nSpr].x;
        y = baseSprite[nSpr].y;

        if (pSpr->cstat & 0x2000)
        {
            if (a2)
                RotatePoint(&x, &y, a2, fX, fY);

            viewBackupSpriteLoc(nSpr, pSpr);
            pSpr->ang = (pSpr->ang+da) & kAngMask;
            pSpr->x = x+x2-fX;
            pSpr->y = y+y2-fY;
            pSpr->z += dz;
        }
        else if (pSpr->cstat & 0x4000)
        {
            if (a2)
                RotatePoint(&x, &y, -a2, fX, fY);

            viewBackupSpriteLoc(nSpr, pSpr);
            pSpr->ang = (pSpr->ang-da) & kAngMask;
            pSpr->x = x-(x2-fX);
            pSpr->y = y-(y2-fY);
            pSpr->z -= dz;
        }
        else if (pXSect->Drag)
        {
            if ((r = pathSpriteStaysOnSprite(pList, pSpr)) > 0)
            {
                switch(r)
                {
                    case 2:
                        if (da)
                            RotatePoint(&pSpr->x, &pSpr->y, -da, x1, y1);

                        viewBackupSpriteLoc(nSpr, pSpr);
                        pSpr->ang = (pSpr->ang-da) & kAngMask;
                        pSpr->x -= dx;
                        pSpr->y -= dy;
                        pSpr->z -= dz;
                        break;
                    default:
                        if (da)
                            RotatePoint(&pSpr->x, &pSpr->y, da, x1, y1);

                        viewBackupSpriteLoc(nSpr, pSpr);
                        pSpr->ang = (pSpr->ang+da) & kAngMask;
                        pSpr->x += dx;
                        pSpr->y += dy;
                        pSpr->z += dz;
                        break;
                }

                // Force movement process functions
                // to update touch data.

                if ((r = IsPhysicsSprite(pSpr)) == 2)
                {
                    xsprite[pSpr->extra].physAttr |= kPhysFalling;
                }
                else if (r == 1)
                    pSpr->flags |= kPhysFalling;
            }
        }

        pathSpriteFixSector(pSpr);
    }
}

int pathSpriteBusy(unsigned int nSect, unsigned int a2, int causerID)
{
    static BITSECTOR visited; spritetype* pSpr;
    int i, j, t; IDLIST* pList;
    int32_t* p;

    XSECTOR *pXSect     = &xsector[sector[nSect].extra];

    spritetype *pFirst  = &sprite[basePath[nSect]];
    spritetype *pThis   = &sprite[pXSect->marker0];
    spritetype *pNext   = &sprite[pXSect->marker1];
    XSPRITE *pXThis     = &xsprite[pThis->extra];
    XSPRITE *pXNext     = &xsprite[pNext->extra];

    // the hardest part is to add/remove sprites dynamically...

    memset(visited, 0, sizeof(visited));
    pList = pathSpriteGetList(nSect);
    i = pList->Length();
    p = pList->First();

    while(--i >= 0)
    {
        pSpr = &sprite[p[i]];
        if (pSpr->statnum >= kMaxStatus || pSpr->sectnum < 0)
        {
            pList->Remove(pSpr->index), p = pList->First();     // watch out for ptr!
            continue;
        }

        if (pXSect->Drag)
        {
            if (!(pSpr->cstat & 0x6000))
            {
                if (!pathSpriteStaysOnSprite(pList, pSpr))
                    pList->Remove(pSpr->index), p = pList->First();     // watch out for ptr!
            }

            t = pSpr->sectnum;
            if (!TestBitString(visited, t))
            {
                for (j = headspritesect[t]; j >= 0; j = nextspritesect[j])
                {
                    if (!pList->Exists(j) && pathSpriteStaysOnSprite(pList, &sprite[j]))
                        pList->Add(j);
                }

                SetBitString(visited, t);
                p = pList->First(); // watch out for ptr!
            }
        }
    }

    pathSpriteTranslate
    (
        nSect,
        GetWaveValue(pXSect->busy, pXThis->wave), GetWaveValue(a2, pXThis->wave),
        pFirst->x, pFirst->y,
        pThis->x, pThis->y, pThis->z, pThis->ang,
        pNext->x, pNext->y, pNext->z, pNext->ang
    );

    pXSect->busy = a2;
    if ((pXSect->busy & 0xffff) == 0)
    {
        if (!pXSect->state && pXSect->triggerOn) // trigger marker at arrival
            trTriggerSprite(pNext->index, pXNext, pXSect->command, kCauserGame);

        evPost(nSect, 6, EVTIME2TICKS(pXNext->waitTime), kCmdOn, causerID);
        pXSect->busy = pXSect->state = 0;
        SectorEndSound(nSect, 1);

        if (pXThis->data4)
            PathSound(nSect, pXThis->data4);

        pXSect->marker0     = pXSect->marker1;
        pXSect->data        = pXNext->data1;
        return 3;
    }

    return 0;
}

XSPRITE* pathSectFindNextMarker(XSECTOR* pXSect, XSPRITE *pXMark, char dir)
{
    spritetype* pSpr; XSPRITE* pXSpr;
    int nID, i;

    nID = (pXMark != NULL) ? ((dir) ? pXMark->data2 : pXMark->data1) : pXSect->data;

    for (i = headspritestat[kStatPathMarker]; i >= 0; i = nextspritestat[i])
    {
        pSpr = &sprite[i];
        if (!xspriRangeIsFine(pSpr->extra))
            continue;

        pXSpr = &xsprite[pSpr->extra];
        if (!pXSpr->locked && !pXSpr->isTriggered)
        {
            if ((dir && pXSpr->data1 == nID) || (!dir && pXSpr->data2 == nID))
                return pXSpr;
        }
    }

    return NULL;
}

void pathSpriteSave(LoadSave* pSave)
{
    int32_t i, *p;
    IDLIST* pList;

    // total number of sectors
    pSave->Write(&gPathSprCount, sizeof(gPathSprCount));

    for (i = 0; i < numsectors; i++)
    {
        if (sector[i].type == kModernSectorPathSprite)
        {
            pSave->Write(&gPathSprIndex[i], sizeof(gPathSprIndex[i]));
            pList = pathSpriteGetList(i);
            p = pList->First();

            // writes -1 in the end
            do pSave->Write(&*p, sizeof(int32_t));
            while (*p++ >= 0);
        }
    }
}

void pathSpriteLoad(LoadSave* pSave)
{
    IDLIST* pList; int32_t i, j;

    pathSpriteClear();

    // total number of sectors
    pSave->Read(&gPathSprCount, sizeof(gPathSprCount));

    for (i = 0; i < numsectors; i++)
    {
        if (sector[i].type == kModernSectorPathSprite)
        {
            pSave->Read(&gPathSprIndex[i], sizeof(gPathSprIndex[i]));
            j = gPathSprIndex[i], gPathSprList[j] = pList = new IDLIST(true);

            while ( 1 )
            {
                pSave->Read(&j, sizeof(j));
                if (j == -1)
                    break;

                pList->Add(j); // keep adding sprites
            }
        }
    }
}

char scanWallOfSector(SCANWALL* pIn, SCANWALL* pOut)
{
    vec3_t pos = { pIn->pos.x, pIn->pos.y, pIn->pos.z };
    sectortype* pSect = &sector[pIn->s];
    int nSect = pIn->s;
    hitdata_t hit;

    static struct
    {
        int fslope, fstat;
        int cslope, cstat;
        int goalx, goaly;
        int headspr;
        int fz, cz;
    }
    backup;

    backup.fslope   = pSect->floorheinum;
    backup.cslope   = pSect->ceilingheinum;
    backup.fstat    = pSect->floorstat;
    backup.cstat    = pSect->ceilingstat;
    backup.fz       = pSect->floorz;
    backup.cz       = pSect->ceilingz;
    backup.goalx    = hitscangoal.x;
    backup.goaly    = hitscangoal.y;
    backup.headspr  = headspritesect[nSect];

    pSect->floorz       = 0x7ffffff >> 3;
    pSect->ceilingz     = pSect->floorz - 8192;
    pSect->floorheinum  = pSect->ceilingheinum = 0;
    pSect->floorstat    = pSect->ceilingstat = 0;

    hitscangoal.x = hitscangoal.y = 0x1fffffff;
    headspritesect[nSect] = -1;
    pOut->w = -1;

    pos.z = pSect->floorz - 4096;

    hitscan
    (
        &pos, nSect,
        Cos(pIn->pos.a)>>16, Sin(pIn->pos.a)>>16, 0,
        &hit,
        CLIPMASK0
    );

    pSect->floorheinum  = backup.fslope;
    pSect->ceilingheinum = backup.cslope;
    pSect->floorstat    = backup.fstat;
    pSect->ceilingstat  = backup.cstat;
    pSect->floorz       = backup.fz;
    pSect->ceilingz     = backup.cz;

    hitscangoal.x = backup.goalx; hitscangoal.y = backup.goaly;
    headspritesect[nSect] = backup.headspr;

    pOut->w = hit.wall;
    pOut->s = hit.sect;

    if (pOut->w >= 0)
    {
        pOut->pos.New((GetWallAngle(pOut->w) + kAng90) & kAngMask, hit.x, hit.y, pIn->pos.z);
        return 1;
    }

    pOut->pos.New(pIn->pos.a, hit.x, hit.y, pIn->pos.z);
    return 0;
}


class nnExtLoadSave : public LoadSave
{
    const char* nnExtBlkSign[2] =
    {
        "nnExt>", // block starts
        "<nnExt", // block ends
    };

    virtual void Load(void);
    virtual void Save(void);
};

void viewCameraLoad(LoadSave* pSave)
{
    int const nLen = sizeof(VIEWCAMERA) - sizeof(PLAYER);
    unsigned char* p = (unsigned char*)&gViewCam;
    p += sizeof(PLAYER);

    pSave->Read(p, nLen);

    VIEWCAMERA* pCam = &gViewCam;
    if (gView && pCam->active)
    {
        spritetype* pCSpr = spriRangeIsFine(pCam->nCamSpr) ? &sprite[pCam->nCamSpr] : NULL;
        spritetype* pTSpr = spriRangeIsFine(pCam->nTrackSpr) ? &sprite[pCam->nTrackSpr] : NULL;
        spritetype* pCSrc = spriRangeIsFine(pCam->nCamSrc) ? &sprite[pCam->nCamSrc] : NULL;
        XSPRITE* pXCSrc = (pCSrc != NULL && xsprIsFine(pCSrc)) ? &xsprite[pCSrc->extra] : NULL;

        useViewCamera(pXCSrc, pCSpr, pTSpr, gView);
    }
}

void nnExtLoadSave::Load(void)
{
    char tmp[32];
    if (!gModernMap)
        return;

    memset(tmp, 0, sizeof(tmp));
    Read(tmp, Bstrlen(nnExtBlkSign[0]));
    gSprNSect.Load(this);
    laserLoad(this);
    pathSpriteLoad(this);
    cdudeLoad(this);
    viewCameraLoad(this);
    Read(&gCustomMusicStatus, sizeof(gCustomMusicStatus));
    Read(tmp, Bstrlen(nnExtBlkSign[1]));
}

void viewCameraSave(LoadSave* pSave)
{
    int const nLen = sizeof(VIEWCAMERA) - sizeof(PLAYER);
    unsigned char* p = (unsigned char*)&gViewCam;
    p += sizeof(PLAYER);

    pSave->Write(p, nLen);
}

void nnExtLoadSave::Save(void)
{
    if (!gModernMap)
        return;

    Write(nnExtBlkSign[0], strlen(nnExtBlkSign[0]));
    gSprNSect.Save(this);
    laserSave(this);
    pathSpriteSave(this);
    cdudeSave(this);
    viewCameraSave(this);
    Write(&gCustomMusicStatus, sizeof(gCustomMusicStatus));
    Write(nnExtBlkSign[1], strlen(nnExtBlkSign[1]));
}

static nnExtLoadSave* myLoadSave;
void nnExtLoadSaveConstruct(void)
{
    myLoadSave = new nnExtLoadSave();
}
#endif

///////////////////////////////////////////////////////////////////
// This file provides modern features for mappers.
// For full documentation please visit http://cruo.bloodgame.ru/xxsystem
///////////////////////////////////////////////////////////////////