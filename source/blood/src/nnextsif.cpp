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

#include "build.h"
#include "common_game.h"
#include "tile.h"
#include "seq.h"
#include "trig.h"
#include "view.h"
#include "endgame.h"
#include "aicdud.h"
#include "mmulti.h"
#include "nnexts.h"
#include "nnextsif.h"
#include "mirrors.h"

#define kSerialStep					        100000

#define kPushRange                          3
#define kCmdPush                            64
#define kCmdPop                             100


enum ENUM_EVENT_OBJECT {
EVOBJ_SPRITE                    = OBJ_SPRITE,
EVOBJ_SECTOR                    = OBJ_SECTOR,
EVOBJ_WALL                      = OBJ_WALL,
};

enum ENUM_CONDITION_SERIAL {
kSerialSector                   = kSerialStep   + kMaxSectors + kMaxWalls + kMaxSprites,
kSerialWall                     = kSerialSector + kSerialStep,
kSerialSprite                   = kSerialWall   + kSerialStep,
kSerialMax                      = kSerialSprite + kSerialStep,
};

enum ENUM_CONDITION_ERROR {
kErrInvalidObject               = 0,
kErrInvalidSerial,
kErrUnknownObject,
kErrInvalidArgsPass,
kErrObjectUnsupp,
kErrNotImplementedCond,
};

enum ENUM_CONDITION_TYPE {
CNON                            = 0,
CGAM,
CMIX,
CWAL,
CSEC,
CPLY,
CDUDG,
CDUDC,
CSPR,
};

struct CHECKFUNC_INFO
{
    char (*pFunc)( void );              // function to call the condition
    unsigned int type           : 5;	// type of condition
    const char* name;                   // for errors output
};

struct CONDITION_INFO
{
    char (*pFunc)( void );              // condition function
    unsigned int id             : 10;	// condition id
    unsigned int type           : 5;	// type of condition
    unsigned int isBool         : 1;    // false = do comparison using Cmp()
    unsigned int xReq           : 1;    // is x-object required?
    CHECKFUNC_INFO* pCaller  = NULL;	// provided for easy access and must be set during init
};

struct TRACKING_CONDITION
{
    unsigned int id             : 16;   // x-sprite index of condition
    OBJECT_LIST* objects;               // a dynamic list of objects it contains
};

static const char* gErrors[] =
{
    "Object #%d (objType: %d) is not a %s!",
    "%d is not condition serial!",
    "Unknown object type %d, index %d.",
    "Invalid arguments passed.",
    "Unsupported %s type %d, extra: %d.",
    "Condition is not implemented.",
};

/** GLOBAL coditions table
********************************************************************************/
static CONDITION_INFO* gConditions = NULL;
static unsigned int gNumConditions = 0;



/** TRACKING(LOOPED) coditions list
********************************************************************************/
static TRACKING_CONDITION* gTrackingConditionsList = NULL;
static unsigned int gTrackingConditionsListLength = 0;



/** Variables that is relative to current condition
********************************************************************************/
static spritetype* pCond = NULL; static XSPRITE* pXCond = NULL;		// current condition
static int arg1 = 0, arg2 = 0, arg3 = 0;							// arguments of current condition (data2, data3, data4)
static int cmpOp = 0;												// current comparison operator (condition sprite cstat)
static char PUSH = false;											// current stack push status (kCmdNumberic)
static CONDITION_INFO* pEntry = NULL;								// current condition db entry
static EVENT* pEvent = NULL;										// current event

static PLAYER* pPlayer = NULL;										// player in the focus
static int objType = -1, objIndex = -1;						        // object in the focus
static spritetype* pSpr = NULL; 	static XSPRITE* pXSpr = NULL;	// (x)sprite in the focus
static walltype* pWall = NULL; 	    static XWALL* pXWall = NULL;	// (x)wall in the focus
static sectortype* pSect = NULL; 	static XSECTOR* pXSect = NULL;	// (x)sector in the focus
static char xAvail = false;											// x-object indicator



/** INTERFACE functions
********************************************************************************/
static void Unserialize(int nSerial, int* oType, int* oIndex);
static char Cmp(int val, int nArg1, int nArg2);
static int  Serialize(int oType, int oIndex);
static void Push(int oType, int oIndex);
static void TriggerObject(int nSerial);
static void Error(const char* pFormat, ...);
static void ReceiveObjects(EVENT* pFrom);
static char Cmp(int val);
static char DefaultResult();
static void Restore();

static char CheckCustomDude();
static char CheckGeneric();
static char CheckSector();
static char CheckSprite();
static char CheckObject();
static char CheckPlayer();
static char CheckWall();
static char CheckDude();



/** A LIST OF CONDITION FUNCTION CALLERS
********************************************************************************/
static CHECKFUNC_INFO gCheckFuncInfo[] =
{
    { CheckGeneric,				CGAM,		"Game"			},
    { CheckObject,				CMIX,		"Mixed"			},
    { CheckWall,				CWAL,		"Wall"          },
    { CheckSector, 				CSEC,		"Sector"        },
    { CheckPlayer,				CPLY,		"Player"        },
    { CheckDude,				CDUDG,		"Dude"          },
    { CheckCustomDude,			CDUDC,		"Custom Dude"   },
    { CheckSprite,				CSPR,		"Sprite"        },
    { CheckGeneric,				CNON,		"Unknown"       },
};

static int qsSortCheckFuncInfo(CHECKFUNC_INFO* ref1, CHECKFUNC_INFO* ref2)
{
    return ref1->type - ref2->type;
}



/** ERROR functions
********************************************************************************/
static char errCondNotImplemented(void)
{
    Error(gErrors[kErrNotImplementedCond]);
    return false;
}


/** HELPER functions
********************************************************************************/
static char helperChkSprite(int nSprite)
{
    if (!rngok(nSprite, 0, kMaxSprites)) return false;
    else if (PUSH) Push(EVOBJ_SPRITE, nSprite);
    return true;
}

static char helperChkSector(int nSect)
{
    if (!rngok(nSect, 0, numsectors)) return false;
    else if (PUSH) Push(EVOBJ_SECTOR, nSect);
    return true;
}

static char helperChkWall(int nWall)
{
    if (!rngok(nWall, 0, numwalls)) return false;
    else if (PUSH) Push(EVOBJ_WALL, nWall);
    return true;
}

static char helperCmpHeight(char ceil)
{
    int nHeigh1, nHeigh2;
    switch (pSect->type)
    {
        case kSectorZMotion:
        case kSectorRotate:
        case kSectorSlide:
        case kSectorSlideMarked:
        case kSectorRotateMarked:
        case kSectorRotateStep:
            if (ceil)
            {
                nHeigh1 = ClipLow(abs(pXSect->onCeilZ - pXSect->offCeilZ), 1);
                nHeigh2 = abs(pSect->ceilingz - pXSect->offCeilZ);
            }
            else
            {
                nHeigh1 = ClipLow(abs(pXSect->onFloorZ - pXSect->offFloorZ), 1);
                nHeigh2 = abs(pSect->floorz - pXSect->offFloorZ);
            }
            return Cmp((kPercFull * nHeigh2) / nHeigh1);
        default:
            Error(gErrors[kErrObjectUnsupp], "sector", pSect->type, pSect->extra);
            return false;
    }
}

static char helperCmpData(int nData)
{
    switch (objType)
    {
        case EVOBJ_WALL:
            return Cmp(pXWall->data);
        case EVOBJ_SPRITE:
            switch (nData)
            {
                case 1:		return Cmp(pXSpr->data1);
                case 2:		return Cmp(pXSpr->data2);
                case 3:		return Cmp(pXSpr->data3);
                case 4:		return Cmp(pXSpr->data4);
            }
            break;
        case EVOBJ_SECTOR:
            return Cmp(pXSect->data);
    }

    return Cmp(0);
}

static char helperCmpSeq(int (*pFunc) (int, int))
{
    switch (objType)
    {
        case EVOBJ_SPRITE:
            return Cmp(pFunc(3, pSpr->extra));
        case EVOBJ_WALL:
            switch (arg3)
            {
                default:	return Cmp(pFunc(0, pWall->extra)) || Cmp(pFunc(4, pWall->extra));
                case  1:	return Cmp(pFunc(0, pWall->extra));
                case  2:	return Cmp(pFunc(4, pWall->extra));	// masked wall
            }
            break;
        case EVOBJ_SECTOR:
            switch (arg3)
            {
                default:	return Cmp(pFunc(1, pSect->extra)) || Cmp(pFunc(2, pSect->extra));
                case  1:	return Cmp(pFunc(1, pSect->extra));
                case  2:	return Cmp(pFunc(2, pSect->extra));
            }
            break;
    }

    return Cmp(0);

}

