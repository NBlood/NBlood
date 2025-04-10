//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
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

#include "demo.h"
#include "duke3d.h"
#include "enet.h"
#include "input.h"
#include "savegame.h"

#ifdef __ANDROID__
#include "android.h"
#endif

hudweapon_t hudweap;

#ifdef SPLITSCREEN_MOD_HACKS
static int32_t g_snum;
#endif

extern int32_t g_levelTextTime, ticrandomseed;

int32_t g_numObituaries = 0;
int32_t g_numSelfObituaries = 0;


int const icon_to_inv[ICON_MAX] = { GET_FIRSTAID, GET_FIRSTAID, GET_STEROIDS, GET_HOLODUKE,
                                    GET_JETPACK,  GET_HEATS,    GET_SCUBA,    GET_BOOTS };

int const inv_to_icon[GET_MAX] = { ICON_STEROIDS, ICON_NONE,  ICON_SCUBA, ICON_HOLODUKE, ICON_JETPACK, ICON_NONE,
                                   ICON_NONE,     ICON_HEATS, ICON_NONE,  ICON_FIRSTAID, ICON_BOOTS };

void P_AddKills(DukePlayer_t * const pPlayer, uint16_t kills)
{
    pPlayer->actors_killed += kills;
}

void P_UpdateScreenPal(DukePlayer_t * const pPlayer)
{
    int       inWater       = 0;
    int const playerSectnum = pPlayer->cursectnum;

    if (pPlayer->heat_on)
        pPlayer->palette = SLIMEPAL;
    else if (playerSectnum < 0)
        pPlayer->palette = BASEPAL;
    else if (sector[playerSectnum].ceilingpicnum >= FLOORSLIME && sector[playerSectnum].ceilingpicnum <= FLOORSLIME + 2)
    {
        pPlayer->palette = SLIMEPAL;
        inWater          = 1;
    }
    else
    {
        pPlayer->palette     = (sector[pPlayer->cursectnum].lotag == ST_2_UNDERWATER) ? WATERPAL : BASEPAL;
        inWater              = 1;
    }

    g_restorePalette = 1+inWater;
}

static void P_IncurDamage(DukePlayer_t * const pPlayer)
{
    if (VM_OnEvent(EVENT_INCURDAMAGE, pPlayer->i, P_Get(pPlayer->i)) != 0)
        return;

    sprite[pPlayer->i].extra -= pPlayer->extra_extra8>>8;

    int playerDamage = sprite[pPlayer->i].extra - pPlayer->last_extra;

    if (playerDamage >= 0)
        return;

    pPlayer->extra_extra8 = 0;

    if (pPlayer->inv_amount[GET_SHIELD] > 0)
    {
        int const shieldDamage = playerDamage * (20 + (krand()%30)) / 100;

        playerDamage                     -= shieldDamage;
        pPlayer->inv_amount[GET_SHIELD] += shieldDamage;

        if (pPlayer->inv_amount[GET_SHIELD] < 0)
        {
            playerDamage += pPlayer->inv_amount[GET_SHIELD];
            pPlayer->inv_amount[GET_SHIELD] = 0;
        }
    }

    sprite[pPlayer->i].extra = pPlayer->last_extra + playerDamage;

    int const admg = klabs(playerDamage);
    I_AddForceFeedback((admg << FF_PLAYER_DMG_SCALE), (admg << FF_PLAYER_DMG_SCALE), (admg << FF_PLAYER_TIME_SCALE));
}

void P_QuickKill(DukePlayer_t * const pPlayer)
{
    P_PalFrom(pPlayer, 48, 48,48,48);
    I_AddForceFeedback(pPlayer->max_player_health << FF_PLAYER_DMG_SCALE, pPlayer->max_player_health << FF_PLAYER_DMG_SCALE, pPlayer->max_player_health << FF_PLAYER_TIME_SCALE);

    sprite[pPlayer->i].extra = 0;
    sprite[pPlayer->i].cstat |= 32768;

#ifndef EDUKE32_STANDALONE
    if (!FURY && ud.god == 0)
        A_DoGuts(pPlayer->i,JIBS6,8);
#endif
}

static void Proj_DoWaterTracers(vec3_t startPos, vec3_t const *endPos, int n, int16_t sectNum)
{
    if ((klabs(startPos.x - endPos->x) + klabs(startPos.y - endPos->y)) < 3084)
        return;

    vec3_t const v_inc = { tabledivide32_noinline(endPos->x - startPos.x, n + 1), tabledivide32_noinline(endPos->y - startPos.y, n + 1),
                           tabledivide32_noinline(endPos->z - startPos.z, n + 1) };

    for (bssize_t i=n; i>0; i--)
    {
        startPos.x += v_inc.x;
        startPos.y += v_inc.y;
        startPos.z += v_inc.z;

        updatesector(startPos.x, startPos.y, &sectNum);

        if (sectNum < 0)
            break;

        A_InsertSprite(sectNum, startPos.x, startPos.y, startPos.z, WATERBUBBLE, -32, 4 + (krand() & 3), 4 + (krand() & 3), krand() & 2047, 0, 0,
                       g_player[0].ps->i, 5);
    }
}

static inline projectile_t *Proj_GetProjectile(int tile)
{
    return ((unsigned)tile < MAXTILES && g_tile[tile].proj) ? g_tile[tile].proj : &DefaultProjectile;
}

static void A_HitscanProjTrail(const vec3_t *startPos, const vec3_t *endPos, int projAng, int tileNum, int16_t sectNum)
{
    const projectile_t *const pProj = Proj_GetProjectile(tileNum);

    vec3_t        spawnPos = { startPos->x + tabledivide32_noinline(sintable[(348 + projAng + 512) & 2047], pProj->offset),
                               startPos->y + tabledivide32_noinline(sintable[(projAng + 348) & 2047], pProj->offset),
                               startPos->z + 1024 + (pProj->toffset << 8) };

    int32_t      n         = ((FindDistance2D(spawnPos.x - endPos->x, spawnPos.y - endPos->y)) >> 8) + 1;

    vec3_t const increment = { tabledivide32_noinline((endPos->x - spawnPos.x), n),
                               tabledivide32_noinline((endPos->y - spawnPos.y), n),
                               tabledivide32_noinline((endPos->z - spawnPos.z), n) };

    spawnPos.x += increment.x >> 2;
    spawnPos.y += increment.y >> 2;
    spawnPos.z += increment.z >> 2;

    int32_t j;

    for (bssize_t i = pProj->tnum; i > 0; --i)
    {
        spawnPos.x += increment.x;
        spawnPos.y += increment.y;
        spawnPos.z += increment.z;

        updatesectorz(spawnPos.x, spawnPos.y, spawnPos.z, &sectNum);

        if (sectNum < 0)
            break;

        getzsofslope(sectNum, spawnPos.x, spawnPos.y, &n, &j);

        if (spawnPos.z > j || spawnPos.z < n)
            break;

        j = A_InsertSprite(sectNum, spawnPos.x, spawnPos.y, spawnPos.z, pProj->trail, -32,
                           pProj->txrepeat, pProj->tyrepeat, projAng, 0, 0, g_player[0].ps->i, 0);
        changespritestat(j, STAT_ACTOR);
    }
}

int32_t A_GetHitscanRange(int spriteNum)
{
    int const zOffset = (PN(spriteNum) == APLAYER) ? g_player[P_Get(spriteNum)].ps->spritezoffset : 0;
    hitdata_t hitData;

    SZ(spriteNum) -= zOffset;
    hitscan(&sprite[spriteNum].xyz, SECT(spriteNum), sintable[(SA(spriteNum) + 512) & 2047],
            sintable[SA(spriteNum) & 2047], 0, &hitData, CLIPMASK1);
    SZ(spriteNum) += zOffset;

    return (FindDistance2D(hitData.x - SX(spriteNum), hitData.y - SY(spriteNum)));
}

static int A_FindTargetSprite(const spritetype *pSprite, int projAng, int projecTile)
{
    static int const aimstats[] = {
        STAT_PLAYER, STAT_DUMMYPLAYER, STAT_ACTOR, STAT_ZOMBIEACTOR
    };

    int const playerNum = pSprite->picnum == APLAYER ? P_GetP(pSprite) : -1;

    if (playerNum != -1)
    {
        if (!g_player[playerNum].ps->auto_aim)
            return -1;

        if (g_player[playerNum].ps->auto_aim == 2)
        {
            if (A_CheckSpriteTileFlags(projecTile,SFLAG_PROJECTILE) && (Proj_GetProjectile(projecTile)->workslike & PROJECTILE_RPG))
                return -1;

#ifndef EDUKE32_STANDALONE
            if (!FURY)
            {
                switch (tileGetMapping(projecTile))
                {
                    case TONGUE__:
                    case FREEZEBLAST__:
                    case SHRINKSPARK__:
                    case SHRINKER__:
                    case RPG__:
                    case FIRELASER__:
                    case SPIT__:
                    case COOLEXPLOSION1__:
                        return -1;
                    default:
                        break;
                }
            }
#endif
        }
    }

    int const spriteAng = pSprite->ang;

#ifndef EDUKE32_STANDALONE
    int const isShrinker = (pSprite->picnum == APLAYER && PWEAPON(playerNum, g_player[playerNum].ps->curr_weapon, WorksLike) == SHRINKER_WEAPON);
    int const isFreezer  = (pSprite->picnum == APLAYER && PWEAPON(playerNum, g_player[playerNum].ps->curr_weapon, WorksLike) == FREEZE_WEAPON);
#endif

    vec2_t const d1 = { sintable[(spriteAng + 512 - projAng) & 2047], sintable[(spriteAng - projAng) & 2047] };
    vec2_t const d2 = { sintable[(spriteAng + 512 + projAng) & 2047], sintable[(spriteAng + projAng) & 2047] };
    vec2_t const d3 = { sintable[(spriteAng + 512) & 2047], sintable[spriteAng & 2047] };

    int lastDist   = INT32_MAX;
    int bestSprite = -1;

    for (bssize_t k=0; k<4; k++)
    {
        if (bestSprite >= 0)
            break;

        for (bssize_t spriteNum=headspritestat[aimstats[k]]; spriteNum >= 0; spriteNum=nextspritestat[spriteNum])
        {
            if ((sprite[spriteNum].xrepeat > 0 && sprite[spriteNum].extra >= 0 &&
                 (sprite[spriteNum].cstat & (257 + 32768)) == 257) &&
                (A_CheckEnemySprite(&sprite[spriteNum]) || k < 2))
            {
                if (A_CheckEnemySprite(&sprite[spriteNum]) || PN(spriteNum) == APLAYER)
                {
                    if (PN(spriteNum) == APLAYER && pSprite->picnum == APLAYER && pSprite != &sprite[spriteNum] &&
                        (GTFLAGS(GAMETYPE_PLAYERSFRIENDLY) ||
                         (GTFLAGS(GAMETYPE_TDM) && g_player[P_Get(spriteNum)].ps->team == g_player[playerNum].ps->team)))
                        continue;

#ifndef EDUKE32_STANDALONE
                    if (!FURY && ((isShrinker && sprite[spriteNum].xrepeat < 30
                        && (PN(spriteNum) == SHARK || !(PN(spriteNum) >= GREENSLIME && PN(spriteNum) <= GREENSLIME + 7)))
                        || (isFreezer && sprite[spriteNum].pal == 1)))
                        continue;
#endif
                }

                vec2_t const vd = { (SX(spriteNum) - pSprite->x), (SY(spriteNum) - pSprite->y) };

                if ((d1.y * vd.x <= d1.x * vd.y) && (d2.y * vd.x >= d2.x * vd.y))
                {
                    int const spriteDist = mulscale14(d3.x, vd.x) + mulscale14(d3.y, vd.y);

                    if (spriteDist > 512 && spriteDist < lastDist)
                    {
                        int onScreen = 1;

                        if (pSprite->picnum == APLAYER)
                        {
                            auto const ps = g_player[P_GetP(pSprite)].ps;
                            onScreen = (klabs(scale(SZ(spriteNum)-pSprite->z,10,spriteDist)-fix16_to_int(ps->q16horiz+ps->q16horizoff-F16(100))) < 100);
                        }

#ifndef EDUKE32_STANDALONE
                        int const zOffset = (!FURY && (PN(spriteNum) == ORGANTIC || PN(spriteNum) == ROTATEGUN)) ? 0 : ZOFFSET5;
#else
                        int const zOffset = ZOFFSET5;
#endif
                        int const canSee = cansee(SX(spriteNum), SY(spriteNum), SZ(spriteNum) - zOffset, SECT(spriteNum),
                                                  pSprite->x, pSprite->y, pSprite->z - ZOFFSET5, pSprite->sectnum);

                        if (onScreen && canSee)
                        {
                            lastDist   = spriteDist;
                            bestSprite = spriteNum;
                        }
                    }
                }
            }
        }
    }

    return bestSprite;
}

static void A_SetHitData(int spriteNum, const hitdata_t *hitData)
{
    actor[spriteNum].t_data[6] = hitData->wall;
    actor[spriteNum].t_data[7] = hitData->sect;
    actor[spriteNum].t_data[8] = hitData->sprite;
}

#ifndef EDUKE32_STANDALONE
static int CheckShootSwitchTile(int tileNum)
{
    if (FURY)
        return 0;

    return tileNum == DIPSWITCH || tileNum == DIPSWITCH + 1 || tileNum == DIPSWITCH2 || tileNum == DIPSWITCH2 + 1 ||
           tileNum == DIPSWITCH3 || tileNum == DIPSWITCH3 + 1 || tileNum == HANDSWITCH || tileNum == HANDSWITCH + 1;
}
#endif

static int32_t safeldist(int32_t spriteNum, const void *pSprite)
{
    int32_t distance = ldist(&sprite[spriteNum], pSprite);
    return distance ? distance : 1;
}

// flags:
//  1: do sprite center adjustment (cen-=(8<<8)) for GREENSLIME or ROTATEGUN
//  2: do auto getangle only if not RECON (if clear, do unconditionally)
static int GetAutoAimAng(int spriteNum, int playerNum, int projecTile, int zAdjust, int aimFlags,
                               const vec3_t *startPos, int projVel, int32_t *pZvel, int *pAng)
{
    int returnSprite = -1;

    Bassert((unsigned)playerNum < MAXPLAYERS);

    Gv_SetVar(g_aimAngleVarID, g_player[playerNum].ps->auto_aim == 3 ? AUTO_AIM_ANGLE<<1 : AUTO_AIM_ANGLE, spriteNum, playerNum);

    VM_OnEvent(EVENT_GETAUTOAIMANGLE, spriteNum, playerNum);

    int aimang = Gv_GetVar(g_aimAngleVarID, spriteNum, playerNum);
    if (aimang > 0)
        returnSprite = A_FindTargetSprite(&sprite[spriteNum], aimang, projecTile);

    if (returnSprite >= 0)
    {
        auto const pSprite = (uspriteptr_t)&sprite[returnSprite];
        int        zCenter = 2 * (pSprite->yrepeat * tilesiz[pSprite->picnum].y) + zAdjust;

        if (aimFlags &&
            (STANDALONE_EVAL(false, (pSprite->picnum >= GREENSLIME && pSprite->picnum <= GREENSLIME + 7) || pSprite->picnum == ROTATEGUN) || pSprite->cstat & CSTAT_SPRITE_YCENTER))
            zCenter -= ZOFFSET3;

        int spriteDist = safeldist(g_player[playerNum].ps->i, &sprite[returnSprite]);
        *pZvel         = tabledivide32_noinline((pSprite->z - startPos->z - zCenter) * projVel, spriteDist);

        if (!(aimFlags&2) || sprite[returnSprite].picnum != RECON)
            *pAng = getangle(pSprite->x-startPos->x, pSprite->y-startPos->y);
    }

    return returnSprite;
}

static void Proj_DirectSpawn(int spriteNum, int projecTile, const hitdata_t *hitData)
{
    if (projecTile >= 0)
    {
        int spawned = A_Spawn(spriteNum, projecTile);
        A_SetHitData(spawned, hitData);
    }
}

static void Proj_IndirectSpawn(int spriteNum, int projecTile, const hitdata_t *hitData)
{
    projectile_t *const pProj = Proj_GetProjectile(projecTile);
    if (pProj->spawns >= 0)
    {
        int spawned = A_Spawn(spriteNum, pProj->spawns);

        if (projecTile >= 0)
        {
            if (pProj->sxrepeat > 4)
                sprite[spawned].xrepeat = pProj->sxrepeat;

            if (pProj->syrepeat > 4)
                sprite[spawned].yrepeat = pProj->syrepeat;
        }

        A_SetHitData(spawned, hitData);
    }
}

static void Proj_Spawn(int spriteNum, uint16_t projecTile, const hitdata_t *hitData, bool directSpawn)
{
    if (directSpawn)
        Proj_DirectSpawn(spriteNum, projecTile, hitData);
    else
        Proj_IndirectSpawn(spriteNum, projecTile, hitData);
}

// <extra>: damage that this shotspark does
static int Proj_InsertShotspark(const hitdata_t *hitData, int spriteNum, int projecTile, int sparkSize, int sparkAng, int damage)
{
    int returnSprite = A_InsertSprite(hitData->sect, hitData->x, hitData->y, hitData->z, SHOTSPARK1, -15,
                                     sparkSize, sparkSize, sparkAng, 0, 0, spriteNum, 4);

    sprite[returnSprite].extra = damage;
    sprite[returnSprite].yvel  = projecTile;  // This is a hack to allow you to detect which weapon spawned a SHOTSPARK1

    A_SetHitData(returnSprite, hitData);

    return returnSprite;
}

int Proj_GetDamage(projectile_t const *pProj)
{
    Bassert(pProj);

    int damage = pProj->extra;

    if (pProj->extra_rand > 0)
        damage += (krand() % pProj->extra_rand);

    return damage;
}

static void Proj_MaybeAddSpread(int doSpread, int32_t *zvel, int *shootAng, int zRange, int angRange)
{
    if (doSpread)
    {
        // Ranges <= 1 mean no spread at all. A range of 1 calls krand() though.
        if (zRange > 0)
            *zvel += (zRange >> 1) - krand() % zRange;

        if (angRange > 0)
            *shootAng += (angRange >> 1) - krand() % angRange;
    }
}

static int g_overrideShootZvel = 0;  // a boolean
static int g_shootZvel;  // the actual zvel if the above is !=0

static int A_GetShootZvel(int defaultZvel)
{
    return g_overrideShootZvel ? g_shootZvel : defaultZvel;
}

// Prepare hitscan weapon fired from player p.
static void P_PreFireHitscan(int spriteNum, int playerNum, int projecTile, vec3_t *srcVect, int32_t *zvel, int *shootAng,
                             int accurateAim, int doSpread)
{
    int angRange  = 32;
    int zRange    = 256;
    int aimSprite = GetAutoAimAng(spriteNum, playerNum, projecTile, 5 << 8, 0 + 1, srcVect, 256, zvel, shootAng);

    auto const pPlayer = g_player[playerNum].ps;

    Gv_SetVar(g_angRangeVarID, angRange, spriteNum, playerNum);
    Gv_SetVar(g_zRangeVarID, zRange, spriteNum, playerNum);

    VM_OnEvent(EVENT_GETSHOTRANGE, spriteNum, playerNum);

    angRange = Gv_GetVar(g_angRangeVarID, spriteNum, playerNum);
    zRange   = Gv_GetVar(g_zRangeVarID, spriteNum, playerNum);

    if (accurateAim)
    {
        if (!pPlayer->auto_aim)
        {
            hitdata_t hitData;

            *zvel = A_GetShootZvel(fix16_to_int(F16(100)-pPlayer->q16horiz-pPlayer->q16horizoff)<<5);

            hitscan(srcVect, sprite[spriteNum].sectnum, sintable[(*shootAng + 512) & 2047],
                    sintable[*shootAng & 2047], *zvel << 6, &hitData, CLIPMASK1);

            if (hitData.sprite != -1)
            {
                int const statNumMap = ((1 << STAT_ACTOR) | (1 << STAT_ZOMBIEACTOR) | (1 << STAT_PLAYER) | (1 << STAT_DUMMYPLAYER));
                int const statNum    = sprite[hitData.sprite].statnum;

                if ((unsigned)statNum <= 30 && (statNumMap & (1 << statNum)))
                    aimSprite = hitData.sprite;
            }
        }

        if (aimSprite == -1)
            goto notarget;
    }
    else
    {
        if (aimSprite == -1)  // no target
        {
notarget:
            *zvel = fix16_to_int(F16(100)-pPlayer->q16horiz-pPlayer->q16horizoff)<<5;
        }

        Proj_MaybeAddSpread(doSpread, zvel, shootAng, zRange, angRange);
    }

    // ZOFFSET6 is added to this position at the same time as the player's pyoff in A_ShootWithZvel()
    srcVect->z -= ZOFFSET6;
}

// Hitscan weapon fired from actor (sprite s);
static void A_PreFireHitscan(const spritetype *pSprite, vec3_t * const srcVect, int32_t * const zvel, int * const shootAng, int const doSpread)
{
    int const  playerNum  = A_FindPlayer(pSprite, NULL);
    auto const pPlayer    = g_player[playerNum].ps;
    int const  playerDist = safeldist(pPlayer->i, pSprite);

    *zvel = tabledivide32_noinline((pPlayer->pos.z - srcVect->z) << 8, playerDist);

    srcVect->z -= ZOFFSET6;

    if (pSprite->picnum == BOSS1)
        *shootAng = getangle(pPlayer->pos.x - srcVect->x, pPlayer->pos.y - srcVect->y);

    Proj_MaybeAddSpread(doSpread, zvel, shootAng, 256, 128 >> (uint8_t)(pSprite->picnum != BOSS1));
}

static int Proj_DoHitscan(int spriteNum, int32_t const cstatmask, const vec3_t * const srcVect, int zvel, int const shootAng, hitdata_t * const hitData)
{
    auto const pSprite = &sprite[spriteNum];

    pSprite->cstat &= ~cstatmask;
    zvel = A_GetShootZvel(zvel);
    int16_t sectnum = pSprite->sectnum;
    updatesector(srcVect->x, srcVect->y, &sectnum);
    hitscan(srcVect, sectnum, sintable[(shootAng + 512) & 2047], sintable[shootAng & 2047], zvel << 6, hitData, CLIPMASK1);
    pSprite->cstat |= cstatmask;

    return (hitData->sect < 0);
}

static void Proj_DoRandDecalSize(int const spriteNum, int const projecTile)
{
    const projectile_t *const proj    = Proj_GetProjectile(projecTile);
    auto const         pSprite = &sprite[spriteNum];

    if (proj->workslike & PROJECTILE_RANDDECALSIZE)
        pSprite->xrepeat = pSprite->yrepeat = clamp((krand() & proj->xrepeat), pSprite->yrepeat, pSprite->xrepeat);
    else
    {
        pSprite->xrepeat = proj->xrepeat;
        pSprite->yrepeat = proj->yrepeat;
    }
}

static int SectorContainsSE13(int const sectNum)
{
    if (sectNum >= 0)
    {
        for (bssize_t SPRITES_OF_SECT(sectNum, i))
        {
            if (sprite[i].statnum == STAT_EFFECTOR && sprite[i].lotag == SE_13_EXPLOSIVE)
                return 1;
        }
    }
    return 0;
}

// Maybe handle bit 2 (swap wall bottoms).
// (in that case walltype *hitwal may be stale)
static inline void HandleHitWall(hitdata_t *hitData)
{
    auto const hitWall = (uwallptr_t)&wall[hitData->wall];

    if ((hitWall->cstat & 2) && redwallp(hitWall) && (hitData->z >= sector[hitWall->nextsector].floorz))
        hitData->wall = hitWall->nextwall;
}

// Maybe damage a ceiling or floor as the consequence of projectile impact.
// Returns 1 if projectile hit a parallaxed ceiling.
// NOTE: Compare with Proj_MaybeDamageCF() in actors.c
static int Proj_MaybeDamageCF2(int const spriteNum, int const zvel, int const hitSect)
{
    Bassert(hitSect >= 0);

    if (zvel < 0)
    {
        if (sector[hitSect].ceilingstat&1)
            return 1;

        Sect_DamageCeiling(spriteNum, hitSect);
    }
    else if (zvel > 0)
    {
        if (sector[hitSect].floorstat&1)
        {
            // Keep original Duke3D behavior: pass projectiles through
            // parallaxed ceilings, but NOT through such floors.
            return 0;
        }

        Sect_DamageFloor(spriteNum, hitSect);
    }

    return 0;
}

static void P_DoWeaponRumble(int playerNum)
{
    if (!joystick.hasRumble || !ud.config.controllerRumble)
        return;

    auto const pPlayer = g_player[playerNum].ps;

    int const shoots = PWEAPON(playerNum, pPlayer->curr_weapon, Shoots);
    int const base   = A_CheckSpriteTileFlags(shoots, SFLAG_PROJECTILE) ? Proj_GetProjectile(shoots)->extra : G_DefaultActorHealthForTile(shoots);
    int const dmg    = clamp(base * PWEAPON(playerNum, pPlayer->curr_weapon, ShotsPerBurst), FF_WEAPON_DMG_MIN, FF_WEAPON_DMG_MAX);

    I_AddForceFeedback((dmg << FF_WEAPON_DMG_SCALE), (dmg << FF_WEAPON_DMG_SCALE), max<int>(FF_WEAPON_MAX_TIME, dmg << FF_WEAPON_TIME_SCALE));
}

// Finish shooting hitscan weapon from player <p>. <k> is the inserted SHOTSPARK1.
// * <spawnObject> is passed to Proj_MaybeSpawn()
// * <decalTile> and <wallDamage> are for wall impact
// * <wallDamage> is passed to A_DamageWall()
// * <decalFlags> is for decals upon wall impact:
//    1: handle random decal size (tile <atwith>)
//    2: set cstat to wall-aligned + random x/y flip
//
// TODO: maybe split into 3 cases (hit neither wall nor sprite, hit sprite, hit wall)?

static int P_PostFireHitscan(int playerNum, int const spriteNum, hitdata_t *const hitData, int const STANDALONE_UNUSED(spriteOwner),
                             int const projecTile, int const zvel, int const spawnTile, int const decalTile, int const wallDamage,
                             int const decalFlags, bool directProjecTileSpawn)
{
#ifdef EDUKE32_STANDALONE
    UNREFERENCED_PARAMETER(playerNum);
    UNREFERENCED_CONST_PARAMETER(spriteOwner);
#endif
    if (hitData->wall == -1 && hitData->sprite == -1)
    {
        if (Proj_MaybeDamageCF2(spriteNum, zvel, hitData->sect))
        {
            sprite[spriteNum].xrepeat = 0;
            sprite[spriteNum].yrepeat = 0;
            return -1;
        }

        Proj_Spawn(spriteNum, spawnTile, hitData, directProjecTileSpawn);
    }
    else if (hitData->sprite >= 0)
    {
        A_DamageObject(hitData->sprite, spriteNum);

#ifndef EDUKE32_STANDALONE
        if (!FURY && sprite[hitData->sprite].picnum == APLAYER &&
            (ud.ffire == 1 || (!GTFLAGS(GAMETYPE_PLAYERSFRIENDLY) && GTFLAGS(GAMETYPE_TDM) &&
                               g_player[P_Get(hitData->sprite)].ps->team != g_player[P_Get(spriteOwner)].ps->team)))
        {
            int jibSprite = A_Spawn(spriteNum, JIBS6);

            sprite[spriteNum].xrepeat = sprite[spriteNum].yrepeat = 0;
            sprite[jibSprite].z += ZOFFSET6;
            sprite[jibSprite].xvel    = 16;
            sprite[jibSprite].xrepeat = sprite[jibSprite].yrepeat = 24;
            sprite[jibSprite].ang += 64 - (krand() & 127);
        }
        else
#endif
        {
            Proj_Spawn(spriteNum, spawnTile, hitData, directProjecTileSpawn);
        }
#ifndef EDUKE32_STANDALONE
        if (!FURY && playerNum >= 0 && CheckShootSwitchTile(sprite[hitData->sprite].picnum))
        {
            P_ActivateSwitch(playerNum, hitData->sprite, 1);
            return -1;
        }
#endif
    }
    else if (hitData->wall >= 0)
    {
        auto const hitWall = (uwallptr_t)&wall[hitData->wall];

        Proj_Spawn(spriteNum, spawnTile, hitData, directProjecTileSpawn);

        if (CheckDoorTile(hitWall->picnum) == 1)
            goto SKIPBULLETHOLE;

#ifndef EDUKE32_STANDALONE
        if (!FURY && playerNum >= 0 && CheckShootSwitchTile(hitWall->picnum))
        {
            P_ActivateSwitch(playerNum, hitData->wall, 0);
            return -1;
        }
#endif

        if (hitWall->hitag != 0 || (hitWall->nextwall >= 0 && wall[hitWall->nextwall].hitag != 0))
            goto SKIPBULLETHOLE;

        if ((hitData->sect >= 0 && sector[hitData->sect].lotag == 0) &&
            (hitWall->overpicnum != BIGFORCE && (hitWall->cstat & 16) == 0) &&
            ((hitWall->nextsector >= 0 && sector[hitWall->nextsector].lotag == 0) || (hitWall->nextsector == -1 && sector[hitData->sect].lotag == 0)))
        {
            int decalSprite;

            if (SectorContainsSE13(hitWall->nextsector))
                goto SKIPBULLETHOLE;

            for (SPRITES_OF(STAT_MISC, decalSprite))
                if (sprite[decalSprite].picnum == decalTile && dist(&sprite[decalSprite], &sprite[spriteNum]) < (12 + (krand() & 7)))
                    goto SKIPBULLETHOLE;

            if (decalTile >= 0)
            {
                decalSprite = A_Spawn(spriteNum, decalTile);

                auto const decal = &sprite[decalSprite];

                A_SetHitData(decalSprite, hitData);

                if (!A_CheckSpriteFlags(decalSprite, SFLAG_DECAL))
                    actor[decalSprite].flags |= SFLAG_DECAL;

                int32_t diffZ;
                spriteheightofs(decalSprite, &diffZ, 0);

                decal->z += diffZ >> 1;
                decal->ang = (getangle(hitWall->x - wall[hitWall->point2].x, hitWall->y - wall[hitWall->point2].y) + 1536) & 2047;

                if (decalFlags & 1)
                    Proj_DoRandDecalSize(decalSprite, projecTile);

                if (decalFlags & 2)
                    decal->cstat = 16 + (krand() & (8 + 4));

                A_SetSprite(decalSprite, CLIPMASK0);

                // BULLETHOLE already adds itself to the deletion queue in
                // A_Spawn(). However, some other tiles do as well.
                if (decalTile != BULLETHOLE)
                    A_AddToDeleteQueue(decalSprite);
            }
        }

SKIPBULLETHOLE:
        HandleHitWall(hitData);
        A_DamageWall(spriteNum, hitData->wall, hitData->xyz, wallDamage);
    }

    return 0;
}

// Finish shooting hitscan weapon from actor (sprite <i>).
static int A_PostFireHitscan(const hitdata_t *hitData, int const spriteNum, int const projecTile, int const zvel, int const shootAng,
                             int const extra, int const spawnTile, int const wallDamage, bool directProjecTileSpawn)
{
    int const returnSprite = Proj_InsertShotspark(hitData, spriteNum, projecTile, 24, shootAng, extra);

    if (hitData->sprite >= 0)
    {
        A_DamageObject(hitData->sprite, returnSprite);

        if (sprite[hitData->sprite].picnum != APLAYER)
            Proj_Spawn(returnSprite, spawnTile, hitData, directProjecTileSpawn);
        else
            sprite[returnSprite].xrepeat = sprite[returnSprite].yrepeat = 0;
    }
    else if (hitData->wall >= 0)
    {
        A_DamageWall(returnSprite, hitData->wall, hitData->xyz, wallDamage);
        Proj_Spawn(returnSprite, spawnTile, hitData, directProjecTileSpawn);
    }
    else
    {
        if (Proj_MaybeDamageCF2(returnSprite, zvel, hitData->sect))
        {
            sprite[returnSprite].xrepeat = 0;
            sprite[returnSprite].yrepeat = 0;
        }
        else Proj_Spawn(returnSprite, spawnTile, hitData, directProjecTileSpawn);
    }

    return returnSprite;
}

