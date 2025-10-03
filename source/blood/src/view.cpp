//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 Nuke.YKT

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
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "a.h"
#include "build.h"
#include "colmatch.h"
#include "pragmas.h"
#include "mmulti.h"
#include "osd.h"
#include "common_game.h"

#include "aihand.h"
#include "blood.h"
#include "choke.h"
#include "config.h"
#include "db.h"
#include "demo.h"
#include "endgame.h"
#include "gamemenu.h"
#include "gameutil.h"
#include "globals.h"
#include "gfx.h"
#include "levels.h"
#include "loadsave.h"
#include "map2d.h"
#include "messages.h"
#include "menu.h"
#include "mirrors.h"
#include "network.h"
#include "player.h"
#include "replace.h"
#include "screen.h"
#include "sectorfx.h"
#include "tile.h"
#include "trig.h"
#include "view.h"
#include "warp.h"
#include "weapon.h"
#ifdef NOONE_EXTENSIONS
#include "nnexts.h"
#endif

struct VIEW {
    int at0;
    int at4;
    int at8; // bob height
    int atc; // bob width
    int at10;
    int at14;
    int at18; // bob sway y
    int at1c; // bob sway x
    fix16_t at20;
    fix16_t at24; // horiz
    int at28; // horizoff
    int at2c;
    fix16_t at30; // angle
    int at34; // weapon z
    int at38; // view z
    int at3c;
    int at40;
    int at44;
    int at48; // posture
    int at4c; // spin
    int at50; // x
    int at54; // y
    int at58; // z
    int at5c; //xvel
    int at60; //yvel
    int at64; //zvel
    short at68; // sectnum
    unsigned int at6a; // floordist
    char at6e; // look center
    char at6f;
    char at70; // run
    char at71; // jump
    char at72; // underwater
    short at73; // sprite flags
    SPRITEHIT at75;
};

VIEW gPrevView[kMaxPlayers];
VIEWPOS gViewPos;
int gViewIndex;

struct INTERPOLATE {
    void *pointer;
    int value;
    int value2;
    INTERPOLATE_TYPE type;
};

int pcBackground;
int gViewMode = 3;
int gViewSize = 2;

bool gPrediction = true;

VIEW predict, predictOld;

VIEW predictFifo[256];

int gInterpolate;
int nInterpolations;
int nInterpolationsPanning;
char gInterpolateSprite[bitmap_size(kMaxSprites)];
char gInterpolateWall[bitmap_size(kMaxWalls)];
char gInterpolateSector[bitmap_size(kMaxSectors)];
char gInterpolatePanningWall[bitmap_size(kMaxWalls)];
char gInterpolatePanningCeiling[bitmap_size(kMaxSectors)];
char gInterpolatePanningFloor[bitmap_size(kMaxSectors)];

#define kMaxInterpolationsPanning 1024 // dedicate number of interpolations to texture panning
#define kMaxInterpolations (16384+kMaxInterpolationsPanning)

INTERPOLATE gInterpolation[kMaxInterpolations];

int gViewXCenter, gViewYCenter;
int gViewX0, gViewY0, gViewX1, gViewY1;
int gViewX0S, gViewY0S, gViewX1S, gViewY1S;
int xscale, xscalecorrect, yscale, xstep, ystep;

int gScreenTilt;

CGameMessageMgr gGameMessageMgr;

bool bLoadScreenCrcMatch = false;

void RotateYZ(int *pX, int *pY, int *pZ, int ang)
{
    UNREFERENCED_PARAMETER(pX);
    int oY, oZ, angSin, angCos;
    oY = *pY;
    oZ = *pZ;
    angSin = Sin(ang);
    angCos = Cos(ang);
    *pY = dmulscale30r(oY,angCos,oZ,-angSin);
    *pZ = dmulscale30r(oY,angSin,oZ,angCos);
}

void RotateXZ(int *pX, int *pY, int *pZ, int ang)
{
    UNREFERENCED_PARAMETER(pY);
    int oX, oZ, angSin, angCos;
    oX = *pX;
    oZ = *pZ;
    angSin = Sin(ang);
    angCos = Cos(ang);
    *pX = dmulscale30r(oX,angCos,oZ,-angSin);
    *pZ = dmulscale30r(oX,angSin,oZ,angCos);
}

void RotateXY(int *pX, int *pY, int *pZ, int ang)
{
    UNREFERENCED_PARAMETER(pZ);
    int oX, oY, angSin, angCos;
    oX = *pX;
    oY = *pY;
    angSin = Sin(ang);
    angCos = Cos(ang);
    *pX = dmulscale30r(oX,angCos,oY,-angSin);
    *pY = dmulscale30r(oX,angSin,oY,angCos);
}

FONT gFont[kFontNum];

void FontSet(int id, int tile, int space)
{
    if (id < 0 || id >= kFontNum || tile < 0 || tile >= kMaxTiles)
        return;

    FONT* pFont = &gFont[id];
    int yoff = 0;

    DICTNODE* hQFont = gSysRes.Lookup(id, "QFN");
    if (hQFont)
    {
        QFONT *pQFont = (QFONT*)gSysRes.Load(hQFont);
        for (int i = 32; i < 128; i++)
        {
            QFONTCHAR* pChar = &pQFont->at20[i];
            yoff = min(yoff, pQFont->atf + pChar->oy);
        }
        for (int i = 32; i < 128; i++)
        {
            int const nTile = tile + i - 32;
            if (waloff[nTile])
                continue;
            QFONTCHAR *pChar = &pQFont->at20[i];
            int const width = max(pChar->w, pChar->ox);
            int const height = max(pQFont->atf+pChar->oy+pChar->h-yoff, 1);
            char *tilePtr = (char*)tileCreate(nTile, width, height);
            if (!tilePtr)
                continue;
            Bmemset(tilePtr, 255, width * height);
            for (int x = 0; x < pChar->w; x++)
            {
                for (int y = 0; y < pChar->h; y++)
                {
                    int const dx = x;
                    int const dy = y + pQFont->atf + pChar->oy-yoff;
                    if (dx >= 0 && dx < width && dy >= 0 && dy < height)
                        tilePtr[dx*height + dy] = pQFont->at820[pChar->offset + x * pChar->h + y];
                }
            }
        }

        pFont->tile = tile;
        pFont->xSize = pQFont->at12;
        pFont->ySize = pQFont->at13;
        pFont->space = pQFont->at11;
        pFont->yoff = yoff;

        return;
    }
    int xSize = 0;
    int ySize = 0;
    pFont->tile = tile;
    for (int i = 0; i < 96; i++)
    {
        if (tilesiz[tile+i].x > xSize)
            xSize = tilesiz[tile+i].x;
        if (tilesiz[tile+i].y > ySize)
            ySize = tilesiz[tile+i].y;
    }
    pFont->xSize = xSize;
    pFont->ySize = ySize;
    pFont->space = space;
    pFont->yoff = yoff;
}

void viewGetFontInfo(int id, const char *unk1, int *pXSize, int *pYSize)
{
    if (id < 0 || id >= kFontNum)
        return;
    FONT *pFont = &gFont[id];
    if (!unk1)
    {
        if (pXSize)
            *pXSize = pFont->xSize;
        if (pYSize)
            *pYSize = pFont->ySize;
    }
    else
    {
        int width = -pFont->space;
        for (const char *pBuf = unk1; *pBuf != 0; pBuf++)
        {
            int tile = ((*pBuf-32)&127)+pFont->tile;
            if (tilesiz[tile].x != 0 && tilesiz[tile].y != 0)
                width += tilesiz[tile].x+pFont->space;
        }
        if (pXSize)
            *pXSize = width;
        if (pYSize)
            *pYSize = pFont->ySize;
    }
}

void viewUpdatePages(void)
{
    pcBackground = numpages;
}

void viewToggle(int viewMode)
{
    if (viewMode == 3)
        gViewMode = 4;
    else
    {
        gViewMode = 3;
        viewResizeView(gViewSize);
    }
}

void viewInitializePrediction(void)
{
    predict.at30 = gMe->q16ang;
    predict.at20 = gMe->q16look;
    predict.at24 = gMe->q16horiz;
    predict.at28 = gMe->q16slopehoriz;
    predict.at2c = gMe->slope;
    predict.at6f = gMe->cantJump;
    predict.at70 = gMe->isRunning;
    predict.at72 = gMe->isUnderwater;
    predict.at71 = gMe->input.buttonFlags.jump;
    predict.at50 = gMe->pSprite->x;
    predict.at54 = gMe->pSprite->y;
    predict.at58 = gMe->pSprite->z;
    predict.at68 = gMe->pSprite->sectnum;
    predict.at73 = gMe->pSprite->flags;
    predict.at5c = xvel[gMe->pSprite->index];
    predict.at60 = yvel[gMe->pSprite->index];
    predict.at64 = zvel[gMe->pSprite->index];
    predict.at6a = gMe->pXSprite->height;
    predict.at48 = gMe->posture;
    predict.at4c = gMe->spin;
    predict.at6e = gMe->input.keyFlags.lookCenter;
    memcpy(&predict.at75,&gSpriteHit[gMe->pSprite->extra],sizeof(SPRITEHIT));
    predict.at0 = gMe->bobPhase;
    predict.at4 = gMe->bobAmp;
    predict.at8 = gMe->bobHeight;
    predict.atc = gMe->bobWidth;
    predict.at10 = gMe->swayPhase;
    predict.at14 = gMe->swayAmp;
    predict.at18 = gMe->swayHeight;
    predict.at1c = gMe->swayWidth;
    predict.at34 = gMe->zWeapon-gMe->zView-(12<<8);
    predict.at38 = gMe->zView;
    predict.at3c = gMe->zViewVel;
    predict.at40 = gMe->zWeapon;
    predict.at44 = gMe->zWeaponVel;
    predictOld = predict;
    if (numplayers != 1)
    {
        gViewAngle = predict.at30;
        gViewLook = predict.at20;
    }
}

void viewUpdatePrediction(GINPUT *pInput)
{
    predictOld = predict;
    short bakCstat = gMe->pSprite->cstat;
    gMe->pSprite->cstat = 0;
    fakePlayerProcess(gMe, pInput);
    fakeActProcessSprites();
    gMe->pSprite->cstat = bakCstat;
    predictFifo[gPredictTail&255] = predict;
    gPredictTail++;
    if (numplayers != 1)
    {
        gViewAngle = predict.at30;
        gViewLook = predict.at20;
    }
}

void sub_158B4(PLAYER *pPlayer)
{
    predict.at38 = predict.at58 - pPlayer->pPosture[pPlayer->lifeMode][predict.at48].eyeAboveZ;
    predict.at40 = predict.at58 - pPlayer->pPosture[pPlayer->lifeMode][predict.at48].weaponAboveZ;
}

void fakeProcessInput(PLAYER *pPlayer, GINPUT *pInput)
{
    POSTURE *pPosture = &pPlayer->pPosture[pPlayer->lifeMode][predict.at48];

    if (numplayers > 1 && gPrediction)
    {
        gViewAngleAdjust = 0.f;
        gViewLookRecenter = false;
        gViewLookAdjust = 0.f;
    }

#if 0 // syncFlags.run is not passed to input packet on ProcessFrame(), so don't apply this logic here
    predict.at70 = VanillaMode() ? pInput->syncFlags.run : 0;
#else
    predict.at70 = 0;
#endif
    predict.at71 = pInput->buttonFlags.jump;
    if (predict.at48 == 1)
    {
        int x = Cos(fix16_to_int(predict.at30));
        int y = Sin(fix16_to_int(predict.at30));
        if (pInput->forward)
        {
            int forward = pInput->forward;
            if (forward > 0)
                forward = mulscale8(pPosture->frontAccel, forward);
            else
                forward = mulscale8(pPosture->backAccel, forward);
            predict.at5c += mulscale30(forward, x);
            predict.at60 += mulscale30(forward, y);
        }
        if (pInput->strafe)
        {
            int strafe = pInput->strafe;
            strafe = mulscale8(pPosture->sideAccel, strafe);
            predict.at5c += mulscale30(strafe, y);
            predict.at60 -= mulscale30(strafe, x);
        }
    }
    else if (predict.at6a < 0x100)
    {
        int speed = 0x10000;
        if (predict.at6a > 0)
            speed -= divscale16(predict.at6a, 0x100);
        int x = Cos(fix16_to_int(predict.at30));
        int y = Sin(fix16_to_int(predict.at30));
        if (pInput->forward)
        {
            int forward = pInput->forward;
            if (forward > 0)
                forward = mulscale8(pPosture->frontAccel, forward);
            else
                forward = mulscale8(pPosture->backAccel, forward);
            if (predict.at6a)
                forward = mulscale16(forward, speed);
            predict.at5c += mulscale30(forward, x);
            predict.at60 += mulscale30(forward, y);
        }
        if (pInput->strafe)
        {
            int strafe = pInput->strafe;
            strafe = mulscale8(pPosture->sideAccel, strafe);
            if (predict.at6a)
                strafe = mulscale16(strafe, speed);
            predict.at5c += mulscale30(strafe, y);
            predict.at60 -= mulscale30(strafe, x);
        }
    }
    if (pInput->q16turn)
    {
        if (VanillaMode())
            predict.at30 = ((predict.at30&0x7ff0000)+(pInput->q16turn&0x7ff0000))&0x7ffffff;
        else
            predict.at30 = (predict.at30+pInput->q16turn)&0x7ffffff;
    }
    if (pInput->keyFlags.spin180)
        if (!predict.at4c)
            predict.at4c = -kAng180;
    if (predict.at4c < 0)
    {
        const int speed = (predict.at48 == 1) ? 64 : 128;
        predict.at4c = min(predict.at4c+speed, 0);
        predict.at30 += fix16_from_int(speed);
        if (numplayers > 1 && gPrediction)
            gViewAngleAdjust += float(ClipHigh(-predict.at4c, speed)); // don't overturn when nearing end of spin
    }

    if (!predict.at71)
        predict.at6f = 0;

    switch (predict.at48)
    {
    case 1:
        if (predict.at71)
            predict.at64 -= pPosture->normalJumpZ;//0x5b05;
        if (pInput->buttonFlags.crouch)
            predict.at64 += pPosture->normalJumpZ;//0x5b05;
        break;
    case 2:
        if (!pInput->buttonFlags.crouch)
            predict.at48 = 0;
        break;
    default:
        if (!predict.at6f && predict.at71 && predict.at6a == 0) {
            if (packItemActive(pPlayer, kPackJumpBoots)) predict.at64 = pPosture->pwupJumpZ;//-0x175555;
            else predict.at64 = pPosture->normalJumpZ;//-0xbaaaa;
            predict.at6f = 1;
        }
        if (pInput->buttonFlags.crouch)
            predict.at48 = 2;
        break;
    }
#if 0
    if (predict.at6e && !pInput->buttonFlags.lookUp && !pInput->buttonFlags.lookDown)
    {
        if (predict.at20 < 0)
            predict.at20 = fix16_min(predict.at20+F16(4), F16(0));
        if (predict.at20 > 0)
            predict.at20 = fix16_max(predict.at20-F16(4), F16(0));
        if (predict.at20 == 0)
            predict.at6e = 0;
    }
    else
    {
        if (pInput->buttonFlags.lookUp)
            predict.at20 = fix16_min(predict.at20+F16(4), F16(60));
        if (pInput->buttonFlags.lookDown)
            predict.at20 = fix16_max(predict.at20-F16(4), F16(-60));
    }
    predict.at20 = fix16_clamp(predict.at20+pInput->q16mlook, F16(-60), F16(60));

    if (predict.at20 > 0)
        predict.at24 = mulscale30(F16(120), Sin(fix16_to_int(predict.at20<<3)));
    else if (predict.at20 < 0)
        predict.at24 = mulscale30(F16(180), Sin(fix16_to_int(predict.at20<<3)));
    else
        predict.at24 = 0;
#endif
    CONSTEXPR int upAngle = 289;
    CONSTEXPR int downAngle = -347;
    CONSTEXPR double lookStepUp = 4.0*upAngle/60.0;
    CONSTEXPR double lookStepDown = -4.0*downAngle/60.0;
    if (predict.at6e && !pInput->buttonFlags.lookUp && !pInput->buttonFlags.lookDown)
    {
        if (predict.at20 < 0)
            predict.at20 = fix16_min(predict.at20+F16(lookStepDown), F16(0));
        if (predict.at20 > 0)
            predict.at20 = fix16_max(predict.at20-F16(lookStepUp), F16(0));
        if (predict.at20 == 0)
            predict.at6e = 0;
    }
    else
    {
        if (pInput->buttonFlags.lookUp)
            predict.at20 = fix16_min(predict.at20+F16(lookStepUp), F16(upAngle));
        if (pInput->buttonFlags.lookDown)
            predict.at20 = fix16_max(predict.at20-F16(lookStepDown), F16(downAngle));
    }
    if (numplayers > 1 && gPrediction)
    {
        if (pInput->buttonFlags.lookUp)
        {
            gViewLookAdjust += float(lookStepUp);
        }
        if (pInput->buttonFlags.lookDown)
        {
            gViewLookAdjust -= float(lookStepDown);
        }
        gViewLookRecenter = predict.at6e && !pInput->buttonFlags.lookUp && !pInput->buttonFlags.lookDown;
    }
    predict.at20 = fix16_clamp(predict.at20+(pInput->q16mlook<<3), F16(downAngle), F16(upAngle));
    predict.at24 = fix16_from_float(100.f*tanf(fix16_to_float(predict.at20)*fPI/1024.f));

    int nSector = predict.at68;
    int florhit = predict.at75.florhit & 0xc000;
    char va;
    if (predict.at6a < 16 && (florhit == 0x4000 || florhit == 0))
        va = 1;
    else
        va = 0;
    if (va && (sector[nSector].floorstat&2) != 0)
    {
        int z1 = getflorzofslope(nSector, predict.at50, predict.at54);
        int x2 = predict.at50+mulscale30(64, Cos(fix16_to_int(predict.at30)));
        int y2 = predict.at54+mulscale30(64, Sin(fix16_to_int(predict.at30)));
        short nSector2 = nSector;
        updatesector(x2, y2, &nSector2);
        if (nSector2 == nSector)
        {
            int z2 = getflorzofslope(nSector2, x2, y2);
            predict.at28 = interpolate(predict.at28, fix16_from_int(z1-z2)>>3, 0x4000);
        }
    }
    else
    {
        predict.at28 = interpolate(predict.at28, 0, 0x4000);
        if (klabs(predict.at28) < 4)
            predict.at28 = 0;
    }
    predict.at2c = (-fix16_to_int(predict.at24))<<7;
}

void fakePlayerProcess(PLAYER *pPlayer, GINPUT *pInput)
{
    spritetype *pSprite = pPlayer->pSprite;
    XSPRITE *pXSprite = pPlayer->pXSprite;
    POSTURE* pPosture = &pPlayer->pPosture[pPlayer->lifeMode][predict.at48];

    int top, bottom;
    GetSpriteExtents(pSprite, &top, &bottom);

    top += predict.at58-pSprite->z;
    bottom += predict.at58-pSprite->z;

    int dzb = (bottom-predict.at58)/4;
    int dzt = (predict.at58-top)/4;

    int dw = pSprite->clipdist<<2;
    short nSector = predict.at68;
    if (!gNoClip)
    {
        pushmove_old((int32_t*)&predict.at50, (int32_t*)&predict.at54, (int32_t*)&predict.at58, &predict.at68, dw, dzt, dzb, CLIPMASK0);
        if (predict.at68 == -1)
            predict.at68 = nSector;
    }
    fakeProcessInput(pPlayer, pInput);

    int nSpeed = approxDist(predict.at5c, predict.at60);

    predict.at3c = interpolate(predict.at3c, predict.at64, 0x7000);
    int dz = predict.at58-pPosture->eyeAboveZ-predict.at38;
    if (dz > 0)
        predict.at3c += mulscale16(dz<<8, 0xa000);
    else
        predict.at3c += mulscale16(dz<<8, 0x1800);
    predict.at38 += predict.at3c>>8;

    predict.at44 = interpolate(predict.at44, predict.at64, 0x5000);
    dz = predict.at58-pPosture->weaponAboveZ-predict.at40;
    if (dz > 0)
        predict.at44 += mulscale16(dz<<8, 0x8000);
    else
        predict.at44 += mulscale16(dz<<8, 0xc00);
    predict.at40 += predict.at44>>8;

    predict.at34 = predict.at40 - predict.at38 - (12<<8);

    predict.at0 = ClipLow(predict.at0-kTicsPerFrame, 0);

    nSpeed >>= 16;
    if (predict.at48 == 1)
    {
        predict.at4 = (predict.at4+17)&2047;
        predict.at14 = (predict.at14+17)&2047;
        predict.at8 = mulscale30(10*pPosture->bobV,Sin(predict.at4*2));
        predict.atc = mulscale30(predict.at0*pPosture->bobH,Sin(predict.at4-256));
        predict.at18 = mulscale30(predict.at0*pPosture->swayV,Sin(predict.at14*2));
        predict.at1c = mulscale30(predict.at0*pPosture->swayH,Sin(predict.at14-0x155));
    }
    else
    {
        if (pXSprite->height < 256)
        {
            predict.at4 = (predict.at4+(pPosture->pace[predict.at70]*4))&2047;
            predict.at14 = (predict.at14+(pPosture->pace[predict.at70]*4)/2)&2047;
            if (predict.at70)
            {
                if (predict.at0 < 60)
                    predict.at0 = ClipHigh(predict.at0 + nSpeed, 60);
            }
            else
            {
                if (predict.at0 < 30)
                    predict.at0 = ClipHigh(predict.at0 + nSpeed, 30);
            }
        }
        predict.at8 = mulscale30(predict.at0*pPosture->bobV,Sin(predict.at4*2));
        predict.atc = mulscale30(predict.at0*pPosture->bobH,Sin(predict.at4-256));
        predict.at18 = mulscale30(predict.at0*pPosture->swayV,Sin(predict.at14*2));
        predict.at1c = mulscale30(predict.at0*pPosture->swayH,Sin(predict.at14-0x155));
    }
    if (!pXSprite->health)
        return;
    predict.at72 = 0;
    if (predict.at48 == 1)
    {
        predict.at72 = 1;
        int nSector = predict.at68;
        int nLink = gLowerLink[nSector];
        if (nLink > 0 && (sprite[nLink].type == kMarkerLowGoo || sprite[nLink].type == kMarkerLowWater))
        {
            if (getceilzofslope(nSector, predict.at50, predict.at54) > predict.at38)
                predict.at72 = 0;
        }
    }
}