static char helperChkHitscan(int nWhat)
{
    int nAng = pSpr->ang & kAngMask;
    int nOldStat = pSpr->cstat;
    int nHit = -1, nSlope = 0;
    int nMask = -1;

    if ((nOldStat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_SLOPE)
        nSlope = spriteGetSlope(pSpr->index);

    pSpr->cstat = 0;
    if (nOldStat & CSTAT_SPRITE_YCENTER)
        pSpr->cstat |= CSTAT_SPRITE_YCENTER;

    if ((nOldStat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_FLOOR)
        pSpr->cstat |= CSTAT_SPRITE_ALIGNMENT_FLOOR;

    switch (arg1)
    {
        case  0: 	nMask = CLIPMASK0 | CLIPMASK1; break;
        case  1: 	nMask = CLIPMASK0; break;
        case  2: 	nMask = CLIPMASK1; break;
    }

    if ((pPlayer = getPlayerById(pSpr->type)) != NULL)
    {
        nHit = HitScan(pSpr, pPlayer->zWeapon, pPlayer->aim.dx, pPlayer->aim.dy, pPlayer->aim.dz, nMask, arg3 << 1);
    }
    else if (IsDudeSprite(pSpr))
    {
        nHit = HitScan(pSpr, pSpr->z, Cos(nAng) >> 16, Sin(nAng) >> 16, gDudeSlope[pSpr->extra], nMask, arg3 << 1);
    }
    else if ((nOldStat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_SLOPE)
    {
        nSlope = (nOldStat & CSTAT_SPRITE_YFLIP) ? (0x08000 - abs(nSlope)) : -(0x08000 - abs(nSlope));
        nHit = HitScan(pSpr, pSpr->z, Cos(nAng) >> 16, Sin(nAng) >> 16, nSlope, nMask, arg3 << 1);
    }
    else if ((nOldStat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_FLOOR)
    {
        nSlope = (nOldStat & CSTAT_SPRITE_YFLIP) ? (0x10000) : -(0x10000);
        nHit = HitScan(pSpr, pSpr->z, Cos(nAng) >> 16, Sin(nAng) >> 16, nSlope, nMask, arg3 << 1);
    }
    else
    {
        nHit = HitScan(pSpr, pSpr->z, Cos(nAng) >> 16, Sin(nAng) >> 16, 0, nMask, arg3 << 1);
    }

    pSpr->cstat = nOldStat;

    if (nHit < 0)
        return false;

    switch (nWhat)
    {
        case 1:	// ceil
            if (nHit != 1) return false;
            else if (PUSH) Push(EVOBJ_SECTOR, gHitInfo.hitsect);
            return true;
        case 2:	// floor
            if (nHit != 2) return false;
            else if (PUSH) Push(EVOBJ_SECTOR, gHitInfo.hitsect);
            return true;
        case 3:	// wall
            if (nHit != 0 && nHit != 4) return false;
            else if (PUSH) Push(EVOBJ_WALL, gHitInfo.hitwall);
            return true;
        case 4:	// sprite
            if (nHit != 3) return false;
            else if (PUSH) Push(EVOBJ_SPRITE, gHitInfo.hitsprite);
            return true;
        case 5: // masked wall
            if (nHit != 4) return false;
            else if (PUSH) Push(EVOBJ_WALL, gHitInfo.hitwall);
            return true;
    }

    Error(gErrors[kErrInvalidArgsPass]);
    return false;
}

static char helperChkMarker(int nWhat)
{
    int t;
    if (pXSpr->dudeFlag4 && spriRangeIsFine(pXSpr->target) && sprite[pXSpr->target].type == kMarkerPath)
    {
        switch (nWhat) {
            case 1:
                if ((t = aiPatrolMarkerBusy(pSpr->index, pXSpr->target)) >= 0)
                {
                    if (PUSH) Push(EVOBJ_SPRITE, t);
                    return true;
                }
                break;
            case 2:
                if (aiPatrolMarkerReached(pSpr, pXSpr))
                {
                    if (PUSH) Push(EVOBJ_SPRITE, pXSpr->target);
                    return true;
                }
                break;
            default:
                Error(gErrors[kErrInvalidArgsPass]);
                break;
        }
    }

    return false;
};

static char helperChkTarget(int nWhat)
{
    int t = 0;
    if (spriRangeIsFine(pXSpr->target))
    {
        spritetype* pTrgt = &sprite[pXSpr->target]; DUDEINFO* pInfo = getDudeInfo(pSpr->type);
        int eyeAboveZ = pInfo->eyeHeight * pSpr->yrepeat << 2;
        int dx = pTrgt->x - pSpr->x; int dy = pTrgt->y - pSpr->y;

        switch (nWhat) {
            case 1:
                arg1 *= 512, arg2 *= 512;
                t = Cmp(approxDist(dx, dy));
                break;
            case 2:
            case 3:
                t = cansee(pSpr->x, pSpr->y, pSpr->z, pSpr->sectnum, pTrgt->x, pTrgt->y, pTrgt->z - eyeAboveZ, pTrgt->sectnum);
                if (t > 0 && nWhat == 3)
                {
                    t = ((1024 + getangle(dx, dy) - pSpr->ang) & 2047) - 1024;
                    t = (klabs(t) < ((arg1 <= 0) ? pInfo->periphery : ClipHigh(arg1, 2048)));
                }
                break;
            default:
                Error(gErrors[kErrInvalidArgsPass]);
                break;
        }
    }

    if (t <= 0) return false;
    else if (PUSH) Push(EVOBJ_SPRITE, pXSpr->target);
    return true;
};

static char helperChkRoom(short* pArray, int nWhat)
{
    int t = pArray[objIndex];
    if (t >= 0)
    {
        if (PUSH)
        {
            switch (nWhat)
            {
                case 1:
                    Push(EVOBJ_SPRITE, t);
                    break;
                case 2:
                    t = sprite[t].owner & 0x3fff;
                    Push(EVOBJ_SPRITE, t);
                    break;
                default:
                    t = sprite[t].owner & 0x3fff;
                    Push(EVOBJ_SECTOR, sprite[t].sectnum);
                    break;
            }
        }

        return true;
    }

    return false;
}


/** GAME conditions
********************************************************************************/
static char gamCmpLevelMin(void)        { return Cmp(gLevelTime / (kTicsPerSec * 60)); }
static char gamCmpLevelSec(void)        { return Cmp((gLevelTime / kTicsPerSec) % 60); }
static char gamCmpLevelMsec(void)       { return Cmp(((gLevelTime % kTicsPerSec) * 33) / 10); }
static char gamCmpLevelTime(void)       { return Cmp(gLevelTime); }
static char gamCmpKillsTotal(void)      { return Cmp(gKillMgr.at0); }
static char gamCmpKillsDone(void)       { return Cmp(gKillMgr.at4); }
static char gamCmpSecretsTotal(void)    { return Cmp(gSecretMgr.nAllSecrets); }
static char gamCmpSecretsDone(void)     { return Cmp(gSecretMgr.nNormalSecretsFound); }
static char gamCmpVisibility(void)      { return Cmp(gVisibility); }
static char gamChkGotpic(void)          { return (rngok(arg3, 0, kMaxTiles) && TestBitString(gotpic, arg3)); }
static char gamChkChance(void)          { return Chance((0x10000 * arg3) / kPercFull); }
static char gamCmpRandom(void)          { return Cmp(nnExtRandom(arg1, arg2)); }
static char gamCmpStatnumCount(void)    { return Cmp(gStatCount[ClipRange(arg3, 0, kMaxStatus)]); }
static char gamCmpNumsprites(void)      { return Cmp(Numsprites); }
static char gamChkPlayerConnect(void)
{
    int i;
    for (i = connecthead; i >= 0; i = connectpoint2[i])
    {
        if (gPlayer[i].nPlayer + 1 != arg3) continue;
        else if (PUSH) Push(EVOBJ_SPRITE, gPlayer[i].nSprite);
        return true;
    }

    return false;
}
static char gamChkSector(void)      { return helperChkSector(arg1); }
static char gamChkWall(void)        { return helperChkWall(arg1); }
static char gamChkSprite(void)      { return helperChkSprite(arg1); }



/** SECTOR conditions
********************************************************************************/
static char sectCmpVisibility(void)     { return Cmp(pSect->visibility); }
static char sectChkShow2dSector(void)   { return TestBitString(show2dsector, objIndex); }
static char sectChkGotSector(void)      { return TestBitString(gotsector, objIndex); }
static char sectCmpFloorSlope(void)     { return Cmp(pSect->floorheinum); }
static char sectCmpCeilSlope(void)      { return Cmp(pSect->ceilingheinum); }
static char sectChkSprTypeInSect(void)
{
    int i;
    for (i = headspritesect[objIndex]; i >= 0; i = nextspritesect[i])
    {
        if (!Cmp(sprite[i].type)) continue;
        else if (PUSH) Push(EVOBJ_SPRITE, i);
        return true;
    }
    return false;
}

static char sectCmpFloorHeight(void)    { return helperCmpHeight(false); }
static char sectCmpCeilHeight(void)     { return helperCmpHeight(true); }
static char sectChkUnderwater(void)     { return pXSect->Underwater; }
static char sectChkPaused(void)         { return !pXSect->unused1; }
static char sectCmpDepth(void)          { return Cmp(pXSect->Depth); }
static char sectChkUpperRoom(void)      { return helperChkRoom(gUpperLink, arg3); }
static char sectChkLowerRoom(void)      { return helperChkRoom(gLowerLink, arg3); }



/** WALL conditions
********************************************************************************/
static char wallCmpOverpicnum(void)     { return Cmp(pWall->overpicnum); }
static char wallChkShow2dWall(void)     { return TestBitString(show2dwall, objIndex); }
static char wallChkIsMirror(void)
{
    return (pWall->type != kWallStack && Cmp(pWall->picnum, mirrorPicStart, mirrorPicEnd-1));
}

static char wallChkSector(void)         { return helperChkSector(sectorofwall(objIndex)); }
static char wallChkNextSector(void)     { return helperChkSector(pWall->nextsector); }
static char wallChkNextWallSector(void) { return helperChkSector(sectorofwall(pWall->nextwall)); }
static char wallChkNextWall(void)       { return helperChkWall(pWall->nextwall); }
static char wallChkPoint2(void)         { return helperChkWall(pWall->point2); }





/** MIXED OBJECT conditions
********************************************************************************/
static char mixChkObjSect(void)         { return (objType == EVOBJ_SECTOR && sectRangeIsFine(objIndex)); }
static char mixChkObjWall(void)         { return (objType == EVOBJ_WALL && wallRangeIsFine(objIndex)); }
static char mixChkObjSpr(void)          { return (objType == EVOBJ_SPRITE && spriRangeIsFine(objIndex)); }
static char mixChkXRange(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return xwallRangeIsFine(pWall->extra);
        case EVOBJ_SPRITE:	return xspriRangeIsFine(pSpr->extra);
        case EVOBJ_SECTOR:	return xsectRangeIsFine(pSect->extra);
    }

    return false;
}
/**---------------------------------------------------------------------------**/
static char mixCmpLotag(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pWall->type);
        case EVOBJ_SPRITE:	return Cmp(pSpr->type);
        case EVOBJ_SECTOR:	return Cmp(pSect->type);
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpPicSurface(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(surfType[pWall->picnum]);
        case EVOBJ_SPRITE:	return Cmp(surfType[pSpr->picnum]);
        case EVOBJ_SECTOR:
            switch (arg3)
            {
                default:	return (Cmp(surfType[pSect->floorpicnum]) || Cmp(surfType[pSect->ceilingpicnum]));
                case  1:	return Cmp(surfType[pSect->floorpicnum]);
                case  2:	return Cmp(surfType[pSect->ceilingpicnum]);
            }
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpPic(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pWall->picnum);
        case EVOBJ_SPRITE:	return Cmp(pSpr->picnum);
        case EVOBJ_SECTOR:
            switch (arg3)
            {
                default:	return (Cmp(pSect->floorpicnum) || Cmp(pSect->ceilingpicnum));
                case  1:	return Cmp(pSect->floorpicnum);
                case  2:	return Cmp(pSect->ceilingpicnum);
            }
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpPal(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pWall->pal);
        case EVOBJ_SPRITE:	return Cmp(pSpr->pal);
        case EVOBJ_SECTOR:
            switch (arg3)
            {
                default:	return (Cmp(pSect->floorpal) || Cmp(pSect->ceilingpal));
                case  1:	return Cmp(pSect->floorpal);
                case  2:	return Cmp(pSect->ceilingpal);
            }
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpShade(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pWall->shade);
        case EVOBJ_SPRITE:	return Cmp(pSpr->shade);
        case EVOBJ_SECTOR:
            switch (arg3)
            {
                default:	return (Cmp(pSect->floorshade) || Cmp(pSect->ceilingshade));
                case  1:	return Cmp(pSect->floorshade);
                case  2:	return Cmp(pSect->ceilingshade);
            }
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpCstat(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return (bool)((arg3) ? Cmp(pWall->cstat & arg3) : (pWall->cstat & arg1));
        case EVOBJ_SPRITE:	return (bool)((arg3) ? Cmp(pSpr->cstat & arg3) : (pSpr->cstat & arg1));
        case EVOBJ_SECTOR:	// !!!
            switch (arg3)
            {
                default:	return ((pSect->floorstat & arg1) || (pSect->ceilingstat & arg1));
                case  1:	return (pSect->floorstat & arg1);
                case  2:	return (pSect->ceilingstat & arg1);
            }
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpHitag(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return (bool)((arg3) ? Cmp(pWall->hitag & arg3) : (pWall->hitag & arg1));
        case EVOBJ_SPRITE:	return (bool)((arg3) ? Cmp(pSpr->hitag & arg3) : (pSpr->hitag & arg1));
        case EVOBJ_SECTOR:	return (bool)((arg3) ? Cmp(pSect->hitag & arg3) : (pSect->hitag & arg1));
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpXrepeat(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pWall->xrepeat);
        case EVOBJ_SPRITE:	return Cmp(pSpr->xrepeat);
        case EVOBJ_SECTOR:	return Cmp(pSect->floorxpanning);
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpXoffset(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pWall->xpanning);
        case EVOBJ_SPRITE:	return Cmp(pSpr->xoffset);
        case EVOBJ_SECTOR:	return Cmp(pSect->ceilingxpanning);
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpYrepeat(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pWall->yrepeat);
        case EVOBJ_SPRITE:	return Cmp(pSpr->yrepeat);
        case EVOBJ_SECTOR:	return Cmp(pSect->floorypanning);
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpYoffset(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pWall->ypanning);
        case EVOBJ_SPRITE:	return Cmp(pSpr->yoffset);
        case EVOBJ_SECTOR:	return Cmp(pSect->ceilingypanning);
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpData1(void) { return helperCmpData(1); }
static char mixCmpData2(void) { return helperCmpData(2); }
static char mixCmpData3(void) { return helperCmpData(3); }
static char mixCmpData4(void) { return helperCmpData(4); }
static char mixCmpRXId(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pXWall->rxID);
        case EVOBJ_SPRITE:	return Cmp(pXSpr->rxID);
        case EVOBJ_SECTOR:	return Cmp(pXSect->rxID);
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixCmpTXId(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pXWall->txID);
        case EVOBJ_SPRITE:	return Cmp(pXSpr->txID);
        case EVOBJ_SECTOR:	return Cmp(pXSect->txID);
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixChkLock(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return pXWall->locked;
        case EVOBJ_SPRITE:	return pXSpr->locked;
        case EVOBJ_SECTOR:	return pXSect->locked;
    }

    return false;
}
/**---------------------------------------------------------------------------**/
static char mixChkTriggerOn(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return pXWall->triggerOn;
        case EVOBJ_SPRITE:	return pXSpr->triggerOn;
        case EVOBJ_SECTOR:	return pXSect->triggerOn;
    }

    return false;
}
/**---------------------------------------------------------------------------**/
static char mixChkTriggerOff(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return pXWall->triggerOff;
        case EVOBJ_SPRITE:	return pXSpr->triggerOff;
        case EVOBJ_SECTOR:	return pXSect->triggerOff;
    }

    return false;
}
/**---------------------------------------------------------------------------**/
static char mixChkTriggerOnce(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return pXWall->triggerOnce;
        case EVOBJ_SPRITE:	return pXSpr->triggerOnce;
        case EVOBJ_SECTOR:	return pXSect->triggerOnce;
    }

    return false;
}
/**---------------------------------------------------------------------------**/
static char mixChkIsTriggered(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return pXWall->isTriggered;
        case EVOBJ_SPRITE:	return pXSpr->isTriggered;
        case EVOBJ_SECTOR:	return pXSect->isTriggered;
    }

    return false;
}
/**---------------------------------------------------------------------------**/
static char mixChkState(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return pXWall->state;
        case EVOBJ_SPRITE:	return pXSpr->state;
        case EVOBJ_SECTOR:	return pXSect->state;
    }

    return false;
}
/**---------------------------------------------------------------------------**/
static char mixCmpBusy(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp((kPercFull * pXWall->busy) / 65536);
        case EVOBJ_SPRITE:	return Cmp((kPercFull * pXSpr->busy) / 65536);
        case EVOBJ_SECTOR:	return Cmp((kPercFull * pXSect->busy) / 65536);
    }

    return Cmp(0);
}
/**---------------------------------------------------------------------------**/
static char mixChkPlayerOnly(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return pXWall->dudeLockout;
        case EVOBJ_SPRITE:	return pXSpr->DudeLockout;
        case EVOBJ_SECTOR:	return pXSect->dudeLockout;
    }

    return false;
}
/**---------------------------------------------------------------------------**/
static char mixCmpSeqID(void)           { return helperCmpSeq(seqGetID); }
static char mixCmpSeqFrame(void)        { return helperCmpSeq(seqGetStatus); }
static char mixCmpObjIndex(void)        { return Cmp(objIndex); }
static char mixCmpObjXIndex(void)
{
    switch (objType)
    {
        case EVOBJ_WALL: 	return Cmp(pWall->extra);
        case EVOBJ_SPRITE:	return Cmp(pSpr->extra);
        case EVOBJ_SECTOR:	return Cmp(pSect->extra);
    }

    return Cmp(0);
}