// Common "spawn blood?" predicate.
// minzdiff: minimal "step" height for blood to be spawned
static int Proj_CheckBlood(vec3_t const *const srcVect, hitdata_t const *const hitData, int const bloodRange, int const minZdiff)
{
    if (hitData->wall < 0 || hitData->sect < 0)
        return 0;

    auto const hitWall = (uwallptr_t)&wall[hitData->wall];

    if ((FindDistance2D(srcVect->x - hitData->x, srcVect->y - hitData->y) < bloodRange)
        && (hitWall->overpicnum != BIGFORCE && (hitWall->cstat & 16) == 0)
        && (sector[hitData->sect].lotag == 0)
        && (hitWall->nextsector < 0 || (sector[hitWall->nextsector].lotag == 0 && sector[hitData->sect].lotag == 0
                                        && sector[hitData->sect].floorz - sector[hitWall->nextsector].floorz > minZdiff)))
        return 1;

    return 0;
}

static void Proj_HandleKnee(hitdata_t *const hitData, int const spriteNum, int const playerNum, int const projecTile, int const shootAng,
                            const projectile_t *const proj, int const inserttile, int const randomDamage, int const spawnTile,
                            int const soundNum)
{
    auto const pPlayer = playerNum >= 0 ? g_player[playerNum].ps : NULL;

    int kneeSprite = A_InsertSprite(hitData->sect,hitData->x,hitData->y,hitData->z,
                                    inserttile,-15,0,0,shootAng,32,0,spriteNum,4);

    if (proj != NULL)
    {
        // Custom projectiles.
        SpriteProjectile[kneeSprite].workslike = Proj_GetProjectile(sprite[kneeSprite].picnum)->workslike;
        sprite[kneeSprite].extra = proj->extra;
    }

    if (randomDamage > 0)
        sprite[kneeSprite].extra += (krand()&randomDamage);

    if (playerNum >= 0)
    {
        if (spawnTile >= 0)
        {
            int k = A_Spawn(kneeSprite, spawnTile);
            sprite[k].z -= ZOFFSET3;
            A_SetHitData(k, hitData);
        }

        if (soundNum >= 0)
            A_PlaySound(soundNum, kneeSprite);
    }

    if (pPlayer != NULL && pPlayer->inv_amount[GET_STEROIDS] > 0 && pPlayer->inv_amount[GET_STEROIDS] < 400)
        sprite[kneeSprite].extra += (pPlayer->max_player_health>>2);

    int const dmg = clamp<int>(sprite[kneeSprite].extra, FF_WEAPON_DMG_MIN, FF_WEAPON_DMG_MAX);

    if (hitData->sprite >= 0 && sprite[hitData->sprite].picnum != ACCESSSWITCH && sprite[hitData->sprite].picnum != ACCESSSWITCH2)
    {
        I_AddForceFeedback((dmg << FF_WEAPON_DMG_SCALE), (dmg << FF_WEAPON_DMG_SCALE), max<int>(FF_WEAPON_MAX_TIME, dmg << FF_WEAPON_TIME_SCALE));
        A_DamageObject(hitData->sprite, kneeSprite);
        if (playerNum >= 0)
            P_ActivateSwitch(playerNum, hitData->sprite,1);
    }
    else if (hitData->wall >= 0)
    {
        I_AddForceFeedback((dmg << FF_WEAPON_DMG_SCALE), (dmg << FF_WEAPON_DMG_SCALE), max<int>(FF_WEAPON_MAX_TIME, dmg << FF_WEAPON_TIME_SCALE));
        HandleHitWall(hitData);

        if (wall[hitData->wall].picnum != ACCESSSWITCH && wall[hitData->wall].picnum != ACCESSSWITCH2)
        {
            A_DamageWall(kneeSprite, hitData->wall, hitData->xyz, projecTile);
            if (playerNum >= 0)
                P_ActivateSwitch(playerNum, hitData->wall,0);
        }
    }
}

#define MinibossScale(i, s) (((s)*sprite[i].yrepeat)/80)

static int A_ShootCustom(int const spriteNum, int const projecTile, int shootAng, vec3_t * const startPos)
{
    /* Custom projectiles */
    hitdata_t           hitData;
    projectile_t *const pProj     = Proj_GetProjectile(projecTile);
    auto const   pSprite   = &sprite[spriteNum];
    int const           playerNum = (pSprite->picnum == APLAYER) ? P_GetP(pSprite) : -1;
    auto const pPlayer   = playerNum >= 0 ? g_player[playerNum].ps : NULL;

#ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER && pProj->flashcolor)
    {
        vec3_t const offset = { -((sintable[(pSprite->ang+512)&2047])>>7), -((sintable[(pSprite->ang)&2047])>>7), PHEIGHT };
        G_AddGameLight(spriteNum, pSprite->sectnum, offset, 8192, 0, 100, pProj->flashcolor, PR_LIGHT_PRIO_MAX_GAME);
        practor[spriteNum].lightcount = 2;
    }
#endif // POLYMER

    if (pProj->offset == 0)
        pProj->offset = 1;

    int     otherSprite = -1;
    int32_t zvel = 0;

    switch (pProj->workslike & PROJECTILE_TYPE_MASK)
    {
    case PROJECTILE_HITSCAN:
        if (!(pProj->workslike & PROJECTILE_NOSETOWNERSHADE) && pSprite->extra >= 0)
            pSprite->shade = pProj->shade;

        if (playerNum >= 0)
            P_PreFireHitscan(spriteNum, playerNum, projecTile, startPos, &zvel, &shootAng,
                             pProj->workslike & PROJECTILE_ACCURATE_AUTOAIM, !(pProj->workslike & PROJECTILE_ACCURATE));
        else
            A_PreFireHitscan(pSprite, startPos, &zvel, &shootAng, !(pProj->workslike & PROJECTILE_ACCURATE));

        if (Proj_DoHitscan(spriteNum, (pProj->cstat >= 0) ? pProj->cstat : 256 + 1, startPos, zvel, shootAng, &hitData))
            return -1;

        if (pProj->range > 0 && klabs(startPos->x - hitData.x) + klabs(startPos->y - hitData.y) > pProj->range)
            return -1;

        if (pProj->trail >= 0)
            A_HitscanProjTrail(startPos, &hitData.xyz, shootAng, projecTile, pSprite->sectnum);

        if (pProj->workslike & PROJECTILE_WATERBUBBLES)
        {
            if ((krand() & 15) == 0 && sector[hitData.sect].lotag == ST_2_UNDERWATER)
                Proj_DoWaterTracers(hitData.xyz, startPos, 8 - (ud.multimode >> 1), pSprite->sectnum);
        }

        if (playerNum >= 0)
        {
            otherSprite = Proj_InsertShotspark(&hitData, spriteNum, projecTile, 10, shootAng, Proj_GetDamage(pProj));

            if (P_PostFireHitscan(playerNum, otherSprite, &hitData, spriteNum, projecTile, zvel, projecTile, pProj->decal,
                                  projecTile, 1 + 2, false) < 0)
                return -1;
        }
        else
        {
            otherSprite =
            A_PostFireHitscan(&hitData, spriteNum, projecTile, zvel, shootAng, Proj_GetDamage(pProj), projecTile, projecTile, false);
        }

        if ((krand() & 255) < 4 && pProj->isound >= 0)
            S_PlaySound3D(pProj->isound, otherSprite, hitData.xyz);

        return -1;

    case PROJECTILE_RPG:
        if (!(pProj->workslike & PROJECTILE_NOSETOWNERSHADE) && pSprite->extra >= 0)
            pSprite->shade = pProj->shade;

        if (pPlayer != NULL)
        {
            // NOTE: j is a SPRITE_INDEX
            otherSprite = GetAutoAimAng(spriteNum, playerNum, projecTile, 8<<8, 0+2, startPos, pProj->vel, &zvel, &shootAng);

            if (otherSprite < 0)
                zvel = fix16_to_int(F16(100)-pPlayer->q16horiz-pPlayer->q16horizoff)*(pProj->vel/8);

            if (pProj->sound >= 0)
                A_PlaySound(pProj->sound, spriteNum);
        }
        else
        {
            if (!(pProj->workslike & PROJECTILE_NOAIM))
            {
                int const otherPlayer     = A_FindPlayer(pSprite, NULL);
                int const otherPlayerDist = safeldist(g_player[otherPlayer].ps->i, pSprite);

                shootAng = getangle(g_player[otherPlayer].ps->opos.x - startPos->x,
                                      g_player[otherPlayer].ps->opos.y - startPos->y);

                zvel = tabledivide32_noinline((g_player[otherPlayer].ps->opos.z - startPos->z) * pProj->vel, otherPlayerDist);

                if (A_CheckEnemySprite(pSprite) && (AC_MOVFLAGS(pSprite, &actor[spriteNum]) & face_player_smart))
                    shootAng = pSprite->ang + (krand() & 31) - 16;
            }
        }

        if (numplayers > 1 && g_netClient) return -1;
        else
        {
            // l may be a SPRITE_INDEX, see above
            int const l = (playerNum >= 0 && otherSprite >= 0) ? otherSprite : -1;

            zvel = A_GetShootZvel(zvel);
            otherSprite = A_InsertSprite(pSprite->sectnum,
                startPos->x + tabledivide32_noinline(sintable[(348 + shootAng + 512) & 2047], pProj->offset),
                startPos->y + tabledivide32_noinline(sintable[(shootAng + 348) & 2047], pProj->offset),
                startPos->z - (1 << 8), projecTile, 0, 14, 14, shootAng, pProj->vel, zvel, spriteNum, 4);

            sprite[otherSprite].extra = Proj_GetDamage(pProj);

            if (!(pProj->workslike & PROJECTILE_BOUNCESOFFWALLS))
                sprite[otherSprite].yvel = l;  // NOT_BOUNCESOFFWALLS_YVEL
            else
            {
                sprite[otherSprite].yvel = (pProj->bounces >= 1) ? pProj->bounces : g_numFreezeBounces;
                sprite[otherSprite].zvel -= (2 << 4);
            }

            sprite[otherSprite].pal       = (pProj->pal >= 0) ? pProj->pal : 0;
            sprite[otherSprite].xrepeat   = pProj->xrepeat;
            sprite[otherSprite].yrepeat   = pProj->yrepeat;
            sprite[otherSprite].cstat     = (pProj->cstat >= 0) ? pProj->cstat : 128;
            sprite[otherSprite].clipdist  = (pProj->clipdist != 255) ? pProj->clipdist : 40;
            SpriteProjectile[otherSprite] = *Proj_GetProjectile(sprite[otherSprite].picnum);

            return otherSprite;
        }

    case PROJECTILE_KNEE:
        if (playerNum >= 0)
        {
            zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) << 5;
            startPos->z += (6 << 8);
            shootAng += 15;
        }
        else if (!(pProj->workslike & PROJECTILE_NOAIM))
        {
            int32_t playerDist;
            otherSprite = g_player[A_FindPlayer(pSprite, &playerDist)].ps->i;
            zvel = tabledivide32_noinline((sprite[otherSprite].z - startPos->z) << 8, playerDist + 1);
            shootAng = getangle(sprite[otherSprite].x - startPos->x, sprite[otherSprite].y - startPos->y);
        }

        Proj_DoHitscan(spriteNum, 0, startPos, zvel, shootAng, &hitData);

        if (hitData.sect < 0) return -1;

        if (pProj->range == 0)
            pProj->range = 1024;

        if (pProj->range > 0 && klabs(startPos->x - hitData.x) + klabs(startPos->y - hitData.y) > pProj->range)
            return -1;

        Proj_HandleKnee(&hitData, spriteNum, playerNum, projecTile, shootAng,
                        pProj, projecTile, pProj->extra_rand, pProj->spawns, pProj->sound);

        return -1;

    case PROJECTILE_BLOOD:
        shootAng += 64 - (krand() & 127);

        if (playerNum < 0)
            shootAng += 1024;

        zvel = 1024 - (krand() & 2047);

        Proj_DoHitscan(spriteNum, 0, startPos, zvel, shootAng, &hitData);

        if (pProj->range == 0)
            pProj->range = 1024;

        if (Proj_CheckBlood(startPos, &hitData, pProj->range, mulscale3(pProj->yrepeat, tilesiz[pProj->decal].y) << 8))
        {
            uwallptr_t const hitWall = (uwallptr_t)&wall[hitData.wall];

            if (FindDistance2D(hitWall->x - wall[hitWall->point2].x, hitWall->y - wall[hitWall->point2].y) >
                (mulscale3(pProj->xrepeat + 8, tilesiz[pProj->decal].x)))
            {
                if (SectorContainsSE13(hitWall->nextsector))
                    return -1;

                if (hitWall->nextwall >= 0 && wall[hitWall->nextwall].hitag != 0)
                    return -1;

                if (hitWall->hitag == 0 && pProj->decal >= 0)
                {
                    otherSprite = A_Spawn(spriteNum, pProj->decal);

                    A_SetHitData(otherSprite, &hitData);

                    if (!A_CheckSpriteFlags(otherSprite, SFLAG_DECAL))
                        actor[otherSprite].flags |= SFLAG_DECAL;

                    sprite[otherSprite].ang = getangle(hitWall->x - wall[hitWall->point2].x,
                        hitWall->y - wall[hitWall->point2].y) + 512;
                    sprite[otherSprite].xyz = hitData.xyz;

                    Proj_DoRandDecalSize(otherSprite, projecTile);

                    sprite[otherSprite].z += sprite[otherSprite].yrepeat << 8;

                    //                                sprite[spawned].cstat = 16+(krand()&12);
                    sprite[otherSprite].cstat = 16;

                    if (krand() & 1)
                        sprite[otherSprite].cstat |= 4;

                    if (krand() & 1)
                        sprite[otherSprite].cstat |= 8;

                    sprite[otherSprite].shade = sector[sprite[otherSprite].sectnum].floorshade;

                    A_SetSprite(otherSprite, CLIPMASK0);
                    A_AddToDeleteQueue(otherSprite);
                    changespritestat(otherSprite, 5);
                }
            }
        }

        return -1;

    default:
        return -1;
    }
}

#ifndef EDUKE32_STANDALONE
static int32_t A_ShootHardcoded(int spriteNum, int projecTile, int shootAng, vec3_t startPos,
                                spritetype *pSprite, int const playerNum, DukePlayer_t * const pPlayer)
{
    hitdata_t hitData;
    int const spriteSectnum = pSprite->sectnum;
    int32_t Zvel;
    int vel;

    switch (tileGetMapping(projecTile))
    {
        case BLOODSPLAT1__:
        case BLOODSPLAT2__:
        case BLOODSPLAT3__:
        case BLOODSPLAT4__:
            shootAng += 64 - (krand() & 127);
            if (playerNum < 0)
                shootAng += 1024;
            Zvel = 1024 - (krand() & 2047);
            fallthrough__;
        case KNEE__:
            if (projecTile == KNEE)
            {
                if (playerNum >= 0)
                {
                    Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) << 5;
                    startPos.z += (6 << 8);
                    shootAng += 15;
                }
                else
                {
                    int32_t   playerDist;
                    int const playerSprite = g_player[A_FindPlayer(pSprite, &playerDist)].ps->i;
                    Zvel                   = tabledivide32_noinline((sprite[playerSprite].z - startPos.z) << 8, playerDist + 1);
                    shootAng             = getangle(sprite[playerSprite].x - startPos.x, sprite[playerSprite].y - startPos.y);
                }
            }

            Proj_DoHitscan(spriteNum, 0, &startPos, Zvel, shootAng, &hitData);

            if (projecTile >= BLOODSPLAT1 && projecTile <= BLOODSPLAT4)
            {
                if (Proj_CheckBlood(&startPos, &hitData, 1024, 16 << 8))
                {
                    uwallptr_t const hitwal = (uwallptr_t)&wall[hitData.wall];

                    if (SectorContainsSE13(hitwal->nextsector))
                        return -1;

                    if (hitwal->nextwall >= 0 && wall[hitwal->nextwall].hitag != 0)
                        return -1;

                    if (hitwal->hitag == 0)
                    {
                        int const spawnedSprite = A_Spawn(spriteNum, projecTile);
                        sprite[spawnedSprite].ang
                        = (getangle(hitwal->x - wall[hitwal->point2].x, hitwal->y - wall[hitwal->point2].y) + 1536) & 2047;
                        sprite[spawnedSprite].xyz = hitData.xyz;
                        sprite[spawnedSprite].cstat |= (krand() & 4);
                        A_SetSprite(spawnedSprite, CLIPMASK0);
                        setsprite(spawnedSprite, &sprite[spawnedSprite].xyz);
                        if (PN(spriteNum) == OOZFILTER || PN(spriteNum) == NEWBEAST)
                            sprite[spawnedSprite].pal = 6;
                    }
                }

                return -1;
            }

            if (hitData.sect < 0)
                break;

            if (klabs(startPos.x - hitData.x) + klabs(startPos.y - hitData.y) < 1024)
                Proj_HandleKnee(&hitData, spriteNum, playerNum, projecTile, shootAng, NULL, KNEE, 7, SMALLSMOKE, KICK_HIT);
            break;

        case SHOTSPARK1__:
        case SHOTGUN__:
        case CHAINGUN__:
        {
            if (pSprite->extra >= 0)
                pSprite->shade = -96;

            if (playerNum >= 0)
                P_PreFireHitscan(spriteNum, playerNum, projecTile, &startPos, &Zvel, &shootAng,
                    projecTile == SHOTSPARK1__ && !WW2GI, 1);
            else
                A_PreFireHitscan(pSprite, &startPos, &Zvel, &shootAng, 1);

            if (Proj_DoHitscan(spriteNum, 256 + 1, &startPos, Zvel, shootAng, &hitData))
                return -1;

            if ((krand() & 15) == 0 && sector[hitData.sect].lotag == ST_2_UNDERWATER)
                Proj_DoWaterTracers(hitData.xyz, &startPos, 8 - (ud.multimode >> 1), pSprite->sectnum);

            int spawnedSprite;

            if (playerNum >= 0)
            {
                spawnedSprite = Proj_InsertShotspark(&hitData, spriteNum, projecTile, 10, shootAng, G_DefaultActorHealthForTile(projecTile) + (krand() % 6));

                if (P_PostFireHitscan(playerNum, spawnedSprite, &hitData, spriteNum, projecTile, Zvel, SMALLSMOKE, BULLETHOLE, SHOTSPARK1, 0, true) < 0)
                    return -1;
            }
            else
            {
                spawnedSprite = A_PostFireHitscan(&hitData, spriteNum, projecTile, Zvel, shootAng, G_DefaultActorHealthForTile(projecTile), SMALLSMOKE, SHOTSPARK1, true);
            }

            if ((krand() & 255) < 4)
                S_PlaySound3D(PISTOL_RICOCHET, spawnedSprite, hitData.xyz);

            return -1;
        }

        case GROWSPARK__:
        {
            if (playerNum >= 0)
                P_PreFireHitscan(spriteNum, playerNum, projecTile, &startPos, &Zvel, &shootAng, 1, 1);
            else
                A_PreFireHitscan(pSprite, &startPos, &Zvel, &shootAng, 1);

            if (Proj_DoHitscan(spriteNum, 256 + 1, &startPos, Zvel, shootAng, &hitData))
                return -1;

            int const otherSprite = A_InsertSprite(hitData.sect, hitData.x, hitData.y, hitData.z, GROWSPARK, -16, 28, 28,
                                                   shootAng, 0, 0, spriteNum, 1);

            sprite[otherSprite].pal = 2;
            sprite[otherSprite].cstat |= 130;
            sprite[otherSprite].xrepeat = sprite[otherSprite].yrepeat = 1;
            A_SetHitData(otherSprite, &hitData);

            if (hitData.wall == -1 && hitData.sprite == -1 && hitData.sect >= 0)
            {
                Proj_MaybeDamageCF2(otherSprite, Zvel, hitData.sect);
            }
            else if (hitData.sprite >= 0)
                A_DamageObject(hitData.sprite, otherSprite);
            else if (hitData.wall >= 0 && wall[hitData.wall].picnum != ACCESSSWITCH && wall[hitData.wall].picnum != ACCESSSWITCH2)
                A_DamageWall(otherSprite, hitData.wall, hitData.xyz, projecTile);
        }
        break;

        case FIREBALL__:
            if (!WORLDTOUR)
                break;
            fallthrough__;
        case FIRELASER__:
        case SPIT__:
        case COOLEXPLOSION1__:
        {
            if (pSprite->extra >= 0)
                pSprite->shade = -96;

            switch (projecTile)
            {
                case SPIT__: vel = 292; break;
                case COOLEXPLOSION1__:
                    vel = (pSprite->picnum == BOSS2) ? 644 : 348;
                    startPos.z -= (4 << 7);
                    break;
                case FIREBALL__:
                    if (pSprite->picnum == BOSS5 || pSprite->picnum == BOSS5STAYPUT)
                    {
                        vel = 968;
                        startPos.z += 0x1800;
                        break;
                    }
                    fallthrough__;
                case FIRELASER__:
                default:
                    vel = 840;
                    startPos.z -= (4 << 7);
                    break;
            }

            if (playerNum >= 0)
            {
                if (projecTile == FIREBALL)
                {
                    Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 98;
                    startPos.x += sintable[(348+shootAng+512)&2047]/448;
                    startPos.y += sintable[(348+shootAng)&2047]/448;
                    startPos.z += 0x300;
                }
                else if (GetAutoAimAng(spriteNum, playerNum, projecTile, -ZOFFSET4, 0, &startPos, vel, &Zvel, &shootAng) < 0)
                    Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 98;
            }
            else
            {
                int const otherPlayer = A_FindPlayer(pSprite, NULL);
                shootAng           += 16 - (krand() & 31);
                hitData.x         = safeldist(g_player[otherPlayer].ps->i, pSprite);
                Zvel                  = tabledivide32_noinline((g_player[otherPlayer].ps->opos.z - startPos.z + (3 << 8)) * vel, hitData.x);
            }

            Zvel = A_GetShootZvel(Zvel);

            int spriteSize = (playerNum >= 0) ? 7 : 18;

            if (projecTile == SPIT)
            {
                spriteSize = 18;
                startPos.z -= (10 << 8);
            }

            int const returnSprite = A_InsertSprite(spriteSectnum, startPos.x, startPos.y, startPos.z, projecTile, -127, spriteSize, spriteSize,
                                                    shootAng, vel, Zvel, spriteNum, 4);

            sprite[returnSprite].extra += (krand() & 7);

            if (projecTile == COOLEXPLOSION1)
            {
                sprite[returnSprite].shade = 0;

                if (PN(spriteNum) == BOSS2)
                {
                    int const saveXvel        = sprite[returnSprite].xvel;
                    sprite[returnSprite].xvel = MinibossScale(spriteNum, 1024);
                    A_SetSprite(returnSprite, CLIPMASK0);
                    sprite[returnSprite].xvel = saveXvel;
                    sprite[returnSprite].ang += 128 - (krand() & 255);
                }
            }
            else if (projecTile == FIREBALL)
            {
                if (PN(spriteNum) == BOSS5 || PN(spriteNum) == BOSS5STAYPUT || playerNum >= 0)
                {
                    sprite[returnSprite].xrepeat = 40;
                    sprite[returnSprite].yrepeat = 40;
                }
                sprite[returnSprite].yvel = playerNum;
                //sprite[returnSprite].cstat |= 0x4000;
            }

            sprite[returnSprite].cstat    = 128;
            sprite[returnSprite].clipdist = 4;

            return returnSprite;
        }

        case FREEZEBLAST__:
            startPos.z += (3 << 8);
            fallthrough__;
        case RPG__:
        {
            // XXX: "CODEDUP"
            if (pSprite->extra >= 0)
                pSprite->shade = -96;

            vel = 644;

            int j = -1;

            if (playerNum >= 0)
            {
                // NOTE: j is a SPRITE_INDEX
                j = GetAutoAimAng(spriteNum, playerNum, projecTile, ZOFFSET3, 0 + 2, &startPos, vel, &Zvel, &shootAng);

                if (j < 0)
                    Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 81;

                if (projecTile == RPG)
                    A_PlaySound(RPG_SHOOT, spriteNum);
            }
            else
            {
                // NOTE: j is a player index
                j          = A_FindPlayer(pSprite, NULL);
                shootAng = getangle(g_player[j].ps->opos.x - startPos.x, g_player[j].ps->opos.y - startPos.y);
                if (PN(spriteNum) == BOSS3)
                    startPos.z -= MinibossScale(spriteNum, ZOFFSET5);
                else if (PN(spriteNum) == BOSS2)
                {
                    vel += 128;
                    startPos.z += MinibossScale(spriteNum, 24 << 8);
                }

                Zvel = tabledivide32_noinline((g_player[j].ps->opos.z - startPos.z) * vel, safeldist(g_player[j].ps->i, pSprite));

                if (A_CheckEnemySprite(pSprite) && (AC_MOVFLAGS(pSprite, &actor[spriteNum]) & face_player_smart))
                    shootAng = pSprite->ang + (krand() & 31) - 16;
            }

            if (numplayers > 1 && g_netClient)
                return -1;

            Zvel                   = A_GetShootZvel(Zvel);
            int const returnSprite = A_InsertSprite(spriteSectnum, startPos.x + (sintable[(348 + shootAng + 512) & 2047] / 448),
                                                    startPos.y + (sintable[(shootAng + 348) & 2047] / 448), startPos.z - (1 << 8),
                                                    projecTile, 0, 14, 14, shootAng, vel, Zvel, spriteNum, 4);
            auto const pReturn = &sprite[returnSprite];

            pReturn->extra += (krand() & 7);
            if (projecTile != FREEZEBLAST)
                pReturn->yvel = (playerNum >= 0 && j >= 0) ? j : -1;  // RPG_YVEL
            else
            {
                pReturn->yvel = g_numFreezeBounces;
                pReturn->xrepeat >>= 1;
                pReturn->yrepeat >>= 1;
                pReturn->zvel -= (2 << 4);
            }

            if (playerNum == -1)
            {
                if (PN(spriteNum) == BOSS3)
                {
                    if (krand() & 1)
                    {
                        pReturn->x -= MinibossScale(spriteNum, sintable[shootAng & 2047] >> 6);
                        pReturn->y -= MinibossScale(spriteNum, sintable[(shootAng + 1024 + 512) & 2047] >> 6);
                        pReturn->ang -= MinibossScale(spriteNum, 8);
                    }
                    else
                    {
                        pReturn->x += MinibossScale(spriteNum, sintable[shootAng & 2047] >> 6);
                        pReturn->y += MinibossScale(spriteNum, sintable[(shootAng + 1024 + 512) & 2047] >> 6);
                        pReturn->ang += MinibossScale(spriteNum, 4);
                    }
                    pReturn->xrepeat = MinibossScale(spriteNum, 42);
                    pReturn->yrepeat = MinibossScale(spriteNum, 42);
                }
                else if (PN(spriteNum) == BOSS2)
                {
                    pReturn->x -= MinibossScale(spriteNum, sintable[shootAng & 2047] / 56);
                    pReturn->y -= MinibossScale(spriteNum, sintable[(shootAng + 1024 + 512) & 2047] / 56);
                    pReturn->ang -= MinibossScale(spriteNum, 8) + (krand() & 255) - 128;
                    pReturn->xrepeat = 24;
                    pReturn->yrepeat = 24;
                }
                else if (projecTile != FREEZEBLAST)
                {
                    pReturn->xrepeat = 30;
                    pReturn->yrepeat = 30;
                    pReturn->extra >>= 2;
                }
            }
            else if (PWEAPON(playerNum, g_player[playerNum].ps->curr_weapon, WorksLike) == DEVISTATOR_WEAPON)
            {
                pReturn->extra >>= 2;
                pReturn->ang += 16 - (krand() & 31);
                pReturn->zvel += 256 - (krand() & 511);

                if (g_player[playerNum].ps->hbomb_hold_delay)
                {
                    pReturn->x -= sintable[shootAng & 2047] / 644;
                    pReturn->y -= sintable[(shootAng + 1024 + 512) & 2047] / 644;
                }
                else
                {
                    pReturn->x += sintable[shootAng & 2047] >> 8;
                    pReturn->y += sintable[(shootAng + 1024 + 512) & 2047] >> 8;
                }
                pReturn->xrepeat >>= 1;
                pReturn->yrepeat >>= 1;
            }

            pReturn->cstat    = 128;
            pReturn->clipdist = (projecTile == RPG) ? 4 : 40;

            return returnSprite;
        }

        case HANDHOLDINGLASER__:
        {
            int const zOffset     = (playerNum >= 0) ? g_player[playerNum].ps->pyoff : 0;
            Zvel                  = (playerNum >= 0) ? fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 32 : 0;

            startPos.z -= zOffset;
            Proj_DoHitscan(spriteNum, 0, &startPos, Zvel, shootAng, &hitData);
            startPos.z += zOffset;

            int placeMine = 0;
            if (hitData.sprite >= 0)
                break;

            if (hitData.wall >= 0 && hitData.sect >= 0)
            {
                uint32_t xdiff_sq = (hitData.x - startPos.x) * (hitData.x - startPos.x);
                uint32_t ydiff_sq = (hitData.y - startPos.y) * (hitData.y - startPos.y);
                if (xdiff_sq + ydiff_sq < (290 * 290))
                {
                    // ST_2_UNDERWATER
                    if (wall[hitData.wall].nextsector >= 0)
                    {
                        if (sector[wall[hitData.wall].nextsector].lotag <= 2 && sector[hitData.sect].lotag <= 2)
                            placeMine = 1;
                    }
                    else if (sector[hitData.sect].lotag <= 2)
                        placeMine = 1;
                }

            }
            if (placeMine == 1)
            {
                int const tripBombMode = (playerNum < 0) ? 0 :
                                                           Gv_GetVarByLabel("TRIPBOMB_CONTROL", TRIPBOMB_TRIPWIRE,
                                                                            g_player[playerNum].ps->i, playerNum);
                int const spawnedSprite = A_InsertSprite(hitData.sect, hitData.x, hitData.y, hitData.z, TRIPBOMB, -16, 4, 5,
                                                         shootAng, 0, 0, spriteNum, 6);
                if (tripBombMode & TRIPBOMB_TIMER)
                {
                    int32_t lLifetime = Gv_GetVarByLabel("STICKYBOMB_LIFETIME", NAM_GRENADE_LIFETIME, g_player[playerNum].ps->i, playerNum);
                    int32_t lLifetimeVar
                    = Gv_GetVarByLabel("STICKYBOMB_LIFETIME_VAR", NAM_GRENADE_LIFETIME_VAR, g_player[playerNum].ps->i, playerNum);
                    // set timer.  blows up when at zero....
                    actor[spawnedSprite].t_data[7] = lLifetime + mulscale14(krand(), lLifetimeVar) - lLifetimeVar;
                    // TIMER_CONTROL
                    actor[spawnedSprite].t_data[6] = 1;
                }
                else
                    sprite[spawnedSprite].hitag = spawnedSprite;

                A_PlaySound(LASERTRIP_ONWALL, spawnedSprite);
                sprite[spawnedSprite].xvel = -20;
                A_SetSprite(spawnedSprite, CLIPMASK0);
                sprite[spawnedSprite].cstat = 16;

                int const p2      = wall[hitData.wall].point2;
                int const wallAng = getangle(wall[hitData.wall].x - wall[p2].x, wall[hitData.wall].y - wall[p2].y) - 512;

                actor[spawnedSprite].t_data[5] = sprite[spawnedSprite].ang = wallAng;

                return spawnedSprite;
            }

            // Duke 1.5 behavior: If the hitscan check succeeds, but placing of the tripbomb fails, refund the player's ammo.
            pPlayer->ammo_amount[pPlayer->curr_weapon]++;

            return -1;
        }

        case BOUNCEMINE__:
        case MORTER__:
        {
            if (pSprite->extra >= 0)
                pSprite->shade = -96;

            int const playerSprite = g_player[A_FindPlayer(pSprite, NULL)].ps->i;
            int const playerDist   = ldist(&sprite[playerSprite], pSprite);

            Zvel = -playerDist >> 1;

            if (Zvel < -4096)
                Zvel = -2048;

            vel  = playerDist >> 4;
            Zvel = A_GetShootZvel(Zvel);

            A_InsertSprite(spriteSectnum, startPos.x + (sintable[(512 + shootAng + 512) & 2047] >> 8),
                           startPos.y + (sintable[(shootAng + 512) & 2047] >> 8), startPos.z + (6 << 8), projecTile, -64, 32, 32,
                           shootAng, vel, Zvel, spriteNum, 1);
            break;
        }

        case SHRINKER__:
        {
            if (pSprite->extra >= 0)
                pSprite->shade = -96;

            if (playerNum >= 0)
            {
                if (NAM_WW2GI || GetAutoAimAng(spriteNum, playerNum, projecTile, ZOFFSET6, 0, &startPos, 768, &Zvel, &shootAng) < 0)
                    Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 98;
            }
            else if (pSprite->statnum != STAT_EFFECTOR)
            {
                int const otherPlayer = A_FindPlayer(pSprite, NULL);
                Zvel                  = tabledivide32_noinline((g_player[otherPlayer].ps->opos.z - startPos.z) * 512,
                                              safeldist(g_player[otherPlayer].ps->i, pSprite));
            }
            else
                Zvel = 0;

            Zvel                   = A_GetShootZvel(Zvel);
            int const returnSprite = A_InsertSprite(spriteSectnum, startPos.x + (sintable[(512 + shootAng + 512) & 2047] >> 12),
                                                    startPos.y + (sintable[(shootAng + 512) & 2047] >> 12), startPos.z + (2 << 8),
                                                    SHRINKSPARK, -16, 28, 28, shootAng, 768, Zvel, spriteNum, 4);
            sprite[returnSprite].cstat    = 128;
            sprite[returnSprite].clipdist = 32;

            return returnSprite;
        }
        case FLAMETHROWERFLAME__:
        {
            if (!WORLDTOUR)
                break;

            if (pSprite->extra >= 0) pSprite->shade = -96;
            vel = 400;
            int j, underwater;
            if (playerNum >= 0)
            {
                Zvel = fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 81;
                int xv = sprite[pPlayer->i].xvel;
                if (xv)
                {
                    int ang = getangle(startPos.x-pPlayer->opos.x,startPos.y-pPlayer->opos.y);
                    ang = 512-(1024-klabs(klabs(ang-shootAng)-1024));
                    vel = 400+int(float(ang)*(1.f/512.f)*float(xv));
                }
                underwater = sector[pPlayer->cursectnum].lotag == ST_2_UNDERWATER;
            }
            else
            {
                // NOTE: j is a player index
                j          = A_FindPlayer(pSprite, NULL);
                shootAng = getangle(g_player[j].ps->opos.x - startPos.x, g_player[j].ps->opos.y - startPos.y);
                if (PN(spriteNum) == BOSS3 || PN(spriteNum) == BOSS3STAYPUT)
                    startPos.z -= MinibossScale(spriteNum, ZOFFSET5);
                else if (PN(spriteNum) == BOSS5 || PN(spriteNum) == BOSS5STAYPUT)
                {
                    vel += 128;
                    startPos.z += MinibossScale(spriteNum, 24 << 8);
                }

                Zvel = tabledivide32_noinline((g_player[j].ps->opos.z - startPos.z) * vel, safeldist(g_player[j].ps->i, pSprite));

                if (A_CheckEnemySprite(pSprite) && (AC_MOVFLAGS(pSprite, &actor[spriteNum]) & face_player_smart))
                    shootAng = pSprite->ang + (krand() & 31) - 16;
                underwater = sector[pSprite->sectnum].lotag == 2;
            }
            if (underwater)
            {
                if ((krand() % 5) != 0)
                    return -1;
                j = A_Spawn(spriteNum, WATERBUBBLE);
            }
            else
            {
                j = A_Spawn(spriteNum, projecTile);
                sprite[j].zvel = Zvel;
                sprite[j].xvel = vel;
            }
            sprite[j].x = startPos.x+sintable[(shootAng+630)&2047]/448;
            sprite[j].y = startPos.y+sintable[(shootAng+112)&2047]/448;
            sprite[j].z = startPos.z-0x100;
            sprite[j].cstat = 128;
            sprite[j].ang = shootAng;
            sprite[j].xrepeat = sprite[j].yrepeat = 2;
            sprite[j].clipdist = 40;
            sprite[j].owner = spriteNum;
            sprite[j].yvel = playerNum;
            if (playerNum == -1 && (sprite[spriteNum].picnum == BOSS5 || sprite[spriteNum].picnum == BOSS5STAYPUT))
            {
                sprite[j].xrepeat = sprite[j].yrepeat = 10;
                sprite[j].x -= sintable[shootAng&2047]/56;
                sprite[j].y -= sintable[(shootAng-512)&2047]/56;
            }
            return j;
        }
        case FIREFLY__:
        {
            if (!WORLDTOUR)
                break;

            int j = A_Spawn(spriteNum, projecTile);
            sprite[j].xyz = startPos;
            sprite[j].ang = shootAng;
            sprite[j].xvel = 500;
            sprite[j].zvel = 0;
            return j;
        }
    }

    return -1;
}
#endif