void fakeMoveDude(spritetype *pSprite)
{
    PLAYER *pPlayer = NULL;
    int bottom, top;
    if (IsPlayerSprite(pSprite))
        pPlayer = &gPlayer[pSprite->type-kDudePlayer1];
    dassert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    GetSpriteExtents(pSprite, &top, &bottom);
    top += predict.at58 - pSprite->z;
    bottom += predict.at58 - pSprite->z;
    int bz = (bottom-predict.at58)/4;
    int tz = (predict.at58-top)/4;
    int wd = pSprite->clipdist*4;
    int nSector = predict.at68;
    dassert(nSector >= 0 && nSector < kMaxSectors);
    if (predict.at5c || predict.at60)
    {
        if (pPlayer && gNoClip)
        {
            predict.at50 += predict.at5c>>12;
            predict.at54 += predict.at60>>12;
            if (!FindSector(predict.at50, predict.at54, &nSector))
                nSector = predict.at68;
        }
        else
        {
            short bakCstat = pSprite->cstat;
            pSprite->cstat &= ~257;
            predict.at75.hit = ClipMove(&predict.at50, &predict.at54, &predict.at58, &nSector, predict.at5c >> 12, predict.at60 >> 12, wd, tz, bz, CLIPMASK0);
            if (nSector == -1)
                nSector = predict.at68;
                    
            if (sector[nSector].type >= kSectorPath && sector[nSector].type <= kSectorRotate)
            {
                short nSector2 = nSector;
                pushmove_old((int32_t*)&predict.at50, (int32_t*)&predict.at54, (int32_t*)&predict.at58, &nSector2, wd, tz, bz, CLIPMASK0);
                if (nSector2 != -1)
                    nSector = nSector2;
            }

            dassert(nSector >= 0);

            pSprite->cstat = bakCstat;
        }
        switch (predict.at75.hit&0xc000)
        {
        case 0x8000:
        {
            int nHitWall = predict.at75.hit&0x3fff;
            walltype *pHitWall = &wall[nHitWall];
            if (pHitWall->nextsector != -1)
            {
                sectortype *pHitSector = &sector[pHitWall->nextsector];
                if (top < pHitSector->ceilingz || bottom > pHitSector->floorz)
                {
                    // ???
                }
            }
            actWallBounceVector(&predict.at5c, &predict.at60, nHitWall, 0);
            break;
        }
        }
    }
    if (predict.at68 != nSector)
    {
        dassert(nSector >= 0 && nSector < kMaxSectors);
        predict.at68 = nSector;
    }
    char bUnderwater = 0;
    char bDepth = 0;
    int nXSector = sector[nSector].extra;
    if (nXSector > 0)
    {
        XSECTOR *pXSector = &xsector[nXSector];
        if (pXSector->Underwater)
            bUnderwater = 1;
        if (pXSector->Depth)
            bDepth = 1;
    }
    int nUpperLink = gUpperLink[nSector];
    int nLowerLink = gLowerLink[nSector];
    if (nUpperLink >= 0 && (sprite[nUpperLink].type == kMarkerUpWater || sprite[nUpperLink].type == kMarkerUpGoo))
        bDepth = 1;
    if (nLowerLink >= 0 && (sprite[nLowerLink].type == kMarkerLowWater || sprite[nLowerLink].type == kMarkerLowGoo))
        bDepth = 1;
    if (pPlayer)
        wd += 16;

    if (predict.at64)
        predict.at58 += predict.at64 >> 8;

    spritetype pSpriteBak = *pSprite;
    spritetype *pTempSprite = pSprite;
    pTempSprite->x = predict.at50;
    pTempSprite->y = predict.at54;
    pTempSprite->z = predict.at58;
    pTempSprite->sectnum = predict.at68;
    int ceilZ, ceilHit, floorZ, floorHit;
    GetZRange(pTempSprite, &ceilZ, &ceilHit, &floorZ, &floorHit, wd, CLIPMASK0);
    GetSpriteExtents(pTempSprite, &top, &bottom);
    if (predict.at73 & 2)
    {
        int vc = 58254;
        if (bDepth)
        {
            if (bUnderwater)
            {
                int cz = getceilzofslope(nSector, predict.at50, predict.at54);
                if (cz > top)
                    vc += ((bottom-cz)*-80099) / (bottom-top);
                else
                    vc = 0;
            }
            else
            {
                int fz = getflorzofslope(nSector, predict.at50, predict.at54);
                if (fz < bottom)
                    vc += ((bottom-fz)*-80099) / (bottom-top);
            }
        }
        else
        {
            if (bUnderwater)
                vc = 0;
            else if (bottom >= floorZ)
                vc = 0;
        }
        if (vc)
        {
            predict.at58 += ((vc*4)/2)>>8;
            predict.at64 += vc;
        }
    }
    GetSpriteExtents(pTempSprite, &top, &bottom);
    if (bottom >= floorZ)
    {
        int floorZ2 = floorZ;
        int floorHit2 = floorHit;
        GetZRange(pTempSprite, &ceilZ, &ceilHit, &floorZ, &floorHit, pSprite->clipdist<<2, CLIPMASK0, PARALLAXCLIP_CEILING|PARALLAXCLIP_FLOOR);
        if (bottom <= floorZ && predict.at58-floorZ2 < bz)
        {
            floorZ = floorZ2;
            floorHit = floorHit2;
        }
    }
    if (floorZ <= bottom)
    {
        predict.at75.florhit = floorHit;
        predict.at58 += floorZ-bottom;
        int var44 = predict.at64-velFloor[predict.at68];
        if (var44 > 0)
        {
            actFloorBounceVector(&predict.at5c, &predict.at60, &var44, predict.at68, 0);
            predict.at64 = var44;
            if (klabs(predict.at64) < 0x10000)
            {
                predict.at64 = velFloor[predict.at68];
                predict.at73 &= ~4;
            }
            else
                predict.at73 |= 4;
        }
        else if (predict.at64 == 0)
            predict.at73 &= ~4;
    }
    else
    {
        predict.at75.florhit = 0;
        if (predict.at73 & 2)
            predict.at73 |= 4;
    }
    if (top <= ceilZ)
    {
        predict.at75.ceilhit = ceilHit;
        predict.at58 += ClipLow(ceilZ-top, 0);
        if (predict.at64 <= 0 && (predict.at73&4))
            predict.at64 = mulscale16(-predict.at64, 0x2000);
    }
    else
        predict.at75.ceilhit = 0;

    GetSpriteExtents(pTempSprite, &top, &bottom);
    *pSprite = pSpriteBak;
    predict.at6a = ClipLow(floorZ-bottom, 0)>>8;
    if (predict.at5c || predict.at60)
    {
        if ((floorHit & 0xc000) == 0xc000)
        {
            int nHitSprite = floorHit & 0x3fff;
            if ((sprite[nHitSprite].cstat & 0x30) == 0)
            {
                predict.at5c += mulscale2(4, predict.at50 - sprite[nHitSprite].x);
                predict.at60 += mulscale2(4, predict.at54 - sprite[nHitSprite].y);
                return;
            }
        }
        int nXSector = sector[pSprite->sectnum].extra;
        if (nXSector > 0 && xsector[nXSector].Underwater)
            return;
        if (predict.at6a >= 0x100)
            return;
        int nDrag = gDudeDrag;
        if (predict.at6a > 0)
            nDrag -= scale(gDudeDrag, predict.at6a, 0x100);
        predict.at5c -= mulscale16r(predict.at5c, nDrag);
        predict.at60 -= mulscale16r(predict.at60, nDrag);
        if (approxDist(predict.at5c, predict.at60) < 0x1000)
            predict.at5c = predict.at60 = 0;
    }
}

void fakeActAirDrag(spritetype *pSprite, int num)
{
    UNREFERENCED_PARAMETER(pSprite);
    int xvec = 0;
    int yvec = 0;
    int nSector = predict.at68;
    dassert(nSector >= 0 && nSector < kMaxSectors);
    sectortype *pSector = &sector[nSector];
    int nXSector = pSector->extra;
    if (nXSector > 0)
    {
        dassert(nXSector < kMaxXSectors);
        XSECTOR *pXSector = &xsector[nXSector];
        if (pXSector->windVel && (pXSector->windAlways || pXSector->busy))
        {
            int vel = pXSector->windVel<<12;
            if (!pXSector->windAlways && pXSector->busy)
                vel = mulscale16(vel, pXSector->busy);
            xvec = mulscale30(vel, Cos(pXSector->windAng));
            yvec = mulscale30(vel, Sin(pXSector->windAng));
        }
    }
    predict.at5c += mulscale16(xvec-predict.at5c, num);
    predict.at60 += mulscale16(yvec-predict.at60, num);
    predict.at64 -= mulscale16(predict.at64, num);
}

void fakeActProcessSprites(void)
{
    spritetype *pSprite = gMe->pSprite;
    if (pSprite->statnum == kStatDude)
    {
        int nXSprite = pSprite->extra;
        dassert(nXSprite > 0 && nXSprite < kMaxXSprites);
        int nSector = predict.at68;
        int nXSector = sector[nSector].extra;
        XSECTOR *pXSector = NULL;
        if (nXSector > 0)
        {
            dassert(nXSector > 0 && nXSector < kMaxXSectors);
            dassert(xsector[nXSector].reference == nSector);
            pXSector = &xsector[nXSector];
        }
        if (pXSector)
        {
            int top, bottom;
            GetSpriteExtents(pSprite, &top, &bottom);
            top += predict.at58 - pSprite->z;
            bottom += predict.at58 - pSprite->z;
            if (getflorzofslope(nSector, predict.at50, predict.at54) < bottom)
            {
                int angle = pXSector->panAngle;
                int speed = 0;
                if (pXSector->panAlways || pXSector->state || pXSector->busy)
                {
                    speed = pXSector->panVel << 9;
                    if (!pXSector->panAlways && pXSector->busy)
                        speed = mulscale16(speed, pXSector->busy);
                }
                if (sector[nSector].floorstat&64)
                    angle = (GetWallAngle(sector[nSector].wallptr)+512)&2047;
                predict.at5c += mulscale30(speed,Cos(angle));
                predict.at60 += mulscale30(speed,Sin(angle));
            }
        }
        if (pXSector && pXSector->Underwater)
            fakeActAirDrag(pSprite, 5376);
        else
            fakeActAirDrag(pSprite, 128);

        if ((predict.at73 & 4) != 0 || predict.at5c != 0 || predict.at60 != 0 || predict.at64 != 0 || velFloor[predict.at68] != 0 || velCeil[predict.at68] != 0)
        {
            fakeMoveDude(pSprite);
        }
    }
}

void viewCorrectPrediction(void)
{
    if (numplayers == 1)
    {
        gViewLook = gMe->q16look;
        gViewAngle = gMe->q16ang;
        return;
    }
    spritetype *pSprite = gMe->pSprite;
    VIEW *pView = &predictFifo[(gNetFifoTail-1)&255];
    const char bCalPrediction = !VanillaMode() || (gMe->q16ang != pView->at30 || pView->at24 != gMe->q16horiz || pView->at50 != pSprite->x || pView->at54 != pSprite->y || pView->at58 != pSprite->z);
    if (bCalPrediction)
    {
        viewInitializePrediction();
        predictOld = gPrevView[myconnectindex];
        gPredictTail = gNetFifoTail;
        while (gPredictTail < gNetFifoHead[myconnectindex])
        {
            viewUpdatePrediction(&gFifoInput[gPredictTail&255][myconnectindex]);
        }
    }
}

void viewBackupView(int nPlayer)
{
    PLAYER *pPlayer = &gPlayer[nPlayer];
    VIEW *pView = &gPrevView[nPlayer];
    pView->at30 = pPlayer->q16ang;
    pView->at50 = pPlayer->pSprite->x;
    pView->at54 = pPlayer->pSprite->y;
    if (!VanillaMode())
        pView->at58 = pPlayer->pSprite->z;
    pView->at38 = pPlayer->zView;
    pView->at34 = pPlayer->zWeapon-pPlayer->zView-0xc00;
    pView->at24 = pPlayer->q16horiz;
    pView->at28 = pPlayer->q16slopehoriz;
    pView->at2c = pPlayer->slope;
    pView->at8 = pPlayer->bobHeight;
    pView->atc = pPlayer->bobWidth;
    pView->at18 = pPlayer->swayHeight;
    pView->at1c = pPlayer->swayWidth;
}

void viewCorrectViewOffsets(int nPlayer, vec3_t const *oldpos)
{
    PLAYER *pPlayer = &gPlayer[nPlayer];
    VIEW *pView = &gPrevView[nPlayer];
    pView->at50 += pPlayer->pSprite->x-oldpos->x;
    pView->at54 += pPlayer->pSprite->y-oldpos->y;
    if (!VanillaMode())
        pView->at58 += pPlayer->pSprite->z-oldpos->z;
    pView->at38 += pPlayer->pSprite->z-oldpos->z;
}

void viewClearInterpolations(void)
{
    nInterpolations = 0;
    nInterpolationsPanning = 0;
    memset(gInterpolateSprite, 0, sizeof(gInterpolateSprite));
    memset(gInterpolateWall, 0, sizeof(gInterpolateWall));
    memset(gInterpolateSector, 0, sizeof(gInterpolateSector));
    memset(gInterpolatePanningWall, 0, sizeof(gInterpolatePanningWall));
    memset(gInterpolatePanningCeiling, 0, sizeof(gInterpolatePanningCeiling));
    memset(gInterpolatePanningFloor, 0, sizeof(gInterpolatePanningFloor));
}

void viewAddInterpolation(void *data, INTERPOLATE_TYPE type)
{
    if (nInterpolations == kMaxInterpolations)
        ThrowError("Too many interpolations");
    if ((nInterpolationsPanning == kMaxInterpolationsPanning) && (type == INTERPOLATE_TYPE_CHAR_PANNING)) // too many texture panning interpolations, don't add anymore
        return;
    INTERPOLATE *pInterpolate = &gInterpolation[nInterpolations++];
    pInterpolate->pointer = data;
    pInterpolate->type = type;
    switch (type)
    {
    case INTERPOLATE_TYPE_INT:
        pInterpolate->value = *((int*)data);
        break;
    case INTERPOLATE_TYPE_SHORT:
        pInterpolate->value = *((short*)data);
        break;
    case INTERPOLATE_TYPE_CHAR_PANNING:
        nInterpolationsPanning++;
        fallthrough__;
    case INTERPOLATE_TYPE_CHAR:
        pInterpolate->value = *((char*)data);
        break;
    }
}

void CalcInterpolations(void)
{
    int i, value;
    INTERPOLATE *pInterpolate = gInterpolation;
    for (i = 0; i < nInterpolations; i++, pInterpolate++)
    {
        value = pInterpolate->value;
        switch (pInterpolate->type)
        {
        case INTERPOLATE_TYPE_INT:
        {
            const int value2 = *((int*)pInterpolate->pointer);
            pInterpolate->value2 = value2;
            if (value2 != value)
                *((int*)pInterpolate->pointer) = (int)interpolate(value, value2, gInterpolate);
            break;
        }
        case INTERPOLATE_TYPE_SHORT:
        {
            const int value2 = *((short*)pInterpolate->pointer);
            pInterpolate->value2 = value2;
            if (value2 != value)
                *((short*)pInterpolate->pointer) = (short)interpolate(value, value2, gInterpolate);
            break;
        }
        case INTERPOLATE_TYPE_CHAR_PANNING:
        case INTERPOLATE_TYPE_CHAR:
        {
            const int value2 = *((char*)pInterpolate->pointer);
            pInterpolate->value2 = value2;
            const int nDiff = value - value2;
            if (nDiff == 0)
                break;
            if (nDiff > 127) // handle overflow gracefully
                value -= 256;
            else if (nDiff < -128)
                value += 256;
            *((char*)pInterpolate->pointer) = (char)interpolate(value, value2, gInterpolate);
            break;
        }
        }
    }
}

void RestoreInterpolations(void)
{
    int i;
    INTERPOLATE *pInterpolate = gInterpolation;
    for (i = 0; i < nInterpolations; i++, pInterpolate++)
    {
        switch (pInterpolate->type)
        {
        case INTERPOLATE_TYPE_INT:
            *((int*)pInterpolate->pointer) = pInterpolate->value2;
            break;
        case INTERPOLATE_TYPE_SHORT:
            *((short*)pInterpolate->pointer) = pInterpolate->value2;
            break;
        case INTERPOLATE_TYPE_CHAR_PANNING:
        case INTERPOLATE_TYPE_CHAR:
            *((char*)pInterpolate->pointer) = pInterpolate->value2;
            break;
        }
    }
}

void viewDrawText(int nFont, const char *pString, int x, int y, int nShade, int nPalette, int position, char shadow, unsigned int nStat, uint8_t alpha)
{
    if (nFont < 0 || nFont >= kFontNum || !pString) return;
    FONT *pFont = &gFont[nFont];

    y += pFont->yoff;

    if (position)
    {
        const char *s = pString;
        int width = -pFont->space;
        while (*s)
        {
            int nTile = ((*s-' ')&127)+pFont->tile;
            if (tilesiz[nTile].x && tilesiz[nTile].y)
                width += tilesiz[nTile].x+pFont->space;
            s++;
        }
        if (position == 1)
            width >>= 1;
        x -= width;
    }
    const char *s = pString;
    while (*s)
    {
        int nTile = ((*s-' ')&127) + pFont->tile;
        if (tilesiz[nTile].x && tilesiz[nTile].y)
        {
            if (shadow)
            {
                rotatesprite_fs_alpha((x+1)<<16, (y+1)<<16, 65536, 0, nTile, 127, nPalette, 26|nStat, alpha);
            }
            rotatesprite_fs_alpha(x<<16, y<<16, 65536, 0, nTile, nShade, nPalette, 26|nStat, alpha);
            x += tilesiz[nTile].x+pFont->space;
        }
        s++;
    }
}

void viewTileSprite(int nTile, int nShade, int nPalette, int x1, int y1, int x2, int y2)
{
    Rect rect1 = Rect(x1, y1, x2, y2);
    Rect rect2 = Rect(0, 0, xdim, ydim);
    rect1 &= rect2;

    if (!rect1)
        return;

    dassert(nTile >= 0 && nTile < kMaxTiles);
    int width = tilesiz[nTile].x;
    int height = tilesiz[nTile].y;
    int bx1 = DecBy(rect1.x0+1, width);
    int by1 = DecBy(rect1.y0+1, height);
    int bx2 = IncBy(rect1.x1-1, width);
    int by2 = IncBy(rect1.y1-1, height);
    for (int x = bx1; x < bx2; x += width)
        for (int y = by1; y < by2; y += height)
            rotatesprite(x<<16, y<<16, 65536, 0, nTile, nShade, nPalette, 64+16+8, x1, y1, x2-1, y2-1);
}

void InitStatusBar(void)
{
    tileLoadTile(2200);
}
void DrawStatSprite(int nTile, int x, int y, int nShade, int nPalette, unsigned int nStat, int nScale)
{
    rotatesprite(x<<16, y<<16, nScale, 0, nTile, nShade, nPalette, nStat | 74, 0, 0, xdim-1, ydim-1);
}
void DrawStatMaskedSprite(int nTile, int x, int y, int nShade, int nPalette, unsigned int nStat, int nScale)
{
    rotatesprite(x<<16, y<<16, nScale, 0, nTile, nShade, nPalette, nStat | 10, 0, 0, xdim-1, ydim-1);
}

void DrawStatNumber(const char *pFormat, int nNumber, int nTile, int x, int y, int nShade, int nPalette, unsigned int nStat, int nScale)
{
    char tempbuf[80];
    int width = tilesiz[nTile].x+1;
    x <<= 16;
    sprintf(tempbuf, pFormat, nNumber);
    const size_t nLength = strlen(tempbuf);
    for (size_t i = 0; i < nLength; i++, x += width*nScale)
    {
        int numTile, numScale, numY;
        if (tempbuf[i] == ' ')
            continue;
        if (tempbuf[i] == '-')
        {
            switch (nTile)
            {
            case 2190:
            case kSBarNumberHealth:
                numTile = kSBarNegative;
                break;
            case 2240:
            case kSBarNumberAmmo:
                numTile = kSBarNegative+1;
                break;
            case kSBarNumberInv:
                numTile = kSBarNegative+2;
                break;
            case kSBarNumberArmor1:
                numTile = kSBarNegative+3;
                break;
            case kSBarNumberArmor2:
                numTile = kSBarNegative+4;
                break;
            case kSBarNumberArmor3:
                numTile = kSBarNegative+5;
                break;
            default: // unknown font tile type, skip drawing minus sign
                continue;
            }
            numScale = nScale/3;
            numY = (y<<16) + (1<<15); // offset to center of number row
        }
        else // regular number
        {
            numTile = nTile+tempbuf[i]-'0';
            numScale = nScale;
            numY = y<<16;
        }
        rotatesprite(x, numY, numScale, 0, numTile, nShade, nPalette, nStat | 10, 0, 0, xdim-1, ydim-1);
    }
}