static char mixCmpSerials(void)
{
    unsigned int i = 0, d;
    int serials[kPushRange + 1] = { pXCond->targetX,  pXCond->targetY,  pXCond->targetZ,  pXCond->sysData1 };
    int t[3];
    
    while (i < LENGTH(t))
    {
        d = getDigitFromValue(arg1, i);
        if (!rngok(d, 1, LENGTH(serials)))
        {
            Error(gErrors[kErrInvalidArgsPass]);
            return false;
        }

        t[i++] = serials[d - 1];
    }
    
    return Cmp(t[0], t[1], t[2]);

}
static char mixChkEventCauser(void)     { return helperChkSprite(pEvent->causer); }
static char mixCmpEventCmd(void)        { return Cmp(pEvent->cmd); }





/** SPRITE conditions
********************************************************************************/
static char sprCmpAng(void)             { return Cmp((arg3 == 0) ? (pSpr->ang & 2047) : pSpr->ang); }
static char sprChkShow2dSprite(void)    { return TestBitString(show2dsprite, objIndex); }
static char sprCmpStatnum(void)         { return Cmp(pSpr->statnum); }
static char sprChkRespawn(void)         { return ((pSpr->flags & kHitagRespawn) || pSpr->statnum == kStatRespawn); }
static char sprCmpSlope(void)           { return Cmp(spriteGetSlope(pSpr->index)); }
static char sprCmpClipdist(void)        { return Cmp(pSpr->clipdist); }
static char sprChkOwner(void)           { return helperChkSprite(pSpr->owner); }
static char sprChkSector(void)          { return helperChkSector(pSpr->sectnum); }
static char sprCmpVelocityNew(void)
{
    switch (arg3)
    {
        case 0:		return (Cmp(xvel[pSpr->index]) || Cmp(yvel[pSpr->index]) || Cmp(zvel[pSpr->index]));
        case 1:		return Cmp(xvel[pSpr->index]);
        case 2:		return Cmp(yvel[pSpr->index]);
        case 3:		return Cmp(zvel[pSpr->index]);
    }

    Error(gErrors[kErrInvalidArgsPass]);
    return false;
}
static char sprCmpChkVelocity(void)
{
    if (arg3)
        return sprCmpVelocityNew();

    switch (arg1)
    {
        case 0:		return (xvel[pSpr->index] != 0 || yvel[pSpr->index] != 0 || zvel[pSpr->index] != 0);
        case 1:		return (xvel[pSpr->index] != 0);
        case 2:		return (yvel[pSpr->index] != 0);
        case 3:		return (zvel[pSpr->index] != 0);
    }

    Error(gErrors[kErrInvalidArgsPass]);
    return false;
}

