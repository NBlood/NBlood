//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 Nuke.YKT
Copyright (C) NoOne

*********************************************************************
NoOne: This file provides dude patrol functionality in modern maps.
For full documentation visit: http://cruo.bloodgame.ru/xxsystem
*********************************************************************

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

#ifdef NOONE_EXTENSIONS
#include "nnexts.h"
#include "aipatrol.h"
#include "nnextcdud.h"
#include "trig.h"
#include "mmulti.h"
//-------------------------------------------------------------------------
#define kPatrolAlarmSeeDist         10000
#define kMaxPatrolVelocity          500000
#define kMinPatrolTurnDelay         8
#define kPatrolTurnDelayRange       20
#define kMaxPatrolSounds            LENGTH(BonkleCache)
//-------------------------------------------------------------------------
enum enum_PATH_DIR
{
kPatrolMoveForward                  = 0,
kPatrolMoveBackward                 = 1,
};

static struct PATROL_FOUND_SOUNDS
{
    int32_t snd;
    uint8_t cnt;
}
patrolSnd[kMaxPatrolSounds];

static void aiPatrolThink(spritetype* pSpr, XSPRITE* pXSpr);
static int  aiPatrolThinkTarget(spritetype* pSpr, XSPRITE* pXSpr);
static void aiPatrolMoveStop(spritetype* pSpr, XSPRITE* pXSpr);
static void aiPatrolEnterTurn(spritetype* pSpr, XSPRITE* pXSpr);
static void aiPatrolMove(spritetype* pSpr, XSPRITE* pXSpr);
static void aiPatrolAlarm(spritetype* pSpr, XSPRITE* pXSpr, spritetype* pTarg);
static void aiPatrolRandGoalAng(spritetype* pSpr, XSPRITE* pXSpr);
static void aiPatrolSetDirection(spritetype* pSpr, XSPRITE* pXSpr, int nAng);
static char aiPatrolSetupCrouchFlag(spritetype* pSpr, XSPRITE* pXSpr, int nMask);

static int  markerFindNext(XSPRITE* pXMark, char back);
static char markerIsNode(XSPRITE* pXMark, char back);

static char canSee(spritetype* pSpr, spritetype* pOth);
static char canMove(spritetype* pSpr, int nTarg, int nAng, int nRange);
static char cansee_NOTROR(int32_t x1, int32_t y1, int32_t z1, int16_t orig_sect1, int32_t x2, int32_t y2, int32_t z2, int16_t orig_sect2, int32_t wallmask = CSTAT_WALL_1WAY);
static char spritesTouching(int nXSpr1, int nXSpr2);

//-------------------------------------------------------------------------
static AISTATE genPatrolStates[] =
{
    { kAiStatePatrolIdle, 0, -1, 0, aiPatrolMoveStop, NULL, aiPatrolThink, NULL },
    { kAiStatePatrolMove, 0, -1, 0, NULL, aiPatrolMove, aiPatrolThink, NULL },
    { kAiStatePatrolTurn, 0, -1, 0, aiPatrolEnterTurn, aiPatrolMove, aiPatrolThink, NULL },
    { kAiStatePatrolGuard, 0, -1, 0, NULL, aiPatrolMove, aiPatrolThink, NULL },
};
//-------------------------------------------------------------------------