void TileHGauge(int nTile, int x, int y, int nMult, int nDiv, int nStat, int nScale)
{
    int bx = scale(mulscale16(tilesiz[nTile].x,nScale),nMult,nDiv)+x;
    int sbx;
    switch (nStat&(512+256))
    {
    case 256:
        sbx = mulscale16(bx, xscalecorrect)-1;
        break;
    case 512:
        bx -= 320;
        sbx = xdim+mulscale16(bx, xscalecorrect)-1;
        break;
    default:
        bx -= 160;
        sbx = (xdim>>1)+mulscale16(bx, xscalecorrect)-1;
        break;
    }
    rotatesprite(x<<16, y<<16, nScale, 0, nTile, 0, 0, nStat|90, 0, 0, sbx, ydim-1);
}

int gPackIcons[kPackMax] = {
    2569, 2564, 2566, 2568, 2560
};

struct PACKICON2 {
    short nTile;
    int nScale;
    int nYOffs;
};

PACKICON2 gPackIcons2[kPackMax] = {
    { 519, (int)(65536*0.5), 0 },
    { 830, (int)(65536*0.3), 0 },
    { 760, (int)(65536*0.6), 0 },
    { 839, (int)(65536*0.5), -4 },
    { 827, (int)(65536*0.4), 0 },
};

struct AMMOICON {
    short nTile;
    int nScale;
    int nYOffs;
};

AMMOICON gAmmoIcons[] = {
    { -1, 0, 0 },
    { 816, (int)(65536 * 0.5), 0 },
    { 619, (int)(65536 * 0.8), 0 },
    { 817, (int)(65536 * 0.7), 3 },
    { 801, (int)(65536 * 0.5), -6 },
    { 589, (int)(65536 * 0.7), 2 },
    { 618, (int)(65536 * 0.5), 4 },
    { 548, (int)(65536 * 0.3), -6 },
    { 820, (int)(65536 * 0.3), -6 },
    { 525, (int)(65536 * 0.6), -6 },
    { 811, (int)(65536 * 0.5), 2 },
    { 810, (int)(65536 * 0.45), 2 },
};

struct WEAPONICON {
    short nTile;
    char zOffset;
};

WEAPONICON gWeaponIcon[] = {
    { -1, 0 },
    { -1, 0 }, // 1: pitchfork
    { 524, 4 }, // 2: flare gun
    { 559, 7 }, // 3: shotgun
    { 558, 5 }, // 4: tommy gun
    { 526, 11 }, // 5: napalm launcher
    { 589, 7 }, // 6: dynamite
    { 618, 6 }, // 7: spray can
    { 539, 11 }, // 8: tesla gun
    { 800, 0 }, // 9: life leech
    { 525, 13 }, // 10: voodoo doll
    { 811, 7 }, // 11: proxy bomb
    { 810, 7 }, // 12: remote bomb
    { -1, 0 },
};

WEAPONICON gWeaponIconVoxel[] = {
    { -1, 0 },
    { -1, 0 }, // 1: pitchfork
    { 524, 6 }, // 2: flare gun
    { 559, 6 }, // 3: shotgun
    { 558, 8 }, // 4: tommy gun
    { 526, 6 }, // 5: napalm launcher
    { 589, 11 }, // 6: dynamite
    { 618, 11 }, // 7: spray can
    { 539, 6 }, // 8: tesla gun
    { 800, 0 }, // 9: life leech
    { 525, 11 }, // 10: voodoo doll
    { 811, 11 }, // 11: proxy bomb
    { 810, 11 }, // 12: remote bomb
    { -1, 0 },
};

void viewDrawStats(PLAYER *pPlayer, int x, int y)
{
    static int gLastPageTimeStats = 0;
    const int nFont = 3;
    char buffer[128];
    if (!gLevelStats || ((gLevelStats == 2) && (gViewMode == 3)))
        return;

    int nHeight;
    viewGetFontInfo(nFont, NULL, NULL, &nHeight);
    sprintf(buffer, "T:%d:%02d.%02d",
        (gLevelTime/(kTicsPerSec*60)),
        (gLevelTime/kTicsPerSec)%60,
        ((gLevelTime%kTicsPerSec)*33)/10
        );
    viewDrawText(3, buffer, x, y, 20, 0, 0, true, 256);
    y += nHeight+1;
    if (gGameOptions.nGameType != kGameTypeTeams)
        sprintf(buffer, "K:%d/%d", gKillMgr.at4, max(gKillMgr.at4, gKillMgr.at0));
    else
        sprintf(buffer, "K:%d", pPlayer->fragCount);
    viewDrawText(3, buffer, x, y, 20, 0, 0, true, 256);
    y += nHeight+1;
    sprintf(buffer, "S:%d/%d", gSecretMgr.nNormalSecretsFound, max(gSecretMgr.nNormalSecretsFound, gSecretMgr.nAllSecrets)); // if we found more than there are, increase the total - some levels have a bugged counter
    viewDrawText(3, buffer, x, y, 20, 0, 0, true, 256);
    if (gViewMode == 3 && gViewSize > 3 && gLastPageTimeStats != gLevelTime) // redraw borders
        viewUpdatePages();
    gLastPageTimeStats = gLevelTime;
}

#define kMaxBurnFlames 9

const struct BURNTABLE {
    short nTile;
    unsigned char nStat;
    unsigned char nPal;
    int nScale;
    short nX, nY;
} gBurnTable[kMaxBurnFlames] = {
    {2101, RS_AUTO, 0, 118784,  10, 220},
    {2101, RS_AUTO, 0, 110592,  40, 220},
    {2101, RS_AUTO, 0,  81920,  85, 220},
    {2101, RS_AUTO, 0,  69632, 120, 220},
    {2101, RS_AUTO, 0,  61440, 160, 220},
    {2101, RS_AUTO, 0,  73728, 200, 220},
    {2101, RS_AUTO, 0,  77824, 235, 220},
    {2101, RS_AUTO, 0, 110592, 275, 220},
    {2101, RS_AUTO, 0, 122880, 310, 220}
};

int gBurnTableAspectOffset[kMaxBurnFlames] = {0};

void viewBurnTimeInit(void)
{
    if (!r_usenewaspect) return;

    for (int i = 0; i < kMaxBurnFlames; i++)
    {
        int nX = gBurnTable[i].nX;
        nX = scale(nX-(320>>1), 320>>1, 266>>1); // scale flame position
        nX = scale(nX<<16, xscale, yscale); // multiply by window ratio
        nX += (320>>1)<<16; // offset to center
        gBurnTableAspectOffset[i] = nX;
    }
}

void viewBurnTime(int gScale)
{
    if (!gScale) return;

    for (int i = 0; i < kMaxBurnFlames; i++)
    {
        const BURNTABLE *pBurnTable = &gBurnTable[i];
        const int nTile = gBurnTable[i].nTile+qanimateoffs(pBurnTable->nTile,32768+i);
        int nScale = pBurnTable->nScale;
        if (gScale < 600)
            nScale = scale(nScale, gScale, 600);
        const int nX = r_usenewaspect ? gBurnTableAspectOffset[i] : pBurnTable->nX<<16;
        rotatesprite(nX, pBurnTable->nY<<16, nScale, 0, nTile,
            0, pBurnTable->nPal, pBurnTable->nStat, windowxy1.x, windowxy1.y, windowxy2.x, windowxy2.y);
    }
}

#define kPowerUps 12

const struct POWERUPDISPLAY {
    int nTile;
    int nScaleRatio;
    int yOffset;
} gPowerups[kPowerUps] = {
    {gPowerUpInfo[kPwUpShadowCloak].picnum, fix16_from_float(0.4f), 0}, // invisibility
    {gPowerUpInfo[kPwUpReflectShots].picnum, fix16_from_float(0.4f), 5}, // reflects enemy shots
    {gPowerUpInfo[kPwUpDeathMask].picnum, fix16_from_float(0.3f), 9}, // invulnerability
    {gPowerUpInfo[kPwUpTwoGuns].picnum, fix16_from_float(0.25f), 4}, // guns akimbo
    {gPowerUpInfo[kPwUpShadowCloakUseless].picnum, fix16_from_float(0.4f), 9}, // shadow cloak (does nothing, only appears at near the end of CP04)

    // not in official maps
    {gPowerUpInfo[kPwUpFeatherFall].picnum, fix16_from_float(0.3f), 7}, // feather fall
    {gPowerUpInfo[kPwUpGasMask].picnum, fix16_from_float(0.4f), 4}, // gas mask
    {gPowerUpInfo[kPwUpDoppleganger].picnum, fix16_from_float(0.5f), 5}, // doppelganger
    {gPowerUpInfo[kPwUpAsbestArmor].picnum, fix16_from_float(0.3f), 9}, // asbestos armor
    {gPowerUpInfo[kPwUpGrowShroom].picnum, fix16_from_float(0.4f), 4}, // grow shroom
    {gPowerUpInfo[kPwUpShrinkShroom].picnum, fix16_from_float(0.4f), 4}, // shrink shroom

    {gPowerUpInfo[kPwUpDivingSuit].picnum, fix16_from_float(0.3f), 9}, // diving suit supply
};

void viewDrawPowerUps(PLAYER* pPlayer)
{
    if (!gPowerupDuration)
        return;

    int nPowerActive[kPowerUps];
    nPowerActive[0] = pPlayer->pwUpTime[kPwUpShadowCloak]; // invisibility
    nPowerActive[1] = pPlayer->pwUpTime[kPwUpReflectShots]; // reflects enemy shots
    nPowerActive[2] = pPlayer->pwUpTime[kPwUpDeathMask]; // invulnerability
    nPowerActive[3] = pPlayer->pwUpTime[kPwUpTwoGuns];// guns akimbo
    nPowerActive[4] = pPlayer->pwUpTime[kPwUpShadowCloakUseless]; // shadow cloak

    // not in official maps
    nPowerActive[5] = pPlayer->pwUpTime[kPwUpFeatherFall]; // feather fall
    nPowerActive[6] = pPlayer->pwUpTime[kPwUpGasMask]; // gas mask
    nPowerActive[7] = pPlayer->pwUpTime[kPwUpDoppleganger]; // doppelganger
    nPowerActive[8] = pPlayer->pwUpTime[kPwUpAsbestArmor]; // asbestos armor
    nPowerActive[9] = pPlayer->pwUpTime[kPwUpGrowShroom]; // grow shroom
    nPowerActive[10] = pPlayer->pwUpTime[kPwUpShrinkShroom]; // shrink shroom

    nPowerActive[11] = pPlayer->isUnderwater && packItemActive(pPlayer, kPackDivingSuit) ? pPlayer->pwUpTime[kPwUpDivingSuit] : 0; // diving suit supply

    int nSortPower[kPowerUps+1];
    unsigned char nSortIndex[kPowerUps+1];
    unsigned char nSortCount = 0;
    for (int i = 0; i < kPowerUps; i++) // sort powerups
    {
        if (!nPowerActive[i])
            continue;
        nSortIndex[nSortCount] = i;
        nSortPower[nSortCount] = nPowerActive[i];
        nSortCount++;
    }
    for (int i = 1; i < nSortCount; i++)
    {
        for (int j = 0; j < nSortCount-i; j++)
        {
            if (nSortPower[j] <= nSortPower[j+1])
                continue;
            nSortPower[kPowerUps] = nSortPower[j];
            nSortPower[j] = nSortPower[j+1];
            nSortPower[j+1] = nSortPower[kPowerUps];
            nSortIndex[kPowerUps] = nSortIndex[j];
            nSortIndex[j] = nSortIndex[j+1];
            nSortIndex[j+1] = nSortIndex[kPowerUps];
        }
    }

    const int nWarning = 5;
    const int x = 15;
    int y = 50;
    for (int i = 0; i < nSortCount; i++)
    {
        const POWERUPDISPLAY *pPowerups = &gPowerups[nSortIndex[i]];
        int nTime = nSortPower[i] / 100;
        if (nTime > nWarning || ((int)totalclock & 32))
            DrawStatMaskedSprite(pPowerups->nTile, x, y + pPowerups->yOffset, 0, 0, 256, pPowerups->nScaleRatio);
        DrawStatNumber("%d", nTime, kSBarNumberInv, x + 15, y, 0, nTime > nWarning ? 0 : 2, 256, fix16_from_float(0.5f));
        y += 20;
    }
    static int gLastPageTimePowerup = 0;
    static int gLastPageTimePowerupCount = 0;
    if (gViewMode == 3 && gViewSize > 3) // redraw borders
    {
        if ((gLastPageTimePowerup != gLevelTime && nSortCount) || (gLastPageTimePowerupCount != nSortCount))
            viewUpdatePages();
    }
    gLastPageTimePowerup = gLevelTime;
    gLastPageTimePowerupCount = nSortCount;
}

void viewDrawMapTitle(void)
{
    if (!gShowMapTitle || gGameMenuMgr.m_bActive)
        return;

    int const fadeStartTic = int((videoGetRenderMode() == REND_CLASSIC ? 1.25f : 1.f)*kTicsPerSec);
    int const fadeEndTic = int(1.5f*kTicsPerSec);
    if (gLevelTime > fadeEndTic)
        return;
    uint8_t const alpha = clamp((gLevelTime-fadeStartTic)*255/(fadeEndTic-fadeStartTic), 0, 255);

    if (alpha != 255)
    {
        viewDrawText(1, levelGetTitle(), 160, 50, -128, 0, 1, 1, 0, alpha);
    }
}

void viewDrawAimedPlayerName(void)
{
    if (!gShowPlayerNames || (gGameOptions.nGameType == kGameTypeSinglePlayer) || !gView->pSprite)
        return;
    const int nX = Cos(gView->pSprite->ang)>>16;
    const int nY = Sin(gView->pSprite->ang)>>16;
    if (nX == 0 && nY == 0)
        return;
    const int nZ = gView->slope;

    const int hit = HitScan(gView->pSprite, gView->zView, nX, nY, nZ, CLIPMASK0, 512);
    if (hit == 3)
    {
        spritetype* pSprite = &sprite[gHitInfo.hitsprite];
        if (!IsPlayerSprite(pSprite))
            return;
        char nPlayer = pSprite->type-kDudePlayer1;
        char *szName = gProfile[nPlayer].name;
        int nPalette = (gPlayer[nPlayer].teamId&3)+11;
        viewDrawText(4, szName, 160, 125, -128, nPalette, 1, 1);
    }
}

void viewDrawPack(PLAYER *pPlayer, int x, int y)
{
    int packs[kPackMax];
    if (pPlayer->packItemTime)
    {
        int nPacks = 0;
        int width = 0;
        for (int i = 0; i < kPackMax; i++)
        {
            if (pPlayer->packSlots[i].curAmount)
            {
                packs[nPacks++] = i;
                width += tilesiz[gPackIcons[i]].x + 1;
            }
        }
        width /= 2;
        x -= width;
        for (int i = 0; i < nPacks; i++)
        {
            int nPack = packs[i];
            DrawStatSprite(2568, x+1, y-8);
            DrawStatSprite(2568, x+1, y-6);
            DrawStatSprite(gPackIcons[nPack], x+1, y+1);
            if (nPack == pPlayer->packItemId)
                DrawStatMaskedSprite(2559, x+1, y+1);
            int nShade;
            if (pPlayer->packSlots[nPack].isActive)
                nShade = 4;
            else
                nShade = 24;
            DrawStatNumber("%3d", pPlayer->packSlots[nPack].curAmount, 2250, x-4, y-13, nShade, 0);
            x += tilesiz[gPackIcons[nPack]].x + 1;
        }
    }
    static int gLastPageTimePack = 0;
    if (pPlayer->packItemTime != gLastPageTimePack) // redraw borders
        viewUpdatePages();
    gLastPageTimePack = pPlayer->packItemTime;
}

void DrawPackItemInStatusBar(PLAYER *pPlayer, int x, int y, int x2, int y2, int nStat)
{
    if (pPlayer->packItemId < 0) return;

    DrawStatSprite(gPackIcons[pPlayer->packItemId], x, y, 0, 0, nStat);
    DrawStatNumber("%3d", pPlayer->packSlots[pPlayer->packItemId].curAmount, 2250, x2, y2, 4, 0, nStat);
}

void DrawPackItemInStatusBar2(PLAYER *pPlayer, int x, int y, int x2, int y2, int nStat, int nScale)
{
    if (pPlayer->packItemId < 0) return;

    DrawStatMaskedSprite(gPackIcons2[pPlayer->packItemId].nTile, x, y+gPackIcons2[pPlayer->packItemId].nYOffs, 0, 0, nStat, gPackIcons2[pPlayer->packItemId].nScale);
    DrawStatNumber("%3d", pPlayer->packSlots[pPlayer->packItemId].curAmount, kSBarNumberInv, x2, y2, 4, 0, nStat, nScale);
}

void viewDrawPlayerSlots(void)
{
    for (int nRows = (gNetPlayers - 1) / 4; nRows >= 0; nRows--)
    {
        for (int nCol = 0; nCol < 4; nCol++)
        {
            DrawStatSprite(2229, 40 + nCol * 80, 4 + nRows * 9, 16);
        }
    }
}

char gTempStr[128];

void viewDrawPlayerFrags(void)
{
    viewDrawPlayerSlots();
    for (int i = 0, p = connecthead; p >= 0; i++, p = connectpoint2[p])
    {
        int x = 80 * (i & 3);
        int y = 9 * (i / 4);
        int col = gPlayer[p].teamId & 3;
        char* name = gProfile[p].name;
        if (gProfile[p].skill == 2)
            sprintf(gTempStr, "%s", name);
        else
            sprintf(gTempStr, "%s [%d]", name, gProfile[p].skill);
        Bstrupr(gTempStr);
        viewDrawText(4, gTempStr, x + 4, y + 1, -128, 11 + col, 0, 0);
        sprintf(gTempStr, "%2d", gPlayer[p].fragCount);
        viewDrawText(4, gTempStr, x + 76, y + 1, -128, 11 + col, 2, 0);
    }
}

void viewDrawPlayerFlags(void)
{
    viewDrawPlayerSlots();
    for (int i = 0, p = connecthead; p >= 0; i++, p = connectpoint2[p])
    {
        int x = 80 * (i & 3);
        int y = 9 * (i / 4);
        int col = gPlayer[p].teamId & 3;
        char* name = gProfile[p].name;
        if (gProfile[p].skill == 2)
            sprintf(gTempStr, "%s", name);
        else
            sprintf(gTempStr, "%s [%d]", name, gProfile[p].skill);
        Bstrupr(gTempStr);
        viewDrawText(4, gTempStr, x + 4, y + 1, -128, 11 + col, 0, 0);

        sprintf(gTempStr, "F");
        x += 76;
        if (gPlayer[p].hasFlag & 2)
        {
            viewDrawText(4, gTempStr, x, y + 1, -128, 12, 2, 0);
            x -= 6;
        }

        if (gPlayer[p].hasFlag & 1)
            viewDrawText(4, gTempStr, x, y + 1, -128, 11, 2, 0);
    }
}

void viewDrawCtfHudVanilla(ClockTicks arg)
{
    int x = 1, y = 1;
    if (gPlayerScoreTicks[0] == 0 || ((int)totalclock & 8))
    {
        viewDrawText(0, "BLUE", x, y, -128, 10, 0, 0, 256);
        gPlayerScoreTicks[0] = gPlayerScoreTicks[0] - arg;
        if (gPlayerScoreTicks[0] < 0)
            gPlayerScoreTicks[0] = 0;
        sprintf(gTempStr, "%-3d", gPlayerScores[0]);
        viewDrawText(0, gTempStr, x, y + 10, -128, 10, 0, 0, 256);
    }
    x = 319;
    if (gPlayerScoreTicks[1] == 0 || ((int)totalclock & 8))
    {
        viewDrawText(0, "RED", x, y, -128, 7, 2, 0, 512);
        gPlayerScoreTicks[1] = gPlayerScoreTicks[1] - arg;
        if (gPlayerScoreTicks[1] < 0)
            gPlayerScoreTicks[1] = 0;
        sprintf(gTempStr, "%3d", gPlayerScores[1]);
        viewDrawText(0, gTempStr, x, y + 10, -128, 7, 2, 0, 512);
    }
}

void flashTeamScore(ClockTicks arg, int team, bool show)
{
    dassert(0 == team || 1 == team); // 0: blue, 1: red

    if (gPlayerScoreTicks[team] == 0 || ((int)totalclock & 8))
    {
        gPlayerScoreTicks[team] = gPlayerScoreTicks[team] - arg;
        if (gPlayerScoreTicks[team] < 0)
            gPlayerScoreTicks[team] = 0;

        if (show)
            DrawStatNumber("%d", gPlayerScores[team], kSBarNumberInv, 290, team ? 125 : 90, 0, team ? 2 : 10, 512, 65536 * 0.75);
    }
}