/**---------------------------------------------------------------------------**/
static char sprChkUnderwater(void)      { return isUnderwaterSector(pSpr->sectnum); }
static char sprChkDmgImmune(void)
{
    int i;
    if (arg1 == -1)
    {
        for (i = 0; i < kDmgMax; i++)
        {
            if (!nnExtIsImmune(pSpr, i, 0))
                return false;
        }

        return true;
    }

    return nnExtIsImmune(pSpr, arg1, 0);
}
/**---------------------------------------------------------------------------**/
static char sprChkHitscanCeil(void)     { return helperChkHitscan(1); }
static char sprChkHitscanFloor(void)    { return helperChkHitscan(2); }
static char sprChkHitscanWall(void)     { return helperChkHitscan(3); }
static char sprChkHitscanSpr(void)      { return helperChkHitscan(4); }
static char sprChkHitscanMasked(void)   { return helperChkHitscan(5); }
static char sprChkIsTarget(void)
{
    int i;
    for (i = headspritestat[kStatDude]; i >= 0; i = nextspritestat[i])
    {
        if (pSpr->index == i)
            continue;

        spritetype* pDude = &sprite[i];
        if (IsDudeSprite(pDude) && xspriRangeIsFine(pDude->extra))
        {
            XSPRITE* pXDude = &xsprite[pDude->extra];
            if (pXDude->health <= 0 || pXDude->target != pSpr->index) continue;
            else if (PUSH) Push(EVOBJ_SPRITE, i);
            return true;
        }
    }
    return false;
}
/**---------------------------------------------------------------------------**/
static char sprCmpHealth(void)
{
    int t = 0;
    if (IsDudeSprite(pSpr))
        t = (pXSpr->sysData2 > 0) ? ClipHigh(pXSpr->sysData2 << 4, 65535) : getDudeInfo(pSpr->type)->startHealth << 4;
    else if (pSpr->type == kThingBloodChunks)
        return Cmp(0);
    else if (IsThingSprite(pSpr))
        t = thingInfo[pSpr->type - kThingBase].startHealth << 4;

    t = (kPercFull * pXSpr->health) / ClipLow(t, 1);
    return Cmp((!t && pXSpr->health) ? 1 : t);
}
/**---------------------------------------------------------------------------**/
static char sprChkTouchCeil(void)
{
    if ((gSpriteHit[pSpr->extra].ceilhit & 0xc000) != 0x4000) return false;
    else if (PUSH) Push(EVOBJ_SECTOR, gSpriteHit[pSpr->extra].ceilhit & 0x3fff);
    return true;
}
/**---------------------------------------------------------------------------**/
static char sprChkTouchFloor(void)
{
    if ((gSpriteHit[pSpr->extra].florhit & 0xc000) != 0x4000) return false;
    else if (PUSH) Push(EVOBJ_SECTOR, gSpriteHit[pSpr->extra].florhit & 0x3fff);
    return true;
}
/**---------------------------------------------------------------------------**/
static char sprChkTouchWall(void)
{
    if ((gSpriteHit[pSpr->extra].hit & 0xc000) != 0x8000) return false;
    else if (PUSH) Push(EVOBJ_WALL, gSpriteHit[pSpr->extra].hit & 0x3fff);
    return true;
}
/**---------------------------------------------------------------------------**/
static char sprChkTouchSpite(void)
{
    int id = -1;
    SPRITEHIT* pHit;

    if (xAvail)
    {
        pHit = &gSpriteHit[pSpr->extra];
        switch (arg3)
        {
        case 0:
        case 1:
            if ((pHit->florhit & 0xc000) == 0xc000) id = pHit->florhit & 0x3fff;
            if (arg3 || id >= 0) break;
            fallthrough__;
        case 2:
            if ((pHit->hit & 0xc000) == 0xc000) id = pHit->hit & 0x3fff;
            if (arg3 || id >= 0) break;
            fallthrough__;
        case 3:
            if ((pHit->ceilhit & 0xc000) == 0xc000) id = pHit->ceilhit & 0x3fff;
            break;
        }
    }

    // check if something touching this sprite
    if (id < 0 && sectRangeIsFine(pSpr->sectnum))
    {
        for (id = headspritesect[pSpr->sectnum]; id >= 0; id = nextspritesect[id])
        {
            if (sprite[id].extra <= 0)
                continue;

            pHit = &gSpriteHit[sprite[id].extra];
            if (arg3 == 1 || !arg3)
            {
                if ((pHit->ceilhit & 0xc000) == 0xc000 && (pHit->ceilhit & 0x3fff) == objIndex)
                    break;
            }

            if (arg3 == 2 || !arg3)
            {
                if ((pHit->hit & 0xc000) == 0xc000 && (pHit->hit & 0x3fff) == objIndex)
                    break;
            }

            if (arg3 == 3 || !arg3)
            {
                if ((pHit->florhit & 0xc000) == 0xc000 && (pHit->florhit & 0x3fff) == objIndex)
                    break;
            }
        }
    }

    if (id < 0) return false;
    else if (PUSH) Push(EVOBJ_SPRITE, id);
    return true;
}
/**---------------------------------------------------------------------------**/
static char sprCmpBurnTime(void)
{
    int t = (IsDudeSprite(pSpr)) ? 2400 : 1200;
    if (!Cmp((kPercFull * pXSpr->burnTime) / t)) return false;
    else if (PUSH && spriRangeIsFine(pXSpr->burnSource)) Push(EVOBJ_SPRITE, pXSpr->burnSource);
    return true;
}
/**---------------------------------------------------------------------------**/
static char sprChkIsFlareStuck(void)
{
    int i;
    for (i = headspritestat[kStatFlare]; i >= 0; i = nextspritestat[i])
    {
        spritetype* pFlare = &sprite[i];
        if (!xspriRangeIsFine(pFlare->extra) || (pFlare->flags & kHitagFree))
            continue;

        XSPRITE* pXFlare = &xsprite[pFlare->extra];
        if (pXFlare->target != objIndex) continue;
        else if (PUSH) Push(EVOBJ_SPRITE, i);
        return true;
    }

    return false;
}
static char sprCmpMass(void)        { return Cmp(getSpriteMassBySize(pSpr)); }
static char sprChkTarget(void)      { return helperChkSprite(pXSpr->target); }




/** GLOBAL DUDE functions
********************************************************************************/
static char gdudChkHaveTargets(void)
{
    if (!spriRangeIsFine(pXSpr->target)) return false;
    else if (!IsDudeSprite(&sprite[pXSpr->target]) && sprite[pXSpr->target].type != kMarkerPath) return false;
    else if (PUSH) Push(EVOBJ_SPRITE, pXSpr->target);
    return true;
};
static char gdudChkInAiFight(void)          { return aiFightDudeIsAffected(pXSpr); };
static char gdudChkTargetDistance(void)     { return helperChkTarget(1); };
static char gdudChkTargetCansee(void)       { return helperChkTarget(2); };
static char gdudChkTargetCanseePerip(void)  { return helperChkTarget(3); };
static char gdudChkFlagPatrol(void)         { return pXSpr->dudeFlag4; };
static char gdudChkFlagDeaf(void)           { return pXSpr->dudeDeaf; };
static char gdudChkFlagBlind(void)          { return pXSpr->dudeGuard; };
static char gdudChkFlagAlarm(void)          { return pXSpr->dudeAmbush; };
static char gdudChkFlagStealth(void)        { return ((pXSpr->unused1 & kDudeFlagStealth) != 0); };
static char gdudChkMarkerBusy(void)         { return helperChkMarker(1); };
static char gdudChkMarkerReached(void)      { return helperChkMarker(2); };
static char gdudCmpSpotProgress(void)
{
    if (!pXSpr->dudeFlag4 || !spriRangeIsFine(pXSpr->target) || sprite[pXSpr->target].type != kMarkerPath)	return Cmp(0);
    else if (!(pXSpr->unused1 & kDudeFlagStealth) || !valueIsBetween(pXSpr->data3, 0, kMaxPatrolSpotValue))	return Cmp(0);
    else return Cmp((kPercFull * pXSpr->data3) / kMaxPatrolSpotValue);
};
static char gdudChkLockout(void)            { return getDudeInfo(pSpr->type)->lockOut; };
static char gdudCmpAiStateType(void)        { return Cmp(pXSpr->aiState->stateType); };
static char gdudCmpAiStateTimer(void)       { return Cmp(pXSpr->stateTimer); };





/** CUSTOM DUDE functions
********************************************************************************/
static char cdudChkLeechThrown(void)
{
    CUSTOMDUDE* pDude = cdudeGet(pSpr);
    if (!pDude->IsLeechBroken() && pDude->pXLeech)
        return helperChkSprite(pDude->pXLeech->reference);

    return false;
};
static char cdudChkLeechDead(void)
{
    CUSTOMDUDE* pDude = cdudeGet(pSpr);
    if (pDude->IsLeechBroken()) return true;
    else if (PUSH && pDude->pXLeech) Push(EVOBJ_SPRITE, pDude->pXLeech->reference);
    return false;
};
static char cdudCmpSummoned(void)
{
    IDLIST* pSlaves = cdudeGet(pSpr)->slaves.list;
    if (!pSlaves)
        return Cmp(0);

    return Cmp(pSlaves->Length());
};
static char cdudChkIfAble(void)
{
    switch (arg3)
    {
        case 1: return false;
        case 2: return cdudeGet(pSpr)->CanBurn();
        case 3: return cdudeGet(pSpr)->CanCrouch();
        case 4: return cdudeGet(pSpr)->CanElectrocute();
        case 5: return false;
        case 6: return cdudeGet(pSpr)->CanRecoil();
        case 7: return cdudeGet(pSpr)->CanSwim();
        case 8: return cdudeGet(pSpr)->CanMove();
        default:
            Error(gErrors[kErrInvalidArgsPass]);
            break;
    }

    return false;
};
static char cdudCmpDispersion(void)
{
    CUSTOMDUDE_WEAPON* pWeapon = cdudeGet(pSpr)->pWeapon;
    if (!pWeapon)
        return Cmp(0);

    return Cmp(pWeapon->dispersion[0]);
};





/** PLAYER functions
********************************************************************************/
static char plyCmpConnected(void)
{
    if (!Cmp(pPlayer->nPlayer + 1)) return false;
    else return helperChkSprite(pPlayer->nSprite);
}
static char plyCmpTeam(void)                { return Cmp(pPlayer->teamId + 1); }
static char plyChkHaveKey(void)             { return (valueIsBetween(arg1, 0, 8) && pPlayer->hasKey[arg1 - 1]); }
static char plyChkHaveWeapon(void)          { return (valueIsBetween(arg1, 0, 15) && pPlayer->hasWeapon[arg1 - 1]); }
static char plyCmpCurWeapon(void)           { return Cmp(pPlayer->curWeapon); }
static char plyCmpPackItemAmount(void)      { return (valueIsBetween(arg1, 0, 6) && Cmp(pPlayer->packSlots[arg1 - 1].curAmount)); }
static char plyChkPackItemActive(void)      { return (valueIsBetween(arg1, 0, 6) && pPlayer->packSlots[arg1 - 1].isActive); }
static char plyCmpPackItemSelect(void)      { return Cmp(pPlayer->packItemId + 1); }
static char plyCmpPowerupAmount(void)
{
    int t;
    if (arg3 > 0 && arg3 <= (kMaxAllowedPowerup - (kMinAllowedPowerup << 1) + 1))
    {
        t = (kMinAllowedPowerup + arg3) - 1; // allowable powerups
        return Cmp(pPlayer->pwUpTime[t] / kPercFull);
    }

    Error("Unexpected powerup #%d", arg3);
    return false;
}
static char plyChkKillerSprite(void)    { return helperChkSprite(pPlayer->fraggerId); }
static char plyChkKeyPress(void)
{
    GINPUT* p = &gFifoInput[(gNetFifoTail-1) & 255][pPlayer->nPlayer];
    
    switch (arg1)
    {
        case 1:  return (p->forward > 0);                                       // forward
        case 2:  return (p->forward < 0);                                       // backward
        case 3:  return (p->strafe > 0);                                        // left
        case 4:  return (p->strafe < 0);                                        // right
        case 5:  return (p->buttonFlags.jump);                                  // jump
        case 6:  return (p->buttonFlags.crouch);                                // crouch
        case 7:  return (p->buttonFlags.shoot);                                 // normal fire weapon
        case 8:  return (p->buttonFlags.shoot2);                                // alt fire weapon
        case 9:  return (p->keyFlags.action);                                   // action
        case 10: return (p->q16turn < 0);                                       // turn left
        case 11: return (p->q16turn > 0);                                       // turn right
        case 12: return (p->q16mlook < 0 || p->buttonFlags.lookDown);           // look down
        case 13: return (p->q16mlook > 0 || p->buttonFlags.lookUp);             // look up
        case 14: return (p->keyFlags.lookCenter);                               // look center
        case 15: return (p->keyFlags.holsterWeapon);                            // holster
        case 16:                                                                // sel weapon
            if (arg2 > 0)
                return (p->newWeapon == arg2);                                  // certain
            return (p->newWeapon > 0);                                          // any
        case 17: return (p->keyFlags.nextWeapon);                               // next
        case 18: return (p->keyFlags.prevWeapon);                               // prev
        case 19:                                                                // use item
            switch (arg2)
            {
                case 0: return (p->keyFlags.useItem);                           // any
                case 1: return (p->useFlags.useBeastVision);                    // beast vision
                case 2: return (p->useFlags.useCrystalBall);                    // crystal ball
                case 3: return (p->useFlags.useJumpBoots);                      // jump boots
                case 4: return (p->useFlags.useMedKit);                         // doctor's bag
                default:
                    Error(gErrors[kErrInvalidArgsPass]);
                    break;
            }
            return false;
        case 20: return (p->keyFlags.nextItem);                                 // next
        case 21: return (p->keyFlags.prevItem);                                 // prev
        case 22: return (p->keyFlags.pause);                                    // pause
        case 23: return (p->keyFlags.restart);                                  // restart
        case 24: return (p->keyFlags.quit);                                     // quit
        default:
            Error("Specify a correct key!");
            return false;
    }
}
static char plyChkRunning(void)             { return pPlayer->isRunning; }
static char plyChkFalling(void)             { return pPlayer->fallScream; }
static char plyCmpLifeMode(void)            { return Cmp(pPlayer->lifeMode + 1); }
static char plyCmpPosture(void)             { return Cmp(pPlayer->posture + 1); }
static char plyCmpKillsCount(void)          { return Cmp(pPlayer->fragCount); }
static char plyChkAutoAimTarget(void)       { return helperChkSprite(pPlayer->aimTarget); }
static char plyChkVoodooTarget(void)        { return helperChkSprite(pPlayer->voodooTarget); }

