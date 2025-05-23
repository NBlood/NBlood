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
#include "build.h"
#include "compat.h"
#include "common_game.h"
#include "blood.h"
#include "db.h"
#include "gameutil.h"
#include "levels.h"
#include "loadsave.h"
#include "view.h"
#include "warp.h"
#ifdef NOONE_EXTENSIONS
#include "nnexts.h"
#endif

ZONE gStartZone[kMaxPlayers];
#ifdef NOONE_EXTENSIONS
    ZONE gStartZoneTeam1[kMaxPlayers];
    ZONE gStartZoneTeam2[kMaxPlayers];
    bool gTeamsSpawnUsed = false;
#endif
void warpInit(void)
{
    for (int i = 0; i < kMaxSectors; i++)
    {
        gUpperLink[i] = -1;
        gLowerLink[i] = -1;
    }
    #ifdef NOONE_EXTENSIONS
    int team1 = 0; int team2 = 0; gTeamsSpawnUsed = false; // increment if team start positions specified.
    #endif
    for (int nSprite = 0; nSprite < kMaxSprites; nSprite++)
    {
        if (sprite[nSprite].statnum < kMaxStatus) {
            spritetype *pSprite = &sprite[nSprite];
            int nXSprite = pSprite->extra;
            if (nXSprite > 0) {
                XSPRITE *pXSprite = &xsprite[nXSprite];
                switch (pSprite->type) {
                    case kMarkerSPStart:
                        if ((gGameOptions.nGameType <= kGameTypeCoop) && pXSprite->data1 >= 0 && pXSprite->data1 < kMaxPlayers) {
                            ZONE *pZone = &gStartZone[pXSprite->data1];
                            pZone->x = pSprite->x;
                            pZone->y = pSprite->y;
                            pZone->z = pSprite->z;
                            pZone->sectnum = pSprite->sectnum;
                            pZone->ang = pSprite->ang;
                        }
                        DeleteSprite(nSprite);
                        break;
                    case kMarkerMPStart:
                        if (pXSprite->data1 >= 0 && pXSprite->data2 < kMaxPlayers) {
                            if (gGameOptions.nGameType >= kGameTypeBloodBath) {
                                // default if BB or teams without data2 specified
                                ZONE* pZone = &gStartZone[pXSprite->data1];
                                pZone->x = pSprite->x;
                                pZone->y = pSprite->y;
                                pZone->z = pSprite->z;
                                pZone->sectnum = pSprite->sectnum;
                                pZone->ang = pSprite->ang;
                            
                                #ifdef NOONE_EXTENSIONS
                                    // fill player spawn position according team of player in TEAMS mode.
                                    if (gModernMap && gGameOptions.nGameType == kGameTypeTeams) {
                                        if (pXSprite->data2 == 1) {
                                            pZone = &gStartZoneTeam1[team1];
                                            pZone->x = pSprite->x;
                                            pZone->y = pSprite->y;
                                            pZone->z = pSprite->z;
                                            pZone->sectnum = pSprite->sectnum;
                                            pZone->ang = pSprite->ang;
                                            team1++;

                                        } else if (pXSprite->data2 == 2) {
                                            pZone = &gStartZoneTeam2[team2];
                                            pZone->x = pSprite->x;
                                            pZone->y = pSprite->y;
                                            pZone->z = pSprite->z;
                                            pZone->sectnum = pSprite->sectnum;
                                            pZone->ang = pSprite->ang;
                                            team2++;
                                        }
                                    }
                                #endif

                            }
                            DeleteSprite(nSprite);
                        }
                        break;
                    case kMarkerUpLink:
                        gUpperLink[pSprite->sectnum] = nSprite;
                        pSprite->cstat |= 32768;
                        pSprite->cstat &= ~257;
                        break;
                    case kMarkerLowLink:
                        gLowerLink[pSprite->sectnum] = nSprite;
                        pSprite->cstat |= 32768;
                        pSprite->cstat &= ~257;
                        break;
                    case kMarkerUpWater:
                    case kMarkerUpStack:
                    case kMarkerUpGoo:
                        gUpperLink[pSprite->sectnum] = nSprite;
                        pSprite->cstat |= 32768;
                        pSprite->cstat &= ~257;
                        pSprite->z = getflorzofslope(pSprite->sectnum, pSprite->x, pSprite->y);
                        break;
                    case kMarkerLowWater:
                    case kMarkerLowStack:
                    case kMarkerLowGoo:
                        gLowerLink[pSprite->sectnum] = nSprite;
                        pSprite->cstat |= 32768;
                        pSprite->cstat &= ~257;
                        pSprite->z = getceilzofslope(pSprite->sectnum, pSprite->x, pSprite->y);
                        break;
                }
            }
        }
    }
    
    #ifdef NOONE_EXTENSIONS
    // check if there is enough start positions for teams, if any used
    if (team1 > 0 || team2 > 0) {
        gTeamsSpawnUsed = true;
        if (team1 < kMaxPlayers / 2 || team2 < kMaxPlayers / 2) {
            viewSetSystemMessage("At least 4 spawn positions for each team is recommended.");
            viewSetSystemMessage("Team A positions: %d, Team B positions: %d.", team1, team2);
        }
    }
    #endif

    for (int i = 0; i < kMaxSectors; i++)
    {
        int nSprite = gUpperLink[i];
        if (nSprite >= 0)
        {
            spritetype *pSprite = &sprite[nSprite];
            int nXSprite = pSprite->extra;
            dassert(nXSprite > 0 && nXSprite < kMaxXSprites);
            XSPRITE *pXSprite = &xsprite[nXSprite];
            int nLink = pXSprite->data1;
            for (int j = 0; j < kMaxSectors; j++)
            {
                int nSprite2 = gLowerLink[j];
                if (nSprite2 >= 0)
                {
                    spritetype *pSprite2 = &sprite[nSprite2];
                    int nXSprite = pSprite2->extra;
                    dassert(nXSprite > 0 && nXSprite < kMaxXSprites);
                    XSPRITE *pXSprite2 = &xsprite[nXSprite];
                    if (pXSprite2->data1 == nLink)
                    {
                        pSprite->owner = gLowerLink[j];
                        pSprite2->owner = gUpperLink[i];
                    }
                }
            }
        }
    }
}