void viewDrawCtfHud(ClockTicks arg)
{
    if (gViewSize == 0)
    {
        flashTeamScore(arg, 0, false);
        flashTeamScore(arg, 1, false);
        return;
    }

    bool blueFlagTaken = false;
    bool redFlagTaken = false;
    int blueFlagCarrierColor = 0;
    int redFlagCarrierColor = 0;
    for (int p = connecthead; p >= 0; p = connectpoint2[p])
    {
        if ((gPlayer[p].hasFlag & 1) != 0)
        {
            blueFlagTaken = true;
            blueFlagCarrierColor = gPlayer[p].teamId & 3;
        }
        if ((gPlayer[p].hasFlag & 2) != 0)
        {
            redFlagTaken = true;
            redFlagCarrierColor = gPlayer[p].teamId & 3;
        }
    }

    bool meHaveBlueFlag = gMe->hasFlag & 1;
    DrawStatMaskedSprite(meHaveBlueFlag ? 3558 : 3559, 320, 75, 0, 10, 512, 65536 * 0.35);
    if (gBlueFlagDropped)
        DrawStatMaskedSprite(2332, 305, 83, 0, 10, 512, 65536);
    else if (blueFlagTaken)
        DrawStatMaskedSprite(4097, 307, 77, 0, blueFlagCarrierColor ? 2 : 10, 512, 65536);
    flashTeamScore(arg, 0, true);

    bool meHaveRedFlag = gMe->hasFlag & 2;
    DrawStatMaskedSprite(meHaveRedFlag ? 3558 : 3559, 320, 110, 0, 2, 512, 65536 * 0.35);
    if (gRedFlagDropped)
        DrawStatMaskedSprite(2332, 305, 117, 0, 2, 512, 65536);
    else if (redFlagTaken)
        DrawStatMaskedSprite(4097, 307, 111, 0, redFlagCarrierColor ? 2 : 10, 512, 65536);
    flashTeamScore(arg, 1, true);

    static int gLastPageTimeFlag = 0;
    if (gViewMode == 3 && gViewSize > 3 && (gLastPageTimeFlag != gLevelTime)) // redraw borders
        viewUpdatePages();
    gLastPageTimeFlag = gLevelTime;
}

void UpdateStatusBar(ClockTicks arg)
{
    PLAYER *pPlayer = gView;
    XSPRITE *pXSprite = pPlayer->pXSprite;

    int nPalette = 0;

    if (gGameOptions.nGameType == kGameTypeTeams)
    {
        if (pPlayer->teamId & 1)
            nPalette = 7;
        else
            nPalette = 10;
    }

    if (gViewSize < 0) return;

    if (gViewSize == 1)
    {
        DrawStatMaskedSprite(2169, 12, 195, 0, 0, 256, (int)(65536*0.56));
        DrawStatNumber("%d", pXSprite->health>>4, kSBarNumberHealth, 28, 187, 0, 0, 256);
        if (pPlayer->armor[1])
        {
            DrawStatMaskedSprite(2578, 70, 186, 0, 0, 256, (int)(65536*0.5));
            DrawStatNumber("%3d", pPlayer->armor[1]>>4, kSBarNumberArmor2, 83, 187, 0, 0, 256, (int)(65536*0.65));
        }
        if (pPlayer->armor[0])
        {
            DrawStatMaskedSprite(2586, 112, 195, 0, 0, 256, (int)(65536*0.5));
            DrawStatNumber("%3d", pPlayer->armor[0]>>4, kSBarNumberArmor1, 125, 187, 0, 0, 256, (int)(65536*0.65));
        }
        if (pPlayer->armor[2])
        {
            DrawStatMaskedSprite(2602, 155, 196, 0, 0, 256, (int)(65536*0.5));
            DrawStatNumber("%3d", pPlayer->armor[2]>>4, kSBarNumberArmor3, 170, 187, 0, 0, 256, (int)(65536*0.65));
        }

        DrawPackItemInStatusBar2(pPlayer, 225, 194, 240, 187, 512, (int)(65536*0.7));

        if (pPlayer->curWeapon && pPlayer->weaponAmmo != -1)
        {
            int num = pPlayer->ammoCount[pPlayer->weaponAmmo];
            if (pPlayer->weaponAmmo == 6)
                num /= 10;
            if ((unsigned int)gAmmoIcons[pPlayer->weaponAmmo].nTile < kMaxTiles)
                DrawStatMaskedSprite(gAmmoIcons[pPlayer->weaponAmmo].nTile, 304, 192+gAmmoIcons[pPlayer->weaponAmmo].nYOffs,
                    0, 0, 512, gAmmoIcons[pPlayer->weaponAmmo].nScale);
            DrawStatNumber("%3d", num, kSBarNumberAmmo, 267, 187, 0, 0, 512);
        }

        if (gGameOptions.nGameType <= kGameTypeCoop) // don't show keys for bloodbath/teams as all players have every key
        {
            for (int i = 0; i < 6; i++)
            {
                if (pPlayer->hasKey[i+1])
                    DrawStatMaskedSprite(2552+i, 260+10*i, 170, 0, 0, 512, (int)(65536*0.25));
            }
        }

        if (pPlayer->throwPower && pXSprite->health > 0)
            TileHGauge(2260, 124, 175-10, pPlayer->throwPower, 65536);
        else
            viewDrawPack(pPlayer, 166, 200-tilesiz[2201].y/2-30);
        viewDrawStats(pPlayer, 2, 140);
        viewDrawPowerUps(pPlayer);
    }
    else if (gViewSize <= 2)
    {
        if (pPlayer->throwPower && pXSprite->health > 0)
            TileHGauge(2260, 124, 175, pPlayer->throwPower, 65536);
        else
            viewDrawPack(pPlayer, 166, 200-tilesiz[2201].y/2);
    }
    if (gViewSize == 2)
    {
        DrawStatSprite(2201, 34, 187, 16, nPalette, 256);
        if (pXSprite->health >= 16 || ((int)totalclock&16) || pXSprite->health == 0)
        {
            DrawStatNumber("%3d", pXSprite->health>>4, 2190, 8, 183, 0, 0, 256);
        }
        if (pPlayer->curWeapon && pPlayer->weaponAmmo != -1)
        {
            int num = pPlayer->ammoCount[pPlayer->weaponAmmo];
            if (pPlayer->weaponAmmo == 6)
                num /= 10;
            DrawStatNumber("%3d", num, 2240, 42, 183, 0, 0, 256);
        }
        DrawStatSprite(2173, 284, 187, 16, nPalette, 512);
        if (pPlayer->armor[1])
        {
            TileHGauge(2207, 250, 175, pPlayer->armor[1], 3200, 512);
            DrawStatNumber("%3d", pPlayer->armor[1]>>4, 2230, 255, 178, 0, 0, 512);
        }
        if (pPlayer->armor[0])
        {
            TileHGauge(2209, 250, 183, pPlayer->armor[0], 3200, 512);
            DrawStatNumber("%3d", pPlayer->armor[0]>>4, 2230, 255, 186, 0, 0, 512);
        }
        if (pPlayer->armor[2])
        {
            TileHGauge(2208, 250, 191, pPlayer->armor[2], 3200, 512);
            DrawStatNumber("%3d", pPlayer->armor[2]>>4, 2230, 255, 194, 0, 0, 512);
        }
        DrawPackItemInStatusBar(pPlayer, 286, 186, 302, 183, 512);

        if (gGameOptions.nGameType <= kGameTypeCoop) // don't show keys for bloodbath/teams as all players have every key
        {
            for (int i = 0; i < 6; i++)
            {
                int nTile = 2220+i;
                int x, nStat = 0;
                int y = 200-6;
                if (i&1)
                {
                    x = 320-(78+(i>>1)*10);
                    nStat |= 512;
                }
                else
                {
                    x = 73+(i>>1)*10;
                    nStat |= 256;
                }
                if (pPlayer->hasKey[i+1])
                    DrawStatSprite(nTile, x, y, 0, 0, nStat);
#if 0
                else
                    DrawStatSprite(nTile, x, y, 40, 5, nStat);
#endif
            }
        }
        viewDrawStats(pPlayer, 2, 140);
        viewDrawPowerUps(pPlayer);
    }
    else if (gViewSize > 2)
    {
        viewDrawPack(pPlayer, 160, 200-tilesiz[2200].y);
        DrawStatMaskedSprite(2200, 160, 172, 16, nPalette);
        DrawPackItemInStatusBar(pPlayer, 265, 186, 260, 172);
        if (pXSprite->health >= 16 || ((int)totalclock&16) || pXSprite->health == 0)
        {
            DrawStatNumber("%3d", pXSprite->health>>4, 2190, 86, 183, 0, 0);
        }
        if (pPlayer->curWeapon && pPlayer->weaponAmmo != -1)
        {
            int num = pPlayer->ammoCount[pPlayer->weaponAmmo];
            if (pPlayer->weaponAmmo == 6)
                num /= 10;
            DrawStatNumber("%3d", num, 2240, 216, 183, 0, 0);
        }
        for (int i = 9; i >= 1; i--)
        {
            int x = 135+((i-1)/3)*23;
            int y = 182+((i-1)%3)*6;
            int num = pPlayer->ammoCount[i];
            if (i == 6)
                num /= 10;
            if (i == pPlayer->weaponAmmo)
            {
                DrawStatNumber("%3d", num, 2230, x, y, -128, 10);
            }
            else
            {
                DrawStatNumber("%3d", num, 2230, x, y, 32, 10);
            }
        }

        if (pPlayer->weaponAmmo == 10)
        {
            DrawStatNumber("%2d", pPlayer->ammoCount[10], 2230, 291, 194, -128, 10);
        }
        else
        {
            DrawStatNumber("%2d", pPlayer->ammoCount[10], 2230, 291, 194, 32, 10);
        }

        if (pPlayer->weaponAmmo == 11)
        {
            DrawStatNumber("%2d", pPlayer->ammoCount[11], 2230, 309, 194, -128, 10);
        }
        else
        {
            DrawStatNumber("%2d", pPlayer->ammoCount[11], 2230, 309, 194, 32, 10);
        }

        if (pPlayer->armor[1])
        {
            TileHGauge(2207, 44, 174, pPlayer->armor[1], 3200);
            DrawStatNumber("%3d", pPlayer->armor[1]>>4, 2230, 50, 177, 0, 0);
        }
        if (pPlayer->armor[0])
        {
            TileHGauge(2209, 44, 182, pPlayer->armor[0], 3200);
            DrawStatNumber("%3d", pPlayer->armor[0]>>4, 2230, 50, 185, 0, 0);
        }
        if (pPlayer->armor[2])
        {
            TileHGauge(2208, 44, 190, pPlayer->armor[2], 3200);
            DrawStatNumber("%3d", pPlayer->armor[2]>>4, 2230, 50, 193, 0, 0);
        }
        viewDrawText(3, gVersionString, 20, 191, 32, gVersionPal, 1, 0);

        for (int i = 0; i < 6; i++)
        {
            int nTile = 2220+i;
            int x = 73+(i&1)*173;
            int y = 171+(i>>1)*11;
            if (pPlayer->hasKey[i+1])
                DrawStatSprite(nTile, x, y);
            else
                DrawStatSprite(nTile, x, y, 40, 5);
        }
        DrawStatMaskedSprite(2202, 118, 185, pPlayer->isRunning ? 16 : 40);
        DrawStatMaskedSprite(2202, 201, 185, pPlayer->isRunning ? 16 : 40);
        if (pPlayer->throwPower && pXSprite->health > 0)
        {
            TileHGauge(2260, 124, 175, pPlayer->throwPower, 65536);
        }
        viewDrawStats(pPlayer, 2, 140);
        viewDrawPowerUps(pPlayer);
    }

    if (gGameOptions.nGameType == kGameTypeSinglePlayer) return;

    if (gGameOptions.nGameType == kGameTypeTeams)
    {
        if (VanillaMode())
        {
            viewDrawCtfHudVanilla(arg);
        }
        else
        {
            viewDrawCtfHud(arg);
            viewDrawPlayerFlags();
        }
    }
    else
    {
        viewDrawPlayerFrags();
    }
}

void viewPrecacheTiles(void)
{
    tilePrecacheTile(2173, 0);
    tilePrecacheTile(2200, 0);
    tilePrecacheTile(2201, 0);
    tilePrecacheTile(2202, 0);
    tilePrecacheTile(2207, 0);
    tilePrecacheTile(2208, 0);
    tilePrecacheTile(2209, 0);
    tilePrecacheTile(2229, 0);
    tilePrecacheTile(2260, 0);
    tilePrecacheTile(2559, 0);
    tilePrecacheTile(2169, 0);
    tilePrecacheTile(2578, 0);
    tilePrecacheTile(2586, 0);
    tilePrecacheTile(2602, 0);
    for (int i = 0; i < 10; i++)
    {
        tilePrecacheTile(2190 + i, 0);
        tilePrecacheTile(2230 + i, 0);
        tilePrecacheTile(2240 + i, 0);
        tilePrecacheTile(2250 + i, 0);
        tilePrecacheTile(kSBarNumberHealth + i, 0);
        tilePrecacheTile(kSBarNumberAmmo + i, 0);
        tilePrecacheTile(kSBarNumberInv + i, 0);
        tilePrecacheTile(kSBarNumberArmor1 + i, 0);
        tilePrecacheTile(kSBarNumberArmor2 + i, 0);
        tilePrecacheTile(kSBarNumberArmor3 + i, 0);
    }
    for (int i = 0; i < 6; i++)
    {
        tilePrecacheTile(kSBarNegative + i, 0);
    }
    for (int i = 0; i < kPackMax; i++)
    {
        tilePrecacheTile(gPackIcons[i], 0);
        tilePrecacheTile(gPackIcons2[i].nTile, 0);
    }
    for (int i = 0; i < 6; i++)
    {
        tilePrecacheTile(2220 + i, 0);
        tilePrecacheTile(2552 + i, 0);
    }
}

int *lensTable;

int gZoom = 1024;

int dword_172CE0[16][3];

void viewInit(void)
{
    LOG_F(INFO, "Initializing status bar");
    InitStatusBar();
    FontSet(0, 4096, 0);
    FontSet(1, 4192, 1);
    FontSet(2, 4288, 1);
    FontSet(3, 4384, 1);
    FontSet(4, 4480, 0);

    DICTNODE *hLens = gSysRes.Lookup("LENS", "DAT");
    dassert(hLens != NULL);
    dassert(gSysRes.Size(hLens) == kLensSize * kLensSize * sizeof(int));

    lensTable = (int*)gSysRes.Lock(hLens);
#if B_BIG_ENDIAN == 1
    for (int i = 0; i < kLensSize*kLensSize; i++)
    {
        lensTable[i] = B_LITTLE32(lensTable[i]);
    }
#endif
    char *data = tileAllocTile(LENSBUFFER, kLensSize, kLensSize, 0, 0);
    memset(data, 255, kLensSize*kLensSize);
    gGameMessageMgr.SetState(gMessageState);
    gGameMessageMgr.SetCoordinates(1, 1);
    char nFont;
    if (gMessageFont == 0)
        nFont = 3;
    else
        nFont = 0;

    gGameMessageMgr.SetFont(nFont);
    gGameMessageMgr.SetMaxMessages(gMessageCount);
    gGameMessageMgr.SetMessageTime(gMessageTime);

    for (int i = 0; i < 16; i++)
    {
        dword_172CE0[i][0] = mulscale16(wrand(), 2048);
        dword_172CE0[i][1] = mulscale16(wrand(), 2048);
        dword_172CE0[i][2] = mulscale16(wrand(), 2048);
    }
    gViewMap.Init(0, 0, gZoom, 0, gFollowMap);

    g_frameDelay = calcFrameDelay(r_maxfps);

    bLoadScreenCrcMatch = tileGetCRC32(kLoadScreen) == kLoadScreenCRC;
}

void viewResizeView(int size)
{
    const char bDrawFragsBg = (gGameOptions.nGameType != kGameTypeSinglePlayer) && (!VanillaMode() || gGameOptions.nGameType != kGameTypeTeams);
    int xdimcorrect = ClipHigh(scale(ydim, 4, 3), xdim);
    gViewXCenter = xdim-xdim/2;
    gViewYCenter = ydim-ydim/2;
    xscale = divscale16(xdim, 320);
    xscalecorrect = divscale16(xdimcorrect, 320);
    yscale = divscale16(ydim, 200);
    xstep = divscale16(320, xdim);
    ystep = divscale16(200, ydim);
    gViewSize = ClipRange(size, 0, 7);
    if (gViewSize <= 2)
    {
        gViewX0 = 0;
        gViewX1 = xdim-1;
        gViewY0 = 0;
        gViewY1 = ydim-1;
        if (bDrawFragsBg)
        {
            gViewY0 = (tilesiz[2229].y*ydim*((gNetPlayers+3)/4))/200;
        }
        gViewX0S = divscale16(gViewX0, xscalecorrect);
        gViewY0S = divscale16(gViewY0, yscale);
        gViewX1S = divscale16(gViewX1, xscalecorrect);
        gViewY1S = divscale16(gViewY1, yscale);
    }
    else
    {
        gViewX0 = 0;
        gViewY0 = 0;
        gViewX1 = xdim-1;
        gViewY1 = ydim-1-(25*ydim)/200;
        if (bDrawFragsBg)
        {
            gViewY0 = (tilesiz[2229].y*ydim*((gNetPlayers+3)/4))/200;
        }

        int height = gViewY1-gViewY0;
        gViewX0 += mulscale16(xdim*(gViewSize-3),4096);
        gViewX1 -= mulscale16(xdim*(gViewSize-3),4096);
        gViewY0 += mulscale16(height*(gViewSize-3),4096);
        gViewY1 -= mulscale16(height*(gViewSize-3),4096);
        gViewX0S = divscale16(gViewX0, xscalecorrect);
        gViewY0S = divscale16(gViewY0, yscale);
        gViewX1S = divscale16(gViewX1, xscalecorrect);
        gViewY1S = divscale16(gViewY1, yscale);
    }
    videoSetViewableArea(gViewX0, gViewY0, gViewX1, gViewY1);
    if (gViewMode == 4) // 2D map view
    {
        int nOffset = bDrawFragsBg && !VanillaMode() ? (tilesiz[2229].y*ydim*((gNetPlayers+3)/4))/200 : 0;
        nOffset = divscale16(nOffset, yscale);
        nOffset += gGameOptions.nGameType == kGameTypeSinglePlayer && !VanillaMode() ? 6 : 1;
        gGameMessageMgr.SetCoordinates(1, nOffset);
    }
    else
    {
        int nOffset = 1;
        if ((gGameOptions.nGameType == kGameTypeTeams) && VanillaMode()) // lower text for vanilla CTF hud (v1.21 did not do this)
            nOffset = 15;
        else if ((gGameOptions.nGameType == kGameTypeSinglePlayer) && (gViewSize < 4) && !VanillaMode()) // lower message position for single-player
            nOffset = 6;
        gGameMessageMgr.SetCoordinates(gViewX0S + 1, gViewY0S + nOffset);
    }
    viewSetCrosshairColor(CrosshairColors.r, CrosshairColors.g, CrosshairColors.b);
    viewBurnTimeInit();
    viewUpdatePages();
}

#define kBackTile 253

void UpdateFrame(void)
{
    viewTileSprite(kBackTile, 0, 0, 0, 0, xdim, gViewY0-3);
    viewTileSprite(kBackTile, 0, 0, 0, gViewY1+4, xdim, ydim);
    viewTileSprite(kBackTile, 0, 0, 0, gViewY0-3, gViewX0-3, gViewY1+4);
    viewTileSprite(kBackTile, 0, 0, gViewX1+4, gViewY0-3, xdim, gViewY1+4);

    viewTileSprite(kBackTile, 20, 0, gViewX0-3, gViewY0-3, gViewX0, gViewY1+1);
    viewTileSprite(kBackTile, 20, 0, gViewX0, gViewY0-3, gViewX1+4, gViewY0);
    viewTileSprite(kBackTile, 10, 1, gViewX1+1, gViewY0, gViewX1+4, gViewY1+4);
    viewTileSprite(kBackTile, 10, 1, gViewX0-3, gViewY1+1, gViewX1+1, gViewY1+4);
}

void viewDrawInterface(ClockTicks arg)
{
    const char bDrawFragsBg = (gGameOptions.nGameType != kGameTypeSinglePlayer) && (!VanillaMode() || gGameOptions.nGameType != kGameTypeTeams);
    if (gViewMode == 3 && (gViewSize >= 3 || bDrawFragsBg) && (pcBackground != 0 || videoGetRenderMode() >= REND_POLYMOST))
    {
        UpdateFrame();
        pcBackground--;
    }
    UpdateStatusBar(arg);
}

fix16_t gCameraAng = 0;


int effectDetail[kViewEffectMax] = {
    4, 4, 4, 4, 0, 0, 0, 0, 0, 1, 4, 4, 0, 0, 0, 1, 0, 0, 0
};

char viewUseItemRespawnMarkers(spritetype* pSpr)
{
#ifdef NOONE_EXTENSIONS
    if (IsUserItemSprite(pSpr))
        return userItemViewUseRespawnMarkers(userItemGet(pSpr->type));
#endif

    if ((IsItemSprite(pSpr) || IsAmmoSprite(pSpr))
        && gGameOptions.nItemSettings == 2)
            return 1;

    if (IsWeaponSprite(pSpr)
        && gGameOptions.nWeaponSettings == 3)
            return 1;

    return 0;
}