static char plyCmpQavWeapon(void)           { return Cmp(pPlayer->weaponQav); }
static char plyCmpQavScene(void)            { return Cmp(pPlayer->sceneQav); }
static char plyChkGodMode(void)             { return (pPlayer->godMode || powerupCheck(pPlayer, kPwUpDeathMask)); }
static char plyChkShrink(void)              { return isShrinked(pSpr); }
static char plyChkGrown(void)               { return isGrown(pSpr); }
static char plyCmpArmor(void)
{
    if (valueIsBetween(arg3, 0, 4))
        return Cmp((pPlayer->armor[arg3 - 1] * kPercFull) / 1600);

    Error(gErrors[kErrInvalidArgsPass]);
    return false;
}
static char plyCmpAmmo(void)
{
    if (valueIsBetween(arg3, 0, 12))
        return Cmp(pPlayer->ammoCount[arg3 - 1]);

    Error(gErrors[kErrInvalidArgsPass]);
    return false;
}
static char plyChkSpriteItOwns(void)
{ 
    int i;
    if (rngok(arg3, 0, kMaxStatus + 1))
    {
        for (i = headspritestat[arg3]; i >= 0; i = nextspritestat[i])
        {
            if (!Cmp(sprite[i].type) || actSpriteOwnerToSpriteId(&sprite[i]) != pPlayer->nSprite) continue;
            else if (PUSH) Push(EVOBJ_SPRITE, i);
            return true;
        }

        return false;
    }
    
    Error(gErrors[kErrInvalidArgsPass]);
    return false;
}


/** TABLE OF CONDITION FUNCTIONS
********************************************************************************/
static CONDITION_INFO gConditionsList[] =
{
    { gamCmpLevelMin,			1,		CGAM,		false,		false },	// compare level minutes
    { gamCmpLevelSec,			2,		CGAM,		false,		false },	// compare level seconds
    { gamCmpLevelMsec,			3,		CGAM,		false,		false },	// compare level mseconds
    { gamCmpLevelTime,			4,		CGAM,		false,		false },	// compare level time (unsafe)
    { gamCmpKillsTotal,			5,		CGAM,		false,		false },	// compare current global kills counter
    { gamCmpKillsDone,			6,		CGAM,		false,		false },	// compare total global kills counter
    { gamCmpSecretsDone,		7,		CGAM,		false,		false },	// compare how many secrets found
    { gamCmpSecretsTotal,		8,		CGAM,		false,		false },	// compare total secrets
    { gamCmpVisibility,			20,		CGAM,		false,		false },	// compare global visibility value
    { gamChkGotpic,				21,		CGAM,		true,		false },	// check gotpic
    { gamChkChance,				30,		CGAM,		true,		false },	// check chance %
    { gamCmpRandom,				31,		CGAM,		false,		false },	// compare random
    { gamCmpStatnumCount,		47,		CGAM,		false,		false },	// compare counter of specific statnum sprites
    { gamCmpNumsprites,			48,		CGAM,		false,		false },	// compare counter of total sprites
    { gamChkSector,		        57,		CGAM,		true,		false },	// get sector N if possible
    { gamChkWall,		        58,		CGAM,		true,		false },	// get wall N if possible
    { gamChkSprite,		        59,		CGAM,		true,		false },	// get sprite N if possible
    { gamChkPlayerConnect,		60,		CGAM,		true,		false },	// check if player N connected
    /**--------------------------------------------------------------**/
    { mixChkObjSect,			100,	CMIX,		true,		false },
    { mixChkObjWall,			105,	CMIX,		true,		false },
    { mixChkObjSpr,				110,	CMIX,		true,		false },
    { mixChkXRange,				115,	CMIX,		true,		false },
    { mixCmpLotag,				120,	CMIX,		false,		false },
    { mixCmpPicSurface,			124,	CMIX,		false,		false },
    { mixCmpPic,				125,	CMIX,		false,		false },
    { mixCmpPal,				126,	CMIX,		false,		false },
    { mixCmpShade,				127,	CMIX,		false,		false },
    { mixCmpCstat,				128,	CMIX,		false,		false },
    { mixCmpHitag,				129,	CMIX,		false,		false },
    { mixCmpXrepeat,			130,	CMIX,		false,		false },
    { mixCmpXoffset,			131,	CMIX,		false,		false },
    { mixCmpYrepeat,			132,	CMIX,		false,		false },
    { mixCmpYoffset,			133,	CMIX,		false,		false },
    { mixCmpData1,				141,	CMIX,		false,		true  },
    { mixCmpData2,				142,	CMIX,		false,		true  },
    { mixCmpData3,				143,	CMIX,		false,		true  },
    { mixCmpData4,				144,	CMIX,		false,		true  },
    { mixCmpRXId,				150,	CMIX,		false,		true  },
    { mixCmpTXId,				151,	CMIX,		false,		true  },
    { mixChkLock,				152,	CMIX,		true,		true  },
    { mixChkTriggerOn,			153,	CMIX,		true,		true  },
    { mixChkTriggerOff,			154,	CMIX,		true,		true  },
    { mixChkTriggerOnce,		155,	CMIX,		true,		true  },
    { mixChkIsTriggered,		156,	CMIX,		true,		true  },
    { mixChkState,				157,	CMIX,		true,		true  },
    { mixCmpBusy,				158,	CMIX,		false,		true  },
    { mixChkPlayerOnly,			159,	CMIX,		true,		true  },
    { mixCmpSeqID,				170,	CMIX,		false,		true  },
    { mixCmpSeqFrame,			171,	CMIX,		false,		true  },
    { mixCmpObjIndex,			195,	CMIX,		false,		false },
    { mixCmpObjXIndex,			196,	CMIX,		false,		false },
    { mixCmpSerials,		    197,	CMIX,		false,		false },
    { mixChkEventCauser,		198,	CMIX,		true,		false },	// check event causer
    { mixCmpEventCmd,			199,	CMIX,		false,		false },	// this condition received N command?
    /**--------------------------------------------------------------**/
    { wallCmpOverpicnum,		200,	CWAL,		false,		false },
    { wallChkShow2dWall,		201,	CWAL,		true,		false },	// wall on the minimap
    { wallChkSector,			205,	CWAL,		true,		false },
    { wallChkIsMirror,			210,	CWAL,		true,		false },
    { wallChkNextSector,		215,	CWAL,		true,		false },
    { wallChkNextWall,			220,	CWAL,		true,		false },
    { wallChkPoint2,			221,	CWAL,		true,		false },
    { wallChkNextWallSector,	225,	CWAL,		true,		false },
    /**--------------------------------------------------------------**/
    { sectCmpVisibility,		300,	CSEC,		false,		false },	// compare visibility
    { sectChkShow2dSector,		301,	CSEC,		true,		false },	// sector on the minimap
    { sectChkGotSector,			302,	CSEC,		true,		false },	// sector on the screen
    { sectCmpFloorSlope,		305,	CSEC,		false,		false },	// compare floor slope
    { sectCmpCeilSlope,			306,	CSEC,		false,		false },	// compare ceil slope
    { sectChkSprTypeInSect,		310,	CSEC,		true,		false },	// check is sprite with lotag N in sector
    { sectChkUnderwater,		350,	CSEC,		true,		true  },	// sector is underwater?
    { sectCmpDepth,				351,	CSEC,		false,		true  },	// compare depth level
    { sectCmpFloorHeight,		355,	CSEC,		false,		true  },	// compare floor height (in %)
    { sectCmpCeilHeight,		356,	CSEC,		false,		true  },	// compare ceil height (in %)
    { sectChkPaused,			357,	CSEC,		true,		true  },	// this sector in movement?
    { sectChkUpperRoom,			358,	CSEC,		true,		false },	// get upper room sector or marker
    { sectChkLowerRoom,			359,	CSEC,		true,		false },	// get lower room sector or marker
    /**--------------------------------------------------------------**/
    { plyCmpConnected,			400,	CPLY,		false,		false },
    { plyCmpTeam,				401,	CPLY,		false,		false },
    { plyChkHaveKey,			402,	CPLY,		true,		false },
    { plyChkHaveWeapon,			403,	CPLY,		true,		false },
    { plyCmpCurWeapon,			404,	CPLY,		false,		false },
    { plyCmpPackItemAmount,		405,	CPLY,		false,		false },
    { plyChkPackItemActive,		406,	CPLY,		true,		false },
    { plyCmpPackItemSelect,		407,	CPLY,		false,		false },
    { plyCmpPowerupAmount,		408,	CPLY,		false,		false },
    { plyChkKillerSprite,		409,	CPLY,		true,		false },
    { plyChkKeyPress,			410,	CPLY,		true,		false },	// check keys pressed
    { plyChkRunning,			411,	CPLY,		true,		false },
    { plyChkFalling,			412,	CPLY,		true,		false },
    { plyCmpLifeMode,			413,	CPLY,		false,		false },
    { plyCmpPosture,			414,	CPLY,		false,		false },
    { plyChkSpriteItOwns,	    419,	CPLY,		true,		false },
    { plyCmpArmor,  			420,	CPLY,		false,		false },    // in %
    { plyCmpAmmo,  			    421,	CPLY,		false,		false },
    { plyCmpKillsCount,			430,	CPLY,		false,		false },
    { plyChkAutoAimTarget,		431,	CPLY,		true,		false },
    { plyChkVoodooTarget,		435,	CPLY,		true,		false },
    { plyCmpQavWeapon,			445,	CPLY,		false,		false },
    { plyCmpQavScene,			446,	CPLY,		false,		false },
    { plyChkGodMode,			447,	CPLY,		true,		false },
    { plyChkShrink,				448,	CPLY,		true,		false },
    { plyChkGrown,				449,	CPLY,		true,		false },
    /**--------------------------------------------------------------**/
    { gdudChkHaveTargets,		450,	CDUDG,		true,		true  },	// dude have any targets?
    { gdudChkInAiFight,			451,	CDUDG,		true,		true  },	// dude affected by ai fight?
    { gdudChkTargetDistance,	452,	CDUDG,		true,		true  },	// distance to the target in a range?
    { gdudChkTargetCansee,		453,	CDUDG,		true,		true  },	// is the target visible?
    { gdudChkTargetCanseePerip,	454,	CDUDG,		true,		true  },	// is the target visible with periphery?
    { gdudChkFlagPatrol,		455,	CDUDG,		true,		true  },
    { gdudChkFlagDeaf,			456,	CDUDG,		true,		true  },
    { gdudChkFlagBlind,			457,	CDUDG,		true,		true  },
    { gdudChkFlagAlarm,			458,	CDUDG,		true,		true  },
    { gdudChkFlagStealth,		459,	CDUDG,		true,		true  },
    { gdudChkMarkerBusy,		460,	CDUDG,		true,		true  },	// check if the marker is busy with another dude
    { gdudChkMarkerReached,		461,	CDUDG,		true,		true  },	// check if the marker is reached
    { gdudCmpSpotProgress,		462,	CDUDG,		false,		true  },	// compare spot progress value in %
    { gdudChkLockout,			465,	CDUDG,		true,		true  },	// dude allowed to interact with objects?
    { gdudCmpAiStateType,		466,	CDUDG,		false,		true  },
    { gdudCmpAiStateTimer,		467,	CDUDG,		false,		true  },
    /**--------------------------------------------------------------**/
    { cdudChkLeechThrown,		470,	CDUDC,		true,		false },	// life leech is thrown?
    { cdudChkLeechDead,			471,	CDUDC,		true,		false },	// life leech is destroyed?
    { cdudCmpSummoned,			472,	CDUDC,		false,		false },	// are required amount of dudes is summoned?
    { cdudChkIfAble,			473,	CDUDC,		true,		false },	// check if dude can...
    { cdudCmpDispersion,		474,	CDUDC,		false,		false },	// compare weapon dispersion
    /**--------------------------------------------------------------**/
    { sprCmpAng,				500,	CSPR,		false,		false },	// compare angle
    { sprChkShow2dSprite,		501,	CSEC,		true,		false },	// sprite on the minimap
    { sprCmpStatnum,			505,	CSPR,		false,		false },	// check statnum
    { sprChkRespawn,			506,	CSPR,		true,		false },	// check if on respawn list
    { sprCmpSlope,				507,	CSPR,		false,		false },	// compare slope
    { sprCmpClipdist,			510,	CSPR,		false,		false },	// compare clipdist
    { sprChkOwner,				515,	CSPR,		true,		false },	// check owner sprite
    { sprChkSector,				520,	CSPR,		true,		false },	// stays in a sector?
    { sprCmpChkVelocity,		525,	CSPR,		true,		false },	// check or compare velocity
    { sprCmpVelocityNew,		526,	CSPR,		false,		false },	// compare velocity
    { sprChkUnderwater,			530,	CSPR,		true,		false },	// sector of sprite is underwater?
    { sprChkDmgImmune,			531,	CSPR,		true,		false },	// check if immune to N dmgType
    { sprChkHitscanCeil,		535,	CSPR,		true,		false },	// hitscan: ceil?
    { sprChkHitscanFloor,		536,	CSPR,		true,		false },	// hitscan: floor?
    { sprChkHitscanWall,		537,	CSPR,		true,		false },	// hitscan: wall?
    { sprChkHitscanSpr,			538,	CSPR,		true,		false },	// hitscan: sprite?
    { sprChkHitscanMasked,		539,	CSPR,		true,		false },	// hitscan: masked wall?
    { sprChkIsTarget,			545,	CSPR,		true,		false },	// this sprite is a target of some dude?
    { sprCmpHealth,				550,	CSPR,		false,		true  },	// compare hp (in %)
    { sprChkTouchCeil,			555,	CSPR,		true,		true  },	// touching ceil of sector?
    { sprChkTouchFloor,			556,	CSPR,		true,		true  },	// touching floor of sector?
    { sprChkTouchWall,			557,	CSPR,		true,		true  },	// touching walls of sector?
    { sprChkTouchSpite,			558,	CSPR,		true,		false },	// touching another sprite? (allow no xAvail!)
    { sprCmpBurnTime,			565,	CSPR,		false,		true  },	// compare burn time (in %)
    { sprChkIsFlareStuck,		566,	CSPR,		true,		false },	// any flares stuck in this sprite?
    { sprChkTarget,		        569,	CSPR,		true,		true },	    // use with caution!
    { sprCmpMass,				570,	CSPR,		false,		true  },	// mass of the sprite in a range?
};