static void aiPatrolThink(spritetype* pSpr, XSPRITE* pXSpr)
{
    if (!dudeIsAlive(pSpr))
        return;

    spritetype* pMark; XSPRITE* pXMark; DUDEEXTRA_STATS* pStats; PLAYER* pPlay = NULL;
    int32_t nTarg = -1, nDist, nWTime, nState, nAng, vAng, x, y;
    char itMovesBack;

    // This is indicator for calling aiPatrolSetDirection().
    // The function has been placed in movement
    // processing for smooth camera
    // rotation.
    //
    pXSpr->targetY = 0; // do not call

    if (IsPlayerSprite(pSpr))
    {
        if ((pPlay = getPlayerById(pSpr->type)) != NULL)
            pPlay->restTime = 0; // mute songs
    }
    else if ((nTarg = aiPatrolThinkTarget(pSpr, pXSpr)) >= 0)
    {
        aiPatrolStop(pSpr, nTarg, pXSpr->dudeAmbush);
        return;
    }

    if (!spriRangeIsFine(pXSpr->target) || !xsprIsFine(&sprite[pXSpr->target]))
    {
        aiPatrolStop(pSpr, nTarg, pXSpr->dudeAmbush);
        return;
    }

    pStats = &gDudeExtra[pSpr->extra].stats;
    itMovesBack = (pXSpr->unused2 == kPatrolMoveBackward);
    pMark = &sprite[pXSpr->target]; pXMark = &xsprite[pMark->extra];

    if (aiPatrolTurning(pXSpr->aiState))
    {
        if ((int)pSpr->ang == (int)pXSpr->goalAng)
        {
            if (pMark->flags & kModernTypeFlag64)
                aiPatrolState(pSpr, pXSpr, kAiStatePatrolGuard, pXSpr->stateTimer);
            else
                aiPatrolState(pSpr, pXSpr, kAiStatePatrolIdle, pXSpr->stateTimer);
        }

        return;
    }
    else if (aiPatrolGuarding(pXSpr->aiState))
    {
        // patrol the area in the current marker radius

        vAng = getVelocityAngle(pSpr);
        nAng = getangle(pMark->x - pSpr->x, pMark->y - pSpr->y);

        if (!canMove(pSpr, -1, vAng, pSpr->clipdist << 2))
        {
            aiPatrolState(pSpr, pXSpr, kAiStatePatrolIdle, pXSpr->stateTimer);
            return;
        }

        nDist = ClipLow(pMark->clipdist << 1, 8);
        x = klabs(pMark->x - pSpr->x) >> 4;
        y = klabs(pMark->y - pSpr->y) >> 4;

        if (approxDist(x, y) < nDist)
        {
            nDist -= 32;
            if (approxDist(x, y) >= nDist && klabs(DANGLE(vAng, nAng)) > kAng90)
                aiPatrolState(pSpr, pXSpr, kAiStatePatrolIdle, pXSpr->stateTimer);

            return;
        }
    }
    else if (aiPatrolWaiting(pXSpr->aiState))
    {
        if (pXSpr->stateTimer > 0 || pXMark->data1 == pXMark->data2)
        {
            aiPatrolMoveStop(pSpr, pXSpr); // for slopes/wind

            // turn while waiting
            if ((pMark->flags & kModernTypeFlag16) && --pStats->thinkTime <= 0)
            {
                pStats->thinkTime = kMinPatrolTurnDelay + Random(kPatrolTurnDelayRange);
                aiPatrolState(pSpr, pXSpr, kAiStatePatrolTurn, pXSpr->stateTimer);
            }

            return;
        }

        // trigger at departure
        if (pXMark->triggerOff)
        {
            if (pXMark->txID) // send command
            {
                evSend(pMark->index, OBJ_SPRITE, pXMark->txID, (COMMAND_ID)pXMark->command, pSpr->index);
            }
            else if (pXMark->command == kCmdDudeFlagsSet) // copy dude flags for current dude
            {
                aiPatrolFlagsMgr(pMark, pXMark, pSpr, pXSpr, true, true);
                if (!pXSpr->dudeFlag4) // this dude is not in patrol anymore
                    return;
            }
        }

        if (!aiPatrolSetMarker(pSpr, pXSpr))
        {
            aiPatrolStop(pSpr, -1); // break patrol
            return;
        }

        // start moving to the next marker
        aiPatrolState(pSpr, pXSpr, kAiStatePatrolMove);
    }
    else if (aiPatrolMarkerReached(pSpr, pXSpr))
    {
        pXMark->isTriggered = pXMark->triggerOnce; // can't select this marker for path anymore if true

        if (pXMark->waitTime > 0 || pXMark->data1 == pXMark->data2)
        {
            // take marker's angle
            if ((pMark->flags & kModernTypeFlag4) == 0)
            {
                nAng = pMark->ang;
                if (itMovesBack && (pMark->flags & kModernTypeFlag8) == 0)
                    nAng += kAng180;

                pXSpr->goalAng = nAng & kAngMask;
                if ((int)pSpr->ang != (int)pXSpr->goalAng) // let the enemy play move animation while turning
                    return;
            }

            // trigger at arrival
            if (pXMark->triggerOn)
            {
                if (pXMark->txID) // send command
                {
                    evSend(pMark->index, OBJ_SPRITE, pXMark->txID, (COMMAND_ID)pXMark->command, pSpr->index);
                }
                else if (pXMark->command == kCmdDudeFlagsSet) // copy dude flags for current dude
                {
                    aiPatrolFlagsMgr(pMark, pXMark, pSpr, pXSpr, true, true);
                    if (!pXSpr->dudeFlag4) // this dude is not in patrol anymore
                        return;
                }
            }

            nWTime = EVTIME2TICKS(pXMark->waitTime);
            if (pMark->flags & kModernTypeFlag128)
                nWTime = perc2val(5, nWTime), nWTime += perc2val(Random(95), EVTIME2TICKS(pXMark->waitTime));

            nState = kAiStatePatrolIdle;
            if (pMark->flags & kModernTypeFlag64)
                nState = kAiStatePatrolGuard;

            if (pMark->flags & kModernTypeFlag16)
                pStats->thinkTime = kMinPatrolTurnDelay + Random(kPatrolTurnDelayRange);

            aiPatrolSetupCrouchFlag(pSpr, pXSpr, pMark->flags & kModernTypeFlag3);
            aiPatrolState(pSpr, pXSpr, nState, nWTime);
            return;
        }
        else
        {
            if (pXMark->triggerOn || pXMark->triggerOff)
            {
                if (pXMark->txID)
                {
                    // send command at arrival
                    if (pXMark->triggerOn)
                        evSend(pMark->index, OBJ_SPRITE, pXMark->txID, (COMMAND_ID)pXMark->command, pSpr->index);

                    // send command at departure
                    if (pXMark->triggerOff)
                        evSend(pMark->index, OBJ_SPRITE, pXMark->txID, (COMMAND_ID)pXMark->command, pSpr->index);
                }
                else if (pXMark->command == kCmdDudeFlagsSet) // copy dude flags for current dude
                {
                    aiPatrolFlagsMgr(pMark, pXMark, pSpr, pXSpr, true, true);
                    if (!pXSpr->dudeFlag4) // this dude is not in patrol anymore
                        return;
                }
            }

            if (aiPatrolSetupCrouchFlag(pSpr, pXSpr, pMark->flags & kModernTypeFlag3))
                aiPatrolState(pSpr, pXSpr, kAiStatePatrolMove); // seq must be updated

            if (!aiPatrolSetMarker(pSpr, pXSpr))
            {
                aiPatrolStop(pSpr, -1); // break patrol
                return;
            }
        }
    }
    
    // call aiPatrolSetDirection() to set goalAng
    pXSpr->targetY = 1;
}
//-------------------------------------------------------------------------
static int aiPatrolThinkTarget(spritetype* pSpr, XSPRITE* pXSpr)
{
    static struct
    {
        int32_t seeDist;
        int32_t hearDist;
        int32_t periphery;
        int32_t eyeAboveZ;
    }
    info;

    spritetype* pReg; XSPRITE* pXReg;
    int32_t seeDist, hearDist, feelDist, seeChance, hearChance;
    int32_t nDist, nSpotDist, nSect, sndCnt, dx, dy;
    int32_t i, j, k, d, mod, z;

    uint8_t stealthMode = ((pXSpr->unused1 & kDudeFlagStealth) /*&& gGameOptions.nGameType == kGameTypeSinglePlayer*/);
    uint8_t isInvisible, canHear = pXSpr->dudeDeaf == 0, canSee = pXSpr->dudeGuard == 0;
    uint8_t sndMax = ClipLow((gGameOptions.nDifficulty + 1) >> 1, 1);

    // clear all found sounds
    Bmemset(patrolSnd, 0, sizeof(patrolSnd));
    sndCnt = 0;

    switch (pSpr->type)
    {
        case kDudeModernCustom:
        {
            CUSTOMDUDE* pDude = cdudeGet(pSpr);
            info.periphery = ClipLow(pDude->periphery, kAng60);
            info.seeDist   = (stealthMode) ? pDude->seeDist / 3 : pDude->seeDist / 2;
            info.hearDist  = pDude->hearDist;
            info.eyeAboveZ = pDude->eyeHeight;
            break;
        }
        default:
        {
            DUDEINFO* pInfo = getDudeInfo(pSpr->type);
            info.periphery = ClipLow(pInfo->periphery, kAng60);
            info.seeDist = (stealthMode) ? pInfo->seeDist / 3 : pInfo->seeDist / 2;
            info.hearDist = pInfo->hearDist;
            info.eyeAboveZ = (pInfo->eyeHeight * pSpr->yrepeat) << 2;
            break;
        }
    }

    nSpotDist = info.seeDist, nDist = INT32_MAX;
    for (i = headspritestat[kStatModernPatrolRegion]; i >= 0; i = nextspritestat[i])
    {
        // search in dude regions to clip see/hear distances

        pReg = &sprite[i];
        if (!xsprIsFine(pReg) || (pReg->flags & kModernTypeFlag64) == 0)
            continue; // ignore non-clip mode regions

        pXReg = &xsprite[pReg->extra];
        if (pXReg->locked || (pXReg->data2 < 0 && pXReg->data3 < 0))
            continue;

        if (rngok(pXReg->data1, 1, nDist))
        {
            nDist = pXReg->data1;
            if (approxDist(klabs(pReg->x - pSpr->x) >> 4, klabs(pReg->y - pSpr->y) >> 4) < nDist)
            {
                if (pXReg->data2 >= 0) info.hearDist = pXReg->data2 << 4;
                if (pXReg->data3 >= 0) info.seeDist  = pXReg->data3 << 4;
                if (info.hearDist > nSpotDist)
                    nSpotDist = info.hearDist;
            }
        }
    }

    // search for player targets
    for (i = connecthead; i != -1; i = connectpoint2[i])
    {
        PLAYER* pPlay = &gPlayer[i];
        spritetype* pTarg = pPlay->pSprite;
        if (!xsprIsFine(pTarg) || pTarg->index == pSpr->index)
            continue;

        XSPRITE* pXTarg = &xsprite[pTarg->extra];
        if (pXTarg->health <= 0)
            continue;

        dx = pTarg->x - pSpr->x, dy = pTarg->y - pSpr->y;
        if ((nDist = approxDist(dx, dy)) > nSpotDist)
            continue;

        GetSpriteExtents(pTarg, &z, &j); //use ztop of the target sprite
        if (!cansee(pTarg->x, pTarg->y, z, pTarg->sectnum, pSpr->x, pSpr->y, pSpr->z - info.eyeAboveZ, pSpr->sectnum))
            continue;

        seeDist = info.seeDist; hearDist = info.hearDist; feelDist = hearDist >> 1;
        isInvisible = (powerupCheck(pPlay, kPwUpShadowCloak) > 0);
        seeChance = hearChance = 0x0000;

        if ((pXSpr->unused1 & kDudeFlagIgnoreTouch) == 0)
        {
            if (spritesTouching(pSpr->extra, pTarg->extra) || spritesTouching(pTarg->extra, pSpr->extra))
            {
                if (isInvisible)
                    pPlay->pwUpTime[kPwUpShadowCloak] = 0;

                return pTarg->index;
            }
        }

        if (canHear)
        {
            for (j = 0; j < kMaxPatrolSounds && hearChance < kMaxPatrolSpotValue; j++)
            {
                BONKLE* pBonk = BonkleCache[j];
                if ((pBonk->sfxId <= 0) || (!pBonk->lChan && !pBonk->rChan))
                    continue; // sound is not playing

                if ((d = approxDist(pBonk->curPos.x - pSpr->x, pBonk->curPos.y - pSpr->y)) > hearDist)
                    continue;

                // N same sounds per single enemy
                // the higher difficulty, the more sounds is allowed
                // the more sounds found, the greater chance

                for (k = 0; k < kMaxPatrolSounds; k++)
                {
                    if (patrolSnd[k].snd == pBonk->sfxId && ++patrolSnd[k].cnt >= sndMax)
                        break;
                }

                if (k < kMaxPatrolSounds) continue;
                else if (sndCnt < kMaxPatrolSounds - 1)
                    patrolSnd[sndCnt++].snd = pBonk->sfxId;

                nSect = pBonk->sectnum;
                k = -1;

                if (pBonk->pSndSpr)
                {
                    spritetype* pSndSpr = pBonk->pSndSpr; // sound attached to the sprite
                    if (pSpr->index != pSndSpr->index && actSpriteOwnerToSpriteId(pSndSpr) != pSpr->index)
                        nSect = pSndSpr->sectnum; // get it's sector
                }

                if (nSect >= 0)
                {
                    for (k = headspritesect[nSect]; k >= 0; k = nextspritesect[k])
                    {
                        if (/*sprite[k].index == pTarg->index || */actSpriteOwnerToSpriteId(&sprite[k]) == pTarg->index)
                            break;
                    }
                }

                if (k >= 0)
                    hearChance += mulscale8(pBonk->vol, (hearDist - d) >> 3);
            }
        }

        if (!isInvisible && (canHear || canSee))
        {
            if (stealthMode)
            {
                if (pPlay->posture == kPostureCrouch)
                {
                    seeDist  -= perc2val(25, seeDist);
                    feelDist -= perc2val(75, feelDist);
                }
            }

            if (hearDist)
            {
                if (canHear)
                    canHear = (nDist < hearDist || hearChance > 0);

                if (canHear && nDist < feelDist && (xvel[pTarg->index] || yvel[pTarg->index] || zvel[pTarg->index]))
                {
                    int32_t xv, yv, zv;
                    xv = klabs(xvel[pTarg->index]), yv = klabs(yvel[pTarg->index]), zv = klabs(zvel[pTarg->index]);
                    hearChance += ClipLow(mulscale8(1, ClipLow(((feelDist - nDist) + xv + yv + zv) >> 6, 0)), 0);
                }
            }

            if (seeDist)
            {
                int32_t nAng = klabs(DANGLE(getangle(dx, dy), pSpr->ang));

                if (canSee)
                    canSee = (nDist < seeDist && nAng < info.periphery);

                if (canSee)
                {
                    int32_t base = 100 + ((20 * gGameOptions.nDifficulty) - (nAng / 5));
                    int32_t d = nDist >> 2;
                    int32_t m = divscale8(d, 0x2000);
                    int32_t t = mulscale8(d, m);
                    seeChance = ClipRange(divscale8(base, t), 0, kMaxPatrolSpotValue >> 1);
                }
            }

            if (!canHear && !canSee)
                continue;

            if (stealthMode)
            {
                // search in stealth regions to modify spot chances
                for (j = headspritestat[kStatModernPatrolRegion]; j >= 0; j = nextspritestat[j])
                {
                    pReg = &sprite[j];
                    if (!xspriRangeIsFine(pReg->extra))
                        continue;

                    if ((pReg->flags & kModernTypeFlag64) != 0)
                        continue; // ignore clip mode regions

                    pXReg = &xsprite[pReg->extra];
                    if (pXReg->locked) // ignore locked regions
                        continue;

                    char fixd = (pReg->flags & kModernTypeFlag1) != 0;       // fixed percent value
                    char both = (pReg->flags & kModernTypeFlag4) != 0;       // target AND dude must be in this region
                    char dude = (both | (pReg->flags & kModernTypeFlag2));   // dude must be in this region
                    char trgt = (both | !dude);                              // target must be in this region
                    char crch = (pReg->flags & kModernTypeFlag8) != 0;       // target must have crouch posture

                    if (trgt)
                    {
                        if (crch && pPlay->posture != kPostureCrouch)
                            continue;

                        if (pXReg->data1 > 0)
                        {
                            if (approxDist(klabs(pReg->x - pTarg->x) >> 4, klabs(pReg->y - pTarg->y) >> 4) >= pXReg->data1)
                                continue;
                        }
                        else if (pTarg->sectnum != pReg->sectnum)
                            continue;
                    }


                    if (dude)
                    {
                        if (pXReg->data1 > 0)
                        {
                            if (approxDist(klabs(pReg->x - pSpr->x) >> 4, klabs(pReg->y - pSpr->y) >> 4) >= pXReg->data1)
                                continue;
                        }
                        else if (pSpr->sectnum != pReg->sectnum)
                            continue;
                    }

                    if (hearDist)
                    {
                        if (fixd)
                            hearChance = ClipLow(hearChance, pXReg->data2);

                        mod = perc2val(pXReg->data2, hearChance);
                        if (fixd)  hearChance = mod; else hearChance += mod;
                        hearChance = ClipRange(hearChance, -kMaxPatrolSpotValue, kMaxPatrolSpotValue);
                    }

                    if (seeDist)
                    {
                        if (fixd)
                            seeChance = ClipLow(seeChance, pXReg->data3);

                        mod = perc2val(pXReg->data3, seeChance);
                        if (fixd) seeChance = mod; else seeChance += mod;
                        seeChance = ClipRange(seeChance, -kMaxPatrolSpotValue, kMaxPatrolSpotValue);
                    }


                    // trigger this region if target gonna be spot
                    if (pXReg->txID && pXSpr->data3 + hearChance + seeChance >= kMaxPatrolSpotValue)
                        trTriggerSprite(pReg->index, pXReg, kCmdToggle, pPlay->nSprite);

                    // continue search another stealth regions to affect chances
                }
            }

            if (hearDist && hearChance > 0)
            {
                //consoleSysMsg("Patrol dude #%d hearing the Player #%d.", pSprite->index, pPlayer->nPlayer + 1);
                pXSpr->data3 = ClipRange(pXSpr->data3 + hearChance, -kMaxPatrolSpotValue, kMaxPatrolSpotValue);
                if (!stealthMode)
                    return pTarg->index;
            }

            if (seeDist && seeChance > 0)
            {
                //consoleSysMsg("Patrol dude #%d seeing the Player #%d.", pSprite->index, pPlayer->nPlayer + 1);
                pXSpr->data3 = ClipRange(pXSpr->data3 + seeChance, -kMaxPatrolSpotValue, kMaxPatrolSpotValue);
                if (!stealthMode)
                    return pTarg->index;
            }
        }

        pXSpr->data3 = ClipRange(pXSpr->data3, 0, kMaxPatrolSpotValue);
        if (pXSpr->data3 == kMaxPatrolSpotValue)
            return pTarg->index;

        //int perc = (100 * ClipHigh(pXSprite->data3, kMaxPatrolSpotValue)) / kMaxPatrolSpotValue;
        //viewSetSystemMessage("%d / %d / %d / %d", hearChance, seeDist, seeChance, perc);

    }

    pXSpr->data3 -= ClipLow(((kPercFull * pXSpr->data3) / kMaxPatrolSpotValue) >> 2, 3);
    return -1;
}
//-------------------------------------------------------------------------
static void aiPatrolRandGoalAng(spritetype* pSpr, XSPRITE* pXSpr)
{
    int angArr[] = { kAng45, kAng60, kAng90 };
    int nAng = angArr[Random(LENGTH(angArr))];

    if (aiPatrolMarkerReached(pSpr, pXSpr))
    {
        spritetype* pTarg = &sprite[pXSpr->target];
        if (pTarg->flags & kModernTypeFlag64)
            nAng = -nAng;
    }

    if (nAng > 0 && Chance(0x8000))
        nAng = -nAng;

    pXSpr->goalAng = (pSpr->ang + nAng) & kAngMask;
}
//-------------------------------------------------------------------------
static void aiPatrolMove(spritetype* pSpr, XSPRITE* pXSpr)
{
    if (!dudeIsAlive(pSpr))
        return;

    CUSTOMDUDE* pDude; spritetype* pTarg; XSPRITE* pXTarg;
    int32_t nAng, gAng, nFrontSpeed, nTurnSpeed;
    int32_t nVel, dx, dy;

    char guardMode, isFlying;
    DUDEINFO_EXTRA* pExtra = &gDudeInfoExtra[pSpr->type - kDudeBase];
    DUDEINFO* pInfo = &dudeInfo[pSpr->type - kDudeBase];
    PLAYER* pPlay = NULL; POSTURE* pPosture;

    if (IsPlayerSprite(pSpr))
        pPlay = getPlayerById(pSpr->type);

    switch (pSpr->type)
    {
        case kDudeModernCustom:
            pDude = cdudeGet(pSpr->index);
            isFlying = pDude->IsFlying();
            nFrontSpeed = pDude->GetVelocity(kParVelocityForward);
            nTurnSpeed = pDude->GetVelocity(kParVelocityTurn);
            break;
        default:
            if (pPlay)
            {
                pPosture = &pPlay->pPosture[pPlay->lifeMode][pPlay->posture];
                isFlying = (pSpr->flags & kPhysGravity) == 0;
                nFrontSpeed = pPosture->frontAccel << 3;
                nTurnSpeed = nFrontSpeed >> 11;
            }
            else
            {
                isFlying = pExtra->flying;
                nFrontSpeed = pInfo->frontSpeed;
                nTurnSpeed = (pInfo->angSpeed << 2) >> 4;
            }

            if (pXSpr->unused4 & kDudeFlagCrouch)
                nFrontSpeed >>= 1, nTurnSpeed >>= 1;

            break;
    }
    
    pTarg = (spriRangeIsFine(pXSpr->target)) ? &sprite[pXSpr->target] : NULL;

    if (pTarg && pXSpr->targetY == 1 && (pPlay || (gFrame & 2) == (pSpr->index & 2)))
        aiPatrolSetDirection(pSpr, pXSpr, getangle(pTarg->x - pSpr->x, pTarg->y - pSpr->y));

    nAng = DANGLE(pXSpr->goalAng, pSpr->ang);
    pSpr->ang = (pSpr->ang + ClipRange(nAng, -nTurnSpeed, nTurnSpeed)) & kAngMask;
    if (aiPatrolTurning(pXSpr->aiState) || pTarg == NULL)
        return;

    pXTarg = (xsprIsFine(pTarg)) ? &xsprite[pTarg->extra] : NULL;

    gAng = kAng60;
    if (isFlying || spriteIsUnderwater(pSpr))
        zvel[pSpr->index] = (pTarg->z - pSpr->z) * 6, gAng >>= 1;

    if (klabs(nAng) > gAng)
    {
        aiPatrolMoveStop(pSpr, pXSpr);
        return;
    }

    if ((guardMode = aiPatrolGuarding(pXSpr->aiState)) == 0)
    {
        if (aiPatrolMarkerReached(pSpr, pXSpr))
        {
            if (pXTarg && ((pXTarg->waitTime > 0 || pXTarg->data1 == pXTarg->data2)))
            {
                aiPatrolMoveStop(pSpr, pXSpr);
                return;
            }

            if (isFlying)
                nnExtFixDudeDrag(pSpr, 128);
        }
    }
    else if (isFlying)
        nnExtFixDudeDrag(pSpr, 128);

    if (pXTarg && pXTarg->busyTime > 0)
        nFrontSpeed = ClipHigh((nFrontSpeed / 3) + (2500 * pXTarg->busyTime), 0x47956);

    xvel[pSpr->index] += mulscale30(nFrontSpeed, Cos(pSpr->ang));
    yvel[pSpr->index] += mulscale30(nFrontSpeed, Sin(pSpr->ang));

    if (!guardMode && (pXTarg == NULL || pXTarg->wave != 1))
    {
        dx = (pTarg->x - pSpr->x);
        dy = (pTarg->y - pSpr->y);

        nVel = mulscale16(kMaxPatrolVelocity, approxDist(dx, dy) << 6);
        xvel[pSpr->index] = ClipRange(xvel[pSpr->index], -nVel, nVel);
        yvel[pSpr->index] = ClipRange(yvel[pSpr->index], -nVel, nVel);
    }
}
//-------------------------------------------------------------------------
static void aiPatrolMoveStop(spritetype* pSpr, XSPRITE* pXSpr)
{
    UNREFERENCED_PARAMETER(pXSpr);

    xvel[pSpr->index] = 0;
    yvel[pSpr->index] = 0;

    if ((pSpr->flags & kPhysGravity) == 0)
        zvel[pSpr->index] = 0;
}
//-------------------------------------------------------------------------
static void aiPatrolEnterTurn(spritetype* pSpr, XSPRITE* pXSpr)
{
    UNREFERENCED_PARAMETER(pXSpr);

    aiPatrolMoveStop(pSpr, pXSpr);
    aiPatrolRandGoalAng(pSpr, pXSpr);
}
//-------------------------------------------------------------------------
void aiPatrolState(spritetype* pSpr, XSPRITE* pXSpr, int nState, unsigned int nTime)
{
    DUDEINFO_EXTRA* pExtra; AISTATE *pState, *p; CUSTOMDUDE* pDude;
    PLAYER* pPlay;

    int nSeq = -1;
    char uwater = spriteIsUnderwater(pSpr);
    char crouch = ((pXSpr->unused4 & kDudeFlagCrouch) && !uwater);

    if (!crouch)
        pXSpr->unused4 &= ~kDudeFlagCrouch;

    switch (pSpr->type)
    {
        case kDudeCultistShotgunProne: pSpr->type = kDudeCultistShotgun; break;
        case kDudeCultistTommyProne:   pSpr->type = kDudeCultistTommy;   break;
    }

    pExtra = &gDudeInfoExtra[pSpr->type - kDudeBase];

    switch (nState)
    {
        case kAiStatePatrolMove:
        case kAiStatePatrolTurn:
        case kAiStatePatrolGuard:
            if (uwater)         nSeq = pExtra->mvewseqofs;
            else if (crouch)    nSeq = pExtra->mvecseqofs;
            else                nSeq = pExtra->mvegseqofs;

            if (nSeq >= 0)
                break;

            if ((nSeq = pExtra->mvegseqofs) >= 0)
            {
                crouch = uwater = 0;
                break;
            }

            nState = kAiStatePatrolIdle;

            fallthrough__;
        case kAiStatePatrolIdle:
            if (uwater)         nSeq = pExtra->idlwseqofs;
            else if (crouch)    nSeq = pExtra->idlcseqofs;
            else                nSeq = pExtra->idlgseqofs;

            if (nSeq >= 0)
                break;

            if ((nSeq = pExtra->idlgseqofs) >= 0)
            {
                crouch = uwater = 0;
                break;
            }

            viewSetSystemMessage("Dude type %d unavailable for patrol.", pSpr->type);
            fallthrough__;
        default:
            aiPatrolStop(pSpr, -1);
            return;
    }

    switch (pSpr->type)
    {
        case kDudeModernCustom:
            pDude = cdudeGet(pSpr->index);
            pDude->StatusRem(kCdudeStatusForceCrouch);

            if (uwater && pDude->CanSwim())             pDude->ChangePosture(kCdudePostureW);
            else if (crouch && pDude->CanCrouch())      pDude->ChangePosture(kCdudePostureC);
            else if (pDude->CanFly())                   pDude->ChangePosture(kCdudePostureF);
            else                                        pDude->ChangePosture(kCdudePostureL);

            p = (AISTATE*)pDude->states;
            nSeq = p[nSeq + pDude->posture].seqId;
            break;
        case kDudeCultistTesla:
        case kDudeCultistTNT:
            if (nState == kAiStatePatrolIdle && crouch)
            {
                nSeq = 11537; // these don't have idle crouch seq for some reason...
                break;
            }
            fallthrough__;
        default:
            nSeq += getDudeInfo(pSpr->type)->seqStartID;
            
            if (IsPlayerSprite(pSpr) && (pPlay = getPlayerById(pSpr->type)) != NULL)
            {
                if (uwater)         pPlay->posture = kPostureSwim;
                else if (crouch)    pPlay->posture = kPostureCrouch;
                else                pPlay->posture = kPostureStand;

                if (pPlay == gMe && !aiInPatrolState(pXSpr->aiState))
                {
                    gInput.keyFlags.lookCenter = 1;
                    gInput.syncFlags.keyChange = 1;
                }
            }

            break;
    }

    pState = &genPatrolStates[nState - kAiStatePatrolBase];
    pXSpr->stateTimer = (nTime > 0) ? nTime : pState->stateTicks;
    pXSpr->aiState = pState;

    if (nSeq > 0 && gSysRes.Lookup(nSeq, "SEQ"))
        seqSpawn(nSeq, OBJ_SPRITE, pSpr->extra, pState->funcId);
    else
        seqKill(OBJ_SPRITE, pSpr->extra);

    if (pState->enterFunc)
        pState->enterFunc(pSpr, pXSpr);
}
//-------------------------------------------------------------------------
char aiPatrolMarkerReached(spritetype* pSpr, XSPRITE* pXSpr)
{
    if (!spriRangeIsFine(pXSpr->target))
        return 0;

    spritetype* pMark = &sprite[pXSpr->target];
    if (pMark->type == kMarkerPath)
    {
        int32_t okDist = ClipLow(pMark->clipdist << 1, 8);
        int32_t oX = klabs(pMark->x - pSpr->x) >> 4;
        int32_t oY = klabs(pMark->y - pSpr->y) >> 4;
        int32_t zt[2], zb[2];
        uint8_t checkZ;

        if (approxDist(oX, oY) <= okDist)
        {
            if ((checkZ = spriteIsUnderwater(pSpr)) == 0)
            {
                if (IsCustomDude(pSpr))
                    checkZ = cdudeGet(pSpr->index)->IsFlying();
                else
                    checkZ = gDudeInfoExtra[pSpr->type - kDudeBase].flying;
            }

            if (checkZ)
            {
                okDist = pMark->clipdist << 4;
                GetSpriteExtents(pSpr,  &zt[0], &zb[0]);
                GetSpriteExtents(pMark, &zt[1], &zb[1]);

                if ((klabs(zb[0] - zt[1]) >> 6) > okDist
                    && (klabs(zt[0] - zb[1]) >> 6) > okDist)
                        return 0;
            }

            return 1;
        }

    }

    return 0;
}
//-------------------------------------------------------------------------
int aiPatrolMarkerBusy(int nExcept, int nMark)
{
    spritetype* pDude; XSPRITE* pXDude;
    int i;

    for (i = headspritestat[kStatDude]; i != -1; i = nextspritestat[i])
    {
        pDude = &sprite[i];
        if (pDude->index == nExcept || !dudeIsAlive(pDude))
            continue;

        pXDude = &xsprite[pDude->extra];
        if (pXDude->dudeFlag4 && pXDude->target == nMark)
            return pDude->index;
    }

    return -1;
}
//-------------------------------------------------------------------------
static char aiPatrolGetPathDir(XSPRITE* pXSpr, XSPRITE* pXMark)
{
    if (pXSpr->unused2 == kPatrolMoveForward)
        return (pXMark->data2 == -2) ? kPatrolMoveBackward : kPatrolMoveForward;

    return (markerFindNext(pXMark, kPatrolMoveBackward) >= 0) ? kPatrolMoveBackward : kPatrolMoveForward;
}
//-------------------------------------------------------------------------
char aiPatrolSetMarker(spritetype* pSpr, XSPRITE* pXSpr)
{
    spritetype* pNext = NULL;   XSPRITE* pXNext = NULL;
    spritetype* pCur = NULL;    XSPRITE* pXCur = NULL;
    spritetype* pPrev = NULL;   XSPRITE* pXPrev = NULL;

    int32_t nNext = -1, nNext_DEFAULT = -1, nPrev = -1;
    int32_t nDist = INT32_MAX;
    int32_t nextID, i, d;

    uint8_t isMovesBack, isNode;

    if (spriRangeIsFine(pXSpr->target))
    {
        pCur = &sprite[pXSpr->target];
        if (xsprIsFine(pCur))
            pXCur = &xsprite[pCur->extra];
    }

    // select closest marker that dude can see
    if (pCur == NULL || pCur->type != kMarkerPath || pXCur == NULL)
    {
        for (i = headspritestat[kStatPathMarker]; i >= 0; i = nextspritestat[i])
        {
            pNext = &sprite[i];
            if (!xsprIsFine(pNext))
                continue;

            pXNext = &xsprite[pNext->extra];
            if ((pXNext->locked || pXNext->isTriggered || pXNext->DudeLockout)
                || (d = approxDist(pNext->x - pSpr->x, pNext->y - pSpr->y)) > nDist)
                    continue;

            nDist = d;
            if (canSee(pSpr, pNext))
                nNext = pNext->index;
        }
    }
    else if (pCur->type == kMarkerPath) // set next marker
    {
        if (spriRangeIsFine(pXSpr->targetX) && sprite[pXSpr->targetX].type == kMarkerPath)
        {
            pPrev = &sprite[pXSpr->targetX];
            if (xsprIsFine(pPrev))
                pXPrev = &xsprite[pPrev->extra];
        }

        nPrev = pCur->index;
        isNode = markerIsNode(pXCur, 0);
        pXSpr->unused2 = aiPatrolGetPathDir(pXSpr, pXCur); // decide if it should go back or forward

        if (pXSpr->unused2 == kPatrolMoveBackward && Chance(0x8000) && isNode)
            pXSpr->unused2 = kPatrolMoveForward;

        isMovesBack = (pXSpr->unused2 == kPatrolMoveBackward);
        nextID = (isMovesBack) ? pXCur->data1 : pXCur->data2;

        if (nextID == -1)
            return 0; // break patrol

        for (i = headspritestat[kStatPathMarker]; i >= 0; i = nextspritestat[i])
        {
            pNext = &sprite[i];
            if (pNext->index == pCur->index || !xsprIsFine(pNext))
                continue;

            if (pPrev && pXPrev
                && isNode && pNext->index == pPrev->index && pXCur->data2 == pXPrev->data1)
                    continue;

            pXNext = &xsprite[pNext->extra];
            if (pXNext->locked || pXNext->isTriggered || pXNext->DudeLockout)
                continue;

            if ((isMovesBack && pXNext->data2 != nextID)
                || (!isMovesBack && pXNext->data1 != nextID))
                    continue;

            if (nNext_DEFAULT < 0)
                nNext_DEFAULT = pNext->index;

            if (aiPatrolMarkerBusy(pSpr->index, pNext->index) >= 0)
                continue;

            nNext = pNext->index;
            if (nnExtRandom(0, 5) == 3)
                break;
        }

        if (nNext < 0)
            nNext = nNext_DEFAULT;

        if (nNext < 0)
            consoleSysMsg("Could not get NEXT marker with id #%d for dude #%d! (back = %d)", nextID, pSpr->index, isMovesBack);
    }

    if (!spriRangeIsFine(nNext))
        return 0;

    pXSpr->target = nNext;
    pXSpr->targetX = nPrev; // keep previous marker index here, use actual sprite coords when selecting direction
    return 1;

}
//-------------------------------------------------------------------------
void aiPatrolStop(spritetype* pSpr, int nTarg, char alarm)
{
    XSPRITE* pXSpr; char patrol;

    if (!xspriRangeIsFine(pSpr->extra))
        return;

    DUDEEXTRA_STATS* pStats = &gDudeExtra[pSpr->extra].stats;

    pXSpr = &xsprite[pSpr->extra];
    pXSpr->data3 = 0;                       // reset spot progress
    pXSpr->unused4 &= ~kDudeFlagCrouch;     // reset the crouch status
    pXSpr->unused2 = kPatrolMoveForward;    // reset path direction
    pXSpr->targetX = -1;                    // reset the previous marker index
    pStats->thinkTime = 0;                  // reset turning delay

    if (dudeIsAlive(pSpr))
    {
        if (pXSpr->target >= 0 && sprite[pXSpr->target].type == kMarkerPath)
        {
            if (nTarg < 0)
                pSpr->ang = sprite[pXSpr->target].ang & kAngMask;

            pXSpr->target = -1;
        }

        if (!IsPlayerSprite(pSpr))
        {
            patrol = pXSpr->dudeFlag4, pXSpr->dudeFlag4 = 0;
            if (spriRangeIsFine(nTarg) && IsDudeSprite(&sprite[nTarg]) && xspriRangeIsFine(sprite[nTarg].extra))
            {
                aiSetTarget(pXSpr, nTarg);
                aiActivateDude(pSpr, pXSpr);

                if (alarm)
                    aiPatrolAlarm(pSpr, pXSpr, &sprite[nTarg]);
            }
            else
            {
                pXSpr->target = pSpr->index;
                aiInitSprite(pSpr);
                aiSetTarget(pXSpr, pXSpr->targetX, pXSpr->targetY, pXSpr->targetZ);
            }

            pXSpr->dudeFlag4 = patrol; // this must be kept so enemy can patrol after respawn again
        }
        else
        {
            pXSpr->dudeFlag4 = 0;
            pXSpr->stateTimer = 0;
            pXSpr->aiState = NULL;
        }
    }
}
//-------------------------------------------------------------------------
static void aiPatrolAlarm(spritetype* pSpr, XSPRITE* pXSpr, spritetype* pTarg)
{
    UNREFERENCED_PARAMETER(pXSpr);

    if (!dudeIsAlive(pSpr))
        return;

    spritetype* pDude; XSPRITE* pXDude;
    int nDist;
    int i;

    for (i = headspritestat[kStatDude]; i >= 0; i = nextspritestat[i])
    {
        pDude = &sprite[i];
        if (pDude->index == pSpr->index
            || IsPlayerSprite(pDude) || !dudeIsAlive(pDude))
                continue;

        pXDude = &xsprite[pDude->extra];
        if (pXDude->target == pTarg->index)
            continue;

        nDist = approxDist(pDude->x - pSpr->x, pDude->y - pSpr->y);
        if (nDist >= kPatrolAlarmSeeDist || !canSee(pSpr, pDude))
        {
            nDist = approxDist(pDude->x - pTarg->x, pDude->y - pTarg->y);
            if (nDist >= kPatrolAlarmSeeDist || !canSee(pTarg, pDude))
                continue;
        }

        if (aiInPatrolState(pXDude->aiState))
        {
            aiPatrolStop(pDude, pTarg->index, 0);
        }
        else
        {
            aiSetTarget(pXDude, pTarg->index);
            aiActivateDude(pDude, pXDude);
        }
    }
}
//-------------------------------------------------------------------------
void aiPatrolFlagsMgr(spritetype* pSrc, XSPRITE* pXSrc, spritetype* pDest, XSPRITE* pXDest, char copy, char init)
{
    UNREFERENCED_PARAMETER(pSrc);

    // copy flags
    if (copy)
    {
        pXDest->dudeFlag4  = pXSrc->dudeFlag4;
        pXDest->dudeAmbush = pXSrc->dudeAmbush;
        pXDest->dudeGuard  = pXSrc->dudeGuard;
        pXDest->dudeDeaf   = pXSrc->dudeDeaf;
        pXDest->unused1    = pXSrc->unused1;
    }

    // do init
    if (init && IsDudeSprite(pDest) /*&& !IsPlayerSprite(pDest)*/)
    {
        if (pXDest->dudeFlag4)
        {
            if (!aiInPatrolState(pXDest->aiState))
            {
                pXDest->target          = -1; // reset the target
                pXDest->stateTimer      = 0;

                if (aiPatrolSetMarker(pDest, pXDest))
                {
                    if (aiPatrolMarkerReached(pDest, pXDest))
                        aiPatrolState(pDest, pXDest, kAiStatePatrolIdle);
                    else
                        aiPatrolState(pDest, pXDest, kAiStatePatrolMove);
                    pXDest->data3 = 0;  // reset the spot progress
                }
                else
                    aiPatrolStop(pDest, -1);
            }
        }
        else if (aiInPatrolState(pXDest->aiState))
            aiPatrolStop(pDest, -1);
    }
}
//-------------------------------------------------------------------------
static int markerFindNext(XSPRITE* pXMark, char back)
{
    spritetype* pNext; XSPRITE* pXNext;
    int i;

    for (i = headspritestat[kStatPathMarker]; i >= 0; i = nextspritestat[i])
    {
        pNext = &sprite[i];
        if (pNext->index == pXMark->reference || !xsprIsFine(pNext))
            continue;

        pXNext = &xsprite[pNext->extra];
        if ((pXNext->locked || pXNext->isTriggered || pXNext->DudeLockout)
            || (back && pXNext->data2 != pXMark->data1) || (!back && pXNext->data1 != pXMark->data2))
                continue;

        return pNext->index;
    }

    return -1;
}
//-------------------------------------------------------------------------
static char markerIsNode(XSPRITE* pXMark, char back)
{
    spritetype* pNext; XSPRITE* pXNext;
    int i, c = 0;

    for (i = headspritestat[kStatPathMarker]; i >= 0; i = nextspritestat[i])
    {
        pNext = &sprite[i];
        if (pNext->index == pXMark->reference || !xsprIsFine(pNext))
            continue;

        pXNext = &xsprite[pNext->extra];
        if ((pXNext->locked || pXNext->isTriggered || pXNext->DudeLockout)
            || (back && pXNext->data2 != pXMark->data1) || (!back && pXNext->data1 != pXMark->data2))
                continue;

        if (++c > 1)
            return 1;
    }

    return 0;
}
//-------------------------------------------------------------------------
static char spritesTouching(int nXSpr1, int nXSpr2)
{
    SPRITEHIT* pHit = &gSpriteHit[nXSpr1];
    int nHSpr = -1;

    if ((pHit->hit & 0xc000) == 0xc000)
    {
        nHSpr = pHit->hit & 0x3fff;
        if (spriRangeIsFine(nHSpr) && sprite[nHSpr].extra == nXSpr2)
            return 1;
    }

    if ((pHit->florhit & 0xc000) == 0xc000)
    {
        nHSpr = pHit->florhit & 0x3fff;
        if (spriRangeIsFine(nHSpr) && sprite[nHSpr].extra == nXSpr2)
            return 1;
    }

    if ((pHit->ceilhit & 0xc000) == 0xc000)
    {
        nHSpr = pHit->ceilhit & 0x3fff;
        if (spriRangeIsFine(nHSpr) && sprite[nHSpr].extra == nXSpr2)
            return 1;
    }

    return 0;
}
//-------------------------------------------------------------------------
static char cansee_NOTROR(int32_t x1, int32_t y1, int32_t z1, int16_t orig_sect1, int32_t x2, int32_t y2, int32_t z2, int16_t orig_sect2, int32_t wallmask)
{
    int16_t sect1 = orig_sect1;
    int16_t sect2 = orig_sect2;
    int32_t dacnt, danum;
    const int32_t x21 = x2-x1, y21 = y2-y1, z21 = z2-z1;

    static uint8_t sectbitmap[bitmap_size(MAXSECTORS)];
    Bmemset(sectbitmap, 0, sizeof(sectbitmap));
    if (x1 == x2 && y1 == y2)
        return (sect1 == sect2);


    bitmap_set(sectbitmap, sect1);
    clipsectorlist[0] = sect1; danum = 1;

    for (dacnt=0; dacnt<danum; dacnt++)
    {
        const int32_t dasectnum = clipsectorlist[dacnt];
        auto const sec = (usectorptr_t)&sector[dasectnum];
        uwallptr_t wal;
        bssize_t cnt;

        for (cnt=sec->wallnum,wal=(uwallptr_t)&wall[sec->wallptr]; cnt>0; cnt--,wal++)
        {
            auto const wal2 = (uwallptr_t)&wall[wal->point2];
            const int32_t x31 = wal->x-x1, x34 = wal->x-wal2->x;
            const int32_t y31 = wal->y-y1, y34 = wal->y-wal2->y;

            int32_t x, y, z, nexts, t, bot;
            int32_t cfz[2];

            bot = y21*x34-x21*y34; if (bot <= 0) continue;
            // XXX: OVERFLOW
            t = y21*x31-x21*y31; if ((unsigned)t >= (unsigned)bot) continue;
            t = y31*x34-x31*y34;
            if ((unsigned)t >= (unsigned)bot)
            {
                continue;
            }

            nexts = wal->nextsector;
            if (nexts < 0 || wal->cstat & wallmask)
                return 0;

            t = divscale24(t,bot);
            x = x1 + mulscale24(x21,t);
            y = y1 + mulscale24(y21,t);
            z = z1 + mulscale24(z21,t);

            getzsofslope(dasectnum, x,y, &cfz[0],&cfz[1]);
            if (z <= cfz[0] || z >= cfz[1])
                return 0;

            getzsofslope(nexts, x,y, &cfz[0],&cfz[1]);
            if (z <= cfz[0] || z >= cfz[1])
                return 0;

            if (!bitmap_test(sectbitmap, nexts))
            {
                bitmap_set(sectbitmap, nexts);
                clipsectorlist[danum++] = nexts;
            }
        }
    }

    if (bitmap_test(sectbitmap, sect2))
        return 1;

    return 0;
}
//-------------------------------------------------------------------------
static char canSee(spritetype* pSpr, spritetype* pOth)
{
    int z[2][3];
    z[0][1] = pSpr->z;
    z[1][1] = pOth->z;

    GetSpriteExtents(pSpr, &z[0][0], &z[0][2]);
    GetSpriteExtents(pSpr, &z[1][0], &z[1][2]);

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (cansee_NOTROR(pSpr->x, pSpr->y, z[0][i], pSpr->sectnum, pOth->x, pOth->y, z[1][j], pOth->sectnum))
                return 1;
        }
    }

    return 0;
}
//-------------------------------------------------------------------------
static char aiPatrolSetupCrouchFlag(spritetype* pSpr, XSPRITE* pXSpr, int nMask)
{
    int t = pXSpr->unused4;
    char ok = 0;
    
    switch (nMask)
    {
        case kModernTypeFlag3: pXSpr->unused4 ^= kDudeFlagCrouch;  break;
        case kModernTypeFlag2: pXSpr->unused4 &= ~kDudeFlagCrouch; break;
        case kModernTypeFlag1: pXSpr->unused4 |= kDudeFlagCrouch;  break;
        default: return 0;
    }

    if (pXSpr->unused4 & kDudeFlagCrouch)
    {
        if (rngok(pSpr->type, kDudeBase, kDudeVanillaMax))
        {
            DUDEINFO_EXTRA* pExtra = &gDudeInfoExtra[pSpr->type - kDudeBase];
            ok = (pExtra->idlcseqofs >= 0 && pExtra->mvecseqofs >= 0);
        }
        else
            ok = (IsCustomDude(pSpr) & cdudeGet(pSpr->index)->CanCrouch());

        if (ok == 0)
            pXSpr->unused4 &= ~kDudeFlagCrouch;
    }

    return (t != pXSpr->unused4);
}