tspritetype *viewAddEffect(int nTSprite, VIEW_EFFECT nViewEffect)
{
    dassert(nViewEffect >= 0 && nViewEffect < kViewEffectMax);
    auto pTSprite = &tsprite[nTSprite];
    if (gDetail < effectDetail[nViewEffect] || nTSprite >= kMaxViewSprites) return NULL;
    switch (nViewEffect)
    {
#ifdef NOONE_EXTENSIONS
    case kViewEffectSpotProgress: {
        XSPRITE* pXSprite = &xsprite[pTSprite->extra];
        int perc = (100 * pXSprite->data3) / kMaxPatrolSpotValue;
        int width = (94 * pXSprite->data3) / kMaxPatrolSpotValue;

        int top, bottom;
        GetSpriteExtents(pTSprite, &top, &bottom);

        if (videoGetRenderMode() != REND_CLASSIC) {
            
            auto pNSprite2 = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
            if (!pNSprite2)
                break;

            pNSprite2->picnum = 2203;

            pNSprite2->xrepeat = width;
            pNSprite2->yrepeat = 20;
            pNSprite2->pal = 10;
            if (perc >= 75) pNSprite2->pal = 0;
            else if (perc >= 50) pNSprite2->pal = 6;
            
            pNSprite2->z = top - 2048;
            pNSprite2->shade = -128;


        } else {
            

            auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
            auto pNSprite2 = viewInsertTSprite(pTSprite->sectnum, 32766, pTSprite);
            if (!pNSprite || !pNSprite2)
                break;
            pNSprite->cstat |= CSTAT_SPRITE_TRANSLUCENT_INVERT | CSTAT_SPRITE_TRANSLUCENT;

            pNSprite->picnum = 2229;
            pNSprite2->picnum = 2203;

            pNSprite->xoffset = -1;
            pNSprite->xrepeat = 40;
            pNSprite->yrepeat = 64;
            pNSprite->pal = 5;

            pNSprite2->xrepeat = width;
            pNSprite2->yrepeat = 34;
            pNSprite2->pal = 10;
            if (perc >= 75) pNSprite2->pal = 0;
            else if (perc >= 50) pNSprite2->pal = 6;

            pNSprite->z = pNSprite2->z = top - 2048;
            pNSprite->shade = pNSprite2->shade = -128;

        }
        break;
    }
#endif
    case kViewEffectAtom:
        for (int i = 0; i < 16; i++)
        {
            auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
            if (!pNSprite)
                break;
            int ang = ((int)gFrameClock*2048)/120;
            int nRand1 = dword_172CE0[i][0];
            int nRand2 = dword_172CE0[i][1];
            int nRand3 = dword_172CE0[i][2];
            ang += nRand3;
            int x = mulscale30(512, Cos(ang));
            int y = mulscale30(512, Sin(ang));
            int z = 0;
            RotateYZ(&x, &y, &z, nRand1);
            RotateXZ(&x, &y, &z, nRand2);
            pNSprite->x = pTSprite->x + x;
            pNSprite->y = pTSprite->y + y;
            pNSprite->z = pTSprite->z + (z<<4);
            pNSprite->picnum = 1720;
            pNSprite->shade = -128;
        }
        break;
    case kViewEffectFlag:
    case kViewEffectBigFlag:
    {
        int top, bottom;
        GetSpriteExtents(pTSprite, &top, &bottom);
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->shade = -128;
        pNSprite->pal = 0;
        pNSprite->z = top;
        if (nViewEffect == kViewEffectFlag)
            pNSprite->xrepeat = pNSprite->yrepeat = 24;
        else
            pNSprite->xrepeat = pNSprite->yrepeat = 64;
        pNSprite->picnum = 3558;
        return pNSprite;
    }
    case kViewEffectTesla:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->z = pTSprite->z;
        pNSprite->cstat |= 2;
        pNSprite->shade = -128;
        pNSprite->xrepeat = pTSprite->xrepeat;
        pNSprite->yrepeat = pTSprite->yrepeat;
        pNSprite->picnum = 2135;
        break;
    }
    case kViewEffectShoot:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->shade = -128;
        pNSprite->pal = 0;
        pNSprite->xrepeat = pNSprite->yrepeat = 64;
        pNSprite->picnum = 2605;
        return pNSprite;
    }
    case kViewEffectReflectiveBall:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->shade = 26;
        pNSprite->pal = 0;
        pNSprite->cstat |= 2;
        pNSprite->xrepeat = pNSprite->yrepeat = 64;
        pNSprite->picnum = 2089;
        break;
    }
    case kViewEffectPhase:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        int top, bottom;
        GetSpriteExtents(pTSprite, &top, &bottom);
        pNSprite->shade = 26;
        pNSprite->pal = 0;
        pNSprite->cstat |= 2;
        pNSprite->xrepeat = pNSprite->yrepeat = 24;
        pNSprite->picnum = 626;
        pNSprite->z = top;
        break;
    }
    case kViewEffectTrail:
    {
        int nAng = pTSprite->ang;
        if (pTSprite->cstat & 16)
        {
            nAng = (nAng+512)&2047;
        }
        else
        {
            nAng = (nAng+1024)&2047;
        }
        for (int i = 0; i < 5; i++)
        {
            int nSector = pTSprite->sectnum;
            auto pNSprite = viewInsertTSprite<tspritetype>(nSector, 32767, NULL);
            if (!pNSprite)
                break;
            int nLen = 128+(i<<7);
            int x = mulscale30(nLen, Cos(nAng));
            pNSprite->x = pTSprite->x + x;
            int y = mulscale30(nLen, Sin(nAng));
            pNSprite->y = pTSprite->y + y;
            pNSprite->z = pTSprite->z;
            dassert(nSector >= 0 && nSector < kMaxSectors);
            FindSector(pNSprite->x, pNSprite->y, pNSprite->z, &nSector);
            pNSprite->sectnum = nSector;
            pNSprite->owner = pTSprite->owner;
            pNSprite->picnum = pTSprite->picnum;
            pNSprite->cstat |= 2;
            if (i < 2)
                pNSprite->cstat |= 514;
            pNSprite->shade = ClipLow(pTSprite->shade-16, -128);
            pNSprite->xrepeat = pTSprite->xrepeat;
            pNSprite->yrepeat = pTSprite->yrepeat;
            pNSprite->picnum = pTSprite->picnum;
        }
        break;
    }
    case kViewEffectFlame:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->shade = -128;
        pNSprite->z = pTSprite->z;
        pNSprite->picnum = 908;
        pNSprite->statnum = kStatDecoration;
        pNSprite->xrepeat = pNSprite->yrepeat = (tilesiz[pTSprite->picnum].x*pTSprite->xrepeat)/64;
        break;
    }
    case kViewEffectSmokeHigh:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        int top, bottom;
        GetSpriteExtents(pTSprite, &top, &bottom);
        pNSprite->z = top;
        if (IsDudeSprite(pTSprite))
            pNSprite->picnum = 672;
        else
            pNSprite->picnum = 754;
        pNSprite->cstat |= 2;
        pNSprite->shade = 8;
        pNSprite->xrepeat = pTSprite->xrepeat;
        pNSprite->yrepeat = pTSprite->yrepeat;
        break;
    }
    case kViewEffectSmokeLow:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        int top, bottom;
        GetSpriteExtents(pTSprite, &top, &bottom);
        pNSprite->z = bottom;
        if (pTSprite->type >= kDudeBase && pTSprite->type < kDudeMax)
            pNSprite->picnum = 672;
        else
            pNSprite->picnum = 754;
        pNSprite->cstat |= 2;
        pNSprite->shade = 8;
        pNSprite->xrepeat = pTSprite->xrepeat;
        pNSprite->yrepeat = pTSprite->yrepeat;
        break;
    }
    case kViewEffectTorchHigh:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        int top, bottom;
        GetSpriteExtents(pTSprite, &top, &bottom);
        pNSprite->z = top;
        pNSprite->picnum = 2101;
        pNSprite->shade = -128;
        pNSprite->xrepeat = pNSprite->yrepeat = (tilesiz[pTSprite->picnum].x*pTSprite->xrepeat)/32;
        break;
    }
    case kViewEffectTorchLow:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        int top, bottom;
        GetSpriteExtents(pTSprite, &top, &bottom);
        pNSprite->z = bottom;
        pNSprite->picnum = 2101;
        pNSprite->shade = -128;
        pNSprite->xrepeat = pNSprite->yrepeat = (tilesiz[pTSprite->picnum].x*pTSprite->xrepeat)/32;
        break;
    }
    case kViewEffectShadow:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->z = getflorzofslope(pTSprite->sectnum, pNSprite->x, pNSprite->y);
        if (!VanillaMode()) // support better floor detection for shadows (detect fake floors/allows ROR traversal)
        {
            char bHitFakeFloor = 0;
            short nFakeFloorSprite;
            if (spriRangeIsFine(pTSprite->owner) && !gMirrorDrawing) // don't attempt to check for fake floors if we're rendering a mirror due to getzrange mirrorsector crash
            {
                spritetype *pSprite = &sprite[pTSprite->owner];
                int bakCstat = pSprite->cstat;
                pSprite->cstat &= ~257;
                int ceilZ, ceilHit, floorZ, floorHit;
                GetZRangeAtXYZ(pSprite->x, pSprite->y, pSprite->z, pSprite->sectnum, &ceilZ, &ceilHit, &floorZ, &floorHit, pSprite->clipdist<<2, CLIPMASK0, PARALLAXCLIP_CEILING|PARALLAXCLIP_FLOOR);
                nFakeFloorSprite = floorHit&0x3fff;
                if ((floorHit&0xc000) == 0xc000)
                    bHitFakeFloor = (sprite[nFakeFloorSprite].cstat & (CSTAT_SPRITE_BLOCK|CSTAT_SPRITE_ALIGNMENT_FLOOR|CSTAT_SPRITE_INVISIBLE)) == (CSTAT_SPRITE_BLOCK|CSTAT_SPRITE_ALIGNMENT_FLOOR);
                pSprite->cstat = bakCstat;
            }
            if (bHitFakeFloor) // if there is a fake floor under us, use fake floor as the shadow position
            {
                int top, bottom;
                GetSpriteExtents(&sprite[nFakeFloorSprite], &top, &bottom);
                pNSprite->z = top;
                pNSprite->z--; // offset from fake floor so it isn't z-fighting when being rendered
            }
            else if ((sector[pNSprite->sectnum].floorpicnum >= 4080) && (sector[pNSprite->sectnum].floorpicnum <= 4095)) // if floor has ror, find actual floor
            {
                int cX = pNSprite->x, cY = pNSprite->y, cZ = pNSprite->z, cZrel = pNSprite->z, nSectnum = pNSprite->sectnum;
                for (int i = 0; i < 16; i++) // scan through max stacked sectors
                {
                    if (!CheckLink(&cX, &cY, &cZ, &nSectnum)) // if no more floors underneath, abort
                        break;
                    const int newFloorZ = getflorzofslope(nSectnum, cX, cZ);
                    cZrel += newFloorZ - cZ; // get height difference for next sector's ceiling/floor, and add to relative height for shadow
                    if ((sector[nSectnum].floorpicnum < 4080) || (sector[nSectnum].floorpicnum > 4095)) // if current sector is not open air, use as floor for shadow casting, otherwise continue to next sector
                        break;
                    cZ = newFloorZ;
                }
                pNSprite->z = cZrel;
            }
        }
        pNSprite->shade = 127;
        pNSprite->cstat |= CSTAT_SPRITE_TRANSLUCENT;
        pNSprite->xrepeat = pTSprite->xrepeat;
        pNSprite->yrepeat = pTSprite->yrepeat>>2;
        pNSprite->picnum = pTSprite->picnum;
        if (!VanillaMode() && (pTSprite->type == kThingDroppedLifeLeech)) // fix shadow for thrown lifeleech
            pNSprite->picnum = 800;
        pNSprite->pal = 5;
        int height = tilesiz[pNSprite->picnum].y;
        int center = height/2+picanm[pNSprite->picnum].yofs;
        pNSprite->z -= (pNSprite->yrepeat<<2)*(height-center);
        break;
    }
    case kViewEffectFlareHalo:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->shade = -128;
        pNSprite->pal = 2;
        pNSprite->cstat |= 2;
        pNSprite->z = pTSprite->z;
        pNSprite->xrepeat = pTSprite->xrepeat;
        pNSprite->yrepeat = pTSprite->yrepeat;
        pNSprite->picnum = 2427;
        break;
    }
    case kViewEffectCeilGlow:
    {
        sectortype *pSector = &sector[pTSprite->sectnum];
        if (!VanillaMode()) // if ceiling has ror, don't render effect
        {
            if ((pSector->ceilingpicnum >= 4080) && (pSector->ceilingpicnum <= 4095))
                break;
        }
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->x = pTSprite->x;
        pNSprite->y = pTSprite->y;
        pNSprite->z = pSector->ceilingz;
        pNSprite->picnum = 624;
        pNSprite->shade = ((pTSprite->z-pSector->ceilingz)>>8)-64;
        pNSprite->pal = 2;
        pNSprite->xrepeat = pNSprite->yrepeat = 64;
        pNSprite->cstat |= 106;
        pNSprite->ang = pTSprite->ang;
        pNSprite->owner = pTSprite->owner;
        break;
    }
    case kViewEffectFloorGlow:
    {
        sectortype *pSector = &sector[pTSprite->sectnum];
        if (!VanillaMode()) // if floor has ror, don't render effect
        {
            if ((pSector->floorpicnum >= 4080) && (pSector->floorpicnum <= 4095))
                break;
        }
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->x = pTSprite->x;
        pNSprite->y = pTSprite->y;
        pNSprite->z = pSector->floorz;
        pNSprite->picnum = 624;
        char nShade = (pSector->floorz-pTSprite->z)>>8; 
        pNSprite->shade = nShade-32;
        pNSprite->pal = 2;
        pNSprite->xrepeat = pNSprite->yrepeat = nShade;
        pNSprite->cstat |= 98;
        pNSprite->ang = pTSprite->ang;
        pNSprite->owner = pTSprite->owner;
        break;
    }
    case kViewEffectSpear:
    {
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->z = pTSprite->z;
        if (gDetail > 1)
            pNSprite->cstat |= 514;
        pNSprite->shade = ClipLow(pTSprite->shade-32, -128);
        pNSprite->xrepeat = pTSprite->xrepeat;
        pNSprite->yrepeat = 64;
        pNSprite->picnum = 775;
        break;
    }
    case kViewEffectShowWeapon:
    {
        dassert(pTSprite->type >= kDudePlayer1 && pTSprite->type <= kDudePlayer8);
        PLAYER *pPlayer = &gPlayer[pTSprite->type-kDudePlayer1];
        WEAPONICON weaponIcon = gWeaponIcon[pPlayer->curWeapon];
        const int nTile = weaponIcon.nTile;
        if (nTile < 0)
            break;
        if (pPlayer->pXSprite->health == 0)
            break;
        auto pNSprite = viewInsertTSprite(pTSprite->sectnum, 32767, pTSprite);
        if (!pNSprite)
            break;
        pNSprite->x = pTSprite->x;
        pNSprite->y = pTSprite->y;
        pNSprite->z = pTSprite->z-(32<<8);
        pNSprite->picnum = nTile;
        pNSprite->shade = pTSprite->shade;
        pNSprite->xrepeat = 32;
        pNSprite->yrepeat = 32;
        pNSprite->ang = (gCameraAng + kAng90) & kAngMask; // always face viewer
        if (VanillaMode())
            break;
        const int nVoxel = voxelIndex[nTile];
        if (gShowWeapon == 2 && usevoxels && gDetail >= 4 && videoGetRenderMode() != REND_POLYMER && nVoxel != -1)
        {
            pNSprite->clipdist |= TSPR_FLAGS_SLAB;
            pNSprite->cstat &= ~(8|CSTAT_SPRITE_ALIGNMENT);
            pNSprite->picnum = nVoxel;
            if (pPlayer->curWeapon == kWeaponLifeLeech) // position lifeleech behind player
            {
                pNSprite->x += mulscale30(128, Cos(gCameraAng));
                pNSprite->y += mulscale30(128, Sin(gCameraAng));
            }
            if ((pPlayer->curWeapon == kWeaponLifeLeech) || (pPlayer->curWeapon == kWeaponVoodoo)) // make lifeleech/voodoo doll always face viewer like sprite
                pNSprite->ang = (gCameraAng + kAng180) & kAngMask;
            else if ((pPlayer->curWeapon == kWeaponProxyTNT) || (pPlayer->curWeapon == kWeaponRemoteTNT)) // make proxy/remote tnt always face viewers like sprite
                pNSprite->ang = (gCameraAng + kAng180 + kAng45) & kAngMask;
            pNSprite->z -= gWeaponIconVoxel[pPlayer->curWeapon].zOffset<<8; // offset up
        }
        else
            pNSprite->z -= weaponIcon.zOffset<<8; // offset up
        break;
    }
    }
    return NULL;
}

LOCATION gPrevSpriteLoc[kMaxSprites];
static LOCATION gViewSpritePredictLoc;

inline void viewApplyFloorPal(tspritetype *pTSprite, uint8_t nPal)
{
    if (nPal == 0 && !VanillaMode()) // keep original sprite's palette when floors are using default palette (fixes tommy gun cultists in E3M2)
        return;
    pTSprite->pal = nPal;
}