/** CONDITION INTERFACE FUNCTIONS
********************************************************************************/
static char DefaultResult()     { return (pEntry->isBool) ? false : Cmp(0); }
static char CheckGeneric()      { return pEntry->pFunc(); }
static char CheckSector()
{
    if (objType == EVOBJ_SECTOR && rngok(objIndex, 0, numsectors))
    {
        pSect = &sector[objIndex];
        if (pSect->extra > 0)
        {
            pXSect = &xsector[pSect->extra];
            xAvail = true;
        }
        else
        {
            pXSect = NULL;
            xAvail = false;
            if (pEntry->xReq)
                return DefaultResult();
        }
    }
    else
    {
        Error(gErrors[kErrInvalidObject], objIndex, objType, "sector");
        return false;
    }

    return pEntry->pFunc();
}

static char CheckWall()
{
    if (objType == EVOBJ_WALL && rngok(objIndex, 0, numwalls))
    {
        pWall = &wall[objIndex];
        if (pWall->extra > 0)
        {
            pXWall = &xwall[pWall->extra];
            xAvail = true;
        }
        else
        {
            pXWall = NULL;
            xAvail = false;
            if (pEntry->xReq)
                return DefaultResult();
        }
    }
    else
    {
        Error(gErrors[kErrInvalidObject], objIndex, objType, "wall");
        return false;
    }

    return pEntry->pFunc();
}

static char CheckDude()
{
    if (objType == EVOBJ_SPRITE && rngok(objIndex, 0, kMaxSprites))
    {
        pSpr = &sprite[objIndex];
        if ((!IsDudeSprite(pSpr) && pSpr->type != kThingBloodChunks) || IsPlayerSprite(pSpr))
        {
            Error(gErrors[kErrInvalidObject], objIndex, objType, "dude");
            return false;
        }

        if (pSpr->extra > 0)
        {
            pXSpr = &xsprite[pSpr->extra];
            xAvail = true;
        }
        else
        {
            pXSpr = NULL;
            xAvail = false;
            if (pEntry->xReq)
                return DefaultResult();
        }
    }
    else
    {
        Error(gErrors[kErrInvalidObject], objIndex, objType, "sprite");
        return false;
    }

    return pEntry->pFunc();
}

static char CheckCustomDude()
{
    if (objType == EVOBJ_SPRITE && rngok(objIndex, 0, kMaxSprites))
    {
        pSpr = &sprite[objIndex];
        switch (pSpr->type) {
            case kThingBloodChunks:
                if (pSpr->inittype == kDudeModernCustom) break;
                Error(gErrors[kErrInvalidObject], objIndex, objType, "custom dude");
                return false;
            case kDudeModernCustom:
                break;
            default:
                Error(gErrors[kErrInvalidObject], objIndex, objType, "custom dude");
                return false;
        }

        if (pSpr->extra > 0)
        {
            pXSpr = &xsprite[pSpr->extra];
            xAvail = true;
        }
        else
        {
            pXSpr = NULL;
            xAvail = false;
            if (pEntry->xReq)
                return DefaultResult();
        }
    }
    else
    {
        Error(gErrors[kErrInvalidObject], objIndex, objType, "sprite");
        return false;
    }

    return pEntry->pFunc();
}

static char CheckPlayer()
{
    int i;
    if (objType == EVOBJ_SPRITE && rngok(objIndex, 0, kMaxSprites))
    {
        pSpr = &sprite[objIndex];
        for (i = connecthead; i >= 0; i = connectpoint2[i])
        {
            if (objIndex != gPlayer[i].nSprite) continue;
            pPlayer = &gPlayer[i];
            break;
        }

        // there is no point to check unlinked or disconnected players
        if (i < 0)
            return DefaultResult();

        if (pSpr->extra > 0)
        {
            pXSpr = &xsprite[pSpr->extra];
            xAvail = true;
        }
        else
        {
            pXSpr = NULL;
            xAvail = false;
            if (pEntry->xReq)
                return DefaultResult();
        }
    }
    else
    {
        Error(gErrors[kErrInvalidObject], objIndex, objType, "player");
        return false;
    }

    return pEntry->pFunc();
}

static char CheckSprite()
{
    if (objType == EVOBJ_SPRITE && rngok(objIndex, 0, kMaxSprites))
    {
        pSpr = &sprite[objIndex];
        if (pSpr->extra > 0)
        {
            pXSpr = &xsprite[pSpr->extra];
            xAvail = true;
        }
        else
        {
            pXSpr = NULL;
            xAvail = false;
            if (pEntry->xReq)
                return DefaultResult();
        }
    }
    else
    {
        Error(gErrors[kErrInvalidObject], objIndex, objType, "sprite");
        return false;
    }

    return pEntry->pFunc();
}