int CheckLink(spritetype *pSprite)
{
    int nSector = pSprite->sectnum;
    int nUpper = gUpperLink[nSector];
    int nLower = gLowerLink[nSector];
    if (nUpper >= 0)
    {
        spritetype *pUpper = &sprite[nUpper];
        int z;
        if (pUpper->type == kMarkerUpLink)
            z = pUpper->z;
        else
            z = getflorzofslope(pSprite->sectnum, pSprite->x, pSprite->y);
        if (z <= pSprite->z)
        {
            nLower = pUpper->owner;
            dassert(nLower >= 0 && nLower < kMaxSprites);
            spritetype *pLower = &sprite[nLower];
            dassert(pLower->sectnum >= 0 && pLower->sectnum < kMaxSectors);
            ChangeSpriteSect(pSprite->index, pLower->sectnum);
            vec3_t const oldpos = pSprite->xyz;
            pSprite->x += pLower->x-pUpper->x;
            pSprite->y += pLower->y-pUpper->y;
            int z2;
            if (pLower->type == kMarkerLowLink)
                z2 = pLower->z;
            else
                z2 = getceilzofslope(pSprite->sectnum, pSprite->x, pSprite->y);
            pSprite->z += z2-z;
            if (!VanillaMode()) // if sprite is set to be interpolated, update previous position
                viewCorrectSpriteInterpolateOffsets(pSprite->index, pSprite, &oldpos);
            else
                ClearBitString(gInterpolateSprite, pSprite->index);
            return pUpper->type;
        }
    }
    if (nLower >= 0)
    {
        spritetype *pLower = &sprite[nLower];
        int z;
        if (pLower->type == kMarkerLowLink)
            z = pLower->z;
        else
            z = getceilzofslope(pSprite->sectnum, pSprite->x, pSprite->y);
        if (z >= pSprite->z)
        {
            nUpper = pLower->owner;
            dassert(nUpper >= 0 && nUpper < kMaxSprites);
            spritetype *pUpper = &sprite[nUpper];
            dassert(pUpper->sectnum >= 0 && pUpper->sectnum < kMaxSectors);
            ChangeSpriteSect(pSprite->index, pUpper->sectnum);
            vec3_t const oldpos = pSprite->xyz;
            pSprite->x += pUpper->x-pLower->x;
            pSprite->y += pUpper->y-pLower->y;
            int z2;
            if (pUpper->type == kMarkerUpLink)
                z2 = pUpper->z;
            else
                z2 = getflorzofslope(pSprite->sectnum, pSprite->x, pSprite->y);
            pSprite->z += z2-z;
            if (!VanillaMode()) // if sprite is set to be interpolated, update previous position
                viewCorrectSpriteInterpolateOffsets(pSprite->index, pSprite, &oldpos);
            else
                ClearBitString(gInterpolateSprite, pSprite->index);
            return pLower->type;
        }
    }
    return 0;
}