void viewProcessSprites(int32_t cX, int32_t cY, int32_t cZ, int32_t cA, int32_t smooth)
{
    UNREFERENCED_PARAMETER(smooth);
    dassert(spritesortcnt <= kMaxViewSprites);
    gCameraAng = cA;
    int nViewSprites = spritesortcnt;
    for (int nTSprite = spritesortcnt-1; nTSprite >= 0; nTSprite--)
    {
        tspritetype *pTSprite = &tsprite[nTSprite];
        //int nXSprite = pTSprite->extra;
        int nSprite = pTSprite->owner;
        int nXSprite = sprite[nSprite].extra;
        XSPRITE *pTXSprite = NULL;
        if ((qsprite_filler[nSprite] > gDetail) || (sprite[nSprite].sectnum == -1))
        {
            pTSprite->xrepeat = 0;
            continue;
        }
        if (nXSprite > 0)
        {
            pTXSprite = &xsprite[nXSprite];
        }
        int nTile = pTSprite->picnum;
        if (nTile < 0 || nTile >= kMaxTiles)
        {
            continue;
        }

        auto const tsprflags = pTSprite->clipdist;

        if (gView && (gView->pSprite == &sprite[nSprite]) && IsPlayerSprite(pTSprite) && gViewInterpolate && !VanillaMode()) // improve network player prediction while in third person/co-op view
        {
            pTSprite->x = gViewSpritePredictLoc.x;
            pTSprite->y = gViewSpritePredictLoc.y;
            pTSprite->z = gViewSpritePredictLoc.z;
            pTSprite->ang = fix16_to_int(gViewSpritePredictLoc.ang);
        }
        else if (gViewInterpolate && TestBitString(gInterpolateSprite, nSprite) && !(pTSprite->flags&512))
        {
            LOCATION *pPrevLoc = &gPrevSpriteLoc[nSprite];
            pTSprite->x = interpolate(pPrevLoc->x, pTSprite->x, gInterpolate);
            pTSprite->y = interpolate(pPrevLoc->y, pTSprite->y, gInterpolate);
            pTSprite->z = interpolate(pPrevLoc->z, pTSprite->z, gInterpolate);
            pTSprite->ang = pPrevLoc->ang+mulscale16(((pTSprite->ang-pPrevLoc->ang+1024)&2047)-1024, gInterpolate);
        }
        int nAnim = 0;
        switch (picanm[nTile].extra & 7) {
            case 0:
                //dassert(nXSprite > 0 && nXSprite < kMaxXSprites);
                if (nXSprite <= 0 || nXSprite >= kMaxXSprites) break;
                switch (pTSprite->type) {
                    #ifdef NOONE_EXTENSIONS
                    case kModernCondition:
                    case kModernConditionFalse:
                        if (!gModernMap) break;
                        fallthrough__;
                    #endif
                    case kSwitchToggle:
                    case kSwitchOneWay:
                        if (xsprite[nXSprite].state) nAnim = 1;
                        break;
                    case kSwitchCombo:
                        nAnim = xsprite[nXSprite].data1;
                        break;
                }
                break;
            case 1:
            {
                if (tilehasmodelorvoxel(pTSprite->picnum, pTSprite->pal) && !(spriteext[nSprite].flags&SPREXT_NOTMD))
                {
                    pTSprite->cstat &= ~4;
                    break;
                }
                int dX = cX - pTSprite->x;
                int dY = cY - pTSprite->y;
                RotateVector(&dX, &dY, 128-pTSprite->ang);
                nAnim = GetOctant(dX, dY);
                if (nAnim <= 4)
                {
                    pTSprite->cstat &= ~4;
                }
                else
                {
                    nAnim = 8 - nAnim;
                    pTSprite->cstat |= 4;
                }
                break;
            }
            case 2:
            {
                if (tilehasmodelorvoxel(pTSprite->picnum, pTSprite->pal) && !(spriteext[nSprite].flags&SPREXT_NOTMD))
                {
                    pTSprite->cstat &= ~4;
                    break;
                }
                int dX = cX - pTSprite->x;
                int dY = cY - pTSprite->y;
                RotateVector(&dX, &dY, 128-pTSprite->ang);
                nAnim = GetOctant(dX, dY);
                break;
            }
            case 3:
            {
                if (nXSprite > 0)
                {
                    if (gSpriteHit[nXSprite].florhit == 0)
                        nAnim = 1;
                }
                else
                {
                    int top, bottom;
                    GetSpriteExtents(pTSprite, &top, &bottom);
                    if (getflorzofslope(pTSprite->sectnum, pTSprite->x, pTSprite->y) > bottom)
                        nAnim = 1;
                }
                break;
            }
            case 6:
            case 7:
            {
#ifdef USE_OPENGL
                if (videoGetRenderMode() >= REND_POLYMOST && usemodels && md_tilehasmodel(pTSprite->picnum, pTSprite->pal) >= 0 && !(spriteext[nSprite].flags&SPREXT_NOTMD))
                    break;
#endif
                // Can be overridden by def script
                if (usevoxels && gDetail >= 4 && videoGetRenderMode() != REND_POLYMER && tiletovox[pTSprite->picnum] == -1 && voxelIndex[pTSprite->picnum] != -1 && !(spriteext[nSprite].flags&SPREXT_NOTMD))
                {
                    if ((pTSprite->flags&kHitagRespawn) == 0)
                    {
                        pTSprite->clipdist |= TSPR_FLAGS_SLAB;
                        pTSprite->cstat &= ~(4|8|CSTAT_SPRITE_ALIGNMENT);
                        pTSprite->yoffset += picanm[pTSprite->picnum].yofs;
                        pTSprite->picnum = voxelIndex[pTSprite->picnum];
                        if (!voxoff[pTSprite->picnum][0])
                            qloadvoxel(pTSprite->picnum);
                        if ((picanm[nTile].extra&7) == 7)
                        {
                            pTSprite->ang = ((int)totalclock<<3)&2047;
                        }
                    }
                }
                break;
            }
        }
        while (nAnim > 0)
        {
            pTSprite->picnum += picanm[pTSprite->picnum].num+1;
            nAnim--;
        }

        if (!(tsprflags & TSPR_FLAGS_SLAB) && usevoxels && videoGetRenderMode() != REND_POLYMER && !(spriteext[nSprite].flags&SPREXT_NOTMD))
        {
            int const nRootTile = pTSprite->picnum;
#if 0
            int nAnimTile = pTSprite->picnum + animateoffs_replace(pTSprite->picnum, 32768+nSprite);
            if (tiletovox[nAnimTile] != -1)
            {
                pTSprite->yoffset += picanm[nAnimTile].yofs;
                pTSprite->xoffset += picanm[nAnimTile].xofs;
            }
#endif

            int const nVoxel = tiletovox[pTSprite->picnum];

            if (nVoxel != -1 && ((voxflags[nVoxel] & VF_ROTATE) || (picanm[nRootTile].extra&7) == 7))
                pTSprite->ang = (pTSprite->ang+((int)totalclock<<3))&2047;
        }

#ifdef USE_OPENGL
        if (!(tsprflags & TSPR_FLAGS_SLAB) && usemodels && !(spriteext[nSprite].flags&SPREXT_NOTMD))
        {
            int const nRootTile = pTSprite->picnum;
            int nAnimTile = pTSprite->picnum + animateoffs_replace(pTSprite->picnum, 32768+nSprite);

            if (usemodels && tile2model[Ptile2tile(nAnimTile, pTSprite->pal)].modelid >= 0 &&
                tile2model[Ptile2tile(nAnimTile, pTSprite->pal)].framenum >= 0)
            {
                pTSprite->yoffset += picanm[nAnimTile].yofs;
                pTSprite->xoffset += picanm[nAnimTile].xofs;

                if ((picanm[nRootTile].extra&7) == 7)
                    pTSprite->ang = (pTSprite->ang+((int)totalclock<<3))&2047;
            }
        }
#endif

        sectortype *pSector = &sector[pTSprite->sectnum];
        XSECTOR *pXSector;
        int nShade = pTSprite->shade;
        if (pSector->extra > 0)
        {
            pXSector = &xsector[pSector->extra];
        }
        else
        {
            pXSector = NULL;
        }
        if ((pSector->ceilingstat&1) && (pSector->floorstat&32768) == 0)
        {
            nShade += tileShade[pSector->ceilingpicnum]+pSector->ceilingshade;
        }
        else
        {
            nShade += tileShade[pSector->floorpicnum]+pSector->floorshade;
        }
        nShade += tileShade[pTSprite->picnum];
        pTSprite->shade = ClipRange(nShade, -128, 127);
        if ((pTSprite->flags&kHitagRespawn) && sprite[nSprite].owner == 3)
        {
            dassert(pTXSprite != NULL);
            pTSprite->xrepeat = 48;
            pTSprite->yrepeat = 48;
            pTSprite->shade = -128;
            pTSprite->picnum = 2272 + 2*pTXSprite->respawnPending;
            pTSprite->cstat &= ~514;
            if (viewUseItemRespawnMarkers(&sprite[nSprite]))
            {
                pTSprite->xrepeat = pTSprite->yrepeat = 48;
            }
            else
            {
                pTSprite->xrepeat = pTSprite->yrepeat = 0;
            }
        }
        if (spritesortcnt >= kMaxViewSprites) continue;
        if (pTXSprite && pTXSprite->burnTime > 0)
        {
            pTSprite->shade = ClipRange(pTSprite->shade-16-QRandom(8), -128, 127);
        }
        if (pTSprite->flags&256)
        {
            viewAddEffect(nTSprite, kViewEffectSmokeHigh);
        }
        if (pTSprite->flags&1024)
        {
            pTSprite->cstat |= 4;
        }
        if (pTSprite->flags&2048)
        {
            pTSprite->cstat |= 8;
        }
        switch (pTSprite->statnum) {
        case kStatDecoration: {
            switch (pTSprite->type) {
                case kDecorationCandle:
                    if (!pTXSprite || pTXSprite->state == 1) {
                        pTSprite->shade = -128;
                        viewAddEffect(nTSprite, kViewEffectPhase);
                    } else {
                        pTSprite->shade = -8;
                    }
                    break;
                case kDecorationTorch:
                    if (!pTXSprite || pTXSprite->state == 1) {
                        pTSprite->picnum++;
                        viewAddEffect(nTSprite, kViewEffectTorchHigh);
                    } else {
                        viewAddEffect(nTSprite, kViewEffectSmokeHigh);
                    }
                    break;
                default:
                    if (pXSector && pXSector->color)
                        viewApplyFloorPal(pTSprite, pSector->floorpal);
                    break;
            }
        }
        break;
        case kStatItem: {
            switch (pTSprite->type) {
                case kItemFlagABase:
                    if (pTXSprite && pTXSprite->state > 0 && gGameOptions.nGameType == kGameTypeTeams) {
                        auto pNTSprite = viewAddEffect(nTSprite, kViewEffectBigFlag);
                        if (pNTSprite) pNTSprite->pal = 10;
                    }
                    break;
                case kItemFlagBBase:
                    if (pTXSprite && pTXSprite->state > 0 && gGameOptions.nGameType == kGameTypeTeams) {
                        auto pNTSprite = viewAddEffect(nTSprite, kViewEffectBigFlag);
                        if (pNTSprite) pNTSprite->pal = 7;
                    }
                    break;
                case kItemFlagA:
                    pTSprite->pal = 10;
                    pTSprite->cstat |= 4;
                    break;
                case kItemFlagB:
                    pTSprite->pal = 7;
                    pTSprite->cstat |= 4;
                    break;
                default:
                    if (pTSprite->type >= kItemKeySkull && pTSprite->type < kItemKeyMax)
                        pTSprite->shade = -128;
                    if (pXSector && pXSector->color)
                        viewApplyFloorPal(pTSprite, pSector->floorpal);
                    break;
            }
        }
        break;
        case kStatProjectile: {
            switch (pTSprite->type) {
                case kMissileTeslaAlt:
                    pTSprite->yrepeat = 128;
                    pTSprite->cstat |= 32;
                    break;
                case kMissileTeslaRegular:
                    viewAddEffect(nTSprite, kViewEffectTesla);
                    break;
                case kMissileButcherKnife:
                    viewAddEffect(nTSprite, kViewEffectTrail);
                    break;
                case kMissileFlareRegular:
                case kMissileFlareAlt:
                    if (pTSprite->statnum == kStatFlare) {
                        dassert(pTXSprite != NULL);
                        if (pTXSprite->target == gView->nSprite) {
                            pTSprite->xrepeat = 0;
                            break;
                        }
                    }
                    
                    viewAddEffect(nTSprite, kViewEffectFlareHalo);
                    if (pTSprite->type != kMissileFlareRegular) break;
                    sectortype *pSector = &sector[pTSprite->sectnum];
                    
                    int zDiff = (pTSprite->z - pSector->ceilingz) >> 8;
                    if ((pSector->ceilingstat&1) == 0 && zDiff < 64) {
                        viewAddEffect(nTSprite, kViewEffectCeilGlow);
                    }
                    
                    zDiff = (pSector->floorz - pTSprite->z) >> 8;
                    if ((pSector->floorstat&1) == 0 && zDiff < 64) {
                        viewAddEffect(nTSprite, kViewEffectFloorGlow);
                    }
                    break;
                }
            break;
        }
        case kStatDude:
        {
            if (pTSprite->type == kDudeHand && pTXSprite->aiState == &hand13A3B4)
            {
                spritetype *pTTarget = &sprite[pTXSprite->target];
                dassert(pTXSprite != NULL && pTTarget != NULL);
                if (IsPlayerSprite(pTTarget))
                {
                    pTSprite->xrepeat = 0;
                    break;
                }
            }

            if (pXSector && pXSector->color) viewApplyFloorPal(pTSprite, pSector->floorpal);
            if (powerupCheck(gView, kPwUpBeastVision) > 0) pTSprite->shade = -128;

            if (IsPlayerSprite(pTSprite)) {
                PLAYER *pPlayer = &gPlayer[pTSprite->type-kDudePlayer1];
                if (powerupCheck(pPlayer, kPwUpShadowCloak) && !powerupCheck(gView, kPwUpBeastVision)) {
                    pTSprite->cstat |= 2;
                    pTSprite->pal = 5;
                }  else if (powerupCheck(pPlayer, kPwUpDeathMask)) {
                    pTSprite->shade = -128;
                    pTSprite->pal = 5;
                } else if (powerupCheck(pPlayer, kPwUpDoppleganger)) {
                    pTSprite->pal = 11+(gView->teamId&3);
                }
                
                if (powerupCheck(pPlayer, kPwUpReflectShots)) {
                    viewAddEffect(nTSprite, kViewEffectReflectiveBall);
                }
                
                if (gShowWeapon && gGameOptions.nGameType != kGameTypeSinglePlayer && gView) {
                    viewAddEffect(nTSprite, kViewEffectShowWeapon);
                }
                
                if (pPlayer->flashEffect && (gView != pPlayer || gViewPos != VIEWPOS_0)) {
                    auto pNTSprite = viewAddEffect(nTSprite, kViewEffectShoot);
                    if (pNTSprite) {
                        POSTURE *pPosture = &pPlayer->pPosture[pPlayer->lifeMode][pPlayer->posture];
                        pNTSprite->x += mulscale28(pPosture->zOffset, Cos(pTSprite->ang));
                        pNTSprite->y += mulscale28(pPosture->zOffset, Sin(pTSprite->ang));
                        pNTSprite->z = pPlayer->pSprite->z-pPosture->xOffset;
                    }
                }
                
                if (pPlayer->hasFlag > 0 && gGameOptions.nGameType == kGameTypeTeams) {
                    if (pPlayer->hasFlag&1)  {
                        auto pNTSprite = viewAddEffect(nTSprite, kViewEffectFlag);
                        if (pNTSprite)
                        {
                            pNTSprite->pal = 10;
                            pNTSprite->cstat |= 4;
                        }
                    }
                    if (pPlayer->hasFlag&2) {
                        auto pNTSprite = viewAddEffect(nTSprite, kViewEffectFlag);
                        if (pNTSprite)
                        {
                            pNTSprite->pal = 7;
                            pNTSprite->cstat |= 4;
                        }
                    }
                }
            }
            
            if (nSprite != gView->pSprite->index || gViewPos != VIEWPOS_0 || (gMirrorDrawing && !VanillaMode())) {
                if (getflorzofslope(pTSprite->sectnum, pTSprite->x, pTSprite->y) >= cZ)
                {
                    viewAddEffect(nTSprite, kViewEffectShadow);
                }
            }
            
            #ifdef NOONE_EXTENSIONS
            if (gModernMap) { // add target spot indicator for patrol dudes
                XSPRITE* pTXSprite = &xsprite[pTSprite->extra];
                if (pTXSprite->dudeFlag4 && aiInPatrolState(pTXSprite->aiState) && pTXSprite->data3 > 0 && pTXSprite->data3 <= kMaxPatrolSpotValue)
                    viewAddEffect(nTSprite, kViewEffectSpotProgress);
            }
            #endif
            break;
        }
        case kStatTraps: {
            if (pTSprite->type == kTrapSawCircular) {
                if (pTXSprite->state) {
                    if (pTXSprite->data1) {
                        pTSprite->picnum = 772;
                        if (pTXSprite->data2)
                            viewAddEffect(nTSprite, kViewEffectSpear);
                    }
                } 
                else if (pTXSprite->data1) pTSprite->picnum = 773;
                else pTSprite->picnum = 656;
                
            }
            break;
        }
        case kStatThing: {
            if (pXSector && pXSector->color)
                viewApplyFloorPal(pTSprite, pSector->floorpal);

            if (pTSprite->type < kThingBase || pTSprite->type >= kThingMax || !gSpriteHit[nXSprite].florhit) {
                if ((pTSprite->flags & kPhysMove) && getflorzofslope(pTSprite->sectnum, pTSprite->x, pTSprite->y) >= cZ)
                    viewAddEffect(nTSprite, kViewEffectShadow);
            }
        }
        break;
        }
    }

    for (int nTSprite = spritesortcnt-1; nTSprite >= nViewSprites; nTSprite--)
    {
        tspritetype *pTSprite = &tsprite[nTSprite];
        int nAnim = 0;
        switch (picanm[pTSprite->picnum].extra&7)
        {
            case 1:
            {
                int dX = cX - pTSprite->x;
                int dY = cY - pTSprite->y;
                RotateVector(&dX, &dY, 128-pTSprite->ang);
                nAnim = GetOctant(dX, dY);
                if (nAnim <= 4)
                {
                    pTSprite->cstat &= ~4;
                }
                else
                {
                    nAnim = 8 - nAnim;
                    pTSprite->cstat |= 4;
                }
                break;
            }
            case 2:
            {
                int dX = cX - pTSprite->x;
                int dY = cY - pTSprite->y;
                RotateVector(&dX, &dY, 128-pTSprite->ang);
                nAnim = GetOctant(dX, dY);
                break;
            }
        }
        while (nAnim > 0)
        {
            pTSprite->picnum += picanm[pTSprite->picnum].num+1;
            nAnim--;
        }
    }

#ifdef NOONE_EXTENSIONS
    if (gModernMap) lasersProcessView3D(cX, cY, cZ, cA, -1, 0);
#endif

}

int othercameradist = 1280;
int cameradist = -1;
int othercameraclock, cameraclock;

void CalcOtherPosition(spritetype *pSprite, int *pX, int *pY, int *pZ, int *vsectnum, int nAng, fix16_t zm)
{
    int vX = mulscale30(-Cos(nAng), 1280);
    int vY = mulscale30(-Sin(nAng), 1280);
    int vZ = fix16_to_int(mulscale3(zm, 1280))-(16<<8);
    int bakCstat = pSprite->cstat;
    pSprite->cstat &= ~256;
    dassert(*vsectnum >= 0 && *vsectnum < kMaxSectors);
    FindSector(*pX, *pY, *pZ, vsectnum);
    short nHSector;
    int hX, hY;
    vec3_t pos = {*pX, *pY, *pZ};
    hitdata_t hitdata;
    hitscan(&pos, *vsectnum, vX, vY, vZ, &hitdata, CLIPMASK1);
    nHSector = hitdata.sect;
    hX = hitdata.xyz.x;
    hY = hitdata.xyz.y;
    int dX = hX-*pX;
    int dY = hY-*pY;
    if (klabs(vX)+klabs(vY) > klabs(dX)+klabs(dY))
    {
        *vsectnum = nHSector;
        dX -= ksgn(vX)<<6;
        dY -= ksgn(vY)<<6;
        int nDist;
        if (klabs(vX) > klabs(vY))
        {
            nDist = ClipHigh(divscale16(dX,vX), othercameradist);
        }
        else
        {
            nDist = ClipHigh(divscale16(dY,vY), othercameradist);
        }
        othercameradist = nDist;
    }
    *pX += mulscale16(vX, othercameradist);
    *pY += mulscale16(vY, othercameradist);
    *pZ += mulscale16(vZ, othercameradist);
    othercameradist = ClipHigh(othercameradist+(((int)(totalclock-othercameraclock))<<10), 65536);
    othercameraclock = (int)totalclock;
    if (*vsectnum >= 0 && *vsectnum < kMaxSectors)
        FindSector(*pX, *pY, *pZ, vsectnum);
    else // sector was not found, likely viewpoint is within wall - use sprite sect and continue
        *vsectnum = pSprite->sectnum;
    dassert(*vsectnum >= 0 && *vsectnum < kMaxSectors);
    pSprite->cstat = bakCstat;
}

void CalcPosition(spritetype *pSprite, int *pX, int *pY, int *pZ, int *vsectnum, int nAng, fix16_t zm)
{
    int vX = mulscale30(-Cos(nAng), 1280);
    int vY = mulscale30(-Sin(nAng), 1280);
    int vZ = fix16_to_int(mulscale3(zm, 1280))-(16<<8);
    int bakCstat = pSprite->cstat;
    pSprite->cstat &= ~256;
    dassert(*vsectnum >= 0 && *vsectnum < kMaxSectors);
    FindSector(*pX, *pY, *pZ, vsectnum);
    short nHSector;
    int hX, hY;
    hitscangoal.x = hitscangoal.y = 0x1fffffff;
    vec3_t pos = { *pX, *pY, *pZ };
    hitdata_t hitdata;
    hitscan(&pos, *vsectnum, vX, vY, vZ, &hitdata, CLIPMASK1);
    nHSector = hitdata.sect;
    hX = hitdata.xyz.x;
    hY = hitdata.xyz.y;
    int dX = hX-*pX;
    int dY = hY-*pY;
    if (klabs(vX)+klabs(vY) > klabs(dX)+klabs(dY))
    {
        *vsectnum = nHSector;
        dX -= ksgn(vX)<<6;
        dY -= ksgn(vY)<<6;
        int nDist;
        if (klabs(vX) > klabs(vY))
        {
            nDist = ClipHigh(divscale16(dX,vX), cameradist);
        }
        else
        {
            nDist = ClipHigh(divscale16(dY,vY), cameradist);
        }
        cameradist = nDist;
    }
    *pX += mulscale16(vX, cameradist);
    *pY += mulscale16(vY, cameradist);
    *pZ += mulscale16(vZ, cameradist);
    cameradist = ClipHigh(cameradist+(((int)(totalclock-cameraclock))<<10), 65536);
    cameraclock = (int)totalclock;
    dassert(*vsectnum >= 0 && *vsectnum < kMaxSectors);
    FindSector(*pX, *pY, *pZ, vsectnum);
    pSprite->cstat = bakCstat;
}

// by NoOne: show warning msgs in game instead of throwing errors (in some cases)
void viewSetSystemMessage(const char* pMessage, ...) {
    char buffer[1024]; va_list args; va_start(args, pMessage);
    vsprintf(buffer, pMessage, args);
    
    OSD_Printf("%s\n", buffer); // print it also in console
    gGameMessageMgr.Add(buffer, 15, 7, MESSAGE_PRIORITY_NORMAL);
}

void viewSetMessage(const char *pMessage, const int pal, const MESSAGE_PRIORITY priority)
{
    OSD_Printf("%s\n", pMessage);
    gGameMessageMgr.Add(pMessage, 15, pal, priority);
}

void viewDisplayMessage(void)
{
    gGameMessageMgr.Display();
}

char errMsg[256];

void viewSetErrorMessage(const char *pMessage)
{
    if (!pMessage)
    {
        strcpy(errMsg, "");
    }
    else
    {
        strcpy(errMsg, pMessage);
    }
}

void DoLensEffect(void)
{
    char *d = (char*)waloff[LENSBUFFER];
    dassert(d != NULL);
    char *s = (char*)waloff[CRYSTALBALLBUFFER];
    dassert(s != NULL);
    for (int i = 0; i < kLensSize*kLensSize; i++, d++)
    {
        if (lensTable[i] >= 0)
            *d = s[lensTable[i]];
    }
    tileInvalidate(LENSBUFFER, -1, -1);
}

void UpdateDacs(int nPalette, bool bNoTint)
{
    static RGB newDAC[256];
    static int oldPalette;
    if (oldPalette != nPalette)
    {
        scrSetPalette(nPalette);
        oldPalette = nPalette;
    }

#ifdef USE_OPENGL
    if (videoGetRenderMode() >= REND_POLYMOST)
    {
        gLastPal = 0;
        polytint_t *tint = &hictinting[MAXPALOOKUPS-1];
        int nRed = 0;
        int nGreen = 0;
        int nBlue = 0;
        tint->f = 0;
        switch (nPalette)
        {
        case 0:
        default:
            tint->r = 255;
            tint->g = 255;
            tint->b = 255;
            break;
        case 1:
            tint->r = 132;
            tint->g = 164;
            tint->b = 255;
            break;
        case 2:
            tint->r = 255;
            tint->g = 126;
            tint->b = 105;
            break;
        case 3:
            tint->r = 162;
            tint->g = 186;
            tint->b = 15;
            break;
        case 4:
            tint->r = 255;
            tint->g = 255;
            tint->b = 255;
            break;
        }
        if (!bNoTint && gView != nullptr)
        {
            nRed += gView->pickupEffect;
            nGreen += gView->pickupEffect;
            nBlue -= gView->pickupEffect;

            nRed += ClipHigh(gView->painEffect, 85)*2;
            nGreen -= ClipHigh(gView->painEffect, 85)*3;
            nBlue -= ClipHigh(gView->painEffect, 85)*3;

            nRed -= gView->blindEffect;
            nGreen -= gView->blindEffect;
            nBlue -= gView->blindEffect;

            nRed -= gView->chokeEffect>>6;
            nGreen -= gView->chokeEffect>>5;
            nBlue -= gView->chokeEffect>>6;
        }
        nRed = ClipRange(nRed, -255, 255);
        nGreen = ClipRange(nGreen, -255, 255);
        nBlue = ClipRange(nBlue, -255, 255);

        videoSetPalette(0, nPalette, 2);
        videoTintBlood(nRed, nGreen, nBlue);
    }
    else
#endif
    {
        gLastPal = nPalette;
        if (bNoTint || gView == nullptr)
        {
            memcpy(newDAC, baseDAC, sizeof(newDAC));
        }
        else
        {
            for (int i = 0; i < 256; i++)
            {
                int nRed = baseDAC[i].red;
                int nGreen = baseDAC[i].green;
                int nBlue = baseDAC[i].blue;
                nRed += gView->pickupEffect;
                nGreen += gView->pickupEffect;
                nBlue -= gView->pickupEffect;

                nRed += ClipHigh(gView->painEffect, 85)*2;
                nGreen -= ClipHigh(gView->painEffect, 85)*3;
                nBlue -= ClipHigh(gView->painEffect, 85)*3;

                nRed -= gView->blindEffect;
                nGreen -= gView->blindEffect;
                nBlue -= gView->blindEffect;

                nRed -= gView->chokeEffect>>6;
                nGreen -= gView->chokeEffect>>5;
                nBlue -= gView->chokeEffect>>6;

                newDAC[i].red = ClipRange(nRed, 0, 255);
                newDAC[i].green = ClipRange(nGreen, 0, 255);
                newDAC[i].blue = ClipRange(nBlue, 0, 255);
            }
        }
        if (memcmp(newDAC, curDAC, 768) != 0)
        {
            memcpy(curDAC, newDAC, 768);
            gSetDacRange(0, 256, curDAC);
        }
    }
}