static char CheckObject()
{
    switch (objType)
    {
        case EVOBJ_WALL:    return CheckWall();
        case EVOBJ_SECTOR:  return CheckSector();
        case EVOBJ_SPRITE:  return CheckSprite();
    }

    // conditions can only work with objects in the switch anyway...
    Error(gErrors[kErrUnknownObject], objType, objIndex);
    return false;
}

static void Push(int oType, int oIndex)
{
    // focus on object
    pXCond->targetX = Serialize(oType, oIndex);
    if (pXCond->command > kCmdPush)
    {
        // copy in additional slots
        switch (pXCond->command - kCmdPush)
        {
            case 1:
                pXCond->targetZ = pXCond->targetX;
                break;
            case 2:
                pXCond->sysData1 = pXCond->targetX;
                break;
        }
    }
}

static void Restore()
{
    int t;
    switch (pXCond->command - kCmdPop)
    {
        case 0:
            pXCond->targetX = pXCond->targetY;
            break;
        // additional slots leads to swapping
        // so object in the focus is not lost
        case 1:
            t = pXCond->targetZ;
            pXCond->targetX = pXCond->targetZ;
            pXCond->targetZ = t;
            break;
        case 2:
            t = pXCond->targetX;
            pXCond->targetX  = pXCond->sysData1;
            pXCond->sysData1 = t;
            break;
    }
}

static int Serialize(int oType, int oIndex)
{
    switch (oType)
    {
        case EVOBJ_SECTOR:      return kSerialSector + oIndex;
        case EVOBJ_WALL:        return kSerialWall + oIndex;
        case EVOBJ_SPRITE:      return kSerialSprite + oIndex;
    }

    Error(gErrors[kErrUnknownObject], oType, oIndex);
    return -1;
}

static void Unserialize(int nSerial, int* oType, int* oIndex)
{
    if (rngok(nSerial, kSerialSector, kSerialWall))
    {
        *oIndex = nSerial - kSerialSector;
        *oType = EVOBJ_SECTOR;
    }
    else if (rngok(nSerial, kSerialWall, kSerialSprite))
    {
        *oIndex = nSerial - kSerialWall;
        *oType = EVOBJ_WALL;
    }
    else if (rngok(nSerial, kSerialSprite, kSerialMax))
    {
        *oIndex = nSerial - kSerialSprite;
        *oType = EVOBJ_SPRITE;
    }
    else
    {
        Error(gErrors[kErrInvalidSerial], nSerial);
    }
}

static char Cmp(int val)
{
    if (cmpOp & 0x2000)
        return (cmpOp & CSTAT_SPRITE_BLOCK) ? (val > arg1) : (val >= arg1); // blue sprite
    else if (cmpOp & 0x4000)
        return (cmpOp & CSTAT_SPRITE_BLOCK) ? (val < arg1) : (val <= arg1); // green sprite
    else if (cmpOp & CSTAT_SPRITE_BLOCK)
        return (val >= arg1 && val <= arg2);
    else
        return (val == arg1);
}

static char Cmp(int val, int nArg1, int nArg2)
{
    arg1 = nArg1;
    arg2 = nArg2;

    return Cmp(val);
}

static void Error(const char* pFormat, ...)
{
    char buffer[1024], buffer2[512], condType[32];
    strcpy(condType, (pEntry) ? gCheckFuncInfo[pEntry->type].name : "Unknown");
    Bstrupr(condType);

    va_list args;
    va_start(args, pFormat);
    vsprintf(buffer2, pFormat, args);
    va_end(args);

    Bsprintf(buffer,
        "\n"
        "ERROR IN %s CONDITION ID #%d:\n"
        "%s\n\n"
        "Debug information:\n"
        "--------------------------------------------\n"
        "Condition sprite  =  %d,  RX ID  =  %d,  TX ID  =  %d\n"
        "Arguments  =  %d,  %d,  %d\n"
        "Operator  =  %d\n",
        condType, pXCond->data1, buffer2,
        pXCond->reference,
        pXCond->rxID,
        pXCond->txID,
        arg1,
        arg2,
        arg3,
        cmpOp &= ~CSTAT_SPRITE_INVISIBLE
    );

    ThrowError(buffer);

}

static void ReceiveObjects(EVENT* pFrom)
{
    char srcIsCondition = false;

    objType = pFrom->type; objIndex = pFrom->index;
    if (objType == EVOBJ_SPRITE && pCond->index != objIndex)
        srcIsCondition = (sprite[objIndex].statnum == kStatModernCondition);

    if (!srcIsCondition)
    {
        // save object serials in the "stack"
        pXCond->targetX = Serialize(objType, objIndex);
        pXCond->targetY = pXCond->targetX;
        pXCond->targetZ = pXCond->targetX;
        pXCond->sysData1 = pXCond->targetX;
    }
    else
    {
        // or grab serials of objects from previous conditions
        pXCond->targetX = xsprite[sprite[objIndex].extra].targetX;
        pXCond->targetY = xsprite[sprite[objIndex].extra].targetY;
        pXCond->targetZ = xsprite[sprite[objIndex].extra].targetZ;
        pXCond->sysData1 = xsprite[sprite[objIndex].extra].sysData1;
    }
}

static void TriggerObject(int nSerial)
{
    int oType, oIndex;
    Unserialize(nSerial, &oType, &oIndex);
    nnExtTriggerObject(oType, oIndex, pXCond->command, pCond->index);
}



/** EXTERNAL CODE
********************************************************************************/
void conditionsInit(bool bSaveLoad)
{
    unsigned int i, j;

    if (!gNumConditions)
    {
        if (gConditions)
        {
            Bfree(gConditions);
            gConditions = NULL;
        }

        // sort out *condition function callers* list the right way
        qsort(gCheckFuncInfo, LENGTH(gCheckFuncInfo), sizeof(gCheckFuncInfo[0]), (int(*)(const void*, const void*))qsSortCheckFuncInfo);

        for (i = 0; i < LENGTH(gConditionsList); i++)
        {
            CONDITION_INFO* pTemp = &gConditionsList[i];

            // check for duplicates
            for (j = 0; j < LENGTH(gConditionsList); j++)
            {
                if (i != j)
                    dassert(pTemp->id != gConditionsList[j].id);
            }

            // find the highest condition id
            if (pTemp->id > gNumConditions)
                gNumConditions = pTemp->id;
        }

        // allocate the list
        gNumConditions++;
        gConditions = (CONDITION_INFO*)Bmalloc(sizeof(CONDITION_INFO) * gNumConditions);
        dassert(gConditions != NULL);

        // dummy template for everything
        for (i = 0; i < gNumConditions; i++)
        {
            CONDITION_INFO* pTemp = &gConditions[i];
            pTemp->pFunc = errCondNotImplemented; pTemp->type = CNON;
            pTemp->pCaller = &gCheckFuncInfo[pTemp->type];
            pTemp->isBool = false;
            pTemp->xReq = false;
            pTemp->id = i;
        }

        for (i = 0; i < LENGTH(gConditionsList); i++)
        {
            CONDITION_INFO* pTemp = &gConditionsList[i];

            // proper caller info for each function
            for (j = 0; j < LENGTH(gCheckFuncInfo); j++)
            {
                if (gCheckFuncInfo[j].type != pTemp->type) continue;
                pTemp->pCaller = &gCheckFuncInfo[j];
                break;
            }

            dassert(j < LENGTH(gCheckFuncInfo));

            // sort out condition list the right way
            memcpy(&gConditions[pTemp->id], pTemp, sizeof(gConditionsList[0]));
        }
    }

    // allocate tracking conditions and collect objects for it
    conditionsTrackingAlloc(bSaveLoad);
}

void conditionsTrackingAlloc(bool bSaveLoad)
{
    int nCount = 0, i, j, s, e;

    conditionsTrackingClear();
    for (i = headspritestat[kStatModernCondition]; i >= 0; i = nextspritestat[i])
    {
        spritetype* pSprite = &sprite[i]; XSPRITE* pXSprite = &xsprite[pSprite->extra];
        if (pSprite->extra <= 0 || !pXSprite->busyTime || pXSprite->isTriggered)
            continue;

        gTrackingConditionsList = (TRACKING_CONDITION*)Brealloc(gTrackingConditionsList, (nCount + 1) * sizeof(TRACKING_CONDITION));
        if (!gTrackingConditionsList)
            break;

        TRACKING_CONDITION* pTr = &gTrackingConditionsList[nCount];
        pTr->objects = new(OBJECT_LIST);

        if (pXSprite->rxID >= kChannelUser)
        {
            for (j = 0; j < numsectors; j++)
            {
                for (s = headspritesect[j]; s >= 0; s = nextspritesect[s])
                {
                    spritetype* pObj = &sprite[s];
                    if (pObj->extra <= 0 || pObj->index == pSprite->index)
                        continue;

                    XSPRITE* pXObj = &xsprite[pObj->extra];
                    if (pXObj->txID != pXSprite->rxID)
                        continue;

                    // check exceptions
                    switch (pObj->type)
                    {
                        case kModernCondition:
                        case kModernConditionFalse:
                        case kSwitchToggle:
                        case kSwitchOneWay:
                            break;
                        case kModernPlayerControl:
                            if (pObj->statnum == kStatModernPlayerLinker && bSaveLoad)
                            {
                                // assign player sprite after savegame loading
                                pTr->objects->Add(EVOBJ_SPRITE, pXObj->sysData1);
                                break;
                            }
                            fallthrough__;
                        default:
                            pTr->objects->Add(EVOBJ_SPRITE, pObj->index);
                            break;
                    }
                }

                s = sector[j].wallptr; e = s + sector[j].wallnum - 1;
                while (s <= e)
                {
                    walltype* pObj = &wall[s];
                    if (pObj->extra > 0 && xwall[pObj->extra].txID == pXSprite->rxID)
                    {
                        // check exceptions
                        switch (pObj->type)
                        {
                            case kSwitchToggle:
                            case kSwitchOneWay:
                                break;
                            default:
                                pTr->objects->Add(EVOBJ_WALL, s);
                                break;
                        }
                    }

                    s++;
                }

                sectortype* pObj = &sector[j];
                if (pObj->extra > 0 && xsector[pObj->extra].txID == pXSprite->rxID)
                    pTr->objects->Add(EVOBJ_SECTOR, j);
            }
        }

        // allow self tracking
        if (!pTr->objects->Length())
            pTr->objects->Add(EVOBJ_SPRITE, pSprite->index);

        pTr->id = pSprite->extra;
        nCount++;
    }

    if (!nCount)
        conditionsTrackingClear();

    gTrackingConditionsListLength = nCount;
}