// a replacement of vanilla CanMove for patrol dudes
static char canMove(spritetype* pSpr, int nTarg, int nAng, int nRange)
{
    int x = pSpr->x, y = pSpr->y, z = pSpr->z, nSect = pSpr->sectnum;
    HitScan(pSpr, z, Cos(nAng) >> 16, Sin(nAng) >> 16, 0, CLIPMASK0, nRange);
    int nDist = approxDist(x - gHitInfo.hitx, y - gHitInfo.hity);

    if (nDist - (pSpr->clipdist << 2) < nRange)
    {
        if (gHitInfo.hitsprite < 0 || nTarg != gHitInfo.hitsprite)
            return 0;
    }

    x += mulscale30(nRange, Cos(nAng));
    y += mulscale30(nRange, Sin(nAng));
    if (!FindSector(x, y, z, &nSect))
        return 0;

    if (pSpr->extra > 0)
    {
        SPRITEHIT* pHit = &gSpriteHit[pSpr->extra];
        int nHID;

        if ((pHit->hit & 0xc000) == 0xc000)
        {
            nHID = pHit->hit & 0x3fff;
            spritetype* pHSpr = &sprite[nHID];
            int tAng = getangle(pHSpr->x - pSpr->x, pHSpr->y - pSpr->y);

            if (klabs(DANGLE(nAng, tAng)) <= kAng45)
                return 0;
        }
    }

    return 1;
}