char otherMirrorGotpic[2];
char bakMirrorGotpic[2];
// int gVisibility;

int deliriumTilt, deliriumTurn, deliriumPitch;
int gScreenTiltO, deliriumTurnO, deliriumPitchO;

int gShowFrameRate = 1;

void viewUpdateDelirium(void)
{
    gScreenTiltO = gScreenTilt;
    deliriumTurnO = deliriumTurn;
    deliriumPitchO = deliriumPitch;
    int powerCount;
    if ((powerCount = powerupCheck(gView, kPwUpDeliriumShroom)) != 0)
    {
        int tilt1 = 170, tilt2 = 170, pitch = 20;
        int timer = (int)gFrameClock << 1;
        if (powerCount < 512)
        {
            int powerScale = (powerCount<<16) / 512;
            tilt1 = mulscale16(tilt1, powerScale);
            tilt2 = mulscale16(tilt2, powerScale);
            pitch = mulscale16(pitch, powerScale);
        }
        int sin2 = costable[(2*timer-512)&2047] / 2;
        int sin3 = costable[(3*timer-512)&2047] / 2;
        gScreenTilt = mulscale30(sin2+sin3,tilt1);
        int sin4 = costable[(4*timer-512)&2047] / 2;
        deliriumTurn = mulscale30(sin3+sin4,tilt2);
        int sin5 = costable[(5*timer-512)&2047] / 2;
        deliriumPitch = mulscale30(sin4+sin5,pitch);
        return;
    }
    gScreenTilt = ((gScreenTilt+1024)&2047)-1024;
    if (gScreenTilt > 0)
    {
        gScreenTilt -= 8;
        if (gScreenTilt < 0)
            gScreenTilt = 0;
    }
    else if (gScreenTilt < 0)
    {
        gScreenTilt += 8;
        if (gScreenTilt >= 0)
            gScreenTilt = 0;
    }
}

int shakeHoriz, shakeAngle, shakeX, shakeY, shakeZ, shakeBobX, shakeBobY;

void viewUpdateShake(void)
{
    shakeHoriz = 0;
    shakeAngle = 0;
    shakeX = 0;
    shakeY = 0;
    shakeZ = 0;
    shakeBobX = 0;
    shakeBobY = 0;
    if (gView->flickerEffect)
    {
        int nValue = ClipHigh(gView->flickerEffect * 8, 2000);
        shakeHoriz += QRandom2(nValue >> 8);
        shakeAngle += QRandom2(nValue >> 8);
        shakeX += QRandom2(nValue >> 4);
        shakeY += QRandom2(nValue >> 4);
        shakeZ += QRandom2(nValue);
        shakeBobX += QRandom2(nValue);
        shakeBobY += QRandom2(nValue);
    }
    if (gView->quakeEffect)
    {
        int nValue = ClipHigh(gView->quakeEffect * 8, 2000);
        shakeHoriz += QRandom2(nValue >> 8);
        shakeAngle += QRandom2(nValue >> 8);
        shakeX += QRandom2(nValue >> 4);
        shakeY += QRandom2(nValue >> 4);
        shakeZ += QRandom2(nValue);
        shakeBobX += QRandom2(nValue);
        shakeBobY += QRandom2(nValue);
    }
}

float r_ambientlight = 1.0, r_ambientlightrecip = 1.0;

int gLastPal = 0;

int32_t g_frameRate;

void viewDrawScreen(void)
{
    int nPalette = 0;
    static ClockTicks lastUpdate;
    int defaultHoriz = gCenterHoriz ? 100 : 90;

#ifdef USE_OPENGL
    polymostcenterhoriz = defaultHoriz;
#endif
    ClockTicks delta = totalclock - lastUpdate;
    if (delta < 0)
        delta = 0;
    lastUpdate = totalclock;
    if (!gPaused && (!CGameMenuMgr::m_bActive || gGameOptions.nGameType != kGameTypeSinglePlayer))
    {
        gInterpolate = ((totalclock-gNetFifoClock)+kTicsPerFrame).toScale16()/kTicsPerFrame;
    }
    if (gInterpolate < 0 || gInterpolate > 65536)
    {
        gInterpolate = ClipRange(gInterpolate, 0, 65536);
    }
    if (gViewInterpolate)
    {
        CalcInterpolations();
    }

    if (!gPaused && (!CGameMenuMgr::m_bActive || gGameOptions.nGameType != kGameTypeSinglePlayer))
        rotatespritesmoothratio = gInterpolate;
    else
        rotatespritesmoothratio = 65536;

    if (gViewMode == 3 || gViewMode == 4 || gOverlayMap)
    {
        DoSectorLighting();
    }
    if (gViewMode == 3 || gOverlayMap)
    {
        int yxAspect = yxaspect;
        int viewingRange = viewingrange;
        if (r_usenewaspect)
        {
            newaspect_enable = 1;
            videoSetCorrectedAspect();
        }
        const int viewingRange_fov = Blrintf(float(viewingrange) * tanf(gFov * (PI/360.f)));
        renderSetAspect(viewingRange_fov, yxaspect);
        int cX = gView->pSprite->x;
        int cY = gView->pSprite->y;
        gViewSpritePredictLoc.z = gView->pSprite->z;
        int cZ = gView->zView;
        int zDelta = gView->zWeapon-gView->zView-(12<<8);
        fix16_t cA = gView->q16ang;
        fix16_t q16horiz = gView->q16horiz;
        fix16_t q16slopehoriz = gView->q16slopehoriz;
        int v74 = gView->bobWidth;
        int v8c = gView->bobHeight;
        int v4c = gView->swayWidth;
        int v48 = gView->swayHeight;
        int nSectnum = gView->pSprite->sectnum;
        if (gViewInterpolate)
        {
            if (numplayers > 1 && gView == gMe && gPrediction && gMe->pXSprite->health > 0)
            {
                nSectnum = predict.at68;
                cX = interpolate(predictOld.at50, predict.at50, gInterpolate);
                cY = interpolate(predictOld.at54, predict.at54, gInterpolate);
                cZ = interpolate(predictOld.at38, predict.at38, gInterpolate);
                zDelta = interpolate(predictOld.at34, predict.at34, gInterpolate);
                cA = interpolateangfix16(predictOld.at30, predict.at30, gInterpolate);
                q16horiz = interpolate(predictOld.at24, predict.at24, gInterpolate);
                q16slopehoriz = interpolate(predictOld.at28, predict.at28, gInterpolate);
                v74 = interpolate(predictOld.atc, predict.atc, gInterpolate);
                v8c = interpolate(predictOld.at8, predict.at8, gInterpolate);
                v4c = interpolate(predictOld.at1c, predict.at1c, gInterpolate);
                v48 = interpolate(predictOld.at18, predict.at18, gInterpolate);
                gViewSpritePredictLoc.z = interpolate(predictOld.at58, predict.at58, gInterpolate);
            }
            else
            {
                VIEW *pView = &gPrevView[gViewIndex];
                cX = interpolate(pView->at50, cX, gInterpolate);
                cY = interpolate(pView->at54, cY, gInterpolate);
                cZ = interpolate(pView->at38, cZ, gInterpolate);
                zDelta = interpolate(pView->at34, zDelta, gInterpolate);
                cA = interpolateangfix16(pView->at30, cA, gInterpolate);
                q16horiz = interpolate(pView->at24, q16horiz, gInterpolate);
                q16slopehoriz = interpolate(pView->at28, q16slopehoriz, gInterpolate);
                v74 = interpolate(pView->atc, v74, gInterpolate);
                v8c = interpolate(pView->at8, v8c, gInterpolate);
                v4c = interpolate(pView->at1c, v4c, gInterpolate);
                v48 = interpolate(pView->at18, v48, gInterpolate);
                gViewSpritePredictLoc.z = interpolate(pView->at58, gViewSpritePredictLoc.z, gInterpolate);
            }
        }
        if (gView == gMe && (numplayers <= 1 || gPrediction) && gView->pXSprite->health != 0 && !gDemo.at1 && !VanillaMode())
        {
            fix16_t q16look;
            cA = gViewAngle;
            q16look = gViewLook;
            q16horiz = fix16_from_float(100.f * tanf(fix16_to_float(q16look) * fPI / 1024.f));
        }
        gViewSpritePredictLoc.x = cX, gViewSpritePredictLoc.y = cY, gViewSpritePredictLoc.ang = cA;
        viewUpdateShake();
        q16horiz += fix16_from_int(shakeHoriz);
        cA += fix16_from_int(shakeAngle);
        cX += shakeX;
        cY += shakeY;
        cZ += shakeZ;
        v4c += shakeBobX;
        v48 += shakeBobY;
        q16horiz += fix16_from_int(mulscale30(0x40000000-Cos(gView->tiltEffect<<2), 30));
        if (gViewPos == VIEWPOS_0)
        {
            if (gViewHBobbing)
            {
                cX -= mulscale30(v74, Sin(fix16_to_int(cA)))>>4;
                cY += mulscale30(v74, Cos(fix16_to_int(cA)))>>4;
            }
            if (gViewVBobbing)
            {
                cZ += v8c;
            }
            if (gSlopeTilting)
            {
                q16horiz += q16slopehoriz;
            }
            cZ += fix16_to_int(q16horiz*10);
            cameradist = -1;
            cameraclock = (int)totalclock;
        }
        else
        {
            CalcPosition(gView->pSprite, (int*)&cX, (int*)&cY, (int*)&cZ, &nSectnum, fix16_to_int(cA), q16horiz);
        }
        const char bLink = CheckLink((int*)&cX, (int*)&cY, (int*)&cZ, &nSectnum);
        int nTilt = gViewInterpolate ? interpolateang(gScreenTiltO, gScreenTilt, gInterpolate) : gScreenTilt;
        char nPalCrystalBall = 0;
        bool bDelirium = powerupCheck(gView, kPwUpDeliriumShroom) > 0;
        static bool bDeliriumOld = false;
        int tiltcs = 0, tiltdim = 320;
        const char bCrystalBall = (powerupCheck(gView, kPwUpCrystalBall) > 0) && (gNetPlayers > 1);
#ifdef USE_OPENGL
        renderSetRollAngle(0);
#endif
        if (nTilt || bDelirium)
        {
            if (videoGetRenderMode() == REND_CLASSIC)
            {
                int vr = viewingrange;
                walock[TILTBUFFER] = CACHE1D_PERMANENT;
                if (!waloff[TILTBUFFER])
                {
                    tileAllocTile(TILTBUFFER, 640, 640, 0, 0);
                }
                if (xdim >= 640 && ydim >= 640)
                {
                    tiltcs = 1;
                    tiltdim = 640;
                }
                renderSetTarget(TILTBUFFER, tiltdim, tiltdim);
                int nAng = nTilt&(kAng90-1);
                if (nAng > kAng45)
                {
                    nAng = kAng90-nAng;
                }
                renderSetAspect(mulscale16(vr, dmulscale32(Cos(nAng), 262144, Sin(nAng), 163840)), yxaspect);
            }
#ifdef USE_OPENGL
            else
                renderSetRollAngle(nTilt);
#endif
        }
        else if (bCrystalBall)
        {
            int tmp = ((int)totalclock/240)%(gNetPlayers-1);
            int i = connecthead;
            while (1)
            {
                if (i == gViewIndex)
                    i = connectpoint2[i];
                if (tmp == 0)
                    break;
                i = connectpoint2[i];
                tmp--;
            }
            PLAYER *pOther = &gPlayer[i];
            if (!waloff[CRYSTALBALLBUFFER])
            {
                tileAllocTile(CRYSTALBALLBUFFER, 128, 128, 0, 0);
            }
            renderSetTarget(CRYSTALBALLBUFFER, 128, 128);
            renderSetAspect(65536, 78643);
            int vd8 = pOther->pSprite->x;
            int vd4 = pOther->pSprite->y;
            int vd0 = pOther->zView;
            int vcc = pOther->pSprite->sectnum;
            int v50 = pOther->pSprite->ang;
            int v54 = 0;
            if (pOther->flickerEffect)
            {
                int nValue = ClipHigh(pOther->flickerEffect*8, 2000);
                v54 += QRandom2(nValue>>8);
                v50 += QRandom2(nValue>>8);
                vd8 += QRandom2(nValue>>4);
                vd4 += QRandom2(nValue>>4);
                vd0 += QRandom2(nValue);
            }
            if (pOther->quakeEffect)
            {
                int nValue = ClipHigh(pOther->quakeEffect*8, 2000);
                v54 += QRandom2(nValue >> 8);
                v50 += QRandom2(nValue >> 8);
                vd8 += QRandom2(nValue >> 4);
                vd4 += QRandom2(nValue >> 4);
                vd0 += QRandom2(nValue);
            }
            CalcOtherPosition(pOther->pSprite, &vd8, &vd4, &vd0, &vcc, v50, 0);
            CheckLink(&vd8, &vd4, &vd0, &vcc);
            if (IsUnderwaterSector(vcc))
                nPalCrystalBall = 10;
            memcpy(bakMirrorGotpic, gotpic+510, 2);
            memcpy(gotpic+510, otherMirrorGotpic, 2);
            g_visibility = (int32_t)(ClipLow(gVisibility-32*pOther->visibility, 0) * (numplayers > 1 ? 1.f : r_ambientlightrecip));
            int vc4, vc8;
            getzsofslope(vcc, vd8, vd4, &vc8, &vc4);
            if (VanillaMode() ? (vd0 >= vc4) : (vd0 > vc4-(8<<4)) && (gUpperLink[vcc] == -1)) // clamp to floor
            {
                vd0 = vc4-(8<<4);
            }
            if (VanillaMode() ? (vd0 <= vc8) : (vd0 < vc8+(8<<4)) && (gLowerLink[vcc] == -1)) // clamp to ceiling
            {
                vd0 = vc8+(8<<4);
            }
            v54 = ClipRange(v54, -200, 200);
            int nRORLimit = 32; // limit ROR rendering to 32 times
RORHACKOTHER:
            int ror_status[16];
            for (int i = 0; i < 16; i++)
                ror_status[i] = TestBitString(gotpic, 4080 + i);
            yax_preparedrawrooms();
            DrawMirrors(vd8, vd4, vd0, fix16_from_int(v50), fix16_from_int(v54 + defaultHoriz), gInterpolate, -1);
            drawrooms(vd8, vd4, vd0, v50, v54 + defaultHoriz, vcc);
            yax_drawrooms(viewProcessSprites, vcc, 0, gInterpolate);
            for (int i = 0; nRORLimit && (i < 16); i++) // check if ror needs to be rendered
            {
                if (ror_status[i] != TestBitString(gotpic, 4080 + i))
                {
                    spritesortcnt = 0;
                    nRORLimit--;
                    goto RORHACKOTHER;
                }
            }
            memcpy(otherMirrorGotpic, gotpic+510, 2);
            memcpy(gotpic+510, bakMirrorGotpic, 2);
            viewProcessSprites(vd8, vd4, vd0, v50, gInterpolate);
            renderDrawMasks();
            renderRestoreTarget();
            renderSetAspect(viewingRange_fov, yxaspect);
        }
        else
        {
            othercameraclock = (int)totalclock;
        }

        if (!bDelirium)
        {
            deliriumTilt = 0;
            deliriumTurn = 0;
            deliriumPitch = 0;
        }
        int nSprite = headspritestat[kStatExplosion];
        int unk = 0;
        while (nSprite >= 0)
        {
            spritetype *pSprite = &sprite[nSprite];
            int nXSprite = pSprite->extra;
            dassert(nXSprite > 0 && nXSprite < kMaxXSprites);
            XSPRITE *pXSprite = &xsprite[nXSprite];
            if (TestBitString(gotsector, pSprite->sectnum))
            {
                unk += pXSprite->data3*32;
            }
            nSprite = nextspritestat[nSprite];
        }
        nSprite = headspritestat[kStatProjectile];
        while (nSprite >= 0) {
            spritetype *pSprite = &sprite[nSprite];
            switch (pSprite->type) {
                case kMissileFlareRegular:
                case kMissileTeslaAlt:
                case kMissileFlareAlt:
                case kMissileTeslaRegular:
                    if (TestBitString(gotsector, pSprite->sectnum)) unk += 256;
                    break;
            }
            nSprite = nextspritestat[nSprite];
        }
        g_visibility = (int32_t)(ClipLow(gVisibility - 32 * gView->visibility - unk, 0) * (numplayers > 1 ? 1.f : r_ambientlightrecip));
        if (!gViewInterpolate) 
        {
            cA += fix16_from_int(deliriumTurn);
        }
        else
        {
            cA = (cA + interpolateangfix16(fix16_from_int(deliriumTurnO), fix16_from_int(deliriumTurn), gInterpolate)) & 0x7ffffff;
        }
        int vfc, vf8;
        getzsofslope(nSectnum, cX, cY, &vfc, &vf8);
        if (VanillaMode() ? (cZ >= vf8) : (cZ > vf8-(8<<4)) && (gUpperLink[nSectnum] == -1)) // clamp to floor
        {
            cZ = vf8-(8<<4);
        }
        if (VanillaMode() ? (cZ <= vfc) : (cZ < vfc+(8<<4)) && (gLowerLink[nSectnum] == -1)) // clamp to ceiling
        {
            cZ = vfc+(8<<4);
        }
        q16horiz = ClipRange(q16horiz, F16(-200), F16(200));
        int nRORLimit = 32; // limit ROR rendering to 32 times
        ClearGotSectorSectorFX();
RORHACK:
        int ror_status[16];
        for (int i = 0; i < 16; i++)
            ror_status[i] = TestBitString(gotpic, 4080+i);
        fix16_t deliriumPitchI = gViewInterpolate ? interpolate(fix16_from_int(deliriumPitchO), fix16_from_int(deliriumPitch), gInterpolate) : fix16_from_int(deliriumPitch);
        DrawMirrors(cX, cY, cZ, cA, q16horiz + fix16_from_int(defaultHoriz) + deliriumPitchI, gInterpolate, bLink && !VanillaMode() ? gViewIndex : -1); // only hide self sprite while traversing between sector
        int bakCstat = gView->pSprite->cstat;
        if (gViewPos == VIEWPOS_0) // don't render self while in first person view
        {
            gView->pSprite->cstat |= CSTAT_SPRITE_INVISIBLE;
        }
        else // chase camera - render as transparent
        {
            gView->pSprite->cstat |= CSTAT_SPRITE_TRANSLUCENT_INVERT | CSTAT_SPRITE_TRANSLUCENT;
        }
#ifdef POLYMER
        if (videoGetRenderMode() == REND_POLYMER)
            polymer_setanimatesprites(viewProcessSprites, cX, cY, cZ, fix16_to_int(cA), gInterpolate);
#endif
        yax_preparedrawrooms();
        renderDrawRoomsQ16(cX, cY, cZ, cA, q16horiz + fix16_from_int(defaultHoriz) + deliriumPitchI, nSectnum);
        UpdateGotSectorSectorFX();
        yax_drawrooms(viewProcessSprites, nSectnum, 0, gInterpolate);
        viewProcessSprites(cX, cY, cZ, fix16_to_int(cA), gInterpolate);
        for (int i = 0; nRORLimit && (i < 16); i++) // check if ror needs to be rendered
        {
            if (ror_status[i] != TestBitString(gotpic, 4080+i))
            {
                gView->pSprite->cstat = bakCstat;
                spritesortcnt = 0;
                nRORLimit--;
                goto RORHACK;
            }
        }
        sub_5571C(1);
        int nSpriteSortCnt = spritesortcnt;
        renderDrawMasks();
        spritesortcnt = nSpriteSortCnt;
        sub_5571C(0);
        sub_557C4(cX, cY, gInterpolate);
        renderDrawMasks();
        gView->pSprite->cstat = bakCstat;

        if (nTilt || bDelirium)
        {
            if (videoGetRenderMode() == REND_CLASSIC)
            {
                dassert(waloff[ TILTBUFFER ] != 0);
                renderRestoreTarget();
                int vrc = 64+4+2+1024;
                if (bDelirium)
                {
                    vrc = 64+32+4+2+1+1024;
                }
                int nAng = nTilt & (kAng90-1);
                if (nAng > kAng45)
                {
                    nAng = kAng90 - nAng;
                }
                int nScale = dmulscale32(Cos(nAng), 262144, Sin(nAng), 163840)>>tiltcs;
                rotatesprite(160<<16, 100<<16, nScale, nTilt+512, TILTBUFFER, 0, 0, vrc, gViewX0, gViewY0, gViewX1, gViewY1);
            }
#ifdef USE_OPENGL
            else
            {
                if (videoGetRenderMode() == REND_POLYMOST && gDeliriumBlur)
                {
                    if (!bDeliriumOld)
                    {
                        glAccum(GL_LOAD, 1.f);
                    }
                    else
                    {
                        const float fBlur = pow(1.f/3.f, 30.f/g_frameRate);
                        glAccum(GL_MULT, fBlur);
                        glAccum(GL_ACCUM, 1.f-fBlur);
                        glAccum(GL_RETURN, 1.f);
                    }
                }
            }
#endif
        }

        bDeliriumOld = bDelirium && gDeliriumBlur;

        if (r_usenewaspect)
            newaspect_enable = 0;
        renderSetAspect(viewingRange, yxAspect);
#if 0
        int nClipDist = gView->pSprite->clipdist<<2;
        int ve8, vec, vf0, vf4;
        GetZRange(gView->pSprite, &vf4, &vf0, &vec, &ve8, nClipDist, 0);
        int tmpSect = nSectnum;
        if ((vf0 & 0xc000) == 0x4000)
        {
            tmpSect = vf0 & (kMaxWalls-1);
        }
        int v8 = byte_1CE5C2 > 0 && (sector[tmpSect].ceilingstat&1);
        if (gWeather.at12d8 > 0 || v8)
        {
            gWeather.Draw(cX, cY, cZ, cA, q16horiz + defaultHoriz + deliriumPitch, gWeather.at12d8);
            if (v8)
            {
                gWeather.at12d8 = ClipRange(delta*8+gWeather.at12d8, 0, 4095);
            }
            else
            {
                gWeather.at12d8 = ClipRange(gWeather.at12d8-delta*64, 0, 4095);
            }
        }
#endif
        if (gViewPos == VIEWPOS_0)
        {
            if (gAimReticle)
            {
                rotatesprite(160<<16, defaultHoriz<<16, 65536, 0, kCrosshairTile, 0, g_isAlterDefaultCrosshair ? CROSSHAIR_PAL : 0, 2, gViewX0, gViewY0, gViewX1, gViewY1);
            }
            if (!VanillaMode()) // smooth motion
            {
                cX = (v4c<<8)+(160<<16);
                cY = (v48<<8)+(220<<16)+(zDelta<<9);
            }
            else // quantize like vanilla v1.21
            {
                cX = ((v4c>>8)+160)<<16;
                cY = ((v48>>8)+220+(zDelta>>7))<<16;
            }
            int nShade = sector[nSectnum].floorshade; int nPalette = 0;
            if (sector[gView->pSprite->sectnum].extra > 0) {
                sectortype *pSector = &sector[gView->pSprite->sectnum];
                XSECTOR *pXSector = &xsector[pSector->extra];
                if (pXSector->color)
                    nPalette = pSector->floorpal;
            }
            
            #ifdef NOONE_EXTENSIONS
                if (gView->sceneQav < 0) WeaponDraw(gView, nShade, cX, cY, nPalette);
                else if (gView->pXSprite->health > 0) playerQavSceneDraw(gView, nShade, cX, cY, nPalette);
                else {
                    gView->sceneQav = gView->weaponQav = -1;
                    gView->weaponTimer = 0;
                    gView->curWeapon = kWeaponNone;
                }
            #else
                WeaponDraw(gView, nShade, cX, cY, nPalette);
            #endif
           

        }
        if (gViewPos == VIEWPOS_0 && gView->pXSprite->burnTime > 60)
        {
            viewBurnTime(gView->pXSprite->burnTime);
        }
        if (packItemActive(gView, kPackDivingSuit))
        {
            rotatesprite(0, 0, 65536, 0, 2344, 0, 0, 256+18, gViewX0, gViewY0, gViewX1, gViewY1);
            rotatesprite(320<<16, 0, 65536, 1024, 2344, 0, 0, 512+22, gViewX0, gViewY0, gViewX1, gViewY1);
            rotatesprite(0, 200<<16, 65536, 0, 2344, 0, 0, 256+22, gViewX0, gViewY0, gViewX1, gViewY1);
            rotatesprite(320<<16, 200<<16, 65536, 1024, 2344, 0, 0, 512+18, gViewX0, gViewY0, gViewX1, gViewY1);
            if (gDetail >= 4)
            {
                rotatesprite(15<<16, 3<<16, 65536, 0, 2346, 32, 0, 256+19, gViewX0, gViewY0, gViewX1, gViewY1);
                rotatesprite(212<<16, 77<<16, 65536, 0, 2347, 32, 0, 512+19, gViewX0, gViewY0, gViewX1, gViewY1);
            }
        }
        if (powerupCheck(gView, kPwUpAsbestArmor) > 0)
        {
            rotatesprite(0, 200<<16, 65536, 0, 2358, 0, 0, 256+22, gViewX0, gViewY0, gViewX1, gViewY1);
            rotatesprite(320<<16, 200<<16, 65536, 1024, 2358, 0, 0, 512+18, gViewX0, gViewY0, gViewX1, gViewY1);
        }
        if (bCrystalBall && waloff[CRYSTALBALLBUFFER])
        {
            DoLensEffect();
            viewingRange = viewingrange;
            yxAspect = yxaspect;
            renderSetAspect(65536, 54613);
            rotatesprite(280<<16, 35<<16, 53248, kAng90, LENSBUFFER, 0, nPalCrystalBall, 512+6, gViewX0, gViewY0, gViewX1, gViewY1);
            rotatesprite(280<<16, 35<<16, 53248, 0, 1683, 0, 0, 512+35, gViewX0, gViewY0, gViewX1, gViewY1);
            renderSetAspect(viewingRange, yxAspect);
        }
        
        if (powerupCheck(gView, kPwUpDeathMask) > 0) nPalette = 4;
        else if(powerupCheck(gView, kPwUpReflectShots) > 0) nPalette = 1;
        else if (gView->isUnderwater) {
            if (gView->nWaterPal) nPalette = gView->nWaterPal;
            else {
                if (gView->pXSprite->medium == kMediumWater) nPalette = 1;
                else if (gView->pXSprite->medium == kMediumGoo) nPalette = 3;
                else nPalette = 2;
            }
        }
    }
    if (gViewMode == 4)
    {
        int cX = 0, cY = 0, nAng = 0;
        if (gViewMap.bFollowMode) // calculate get current player position for 2d map for follow mode
        {
            cX = gView->pSprite->x, cY = gView->pSprite->y, nAng = gView->pSprite->ang;
            if (!VanillaMode()) // interpolate angle for 2d map view
            {
                fix16_t cA = gView->q16ang;
                if (gViewInterpolate)
                {
                    if (numplayers > 1 && gView == gMe && gPrediction && gMe->pXSprite->health > 0)
                    {
                        cX = interpolate(predictOld.at50, predict.at50, gInterpolate);
                        cY = interpolate(predictOld.at54, predict.at54, gInterpolate);
                        cA = interpolateangfix16(predictOld.at30, predict.at30, gInterpolate);
                    }
                    else
                    {
                        VIEW *pView = &gPrevView[gViewIndex];
                        cX = interpolate(pView->at50, cX, gInterpolate);
                        cY = interpolate(pView->at54, cY, gInterpolate);
                        cA = interpolateangfix16(pView->at30, cA, gInterpolate);
                    }
                }
                if (gView == gMe && (numplayers <= 1 || gPrediction) && gView->pXSprite->health != 0)
                    cA = gViewAngle;
                nAng = fix16_to_int(cA);
            }
        }
        ClearGotSectorSectorFX();
        gViewMap.Process(cX, cY, nAng);
        UpdateGotSectorSectorFX();
    }
    viewDrawInterface(delta);
    if (IsPlayerSprite(gView->pSprite) && (gView->hand == 1))
    {
        int zn = ((gView->zWeapon-gView->zView-(12<<8))>>7)+220;
        gChoke.Draw(160, zn);
    }
    if (byte_1A76C6)
    {
        DrawStatSprite(2048, xdim-15, 20);
    }
    viewDisplayMessage();
    CalcFrameRate();
#if 0
    if (gShowFrameRate)
    {
        int fX, fY;
        if (gViewMode == 3)
        {
            fX = gViewX1;
        }
        else
        {
            fX = xdim;
        }
        if (gViewMode == 3)
        {
            fY = gViewY0;
        }
        else
        {
            fY = 0;
        }
        if (gViewMode == 4)
        {
            fY += mulscale16(20, yscale);
        }
        sprintf(gTempStr, "%3i", gFrameRate);
        printext256(fX-12, fY, 31, -1, gTempStr, 1);
        fY += 8;
        sprintf(gTempStr, "pos=%d,%d,%d", gView->pSprite->x, gView->pSprite->y, gView->pSprite->z);
        printext256(fX-strlen(gTempStr)*4, fY, 31, -1, gTempStr, 1);
    }
#endif
    viewDrawMapTitle();
    viewDrawAimedPlayerName();
    viewPrintFPS();
    if (gPaused)
    {
        viewDrawText(1, "PAUSED", 160, 10, 0, 0, 1, 0);
    }
    else if (gView != gMe)
    {
        sprintf(gTempStr, "] %s [", gProfile[gView->nPlayer].name);
        viewDrawText(0, gTempStr, 160, 10, 0, 0, 1, 0);
    }
    if (errMsg[0])
    {
        viewDrawText(0, errMsg, 160, 20, 0, 0, 1, 0);
    }
    if (gViewInterpolate)
    {
        RestoreInterpolations();
    }
    UpdateDacs(nPalette);
}