int A_ShootWithZvel(int const spriteNum, int const projecTile, int const forceZvel)
{
    Bassert(projecTile >= 0);

    auto const pSprite   = &sprite[spriteNum];
    int const  playerNum = (pSprite->picnum == APLAYER) ? P_GetP(pSprite) : -1;
    auto const pPlayer   = playerNum >= 0 ? g_player[playerNum].ps : NULL;

    if (forceZvel != SHOOT_HARDCODED_ZVEL)
    {
        g_overrideShootZvel = 1;
        g_shootZvel = forceZvel;
    }
    else
        g_overrideShootZvel = 0;

    int    shootAng;
    vec3_t startPos;

    if (pPlayer != NULL)
    {
        startPos = pPlayer->pos;
        startPos.z += pPlayer->pyoff + ZOFFSET6;
        shootAng = fix16_to_int(pPlayer->q16ang);

        pPlayer->crack_time = PCRACKTIME;
    }
    else
    {
        shootAng = pSprite->ang;
        startPos = pSprite->xyz;
        startPos.z -= (((pSprite->yrepeat * tilesiz[pSprite->picnum].y)<<1) - ZOFFSET6);

        if (pSprite->picnum != ROTATEGUN)
        {
            startPos.z -= (7<<8);

            if (A_CheckEnemySprite(pSprite) && PN(spriteNum) != COMMANDER)
            {
                startPos.x += (sintable[(shootAng+1024+96)&2047]>>7);
                startPos.y += (sintable[(shootAng+512+96)&2047]>>7);
            }
        }

#ifndef EDUKE32_STANDALONE
#ifdef POLYMER
        switch (tileGetMapping(projecTile))
        {
            case FIRELASER__:
            case SHOTGUN__:
            case SHOTSPARK1__:
            case CHAINGUN__:
            case RPG__:
            case MORTER__:
                {
                    vec3_t const offset = { -((sintable[(pSprite->ang+512)&2047])>>7), -((sintable[(pSprite->ang)&2047])>>7), PHEIGHT };
                    G_AddGameLight(spriteNum, pSprite->sectnum, offset, 8192, 0, 100, 255 + (95 << 8), PR_LIGHT_PRIO_MAX_GAME);
                    practor[spriteNum].lightcount = 2;
                }

                break;
            }
#endif // POLYMER
#endif // !EDUKE32_STANDALONE
    }

#ifdef EDUKE32_STANDALONE
    return A_CheckSpriteTileFlags(projecTile, SFLAG_PROJECTILE) ? A_ShootCustom(spriteNum, projecTile, shootAng, &startPos) : -1;
#else
    return A_CheckSpriteTileFlags(projecTile, SFLAG_PROJECTILE)
           ? A_ShootCustom(spriteNum, projecTile, shootAng, &startPos)
           : !FURY ? A_ShootHardcoded(spriteNum, projecTile, shootAng, startPos, pSprite, playerNum, pPlayer) : -1;
#endif
}


//////////////////// HUD WEAPON / MISC. DISPLAY CODE ////////////////////

#ifndef EDUKE32_STANDALONE
static void P_DisplaySpit(void)
{
    auto const pPlayer     = g_player[screenpeek].ps;
    int const  loogCounter = pPlayer->loogcnt;

    if (loogCounter == 0)
        return;

    if (VM_OnEvent(EVENT_DISPLAYSPIT, pPlayer->i, screenpeek) != 0)
        return;

    int const rotY  = loogCounter << 2;
    int const loogs = min<int>(pPlayer->numloogs, ARRAY_SIZE(pPlayer->loogie));

    for (int i=0; i < loogs; i++)
    {
        int const rotAng = klabs(sintable[((loogCounter + i) << 5) & 2047]) >> 5;
        int const rotZoom  = 4096 + ((loogCounter + i) << 9);
        int const rotX     = (-fix16_to_int(g_player[screenpeek].input.q16avel) >> 1) + (sintable[((loogCounter + i) << 6) & 2047] >> 10);

        rotatesprite_fs_id((pPlayer->loogie[i].x + rotX) << 16, (200 + pPlayer->loogie[i].y - rotY) << 16, rotZoom - (i << 8),
                        256 - rotAng, LOOGIE, 0, 0, 2, W_LOOGIE + i);
    }
}
#endif

int P_GetHudPal(const DukePlayer_t *p)
{
    if (sprite[p->i].pal == 1)
        return 1;

    if (p->cursectnum >= 0)
    {
        int const hudPal = sector[p->cursectnum].floorpal;
        if (!g_noFloorPal[hudPal])
            return hudPal;
    }

    return 0;
}

int P_GetKneePal(DukePlayer_t const * pPlayer)
{
    return P_GetKneePal(pPlayer, P_GetHudPal(pPlayer));
}

int P_GetKneePal(DukePlayer_t const * pPlayer, int const hudPal)
{
    return hudPal == 0 ? pPlayer->palookup : hudPal;
}

int P_GetOverheadPal(DukePlayer_t const * pPlayer)
{
    return sprite[pPlayer->i].pal;
}

static int P_DisplayFist(int const fistShade)
{
    DukePlayer_t const *const pPlayer = g_player[screenpeek].ps;
    int fistInc = pPlayer->fist_incs;

    if (fistInc > 32)
        fistInc = 32;

    if (fistInc <= 0)
        return 0;

    switch (VM_OnEvent(EVENT_DISPLAYFIST, pPlayer->i, screenpeek))
    {
        case 1: return 1;
        case -1: return 0;
    }

    int const baseX       = fix16_to_int(g_player[screenpeek].input.q16avel) >> 5;
    int const baseY       = klabs(pPlayer->look_ang) / 9;
    int const fistX       = 222 + baseX - fistInc;
    int const fistY       = 194 + baseY + (sintable[((6 + fistInc) << 7) & 2047] >> 9);
    int const fistZoom    = clamp(65536 - (sintable[(512 + (fistInc << 6)) & 2047] << 2), 40920, 90612);
    int const fistPal     = P_GetHudPal(pPlayer);
    int       wx[2]       = { windowxy1.x, windowxy2.x };
    int const wy[2]       = { windowxy1.y, windowxy2.y };

#ifdef SPLITSCREEN_MOD_HACKS
    // XXX: this is outdated, doesn't handle above/below split.
    if (g_fakeMultiMode==2)
        wx[(g_snum==0)] = (wx[0]+wx[1])/2+1;
#endif

    guniqhudid = W_FIST;
    rotatesprite(fistX << 16, fistY << 16, fistZoom, 0, FIST, fistShade, fistPal, 2 | RS_LERP, wx[0],wy[0], wx[1],wy[1]);
    guniqhudid = 0;

    return 1;
}

#define DRAWEAP_CENTER 262144
#define weapsc(sc) scale(sc, ud.weaponscale, 100)

static int32_t g_dts_yadd;

static void G_DrawTileScaled(int drawX, int drawY, int tileNum, int drawShade, int drawBits, int drawPal, int uniqueID = 0)
{
    int32_t wx[2] = { windowxy1.x, windowxy2.x };
    int32_t wy[2] = { windowxy1.y, windowxy2.y };

    int drawYOffset = 0;
    int drawXOffset = 192<<16;
    int const restoreid = guniqhudid;

    guniqhudid = uniqueID;

    switch (hudweap.cur)
    {
        case DEVISTATOR_WEAPON:
        case TRIPBOMB_WEAPON:
            drawXOffset = 160<<16;
            break;
        default:
            if (drawBits & DRAWEAP_CENTER)
            {
                drawXOffset = 160<<16;
                drawBits &= ~DRAWEAP_CENTER;
            }
            break;
    }

    // bit 4 means "flip x" for G_DrawTileScaled
    int const drawAng = (drawBits & 4) ? 1024 : 0;

#ifdef SPLITSCREEN_MOD_HACKS
    if (g_fakeMultiMode==2)
    {
        int const sideBySide = (ud.screen_size!=0);

        // splitscreen HACK
        drawBits &= ~(1024|512|256);
        if (sideBySide)
        {
            drawBits &= ~8;
            wx[(g_snum==0)] = (wx[0]+wx[1])/2 + 2;
        }
        else
        {
            drawBits |= 8;
            if (g_snum==0)
                drawYOffset = -(100<<16);
            wy[(g_snum==0)] = (wy[0]+wy[1])/2 + 2;
        }
    }
#endif

#ifdef USE_OPENGL
    if (videoGetRenderMode() >= REND_POLYMOST && usemodels && md_tilehasmodel(tileNum,drawPal) >= 0)
        drawYOffset += (224<<16)-weapsc(224<<16);
#endif
    rotatesprite(weapsc(drawX<<16) + (drawXOffset-weapsc(drawXOffset)),
                 weapsc((drawY<<16) + g_dts_yadd) + ((200<<16)-weapsc(200<<16)) + drawYOffset,
                 weapsc(65536L),drawAng,tileNum,drawShade,drawPal,(2|drawBits),
                 wx[0],wy[0], wx[1],wy[1]);
    guniqhudid = restoreid;
}

#ifndef EDUKE32_STANDALONE
static void G_DrawWeaponTile(int weaponX, int weaponY, int weaponTile, int weaponShade, int weaponBits, int weaponPal, int uniqueID = 0)
{
    static int shadef = 0;
    static int palf = 0;

    // basic fading between player weapon shades
    if (shadef != weaponShade && (!weaponPal || palf == weaponPal))
    {
        shadef += (weaponShade - shadef) >> 2;

        if (!((weaponShade - shadef) >> 2))
            shadef = logapproach(shadef, weaponShade);
    }
    else
        shadef = weaponShade;

    palf = weaponPal;

#ifdef USE_OPENGL
    if (videoGetRenderMode() >= REND_POLYMOST)
    {
        if (uniqueID == W_CHAINGUN_TOP)
        {
            if (!usemodels || md_tilehasmodel(weaponTile, weaponPal) < 0)
            {
                // HACK: Draw the upper part of the chaingun two screen
                // pixels (not texels; multiplied by weapon scale) lower
                // first, preventing ugly horizontal seam.
                g_dts_yadd = tabledivide32_noinline(65536 * 2 * 200, ydim);
                G_DrawTileScaled(weaponX, weaponY, weaponTile, shadef, weaponBits, weaponPal, W_CHAINGUN_HACK);
                g_dts_yadd = 0;
            }
        }
    }
#endif

    G_DrawTileScaled(weaponX, weaponY, weaponTile, shadef, weaponBits, weaponPal, uniqueID);
}
#endif

static inline void G_DrawWeaponTileUnfaded(int weaponX, int weaponY, int weaponTile, int weaponShade, int weaponBits, int p, int uniqueID = 0)
{
    G_DrawTileScaled(weaponX, weaponY, weaponTile, weaponShade, weaponBits, p, uniqueID); // skip G_DrawWeaponTile
}

static vec2_t P_GetDisplayBaseXY(DukePlayer_t const * const pPlayer)
{
    return vec2_t
    {
        (fix16_to_int(g_player[screenpeek].input.q16avel) >> 5) - (pPlayer->look_ang >> 1),
        (klabs(pPlayer->look_ang) / 9) - (pPlayer->hard_landing << 3) - (fix16_to_int(pPlayer->q16horiz - pPlayer->q16horizoff) >> 4),
    };
}

static int P_DisplayKnee(int kneeShade)
{
    static int8_t const       knee_y[] = { 0, -8, -16, -32, -64, -84, -108, -108, -108, -72, -32, -8 };
    auto const ps = g_player[screenpeek].ps;

    if (ps->knee_incs == 0)
        return 0;

    switch (VM_OnEvent(EVENT_DISPLAYKNEE, ps->i, screenpeek))
    {
        case 1: return 1;
        case -1: return 0;
    }

    if (ps->knee_incs >= ARRAY_SIZE(knee_y) || sprite[ps->i].extra <= 0)
        return 0;

    auto const base   = P_GetDisplayBaseXY(ps);
    int const kneeX   = 105 + base.x + (knee_y[ps->knee_incs] >> 2);
    int const kneeY   = 280 + base.y + knee_y[ps->knee_incs];
    int const kneePal = P_GetKneePal(ps);

    G_DrawTileScaled(kneeX, kneeY, KNEE, kneeShade, 4 | DRAWEAP_CENTER | RS_LERP, kneePal, W_KNEE);

    return 1;
}

static int P_DisplayKnuckles(int knuckleShade)
{
    if (WW2GI)
        return 0;

    auto const pPlayer = g_player[screenpeek].ps;

    if (pPlayer->knuckle_incs == 0)
        return 0;

    static int8_t const knuckleFrames[] = { 0, 1, 2, 2, 3, 3, 3, 2, 2, 1, 0 };

    switch (VM_OnEvent(EVENT_DISPLAYKNUCKLES, pPlayer->i, screenpeek))
    {
        case 1: return 1;
        case -1: return 0;
    }

    if ((unsigned) (pPlayer->knuckle_incs>>1) >= ARRAY_SIZE(knuckleFrames) || sprite[pPlayer->i].extra <= 0)
        return 0;

    auto const base       = P_GetDisplayBaseXY(pPlayer);
    int const knuckleX    = 160 + base.x;
    int const knuckleY    = 180 + base.y;
    int const knuckleTile = CRACKKNUCKLES + knuckleFrames[pPlayer->knuckle_incs >> 1];
    int const knucklePal  = P_GetHudPal(pPlayer);

    G_DrawTileScaled(knuckleX, knuckleY, knuckleTile, knuckleShade, 4 | DRAWEAP_CENTER | RS_LERP, knucklePal, W_KNUCKLES);

    return 1;
}

void P_SetWeaponGamevars(int playerNum, const DukePlayer_t * const pPlayer)
{
    Gv_SetVar(g_weaponVarID, pPlayer->curr_weapon, pPlayer->i, playerNum);
    Gv_SetVar(g_worksLikeVarID,
              ((unsigned)pPlayer->curr_weapon < MAX_WEAPONS) ? PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) : -1,
              pPlayer->i, playerNum);
}

static void P_FireWeapon(int playerNum)
{
    auto const pPlayer = g_player[playerNum].ps;

    if (VM_OnEvent(EVENT_DOFIRE, pPlayer->i, playerNum) || pPlayer->weapon_pos != 0)
        return;

    if (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) != KNEE_WEAPON)
    {
        pPlayer->ammo_amount[pPlayer->curr_weapon]--;
        P_DoWeaponRumble(playerNum);
    }

    if (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == FLAMETHROWER_WEAPON && sector[pPlayer->cursectnum].lotag == ST_2_UNDERWATER)
        return;

    if (PWEAPON(playerNum, pPlayer->curr_weapon, FireSound) > 0)
        A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, FireSound), pPlayer->i);

    P_SetWeaponGamevars(playerNum, pPlayer);
    //        OSD_Printf("doing %d %d %d\n",PWEAPON(snum, p->curr_weapon, Shoots),p->curr_weapon,snum);

    ud.returnvar[0] = 0;
    if (VM_OnEventWithReturn(EVENT_PREWEAPONSHOOT, pPlayer->i, playerNum, 0) == 0)
    {
        auto const retVal = A_Shoot(pPlayer->i, PWEAPON(playerNum, pPlayer->curr_weapon, Shoots));
        ud.returnvar[0] = 0;
        VM_OnEventWithReturn(EVENT_POSTWEAPONSHOOT, pPlayer->i, playerNum, retVal);
    }

    for (bssize_t burstFire = 1; burstFire < PWEAPON(playerNum, pPlayer->curr_weapon, ShotsPerBurst); burstFire++)
    {
        if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_FIREEVERYOTHER)
        {
            // devastator hack to make the projectiles fire on a delay from player code
            actor[pPlayer->i].t_data[7] = (PWEAPON(playerNum, pPlayer->curr_weapon, ShotsPerBurst)) << 1;
        }
        else
        {
            if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_AMMOPERSHOT &&
                PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) != KNEE_WEAPON)
            {
                if (pPlayer->ammo_amount[pPlayer->curr_weapon] > 0)
                    pPlayer->ammo_amount[pPlayer->curr_weapon]--;
                else
                    break;
            }

            ud.returnvar[0] = burstFire;
            if (VM_OnEventWithReturn(EVENT_PREWEAPONSHOOT, pPlayer->i, playerNum, 0) == 0)
            {
                auto const retVal = A_Shoot(pPlayer->i, PWEAPON(playerNum, pPlayer->curr_weapon, Shoots));
                ud.returnvar[0] = burstFire;
                VM_OnEventWithReturn(EVENT_POSTWEAPONSHOOT, pPlayer->i, playerNum, retVal);
            }
        }
    }

    if (!(PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_NOVISIBLE))
    {
#ifdef POLYMER
        auto s = (uspriteptr_t)&sprite[pPlayer->i];
        vec3_t const offset = { -((sintable[(s->ang+512)&2047])>>7), -((sintable[(s->ang)&2047])>>7), pPlayer->spritezoffset };
        G_AddGameLight(pPlayer->i, pPlayer->cursectnum, offset, 8192, 0, 100, PWEAPON(playerNum, pPlayer->curr_weapon, FlashColor), PR_LIGHT_PRIO_MAX_GAME);
        practor[pPlayer->i].lightcount = 2;
#endif  // POLYMER
        pPlayer->visibility = 0;
    }

    if (WW2GI)
    {
        if (/*!(PWEAPON(playerNum, p->curr_weapon, Flags) & WEAPON_CHECKATRELOAD) && */ pPlayer->reloading == 1 ||
                (PWEAPON(playerNum, pPlayer->curr_weapon, Reload) > PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime) && pPlayer->ammo_amount[pPlayer->curr_weapon] > 0
                 && (PWEAPON(playerNum, pPlayer->curr_weapon, Clip)) && (((pPlayer->ammo_amount[pPlayer->curr_weapon]%(PWEAPON(playerNum, pPlayer->curr_weapon, Clip)))==0))))
        {
            pPlayer->kickback_pic = PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime);
        }
    }
}

static void P_DoWeaponSpawn(int playerNum)
{
    auto const pPlayer = g_player[playerNum].ps;

    // NOTE: For the 'Spawn' member, 0 means 'none', too (originally so,
    // i.e. legacy). The check for <0 was added to the check because mod
    // authors (rightly) assumed that -1 is the no-op value.
    if (PWEAPON(playerNum, pPlayer->curr_weapon, Spawn) <= 0)  // <=0 : AMC TC beta/RC2 has WEAPONx_SPAWN -1
        return;

    int newSprite = A_Spawn(pPlayer->i, PWEAPON(playerNum, pPlayer->curr_weapon, Spawn));

    if ((PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_SPAWNTYPE3))
    {
        // like chaingun shells
        sprite[newSprite].ang += 1024;
        sprite[newSprite].ang &= 2047;
        sprite[newSprite].xvel += 32;
        sprite[newSprite].z += (3<<8);
    }

    A_SetSprite(newSprite,CLIPMASK0);
}

void P_DisplayScuba(void)
{
    if (g_player[screenpeek].ps->scuba_on)
    {
        auto const pPlayer = g_player[screenpeek].ps;

        if (VM_OnEvent(EVENT_DISPLAYSCUBA, pPlayer->i, screenpeek) != 0)
            return;

        int const scubaPal = P_GetHudPal(pPlayer);
        int scubaY = 200 - tilesiz[SCUBAMASK].y;
        if (ud.screen_size > 4 && ud.statusbarmode == 0)
            // Scale the offset of 8px with the status bar, otherwise the bottom of the tile is cut
            scubaY -= scale(8, ud.statusbarscale, 100);

#ifdef SPLITSCREEN_MOD_HACKS
        g_snum = screenpeek;
#endif

        // this is a hack to hide the seam that appears between the two halves of the mask in GL
#ifdef USE_OPENGL
        if (videoGetRenderMode() >= REND_POLYMOST)
            G_DrawTileScaled(44, scubaY, SCUBAMASK, 0, 2 + 16 + DRAWEAP_CENTER, scubaPal);
#endif
        G_DrawTileScaled(43, scubaY, SCUBAMASK, 0, 2 + 16 + DRAWEAP_CENTER, scubaPal);
        G_DrawTileScaled(320 - 43, scubaY, SCUBAMASK, 0, 2 + 4 + 16 + DRAWEAP_CENTER, scubaPal);
    }
}

static int8_t const access_tip_y [] = {
    0, -8, -16, -32, -64, -84, -108, -108, -108, -108, -108, -108, -108, -108, -108, -108, -96, -72, -64, -32, -16,
    /* EDuke32: */ 0, 16, 32, 48,
    // At y coord 64, the hand is already not shown.
};

static int P_DisplayTip(int tipShade)
{
    auto const pPlayer = g_player[screenpeek].ps;

    if (pPlayer->tipincs == 0)
        return 0;

    switch (VM_OnEvent(EVENT_DISPLAYTIP, pPlayer->i, screenpeek))
    {
        case 1: return 1;
        case -1: return 0;
    }

    // Report that the tipping hand has been drawn so that the otherwise
    // selected weapon is not drawn.
    if ((unsigned)pPlayer->tipincs >= ARRAY_SIZE(access_tip_y))
        return 1;

    auto const base     = P_GetDisplayBaseXY(pPlayer);
    int const tipX      = 170 + base.x;
    int const tipY      = 240 + base.y + (access_tip_y[pPlayer->tipincs] >> 1);
    int const tipTile   = TIP + ((26 - pPlayer->tipincs) >> 4);
    int const tipPal    = P_GetHudPal(pPlayer);

    G_DrawTileScaled(tipX, tipY, tipTile, tipShade, DRAWEAP_CENTER | RS_LERP, tipPal, W_TIP);

    return 1;
}

static int P_DisplayAccess(int accessShade)
{
    auto const pSprite = g_player[screenpeek].ps;

    if (pSprite->access_incs == 0)
        return 0;

    switch (VM_OnEvent(EVENT_DISPLAYACCESS, pSprite->i, screenpeek))
    {
        case 1: return 1;
        case -1: return 0;
    }

    if ((unsigned)pSprite->access_incs >= ARRAY_SIZE(access_tip_y)-4 || sprite[pSprite->i].extra <= 0)
        return 1;

    auto const base     = P_GetDisplayBaseXY(pSprite);
    int const accessX   = 170 + base.x + (access_tip_y[pSprite->access_incs] >> 2);
    int const accessY   = 266 + base.y + access_tip_y[pSprite->access_incs];
    int const accessPal = (pSprite->access_spritenum >= 0) ? sprite[pSprite->access_spritenum].pal : 0;

    auto const accessMode = pSprite->access_incs > 3 && (pSprite->access_incs - 3) >> 3;
    int  const accessTile = accessMode ? HANDHOLDINGLASER + (pSprite->access_incs >> 3) : HANDHOLDINGACCESS;
    int  const accessBits = accessMode ? DRAWEAP_CENTER : (4 | DRAWEAP_CENTER);

    G_DrawTileScaled(accessX, accessY, accessTile, accessShade, accessBits | RS_LERP | RS_FORCELERP, accessPal, W_ACCESSCARD);

    return 1;
}