// a replacement of vanilla aiChooseDirection for patrol dudes
static void aiPatrolSetDirection(spritetype* pSpr, XSPRITE* pXSpr, int nAng)
{
    int t1 = dmulscale30(xvel[pSpr->index], Cos(pSpr->ang), yvel[pSpr->index], Sin(pSpr->ang));
    int a = DANGLE(nAng, pSpr->ang), v8 = (a < 0) ? -kAng60 : kAng60;
    int vsi = ((t1 * 15) >> 12) / 2;

    if (canMove(pSpr, pXSpr->target, pSpr->ang + a, vsi))              pXSpr->goalAng = pSpr->ang + a;
    else if (canMove(pSpr, pXSpr->target, pSpr->ang + a / 2, vsi))     pXSpr->goalAng = pSpr->ang + a / 2;
    else if (canMove(pSpr, pXSpr->target, pSpr->ang - a / 2, vsi))     pXSpr->goalAng = pSpr->ang - a / 2;
    else if (canMove(pSpr, pXSpr->target, pSpr->ang + v8, vsi))        pXSpr->goalAng = pSpr->ang + v8;
    else if (canMove(pSpr, pXSpr->target, pSpr->ang, vsi))             pXSpr->goalAng = pSpr->ang;
    else if (canMove(pSpr, pXSpr->target, pSpr->ang - v8, vsi))        pXSpr->goalAng = pSpr->ang - v8;
    else                                                               pXSpr->goalAng = pSpr->ang + kAng60;
}

#endif