int nLoadingScreenTile;
char pzLoadingScreenText1[256], pzLoadingScreenText2[256], pzLoadingScreenText3[256];

void viewLoadingScreenWide(void)
{
#ifdef NOONE_EXTENSIONS
    if (gStatsPicnum != kLoadScreen)
    {
        if (gStatsColor < 255) videoClearScreen(gStatsColor);
        if (gStatsPicnum >= 0)
        {
            static ClockTicks oclock = gFrameClock; gFrameClock = totalclock;
            rotatesprite(160 << 16, 100 << 16, 65536, 0, gStatsPicnum, 0, 0, 0xa, 0, 0, xdim - 1, ydim - 1);
            gFrameClock = oclock;
        }
        return;
    }
#endif
    
    videoClearScreen(0);
#ifdef USE_OPENGL
    if ((blood_globalflags&BLOOD_FORCE_WIDELOADSCREEN) || (bLoadScreenCrcMatch && !(usehightile && h_xsize[kLoadScreen])))
#else
    if ((blood_globalflags&BLOOD_FORCE_WIDELOADSCREEN) || bLoadScreenCrcMatch)
#endif
    {
        if (yxaspect >= 65536)
        {
            rotatesprite(160<<16, 100<<16, 65536, 0, kLoadScreen, 0, 0, 1024+64+8+2, 0, 0, xdim-1, ydim-1);
        }
        else
        {
            int width = roundscale(xdim, 240, ydim);
            int nCount = (width+kLoadScreenWideBackWidth-1)/kLoadScreenWideBackWidth;
            for (int i = 0; i < nCount; i++)
            {
                rotatesprite_fs((i*kLoadScreenWideBackWidth)<<16, 0, 65536, 0, kLoadScreenWideBack, 0, 0, 256+64+16+8+2);
            }
            rotatesprite_fs((kLoadScreenWideSideWidth>>1)<<16, 200<<15, 65536, 0, kLoadScreenWideLeft, 0, 0, 256+8+2);
            rotatesprite_fs((320-(kLoadScreenWideSideWidth>>1))<<16, 200<<15, 65536, 0, kLoadScreenWideRight, 0, 0, 512+8+2);
            rotatesprite_fs(320<<15, 200<<15, 65536, 0, kLoadScreenWideMiddle, 0, 0, 8+2);
        }
    }
    else
        rotatesprite(160<<16, 100<<16, 65536, 0, kLoadScreen, 0, 0, 64+8+2, 0, 0, xdim-1, ydim-1);
}

void viewLoadingScreenUpdate(const char *pzText4, int nPercent)
{
    int vc;
    gMenuTextMgr.GetFontInfo(1, NULL, NULL, &vc);
    if (nLoadingScreenTile == kLoadScreen)
        viewLoadingScreenWide();
    else if (nLoadingScreenTile)
    {
        videoClearScreen(0);
        rotatesprite(160<<16, 100<<16, 65536, 0, nLoadingScreenTile, 0, 0, 0xa, 0, 0, xdim-1, ydim-1);
    }
    if (pzLoadingScreenText1[0])
    {
        rotatesprite(160<<16, 20<<16, 65536, 0, 2038, -128, 0, 78, 0, 0, xdim-1, ydim-1);
        viewDrawText(1, pzLoadingScreenText1, 160, 20-vc/2, -128, 0, 1, 1);
    }
    if (pzLoadingScreenText2[0])
    {
        viewDrawText(1, pzLoadingScreenText2, 160, 50, -128, 0, 1, 1);
    }
    if (pzLoadingScreenText3[0])
    {
        viewDrawText(1, pzLoadingScreenText3, 160, 70, -128, 0, 1, 1);
    }
    if (pzText4)
    {
        viewDrawText(3, pzText4, 160, 124, -128, 0, 1, 1);
    }

    if (nPercent != -1)
        TileHGauge(2260, 86, 110, nPercent, 100, 0, 131072);

    viewDrawText(3, "Please Wait", 160, 134, -128, 0, 1, 1);
}

void viewLoadingScreen(int nTile, const char *pText, const char *pText2, const char *pText3)
{
    UpdateDacs(0, true);
    nLoadingScreenTile = nTile;
    if (pText)
        strncpy(pzLoadingScreenText1, pText, 256);
    else
        pzLoadingScreenText1[0] = 0;
    if (pText2)
        strncpy(pzLoadingScreenText2, pText2, 256);
    else
        pzLoadingScreenText2[0] = 0;
    if (pText3)
        strncpy(pzLoadingScreenText3, pText3, 256);
    else
        pzLoadingScreenText3[0] = 0;
    viewLoadingScreenUpdate(NULL, -1);
}

palette_t CrosshairColors = { 255, 255, 255, 0 };
bool g_isAlterDefaultCrosshair = false;

void viewSetCrosshairColor(int32_t r, int32_t g, int32_t b)
{
    if (!g_isAlterDefaultCrosshair)
        return;

    CrosshairColors.r = r;
    CrosshairColors.g = g;
    CrosshairColors.b = b;

    tileLoad(kCrosshairTile);

    if (!waloff[kCrosshairTile])
        return;

    char *ptr = (char *)waloff[kCrosshairTile];

    int32_t ii = tilesiz[kCrosshairTile].x * tilesiz[kCrosshairTile].y;

    dassert(ii > 0);

    int32_t i = (videoGetRenderMode() == REND_CLASSIC)
        ? paletteGetClosestColor(CrosshairColors.r, CrosshairColors.g, CrosshairColors.b)
        : paletteGetClosestColor(255, 255, 255);  // use white in GL so we can tint it to the right color

    do
    {
        if (*ptr != 255)
            *ptr = i;
        ptr++;
    } while (--ii);

    paletteMakeLookupTable(CROSSHAIR_PAL, NULL, CrosshairColors.r, CrosshairColors.g, CrosshairColors.b, 1);

#ifdef USE_OPENGL
    // XXX: this makes us also load all hightile textures tinted with the crosshair color!
    polytint_t & crosshairtint = hictinting[CROSSHAIR_PAL];
    crosshairtint.r = CrosshairColors.r;
    crosshairtint.g = CrosshairColors.g;
    crosshairtint.b = CrosshairColors.b;
    crosshairtint.f = HICTINT_USEONART | HICTINT_GRAYSCALE;
#endif
    tileInvalidate(kCrosshairTile, -1, -1);
}

void viewResetCrosshairToDefault(void)
{
    paletteFreeLookupTable(CROSSHAIR_PAL);
    tileLoad(kCrosshairTile);
}

#define COLOR_RED redcol
#define COLOR_WHITE whitecol

#define LOW_FPS 60
#define SLOW_FRAME_TIME 20

#if defined GEKKO
# define FPS_YOFFSET 16
#else
# define FPS_YOFFSET 0
#endif

#define FPS_COLOR(x) ((x) ? COLOR_RED : COLOR_WHITE)

int32_t gShowFps, gFramePeriod;

void viewPrintFPS(void)
{
    char tempbuf[128];
    static int32_t frameCount;
    static double cumulativeFrameDelay;
    static double lastFrameTime;
    static float lastFPS, minFPS = std::numeric_limits<float>::max(), maxFPS;
    static double minGameUpdate = std::numeric_limits<double>::max(), maxGameUpdate;

    double frameTime = timerGetFractionalTicks();
    double frameDelay = frameTime - lastFrameTime;
    cumulativeFrameDelay += frameDelay;

    if (frameDelay >= 0)
    {
        int32_t x = (xdim <= 640);

        if (gShowFps)
        {
            int32_t chars = Bsprintf(tempbuf, "%.1f ms, %5.1f fps", frameDelay, lastFPS);

            printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+2+FPS_YOFFSET, 0, -1, tempbuf, x);
            printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+1+FPS_YOFFSET,
                FPS_COLOR(lastFPS < LOW_FPS), -1, tempbuf, x);

            if (gShowFps > 1)
            {
                chars = Bsprintf(tempbuf, "max: %5.1f fps", maxFPS);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+10+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+10+FPS_YOFFSET,
                    FPS_COLOR(maxFPS < LOW_FPS), -1, tempbuf, x);

                chars = Bsprintf(tempbuf, "min: %5.1f fps", minFPS);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+20+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+20+FPS_YOFFSET,
                    FPS_COLOR(minFPS < LOW_FPS), -1, tempbuf, x);
            }
            if (gShowFps > 2)
            {
                if (g_gameUpdateTime > maxGameUpdate) maxGameUpdate = g_gameUpdateTime;
                if (g_gameUpdateTime < minGameUpdate) minGameUpdate = g_gameUpdateTime;

                chars = Bsprintf(tempbuf, "Game Update: %2.2f ms + draw: %2.2f ms", g_gameUpdateTime, g_gameUpdateAndDrawTime);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+30+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+30+FPS_YOFFSET,
                    FPS_COLOR(g_gameUpdateAndDrawTime >= SLOW_FRAME_TIME), -1, tempbuf, x);

                chars = Bsprintf(tempbuf, "GU min/max/avg: %5.2f/%5.2f/%5.2f ms", minGameUpdate, maxGameUpdate, g_gameUpdateAvgTime);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+40+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+40+FPS_YOFFSET,
                    FPS_COLOR(maxGameUpdate >= SLOW_FRAME_TIME), -1, tempbuf, x);
                
                chars = Bsprintf(tempbuf, "bufferjitter: %i", gBufferJitter);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+50+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+50+FPS_YOFFSET,
                    COLOR_WHITE, -1, tempbuf, x);
#if 0
                chars = Bsprintf(tempbuf, "G_MoveActors(): %.3e ms", g_moveActorsTime);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+50+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+50+FPS_YOFFSET,
                    COLOR_WHITE, -1, tempbuf, x);

                chars = Bsprintf(tempbuf, "G_MoveWorld(): %.3e ms", g_moveWorldTime);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+60+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+60+FPS_YOFFSET,
                    COLOR_WHITE, -1, tempbuf, x);
#endif
            }
#if 0
            // lag meter
            if (g_netClientPeer)
            {
                chars = Bsprintf(tempbuf, "%d +- %d ms", (g_netClientPeer->lastRoundTripTime + g_netClientPeer->roundTripTime)/2,
                    (g_netClientPeer->lastRoundTripTimeVariance + g_netClientPeer->roundTripTimeVariance)/2);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+30+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+30+1+FPS_YOFFSET, FPS_COLOR(g_netClientPeer->lastRoundTripTime > 200), -1, tempbuf, x);
            }
#endif
        }

        if (cumulativeFrameDelay >= 1000.0)
        {
            lastFPS = 1000.f * frameCount / cumulativeFrameDelay;
            g_frameRate = Blrintf(lastFPS);
            frameCount = 0;
            cumulativeFrameDelay = 0.0;

            if (gShowFps > 1)
            {
                if (lastFPS > maxFPS) maxFPS = lastFPS;
                if (lastFPS < minFPS) minFPS = lastFPS;

                static int secondCounter;

                if (++secondCounter >= gFramePeriod)
                {
                    maxFPS = (lastFPS + maxFPS) * .5f;
                    minFPS = (lastFPS + minFPS) * .5f;
                    maxGameUpdate = (g_gameUpdateTime + maxGameUpdate) * 0.5;
                    minGameUpdate = (g_gameUpdateTime + minGameUpdate) * 0.5;
                    secondCounter = 0;
                }
            }
        }
        frameCount++;
    }
    lastFrameTime = frameTime;
}

#undef FPS_COLOR

class ViewLoadSave : public LoadSave {
public:
    void Load(void);
    void Save(void);
};

static ViewLoadSave *myLoadSave;

static int messageTime;
static char message[256];

void ViewLoadSave::Load(void)
{
    Read(&messageTime, sizeof(messageTime));
    Read(message, sizeof(message));
    Read(otherMirrorGotpic, sizeof(otherMirrorGotpic));
    Read(bakMirrorGotpic, sizeof(bakMirrorGotpic));
    Read(&gScreenTilt, sizeof(gScreenTilt));
    Read(&deliriumTilt, sizeof(deliriumTilt));
    Read(&deliriumTurn, sizeof(deliriumTurn));
    Read(&deliriumPitch, sizeof(deliriumPitch));
    gScreenTiltO = gScreenTilt;
    deliriumTurnO = deliriumTurn;
    deliriumPitchO = deliriumPitch;
}

void ViewLoadSave::Save(void)
{
    Write(&messageTime, sizeof(messageTime));
    Write(message, sizeof(message));
    Write(otherMirrorGotpic, sizeof(otherMirrorGotpic));
    Write(bakMirrorGotpic, sizeof(bakMirrorGotpic));
    Write(&gScreenTilt, sizeof(gScreenTilt));
    Write(&deliriumTilt, sizeof(deliriumTilt));
    Write(&deliriumTurn, sizeof(deliriumTurn));
    Write(&deliriumPitch, sizeof(deliriumPitch));
}

void ViewLoadSaveConstruct(void)
{
    myLoadSave = new ViewLoadSave();
}