void P_DisplayWeapon(void)
{
    auto const pPlayer     = g_player[screenpeek].ps;
    auto const weaponFrame = &pPlayer->kickback_pic;

    int currentWeapon;

#ifdef SPLITSCREEN_MOD_HACKS
    g_snum = screenpeek;
#endif

    if (pPlayer->newowner >= 0 || ud.camerasprite >= 0 || pPlayer->over_shoulder_on > 0
        || (sprite[pPlayer->i].pal != 1 && sprite[pPlayer->i].extra <= 0))
        return;

    int weaponX       = (160) - 90;
    int weaponY       = klabs(pPlayer->look_ang) / 9;
    int weaponYOffset = 80 - (pPlayer->weapon_pos * pPlayer->weapon_pos);
    int weaponShade   = sprite[pPlayer->i].shade <= 24 ? sprite[pPlayer->i].shade : 24;

    // fixes trying to interpolate between the weapon_pos 0 and fully lowered positions when placing tripbombs
    // FFS, WEAPON_POS_RAISE gets set in P_DoCounters() and then P_ProcessWeapon() decrements it before we can check for it when drawing
#ifndef EDUKE32_STANDALONE
    int32_t weaponBits = pPlayer->weapon_pos == WEAPON_POS_LOWER || pPlayer->weapon_pos >= WEAPON_POS_RAISE-1 ? 0 : RS_LERP;
#endif
    if (P_DisplayFist(weaponShade) || P_DisplayKnuckles(weaponShade) || P_DisplayTip(weaponShade) || P_DisplayAccess(weaponShade))
        goto enddisplayweapon;

    P_DisplayKnee(weaponShade);

    if (ud.weaponsway)
    {
        weaponX -= (sintable[((pPlayer->weapon_sway>>1)+512)&2047]/(1024+512));
        weaponYOffset -= (sprite[pPlayer->i].xrepeat < 32) ? klabs(sintable[(pPlayer->weapon_sway << 2) & 2047] >> 9)
                                                           : klabs(sintable[(pPlayer->weapon_sway >> 1) & 2047] >> 10);
    }
    else weaponYOffset -= 16;

    weaponX -= 58 + pPlayer->weapon_ang;
    weaponYOffset -= (pPlayer->hard_landing << 3);

    currentWeapon       = PWEAPON(screenpeek, (pPlayer->last_weapon >= 0) ? pPlayer->last_weapon : pPlayer->curr_weapon, WorksLike);
    hudweap.gunposy     = weaponYOffset;
    hudweap.lookhoriz   = weaponY;
    hudweap.cur         = currentWeapon;
    hudweap.gunposx     = weaponX;
    hudweap.shade       = weaponShade;
    hudweap.count       = *weaponFrame;
    hudweap.lookhalfang = pPlayer->look_ang >> 1;

    if (VM_OnEvent(EVENT_DISPLAYWEAPON, pPlayer->i, screenpeek) == 0)
    {
#ifndef EDUKE32_STANDALONE
        int const quickKickFrame = 14 - pPlayer->quick_kick;

        if (!FURY && (quickKickFrame != 14 || pPlayer->last_quick_kick) && ud.drawweapon == 1)
        {
            int const weaponPal = P_GetKneePal(pPlayer);

            if (quickKickFrame < 6 || quickKickFrame > 12)
                G_DrawTileScaled(weaponX + 80 - (pPlayer->look_ang >> 1), weaponY + 250 - weaponYOffset, KNEE, weaponShade,
                                 weaponBits | 4 | DRAWEAP_CENTER, weaponPal, W_KNEE2);
            else
                G_DrawTileScaled(weaponX + 160 - 16 - (pPlayer->look_ang >> 1), weaponY + 214 - weaponYOffset, KNEE + 1,
                                 weaponShade, weaponBits | 4 | DRAWEAP_CENTER, weaponPal, W_KNEE2);
        }

        if (!FURY && sprite[pPlayer->i].xrepeat < 40)
        {
            static int32_t fistPos;

            int const weaponPal = P_GetHudPal(pPlayer);

            if (pPlayer->jetpack_on == 0)
            {
                int const playerXvel = sprite[pPlayer->i].xvel;
                weaponY += 32 - (playerXvel >> 3);
                fistPos += playerXvel >> 3;
            }

            currentWeapon = weaponX;
            weaponX += sintable[(fistPos)&2047] >> 10;
            G_DrawTileScaled(weaponX + 250 - (pPlayer->look_ang >> 1), weaponY + 258 - (klabs(sintable[(fistPos)&2047] >> 8)),
                FIST, weaponShade, weaponBits, weaponPal, W_FIST);
            weaponX = currentWeapon - (sintable[(fistPos)&2047] >> 10);
            G_DrawTileScaled(weaponX + 40 - (pPlayer->look_ang >> 1), weaponY + 200 + (klabs(sintable[(fistPos)&2047] >> 8)), FIST,
                weaponShade, weaponBits | 4, weaponPal, W_FIST2);
        }
        else
#endif
        {
            switch (ud.drawweapon)
            {
                case 1: break;
#ifndef EDUKE32_STANDALONE
                case 2:
                    if (!FURY && (unsigned)hudweap.cur < MAX_WEAPONS && hudweap.cur != KNEE_WEAPON)
                        rotatesprite_win(160 << 16, (180 + (pPlayer->weapon_pos * pPlayer->weapon_pos)) << 16, divscale16(ud.statusbarscale, 100), 0,
                                         hudweap.cur == GROW_WEAPON ? GROWSPRITEICON : WeaponPickupSprites[hudweap.cur], 0,
                                         0, 2);
#endif
                default: goto enddisplayweapon;
            }

            if (VM_OnEvent(EVENT_DRAWWEAPON, g_player[screenpeek].ps->i, screenpeek)||(currentWeapon == KNEE_WEAPON && *weaponFrame == 0))
                goto enddisplayweapon;

#ifndef EDUKE32_STANDALONE
            int const doAnim      = !(sprite[pPlayer->i].pal == 1 || ud.pause_on || g_player[myconnectindex].ps->gm & MODE_MENU);
            int const halfLookAng = pPlayer->look_ang >> 1;

            int const weaponPal = P_GetHudPal(pPlayer);

            if (!FURY)
            switch (currentWeapon)
            {
            case KNEE_WEAPON:
            {
                int const kneePal = P_GetKneePal(pPlayer, weaponPal);

                if (*weaponFrame < 5 || *weaponFrame > 9)
                    G_DrawTileScaled(weaponX + 220 - halfLookAng, weaponY + 250 - weaponYOffset, KNEE,
                                     weaponShade, weaponBits, kneePal, W_KNEE);
                else
                    G_DrawTileScaled(weaponX + 160 - halfLookAng, weaponY + 214 - weaponYOffset, KNEE + 1,
                                     weaponShade, weaponBits, kneePal, W_KNEE);
                break;
            }

            case TRIPBOMB_WEAPON:
                weaponX += 8;
                weaponYOffset -= 10;

                if ((*weaponFrame) > 6)
                    weaponY += ((*weaponFrame) << 3);
                else if ((*weaponFrame) < 4)
                    G_DrawWeaponTile(weaponX + 142 - halfLookAng, weaponY + 234 - weaponYOffset, TRIPBOMB, weaponShade, weaponBits, weaponPal, W_TRIPBOMB);

                G_DrawWeaponTile(weaponX + 130 - halfLookAng, weaponY + 249 - weaponYOffset, HANDHOLDINGLASER + ((*weaponFrame) >> 2), weaponShade, weaponBits,
                                 weaponPal, W_TRIPBOMB_LEFTHAND);

                G_DrawWeaponTile(weaponX + 152 - halfLookAng, weaponY + 249 - weaponYOffset, HANDHOLDINGLASER + ((*weaponFrame) >> 2), weaponShade, weaponBits | 4,
                                 weaponPal, W_TRIPBOMB_RIGHTHAND);
                break;

            case RPG_WEAPON:
                weaponX -= sintable[(768 + ((*weaponFrame) << 7)) & 2047] >> 11;
                weaponYOffset += sintable[(768 + ((*weaponFrame) << 7)) & 2047] >> 11;

                if (!WORLDTOUR && !(duke3d_globalflags & DUKE3D_NO_WIDESCREEN_PINNING))
                    weaponBits |= 512;

                if (*weaponFrame > 0)
                {
                    int totalTime;
                    if (*weaponFrame < (WW2GI ? (totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime)) : 8))
                        G_DrawWeaponTile(weaponX + 164, (weaponY << 1) + 176 - weaponYOffset, RPGGUN + ((*weaponFrame) >> 1), weaponShade, weaponBits, weaponPal,
                                         W_RPG_MUZZLE);
                    else if (WW2GI)
                    {
                        totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);
                        int const reloadTime = PWEAPON(screenpeek, pPlayer->curr_weapon, Reload);

                        weaponYOffset -= (*weaponFrame < ((reloadTime - totalTime) / 2 + totalTime))
                                          ? 10 * ((*weaponFrame) - totalTime)   // down
                                          : 10 * (reloadTime - (*weaponFrame)); // up
                    }
                }

                G_DrawWeaponTile(weaponX + 164, (weaponY << 1) + 176 - weaponYOffset, WT_WIDE(RPGGUN), weaponShade, weaponBits, weaponPal, W_RPG);
                break;

            case SHOTGUN_WEAPON:
                weaponX -= 8;

                if (WW2GI)
                {
                    int const totalTime  = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);
                    int const reloadTime = PWEAPON(screenpeek, pPlayer->curr_weapon, Reload);

                    if (*weaponFrame > 0)
                        weaponYOffset -= sintable[(*weaponFrame)<<7]>>12;

                    if (*weaponFrame > 0 && doAnim)
                        weaponX += 1-(wrand()&3);

                    if (*weaponFrame == 0)
                    {
                        G_DrawWeaponTile(weaponX + 146 - halfLookAng, weaponY + 202 - weaponYOffset, SHOTGUN, weaponShade, weaponBits | RS_FORCELERP,
                                         weaponPal, W_SHOTGUN);
                    }
                    else if (*weaponFrame <= totalTime)
                    {
                        G_DrawWeaponTile(weaponX + 146 - halfLookAng, weaponY + 202 - weaponYOffset, SHOTGUN + 1, weaponShade, weaponBits | RS_FORCELERP,
                                         weaponPal, W_SHOTGUN);
                    }
                    // else we are in 'reload time'
                    else
                    {
                        weaponYOffset -= (*weaponFrame < ((reloadTime - totalTime) / 2 + totalTime))
                                         ? 10 * ((*weaponFrame) - totalTime)    // D
                                         : 10 * (reloadTime - (*weaponFrame));  // U

                        G_DrawWeaponTile(weaponX + 146 - halfLookAng, weaponY + 202 - weaponYOffset, SHOTGUN, weaponShade, weaponBits | RS_FORCELERP,
                                         weaponPal, W_SHOTGUN);
                    }

                    break;
                }

                switch (*weaponFrame)
                {
                    case 1:
                    case 2:
                        G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 201 - weaponYOffset, SHOTGUN + 2, -128, weaponBits | RS_FORCELERP, weaponPal,
                                         W_SHOTGUN_MUZZLE);
                        fallthrough__;
                    case 0:
                    case 6:
                    case 7:
                    case 8:
                        G_DrawWeaponTile(weaponX + 146 - halfLookAng, weaponY + 202 - weaponYOffset, SHOTGUN, weaponShade, weaponBits, weaponPal, W_SHOTGUN);
                        break;

                    case 3:
                    case 4:
                        weaponYOffset -= 40;
                        weaponX += 20;

                        G_DrawWeaponTile(weaponX + 178 - halfLookAng, weaponY + 194 - weaponYOffset, SHOTGUN + 1 + ((*(weaponFrame)-1) >> 1), -128, weaponBits,
                                         weaponPal, W_SHOTGUN_MUZZLE);
                        fallthrough__;
                    case 5:
                    case 9:
                    case 10:
                    case 11:
                    case 12:
                        G_DrawWeaponTile(weaponX + 158 - halfLookAng, weaponY + 220 - weaponYOffset, SHOTGUN + 3, weaponShade, weaponBits, weaponPal, W_SHOTGUN);
                        break;

                    case 13:
                    case 14:
                    case 15:
                        G_DrawWeaponTile(32 + weaponX + 166 - halfLookAng, weaponY + 210 - weaponYOffset, SHOTGUN + 4, weaponShade, weaponBits & ~RS_LERP, weaponPal,
                                         W_SHOTGUN);
                        break;

                    case 16:
                    case 17:
                    case 18:
                    case 19:
                    case 24:
                    case 25:
                    case 26:
                    case 27:
                        G_DrawWeaponTile(64 + weaponX + 170 - halfLookAng, weaponY + 196 - weaponYOffset, SHOTGUN + 5, weaponShade, weaponBits, weaponPal,
                                         W_SHOTGUN);
                        break;

                    case 20:
                    case 21:
                    case 22:
                    case 23:
                        G_DrawWeaponTile(64 + weaponX + 176 - halfLookAng, weaponY + 196 - weaponYOffset, SHOTGUN + 6, weaponShade, weaponBits, weaponPal,
                                         W_SHOTGUN);
                        break;


                    case 28:
                    case 29:
                    case 30:
                        G_DrawWeaponTile(32 + weaponX + 156 - halfLookAng, weaponY + 206 - weaponYOffset, SHOTGUN + 4, weaponShade, weaponBits, weaponPal,
                                         W_SHOTGUN);
                        break;
                }
                break;

            case CHAINGUN_WEAPON:
                if (*weaponFrame > 0)
                {
                    weaponYOffset -= sintable[(*weaponFrame)<<7]>>12;

                    if (doAnim)
                        weaponX += 1-(wrand()&3);
                }

                if (WW2GI)
                {
                    int const totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);
                    int const reloadTime = PWEAPON(screenpeek, pPlayer->curr_weapon, Reload);

                    if (*weaponFrame == 0)
                    {
                        G_DrawWeaponTile(weaponX + 178 - halfLookAng, weaponY + 233 - weaponYOffset, CHAINGUN + 1, weaponShade, weaponBits | RS_FORCELERP,
                                         weaponPal, W_CHAINGUN_BOTTOM);
                    }
                    else if (*weaponFrame <= totalTime)
                    {
                        G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 243 - weaponYOffset, CHAINGUN + 2, weaponShade, weaponBits | RS_FORCELERP,
                                         weaponPal, W_CHAINGUN_BOTTOM);
                    }
                    // else we are in 'reload time'
                    // divide reload time into fifths..
                    // 1) move weapon up/right, hand on clip (CHAINGUN - 17)
                    // 2) move weapon up/right, hand removing clip (CHAINGUN - 18)
                    // 3) hold weapon up/right, hand removed clip (CHAINGUN - 19)
                    // 4) hold weapon up/right, hand inserting clip (CHAINGUN - 18)
                    // 5) move weapon down/left, clip inserted (CHAINGUN - 17)
                    else
                    {
                        int iFifths = (reloadTime - totalTime) / 5;
                        if (iFifths < 1)
                            iFifths = 1;

                        if (*weaponFrame < iFifths + totalTime)
                        {
                            // first segment
                            int const weaponOffset = 80 - 10 * (totalTime + iFifths - (*weaponFrame));
                            weaponYOffset += weaponOffset;
                            weaponX += weaponOffset;
                            G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 260 - weaponYOffset, CHAINGUN - 17, weaponShade,
                                             weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_BOTTOM);
                        }
                        else if (*weaponFrame < (iFifths * 2 + totalTime))
                        {
                            // second segment
                            weaponYOffset += 80; // D
                            weaponX += 80;
                            G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 260 - weaponYOffset, CHAINGUN - 18, weaponShade,
                                             weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_BOTTOM);
                        }
                        else if (*weaponFrame < (iFifths * 3 + totalTime))
                        {
                            // third segment
                            // up
                            weaponYOffset += 80;
                            weaponX += 80;
                            G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 260 - weaponYOffset, CHAINGUN - 19, weaponShade,
                                             weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_BOTTOM);
                        }
                        else if (*weaponFrame < (iFifths * 4 + totalTime))
                        {
                            // fourth segment
                            // down
                            weaponYOffset += 80; // D
                            weaponX += 80;
                            G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 260 - weaponYOffset, CHAINGUN - 18, weaponShade,
                                             weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_BOTTOM);
                        }
                        else
                        {
                            // up and left
                            int const weaponOffset = 10 * (reloadTime - (*weaponFrame));
                            weaponYOffset += weaponOffset; // U
                            weaponX += weaponOffset;
                            G_DrawWeaponTile(weaponX + 168 - halfLookAng, weaponY + 260 - weaponYOffset, CHAINGUN - 17, weaponShade,
                                             weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_BOTTOM);
                        }
                    }

                    break;
                }

                switch (*weaponFrame)
                {
                case 0:
                    G_DrawWeaponTile(weaponX + 178 - (pPlayer->look_ang >> 1), weaponY + 233 - weaponYOffset, CHAINGUN + 1, weaponShade,
                                        weaponBits | RS_FORCELERP, weaponPal, W_CHAINGUN_TOP);
                    G_DrawWeaponTile(weaponX + 168 - (pPlayer->look_ang >> 1), weaponY + 260 - weaponYOffset, CHAINGUN, weaponShade, weaponBits, weaponPal,
                                     W_CHAINGUN_BOTTOM);
                    break;

                default:
                    if (*weaponFrame > PWEAPON(screenpeek, CHAINGUN_WEAPON, FireDelay) && *weaponFrame < PWEAPON(screenpeek, CHAINGUN_WEAPON, TotalTime))
                    {
                        int randomOffset = doAnim ? wrand() & 7 : 0;
                        G_DrawWeaponTile(randomOffset + weaponX - 4 + 140 - (pPlayer->look_ang >> 1),
                                            randomOffset + weaponY - ((*weaponFrame) >> 1) + 208 - weaponYOffset, CHAINGUN + 5 + ((*weaponFrame - 4) / 5),
                                            weaponShade, weaponBits, weaponPal);
                        if (doAnim)
                            randomOffset = wrand() & 7;
                        G_DrawWeaponTile(randomOffset + weaponX - 4 + 184 - (pPlayer->look_ang >> 1),
                                            randomOffset + weaponY - ((*weaponFrame) >> 1) + 208 - weaponYOffset, CHAINGUN + 5 + ((*weaponFrame - 4) / 5),
                                            weaponShade, weaponBits, weaponPal);
                    }

                    if (*weaponFrame < PWEAPON(screenpeek, CHAINGUN_WEAPON, TotalTime) - 4)
                    {
                        int const randomOffset = doAnim ? wrand() & 7 : 0;
                        G_DrawWeaponTile(randomOffset + weaponX - 4 + 162 - (pPlayer->look_ang >> 1),
                                            randomOffset + weaponY - ((*weaponFrame) >> 1) + 208 - weaponYOffset, CHAINGUN + 5 + ((*weaponFrame - 2) / 5),
                                            weaponShade, weaponBits, weaponPal);
                        G_DrawWeaponTile(weaponX + 178 - (pPlayer->look_ang >> 1), weaponY + 233 - weaponYOffset, CHAINGUN + 1 + ((*weaponFrame) >> 1),
                                            weaponShade, weaponBits & ~RS_LERP, weaponPal, W_CHAINGUN_TOP);
                    }
                    else
                        G_DrawWeaponTile(weaponX + 178 - (pPlayer->look_ang >> 1), weaponY + 233 - weaponYOffset, CHAINGUN + 1, weaponShade,
                                            weaponBits & ~RS_LERP, weaponPal, W_CHAINGUN_TOP);

                    G_DrawWeaponTile(weaponX + 168 - (pPlayer->look_ang >> 1), weaponY + 260 - weaponYOffset, CHAINGUN, weaponShade, weaponBits & ~RS_LERP,
                                     weaponPal, W_CHAINGUN_BOTTOM);
                    break;
                }

                break;

            case PISTOL_WEAPON:
            {
                if ((*weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, TotalTime) + 1)
                {
                    static uint8_t pistolFrames[] = { 0, 1, 2 };
                    int            pistolOffset   = 195 - 12 + weaponX;

                    if ((*weaponFrame) == PWEAPON(screenpeek, PISTOL_WEAPON, FireDelay))
                        pistolOffset -= 3;

                    G_DrawWeaponTile((pistolOffset - (pPlayer->look_ang >> 1)), (weaponY + 244 - weaponYOffset),
                                     FIRSTGUN + pistolFrames[*weaponFrame > 2 ? 0 : *weaponFrame], weaponShade, weaponBits, weaponPal, W_PISTOL);

                    break;
                }

                if (!WORLDTOUR && !(duke3d_globalflags & DUKE3D_NO_WIDESCREEN_PINNING) && DUKE)
                    weaponBits |= 512;

                int32_t const FIRSTGUN_5 = WORLDTOUR ? FIRSTGUNRELOADWIDE : FIRSTGUN + 5;

                if ((*weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload) - (NAM_WW2GI ? 40 : 17))
                    G_DrawWeaponTile(194 - (pPlayer->look_ang >> 1), weaponY + 230 - weaponYOffset, FIRSTGUN + 4, weaponShade, weaponBits & ~RS_LERP,
                                     weaponPal, W_PISTOL);
                else if ((*weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload) - (NAM_WW2GI ? 35 : 12))
                {
                    G_DrawWeaponTile(244 - ((*weaponFrame) << 3) - (pPlayer->look_ang >> 1), weaponY + 130 - weaponYOffset + ((*weaponFrame) << 4), FIRSTGUN + 6,
                                     weaponShade, weaponBits, weaponPal, W_PISTOL_CLIP);
                    G_DrawWeaponTile(224 - (pPlayer->look_ang >> 1), weaponY + 220 - weaponYOffset, FIRSTGUN_5, weaponShade, weaponBits, weaponPal, W_PISTOL);
                }
                else if ((*weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload) - (NAM_WW2GI ? 30 : 7))
                {
                    G_DrawWeaponTile(124 + ((*weaponFrame) << 1) - (pPlayer->look_ang >> 1), weaponY + 430 - weaponYOffset - ((*weaponFrame) << 3), FIRSTGUN + 6,
                                     weaponShade, weaponBits, weaponPal, W_PISTOL_CLIP);
                    G_DrawWeaponTile(224 - (pPlayer->look_ang >> 1), weaponY + 220 - weaponYOffset, FIRSTGUN_5, weaponShade, weaponBits, weaponPal, W_PISTOL);
                }

                else if ((*weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload) - (NAM_WW2GI ? 12 : 4))
                {
                    G_DrawWeaponTile(184 - (pPlayer->look_ang >> 1), weaponY + 235 - weaponYOffset, FIRSTGUN + 8, weaponShade, weaponBits, weaponPal,
                                     W_PISTOL_HAND);
                    G_DrawWeaponTile(224 - (pPlayer->look_ang >> 1), weaponY + 210 - weaponYOffset, FIRSTGUN_5, weaponShade, weaponBits, weaponPal, W_PISTOL);
                }
                else if ((*weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload) - (NAM_WW2GI ? 6 : 2))
                {
                    G_DrawWeaponTile(164 - (pPlayer->look_ang >> 1), weaponY + 245 - weaponYOffset, FIRSTGUN + 8, weaponShade, weaponBits, weaponPal,
                                     W_PISTOL_HAND);
                    G_DrawWeaponTile(224 - (pPlayer->look_ang >> 1), weaponY + 220 - weaponYOffset, FIRSTGUN_5, weaponShade, weaponBits, weaponPal, W_PISTOL);
                }
                else if ((*weaponFrame) < PWEAPON(screenpeek, PISTOL_WEAPON, Reload))
                    G_DrawWeaponTile(194 - (pPlayer->look_ang >> 1), weaponY + 235 - weaponYOffset, FIRSTGUN_5, weaponShade, weaponBits, weaponPal, W_PISTOL);

                break;
            }

            case HANDBOMB_WEAPON:
                {
                    static uint8_t pipebombFrames [] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2 };

                    if (*weaponFrame >= PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime) || *weaponFrame >= ARRAY_SIZE(pipebombFrames))
                        break;

                    if (*weaponFrame)
                    {
                        if (WW2GI)
                        {
                            int const fireDelay = PWEAPON(screenpeek, pPlayer->curr_weapon, FireDelay);
                            int const totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);

                            if (*weaponFrame <= fireDelay)
                            {
                                // it holds here
                                weaponYOffset -= 5 * (*weaponFrame);  // D
                            }
                            else if (*weaponFrame < ((totalTime - fireDelay) / 2 + fireDelay))
                            {
                                // up and left
                                int const weaponOffset = (*weaponFrame) - fireDelay;
                                weaponYOffset += 10 * weaponOffset;  // U
                                weaponX += 80 * weaponOffset;
                            }
                            else if (*weaponFrame < totalTime)
                            {
                                // start high
                                weaponYOffset += 240;
                                weaponYOffset -= 12 * ((*weaponFrame) - fireDelay);  // D
                                // move left
                                weaponX += 90 - 5 * (totalTime - (*weaponFrame));
                            }
                        }
                        else
                        {
                            if (*weaponFrame < 7)       weaponYOffset -= 10 * (*weaponFrame);  // D
                            else if (*weaponFrame < 12) weaponYOffset += 20 * ((*weaponFrame) - 10);  // U
                            else if (*weaponFrame < 20) weaponYOffset -= 9  * ((*weaponFrame) - 14);  // D
                        }

                        weaponYOffset += 10;
                    }

                    G_DrawWeaponTile(weaponX + 190 - halfLookAng, weaponY + 260 - weaponYOffset, HANDTHROW + pipebombFrames[(*weaponFrame)], weaponShade,
                                     weaponBits, weaponPal, W_HANDBOMB);
                }
                break;

            case HANDREMOTE_WEAPON:
                {
                    static uint8_t remoteFrames[] = { 0, 1, 1, 2, 1, 1, 0, 0, 0, 0, 0 };

                    if (*weaponFrame >= ARRAY_SIZE(remoteFrames))
                        break;

                    weaponX = -48;
                    G_DrawWeaponTile(weaponX + 150 - halfLookAng, weaponY + 258 - weaponYOffset, HANDREMOTE + remoteFrames[(*weaponFrame)], weaponShade,
                                     weaponBits | RS_FORCELERP, weaponPal, W_HANDREMOTE);
                }
                break;

            case DEVISTATOR_WEAPON:
                if (WW2GI)
                {
                    if (*weaponFrame)
                    {
                        int32_t const totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);
                        int32_t const reloadTime = PWEAPON(screenpeek, pPlayer->curr_weapon, Reload);

                        if (*weaponFrame < totalTime)
                        {
                            int const tileOffset = ksgn((*weaponFrame) >> 2);

                            if (pPlayer->ammo_amount[pPlayer->curr_weapon] & 1)
                            {
                                G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits | 4, weaponPal,
                                                 W_DEVISTATOR_LEFT);
                                G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR + tileOffset, -32, weaponBits, weaponPal,
                                                 W_DEVISTATOR_RIGHT);
                            }
                            else
                            {
                                G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR + tileOffset, -32, weaponBits | 4, weaponPal,
                                                 W_DEVISTATOR_LEFT);
                                G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits, weaponPal,
                                                 W_DEVISTATOR_RIGHT);
                            }
                        }
                        // else we are in 'reload time'
                        else
                        {
                            weaponYOffset
                            -= (*weaponFrame < ((reloadTime - totalTime) / 2 + totalTime)) ? 10 * ((*weaponFrame) - totalTime) : 10 * (reloadTime - (*weaponFrame));

                            G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits | 4, weaponPal,
                                             W_DEVISTATOR_LEFT);
                            G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits, weaponPal,
                                             W_DEVISTATOR_RIGHT);
                        }
                    }
                    else
                    {
                        G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits | 4, weaponPal,
                                         W_DEVISTATOR_LEFT);
                        G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits, weaponPal,
                                         W_DEVISTATOR_RIGHT);
                    }
                    break;
                }

                if (*weaponFrame <= PWEAPON(screenpeek, DEVISTATOR_WEAPON, TotalTime) && *weaponFrame > 0)
                {
                    static uint8_t const devastatorFrames[] = { 0, 4, 12, 24, 12, 4, 0 };

                    if (*weaponFrame >= ARRAY_SIZE(devastatorFrames))
                        break;

                    int const tileOffset = ksgn((*weaponFrame) >> 2);

                    if (pPlayer->hbomb_hold_delay)
                    {
                        G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits | 4, weaponPal,
                                         W_DEVISTATOR_LEFT);
                        G_DrawWeaponTile((devastatorFrames[*weaponFrame] >> 1) + weaponX + 268 - halfLookAng,
                                         devastatorFrames[*weaponFrame] + weaponY + 238 - weaponYOffset, DEVISTATOR + tileOffset, -32, weaponBits, weaponPal,
                                         W_DEVISTATOR_RIGHT);
                    }
                    else
                    {
                        G_DrawWeaponTile(-(devastatorFrames[*weaponFrame] >> 1) + weaponX + 30 - halfLookAng,
                                         devastatorFrames[*weaponFrame] + weaponY + 240 - weaponYOffset, DEVISTATOR + tileOffset, -32, weaponBits | 4, weaponPal,
                                         W_DEVISTATOR_LEFT);
                        G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits, weaponPal,
                                         W_DEVISTATOR_RIGHT);
                    }
                }
                else
                {
                    G_DrawWeaponTile(weaponX + 30 - halfLookAng, weaponY + 240 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits | 4, weaponPal,
                                     W_DEVISTATOR_LEFT);
                    G_DrawWeaponTile(weaponX + 268 - halfLookAng, weaponY + 238 - weaponYOffset, DEVISTATOR, weaponShade, weaponBits, weaponPal, W_DEVISTATOR_RIGHT);
                }
                break;

            case FREEZE_WEAPON:
                if (!WORLDTOUR && !(duke3d_globalflags & DUKE3D_NO_WIDESCREEN_PINNING) && DUKE)
                    weaponBits |= 512;

                if ((*weaponFrame) < (PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime) + 1) && (*weaponFrame) > 0)
                {
                    static uint8_t freezerFrames[] = { 0, 0, 1, 1, 2, 2 };

                    if (doAnim)
                    {
                        weaponX += wrand() & 3;
                        weaponY += wrand() & 3;
                    }
                    weaponYOffset -= 16;
                    G_DrawWeaponTile(weaponX + 210 - (pPlayer->look_ang >> 1), weaponY + 261 - weaponYOffset, WORLDTOUR ? FREEZEFIREWIDE : FREEZE + 2, -32,
                                     weaponBits & ~RS_LERP, weaponPal, W_FREEZE_BASE);
                    G_DrawWeaponTile(weaponX + 210 - (pPlayer->look_ang >> 1), weaponY + 235 - weaponYOffset, FREEZE + 3 + freezerFrames[*weaponFrame % 6], -32,
                                     weaponBits & ~RS_LERP, weaponPal, W_FREEZE_TOP);
                }
                else
                    G_DrawWeaponTile(weaponX + 210 - (pPlayer->look_ang >> 1), weaponY + 261 - weaponYOffset, WT_WIDE(FREEZE), weaponShade, weaponBits, weaponPal,
                                     W_FREEZE_BASE);
                break;

            case FLAMETHROWER_WEAPON:
                if ((*weaponFrame) < (PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime) + 1) && (*weaponFrame) > 0 && sector[pPlayer->cursectnum].lotag != ST_2_UNDERWATER)
                {
                    static uint8_t incineratorFrames[] = { 0, 0, 1, 1, 2, 2 };

                    if (doAnim)
                    {
                        weaponX += wrand() & 1;
                        weaponY += wrand() & 1;
                    }
                    weaponYOffset -= 16;
                    G_DrawWeaponTile(weaponX + 210 - (pPlayer->look_ang >> 1), weaponY + 261 - weaponYOffset, FLAMETHROWERFIRE, -32, weaponBits & ~RS_LERP, weaponPal,
                                     W_FREEZE_BASE);
                    G_DrawWeaponTile(weaponX + 210 - (pPlayer->look_ang >> 1), weaponY + 235 - weaponYOffset,
                                     FLAMETHROWERFIRE + 1 + incineratorFrames[*weaponFrame % 6], -32, weaponBits & ~RS_LERP, weaponPal, W_FREEZE_TOP);
                }
                else
                {
                    G_DrawWeaponTile(weaponX + 210 - (pPlayer->look_ang >> 1), weaponY + 261 - weaponYOffset, FLAMETHROWER, weaponShade, weaponBits, weaponPal,
                                     W_FREEZE_BASE);
                    G_DrawWeaponTile(weaponX + 210 - (pPlayer->look_ang >> 1), weaponY + 261 - weaponYOffset, FLAMETHROWERPILOT, weaponShade,
                                     weaponBits | RS_FORCELERP, weaponPal, W_FREEZE_TOP);
                }
                break;

            case GROW_WEAPON:
            case SHRINKER_WEAPON:
            {
                bool const isExpander = currentWeapon == GROW_WEAPON;

                weaponX += 28;
                weaponY += 18;

                if (WW2GI)
                {
                    if (*weaponFrame == 0)
                    {
                        // the 'at rest' display
                        if (isExpander)
                        {
                            G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER - 2, weaponShade, weaponBits, weaponPal,
                                             W_SHRINKER);
                            break;
                        }
                        else if (pPlayer->ammo_amount[currentWeapon] > 0)
                        {
                            G_DrawWeaponTileUnfaded(weaponX + 184 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + 2,
                                                    16 - (sintable[pPlayer->random_club_frame & 2047] >> 10), weaponBits, 0, W_SHRINKER_CRYSTAL);
                            G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER, weaponShade, weaponBits, weaponPal,
                                             W_SHRINKER);
                            break;
                        }
                    }
                    else
                    {
                        // the 'active' display.
                        if (doAnim)
                        {
                            weaponX += wrand() & 3;
                            weaponYOffset += wrand() & 3;
                        }

                        int const totalTime = PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime);
                        int const reloadTime = PWEAPON(screenpeek, pPlayer->curr_weapon, Reload);

                        if (*weaponFrame < totalTime)
                        {
                            if (*weaponFrame >= PWEAPON(screenpeek, pPlayer->curr_weapon, FireDelay))
                            {
                                // after fire time.
                                // lower weapon to reload cartridge (not clip)
                                weaponYOffset -= (isExpander ? 15 : 10) * (totalTime - (*weaponFrame));
                            }
                        }
                        // else we are in 'reload time'
                        else
                        {
                            weaponYOffset -= (*weaponFrame < ((reloadTime - totalTime) / 2 + totalTime))
                                             ? (isExpander ? 5 : 10) * ((*weaponFrame) - totalTime) // D
                                             : 10 * (reloadTime - (*weaponFrame)); // U
                        }
                    }

                    G_DrawWeaponTileUnfaded(weaponX + 184 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + 3 + ((*weaponFrame) & 3), -32, weaponBits,
                                            isExpander ? 2 : 0, 0 /*W_SHRINKER_CRYSTAL*/);

                    G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + (isExpander ? -1 : 1), weaponShade,
                                     weaponBits, weaponPal, 0 /*W_SHRINKER*/);

                    break;
                }

                if ((*weaponFrame) < PWEAPON(screenpeek, pPlayer->curr_weapon, TotalTime) && (*weaponFrame) > 0)
                {
                    if (doAnim)
                    {
                        weaponX += wrand() & 3;
                        weaponYOffset += (wrand() & 3);
                    }

                    G_DrawWeaponTileUnfaded(weaponX + 184 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + 3 + ((*weaponFrame) & 3), -32,
                                            weaponBits & ~RS_LERP, isExpander ? 2 : 0, W_SHRINKER_CRYSTAL);
                    G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 240 - weaponYOffset, WT_WIDE(SHRINKER) + (isExpander ? -1 : 1), weaponShade,
                                     weaponBits & ~RS_LERP, weaponPal, W_SHRINKER);
                }
                else
                {
                    G_DrawWeaponTileUnfaded(weaponX + 184 - halfLookAng, weaponY + 240 - weaponYOffset, SHRINKER + 2,
                                            16 - (sintable[pPlayer->random_club_frame & 2047] >> 10), weaponBits | RS_FORCELERP,
                                            isExpander ? 2 : 0, W_SHRINKER_CRYSTAL);
                    G_DrawWeaponTile(weaponX + 188 - halfLookAng, weaponY + 240 - weaponYOffset, WT_WIDE(SHRINKER) + (isExpander ? -2 : 0), weaponShade,
                                     weaponBits, weaponPal, W_SHRINKER);
                }
                break;
            }
            }
#endif
        }
    }

enddisplayweapon:;
#ifndef EDUKE32_STANDALONE
    P_DisplaySpit();
#endif
}

#define TURBOTURNTIME (TICRATE/8) // 7
#define NORMALTURN    15
#define PREAMBLETURN  5
#define NORMALKEYMOVE 40
#define MAXVEL        ((NORMALKEYMOVE*2)+10)
#define MAXSVEL       ((NORMALKEYMOVE*2)+10)
#define MAXANGVEL     1024
#define MAXHORIZVEL   256

int32_t g_myAimMode, g_myAimStat, g_oldAimStat;
uint64_t g_lastInputTicks;

enum inputlock_t
{
    IL_NOANGLE = 0x1,
    IL_NOHORIZ = 0x2,
    IL_NOMOVE  = 0x4,

    IL_NOTHING = IL_NOANGLE|IL_NOHORIZ|IL_NOMOVE,
};