void conditionsTrackingClear()
{
    int i = gTrackingConditionsListLength;
    if (gTrackingConditionsList)
    {
        while (--i >= 0)
        {
            TRACKING_CONDITION* pTr = &gTrackingConditionsList[i];
            if (pTr->objects)
            {
                delete(pTr->objects);
                pTr->objects = NULL;
            }
        }

        Bfree(gTrackingConditionsList);
    }

    gTrackingConditionsList = NULL;
    gTrackingConditionsListLength = 0;

}

void conditionsTrackingProcess()
{
    EVENT evn; OBJECT* o;
    evn.funcID = kCallbackMax; evn.cmd = kCmdOn; evn.causer = kCauserGame;
    TRACKING_CONDITION* pTr;
    XSPRITE* pXSpr;
    
    int i = gTrackingConditionsListLength;
    int t;

    while (--i >= 0)
    {
        pTr = &gTrackingConditionsList[i];  pXSpr = &xsprite[pTr->id];
        if (pXSpr->locked || pXSpr->isTriggered || ++pXSpr->busy < pXSpr->busyTime)
            continue;

        pXSpr->busy = 0;
        t = pTr->objects->Length(); o = pTr->objects->Ptr();
        while (--t >= 0)
        {
            evn.type = o->type; evn.index = o->index;
            useCondition(&sprite[pXSpr->reference], pXSpr, &evn);
            o++;
        }
    }
}

void conditionsLinkPlayer(XSPRITE* pXCtrl, PLAYER* pPlay)
{
    int i = gTrackingConditionsListLength;
    int t;
    OBJECT* o;

    // search for player control sprite and replace it with actual player sprite
    while (--i >= 0)
    {
        TRACKING_CONDITION* pTr = &gTrackingConditionsList[i];
        XSPRITE* pXTrack = &xsprite[pTr->id];
        if (pXTrack->rxID != pXCtrl->txID) continue;
        if ((t = pTr->objects->Length()) <= 0)
            continue;

        o = pTr->objects->Ptr();
        while (--t >= 0)
        {
            if (o->type == OBJ_SPRITE && o->index == pXCtrl->reference)
            {
                o->index = pPlay->nSprite;
                break;
            }

            o++;
        }
    }
}

// this updates index of object in all conditions
void conditionsUpdateIndex(int oType, int oldIndex, int newIndex)
{
    int i = gTrackingConditionsListLength;
    int t;
    OBJECT* o;

    // update index in tracking conditions first
    while (--i >= 0)
    {
        TRACKING_CONDITION* pTr = &gTrackingConditionsList[i];
        if ((t = pTr->objects->Length()) <= 0)
            continue;

        o = pTr->objects->Ptr();
        while (--t >= 0)
        {
            if (o->type == oType && o->index == oldIndex)
            {
                o->index = newIndex;
                break;
            }

            o++;
        }
    }


    int oldSerial = Serialize(oType, oldIndex);
    int newSerial = Serialize(oType, newIndex);

    // then update serials everywhere
    for (i = headspritestat[kStatModernCondition]; i >= 0; i = nextspritestat[i])
    {
        XSPRITE* pXSpr = &xsprite[sprite[i].extra];
        if (pXSpr->targetX == oldSerial)        pXSpr->targetX = newSerial;
        if (pXSpr->targetY == oldSerial)        pXSpr->targetY = newSerial;
        if (pXSpr->targetZ == oldSerial)        pXSpr->targetZ = newSerial;
        if (pXSpr->sysData1 == oldSerial)       pXSpr->sysData1 = newSerial;
    }


    return;
}

// for showing errors in external code
void conditionsError(XSPRITE* pXSprite, const char* pFormat, ...)
{
    char buffer[512];

    pCond = &sprite[pXSprite->reference]; pXCond = pXSprite;
    arg1 = pXCond->data2, arg2 = pXCond->data3, arg3 = pXCond->data4;
    cmpOp = pCond->cstat;
    pEntry = NULL;

    va_list args;
    va_start(args, pFormat);
    vsprintf(buffer, pFormat, args);
    va_end(args);

    Error(buffer);
}


void useCondition(spritetype* pSource, XSPRITE* pXSource, EVENT* pEvn)
{
    // if it's a tracking condition, it must ignore all the commands sent from objects
    if (pXSource->busyTime && pEvn->funcID != kCallbackMax)
        return;

    bool ok = false;
    bool delayBefore = false, flag8 = false;
    pCond = pSource; pXCond = pXSource;
    pEntry = NULL;

    if (pXCond->waitTime)
    {
        delayBefore     = (pCond->flags & kModernTypeFlag64);
        flag8           = (pCond->flags & kModernTypeFlag8);

        if (pXCond->restState == 0)
        {
            if (delayBefore)
            {
                // start waiting
                evPost(pCond->index, EVOBJ_SPRITE, EVTIME2TICKS(pXCond->waitTime), (COMMAND_ID)kCmdRepeat, pEvn->causer);
                pXCond->restState = 1;

                if (flag8)
                    ReceiveObjects(pEvn); // receive current objects and hold it

                return;
            }
            else
            {
                ReceiveObjects(pEvn); // receive current objects and continue
            }
        }
        else if (pEvn->cmd == kCmdRepeat)
        {
            // finished the waiting
            if (delayBefore)
                pXCond->restState = 0;
        }
        else
        {
            if ((delayBefore && !flag8) || (!delayBefore && flag8))
                ReceiveObjects(pEvn); // continue receiving actual objects while waiting

            return;
        }
    }
    else
    {
        ReceiveObjects(pEvn); // receive current objects and continue
    }

    if (pXCond->restState == 0)
    {
        if (!pXCond->data1) ok = true;
        else if (rngok(pXCond->data1, 0, gNumConditions))
        {
            cmpOp = pCond->cstat;       PUSH = rngok(pXCond->command, kCmdPush, kCmdPush + kPushRange);
            arg1 = pXCond->data2;		arg2 = pXCond->data3;
            arg3 = pXCond->data4;		pEvent = pEvn;

            Unserialize(pXCond->targetX, &objType, &objIndex);
            pEntry = &gConditions[pXCond->data1];
            ok = pEntry->pCaller->pFunc();
        }
        else
        {
            errCondNotImplemented();
            return;
        }

        if ((ok) ^ (pCond->type == kModernConditionFalse))
        {
            pXCond->state = 1;
            if (pXCond->waitTime && !delayBefore) // delay after checking
            {
                // start waiting
                evPost(pCond->index, EVOBJ_SPRITE, EVTIME2TICKS(pXCond->waitTime), (COMMAND_ID)kCmdRepeat, pEvn->causer);
                pXCond->restState = 1;
                return;
            }
        }
        else
        {
            pXCond->state = 0;
        }
    }
    else if (pEvn->cmd == kCmdRepeat)
    {
        pXCond->restState = 0;
    }
    else
    {
        return;
    }

    if (pXCond->state)
    {
        // trigger once per result?
        if (pCond->flags & kModernTypeFlag4)
        {
            if (pXCond->unused2)
                return;

            pXCond->unused2 = 1;
        }

        if (pXCond->triggerOnce)
        #ifdef CONDITIONS_USE_BUBBLE_ACTION
            conditionsBubble(pXCond, conditionsSetIsTriggered, true);
        #else
            conditionsSetIsTriggered(pXCond, true);
        #endif

        if (rngok(pXCond->command, kCmdPop, kCmdPop + kPushRange))
            Restore(); // change focus to the saved object

        // send command to rx bucket
        if (pXCond->txID)
        {
            Unserialize(pXCond->targetX, &objType, &objIndex);
            evSend(pCond->index, EVOBJ_SPRITE, pXCond->txID, (COMMAND_ID)pXCond->command, (objType == EVOBJ_SPRITE) ? objIndex : kCauserGame);
        }

        if (pCond->flags)
        {
            // send it for object that currently in the focus
            if (pCond->flags & kModernTypeFlag1)
            {
                TriggerObject(pXCond->targetX);
                if ((pCond->flags & kModernTypeFlag2) && pXCond->targetX == pXCond->targetY)
                    return;
            }

            // send it for initial object
            if (pCond->flags & kModernTypeFlag2)
            {
                TriggerObject(pXCond->targetY);
            }
        }
    }
    else
    {
        pXCond->unused2 = 0;
    }
}

#ifdef CONDITIONS_USE_BUBBLE_ACTION
void conditionsBubble(XSPRITE* pXStart, void(*pActionFunc)(XSPRITE*, int), int nValue)
{
    int i, j;

    pActionFunc(pXStart, nValue);

    // perform action for whole branch from bottom to top while there is no forks
    for (i = headspritestat[kStatModernCondition]; i >= 0; i = nextspritestat[i])
    {
        XSPRITE* pXSprite = &xsprite[sprite[i].extra];
        if (pXSprite->txID != pXStart->rxID)
            continue;

        for (j = headspritestat[kStatModernCondition]; j >= 0; j = nextspritestat[j])
        {
            XSPRITE* pXSprite2 = &xsprite[sprite[j].extra];
            if (pXSprite2->rxID == pXStart->rxID && pXSprite2 != pXStart)
            {
                if (pActionFunc == conditionsSetIsTriggered)
                {
                    if (pXSprite2->triggerOnce && ((nValue != 0 && !pXSprite2->isTriggered) || (nValue == 0 && pXSprite2->isTriggered)))
                        break; // fork found
                }
                else if (pActionFunc == conditionsSetIsLocked)
                {
                    if ((nValue != 0 && !pXSprite2->locked) || (nValue == 0 && pXSprite2->locked))
                        break; // fork found
                }
                else
                {
                    break; // fork found
                }
            }
        }

        if (j < 0)
            conditionsBubble(pXSprite, pActionFunc, nValue);
    }
}
void conditionsSetIsTriggered(XSPRITE* pXSprite, int nValue)
{
    pXSprite->isTriggered = nValue;
    evKill(pXSprite->reference, OBJ_SPRITE);
}

void conditionsSetIsLocked(XSPRITE* pXSprite, int nValue)
{ 
    pXSprite->locked        = nValue;
    pXSprite->restState     = 0;
    pXSprite->state         = 0;
    evKill(pXSprite->reference, OBJ_SPRITE);
    if (pXSprite->busyTime)
        pXSprite->busy = 0;
}
#endif