int CheckLink(int *x, int *y, int *z, int *nSector)
{
    int nUpper = gUpperLink[*nSector];
    int nLower = gLowerLink[*nSector];
    if (nUpper >= 0)
    {
        spritetype *pUpper = &sprite[nUpper];
        int z1;
        if (pUpper->type == kMarkerUpLink)
            z1 = pUpper->z;
        else
            z1 = getflorzofslope(*nSector, *x, *y);
        if (z1 <= *z)
        {
            nLower = pUpper->owner;
            dassert(nLower >= 0 && nLower < kMaxSprites);
            spritetype *pLower = &sprite[nLower];
            dassert(pLower->sectnum >= 0 && pLower->sectnum < kMaxSectors);
            *nSector = pLower->sectnum;
            *x += pLower->x-pUpper->x;
            *y += pLower->y-pUpper->y;
            int z2;
            if (pUpper->type == kMarkerLowLink)
                z2 = pLower->z;
            else
                z2 = getceilzofslope(*nSector, *x, *y);
            *z += z2-z1;
            return pUpper->type;
        }
    }
    if (nLower >= 0)
    {
        spritetype *pLower = &sprite[nLower];
        int z1;
        if (pLower->type == kMarkerLowLink)
            z1 = pLower->z;
        else
            z1 = getceilzofslope(*nSector, *x, *y);
        if (z1 >= *z)
        {
            nUpper = pLower->owner;
            dassert(nUpper >= 0 && nUpper < kMaxSprites);
            spritetype *pUpper = &sprite[nUpper];
            dassert(pUpper->sectnum >= 0 && pUpper->sectnum < kMaxSectors);
            *nSector = pUpper->sectnum;
            *x += pUpper->x-pLower->x;
            *y += pUpper->y-pLower->y;
            int z2;
            if (pLower->type == kMarkerUpLink)
                z2 = pUpper->z;
            else
                z2 = getflorzofslope(*nSector, *x, *y);
            *z += z2-z1;
            return pLower->type;
        }
    }
    return 0;
}

class WarpLoadSave : public LoadSave
{
public:
    virtual void Load();
    virtual void Save();
};

void WarpLoadSave::Load()
{
    Read(gStartZone, sizeof(gStartZone));
    Read(gUpperLink, sizeof(gUpperLink));
    Read(gLowerLink, sizeof(gLowerLink));
}

void WarpLoadSave::Save()
{
    Write(gStartZone, sizeof(gStartZone));
    Write(gUpperLink, sizeof(gUpperLink));
    Write(gLowerLink, sizeof(gLowerLink));
}

static WarpLoadSave *myLoadSave;

void WarpLoadSaveConstruct(void)
{
    myLoadSave = new WarpLoadSave();
}