static int P_CheckLockedMovement(int const playerNum)
{
    auto &     thisPlayer = g_player[playerNum];
    auto const pPlayer    = thisPlayer.ps;

    if (sprite[pPlayer->i].extra <= 0 || (pPlayer->dead_flag && !ud.god) || pPlayer->fist_incs || pPlayer->transporter_hold > 2 || (pPlayer->hard_landing && !FURY) || pPlayer->access_incs > 0
        || pPlayer->knee_incs > 0
        || (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == TRIPBOMB_WEAPON && pPlayer->kickback_pic > 1
            && pPlayer->kickback_pic < PWEAPON(playerNum, pPlayer->curr_weapon, FireDelay)))
        return IL_NOTHING;

    if (pPlayer->on_crane >= 0)
        return IL_NOMOVE|IL_NOANGLE;

    if (pPlayer->newowner != -1)
        return IL_NOANGLE|IL_NOHORIZ;

    if (pPlayer->return_to_center > 0 || thisPlayer.horizRecenter)
        return IL_NOHORIZ;

    return 0;
}

void P_UpdateAngles(int const playerNum, input_t &input)
{
    auto      &thisPlayer = g_player[playerNum];
    auto const pPlayer    = thisPlayer.ps;

    if (VM_HaveEvent(EVENT_PREUPDATEANGLES))
    {
        input_t pInput = thisPlayer.input;
        thisPlayer.input = input;
        VM_OnEvent(EVENT_PREUPDATEANGLES, pPlayer->i, playerNum);
        input = thisPlayer.input;
        thisPlayer.input = pInput;
    }

    auto const currentNanoTicks  = timerGetNanoTicks();
    auto       elapsedInputTicks = currentNanoTicks - thisPlayer.lastViewUpdate;

    if (!thisPlayer.lastViewUpdate)
        elapsedInputTicks = 0;

    thisPlayer.lastViewUpdate = currentNanoTicks;

    auto scaleToInterval = [=](double x) { return x * REALGAMETICSPERSEC / ((double)timerGetNanoTickRate() / min<double>(elapsedInputTicks, timerGetNanoTickRate())); };

    int const movementLocked = P_CheckLockedMovement(playerNum);

    if ((movementLocked & IL_NOTHING) != IL_NOTHING)
    {
        if (!(movementLocked & IL_NOANGLE))
            pPlayer->q16ang = fix16_sadd(pPlayer->q16ang, input.q16avel) & 0x7FFFFFF;

        if (!(movementLocked & IL_NOHORIZ))
        {
            float horizAngle  = atan2f(pPlayer->q16horiz - F16(100), F16(128)) * (512.f / fPI) + fix16_to_float(input.q16horz);
            pPlayer->q16horiz = F16(100) + Blrintf(F16(128) * tanf(horizAngle * (fPI / 512.f)));
        }
    }

    // A horiz diff of 128 equal 45 degrees, so we convert horiz to 1024 angle units

    if (thisPlayer.horizAngleAdjust)
    {
        float const horizAngle
        = atan2f(pPlayer->q16horiz - F16(100), F16(128)) * (512.f / fPI) + scaleToInterval(thisPlayer.horizAngleAdjust);
        pPlayer->q16horiz = F16(100) + Blrintf(F16(128) * tanf(horizAngle * (fPI / 512.f)));
    }
    else if (pPlayer->return_to_center > 0 || thisPlayer.horizRecenter)
    {
        pPlayer->q16horiz = fix16_sadd(pPlayer->q16horiz, fix16_from_float(scaleToInterval(fix16_to_float(F16(66.666) - fix16_sdiv(pPlayer->q16horiz, F16(1.5))))));

        if ((!pPlayer->return_to_center && thisPlayer.horizRecenter) || (pPlayer->q16horiz >= F16(99.9) && pPlayer->q16horiz <= F16(100.1)))
        {
            pPlayer->q16horiz = F16(100);
            pPlayer->return_to_center = 0;
            thisPlayer.horizRecenter = false;
        }
    }
    int const sectorLotag = pPlayer->cursectnum != -1 ? sector[pPlayer->cursectnum].lotag : 0;
    // calculates automatic view angle for playing without a mouse
    if (!pPlayer->aim_mode && pPlayer->on_ground && sectorLotag != ST_2_UNDERWATER && (sector[pPlayer->cursectnum].floorstat & 2))
    {
        // this is some kind of horse shit approximation of where the player is looking, I guess?
        vec2_t const adjustedPosition = { pPlayer->pos.x + (sintable[(fix16_to_int(pPlayer->q16ang) + 512) & 2047] >> 5),
                                          pPlayer->pos.y + (sintable[fix16_to_int(pPlayer->q16ang) & 2047] >> 5) };
        int16_t currentSector = pPlayer->cursectnum;

        updatesector(adjustedPosition.x, adjustedPosition.y, &currentSector);

        if (currentSector >= 0)
        {
#ifdef YAX_ENABLE
            int const slopeZ = yax_getflorzofslope(currentSector, adjustedPosition);
            int const floorZ = yax_getflorzofslope(pPlayer->cursectnum, pPlayer->pos.xy);
#else
            int const slopeZ = getflorzofslope(currentSector, adjustedPosition.x, adjustedPosition.y);
            int const floorZ = getflorzofslope(pPlayer->cursectnum, pPlayer->pos.x, pPlayer->pos.y);
#endif

            if ((pPlayer->cursectnum == currentSector) || (klabs(floorZ - slopeZ) <= ZOFFSET6))
            {
                pPlayer->q16horizoff = fix16_from_float(fix16_to_float(pPlayer->q16horizoff) + scaleToInterval((floorZ - slopeZ) * 160 * (1.f/65536.f)));
                // LOG_F(INFO, "%g", fix16_to_float(pPlayer->q16horizoff));
            }
        }
    }

    // view centering is only used if there's no input on the right stick (looking/aiming) and the player is moving forward/backward, not strafing
    if (pPlayer->aim_mode&AM_CENTERING && !input.q16avel && !input.q16horz && input.fvel)
    {
        if (pPlayer->q16horiz >= F16(99) && pPlayer->q16horiz <= F16(100))
            thisPlayer.horizRecenter = true;
        else if (pPlayer->q16horiz < F16(99))
            pPlayer->q16horiz = fix16_min(fix16_sadd(pPlayer->q16horiz, fix16_from_float(scaleToInterval(ud.config.JoystickViewCentering))), F16(100));
        else if (pPlayer->q16horiz > F16(100))
            pPlayer->q16horiz = fix16_max(fix16_ssub(pPlayer->q16horiz, fix16_from_float(scaleToInterval(ud.config.JoystickViewCentering))), F16(100));
    }

    int32_t Zvel, shootAng;

// FIXME
    if (pPlayer->aim_mode&AM_AIMASSIST && GetAutoAimAng(pPlayer->i, playerNum, 0, ZOFFSET3, 0 + 2, &pPlayer->pos, 256, &Zvel, &shootAng) != -1)
    {
        if (pPlayer->q16horiz == F16(100))
        {
            fix16_t const f      = F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff;
            fix16_t const target = Blrintf(F16(128) * tanf((Zvel / 32.f) * (fPI / 512.f)));
            fix16_t const scaled = fix16_from_float(scaleToInterval(1.5));

            if (f > target + scaled)
                pPlayer->q16horizoff = fix16_sadd(pPlayer->q16horizoff, scaled);
            else if (f < target - scaled)
                pPlayer->q16horizoff = fix16_ssub(pPlayer->q16horizoff, scaled);
        }
    }
    else if (pPlayer->q16horizoff > F16(0))
    {
        pPlayer->q16horizoff = fix16_ssub(pPlayer->q16horizoff, fix16_from_float(scaleToInterval(fix16_to_float(fix16_div(pPlayer->q16horizoff, F16(8))))));
        pPlayer->q16horizoff = fix16_max(pPlayer->q16horizoff, 0);
    }
    else if (pPlayer->q16horizoff < F16(0))
    {
        pPlayer->q16horizoff = fix16_sadd(pPlayer->q16horizoff, fix16_from_float(scaleToInterval(-fix16_to_float(fix16_div(pPlayer->q16horizoff, F16(8))))));
        pPlayer->q16horizoff = fix16_min(pPlayer->q16horizoff, 0);
    }

    if (thisPlayer.horizSkew)
        pPlayer->q16horiz = fix16_sadd(pPlayer->q16horiz, fix16_from_float(scaleToInterval(thisPlayer.horizSkew)));

    pPlayer->q16horiz    = fix16_clamp(pPlayer->q16horiz, F16(HORIZ_MIN), F16(HORIZ_MAX));
    pPlayer->q16horizoff = fix16_clamp(pPlayer->q16horizoff, F16(HORIZ_MIN), F16(HORIZ_MAX));

    if (pPlayer->newowner == -1)
        sprite[pPlayer->i].ang = fix16_to_int(pPlayer->q16ang);

    if (VM_HaveEvent(EVENT_POSTUPDATEANGLES))
    {
        input_t pInput = thisPlayer.input;
        thisPlayer.input = input;
        VM_OnEvent(EVENT_POSTUPDATEANGLES, pPlayer->i, playerNum);
        input = thisPlayer.input;
        thisPlayer.input = pInput;
    }
}


void P_GetInput(int const playerNum)
{
    auto      &thisPlayer = g_player[playerNum];
    auto const pPlayer    = thisPlayer.ps;
    ControlInfo info;

    if (g_cheatBufLen > 1 || (pPlayer->gm & (MODE_MENU|MODE_TYPE)) || (ud.pause_on && !KB_KeyPressed(sc_Pause)))
    {
        if (!(pPlayer->gm&MODE_MENU))
            CONTROL_GetInput(&info);

        thisPlayer.lastViewUpdate = 0;
        localInput = {};
        localInput.bits    = (((int32_t)g_gameQuit) << SK_GAMEQUIT);
        localInput.extbits = BIT(EK_CHAT_MODE);

        return;
    }

    CONTROL_ProcessBinds();

    if (ud.mouseaiming)
        g_myAimMode = BUTTON(gamefunc_Mouse_Aiming);
    else
    {
        g_oldAimStat = g_myAimStat;
        g_myAimStat  = BUTTON(gamefunc_Mouse_Aiming);

        if (g_myAimStat > g_oldAimStat)
        {
            g_myAimMode ^= 1;
            P_DoQuote(QUOTE_MOUSE_AIMING_OFF + g_myAimMode, pPlayer);
        }
    }

    CONTROL_GetInput(&info);

    if (ud.config.MouseBias)
    {
        if (klabs(info.mousex) > klabs(info.mousey))
            info.mousey = tabledivide32_noinline(info.mousey, ud.config.MouseBias);
        else
            info.mousex = tabledivide32_noinline(info.mousex, ud.config.MouseBias);
    }

    if (ud.config.JoystickAimWeight)
    {
        int const absyaw = klabs(info.dyaw);
        int const abspitch = klabs(info.dpitch);
        //int const origyaw = info.dyaw;
        //int const origpitch = info.dpitch;

        if (absyaw > abspitch)
        {
            if (info.dpitch > 0)
                info.dpitch = max(0, info.dpitch - tabledivide32_noinline(absyaw - abspitch, 8 - ud.config.JoystickAimWeight));
            else if (info.dpitch < 0)
                info.dpitch = min(0, info.dpitch + tabledivide32_noinline(absyaw - abspitch, 8 - ud.config.JoystickAimWeight));

            //OSD_Printf("pitch %d -> %d\n",origpitch, info.dpitch);
        }
        else if (abspitch > absyaw)
        {
            if (info.dyaw > 0)
                info.dyaw = max(0, info.dyaw - tabledivide32_noinline(abspitch - absyaw, 8 - ud.config.JoystickAimWeight));
            else if (info.dyaw < 0)
                info.dyaw = min(0, info.dyaw + tabledivide32_noinline(abspitch - absyaw, 8 - ud.config.JoystickAimWeight));

            //OSD_Printf("yaw %d -> %d\n",origyaw, info.dyaw);
        }
    }

    // JBF: Run key behaviour is selectable
    int const       playerRunning = (ud.runkey_mode) ? (BUTTON(gamefunc_Run) | ud.auto_run) : (ud.auto_run ^ BUTTON(gamefunc_Run));
    int const       turnAmount    = playerRunning ? (NORMALTURN << 1) : NORMALTURN;
    int const       keyMove       = playerRunning ? (NORMALKEYMOVE << 1) : NORMALKEYMOVE;
    constexpr float analogExtent  = 32767.f;  // KEEPINSYNC sdlayer.cpp

    input_t input {};

    auto const currentNanoTicks  = timerGetNanoTicks();
    auto       elapsedInputTicks = currentNanoTicks - g_lastInputTicks;

    if (!g_lastInputTicks)
        elapsedInputTicks = 0;

    g_lastInputTicks = currentNanoTicks;

    auto scaleToInterval = [=](double x) { return x * REALGAMETICSPERSEC / ((double)timerGetNanoTickRate() / min<double>(elapsedInputTicks, timerGetNanoTickRate())); };

    if (BUTTON(gamefunc_Strafe))
    {
        static int strafeyaw;

        input.svel = -(info.mousex + strafeyaw) >> 3;
        strafeyaw  = (info.mousex + strafeyaw) % 8;

        input.svel -= lrint(scaleToInterval(info.dyaw * keyMove / analogExtent));
    }
    else
    {
        input.q16avel = fix16_sadd(input.q16avel, fix16_sdiv(fix16_from_int(info.mousex), F16(32)));
        input.q16avel = fix16_sadd(input.q16avel, fix16_from_float(scaleToInterval(info.dyaw * 64.0 / analogExtent)));
    }

    if (g_myAimMode)
        input.q16horz = fix16_sadd(input.q16horz, fix16_sdiv(fix16_from_int(info.mousey), F16(64)));
    else
        input.fvel = -(info.mousey >> 3);

    if (ud.mouseflip) input.q16horz = -input.q16horz;

    input.q16horz = fix16_ssub(input.q16horz, fix16_from_float(scaleToInterval(info.dpitch * 16.0 / analogExtent)));
    input.svel -= lrint(scaleToInterval(info.dx * keyMove / analogExtent));
    input.fvel -= lrint(scaleToInterval(info.dz * keyMove / analogExtent));

    if (BUTTON(gamefunc_Strafe))
    {
        if (!localInput.svel)
        {
            if (BUTTON(gamefunc_Turn_Left) && !(pPlayer->movement_lock & 4))
                input.svel = keyMove;

            if (BUTTON(gamefunc_Turn_Right) && !(pPlayer->movement_lock & 8))
                input.svel = -keyMove;
        }
    }
    else
    {
        static int32_t turnHeldTime;
        static int32_t lastInputClock;  // MED
        int32_t const  elapsedTics = (int32_t)totalclock - lastInputClock;

        lastInputClock = (int32_t) totalclock;

        if (BUTTON(gamefunc_Turn_Left))
        {
            turnHeldTime += elapsedTics;
            input.q16avel = fix16_ssub(input.q16avel, fix16_from_float(scaleToInterval((turnHeldTime >= TURBOTURNTIME) ? (turnAmount << 1) : (PREAMBLETURN << 1))));
        }
        else if (BUTTON(gamefunc_Turn_Right))
        {
            turnHeldTime += elapsedTics;
            input.q16avel = fix16_sadd(input.q16avel, fix16_from_float(scaleToInterval((turnHeldTime >= TURBOTURNTIME) ? (turnAmount << 1) : (PREAMBLETURN << 1))));
        }
        else
            turnHeldTime = 0;
    }

    if (localInput.svel < keyMove && localInput.svel > -keyMove)
    {
        if (BUTTON(gamefunc_Strafe_Left) && !(pPlayer->movement_lock & 4))
            input.svel += keyMove;

        if (BUTTON(gamefunc_Strafe_Right) && !(pPlayer->movement_lock & 8))
            input.svel += -keyMove;
    }

    if (localInput.fvel < keyMove && localInput.fvel > -keyMove)
    {
        if (BUTTON(gamefunc_Move_Forward) && !(pPlayer->movement_lock & 1))
            input.fvel += keyMove;

        if (BUTTON(gamefunc_Move_Backward) && !(pPlayer->movement_lock & 2))
            input.fvel += -keyMove;
    }

    int weaponSelection;

    for (weaponSelection = gamefunc_Weapon_10; weaponSelection >= gamefunc_Weapon_1; --weaponSelection)
    {
        if (BUTTON(weaponSelection))
        {
            weaponSelection -= (gamefunc_Weapon_1 - 1);
            break;
        }
    }

    if (BUTTON(gamefunc_Last_Weapon))
        weaponSelection = 14;
    else if (BUTTON(gamefunc_Alt_Weapon))
        weaponSelection = 13;
    else if (BUTTON(gamefunc_Next_Weapon) || (BUTTON(gamefunc_Dpad_Select) && input.fvel > 0))
        weaponSelection = 12;
    else if (BUTTON(gamefunc_Previous_Weapon) || (BUTTON(gamefunc_Dpad_Select) && input.fvel < 0))
        weaponSelection = 11;
    else if (weaponSelection == gamefunc_Weapon_1-1)
        weaponSelection = 0;

    if (weaponSelection && (localInput.bits & SK_WEAPON_MASK) == 0)
        localInput.bits |= (weaponSelection << SK_WEAPON_BITS);

    localInput.bits |= (BUTTON(gamefunc_Fire) << SK_FIRE);
    localInput.bits |= (BUTTON(gamefunc_Open) << SK_OPEN);

    int const sectorLotag = pPlayer->cursectnum != -1 ? sector[pPlayer->cursectnum].lotag : 0;
    int const crouchable = sectorLotag != 2 && (sectorLotag != 1 || pPlayer->spritebridge) && !pPlayer->jetpack_on;

    if (pPlayer->cheat_phase < 1)
    {
        if (BUTTON(gamefunc_Toggle_Crouch))
        {
            pPlayer->crouch_toggle = !pPlayer->crouch_toggle && crouchable;

            if (crouchable)
                CONTROL_ClearButton(gamefunc_Toggle_Crouch);
        }

        if (BUTTON(gamefunc_Crouch) || BUTTON(gamefunc_Jump) || pPlayer->jetpack_on || (!crouchable && pPlayer->on_ground))
            pPlayer->crouch_toggle = 0;

        int const crouching = BUTTON(gamefunc_Crouch) || BUTTON(gamefunc_Toggle_Crouch) || pPlayer->crouch_toggle;

        localInput.bits |= (BUTTON(gamefunc_Jump) << SK_JUMP) | (crouching << SK_CROUCH);
    }

    localInput.bits |= (BUTTON(gamefunc_Aim_Up) || (BUTTON(gamefunc_Dpad_Aiming) && input.fvel > 0)) << SK_AIM_UP;
    localInput.bits |= (BUTTON(gamefunc_Aim_Down) || (BUTTON(gamefunc_Dpad_Aiming) && input.fvel < 0)) << SK_AIM_DOWN;
    localInput.bits |= (BUTTON(gamefunc_Center_View) << SK_CENTER_VIEW);

    localInput.bits |= (BUTTON(gamefunc_Look_Left) << SK_LOOK_LEFT) | (BUTTON(gamefunc_Look_Right) << SK_LOOK_RIGHT);
    localInput.bits |= (BUTTON(gamefunc_Look_Up) << SK_LOOK_UP) | (BUTTON(gamefunc_Look_Down) << SK_LOOK_DOWN);

    localInput.bits |= (playerRunning << SK_RUN);

    localInput.bits |= (BUTTON(gamefunc_Inventory_Left) || (BUTTON(gamefunc_Dpad_Select) && (input.svel > 0 || input.q16avel < 0))) << SK_INV_LEFT;
    localInput.bits |= (BUTTON(gamefunc_Inventory_Right) || (BUTTON(gamefunc_Dpad_Select) && (input.svel < 0 || input.q16avel > 0))) << SK_INV_RIGHT;
    localInput.bits |= (BUTTON(gamefunc_Inventory) << SK_INVENTORY);

    localInput.bits |= (BUTTON(gamefunc_Steroids) << SK_STEROIDS) | (BUTTON(gamefunc_NightVision) << SK_NIGHTVISION);
    localInput.bits |= (BUTTON(gamefunc_MedKit) << SK_MEDKIT) | (BUTTON(gamefunc_Holo_Duke) << SK_HOLODUKE);
    localInput.bits |= (BUTTON(gamefunc_Jetpack) << SK_JETPACK);

    localInput.bits |= BUTTON(gamefunc_Holster_Weapon) << SK_HOLSTER;
    localInput.bits |= BUTTON(gamefunc_Quick_Kick) << SK_QUICK_KICK;
    localInput.bits |= BUTTON(gamefunc_TurnAround) << SK_TURNAROUND;

    localInput.bits |= (g_myAimMode << SK_AIMMODE);
    localInput.bits |= (g_gameQuit << SK_GAMEQUIT);
    localInput.bits |= KB_KeyPressed(sc_Pause) << SK_PAUSE;
    localInput.bits |= ((uint32_t)KB_KeyPressed(sc_Escape)) << SK_ESCAPE;

    if (BUTTON(gamefunc_Dpad_Select))
    {
        input.fvel = 0;
        input.svel = 0;
        input.q16avel = 0;
    }
    else if (BUTTON(gamefunc_Dpad_Aiming))
        input.fvel = 0;

    if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_SEMIAUTO && BUTTON(gamefunc_Fire))
        CONTROL_ClearButton(gamefunc_Fire);

    localInput.extbits |= (BUTTON(gamefunc_Move_Forward) || (input.fvel > 0)) << EK_MOVE_FORWARD;
    localInput.extbits |= (BUTTON(gamefunc_Move_Backward) || (input.fvel < 0)) << EK_MOVE_BACKWARD;
    localInput.extbits |= (BUTTON(gamefunc_Strafe_Left) || (input.svel > 0)) << EK_STRAFE_LEFT;
    localInput.extbits |= (BUTTON(gamefunc_Strafe_Right) || (input.svel < 0)) << EK_STRAFE_RIGHT;
    localInput.extbits |= BUTTON(gamefunc_Turn_Left) << EK_TURN_LEFT;
    localInput.extbits |= BUTTON(gamefunc_Turn_Right) << EK_TURN_RIGHT;
    localInput.extbits |= BUTTON(gamefunc_Alt_Fire) << EK_ALT_FIRE;

    if (CONTROL_LastSeenInput == LastSeenInput::Joystick)
    {
        localInput.extbits |= (!!ud.config.JoystickViewCentering) << EK_GAMEPAD_CENTERING;
        localInput.extbits |= (!!ud.config.JoystickAimAssist) << EK_GAMEPAD_AIM_ASSIST;
    }

    // for access in the events
    input.bits = localInput.bits;
    input.extbits = localInput.extbits;

    if (!ud.recstat)
        P_UpdateAngles(playerNum, input);

    // in case bits were altered
    localInput.bits = input.bits;
    localInput.extbits = input.extbits;

    int const movementLocked = P_CheckLockedMovement(playerNum);

    if ((ud.scrollmode && ud.overhead_on) || (movementLocked & IL_NOTHING) == IL_NOTHING)
    {
        if (ud.scrollmode && ud.overhead_on)
        {
            ud.folfvel = input.fvel;
            ud.folsvel = input.svel;
            ud.folavel = fix16_to_int(input.q16avel);
        }

        localInput.fvel = localInput.svel = 0;
        localInput.q16avel = localInput.q16horz = 0;
    }
    else
    {
        if (!(movementLocked & IL_NOMOVE))
        {
            localInput.fvel = clamp(localInput.fvel + input.fvel, -MAXVEL, MAXVEL);
            localInput.svel = clamp(localInput.svel + input.svel, -MAXSVEL, MAXSVEL);
        }

        if (!(movementLocked & IL_NOANGLE))
            localInput.q16avel = fix16_sadd(localInput.q16avel, input.q16avel);

        if (!(movementLocked & IL_NOHORIZ))
        {
            float horizAngle   = atan2f(localInput.q16horz, F16(128)) * (512.f / fPI) + fix16_to_float(input.q16horz);
            localInput.q16horz = fix16_clamp(Blrintf(F16(128) * tanf(horizAngle * (fPI / 512.f))), F16(-MAXHORIZVEL), F16(MAXHORIZVEL));
        }
    }

}

static int32_t P_DoCounters(int playerNum)
{
    auto const pPlayer = g_player[playerNum].ps;

#ifndef EDUKE32_STANDALONE
    if (FURY)
        goto access_incs; // I'm sorry

    if (pPlayer->invdisptime > 0)
        pPlayer->invdisptime--;

    if (pPlayer->tipincs > 0)
        pPlayer->tipincs--;

    if (pPlayer->last_pissed_time > 0)
    {
        switch (--pPlayer->last_pissed_time)
        {
            case GAMETICSPERSEC * 219:
            {
                A_PlaySound(FLUSH_TOILET, pPlayer->i);
                if (playerNum == screenpeek || GTFLAGS(GAMETYPE_COOPSOUND))
                    A_PlaySound(DUKE_PISSRELIEF, pPlayer->i);
            }
            break;
            case GAMETICSPERSEC * 218:
            {
                pPlayer->holster_weapon = 0;
                pPlayer->weapon_pos     = WEAPON_POS_RAISE;
            }
            break;
        }
    }

    if (pPlayer->crack_time > 0)
    {
        if (--pPlayer->crack_time == 0)
        {
            pPlayer->knuckle_incs = 1;
            pPlayer->crack_time   = PCRACKTIME;
        }
    }

    if (pPlayer->inv_amount[GET_STEROIDS] > 0 && pPlayer->inv_amount[GET_STEROIDS] < 400)
    {
        if (--pPlayer->inv_amount[GET_STEROIDS] == 0)
            P_SelectNextInvItem(pPlayer);

        if (!(pPlayer->inv_amount[GET_STEROIDS] & 7))
            if (playerNum == screenpeek || GTFLAGS(GAMETYPE_COOPSOUND))
                A_PlaySound(DUKE_HARTBEAT, pPlayer->i);
    }

    if (pPlayer->heat_on && pPlayer->inv_amount[GET_HEATS] > 0)
    {
        if (--pPlayer->inv_amount[GET_HEATS] == 0)
        {
            pPlayer->heat_on = 0;
            P_SelectNextInvItem(pPlayer);
            A_PlaySound(NITEVISION_ONOFF, pPlayer->i);
            P_UpdateScreenPal(pPlayer);
        }
    }

    if (pPlayer->holoduke_on >= 0)
    {
        if (--pPlayer->inv_amount[GET_HOLODUKE] <= 0)
        {
            A_PlaySound(TELEPORTER, pPlayer->i);
            pPlayer->holoduke_on = -1;
            P_SelectNextInvItem(pPlayer);
        }
    }

    if (pPlayer->jetpack_on && pPlayer->inv_amount[GET_JETPACK] > 0)
    {
        if (--pPlayer->inv_amount[GET_JETPACK] <= 0)
        {
            pPlayer->jetpack_on = 0;
            P_SelectNextInvItem(pPlayer);
            A_PlaySound(DUKE_JETPACK_OFF, pPlayer->i);
            S_StopEnvSound(DUKE_JETPACK_IDLE, pPlayer->i);
            S_StopEnvSound(DUKE_JETPACK_ON, pPlayer->i);
        }
    }

    if (pPlayer->quick_kick > 0 && sprite[pPlayer->i].pal != 1)
    {
        pPlayer->last_quick_kick = pPlayer->quick_kick + 1;

        if (--pPlayer->quick_kick == 8)
            A_Shoot(pPlayer->i, KNEE);
    }
    else if (pPlayer->last_quick_kick > 0)
        --pPlayer->last_quick_kick;

access_incs:
#endif

    if (pPlayer->access_incs && sprite[pPlayer->i].pal != 1)
    {
        ++pPlayer->access_incs;

        if (sprite[pPlayer->i].extra <= 0)
            pPlayer->access_incs = 12;

        if (pPlayer->access_incs == 12)
        {
            if (pPlayer->access_spritenum >= 0)
            {
                P_ActivateSwitch(playerNum, pPlayer->access_spritenum, 1);
                switch (sprite[pPlayer->access_spritenum].pal)
                {
                    case 0: pPlayer->got_access  &= (0xffff - 0x1); break;
                    case 21: pPlayer->got_access &= (0xffff - 0x2); break;
                    case 23: pPlayer->got_access &= (0xffff - 0x4); break;
                }
                pPlayer->access_spritenum = -1;
            }
            else
            {
                P_ActivateSwitch(playerNum,pPlayer->access_wallnum,0);
                switch (wall[pPlayer->access_wallnum].pal)
                {
                    case 0: pPlayer->got_access  &= (0xffff - 0x1); break;
                    case 21: pPlayer->got_access &= (0xffff - 0x2); break;
                    case 23: pPlayer->got_access &= (0xffff - 0x4); break;
                }
            }
        }

        if (pPlayer->access_incs > 20)
        {
            pPlayer->access_incs  = 0;
            pPlayer->weapon_pos   = WEAPON_POS_RAISE;
            pPlayer->kickback_pic = 0;
        }
    }

    if (pPlayer->cursectnum >= 0 && pPlayer->scuba_on == 0 && sector[pPlayer->cursectnum].lotag == ST_2_UNDERWATER)
    {
        if (pPlayer->inv_amount[GET_SCUBA] > 0)
        {
            pPlayer->scuba_on   = 1;
            pPlayer->inven_icon = ICON_SCUBA;
            P_DoQuote(QUOTE_SCUBA_ON, pPlayer);
        }
        else
        {
            if (pPlayer->airleft > 0)
                --pPlayer->airleft;
            else
            {
                pPlayer->extra_extra8 += 32;
                if (pPlayer->last_extra < (pPlayer->max_player_health >> 1) && (pPlayer->last_extra & 3) == 0)
                    A_PlaySound(DUKE_LONGTERM_PAIN, pPlayer->i);
            }
        }
    }
    else if (pPlayer->inv_amount[GET_SCUBA] > 0 && pPlayer->scuba_on)
    {
        pPlayer->inv_amount[GET_SCUBA]--;
        if (pPlayer->inv_amount[GET_SCUBA] == 0)
        {
            pPlayer->scuba_on = 0;
            P_SelectNextInvItem(pPlayer);
        }
    }

#ifndef EDUKE32_STANDALONE
    if (!FURY && pPlayer->knuckle_incs)
    {
        if (++pPlayer->knuckle_incs == 10)
        {
            if (!WW2GI)
            {
                if (totalclock > 1024)
                    if (playerNum == screenpeek || GTFLAGS(GAMETYPE_COOPSOUND))
                    {
                        if (wrand()&1)
                            A_PlaySound(DUKE_CRACK,pPlayer->i);
                        else A_PlaySound(DUKE_CRACK2,pPlayer->i);
                    }

                A_PlaySound(DUKE_CRACK_FIRST,pPlayer->i);
            }
        }
        else if (pPlayer->knuckle_incs == 22 || TEST_SYNC_KEY(g_player[playerNum].input.bits, SK_FIRE))
            pPlayer->knuckle_incs=0;

        return 1;
    }
#endif

    return 0;
}

int16_t WeaponPickupSprites[MAX_WEAPONS] = { KNEE__, FIRSTGUNSPRITE__, SHOTGUNSPRITE__,
        CHAINGUNSPRITE__, RPGSPRITE__, HEAVYHBOMB__, SHRINKERSPRITE__, DEVISTATORSPRITE__,
        TRIPBOMBSPRITE__, FREEZESPRITE__, HEAVYHBOMB__, SHRINKERSPRITE__, FLAMETHROWERSPRITE__
                                           };
// this is used for player deaths
void P_DropWeapon(int const playerNum)
{
    auto const pPlayer       = g_player[playerNum].ps;
    int const  currentWeapon = PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike);

    if ((unsigned)currentWeapon >= MAX_WEAPONS)
        return;

    if (krand() & 1)
        A_Spawn(pPlayer->i, WeaponPickupSprites[currentWeapon]);
#ifndef EDUKE32_STANDALONE
    else if (!FURY)
        switch (PWEAPON(playerNum, currentWeapon, WorksLike))
        {
            case RPG_WEAPON:
            case HANDBOMB_WEAPON: A_Spawn(pPlayer->i, EXPLOSION2); break;
        }
#endif
}

void P_AddAmmo(DukePlayer_t * const pPlayer, int const weaponNum, int const addAmount)
{
    pPlayer->ammo_amount[weaponNum] += addAmount;

    if (pPlayer->ammo_amount[weaponNum] > pPlayer->max_ammo_amount[weaponNum])
        pPlayer->ammo_amount[weaponNum] = pPlayer->max_ammo_amount[weaponNum];
}

static void P_AddWeaponNoSwitch(DukePlayer_t * const p, int const weaponNum)
{
    int const playerNum = P_Get(p->i);  // PASS_SNUM?

    if ((p->gotweapon & (1<<weaponNum)) == 0)
    {
        p->gotweapon |= (1<<weaponNum);

#ifndef EDUKE32_STANDALONE
        if (!FURY && weaponNum == SHRINKER_WEAPON)
            p->gotweapon |= (1<<GROW_WEAPON);
#endif
    }

    if (PWEAPON(playerNum, p->curr_weapon, SelectSound) > 0)
        S_StopEnvSound(PWEAPON(playerNum, p->curr_weapon, SelectSound), p->i);

    if (PWEAPON(playerNum, weaponNum, SelectSound) > 0)
        A_PlaySound(PWEAPON(playerNum, weaponNum, SelectSound), p->i);
}

static void P_ChangeWeapon(DukePlayer_t * const pPlayer, int const weaponNum)
{
    int const    playerNum     = P_Get(pPlayer->i);  // PASS_SNUM?
    int8_t const currentWeapon = pPlayer->curr_weapon;

    if (pPlayer->reloading)
        return;

    int eventReturn = 0;

    if (pPlayer->curr_weapon != weaponNum && VM_HaveEvent(EVENT_CHANGEWEAPON))
        eventReturn = VM_OnEventWithReturn(EVENT_CHANGEWEAPON,pPlayer->i, playerNum, weaponNum);

    if (eventReturn == -1)
        return;

    if (eventReturn != -2)
        pPlayer->curr_weapon = weaponNum;

    pPlayer->random_club_frame = 0;

    if (pPlayer->weapon_pos == 0)
    {
        pPlayer->weapon_pos = -1;
        pPlayer->last_weapon = currentWeapon;
    }
    else if ((unsigned)pPlayer->weapon_pos < WEAPON_POS_RAISE)
    {
        pPlayer->weapon_pos = -pPlayer->weapon_pos;
        pPlayer->last_weapon = currentWeapon;
    }
    else if (pPlayer->last_weapon == weaponNum)
    {
        pPlayer->last_weapon = -1;
        pPlayer->weapon_pos = -pPlayer->weapon_pos;
    }

    if (pPlayer->holster_weapon)
    {
        pPlayer->weapon_pos = WEAPON_POS_RAISE;
        pPlayer->holster_weapon = 0;
        pPlayer->last_weapon = -1;
    }

    if (currentWeapon != pPlayer->curr_weapon &&
        !(PWEAPON(playerNum, currentWeapon, WorksLike) == HANDREMOTE_WEAPON && PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == HANDBOMB_WEAPON) &&
        !(PWEAPON(playerNum, currentWeapon, WorksLike) == HANDBOMB_WEAPON && PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == HANDREMOTE_WEAPON))
    {
        pPlayer->last_used_weapon = currentWeapon;
    }

    pPlayer->kickback_pic = 0;

    P_SetWeaponGamevars(playerNum, pPlayer);
}

void P_AddWeapon(DukePlayer_t *pPlayer, int weaponNum, int switchWeapon)
{
    P_AddWeaponNoSwitch(pPlayer, weaponNum);

    if (switchWeapon)
        P_ChangeWeapon(pPlayer, weaponNum);
}

void P_SelectNextInvItem(DukePlayer_t *pPlayer)
{
    if (pPlayer->inv_amount[GET_FIRSTAID] > 0)
        pPlayer->inven_icon = ICON_FIRSTAID;
    else if (pPlayer->inv_amount[GET_STEROIDS] > 0)
        pPlayer->inven_icon = ICON_STEROIDS;
    else if (pPlayer->inv_amount[GET_JETPACK] > 0)
        pPlayer->inven_icon = ICON_JETPACK;
    else if (pPlayer->inv_amount[GET_HOLODUKE] > 0)
        pPlayer->inven_icon = ICON_HOLODUKE;
    else if (pPlayer->inv_amount[GET_HEATS] > 0)
        pPlayer->inven_icon = ICON_HEATS;
    else if (pPlayer->inv_amount[GET_SCUBA] > 0)
        pPlayer->inven_icon = ICON_SCUBA;
    else if (pPlayer->inv_amount[GET_BOOTS] > 0)
        pPlayer->inven_icon = ICON_BOOTS;
    else
        pPlayer->inven_icon = ICON_NONE;
}

void P_CheckWeapon(DukePlayer_t *pPlayer)
{
    if (pPlayer->reloading || (unsigned)pPlayer->curr_weapon >= MAX_WEAPONS)
        return;

    int playerNum, weaponNum;

    if (pPlayer->wantweaponfire >= 0)
    {
        weaponNum = pPlayer->wantweaponfire;
        pPlayer->wantweaponfire = -1;

        if (weaponNum == pPlayer->curr_weapon)
            return;

        if ((pPlayer->gotweapon & (1<<weaponNum)) && pPlayer->ammo_amount[weaponNum] > 0)
        {
            P_AddWeapon(pPlayer, weaponNum, 1);
            return;
        }
    }

    weaponNum = pPlayer->curr_weapon;

    if ((pPlayer->gotweapon & (1<<weaponNum)) && (pPlayer->ammo_amount[weaponNum] > 0 || !(pPlayer->weaponswitch & 2)))
        return;

    playerNum  = P_Get(pPlayer->i);

    int wpnInc = 0;

    for (wpnInc = 0; wpnInc <= FREEZE_WEAPON; ++wpnInc)
    {
        weaponNum = g_player[playerNum].wchoice[wpnInc];
        if (VOLUMEONE && weaponNum > SHRINKER_WEAPON)
            continue;

        if (weaponNum == KNEE_WEAPON)
            weaponNum = FREEZE_WEAPON;
        else weaponNum--;

        if (weaponNum == KNEE_WEAPON || ((pPlayer->gotweapon & (1<<weaponNum)) && pPlayer->ammo_amount[weaponNum] > 0))
            break;
    }

    if (wpnInc == HANDREMOTE_WEAPON)
        weaponNum = KNEE_WEAPON;

    // Found the weapon

    P_ChangeWeapon(pPlayer, weaponNum);
}

static void DoWallTouchDamage(const DukePlayer_t *pPlayer, int32_t wallNum)
{
    vec3_t const davect = { pPlayer->pos.x + (sintable[(fix16_to_int(pPlayer->q16ang) + 512) & 2047] >> 9),
                      pPlayer->pos.y + (sintable[fix16_to_int(pPlayer->q16ang) & 2047] >> 9), pPlayer->pos.z };

    A_DamageWall(pPlayer->i, wallNum, davect, -1);
}

static void P_CheckTouchDamage(DukePlayer_t *pPlayer, int touchObject)
{
    if ((touchObject = VM_OnEventWithReturn(EVENT_CHECKTOUCHDAMAGE, pPlayer->i, P_Get(pPlayer->i), touchObject)) == -1)
        return;

    if ((touchObject & 49152) == 49152)
    {
#ifndef EDUKE32_STANDALONE
        int const touchSprite = touchObject & (MAXSPRITES - 1);

        if (!FURY && sprite[touchSprite].picnum == CACTUS)
        {
            if (pPlayer->hurt_delay < 8)
            {
                sprite[pPlayer->i].extra -= 5;

                pPlayer->hurt_delay = 16;
                P_PalFrom(pPlayer, 32, 32, 0, 0);
                A_PlaySound(DUKE_LONGTERM_PAIN, pPlayer->i);
            }
        }
#endif
        return;
    }

    if ((touchObject & 49152) != 32768)
        return;

    int const touchWall = touchObject & (MAXWALLS-1);

    if (pPlayer->hurt_delay > 0)
        pPlayer->hurt_delay--;
    else if (wall[touchWall].cstat & FORCEFIELD_CSTAT)
    {
        int const forcePic = G_GetForcefieldPicnum(touchWall);

        switch (tileGetMapping(forcePic))
        {
        case W_FORCEFIELD__:
            sprite[pPlayer->i].extra -= 5;

            pPlayer->hurt_delay = 16;
            P_PalFrom(pPlayer, 32, 32,0,0);

            pPlayer->vel.x = -(sintable[(fix16_to_int(pPlayer->q16ang)+512)&2047]<<8);
            pPlayer->vel.y = -(sintable[(fix16_to_int(pPlayer->q16ang))&2047]<<8);

#ifndef EDUKE32_STANDALONE
            if (!FURY)
                A_PlaySound(DUKE_LONGTERM_PAIN,pPlayer->i);
#endif
            DoWallTouchDamage(pPlayer, touchWall);
            break;

        case BIGFORCE__:
            pPlayer->hurt_delay = GAMETICSPERSEC;
            DoWallTouchDamage(pPlayer, touchWall);
            break;
        }
    }
}

static int P_CheckFloorDamage(DukePlayer_t *pPlayer, int floorTexture)
{
    auto const pSprite = &sprite[pPlayer->i];

    if ((unsigned)(floorTexture = VM_OnEventWithReturn(EVENT_CHECKFLOORDAMAGE, pPlayer->i, P_Get(pPlayer->i), floorTexture)) >= MAXTILES)
        return 0;

    switch (tileGetMapping(floorTexture))
    {
        case HURTRAIL__:
            if (rnd(32))
            {
                if (pPlayer->inv_amount[GET_BOOTS] > 0)
                    return 1;
                else
                {
#ifndef EDUKE32_STANDALONE
                    if (!FURY)
                    {
                        if (!A_CheckSoundPlaying(pPlayer->i, DUKE_LONGTERM_PAIN))
                            A_PlaySound(DUKE_LONGTERM_PAIN, pPlayer->i);

                        if (!A_CheckSoundPlaying(pPlayer->i, SHORT_CIRCUIT))
                            A_PlaySound(SHORT_CIRCUIT, pPlayer->i);
                    }
#endif

                    P_PalFrom(pPlayer, 32, 64, 64, 64);
                    pSprite->extra -= 1 + (krand() & 3);

                    return 0;
                }
            }
            break;

        case FLOORSLIME__:
            if (rnd(16))
            {
                if (pPlayer->inv_amount[GET_BOOTS] > 0)
                    return 1;
                else
                {
#ifndef EDUKE32_STANDALONE
                    if (!FURY && !A_CheckSoundPlaying(pPlayer->i, DUKE_LONGTERM_PAIN))
                        A_PlaySound(DUKE_LONGTERM_PAIN, pPlayer->i);
#endif

                    P_PalFrom(pPlayer, 32, 0, 8, 0);
                    pSprite->extra -= 1 + (krand() & 3);

                    return 0;
                }
            }
            break;

#ifndef EDUKE32_STANDALONE
        case FLOORPLASMA__:
            if (!FURY && rnd(32))
            {
                if (pPlayer->inv_amount[GET_BOOTS] > 0)
                    return 1;
                else
                {
                    if (!A_CheckSoundPlaying(pPlayer->i, DUKE_LONGTERM_PAIN))
                        A_PlaySound(DUKE_LONGTERM_PAIN, pPlayer->i);

                    P_PalFrom(pPlayer, 32, 8, 0, 0);
                    pSprite->extra -= 1 + (krand() & 3);

                    return 0;
                }
            }
            break;
#endif
    }

    return 0;
}


int P_FindOtherPlayer(int playerNum, int32_t *pDist)
{
    int closestPlayer     = playerNum;
    int closestPlayerDist = INT32_MAX;

    for (bssize_t TRAVERSE_CONNECT(otherPlayer))
    {
        if (playerNum != otherPlayer && sprite[g_player[otherPlayer].ps->i].extra > 0)
        {
            int otherPlayerDist = klabs(g_player[otherPlayer].ps->opos.x - g_player[playerNum].ps->pos.x) +
                                  klabs(g_player[otherPlayer].ps->opos.y - g_player[playerNum].ps->pos.y) +
                                  (klabs(g_player[otherPlayer].ps->opos.z - g_player[playerNum].ps->pos.z) >> 4);

            if (otherPlayerDist < closestPlayerDist)
            {
                closestPlayer     = otherPlayer;
                closestPlayerDist = otherPlayerDist;
            }
        }
    }

    *pDist = closestPlayerDist;

    return closestPlayer;
}

void P_FragPlayer(int playerNum)
{
    auto const pPlayer = g_player[playerNum].ps;
    auto const pSprite = &sprite[pPlayer->i];

    if (g_netClient) // [75] The server should not overwrite its own randomseed
        randomseed = ticrandomseed;

    if (pSprite->pal != 1)
    {
        P_PalFrom(pPlayer, 63, 63, 0, 0);
        I_AddForceFeedback(pPlayer->max_player_health << FF_PLAYER_DMG_SCALE, pPlayer->max_player_health << FF_PLAYER_DMG_SCALE, pPlayer->max_player_health << FF_PLAYER_TIME_SCALE);

        pPlayer->pos.z -= ZOFFSET2;
        pSprite->z -= ZOFFSET2;

        pPlayer->dead_flag = (512 - ((krand() & 1) << 10) + (krand() & 255) - 512) & 2047;

        if (pPlayer->dead_flag == 0)
            pPlayer->dead_flag++;

#ifndef NETCODE_DISABLE
        if (g_netServer)
        {
            // this packet might not be needed anymore with the new snapshot code
            packbuf[0] = PACKET_FRAG;
            packbuf[1] = playerNum;
            packbuf[2] = pPlayer->frag_ps;
            packbuf[3] = actor[pPlayer->i].htpicnum;
            B_BUF32(&packbuf[4], ticrandomseed);
            packbuf[8] = myconnectindex;

            enet_host_broadcast(g_netServer, CHAN_GAMESTATE, enet_packet_create(&packbuf[0], 9, ENET_PACKET_FLAG_RELIABLE));
        }
#endif
    }

#ifndef EDUKE32_STANDALONE
    if (!FURY)
    {
        pPlayer->jetpack_on  = 0;
        pPlayer->holoduke_on = -1;

        S_StopEnvSound(DUKE_JETPACK_IDLE, pPlayer->i);

        if (pPlayer->scream_voice > FX_Ok)
        {
            FX_StopSound(pPlayer->scream_voice);
            S_Cleanup();
            pPlayer->scream_voice = -1;
        }
    }
#endif

    if (pSprite->pal != 1 && (pSprite->cstat & 32768) == 0)
        pSprite->cstat = 0;

    if ((g_netServer || ud.multimode > 1) && (pSprite->pal != 1 || (pSprite->cstat & 32768)))
    {
        if (pPlayer->frag_ps != playerNum)
        {
            if (GTFLAGS(GAMETYPE_TDM) && g_player[pPlayer->frag_ps].ps->team == g_player[playerNum].ps->team)
                g_player[pPlayer->frag_ps].ps->fraggedself++;
            else
            {
                g_player[pPlayer->frag_ps].ps->frag++;
                g_player[pPlayer->frag_ps].frags[playerNum]++;
                g_player[playerNum].frags[playerNum]++;  // deaths
            }

            if (playerNum == screenpeek)
            {
                Bsprintf(apStrings[QUOTE_RESERVED], "Killed by %s", &g_player[pPlayer->frag_ps].user_name[0]);
                P_DoQuote(QUOTE_RESERVED, pPlayer);
            }
            else
            {
                Bsprintf(apStrings[QUOTE_RESERVED2], "Killed %s", &g_player[playerNum].user_name[0]);
                P_DoQuote(QUOTE_RESERVED2, g_player[pPlayer->frag_ps].ps);
            }

            if (ud.obituaries)
            {
                Bsprintf(tempbuf, apStrings[OBITQUOTEINDEX + (krand() % g_numObituaries)],
                         &g_player[pPlayer->frag_ps].user_name[0], &g_player[playerNum].user_name[0]);
                G_AddUserQuote(tempbuf);
            }
            else
                krand();
        }
        else
        {
            if (actor[pPlayer->i].htpicnum != APLAYERTOP)
            {
                pPlayer->fraggedself++;
                if ((unsigned)pPlayer->wackedbyactor < MAXSPRITES && A_CheckEnemyTile(sprite[pPlayer->wackedbyactor].picnum))
                    Bsprintf(tempbuf, apStrings[OBITQUOTEINDEX + (krand() % g_numObituaries)], "A monster",
                             &g_player[playerNum].user_name[0]);
                else if (actor[pPlayer->i].htpicnum == NUKEBUTTON)
                    Bsprintf(tempbuf, "^02%s^02 tried to leave", &g_player[playerNum].user_name[0]);
                else
                {
                    // random suicide death string
                    Bsprintf(tempbuf, apStrings[SUICIDEQUOTEINDEX + (krand() % g_numSelfObituaries)],
                             &g_player[playerNum].user_name[0]);
                }
            }
            else
                Bsprintf(tempbuf, "^02%s^02 switched to team %d", &g_player[playerNum].user_name[0], pPlayer->team + 1);

            if (ud.obituaries)
                G_AddUserQuote(tempbuf);
        }
        pPlayer->frag_ps = playerNum;
        pus              = NUMPAGES;
    }
}

#define PIPEBOMB_CONTROL(playerNum) (Gv_GetVarByLabel("PIPEBOMB_CONTROL", PIPEBOMB_REMOTE, -1, playerNum))

static void P_ProcessWeapon(int playerNum)
{
    auto const     pPlayer      = g_player[playerNum].ps;
    uint8_t *const weaponFrame  = &pPlayer->kickback_pic;
    int const      playerShrunk = (sprite[pPlayer->i].yrepeat < 32);
    uint32_t       playerBits   = g_player[playerNum].input.bits;

    switch (pPlayer->weapon_pos)
    {
        case WEAPON_POS_LOWER:
            if (pPlayer->last_weapon >= 0)
            {
                pPlayer->weapon_pos  = WEAPON_POS_RAISE;
                pPlayer->last_weapon = -1;
            }
            else if (pPlayer->holster_weapon == 0)
                pPlayer->weapon_pos = WEAPON_POS_RAISE;
            break;
        case 0: break;
        default: pPlayer->weapon_pos--; break;
    }

    if (TEST_SYNC_KEY(playerBits, SK_FIRE))
    {
        P_SetWeaponGamevars(playerNum, pPlayer);

        if (VM_OnEvent(EVENT_PRESSEDFIRE, pPlayer->i, playerNum) != 0)
            playerBits &= ~BIT(SK_FIRE);
    }

    if (TEST_SYNC_KEY(playerBits, SK_HOLSTER))   // 'Holster Weapon
    {
        P_SetWeaponGamevars(playerNum, pPlayer);

        if (VM_OnEvent(EVENT_HOLSTER, pPlayer->i, playerNum) == 0)
        {
            if (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) != KNEE_WEAPON)
            {
                if (pPlayer->holster_weapon == 0 && pPlayer->weapon_pos == 0)
                {
                    pPlayer->holster_weapon = 1;
                    pPlayer->weapon_pos     = -1;
                    P_DoQuote(QUOTE_WEAPON_LOWERED, pPlayer);
                }
                else if (pPlayer->holster_weapon == 1 && pPlayer->weapon_pos == WEAPON_POS_LOWER)
                {
                    pPlayer->holster_weapon = 0;
                    pPlayer->weapon_pos = WEAPON_POS_RAISE;
                    P_DoQuote(QUOTE_WEAPON_RAISED,pPlayer);
                }
            }

            if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_HOLSTER_CLEARS_CLIP)
            {
                int const weap = pPlayer->curr_weapon, clipcnt = PWEAPON(playerNum, weap, Clip);

                if (pPlayer->ammo_amount[weap] > clipcnt && (pPlayer->ammo_amount[weap] % clipcnt) != 0)
                {
                    pPlayer->ammo_amount[weap] -= pPlayer->ammo_amount[weap] % clipcnt;
                    *weaponFrame                = PWEAPON(playerNum, weap, TotalTime);
                    playerBits                 &= ~BIT(SK_FIRE);  // not firing...
                }

                return;
            }
        }
    }

    int const maybeGlowingWeapon = pPlayer->last_weapon != -1 ? pPlayer->last_weapon : pPlayer->curr_weapon;

    if (PWEAPON(playerNum, maybeGlowingWeapon, Flags) & WEAPON_GLOWS)
    {
        pPlayer->random_club_frame += 64; // Glowing

#ifdef POLYMER
        if (pPlayer->kickback_pic == 0)
        {
            auto   const pSprite = &sprite[pPlayer->i];
            vec3_t const offset = { -((sintable[(pSprite->ang+512)&2047])>>7), -((sintable[(pSprite->ang)&2047])>>7), pPlayer->spritezoffset };

            int const glowRange = (16-klabs(pPlayer->weapon_pos)+(sintable[pPlayer->random_club_frame & 2047]>>10))<<6;

            G_AddGameLight(pPlayer->i, pPlayer->cursectnum, offset, max(glowRange, 0), 0, 100,
                           PWEAPON(playerNum, maybeGlowingWeapon, FlashColor), PR_LIGHT_PRIO_HIGH_GAME);

            practor[pPlayer->i].lightcount = 2;
        }
#endif
    }

    // this is a hack for WEAPON_FIREEVERYOTHER
    if (actor[pPlayer->i].t_data[7])
    {
        actor[pPlayer->i].t_data[7]--;
        if (pPlayer->last_weapon == -1 && actor[pPlayer->i].t_data[7] != 0 && ((actor[pPlayer->i].t_data[7] & 1) == 0))
        {
            if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_AMMOPERSHOT)
            {
                if (pPlayer->ammo_amount[pPlayer->curr_weapon] > 0)
                    pPlayer->ammo_amount[pPlayer->curr_weapon]--;
                else
                {
                    actor[pPlayer->i].t_data[7] = 0;
                    P_CheckWeapon(pPlayer);
                }
            }

            if (actor[pPlayer->i].t_data[7] != 0)
            {
                int const currentShot = ud.returnvar[0] = PWEAPON(playerNum, pPlayer->curr_weapon, ShotsPerBurst) - (actor[pPlayer->i].t_data[7] >> 1);
                if (VM_OnEventWithReturn(EVENT_PREWEAPONSHOOT, pPlayer->i, playerNum, 0) == 0)
                {
                    auto const retVal = A_Shoot(pPlayer->i, PWEAPON(playerNum, pPlayer->curr_weapon, Shoots));
                    ud.returnvar[0] = currentShot;
                    VM_OnEventWithReturn(EVENT_POSTWEAPONSHOOT, pPlayer->i, playerNum, retVal);
                }
            }
        }
    }

    if (pPlayer->rapid_fire_hold == 1)
    {
        if (TEST_SYNC_KEY(playerBits, SK_FIRE))
            return;
        pPlayer->rapid_fire_hold = 0;
    }

    bool const doFire    = (playerBits & BIT(SK_FIRE) && (*weaponFrame) == 0);
    bool const doAltFire = g_player[playerNum].input.extbits & BIT(EK_ALT_FIRE);
    bool startFiring     = false;

    if (doAltFire)
    {
        P_SetWeaponGamevars(playerNum, pPlayer);
        VM_OnEvent(EVENT_ALTFIRE, pPlayer->i, playerNum);
    }

    if (playerShrunk || pPlayer->tipincs || pPlayer->access_incs)
        playerBits &= ~BIT(SK_FIRE);
    else if (doFire && pPlayer->fist_incs == 0 &&
             pPlayer->last_weapon == -1 && (pPlayer->weapon_pos == 0 || pPlayer->holster_weapon == 1))
    {
        pPlayer->crack_time = PCRACKTIME;

        if (pPlayer->holster_weapon == 1)
        {
            if (pPlayer->last_pissed_time <= (GAMETICSPERSEC * 218) && pPlayer->weapon_pos == WEAPON_POS_LOWER)
            {
                pPlayer->holster_weapon = 0;
                pPlayer->weapon_pos     = WEAPON_POS_RAISE;
                P_DoQuote(QUOTE_WEAPON_RAISED, pPlayer);
            }
        }
        else
        {
            P_SetWeaponGamevars(playerNum, pPlayer);

            if (doFire && VM_OnEvent(EVENT_FIRE, pPlayer->i, playerNum) == 0)
            {
                // this event is deprecated
                VM_OnEvent(EVENT_FIREWEAPON, pPlayer->i, playerNum);

                switch (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike))
                {
                    case HANDBOMB_WEAPON:
                        pPlayer->hbomb_hold_delay = 0;
                        if (pPlayer->ammo_amount[pPlayer->curr_weapon] > 0)
                        {
                            startFiring = true;
                            if (PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound) > 0)
                                A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound), pPlayer->i);
                        }
                        break;

                    case HANDREMOTE_WEAPON:
                        pPlayer->hbomb_hold_delay = 0;
                        startFiring               = true;
                        if (PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound) > 0)
                            A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound), pPlayer->i);
                        break;


                    case TRIPBOMB_WEAPON:
                        if (pPlayer->ammo_amount[pPlayer->curr_weapon] > 0)
                        {
                            hitdata_t hitData;
                            int const pq16ang = fix16_to_int(pPlayer->q16ang);
                            hitscan((const vec3_t *)pPlayer, pPlayer->cursectnum, sintable[(pq16ang + 512) & 2047],
                                    sintable[pq16ang & 2047], fix16_to_int(F16(100) - pPlayer->q16horiz - pPlayer->q16horizoff) * 32, &hitData,
                                    CLIPMASK1);

                            if ((hitData.sect < 0 || hitData.sprite >= 0) ||
                                (hitData.wall >= 0 && sector[hitData.sect].lotag > 2))
                                break;

                            if (hitData.wall >= 0 && wall[hitData.wall].overpicnum >= 0)
                                if (wall[hitData.wall].overpicnum == BIGFORCE)
                                    break;

                            uint32_t xdiff_sq, ydiff_sq;
                            int spriteNum = headspritesect[hitData.sect];
                            while (spriteNum >= 0)
                            {
                                xdiff_sq = (sprite[spriteNum].x - hitData.x) * (sprite[spriteNum].x - hitData.x);
                                ydiff_sq = (sprite[spriteNum].y - hitData.y) * (sprite[spriteNum].y - hitData.y);

                                if (sprite[spriteNum].picnum == TRIPBOMB && klabs(sprite[spriteNum].z - hitData.z) < ZOFFSET4
                                        && xdiff_sq + ydiff_sq < (290 * 290))
                                    break;
                                spriteNum = nextspritesect[spriteNum];
                            }

                            // ST_2_UNDERWATER
                            if (spriteNum == -1 && hitData.wall >= 0 && (wall[hitData.wall].cstat & 16) == 0)
                                if ((wall[hitData.wall].nextsector >= 0 && sector[wall[hitData.wall].nextsector].lotag <= 2) ||
                                    (wall[hitData.wall].nextsector == -1 && sector[hitData.sect].lotag <= 2))
                                {
                                    xdiff_sq = (hitData.x - pPlayer->pos.x) * (hitData.x - pPlayer->pos.x);
                                    ydiff_sq = (hitData.y - pPlayer->pos.y) * (hitData.y - pPlayer->pos.y);

                                    if (xdiff_sq + ydiff_sq < (290 * 290))
                                    {
                                        pPlayer->pos.z = pPlayer->opos.z;
                                        pPlayer->vel.z = 0;
                                        startFiring    = true;
                                        if (PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound) > 0)
                                        {
                                            A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound), pPlayer->i);
                                        }
                                    }
                                }
                        }
                        break;

                    case PISTOL_WEAPON:
                    case SHOTGUN_WEAPON:
                    case CHAINGUN_WEAPON:
                    case SHRINKER_WEAPON:
                    case GROW_WEAPON:
                    case FREEZE_WEAPON:
                    case RPG_WEAPON:
                        if (pPlayer->ammo_amount[pPlayer->curr_weapon] > 0)
                        {
                            startFiring = true;
                            if (PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound) > 0)
                                A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound), pPlayer->i);
                        }
                        break;

                    case FLAMETHROWER_WEAPON:
                        if (pPlayer->ammo_amount[pPlayer->curr_weapon] > 0)
                        {
                            startFiring = true;
                            if (PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound) > 0 && sector[pPlayer->cursectnum].lotag != ST_2_UNDERWATER)
                                A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound), pPlayer->i);
                        }
                        break;

                    case DEVISTATOR_WEAPON:
                        if (pPlayer->ammo_amount[pPlayer->curr_weapon] > 0)
                        {
                            startFiring               = true;
                            pPlayer->hbomb_hold_delay = !pPlayer->hbomb_hold_delay;
                            if (PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound) > 0)
                                A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound), pPlayer->i);
                        }
                        break;

                    case KNEE_WEAPON:
                        if (dukeAllowQuickKick() || pPlayer->quick_kick == 0)
                        {
                            startFiring = true;
                            if (PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound) > 0)
                                A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, InitialSound), pPlayer->i);
                        }
                        break;
                }
            }
        }
    }

    if (startFiring || *weaponFrame)
    {
        if (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == HANDBOMB_WEAPON)
        {
            if (PWEAPON(playerNum, pPlayer->curr_weapon, HoldDelay) && ((*weaponFrame) == PWEAPON(playerNum, pPlayer->curr_weapon, FireDelay)) && TEST_SYNC_KEY(playerBits, SK_FIRE))
            {
                pPlayer->rapid_fire_hold = 1;
                return;
            }

            if (++(*weaponFrame) == PWEAPON(playerNum, pPlayer->curr_weapon, HoldDelay))
            {
                pPlayer->ammo_amount[pPlayer->curr_weapon]--;

                if (numplayers < 2 || g_netServer)
                {
                    int pipeBombType;
                    int pipeBombZvel;
                    int pipeBombFwdVel;

                    if (pPlayer->on_ground && TEST_SYNC_KEY(playerBits, SK_CROUCH))
                    {
                        pipeBombFwdVel = 15;
                        pipeBombZvel   = (fix16_to_int(pPlayer->q16horiz + pPlayer->q16horizoff - F16(100)) * 20);
                    }
                    else
                    {
                        pipeBombFwdVel = 140;
                        pipeBombZvel   = -512 - (fix16_to_int(pPlayer->q16horiz + pPlayer->q16horizoff - F16(100)) * 20);
                    }
                    int const pq16ang = fix16_to_int(pPlayer->q16ang);
                    int pipeSpriteNum = A_InsertSprite(pPlayer->cursectnum,
                                       pPlayer->pos.x+(sintable[(pq16ang +512)&2047]>>6),
                                       pPlayer->pos.y+(sintable[pq16ang &2047]>>6),
                                       pPlayer->pos.z,PWEAPON(playerNum, pPlayer->curr_weapon, Shoots),-16,9,9,
                                       pq16ang,(pipeBombFwdVel+(pPlayer->hbomb_hold_delay<<5)),pipeBombZvel,pPlayer->i,1);

                    pipeBombType = PIPEBOMB_CONTROL(playerNum);

                    if (pipeBombType & PIPEBOMB_TIMER)
                    {
                        int pipeLifeTime     = Gv_GetVarByLabel("GRENADE_LIFETIME", NAM_GRENADE_LIFETIME, -1, playerNum);
                        int pipeLifeVariance = Gv_GetVarByLabel("GRENADE_LIFETIME_VAR", NAM_GRENADE_LIFETIME_VAR, -1, playerNum);
                        actor[pipeSpriteNum].t_data[7]= pipeLifeTime
                                            + mulscale14(krand(), pipeLifeVariance)
                                            - pipeLifeVariance;
                        // TIMER_CONTROL
                        actor[pipeSpriteNum].t_data[6]=1;
                    }
                    else actor[pipeSpriteNum].t_data[6]=2;

                    if (pipeBombFwdVel == 15)
                    {
                        sprite[pipeSpriteNum].yvel = 3;
                        sprite[pipeSpriteNum].z += ZOFFSET3;
                    }

                    if (A_GetHitscanRange(pPlayer->i) < 512)
                    {
                        sprite[pipeSpriteNum].ang += 1024;
                        sprite[pipeSpriteNum].zvel /= 3;
                        sprite[pipeSpriteNum].xvel /= 3;
                    }
                }

                pPlayer->hbomb_on = 1;
            }
            else if ((*weaponFrame) < PWEAPON(playerNum, pPlayer->curr_weapon, HoldDelay) && TEST_SYNC_KEY(playerBits, SK_FIRE))
                pPlayer->hbomb_hold_delay++;
            else if ((*weaponFrame) > PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime))
            {
                (*weaponFrame) = 0;
                if (PIPEBOMB_CONTROL(playerNum) == PIPEBOMB_REMOTE)
                {
                    pPlayer->weapon_pos = WEAPON_POS_RAISE;
                    pPlayer->curr_weapon = HANDREMOTE_WEAPON;
                    pPlayer->last_weapon = -1;
                }
                else
                {
                    if (!NAM_WW2GI)
                        pPlayer->weapon_pos = WEAPON_POS_RAISE;
                    P_CheckWeapon(pPlayer);
                }
            }
        }
        else if (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == HANDREMOTE_WEAPON)
        {
            if (++(*weaponFrame) == PWEAPON(playerNum, pPlayer->curr_weapon, FireDelay))
            {
                if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_BOMB_TRIGGER)
                    pPlayer->hbomb_on = 0;

                if (PWEAPON(playerNum, pPlayer->curr_weapon, Shoots) != 0)
                {
                    if (!(PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_NOVISIBLE))
                        pPlayer->visibility = 0;

                    P_SetWeaponGamevars(playerNum, pPlayer);
                    A_Shoot(pPlayer->i, PWEAPON(playerNum, pPlayer->curr_weapon, Shoots));
                }
            }

            if ((*weaponFrame) >= PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime))
            {
                (*weaponFrame) = 0;
                if ((pPlayer->ammo_amount[HANDBOMB_WEAPON] > 0) && PIPEBOMB_CONTROL(playerNum) == PIPEBOMB_REMOTE)
                    P_AddWeapon(pPlayer, HANDBOMB_WEAPON, 1);
                else P_CheckWeapon(pPlayer);
            }
        }
        else
        {
            // the basic weapon...
            (*weaponFrame)++;

            if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_CHECKATRELOAD)
            {
                if (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == TRIPBOMB_WEAPON)
                {
                    if ((*weaponFrame) >= PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime))
                    {
                        (*weaponFrame) = 0;
                        P_CheckWeapon(pPlayer);
                        pPlayer->weapon_pos = WEAPON_POS_LOWER;
                    }
                }
                else if (*weaponFrame >= PWEAPON(playerNum, pPlayer->curr_weapon, Reload))
                    P_CheckWeapon(pPlayer);
            }
            else if (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike)!=KNEE_WEAPON && *weaponFrame >= PWEAPON(playerNum, pPlayer->curr_weapon, FireDelay))
                P_CheckWeapon(pPlayer);

            if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_STANDSTILL
                    && *weaponFrame < (PWEAPON(playerNum, pPlayer->curr_weapon, FireDelay)+1))
            {
                pPlayer->pos.z = pPlayer->opos.z;
                pPlayer->vel.z = 0;
            }

            if (*weaponFrame == PWEAPON(playerNum, pPlayer->curr_weapon, Sound2Time))
                if (PWEAPON(playerNum, pPlayer->curr_weapon, Sound2Sound) > 0)
                    A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, Sound2Sound),pPlayer->i);

            if ((*weaponFrame == PWEAPON(playerNum, pPlayer->curr_weapon, SpawnTime))
                && !(startFiring && (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & (WEAPON_FIREEVERYTHIRD | WEAPON_FIREEVERYOTHER))))
                P_DoWeaponSpawn(playerNum);

            if ((*weaponFrame) >= PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime))
            {
                if (/*!(PWEAPON(snum, p->curr_weapon, Flags) & WEAPON_CHECKATRELOAD) && */ pPlayer->reloading == 1 ||
                        (PWEAPON(playerNum, pPlayer->curr_weapon, Reload) > PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime) && pPlayer->ammo_amount[pPlayer->curr_weapon] > 0
                         && (PWEAPON(playerNum, pPlayer->curr_weapon, Clip)) && (((pPlayer->ammo_amount[pPlayer->curr_weapon]%(PWEAPON(playerNum, pPlayer->curr_weapon, Clip)))==0))))
                {
                    int const weaponReloadTime = PWEAPON(playerNum, pPlayer->curr_weapon, Reload)
                                               - PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime);

                    pPlayer->reloading = 1;

                    if ((*weaponFrame) != (PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime)))
                    {
                        if ((*weaponFrame) == (PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime)+1))
                        {
                            if (PWEAPON(playerNum, pPlayer->curr_weapon, ReloadSound1) > 0)
                                A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, ReloadSound1), pPlayer->i);
                        }
                        else if (((*weaponFrame) ==
                                  (PWEAPON(playerNum, pPlayer->curr_weapon, Reload) - (weaponReloadTime / 3)) &&
                                  !(PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_RELOAD_TIMING)) ||
                                 ((*weaponFrame) ==
                                  (PWEAPON(playerNum, pPlayer->curr_weapon, Reload) - weaponReloadTime + 4) &&
                                  (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_RELOAD_TIMING)))
                        {
                            if (PWEAPON(playerNum, pPlayer->curr_weapon, ReloadSound2) > 0)
                                A_PlaySound(PWEAPON(playerNum, pPlayer->curr_weapon, ReloadSound2), pPlayer->i);
                        }
                        else if ((*weaponFrame) >= (PWEAPON(playerNum, pPlayer->curr_weapon, Reload)))
                        {
                            *weaponFrame       = 0;
                            pPlayer->reloading = 0;
                        }
                    }
                }
                else
                {
                    if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_AUTOMATIC &&
                            (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike)==KNEE_WEAPON || pPlayer->ammo_amount[pPlayer->curr_weapon] > 0))
                    {
                        if (TEST_SYNC_KEY(playerBits, SK_FIRE))
                        {
                            *weaponFrame =
                            (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_RANDOMRESTART) ? 1 + (krand() & 3) : 1;
                        }
                        else *weaponFrame = 0;
                    }
                    else *weaponFrame = 0;

                    if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_RESET &&
                        ((PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == KNEE_WEAPON)
                         || pPlayer->ammo_amount[pPlayer->curr_weapon] > 0))
                    {
                        *weaponFrame = !!(TEST_SYNC_KEY(playerBits, SK_FIRE));
                    }
                }
            }

            if (*weaponFrame >= PWEAPON(playerNum, pPlayer->curr_weapon, FireDelay)
                     && ((*weaponFrame) < PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime))
                     && ((PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == KNEE_WEAPON) || pPlayer->ammo_amount[pPlayer->curr_weapon] > 0))
            {
                if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_AUTOMATIC)
                {
                    if (!(PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_SEMIAUTO))
                    {
                        if (TEST_SYNC_KEY(playerBits, SK_FIRE) == 0 && WW2GI)
                            *weaponFrame = PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime);

                        if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_FIREEVERYTHIRD)
                        {
                            if (((*(weaponFrame))%3) == 0)
                            {
                                P_FireWeapon(playerNum);
                                P_DoWeaponSpawn(playerNum);
                            }
                        }
                        else if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_FIREEVERYOTHER)
                        {
                            P_FireWeapon(playerNum);
                            P_DoWeaponSpawn(playerNum);
                        }
                        else
                        {
                            if (*weaponFrame == PWEAPON(playerNum, pPlayer->curr_weapon, FireDelay))
                            {
                                P_FireWeapon(playerNum);
//                                P_DoWeaponSpawn(snum);
                            }
                        }

                        if (TEST_SYNC_KEY(playerBits, SK_FIRE) == 0 && !WW2GI && (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_RESET))
                            *weaponFrame = PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime);

                        if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_RESET
                            && (*weaponFrame) > PWEAPON(playerNum, pPlayer->curr_weapon, TotalTime)
                                                - PWEAPON(playerNum, pPlayer->curr_weapon, HoldDelay)
                            && ((PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == KNEE_WEAPON)
                                || pPlayer->ammo_amount[pPlayer->curr_weapon] > 0))
                        {
                            *weaponFrame = !!(TEST_SYNC_KEY(playerBits, SK_FIRE));
                        }
                    }
                    else
                    {
                        if (PWEAPON(playerNum, pPlayer->curr_weapon, Flags) & WEAPON_FIREEVERYOTHER)
                        {
                            P_FireWeapon(playerNum);
                            P_DoWeaponSpawn(playerNum);
                        }
                        else
                        {
                            if (*weaponFrame == PWEAPON(playerNum, pPlayer->curr_weapon, FireDelay))
                                P_FireWeapon(playerNum);
                        }
                    }
                }
                else if (*weaponFrame == PWEAPON(playerNum, pPlayer->curr_weapon, FireDelay))
                    P_FireWeapon(playerNum);
            }
        }
    }
}

void P_EndLevel(void)
{
    for (bssize_t TRAVERSE_CONNECT(playerNum))
        g_player[playerNum].ps->gm = MODE_EOL;

    if (ud.from_bonus)
    {
        ud.level_number   = ud.from_bonus;
        ud.m_level_number = ud.level_number;
        ud.from_bonus     = 0;
    }
    else
    {
        ud.level_number   = (++ud.level_number < MAXLEVELS) ? ud.level_number : 0;
        ud.m_level_number = ud.level_number;
    }
}

static int P_DoFist(DukePlayer_t *pPlayer)
{
    // the fist punching NUKEBUTTON

#ifndef EDUKE32_STANDALONE
    if (FURY)
        return 0;

    if (++(pPlayer->fist_incs) == 28)
    {
        if (ud.recstat == 1)
            G_CloseDemoWrite();

        S_PlaySound(PIPEBOMB_EXPLODE);
        P_PalFrom(pPlayer, 48, 64, 64, 64);
    }

    if (pPlayer->fist_incs > 42)
    {
        if (pPlayer->buttonpalette && ud.from_bonus == 0)
        {
            for (bssize_t TRAVERSE_CONNECT(playerNum))
                g_player[playerNum].ps->gm = MODE_EOL;

            ud.from_bonus = ud.level_number + 1;

            if ((unsigned)ud.secretlevel <= MAXLEVELS)
                ud.level_number = ud.secretlevel - 1;

            ud.m_level_number = ud.level_number;
        }
        else
            P_EndLevel();

        pPlayer->fist_incs = 0;

        return 1;
    }
#else
    UNREFERENCED_PARAMETER(pPlayer);
#endif

    return 0;
}

void P_UpdatePosWhenViewingCam(DukePlayer_t *pPlayer)
{
    int const newOwner      = pPlayer->newowner;
    pPlayer->pos            = sprite[newOwner].xyz;
    pPlayer->q16ang         = fix16_from_int(SA(newOwner));
    pPlayer->vel.x          = 0;
    pPlayer->vel.y          = 0;
    sprite[pPlayer->i].xvel = 0;
    pPlayer->look_ang       = 0;
    pPlayer->rotscrnang     = 0;
}

static void P_DoWater(int const playerNum, int const playerBits, int const floorZ, int const ceilZ)
{
    auto const pPlayer = g_player[playerNum].ps;

    // under water
    pPlayer->pycount        += 32;
    pPlayer->pycount        &= 2047;
    pPlayer->jumping_counter = 0;
    pPlayer->pyoff           = sintable[pPlayer->pycount] >> 7;

#ifndef EDUKE32_STANDALONE
    if (!FURY && !A_CheckSoundPlaying(pPlayer->i, DUKE_UNDERWATER))
        A_PlaySound(DUKE_UNDERWATER, pPlayer->i);
#endif
    if (TEST_SYNC_KEY(playerBits, SK_JUMP))
    {
        if (VM_OnEvent(EVENT_SWIMUP, pPlayer->i, playerNum) == 0)
            pPlayer->vel.z = max(min(-pPlayer->swimzincrement, pPlayer->vel.z - pPlayer->swimzincrement), -pPlayer->maxswimzvel);
    }
    else if (TEST_SYNC_KEY(playerBits, SK_CROUCH))
    {
        if (VM_OnEvent(EVENT_SWIMDOWN, pPlayer->i, playerNum) == 0)
            pPlayer->vel.z = min<int>(max<int>(pPlayer->swimzincrement, pPlayer->vel.z + pPlayer->swimzincrement), pPlayer->maxswimzvel);
    }
    else
    {
        // normal view
        if (pPlayer->vel.z < 0)
            pPlayer->vel.z = min(0, pPlayer->vel.z + pPlayer->minswimzvel);

        if (pPlayer->vel.z > 0)
            pPlayer->vel.z = max(0, pPlayer->vel.z - pPlayer->minswimzvel);
    }

    if (pPlayer->vel.z > 2048)
        pPlayer->vel.z >>= 1;

    pPlayer->pos.z += pPlayer->vel.z;

    if (pPlayer->pos.z > (floorZ-(15<<8)))
        pPlayer->pos.z += ((floorZ-(15<<8))-pPlayer->pos.z)>>1;

    if (pPlayer->pos.z < ceilZ)
    {
        pPlayer->pos.z = ceilZ;
        pPlayer->vel.z = 0;
    }

    if ((pPlayer->on_warping_sector == 0 || ceilZ != pPlayer->truecz) && pPlayer->pos.z < ceilZ + PMINHEIGHT)
    {
        pPlayer->pos.z = ceilZ + PMINHEIGHT;
        pPlayer->vel.z = 0;
    }

#ifndef EDUKE32_STANDALONE
    if (!FURY && pPlayer->scuba_on && (krand()&255) < 8)
    {
        int const spriteNum = A_Spawn(pPlayer->i, WATERBUBBLE);
        int const q16ang      = fix16_to_int(pPlayer->q16ang);

        sprite[spriteNum].x      += sintable[(q16ang + 512 + 64 - (g_globalRandom & 128)) & 2047] >> 6;
        sprite[spriteNum].y      += sintable[(q16ang + 64 - (g_globalRandom & 128)) & 2047] >> 6;
        sprite[spriteNum].xrepeat = 3;
        sprite[spriteNum].yrepeat = 2;
        sprite[spriteNum].z       = pPlayer->pos.z + ZOFFSET3;
    }
#endif
}
static void P_DoJetpack(int const playerNum, int const playerBits, int const playerShrunk, int const sectorLotag, int const floorZ)
{
    auto const pPlayer = g_player[playerNum].ps;

    pPlayer->on_ground       = 0;
    pPlayer->jumping_counter = 0;
    pPlayer->hard_landing    = 0;
    pPlayer->falling_counter = 0;
    pPlayer->pycount        += 32;
    pPlayer->pycount        &= 2047;
    pPlayer->pyoff           = sintable[pPlayer->pycount] >> 7;

    g_player[playerNum].horizSkew = 0;

    if (pPlayer->jetpack_on < 11)
    {
        pPlayer->jetpack_on++;
        pPlayer->pos.z -= (pPlayer->jetpack_on<<7); //Goin up
    }
    else if (pPlayer->jetpack_on == 11 && !A_CheckSoundPlaying(pPlayer->i, DUKE_JETPACK_IDLE))
        A_PlaySound(DUKE_JETPACK_IDLE, pPlayer->i);

    int const zAdjust = pPlayer->jetpackzincrement >> (playerShrunk << 1);

    if (TEST_SYNC_KEY(playerBits, SK_JUMP))  // jumping, flying up
    {
        if (VM_OnEvent(EVENT_SOARUP, pPlayer->i, playerNum) == 0)
        {
            pPlayer->pos.z -= zAdjust;
            pPlayer->crack_time = PCRACKTIME;
        }
    }

    if (TEST_SYNC_KEY(playerBits, SK_CROUCH))  // crouching, flying down
    {
        if (VM_OnEvent(EVENT_SOARDOWN, pPlayer->i, playerNum) == 0)
        {
            pPlayer->pos.z += zAdjust;
            pPlayer->crack_time = PCRACKTIME;
        }
    }

    int const Zdiff = (playerShrunk == 0 && (sectorLotag == 0 || sectorLotag == ST_2_UNDERWATER)) ? 32 : 16;

    if (sectorLotag != ST_2_UNDERWATER && pPlayer->scuba_on == 1)
        pPlayer->scuba_on = 0;

    if (pPlayer->pos.z > (floorZ - (Zdiff << 8)))
        pPlayer->pos.z += ((floorZ - (Zdiff << 8)) - pPlayer->pos.z) >> 1;

    if (pPlayer->pos.z < (actor[pPlayer->i].ceilingz + (18 << 8)))
        pPlayer->pos.z = actor[pPlayer->i].ceilingz + (18 << 8);
}

static void P_Dead(int const playerNum, int const sectorLotag, int const floorZ, int const ceilZ)
{
    auto const pPlayer = g_player[playerNum].ps;
    auto const pSprite = &sprite[pPlayer->i];

    if (ud.recstat == 1 && (!g_netServer && ud.multimode < 2))
        G_CloseDemoWrite();

    if ((numplayers < 2 || g_netServer) && pPlayer->dead_flag == 0)
        P_FragPlayer(playerNum);

    if (sectorLotag == ST_2_UNDERWATER)
    {
        if (pPlayer->on_warping_sector == 0)
        {
            if (klabs(pPlayer->pos.z-floorZ) >(pPlayer->spritezoffset>>1))
                pPlayer->pos.z += 348;
        }
        else
        {
            pSprite->z -= 512;
            pSprite->zvel = -348;
        }

        clipmove(&pPlayer->pos, &pPlayer->cursectnum,
            0, 0, pPlayer->clipdist, (4L<<8), (4L<<8), CLIPMASK0);
        //                        p->bobcounter += 32;
    }

    Bmemcpy(&pPlayer->opos, &pPlayer->pos, sizeof(vec3_t));
    pPlayer->oq16ang = pPlayer->q16ang;
    pPlayer->opyoff = pPlayer->pyoff;

    pPlayer->q16horiz = F16(100);
    pPlayer->q16horizoff = 0;

    updatesector(pPlayer->pos.x, pPlayer->pos.y, &pPlayer->cursectnum);

    pushmove(&pPlayer->pos, &pPlayer->cursectnum, 128L, (4L<<8), (20L<<8), CLIPMASK0);

    if (floorZ > ceilZ + ZOFFSET2 && pSprite->pal != 1)
    {
        pPlayer->orotscrnang = pPlayer->rotscrnang;
        pPlayer->rotscrnang = (pPlayer->dead_flag + ((floorZ + pPlayer->pos.z) >> 7)) & 2047;
    }

    pPlayer->on_warping_sector = 0;
}


// Duke3D needs the player sprite to actually be in the floor when shrunk in order to walk under enemies.
// This sucks.
static void P_ClampZ(DukePlayer_t* const pPlayer, int const sectorLotag, int32_t const ceilZ, int32_t const floorZ)
{
#ifndef EDUKE32_STANDALONE
    auto const pSprite      = &sprite[pPlayer->i];
    int const  playerShrunk = (pSprite->yrepeat < 32);

    if (!FURY && playerShrunk)
        return;
#endif

    if ((sectorLotag != ST_2_UNDERWATER || ceilZ != pPlayer->truecz) && pPlayer->pos.z < ceilZ + PMINHEIGHT)
        pPlayer->pos.z = ceilZ + PMINHEIGHT;

    if (sectorLotag != ST_1_ABOVE_WATER && pPlayer->pos.z > floorZ - PMINHEIGHT)
        pPlayer->pos.z = floorZ - PMINHEIGHT;
}

#define GETZRANGECLIPDISTOFFSET 4

void P_ProcessInput(int playerNum)
{
    auto &thisPlayer = g_player[playerNum];

    if (thisPlayer.playerquitflag == 0)
        return;

    thisPlayer.smoothcamera = false;
    thisPlayer.horizAngleAdjust = 0;
    thisPlayer.horizSkew        = 0;

    auto const pPlayer = thisPlayer.ps;
    auto const pSprite = &sprite[pPlayer->i];

    ++pPlayer->player_par;

    VM_OnEvent(EVENT_PROCESSINPUT, pPlayer->i, playerNum);

    auto &pInput = thisPlayer.input;
    uint32_t playerBits = pInput.bits;

    if (pPlayer->cheat_phase > 0)
        playerBits = 0;

    if (pPlayer->cursectnum == -1)
    {
        pPlayer->cursectnum = pSprite->sectnum;
        updatesector(pPlayer->pos.x, pPlayer->pos.y, &pPlayer->cursectnum);
    }

    if (pPlayer->cursectnum == -1)
    {
        if (pSprite->extra > 0 && ud.noclip == 0)
        {
            LOG_F(WARNING, "%s: player killed by cursectnum == -1!", EDUKE32_FUNCTION);
            P_QuickKill(pPlayer);
#ifndef EDUKE32_STANDALONE
            if (!FURY)
                A_PlaySound(SQUISHED, pPlayer->i);
#endif
        }

        pPlayer->cursectnum = 0;
    }

    // sectorLotag can be set to 0 later on, but the same block sets spritebridge to 1
    int sectorLotag       = sector[pPlayer->cursectnum].lotag;
    int getZRangeClipDist = (pPlayer->clipdist >> 1) - GETZRANGECLIPDISTOFFSET;
    int const stepHeight  = (((TEST_SYNC_KEY(playerBits, SK_CROUCH) && sectorLotag != ST_2_UNDERWATER && ((pPlayer->on_ground && !pPlayer->jumping_toggle)))
                            || (sectorLotag == ST_1_ABOVE_WATER && pPlayer->spritebridge != 1)))
                            ? pPlayer->autostep_sbw
                            : pPlayer->autostep;

    int32_t floorZ, ceilZ, highZhit, lowZhit;

    // if not running Ion Fury, purposely break part of the clipping system
    // this is what makes Duke step up onto sprite constructions he shouldn't automatically step up onto
    if (!FURY)
    {
        getZRangeClipDist = pPlayer->clipdist - 1;
        getzrange(&pPlayer->pos, pPlayer->cursectnum, &ceilZ, &highZhit, &floorZ, &lowZhit, getZRangeClipDist, CLIPMASK0);
    }
    else
    {
        uint16_t const ocstat = pSprite->cstat;
        pSprite->cstat &= ~CSTAT_SPRITE_BLOCK;

        int32_t cz[4], fz[4], hzhit[4], lzhit[4];
        vec3_t pos[4] = { pPlayer->pos, pPlayer->pos, pPlayer->pos, pPlayer->pos };

        int const thirdStep = pPlayer->autostep_sbw / 3;

        pos[0].z += pPlayer->autostep_sbw;
        pos[1].z += thirdStep << 1;
        pos[2].z += thirdStep;

        for (int i = 0; i < 4; ++i)
            getzrange(&pos[i], pPlayer->cursectnum, &cz[i], &hzhit[i], &fz[i], &lzhit[i], getZRangeClipDist, CLIPMASK0);

        pSprite->cstat = ocstat;

        ceilZ    = cz[2];
        highZhit = hzhit[2];

        for (int i = 1; i < 3; ++i)
            if (hzhit[i] == hzhit[i+1])
            {
                ceilZ    = cz[i];
                highZhit = hzhit[i];
            }

        floorZ  = fz[1];
        lowZhit = lzhit[1];

        for (int i = 2; i >= 0; --i)
            if (lzhit[i] == lzhit[i+1])
            {
                floorZ  = fz[i];
                lowZhit = lzhit[i];
            }
    }

    pPlayer->spritebridge = 0;
    pPlayer->sbs          = 0;

#ifdef YAX_ENABLE
    yax_getzsofslope(pPlayer->cursectnum, pPlayer->pos.x, pPlayer->pos.y, &pPlayer->truecz, &pPlayer->truefz);
#else
    getcorrectzsofslope(pPlayer->cursectnum, pPlayer->pos.x, pPlayer->pos.y, &pPlayer->truecz, &pPlayer->truefz);
#endif

    int const trueFloorZ    = pPlayer->truefz;
    int const trueFloorDist = klabs(pPlayer->pos.z - trueFloorZ);
    int const playerShrunk  = (pSprite->yrepeat < 32);

    if ((lowZhit & 49152) == 16384 && sectorLotag == 1 && trueFloorDist > pPlayer->spritezoffset + ZOFFSET2)
        sectorLotag = 0;

#ifndef EDUKE32_STANDALONE
    if (!FURY && (highZhit & 49152) == 49152)
    {
        int const spriteNum = highZhit & (MAXSPRITES-1);

        if ((spriteNum != pPlayer->i && sprite[spriteNum].z + PMINHEIGHT > pPlayer->pos.z)
            || (!playerShrunk && sprite[spriteNum].statnum == STAT_ACTOR && sprite[spriteNum].extra >= 0))
        {
            highZhit = 0;
            ceilZ    = pPlayer->truecz;
        }
    }
#endif

    actor[pPlayer->i].floorz   = floorZ;
    actor[pPlayer->i].ceilingz = ceilZ;

    if ((lowZhit & 49152) == 49152)
    {
        int spriteNum = lowZhit&(MAXSPRITES-1);

        if ((sprite[spriteNum].cstat&33) == 33 || (sprite[spriteNum].cstat&17) == 17 ||
                clipshape_idx_for_sprite((uspriteptr_t)&sprite[spriteNum], -1) >= 0)
        {
            // EDuke32 extension: xvel of 1 makes a sprite be never regarded as a bridge.

            if (sectorLotag != ST_2_UNDERWATER && (sprite[spriteNum].xvel & 1) == 0)
            {
                sectorLotag             = 0;
                pPlayer->footprintcount = 0;
                pPlayer->spritebridge   = 1;
                pPlayer->sbs            = spriteNum;
            }
        }
        else if (A_CheckEnemySprite(&sprite[spriteNum]) && sprite[spriteNum].xrepeat > 24
                 && klabs(pSprite->z - sprite[spriteNum].z) < (84 << 8))
        {
            // TX: I think this is what makes the player slide off enemies... might
            // be a good sprite flag to add later.
            // Helix: there's also SLIDE_ABOVE_ENEMY.
            int spriteAng = getangle(sprite[spriteNum].x - pPlayer->pos.x,
                                       sprite[spriteNum].y - pPlayer->pos.y);
            pPlayer->vel.x -= sintable[(spriteAng + 512) & 2047] << 4;
            pPlayer->vel.y -= sintable[spriteAng & 2047] << 4;
        }
    }

    if (pSprite->extra > 0)
        P_IncurDamage(pPlayer);
    else
    {
        pSprite->extra                  = 0;
        pPlayer->inv_amount[GET_SHIELD] = 0;
    }

    pPlayer->last_extra = pSprite->extra;
    pPlayer->loogcnt    = (pPlayer->loogcnt > 0) ? pPlayer->loogcnt - 1 : 0;

    if (pPlayer->fist_incs && P_DoFist(pPlayer)) return;

    if (pPlayer->timebeforeexit > 1 && pPlayer->last_extra > 0)
    {
        if (--pPlayer->timebeforeexit == GAMETICSPERSEC*5)
        {
            FX_StopAllSounds();
            S_ClearSoundLocks();

            if (pPlayer->customexitsound >= 0)
            {
                S_PlaySound(pPlayer->customexitsound);
                P_DoQuote(QUOTE_WEREGONNAFRYYOURASS,pPlayer);
            }
        }
        else if (pPlayer->timebeforeexit == 1)
        {
            P_EndLevel();
            return;
        }
    }

    if (pPlayer->pals.f > 0)
        pPlayer->pals.f--;

    if (pPlayer->fta > 0 && --pPlayer->fta == 0)
    {
        pub = pus = NUMPAGES;
        pPlayer->ftq = 0;
    }

    if (g_levelTextTime > 0)
        g_levelTextTime--;

    if (pSprite->extra <= 0 && !ud.god)
    {
        P_Dead(playerNum, sectorLotag, floorZ, ceilZ);
        return;
    }

    if (pPlayer->transporter_hold > 0)
    {
        pPlayer->transporter_hold--;
        if (pPlayer->transporter_hold == 0 && pPlayer->on_warping_sector)
            pPlayer->transporter_hold = 2;
    }
    else if (pPlayer->transporter_hold < 0)
        pPlayer->transporter_hold++;

    if (pPlayer->newowner >= 0)
    {
        P_UpdatePosWhenViewingCam(pPlayer);
        P_DoCounters(playerNum);

        if (PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == HANDREMOTE_WEAPON)
            P_ProcessWeapon(playerNum);

        return;
    }

    pPlayer->orotscrnang = pPlayer->rotscrnang;

    if (pPlayer->rotscrnang)
    {
        pPlayer->rotscrnang -= (pPlayer->rotscrnang >> 1);

        if (pPlayer->rotscrnang && !(pPlayer->rotscrnang >> 1))
            pPlayer->rotscrnang -= ksgn(pPlayer->rotscrnang);
    }

    pPlayer->olook_ang   = pPlayer->look_ang;

    if (pPlayer->look_ang)
    {
        pPlayer->look_ang -= (pPlayer->look_ang >> 2);

        if (pPlayer->look_ang && !(pPlayer->look_ang >> 2))
            pPlayer->look_ang -= ksgn(pPlayer->look_ang);
    }

    if (TEST_SYNC_KEY(playerBits, SK_LOOK_LEFT))
    {
        // look_left
        if (VM_OnEvent(EVENT_LOOKLEFT,pPlayer->i,playerNum) == 0)
        {
            pPlayer->look_ang -= 152;
            pPlayer->rotscrnang += 24;
        }
    }

    if (TEST_SYNC_KEY(playerBits, SK_LOOK_RIGHT))
    {
        // look_right
        if (VM_OnEvent(EVENT_LOOKRIGHT,pPlayer->i,playerNum) == 0)
        {
            pPlayer->look_ang += 152;
            pPlayer->rotscrnang -= 24;
        }
    }

    int                  velocityModifier = TICSPERFRAME;
    const uint8_t *const weaponFrame      = &pPlayer->kickback_pic;
    int                  floorZOffset     = pPlayer->floorzoffset;
    vec3_t const         backupPos        = pPlayer->opos;

    if (pPlayer->on_crane >= 0)
        goto HORIZONLY;

    pPlayer->weapon_sway = (pSprite->xvel < 32 || pPlayer->on_ground == 0 || pPlayer->bobcounter == 1024)
                           ? (((pPlayer->weapon_sway & 2047) > (1024 + 96))
                           ? (pPlayer->weapon_sway - 96)
                           : (((pPlayer->weapon_sway & 2047) < (1024 - 96)))
                           ? (pPlayer->weapon_sway + 96)
                           : 1024)
                           : pPlayer->bobcounter;

    // NOTE: This silently wraps if the difference is too great, e.g. used to do
    // that when teleported by silent SE7s.
    pSprite->xvel = ksqrt(uhypsq(pPlayer->pos.x - pPlayer->bobpos.x, pPlayer->pos.y - pPlayer->bobpos.y));

    if (pPlayer->on_ground)
        pPlayer->bobcounter += sprite[pPlayer->i].xvel>>1;

    if (ud.noclip == 0 && ((uint16_t)pPlayer->cursectnum >= MAXSECTORS || sector[pPlayer->cursectnum].floorpicnum == MIRROR))
        pPlayer->pos.xy = pPlayer->opos.xy;
    else
        pPlayer->opos.xy = pPlayer->pos.xy;

    pPlayer->bobpos  = pPlayer->pos.xy;
    pPlayer->opos.z  = pPlayer->pos.z;
    pPlayer->opyoff  = pPlayer->pyoff;

    updatesector(pPlayer->pos.x, pPlayer->pos.y, &pPlayer->cursectnum);

    if (!ud.noclip)
        pushmove(&pPlayer->pos, &pPlayer->cursectnum, pPlayer->clipdist - 1, (4L<<8), stepHeight, CLIPMASK0);

    // Shrinking code

    if (sectorLotag == ST_2_UNDERWATER)
        P_DoWater(playerNum, playerBits, floorZ, ceilZ);
    else if (pPlayer->jetpack_on)
        P_DoJetpack(playerNum, playerBits, playerShrunk, sectorLotag, floorZ);
    else
    {
        pPlayer->airleft  = 15 * GAMETICSPERSEC;  // 13 seconds
        pPlayer->scuba_on = 0;

        if (sectorLotag == ST_1_ABOVE_WATER && pPlayer->spritebridge == 0)
        {
            floorZOffset = pPlayer->shrunkzoffset;

            if (!playerShrunk)
            {
                floorZOffset      = pPlayer->waterzoffset;
                pPlayer->pycount += 32;
                pPlayer->pycount &= 2047;
                pPlayer->pyoff    = sintable[pPlayer->pycount] >> 6;

                if (trueFloorDist <= pPlayer->spritezoffset)
                {
                    if (pPlayer->on_ground == 1)
                    {
#ifdef YAX_ENABLE
                        if (yax_getbunch(pPlayer->cursectnum, YAX_FLOOR) == -1)
#endif
                        {
                            if (pPlayer->dummyplayersprite < 0)
                                pPlayer->dummyplayersprite = A_Spawn(pPlayer->i,PLAYERONWATER);

                            sprite[pPlayer->dummyplayersprite].cstat |= 32768;
                            sprite[pPlayer->dummyplayersprite].pal = sprite[pPlayer->i].pal;
                        }
                        pPlayer->footprintpal                  = (sector[pPlayer->cursectnum].floorpicnum == FLOORSLIME) ? 8 : 0;
                        pPlayer->footprintshade                = 0;
                    }
                }
            }
        }
        else if (pPlayer->footprintcount > 0 && pPlayer->on_ground)
        {
            if (pPlayer->cursectnum >= 0 && (sector[pPlayer->cursectnum].floorstat & 2) != 2)
            {
                int spriteNum = -1;

                for (spriteNum = headspritesect[pPlayer->cursectnum]; spriteNum >= 0; spriteNum = nextspritesect[spriteNum])
                {
                    if (sprite[spriteNum].picnum == FOOTPRINTS || sprite[spriteNum].picnum == FOOTPRINTS2 ||
                        sprite[spriteNum].picnum == FOOTPRINTS3 || sprite[spriteNum].picnum == FOOTPRINTS4)
                    {
                        if (klabs(sprite[spriteNum].x - pPlayer->pos.x) < 384 &&
                            klabs(sprite[spriteNum].y - pPlayer->pos.y) < 384)
                            break;
                    }
                }

                if (spriteNum < 0)
                {
                    if (sector[pPlayer->cursectnum].lotag == 0 &&
                        sector[pPlayer->cursectnum].hitag == 0)
#ifdef YAX_ENABLE
                        if (yax_getbunch(pPlayer->cursectnum, YAX_FLOOR) < 0 || (sector[pPlayer->cursectnum].floorstat & 512))
#endif
                        {
                            switch (krand() & 3)
                            {
                                case 0: spriteNum  = A_Spawn(pPlayer->i, FOOTPRINTS); break;
                                case 1: spriteNum  = A_Spawn(pPlayer->i, FOOTPRINTS2); break;
                                case 2: spriteNum  = A_Spawn(pPlayer->i, FOOTPRINTS3); break;
                                default: spriteNum = A_Spawn(pPlayer->i, FOOTPRINTS4); break;
                            }
                            sprite[spriteNum].pal   = pPlayer->footprintpal;
                            sprite[spriteNum].shade = pPlayer->footprintshade;
                            pPlayer->footprintcount--;
                        }
                }
            }
        }

        if (pPlayer->pos.z < (floorZ-floorZOffset))  //falling
        {
            // this is what keeps you glued to the ground when you're running down slopes
            if ((!TEST_SYNC_KEY(playerBits, SK_JUMP) && !(TEST_SYNC_KEY(playerBits, SK_CROUCH))) && pPlayer->on_ground &&
                (sector[pPlayer->cursectnum].floorstat & 2) && pPlayer->pos.z >= (floorZ - floorZOffset - ZOFFSET2))
                pPlayer->pos.z = floorZ - floorZOffset;
            else
            {
                pPlayer->vel.z += pPlayer->gravity;  // (TICSPERFRAME<<6);

                if (pPlayer->vel.z >= ACTOR_MAXFALLINGZVEL)
                    pPlayer->vel.z = ACTOR_MAXFALLINGZVEL;

                if (pPlayer->vel.z > 2400 && pPlayer->falling_counter < 255)
                {
                    pPlayer->falling_counter++;
                    if (pPlayer->falling_counter >= 38 && pPlayer->scream_voice <= FX_Ok)
                    {
                        int32_t voice = A_PlaySound(DUKE_SCREAM,pPlayer->i);
                        if (voice <= 127)  // XXX: p->scream_voice is an int8_t
                            pPlayer->scream_voice = voice;
                    }
                }

                if ((pPlayer->pos.z + pPlayer->vel.z) >= (floorZ - floorZOffset) && pPlayer->cursectnum >= 0)  // hit the ground
                {
                    if (sector[pPlayer->cursectnum].lotag != ST_1_ABOVE_WATER)
                    {
                        if (pPlayer->falling_counter > 62)
                            P_QuickKill(pPlayer);
                        else if (pPlayer->falling_counter > 9)
                        {
                            // Falling damage.
                            pSprite->extra -= pPlayer->falling_counter - (krand() & 3);

#ifndef EDUKE32_STANDALONE
                            if (!FURY)
                            {
                                if (pSprite->extra <= 0)
                                    A_PlaySound(SQUISHED, pPlayer->i);
                                else
                                {
                                    A_PlaySound(DUKE_LAND, pPlayer->i);
                                    A_PlaySound(DUKE_LAND_HURT, pPlayer->i);
                                }
                            }
#endif
                            P_PalFrom(pPlayer, 32, 16, 0, 0);
                        }
#ifndef EDUKE32_STANDALONE
                        else if (!FURY && pPlayer->vel.z > 2048)
                            A_PlaySound(DUKE_LAND, pPlayer->i);
#endif
                    }
                }
                else
                    pPlayer->on_ground = 0;
            }
        }
        else
        {
            pPlayer->falling_counter = 0;

            if (pPlayer->scream_voice > FX_Ok)
            {
                FX_StopSound(pPlayer->scream_voice);
                S_Cleanup();
                pPlayer->scream_voice = -1;
            }

            if (sectorLotag != ST_1_ABOVE_WATER &&
                (pPlayer->on_ground == 0 && pPlayer->vel.z > (ACTOR_MAXFALLINGZVEL >> 1)))
            {
                pPlayer->hard_landing = pPlayer->vel.z >> 10;
                I_AddForceFeedback((pPlayer->hard_landing << FF_PLAYER_DMG_SCALE), (pPlayer->hard_landing << FF_PLAYER_DMG_SCALE), (pPlayer->hard_landing << FF_PLAYER_TIME_SCALE));
            }

            pPlayer->on_ground = 1;

            if (floorZOffset==pPlayer->floorzoffset)
            {
                //Smooth on the ground
                int Zdiff = ((floorZ - floorZOffset) - pPlayer->pos.z) >> 1;

                // why does this even exist?
                if (klabs(Zdiff) < pPlayer->floorzcutoff)
                    Zdiff = 0;
                else if (!playerShrunk)
                    pPlayer->pos.z += Zdiff;

                pPlayer->vel.z -= pPlayer->floorzrebound;

                if (pPlayer->vel.z < 0)
                    pPlayer->vel.z = 0;
            }
            else if (pPlayer->jumping_counter == 0)
            {
                pPlayer->pos.z += ((floorZ - (floorZOffset >> 1)) - pPlayer->pos.z) >> 1;  // Smooth on the water

                if (pPlayer->on_warping_sector == 0 && pPlayer->pos.z > floorZ - pPlayer->minwaterzdist)
                {
                    pPlayer->pos.z = floorZ - pPlayer->minwaterzdist;
                    pPlayer->vel.z >>= 1;
                }
            }

            if (TEST_SYNC_KEY(playerBits, SK_CROUCH))
            {
                // crouching
                if (VM_OnEvent(EVENT_CROUCH,pPlayer->i,playerNum) == 0)
                {
                    if (pPlayer->jumping_toggle == 0)
                    {
                        pPlayer->pos.z += pPlayer->crouchzincrement;
                        pPlayer->crack_time = PCRACKTIME;
                    }
                }
            }

            // jumping
            if (!TEST_SYNC_KEY(playerBits, SK_JUMP) && pPlayer->jumping_toggle)
                pPlayer->jumping_toggle--;
            else if (TEST_SYNC_KEY(playerBits, SK_JUMP) && pPlayer->jumping_toggle == 0 && !pPlayer->jumping_counter)
            {
                // dummy variables must be separate, otherwise breaks TROR computation
                int32_t floorZ2, ceilZ2, dummy, dummy2;

                getzrange(&pPlayer->pos, pPlayer->cursectnum, &ceilZ2, &dummy, &floorZ2, &dummy2, getZRangeClipDist, CLIPMASK0);

                if (klabs(floorZ2-ceilZ2) > pPlayer->spritezoffset + ZOFFSET3)
                {
                    if (VM_OnEvent(EVENT_JUMP,pPlayer->i,playerNum) == 0)
                    {
                        pPlayer->jumping_toggle = 1;

                        if (!TEST_SYNC_KEY(playerBits, SK_CROUCH))
                            pPlayer->jumping_counter = 1;
                        else
                        {
                            pPlayer->jumping_toggle = 2;

                            if (myconnectindex == playerNum)
                                CONTROL_ClearButton(gamefunc_Jump);
                        }
                    }
                }
            }
        }

        if (pPlayer->jumping_counter)
        {
            if (!TEST_SYNC_KEY(playerBits, SK_JUMP) && pPlayer->jumping_toggle)
                pPlayer->jumping_toggle--;

            if (pPlayer->jumping_counter < (1024+256))
            {
                if (sectorLotag == ST_1_ABOVE_WATER && pPlayer->jumping_counter > 768)
                {
                    pPlayer->jumping_counter = 0;
                    pPlayer->vel.z = -512;
                }
                else
                {
                    pPlayer->vel.z -= (sintable[(2048-128+pPlayer->jumping_counter)&2047])/12;
                    pPlayer->jumping_counter += 180;
                    pPlayer->on_ground = 0;
                }
            }
            else
            {
                pPlayer->jumping_counter = 0;
                pPlayer->vel.z = 0;
            }
        }

        if (ceilZ != pPlayer->truecz && pPlayer->jumping_counter && pPlayer->pos.z <= (ceilZ + PMINHEIGHT + 128))
        {
            pPlayer->jumping_counter = 0;
            if (pPlayer->vel.z < 0)
                pPlayer->vel.x = pPlayer->vel.y = 0;
            pPlayer->vel.z = 128;
        }
    }

    if (P_CheckLockedMovement(playerNum) & IL_NOMOVE)
    {
        velocityModifier = 0;
        pPlayer->vel.x   = 0;
        pPlayer->vel.y   = 0;
    }
    else if (pInput.q16avel)
        pPlayer->crack_time = PCRACKTIME;

    if (pPlayer->spritebridge == 0)
    {
        int const floorPicnum = sector[pSprite->sectnum].floorpicnum;

#ifndef EDUKE32_STANDALONE
        if (!FURY && (floorPicnum == PURPLELAVA || sector[pSprite->sectnum].ceilingpicnum == PURPLELAVA))
        {
            if (pPlayer->inv_amount[GET_BOOTS] > 0)
            {
                pPlayer->inv_amount[GET_BOOTS]--;
                pPlayer->inven_icon = ICON_BOOTS;
                if (pPlayer->inv_amount[GET_BOOTS] <= 0)
                    P_SelectNextInvItem(pPlayer);
            }
            else
            {
                if (!A_CheckSoundPlaying(pPlayer->i,DUKE_LONGTERM_PAIN))
                    A_PlaySound(DUKE_LONGTERM_PAIN,pPlayer->i);
                P_PalFrom(pPlayer, 32, 0, 8, 0);
                pSprite->extra--;
            }
        }
#endif
        if (pPlayer->on_ground && trueFloorDist <= pPlayer->spritezoffset+ZOFFSET2 && P_CheckFloorDamage(pPlayer, floorPicnum))
        {
            P_DoQuote(QUOTE_BOOTS_ON, pPlayer);
            pPlayer->inv_amount[GET_BOOTS] -= 2;
            if (pPlayer->inv_amount[GET_BOOTS] <= 0)
            {
                pPlayer->inv_amount[GET_BOOTS] = 0;
                P_SelectNextInvItem(pPlayer);
            }
        }
    }

    if (pInput.extbits & BIT(EK_MOVE_FORWARD))         VM_OnEvent(EVENT_MOVEFORWARD,  pPlayer->i, playerNum);
    if (pInput.extbits & BIT(EK_MOVE_BACKWARD))        VM_OnEvent(EVENT_MOVEBACKWARD, pPlayer->i, playerNum);
    if (pInput.extbits & BIT(EK_STRAFE_LEFT))  VM_OnEvent(EVENT_STRAFELEFT,   pPlayer->i, playerNum);
    if (pInput.extbits & BIT(EK_STRAFE_RIGHT)) VM_OnEvent(EVENT_STRAFERIGHT,  pPlayer->i, playerNum);

    if (pInput.extbits & BIT(EK_TURN_LEFT) || pInput.q16avel < 0)
        VM_OnEvent(EVENT_TURNLEFT, pPlayer->i, playerNum);

    if (pInput.extbits & BIT(EK_TURN_RIGHT) || pInput.q16avel > 0)
        VM_OnEvent(EVENT_TURNRIGHT, pPlayer->i, playerNum);

    if (pPlayer->vel.x || pPlayer->vel.y || pInput.fvel || pInput.svel)
    {
        pPlayer->crack_time = PCRACKTIME;

#ifndef EDUKE32_STANDALONE
        if (!FURY && pPlayer->cursectnum != -1)
        {
            int const checkWalkSound = sintable[pPlayer->bobcounter & 2047] >> 12;

            if (trueFloorDist <= pPlayer->spritezoffset + ZOFFSET2)
            {
                if (checkWalkSound == 1 || checkWalkSound == 3)
                {
                    if (pPlayer->walking_snd_toggle == 0 && pPlayer->on_ground)
                    {
                        switch (sectorLotag)
                        {
                            case 0:
                            {
                                int const walkPicnum = (lowZhit >= 0 && (lowZhit & 49152) == 49152)
                                                       ? TrackerCast(sprite[lowZhit & (MAXSPRITES - 1)].picnum)
                                                       : TrackerCast(sector[pPlayer->cursectnum].floorpicnum);

                                switch (tileGetMapping(walkPicnum))
                                {
                                    case PANNEL1__:
                                    case PANNEL2__:
                                        A_PlaySound(DUKE_WALKINDUCTS, pPlayer->i);
                                        pPlayer->walking_snd_toggle = 1;
                                        break;
                                }
                            }
                            break;

                            case ST_1_ABOVE_WATER:
                                if (!pPlayer->spritebridge)
                                {
                                    if ((krand() & 1) == 0)
                                        A_PlaySound(DUKE_ONWATER, pPlayer->i);
                                    pPlayer->walking_snd_toggle = 1;
                                }
                                break;
                        }
                    }
                }
                else if (pPlayer->walking_snd_toggle > 0)
                    pPlayer->walking_snd_toggle--;
            }

            if (pPlayer->jetpack_on == 0 && pPlayer->inv_amount[GET_STEROIDS] > 0 && pPlayer->inv_amount[GET_STEROIDS] < 400)
                velocityModifier <<= 1;
        }
#endif

        pPlayer->vel.x += (((pInput.fvel) * velocityModifier) << 6);
        pPlayer->vel.y += (((pInput.svel) * velocityModifier) << 6);

        int playerSpeedReduction = 0;

        if (sectorLotag == ST_2_UNDERWATER)
            playerSpeedReduction = pPlayer->swimspeedmodifier;
        else if (((pPlayer->on_ground && TEST_SYNC_KEY(playerBits, SK_CROUCH))
                  || (*weaponFrame > 10 && PWEAPON(playerNum, pPlayer->curr_weapon, WorksLike) == KNEE_WEAPON)))
            playerSpeedReduction = pPlayer->crouchspeedmodifier;
        else if (pPlayer->on_ground && !pPlayer->jumping_toggle && !TEST_SYNC_KEY(playerBits, SK_CROUCH)
                 && !playerShrunk && (klabs(pPlayer->truefz - pPlayer->truecz) - (PMINHEIGHT << 1)) < stepHeight)
        {
            playerSpeedReduction = pPlayer->crouchspeedmodifier;
//            pPlayer->pos.z += PCROUCHINCREMENT;
        }

        pPlayer->vel.x = mulscale16(pPlayer->vel.x, pPlayer->runspeed - playerSpeedReduction);
        pPlayer->vel.y = mulscale16(pPlayer->vel.y, pPlayer->runspeed - playerSpeedReduction);

        if (klabs(pPlayer->vel.x) < 2048 && klabs(pPlayer->vel.y) < 2048)
            pPlayer->vel.x = pPlayer->vel.y = 0;

#ifndef EDUKE32_STANDALONE
        if (!FURY && playerShrunk)
        {
            pPlayer->vel.x = mulscale16(pPlayer->vel.x, pPlayer->runspeed - (pPlayer->runspeed >> 1) + (pPlayer->runspeed >> 2));
            pPlayer->vel.y = mulscale16(pPlayer->vel.y, pPlayer->runspeed - (pPlayer->runspeed >> 1) + (pPlayer->runspeed >> 2));
        }
#endif
    }

    // This makes the player view lower when shrunk. This needs to happen before clipmove().
    // Why? Because stupid fucking Duke3D puts the player sprite entirely into the floor.
#ifndef EDUKE32_STANDALONE
    if (!FURY && playerShrunk && pPlayer->jetpack_on == 0 && sectorLotag != ST_2_UNDERWATER && sectorLotag != ST_1_ABOVE_WATER)
        pPlayer->pos.z += ZOFFSET5 - (sprite[pPlayer->i].yrepeat<<8);
#endif
HORIZONLY:;
    if (ud.noclip)
    {
        pPlayer->pos.x += pPlayer->vel.x >> 14;
        pPlayer->pos.y += pPlayer->vel.y >> 14;
        updatesector(pPlayer->pos.x, pPlayer->pos.y, &pPlayer->cursectnum);
        changespritesect(pPlayer->i, pPlayer->cursectnum);
    }
    else
    {
#ifdef YAX_ENABLE
        int const playerSectNum = pPlayer->cursectnum;
        int16_t   ceilingBunch, floorBunch;

        if (playerSectNum >= 0)
            yax_getbunches(playerSectNum, &ceilingBunch, &floorBunch);

        // This updatesectorz conflicts with Duke3D's way of teleporting through water,
        // so make it a bit conditional... OTOH, this way we have an ugly z jump when
        // changing from above water to underwater

        if ((playerSectNum >= 0 && !(sector[playerSectNum].lotag == ST_1_ABOVE_WATER && pPlayer->on_ground && floorBunch >= 0))
            && ((floorBunch >= 0 && !(sector[playerSectNum].floorstat & 512))
                || (ceilingBunch >= 0 && !(sector[playerSectNum].ceilingstat & 512))))
        {
            pPlayer->cursectnum += MAXSECTORS;  // skip initial z check, restored by updatesectorz
            updatesectorz(pPlayer->pos.x, pPlayer->pos.y, pPlayer->pos.z, &pPlayer->cursectnum);
        }
#endif

        P_ClampZ(pPlayer, sectorLotag, ceilZ, floorZ);

        int const touchObject = FURY ? clipmove(&pPlayer->pos, &pPlayer->cursectnum, pPlayer->vel.x + (pPlayer->fric.x << 9),
                                                   pPlayer->vel.y + (pPlayer->fric.y << 9), pPlayer->clipdist, (4L << 8), stepHeight, CLIPMASK0)
                                        : clipmove(&pPlayer->pos, &pPlayer->cursectnum, pPlayer->vel.x, pPlayer->vel.y, pPlayer->clipdist,
                                                   (4L << 8), stepHeight, CLIPMASK0);

        if (touchObject)
            P_CheckTouchDamage(pPlayer, touchObject);

        if (FURY)
            pPlayer->fric.x = pPlayer->fric.y = 0;
    }

    if (pPlayer->jetpack_on == 0)
    {
        if (pSprite->xvel > 16)
        {
            if (sectorLotag != ST_1_ABOVE_WATER && sectorLotag != ST_2_UNDERWATER && pPlayer->on_ground)
            {
                pPlayer->pycount += 52;
                pPlayer->pycount &= 2047;
                pPlayer->pyoff = klabs(pSprite->xvel * sintable[pPlayer->pycount]) / 1536;
            }
        }
        else if (sectorLotag != ST_2_UNDERWATER && sectorLotag != ST_1_ABOVE_WATER)
            pPlayer->pyoff = 0;

        if (sectorLotag != ST_2_UNDERWATER)
            pPlayer->pos.z += pPlayer->vel.z;
    }

    P_ClampZ(pPlayer, sectorLotag, ceilZ, floorZ);

    if (pPlayer->cursectnum >= 0)
    {
        pPlayer->pos.z += pPlayer->spritezoffset;
        sprite[pPlayer->i].xyz = pPlayer->pos;
        pPlayer->pos.z -= pPlayer->spritezoffset;

        changespritesect(pPlayer->i, pPlayer->cursectnum);
    }

    // ST_2_UNDERWATER
    if (pPlayer->cursectnum >= 0 && sectorLotag < 3)
    {
        auto const pSector = (usectorptr_t)&sector[pPlayer->cursectnum];

        // TRAIN_SECTOR_TO_SE_INDEX
        if ((!ud.noclip && pSector->lotag == ST_31_TWO_WAY_TRAIN) &&
            ((unsigned)pSector->hitag < MAXSPRITES && sprite[pSector->hitag].xvel && actor[pSector->hitag].t_data[0] == 0))
        {
            P_QuickKill(pPlayer);
            return;
        }
    }

#ifndef EDUKE32_STANDALONE
    if (!FURY && (pPlayer->cursectnum >= 0 && trueFloorDist < pPlayer->spritezoffset && pPlayer->on_ground && sectorLotag != ST_1_ABOVE_WATER &&
         playerShrunk == 0 && sector[pPlayer->cursectnum].lotag == ST_1_ABOVE_WATER) && (!A_CheckSoundPlaying(pPlayer->i, DUKE_ONWATER)))
            A_PlaySound(DUKE_ONWATER, pPlayer->i);
#endif

    pPlayer->on_warping_sector = 0;

    bool mashedPotato = false;

    if (pPlayer->cursectnum >= 0 && ud.noclip == 0)
    {
RECHECK:
        int const  pushResult = pushmove(&pPlayer->pos, &pPlayer->cursectnum, pPlayer->clipdist - 1, (4L<<8), (4L<<8), CLIPMASK0, !mashedPotato);
        bool const squishPlayer = pushResult < 0;

        if (squishPlayer || klabs(actor[pPlayer->i].floorz-actor[pPlayer->i].ceilingz) < pPlayer->spritezoffset + ZOFFSET3)
        {
            if (!(sector[pSprite->sectnum].lotag & 0x8000u) &&
                (isanunderoperator(sector[pSprite->sectnum].lotag) || isanearoperator(sector[pSprite->sectnum].lotag)))
                G_ActivateBySector(pSprite->sectnum, pPlayer->i);

            if (squishPlayer)
            {
                if (mashedPotato)
                {
                    P_DoQuote(QUOTE_SQUISHED, pPlayer);
                    P_QuickKill(pPlayer);
                    return;
                }

                mashedPotato = true;
                pPlayer->pos = pPlayer->opos = backupPos;
                goto RECHECK;
            }
        }
        else if (klabs(floorZ - ceilZ) < ZOFFSET5 && isanunderoperator(sector[pPlayer->cursectnum].lotag))
            G_ActivateBySector(pPlayer->cursectnum, pPlayer->i);
    }

    if (pPlayer->return_to_center > 0)
        pPlayer->return_to_center--;

    if (TEST_SYNC_KEY(playerBits, SK_CENTER_VIEW) || pPlayer->hard_landing)
        if (VM_OnEvent(EVENT_RETURNTOCENTER, pPlayer->i,playerNum) == 0)
            pPlayer->return_to_center = 9;

    if (TEST_SYNC_KEY(playerBits, SK_LOOK_UP))
    {
        if (VM_OnEvent(EVENT_LOOKUP,pPlayer->i,playerNum) == 0)
        {
            pPlayer->return_to_center = 9;
            thisPlayer.horizRecenter = true;
            thisPlayer.horizAngleAdjust = 12<<(int)(TEST_SYNC_KEY(playerBits, SK_RUN));
        }
    }

    if (TEST_SYNC_KEY(playerBits, SK_LOOK_DOWN))
    {
        if (VM_OnEvent(EVENT_LOOKDOWN,pPlayer->i,playerNum) == 0)
        {
            pPlayer->return_to_center = 9;
            thisPlayer.horizRecenter = true;
            thisPlayer.horizAngleAdjust = -(12<<(int)(TEST_SYNC_KEY(playerBits, SK_RUN)));
        }
    }

    if (TEST_SYNC_KEY(playerBits, SK_AIM_UP))
    {
        if (VM_OnEvent(EVENT_AIMUP,pPlayer->i,playerNum) == 0)
        {
            thisPlayer.horizAngleAdjust = 6 << (int)(TEST_SYNC_KEY(playerBits, SK_RUN));
            thisPlayer.horizRecenter    = false;
            pPlayer->return_to_center   = 0;
        }
    }

    if (TEST_SYNC_KEY(playerBits, SK_AIM_DOWN))
    {
        if (VM_OnEvent(EVENT_AIMDOWN,pPlayer->i,playerNum) == 0)
        {
            thisPlayer.horizAngleAdjust = -(6 << (int)(TEST_SYNC_KEY(playerBits, SK_RUN)));
            thisPlayer.horizRecenter    = false;
            pPlayer->return_to_center   = 0;
        }
    }

    if (pPlayer->hard_landing > 0)
    {
        thisPlayer.horizSkew = -(pPlayer->hard_landing << 4);
        pPlayer->hard_landing--;
    }

    //Shooting code/changes

    if (pPlayer->show_empty_weapon > 0)
    {
        --pPlayer->show_empty_weapon;

        if (pPlayer->show_empty_weapon == 0 && (pPlayer->weaponswitch & 2) && pPlayer->ammo_amount[pPlayer->curr_weapon] <= 0)
        {
#ifndef EDUKE32_STANDALONE
            if (!FURY)
            {
                if (pPlayer->last_full_weapon == GROW_WEAPON)
                    pPlayer->subweapon |= (1 << GROW_WEAPON);
                else if (pPlayer->last_full_weapon == SHRINKER_WEAPON)
                    pPlayer->subweapon &= ~(1 << GROW_WEAPON);
            }
#endif
            P_AddWeapon(pPlayer, pPlayer->last_full_weapon, 1);
            return;
        }
    }

#ifndef EDUKE32_STANDALONE
    if (!FURY && pPlayer->knee_incs > 0)
    {
        thisPlayer.horizSkew = -48;
        thisPlayer.horizRecenter = true;
        pPlayer->return_to_center = 9;

        if (++pPlayer->knee_incs > 15)
        {
            pPlayer->knee_incs      = 0;
            pPlayer->holster_weapon = 0;
            pPlayer->weapon_pos     = klabs(pPlayer->weapon_pos);

            if (pPlayer->actorsqu >= 0 && sprite[pPlayer->actorsqu].statnum != MAXSTATUS &&
                dist(&sprite[pPlayer->i], &sprite[pPlayer->actorsqu]) < 1400)
            {
                int const dmg = G_DefaultActorHealthForTile(KNEE);
                I_AddForceFeedback((dmg << FF_WEAPON_DMG_SCALE), (dmg << FF_WEAPON_DMG_SCALE), max<int>(FF_WEAPON_MAX_TIME, dmg << FF_WEAPON_TIME_SCALE));

                A_DoGuts(pPlayer->actorsqu, JIBS6, 7);
                A_Spawn(pPlayer->actorsqu, BLOODPOOL);
                A_PlaySound(SQUISHED, pPlayer->actorsqu);
                switch (tileGetMapping(sprite[pPlayer->actorsqu].picnum))
                {
                    case FEM1__:
                    case FEM2__:
                    case FEM3__:
                    case FEM4__:
                    case FEM5__:
                    case FEM6__:
                    case FEM7__:
                    case FEM8__:
                    case FEM9__:
                    case FEM10__:
                    case PODFEM1__:
                    case NAKED1__:
                    case STATUE__:
                        if (sprite[pPlayer->actorsqu].yvel)
                            G_OperateRespawns(sprite[pPlayer->actorsqu].yvel);
                        A_DeleteSprite(pPlayer->actorsqu);
                        break;
                    case APLAYER__:
                    {
                        const int playerSquished = P_Get(pPlayer->actorsqu);
                        P_QuickKill(g_player[playerSquished].ps);
                        g_player[playerSquished].ps->frag_ps = playerNum;
                        break;
                    }
                    default:
                        if (A_CheckEnemySprite(&sprite[pPlayer->actorsqu]))
                            P_AddKills(pPlayer, 1);
                        A_DeleteSprite(pPlayer->actorsqu);
                        break;
                }
            }
            pPlayer->actorsqu = -1;
        }
    }
#endif

    if (P_DoCounters(playerNum))
        return;

    P_ProcessWeapon(playerNum);
}


#include "sjson.h"

int portableBackupSave(const char * path, const char * name, int volume, int level)
{
    if (!FURY)
        return 0;

    char fn[BMAX_PATH];

    if (G_ModDirSnprintf(fn, sizeof(fn), "%s.ext", path))
    {
        return 1;
    }

    sjson_context * ctx = sjson_create_context(0, 0, NULL);
    if (!ctx)
    {
        LOG_F(ERROR, "Could not create sjson_context");
        return 1;
    }

    sjson_node * root = sjson_mkobject(ctx);

    sjson_put_string(ctx, root, "name", name);
    // sjson_put_string(ctx, root, "map", currentboardfilename);
    sjson_put_int(ctx, root, "volume", volume);
    sjson_put_int(ctx, root, "level", level);
    sjson_put_int(ctx, root, "skill", ud.player_skill);

    {
        sjson_node * players = sjson_mkarray(ctx);
        sjson_append_member(ctx, root, "players", players);

        for (int TRAVERSE_CONNECT(p))
        {
            playerdata_t const * playerData = &g_player[p];
            DukePlayer_t const * ps = playerData->ps;
            auto pSprite = (uspritetype const *)&sprite[ps->i];

            sjson_node * player = sjson_mkobject(ctx);
            sjson_append_element(players, player);

            sjson_put_int(ctx, player, "extra", pSprite->extra);
            sjson_put_int(ctx, player, "max_player_health", ps->max_player_health);

            sjson_node * gotweapon = sjson_put_array(ctx, player, "gotweapon");
            for (int w = 0; w < MAX_WEAPONS; ++w)
                sjson_append_element(gotweapon, sjson_mkbool(ctx, !!(ps->gotweapon & (1<<w))));

            sjson_put_int16s(ctx, player, "ammo_amount", ps->ammo_amount, MAX_WEAPONS);
            sjson_put_int16s(ctx, player, "max_ammo_amount", ps->max_ammo_amount, MAX_WEAPONS);
            sjson_put_int16s(ctx, player, "inv_amount", ps->inv_amount, GET_MAX);

            sjson_put_int(ctx, player, "max_shield_amount", ps->max_shield_amount);

            sjson_put_int(ctx, player, "curr_weapon", ps->curr_weapon);
            sjson_put_int(ctx, player, "subweapon", ps->subweapon);
            sjson_put_int(ctx, player, "inven_icon", ps->inven_icon);

            sjson_node* vars = sjson_mkobject(ctx);
            sjson_append_member(ctx, player, "vars", vars);

            for (int j=0; j<g_gameVarCount; j++)
            {
                gamevar_t & var = aGameVars[j];

                if (!(var.flags & GAMEVAR_SERIALIZE))
                    continue;

                if ((var.flags & (GAMEVAR_PERPLAYER|GAMEVAR_PERACTOR)) != GAMEVAR_PERPLAYER)
                    continue;

                sjson_put_int(ctx, vars, var.szLabel, Gv_GetVar(j, ps->i, p));
            }
        }
    }

    {
        sjson_node * vars = sjson_mkobject(ctx);
        sjson_append_member(ctx, root, "vars", vars);

        for (int j=0; j<g_gameVarCount; j++)
        {
            gamevar_t & var = aGameVars[j];

            if (!(var.flags & GAMEVAR_SERIALIZE))
                continue;

            if (var.flags & (GAMEVAR_PERPLAYER|GAMEVAR_PERACTOR))
                continue;

            sjson_put_int(ctx, vars, var.szLabel, Gv_GetVar(j));
        }
    }

    char errmsg[256];
    if (!sjson_check(root, errmsg))
    {
        LOG_F(ERROR, "%s", errmsg);
        sjson_destroy_context(ctx);
        return 1;
    }

    char * encoded = sjson_stringify(ctx, root, "  ");

    buildvfs_FILE fil = buildvfs_fopen_write(fn);
    if (!fil)
    {
        sjson_destroy_context(ctx);
        return 1;
    }

    buildvfs_fwrite(encoded, strlen(encoded), 1, fil);
    buildvfs_fclose(fil);

    sjson_free_string(ctx, encoded);
    sjson_destroy_context(ctx);

    return 0;
}
