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
#include "build.h"
#include "mmulti.h"
#include "actor.h"
#include "blood.h"
#include "callback.h"
#include "config.h"
#include "controls.h"
#include "demo.h"
#include "eventq.h"
#include "fx.h"
#include "gib.h"
#include "globals.h"
#include "levels.h"
#include "loadsave.h"
#include "map2d.h"
#include "network.h"
#include "player.h"
#include "seq.h"
#include "sfx.h"
#include "sound.h"
#include "tile.h"
#include "triggers.h"
#include "trig.h"
#include "view.h"
#include "warp.h"
#include "weapon.h"
#include "common_game.h"
#include "messages.h"
#ifdef NOONE_EXTENSIONS
#include "nnexts.h"
#include "nnextsif.h"
#endif

PLAYER gPlayer[kMaxPlayers];
PLAYER *gMe, *gView;

PROFILE gProfile[kMaxPlayers];

bool gBlueFlagDropped = false;
bool gRedFlagDropped = false;

int gPlayerScores[kMaxPlayers];
ClockTicks gPlayerScoreTicks[kMaxPlayers];

// V = has effect in game, X = no effect in game
POWERUPINFO gPowerUpInfo[kMaxPowerUps] = {
    { -1, 1, 1, 1 },            // 00: V keys
    { -1, 1, 1, 1 },            // 01: V keys
    { -1, 1, 1, 1 },            // 02: V keys
    { -1, 1, 1, 1 },            // 03: V keys
    { -1, 1, 1, 1 },            // 04: V keys
    { -1, 1, 1, 1 },            // 05: V keys
    { -1, 1, 1, 1 },            // 06: V keys
    { -1, 0, 100, 100 },        // 07: V doctor's bag
    { -1, 0, 50, 100 },         // 08: V medicine pouch
    { -1, 0, 20, 100 },         // 09: V life essense
    { -1, 0, 100, 200 },        // 10: V life seed
    { -1, 0, 2, 200 },          // 11: V red potion
    { 783, 0, 3600, 432000 },   // 12: V feather fall
    { 896, 0, 3600, 432000 },   // 13: V cloak of invisibility
    { 825, 1, 3600, 432000 },   // 14: V death mask (invulnerability)
    { 827, 0, 3600, 432000 },   // 15: V jump boots
    { 828, 0, 3600, 432000 },   // 16: X raven flight
    { 829, 0, 3600, 1728000 },  // 17: V guns akimbo
    { 830, 0, 3600, 432000 },   // 18: V diving suit
    { 831, 0, 3600, 432000 },   // 19: V gas mask
    { -1, 0, 3600, 432000 },    // 20: X clone
    { 2566, 0, 3600, 432000 },  // 21: V crystal ball
    { 836, 0, 3600, 432000 },   // 22: X decoy
    { 853, 0, 3600, 432000 },   // 23: V doppleganger
    { 2428, 0, 3600, 432000 },  // 24: V reflective shots
    { 839, 0, 3600, 432000 },   // 25: V beast vision
    { 768, 0, 3600, 432000 },   // 26: X cloak of shadow (useless)
    { 840, 0, 3600, 432000 },   // 27: X rage shroom
    { 841, 0, 900, 432000 },    // 28: V delirium shroom
    { 842, 0, 3600, 432000 },   // 29: V grow shroom (gModernMap only)
    { 843, 0, 3600, 432000 },   // 30: V shrink shroom (gModernMap only)
    { -1, 0, 3600, 432000 },    // 31: X death mask (useless)
    { -1, 0, 3600, 432000 },    // 32: X wine goblet
    { -1, 0, 3600, 432000 },    // 33: X wine bottle
    { -1, 0, 3600, 432000 },    // 34: X skull grail
    { -1, 0, 3600, 432000 },    // 35: X silver grail
    { -1, 0, 3600, 432000 },    // 36: X tome
    { -1, 0, 3600, 432000 },    // 37: X black chest
    { -1, 0, 3600, 432000 },    // 38: X wooden chest
    { 837, 1, 3600, 432000 },   // 39: V asbestos armor
    { -1, 0, 1, 432000 },       // 40: V basic armor
    { -1, 0, 1, 432000 },       // 41: V body armor
    { -1, 0, 1, 432000 },       // 42: V fire armor
    { -1, 0, 1, 432000 },       // 43: V spirit armor
    { -1, 0, 1, 432000 },       // 44: V super armor
    { 0, 0, 0, 0 },             // 45: ? unknown
    { 0, 0, 0, 0 },             // 46: ? unknown
    { 0, 0, 0, 0 },             // 47: ? unknown
    { 0, 0, 0, 0 },             // 48: ? unknown
    { 0, 0, 0, 0 },             // 49: X dummy
    { 833, 1, 1, 1 }            // 50: V kModernItemLevelMap (gModernMap only)
};

int Handicap[] = {
    144, 208, 256, 304, 368
};

POSTURE gPostureDefaults[kModeMax][kPostureMax] = {
    
    // normal human
    {
        { 0x4000, 0x4000, 0x4000, 14, 17, 24, 16, 32, 80, 0x1600, 0x1200, 0xc00, 0x90, -0xbaaaa, -0x175555 },
        { 0x1200, 0x1200, 0x1200, 14, 17, 24, 16, 32, 80, 0x1400, 0x1000, -0x600, 0xb0, 0x5b05, 0 },
        { 0x2000, 0x2000, 0x2000, 22, 28, 24, 16, 16, 40, 0x800, 0x600, -0x600, 0xb0, 0, 0 },
    },

    // normal beast
    {
        { 0x4000, 0x4000, 0x4000, 14, 17, 24, 16, 32, 80, 0x1600, 0x1200, 0xc00, 0x90, -0xbaaaa, -0x175555 },
        { 0x1200, 0x1200, 0x1200, 14, 17, 24, 16, 32, 80, 0x1400, 0x1000, -0x600, 0xb0, 0x5b05, 0 },
        { 0x2000, 0x2000, 0x2000, 22, 28, 24, 16, 16, 40, 0x800, 0x600, -0x600, 0xb0, 0, 0 },
    },

    // shrink human
    {
        { 10384, 10384, 10384, 14, 17, 24, 16, 32, 80, 5632, 4608, 3072, 144, -564586, -1329173 },
        { 2108, 2108, 2108, 14, 17, 24, 16, 32, 80, 5120, 4096, -1536, 176, 0x5b05, 0 },
        { 2192, 2192, 2192, 22, 28, 24, 16, 16, 40, 2048, 1536, -1536, 176, 0, 0 },
    },

    // grown human
    {
        { 19384, 19384, 19384, 14, 17, 24, 16, 32, 80, 5632, 4608, 3072, 144, -1014586, -1779173 },
        { 5608, 5608, 5608, 14, 17, 24, 16, 32, 80, 5120, 4096, -1536, 176, 0x5b05, 0 },
        { 11192, 11192, 11192, 22, 28, 24, 16, 16, 40, 2048, 1536, -1536, 176, 0, 0 },
    },
};

AMMOINFO gAmmoInfo[] = {
    { 0, -1 },
    { 100, -1 },
    { 100, 4 },
    { 500, 5 },
    { 100, -1 },
    { 50, -1 },
    { 2880, -1 },
    { 250, -1 },
    { 100, -1 },
    { 100, -1 },
    { 50, -1 },
    { 50, -1 },
};

struct ARMORDATA {
    int at0;
    int at4;
    int at8;
    int atc;
    int at10;
    int at14;
};
ARMORDATA armorData[5] = {
    { 0x320, 0x640, 0x320, 0x640, 0x320, 0x640 },
    { 0x640, 0x640, 0, 0x640, 0, 0x640 },
    { 0, 0x640, 0x640, 0x640, 0, 0x640 },
    { 0, 0x640, 0, 0x640, 0x640, 0x640 },
    { 0xc80, 0xc80, 0xc80, 0xc80, 0xc80, 0xc80 }
};

void PlayerSurvive(int, int);
void PlayerKneelsOver(int, int);

int nPlayerSurviveClient = seqRegisterClient(PlayerSurvive);
int nPlayerKneelClient = seqRegisterClient(PlayerKneelsOver);

struct VICTORY {
    const char *at0;
    int at4;
};

VICTORY gVictory[] = {
    { "%s boned %s like a fish", 4100 },
    { "%s castrated %s", 4101 },
    { "%s creamed %s", 4102 },
    { "%s destroyed %s", 4103 },
    { "%s diced %s", 4104 },
    { "%s disemboweled %s", 4105 },
    { "%s flattened %s", 4106 },
    { "%s gave %s Anal Justice", 4107 },
    { "%s gave AnAl MaDnEsS to %s", 4108 },
    { "%s hurt %s real bad", 4109 },
    { "%s killed %s", 4110 },
    { "%s made mincemeat out of %s", 4111 },
    { "%s massacred %s", 4112 },
    { "%s mutilated %s", 4113 },
    { "%s reamed %s", 4114 },
    { "%s ripped %s a new orifice", 4115 },
    { "%s slaughtered %s", 4116 },
    { "%s sliced %s", 4117 },
    { "%s smashed %s", 4118 },
    { "%s sodomized %s", 4119 },
    { "%s splattered %s", 4120 },
    { "%s squashed %s", 4121 },
    { "%s throttled %s", 4122 },
    { "%s wasted %s", 4123 },
    { "%s body bagged %s", 4124 },
};

struct SUICIDE {
    const char *at0;
    int at4;
};

SUICIDE gSuicide[] = {
    { "%s is excrement", 4202 },
    { "%s is hamburger", 4203 },
    { "%s suffered scrotum separation", 4204 },
    { "%s volunteered for population control", 4206 },
    { "%s has suicided", 4207 },
};

struct DAMAGEINFO {
    int at0;
    int at4[3];
    int at10[3];
};

DAMAGEINFO damageInfo[kDamageMax] = {
    { -1, 731, 732, 733, 710, 710, 710 },
    { 1, 742, 743, 744, 711, 711, 711 },
    { 0, 731, 732, 733, 712, 712, 712 },
    { 1, 731, 732, 733, 713, 713, 713 },
    { -1, 724, 724, 724, 714, 714, 714 },
    { 2, 731, 732, 733, 715, 715, 715 },
    { 0, 0, 0, 0, 0, 0, 0 }
};

uint32_t PLAYER::CalcNonSpriteChecksum(void)
{
    // This was originally written to calculate the checksum
    // the way OUWB v1.21 does. Therefore, certain bits may
    // be skipped or calculated in a different order.
    int i;
    uint32_t sum = 0;
    sum += used1&0xFFFFFFFF;
    sum += weaponQav&0xFFFFFFFF;
    sum += qavCallback&0xFFFFFFFF;
    sum += (!!isRunning) | ((posture&0xFFFFFF)<<8);
    sum += ((posture>>24)&255) | ((sceneQav&0xFFFFFF)<<8);
    sum += ((sceneQav>>24)&255) | ((bobPhase&0xFFFFFF)<<8);
    sum += ((bobPhase>>24)&255) | ((bobAmp&0xFFFFFF)<<8);
    sum += ((bobAmp>>24)&255) | ((bobHeight&0xFFFFFF)<<8);
    sum += ((bobHeight>>24)&255) | ((bobWidth&0xFFFFFF)<<8);
    sum += ((bobWidth>>24)&255) | ((swayPhase&0xFFFFFF)<<8);
    sum += ((swayPhase>>24)&255) | ((swayAmp&0xFFFFFF)<<8);
    sum += ((swayAmp>>24)&255) | ((swayHeight&0xFFFFFF)<<8);
    sum += ((swayHeight>>24)&255) | ((swayWidth&0xFFFFFF)<<8);
    sum += ((swayWidth>>24)&255) | ((nPlayer&0xFFFFFF)<<8);
    sum += ((nPlayer>>24)&255) | ((nSprite&0xFFFFFF)<<8);
    sum += ((nSprite>>24)&255) | ((lifeMode&0xFFFFFF)<<8);
    sum += ((lifeMode>>24)&255) | ((bloodlust&0xFFFFFF)<<8);
    sum += ((bloodlust>>24)&255) | ((zView&0xFFFFFF)<<8);
    sum += ((zView>>24)&255) | ((zViewVel&0xFFFFFF)<<8);
    sum += ((zViewVel>>24)&255) | ((zWeapon&0xFFFFFF)<<8);
    sum += ((zWeapon>>24)&255) | ((zWeaponVel&0xFFFFFF)<<8);

    int32_t look = fix16_to_int(q16look), horiz = fix16_to_int(q16horiz),
            slopehoriz = fix16_to_int(q16slopehoriz);

    sum += ((zWeaponVel>>24)&255) | ((look&0xFFFFFF)<<8);
    sum += ((look>>24)&255) | ((horiz&0xFFFFFF)<<8);
    sum += ((horiz>>24)&255) | ((slopehoriz&0xFFFFFF)<<8);
    sum += ((slopehoriz>>24)&255) | ((slope&0xFFFFFF)<<8);
    sum += ((slope>>24)&255) | ((!!isUnderwater)<<8) |
           ((!!hasKey[0])<<16) | ((!!hasKey[1])<<24);
    sum += (!!hasKey[2]) | ((!!hasKey[3])<<8) |
           ((!!hasKey[4])<<16) | ((!!hasKey[5])<<24);
    sum += (!!hasKey[6]) | ((!!hasKey[7])<<8) |
           ((hasFlag&255)<<16) | ((used2[0]&255)<<24);
    sum += ((used2[0]>>8)&255) | ((used2[1]&65535)<<16) | ((used2[2]&255)<<24);
    sum += ((used2[2]>>8)&255) | ((used2[3]&65535)<<16) | ((used2[4]&255)<<24);
    sum += ((used2[4]>>8)&255) | ((used2[5]&65535)<<16) | ((used2[6]&255)<<24);
    sum += ((used2[6]>>8)&255) | ((used2[7]&65535)<<16) |
            ((damageControl[0]&255)<<24);
    for (i = 0; i < kDamageMax-1; ++i)
        sum += ((damageControl[i]>>8)&0xFFFFFF) | ((damageControl[i+1]&255)<<24);
    sum += ((damageControl[kDamageMax-1]>>8)&0xFFFFFF) | ((curWeapon&255)<<24);
    sum += (nextWeapon&255) | ((weaponTimer&0xFFFFFF)<<8);
    sum += ((weaponTimer>>24)&255) | ((weaponState&0xFFFFFF)<<8);
    sum += ((weaponState>>24)&255) | ((weaponAmmo&0xFFFFFF)<<8);
    sum += ((weaponAmmo>>24)&255) | (!!hasWeapon[0]<<8) |
           (!!hasWeapon[1]<<16) | (!!hasWeapon[2]<<24);
    sum += (!!hasWeapon[3]) | (!!hasWeapon[4]<<8) |
           (!!hasWeapon[5]<<16) | (!!hasWeapon[6]<<24);
    sum += (!!hasWeapon[7]) | (!!hasWeapon[8]<<8) |
           (!!hasWeapon[9]<<16) | (!!hasWeapon[10]<<24);
    sum += (!!hasWeapon[11]) | (!!hasWeapon[12]<<8) |
           (!!hasWeapon[13]<<16) | ((weaponMode[0]&255)<<24);
    for (i = 0; i < 13; ++i)
        sum += ((weaponMode[i]>>8)&0xFFFFFF) | ((weaponMode[i+1]&255)<<24);
    sum += ((weaponMode[13]>>8)&0xFFFFFF) | ((weaponOrder[0][0]&255)<<24);
    for (i = 0; i < 13; ++i)
        sum += ((weaponOrder[0][i]>>8)&0xFFFFFF) | ((weaponOrder[0][i+1]&255)<<24);
    sum += ((weaponOrder[0][13]>>8)&0xFFFFFF) | ((weaponOrder[1][0]&255)<<24);
    for (i = 0; i < 13; ++i)
        sum += ((weaponOrder[1][i]>>8)&0xFFFFFF) | ((weaponOrder[1][i+1]&255)<<24);
    sum += ((weaponOrder[1][13]>>8)&0xFFFFFF) | ((ammoCount[0]&255)<<24);
    for (i = 0; i < 11; ++i)
        sum += ((ammoCount[i]>>8)&0xFFFFFF) | ((ammoCount[i+1]&255)<<24);
    sum += ((ammoCount[11]>>8)&0xFFFFFF) | ((!!qavLoop)<<24);
    sum += fuseTime&0xFFFFFFFF;
    sum += throwTime&0xFFFFFFFF;
    sum += throwPower&0xFFFFFFFF;
    sum += aim.dx&0xFFFFFFFF;
    sum += aim.dy&0xFFFFFFFF;
    sum += aim.dz&0xFFFFFFFF;
    sum += relAim.dx&0xFFFFFFFF;
    sum += relAim.dy&0xFFFFFFFF;
    sum += relAim.dz&0xFFFFFFFF;
    sum += aimTarget&0xFFFFFFFF;
    sum += aimTargetsCount&0xFFFFFFFF;
    for (i = 0; i < 16; i += 2)
        sum += (aimTargets[i]&65535) | ((aimTargets[i+1]&65535) << 16);
    sum += deathTime&0xFFFFFFFF;
    for (i = 0; i < 49; ++i)
        sum += pwUpTime[i]&0xFFFFFFFF;
    sum += fragCount&0xFFFFFFFF;
    for (i = 0; i < 8; ++i)
        sum += fragInfo[i]&0xFFFFFFFF;
    sum += teamId&0xFFFFFFFF;
    sum += fraggerId&0xFFFFFFFF;
    sum += underwaterTime&0xFFFFFFFF;
    sum += bloodTime&0xFFFFFFFF;
    sum += gooTime&0xFFFFFFFF;
    sum += wetTime&0xFFFFFFFF;
    sum += bubbleTime&0xFFFFFFFF;
    sum += at306&0xFFFFFFFF;
    sum += restTime&0xFFFFFFFF;
    sum += kickPower&0xFFFFFFFF;
    sum += laughCount&0xFFFFFFFF;
    sum += spin&0xFFFFFFFF;
    sum += (!!godMode) | ((!!fallScream)<<8) |
           ((!!cantJump)<<16) | ((packItemTime&255)<<24);
    sum += ((packItemTime>>8)&0xFFFFFF) | ((packItemId&255)<<24);
    sum += ((packItemId>>8)&0xFFFFFF) | ((!!packSlots[0].isActive)<<24);
    sum += packSlots[0].curAmount&0xFFFFFFFF;
    sum += (!!packSlots[1].isActive) | ((packSlots[1].curAmount&0xFFFFFF)<<8);
    sum += ((packSlots[1].curAmount>>24)&255) |
           ((!!packSlots[2].isActive)<<8) | ((packSlots[2].curAmount&65535)<<16);
    sum += ((packSlots[2].curAmount>>16)&65535) |
           ((!!packSlots[3].isActive)<<16) | ((packSlots[3].curAmount&255)<<8);
    sum += ((packSlots[3].curAmount>>8)&0xFFFFFF) |
           ((!!packSlots[4].isActive)<<24);
    sum += packSlots[4].curAmount&0xFFFFFFFF;
    for (i = 0; i < 3; ++i)
        sum += armor[i]&0xFFFFFFFF;
    sum += voodooTarget&0xFFFFFFFF;
    sum += voodooTargets&0xFFFFFFFF;
    sum += voodooVar1&0xFFFFFFFF;
    sum += vodooVar2&0xFFFFFFFF;
    sum += flickerEffect&0xFFFFFFFF;
    sum += tiltEffect&0xFFFFFFFF;
    sum += visibility&0xFFFFFFFF;
    sum += painEffect&0xFFFFFFFF;
    sum += blindEffect&0xFFFFFFFF;
    sum += chokeEffect&0xFFFFFFFF;
    sum += handTime&0xFFFFFFFF;
    sum += (!!hand) | ((pickupEffect&0xFFFFFF)<<8);
    sum += ((pickupEffect>>24)&255) | ((flashEffect&0xFFFFFF)<<8);
    sum += ((flashEffect>>24)&255) | ((quakeEffect&0xFFFFFF)<<8);
    return sum;
}

int powerupCheck(PLAYER *pPlayer, int nPowerUp)
{
    dassert(pPlayer != NULL);
    dassert(nPowerUp >= 0 && nPowerUp < kMaxPowerUps);
    int nPack = powerupToPackItem(nPowerUp);
    if (nPack >= 0 && !packItemActive(pPlayer, nPack))
        return 0;
    return pPlayer->pwUpTime[nPowerUp];
}

char powerupAkimboWeapons(int nWeapon)
{
    switch (nWeapon)
    {
    case kWeaponFlare:
    case kWeaponShotgun:
    case kWeaponTommy:
    case kWeaponNapalm:
    case kWeaponTesla:
        return 1;
    default:
        break;
    }
    return 0;
}

char powerupActivate(PLAYER *pPlayer, int nPowerUp)
{
    if (powerupCheck(pPlayer, nPowerUp) > 0 && gPowerUpInfo[nPowerUp].pickupOnce)
        return 0;
    if (!pPlayer->pwUpTime[nPowerUp])
        pPlayer->pwUpTime[nPowerUp] = gPowerUpInfo[nPowerUp].bonusTime;
    int nPack = powerupToPackItem(nPowerUp);
    if (nPack >= 0)
        pPlayer->packSlots[nPack].isActive = 1;
    
    switch (nPowerUp + kItemBase) {
        #ifdef NOONE_EXTENSIONS
        case kItemModernMapLevel:
            if (gModernMap) gFullMap = true;
            break;
        case kItemShroomShrink:
            if (!gModernMap) break;
            else if (isGrown(pPlayer->pSprite)) playerDeactivateShrooms(pPlayer);
            else playerSizeShrink(pPlayer, 2);
            break;
        case kItemShroomGrow:
            if (!gModernMap) break;
            else if (isShrinked(pPlayer->pSprite)) playerDeactivateShrooms(pPlayer);
            else {
                playerSizeGrow(pPlayer, 2);
                if (powerupCheck(&gPlayer[pPlayer->pSprite->type - kDudePlayer1], kPwUpShadowCloak) > 0) {
                    powerupDeactivate(pPlayer, kPwUpShadowCloak);
                    pPlayer->pwUpTime[kPwUpShadowCloak] = 0;
                }

                if (ceilIsTooLow(pPlayer->pSprite))
                    actDamageSprite(pPlayer->pSprite->index, pPlayer->pSprite, kDamageExplode, 65535);
            }
            break;
        #endif
        case kItemFeatherFall:
        case kItemJumpBoots:
            pPlayer->damageControl[kDamageFall]++;
            break;
        case kItemReflectShots: // reflective shots
            if (pPlayer == gMe && gGameOptions.nGameType == kGameTypeSinglePlayer)
                sfxSetReverb2(1);
            break;
        case kItemDeathMask:
            for (int i = 0; i < kDamageMax; i++)
                pPlayer->damageControl[i]++;
            break;
        case kItemDivingSuit: // diving suit
            pPlayer->damageControl[kDamageDrown]++;
            if (pPlayer == gMe && gGameOptions.nGameType == kGameTypeSinglePlayer)
                sfxSetReverb(1);
            break;
        case kItemGasMask:
            pPlayer->damageControl[kDamageDrown]++;
            break;
        case kItemArmorAsbest:
            pPlayer->damageControl[kDamageBurn]++;
            break;
        case kItemTwoGuns:
            if (!VanillaMode() && !powerupAkimboWeapons(pPlayer->curWeapon)) // if weapon doesn't have a akimbo state, don't raise weapon
                break;
            pPlayer->input.newWeapon = pPlayer->curWeapon;
            WeaponRaise(pPlayer);
            break;
    }
    sfxPlay3DSound(pPlayer->pSprite, 776, -1, 0);
    return 1;
}

void powerupDeactivate(PLAYER *pPlayer, int nPowerUp)
{
    int nPack = powerupToPackItem(nPowerUp);
    if (nPack >= 0)
        pPlayer->packSlots[nPack].isActive = 0;
    
    switch (nPowerUp + kItemBase) {
        #ifdef NOONE_EXTENSIONS
        case kItemShroomShrink:
            if (gModernMap) {
                playerSizeReset(pPlayer);
                if (ceilIsTooLow(pPlayer->pSprite))
                    actDamageSprite(pPlayer->pSprite->index, pPlayer->pSprite, kDamageExplode, 65535);
            }
            break;
        case kItemShroomGrow:
            if (gModernMap) playerSizeReset(pPlayer);
            break;
        #endif
        case kItemFeatherFall:
        case kItemJumpBoots:
            pPlayer->damageControl[kDamageFall]--;
            break;
        case kItemDeathMask:
            for (int i = 0; i < kDamageMax; i++)
                pPlayer->damageControl[i]--;
            break;
        case kItemDivingSuit:
            pPlayer->damageControl[kDamageDrown]--;
            if ((pPlayer == gMe) && (VanillaMode() || !powerupCheck(pPlayer, kPwUpReflectShots)))
                sfxSetReverb(0);
            break;
        case kItemReflectShots:
            if ((pPlayer == gMe) && (VanillaMode() || !packItemActive(pPlayer, kPackDivingSuit)))
                sfxSetReverb(0);
            break;
        case kItemGasMask:
            pPlayer->damageControl[kDamageDrown]--;
            break;
        case kItemArmorAsbest:
            pPlayer->damageControl[kDamageBurn]--;
            break;
        case kItemTwoGuns:
            if (!VanillaMode() && !powerupAkimboWeapons(pPlayer->curWeapon)) // if weapon doesn't have a akimbo state, don't raise weapon
                break;
            pPlayer->input.newWeapon = pPlayer->curWeapon;
            WeaponRaise(pPlayer);
            break;
    }
}

void powerupSetState(PLAYER *pPlayer, int nPowerUp, char bState)
{
    if (!bState)
        powerupActivate(pPlayer, nPowerUp);
    else
        powerupDeactivate(pPlayer, nPowerUp);
}

void powerupProcess(PLAYER *pPlayer)
{
    pPlayer->packItemTime = ClipLow(pPlayer->packItemTime-kTicsPerFrame, 0);
    for (int i = kMaxPowerUps-1; i >= 0; i--)
    {
        int nPack = powerupToPackItem(i);
        if (nPack >= 0)
        {
            if (pPlayer->packSlots[nPack].isActive)
            {
                pPlayer->pwUpTime[i] = ClipLow(pPlayer->pwUpTime[i]-kTicsPerFrame, 0);
                if (pPlayer->pwUpTime[i])
                    pPlayer->packSlots[nPack].curAmount = (100*pPlayer->pwUpTime[i])/gPowerUpInfo[i].bonusTime;
                else
                {
                    powerupDeactivate(pPlayer, i);
                    if (pPlayer->packItemId == nPack)
                        pPlayer->packItemId = 0;
                }
            }
        }
        else if (pPlayer->pwUpTime[i] > 0)
        {
            pPlayer->pwUpTime[i] = ClipLow(pPlayer->pwUpTime[i]-kTicsPerFrame, 0);
            if (!pPlayer->pwUpTime[i])
                powerupDeactivate(pPlayer, i);
        }
    }
}

void powerupClear(PLAYER *pPlayer)
{
    if (!VanillaMode() && (pPlayer == gMe)) // turn off reverb sound effects
    {
        if (packItemActive(pPlayer, kPackDivingSuit) || powerupCheck(pPlayer, kPwUpReflectShots)) // if diving suit/reflective shots powerup is active, turn off reverb effect
            sfxSetReverb(0);
    }
    for (int i = 0; i < kMaxPowerUps; i++)
    {
        pPlayer->pwUpTime[i] = 0;
    }
}

void powerupInit(void)
{
}

int packItemToPowerup(int nPack)
{
    int nPowerUp = -1;
    switch (nPack) {
        case kPackMedKit:
            break;
        case kPackDivingSuit:
            nPowerUp = kPwUpDivingSuit;
            break;
        case kPackCrystalBall:
            nPowerUp = kPwUpCrystalBall;
            break;
        case kPackBeastVision:
            nPowerUp = kPwUpBeastVision;
            break;
        case kPackJumpBoots:
            nPowerUp = kPwUpJumpBoots;
            break;
        default:
            ThrowError("Unhandled pack item %d", nPack);
            break;
    }
    return nPowerUp;
}

int powerupToPackItem(int nPowerUp)
{
    switch (nPowerUp) {
        case kPwUpDivingSuit:
            return kPackDivingSuit;
        case kPwUpCrystalBall:
            return kPackCrystalBall;
        case kPwUpBeastVision:
            return kPackBeastVision;
        case kPwUpJumpBoots:
            return kPackJumpBoots;
    }
    return -1;
}

char packAddItem(PLAYER *pPlayer, unsigned int nPack)
{
    if (nPack < kPackMax)
    {
        if (pPlayer->packSlots[nPack].curAmount >= 100)
            return 0;
        pPlayer->packSlots[nPack].curAmount = 100;
        int nPowerUp = packItemToPowerup(nPack);
        if (nPowerUp >= 0)
            pPlayer->pwUpTime[nPowerUp] = gPowerUpInfo[nPowerUp].bonusTime;
        if (pPlayer->packItemId == -1)
            pPlayer->packItemId = nPack;
        if (!pPlayer->packSlots[pPlayer->packItemId].curAmount)
            pPlayer->packItemId = nPack;
    }
    else
        ThrowError("Unhandled pack item %d", nPack);
    return 1;
}

int packCheckItem(PLAYER *pPlayer, int nPack)
{
    return pPlayer->packSlots[nPack].curAmount;
}

char packItemActive(PLAYER *pPlayer, int nPack)
{
    return pPlayer->packSlots[nPack].isActive;
}

void packUseItem(PLAYER *pPlayer, int nPack)
{
    char bActivate = 0;
    int nPowerUp = -1;
    if (pPlayer->packSlots[nPack].curAmount > 0)
    {
        switch (nPack)
        {
        case kPackMedKit:
        {
            XSPRITE *pXSprite = pPlayer->pXSprite;
            unsigned int health = pXSprite->health>>4;
            if (health < 100)
            {
                int heal = ClipHigh(100-health, pPlayer->packSlots[0].curAmount);
                actHealDude(pXSprite, heal, 100);
                pPlayer->packSlots[kPackMedKit].curAmount -= heal;
            }
            break;
        }
        case kPackDivingSuit:
            bActivate = 1;
            nPowerUp = kPwUpDivingSuit;
            break;
        case kPackCrystalBall:
            bActivate = 1;
            nPowerUp = kPwUpCrystalBall;
            break;
        case kPackBeastVision:
            bActivate = 1;
            nPowerUp = kPwUpBeastVision;
            break;
        case kPackJumpBoots:
            bActivate = 1;
            nPowerUp = kPwUpJumpBoots;
            break;
        default:
            ThrowError("Unhandled pack item %d", nPack);
            return;
        }
    }
    pPlayer->packItemTime = 0;
    if (bActivate)
        powerupSetState(pPlayer, nPowerUp, pPlayer->packSlots[nPack].isActive);
}

void packPrevItem(PLAYER *pPlayer)
{
    if (pPlayer->packItemTime > 0)
    {
        for (int nPrev = ClipLow(pPlayer->packItemId-1,kPackBase); nPrev >= kPackBase; nPrev--)
        {
            if (pPlayer->packSlots[nPrev].curAmount)
            {
                pPlayer->packItemId = nPrev;
                break;
            }
        }
    }
    pPlayer->packItemTime = 600;
}

void packNextItem(PLAYER *pPlayer)
{
    if (pPlayer->packItemTime > 0)
    {
        for (int nNext = ClipHigh(pPlayer->packItemId+1,kPackMax); nNext < kPackMax; nNext++)
        {
            if (pPlayer->packSlots[nNext].curAmount)
            {
                pPlayer->packItemId = nNext;
                break;
            }
        }
    }
    pPlayer->packItemTime = 600;
}

char playerSeqPlaying(PLAYER * pPlayer, int nSeq)
{
    int nCurSeq = seqGetID(3, pPlayer->pSprite->extra);
    if (pPlayer->pDudeInfo->seqStartID+nSeq == nCurSeq && seqGetStatus(3,pPlayer->pSprite->extra) >= 0)
        return 1;
    return 0;
}

void playerSetRace(PLAYER *pPlayer, int nLifeMode)
{
    dassert(nLifeMode >= kModeHuman && nLifeMode <= kModeHumanGrown);
    DUDEINFO *pDudeInfo = pPlayer->pDudeInfo;
    *pDudeInfo = gPlayerTemplate[nLifeMode];
    pPlayer->lifeMode = nLifeMode;
    
    // By NoOne: don't forget to change clipdist for grow and shrink modes
    pPlayer->pSprite->clipdist = pDudeInfo->clipdist;
    
    for (int i = 0; i < kDamageMax; i++)
        pDudeInfo->curDamage[i] = mulscale8(Handicap[gProfile[pPlayer->nPlayer].skill], pDudeInfo->startDamage[i]);
}

void playerSetGodMode(PLAYER *pPlayer, char bGodMode)
{
    if (bGodMode)
    {
        for (int i = 0; i < kDamageMax; i++)
            pPlayer->damageControl[i]++;
    }
    else
    {
        for (int i = 0; i < kDamageMax; i++)
            pPlayer->damageControl[i]--;
    }
    pPlayer->godMode = bGodMode;
}

void playerResetInertia(PLAYER *pPlayer)
{
    POSTURE *pPosture = &pPlayer->pPosture[pPlayer->lifeMode][pPlayer->posture];
    pPlayer->zView = pPlayer->pSprite->z-pPosture->eyeAboveZ;
    pPlayer->zWeapon = pPlayer->pSprite->z-pPosture->weaponAboveZ;
    viewBackupView(pPlayer->nPlayer);
}

void playerCorrectInertia(PLAYER* pPlayer, vec3_t const *oldpos)
{
    pPlayer->zView += pPlayer->pSprite->z-oldpos->z;
    pPlayer->zWeapon += pPlayer->pSprite->z-oldpos->z;
    viewCorrectViewOffsets(pPlayer->nPlayer, oldpos);
}

void playerResetPowerUps(PLAYER* pPlayer)
{
    for (int i = 0; i < kMaxPowerUps; i++) {
        if (!VanillaMode() && (i == kPwUpJumpBoots || i == kPwUpDivingSuit || i == kPwUpCrystalBall || i == kPwUpBeastVision))
            continue;
        pPlayer->pwUpTime[i] = 0;
    }
    if (!VanillaMode() && (pPlayer == gView)) // reset delirium tilt view variables
    {
        gScreenTiltO = gScreenTilt = 0;
        deliriumTurnO = deliriumTurn = 0;
        deliriumPitchO = deliriumPitch = 0;
    }
}

void playerResetPosture(PLAYER* pPlayer) {
    memcpy(pPlayer->pPosture, gPostureDefaults, sizeof(gPostureDefaults));
    if (!VanillaMode()) {
        pPlayer->bobPhase = 0;
        pPlayer->bobAmp = 0;
        pPlayer->swayAmp = 0;
        pPlayer->bobHeight = 0;
        pPlayer->bobWidth = 0;
        pPlayer->swayHeight = 0;
        pPlayer->swayWidth = 0;
    }
    if (pPlayer == gMe) // only reset crouch toggle state if resetting our own posture
        gCrouchToggleState = 0; // reset crouch toggle state
}

void playerStart(int nPlayer, int bNewLevel)
{
    PLAYER* pPlayer = &gPlayer[nPlayer];
    GINPUT* pInput = &pPlayer->input;
    ZONE* pStartZone = NULL;

    // normal start position
    if (gGameOptions.nGameType <= kGameTypeCoop)
        pStartZone = &gStartZone[nPlayer];

    #ifdef NOONE_EXTENSIONS
    // let's check if there is positions of teams is specified
    // if no, pick position randomly, just like it works in vanilla.
    else if (gModernMap && gGameOptions.nGameType == kGameTypeTeams && gTeamsSpawnUsed == true) {
        int maxRetries = 5;
        while (maxRetries-- > 0) {
            if (pPlayer->teamId == 0) pStartZone = &gStartZoneTeam1[Random(3)];
            else pStartZone = &gStartZoneTeam2[Random(3)];

            if (maxRetries != 0) {
                // check if there is no spawned player in selected zone
                for (int i = headspritesect[pStartZone->sectnum]; i >= 0; i = nextspritesect[i]) {
                    spritetype* pSprite = &sprite[i];
                    if (pStartZone->x == pSprite->x && pStartZone->y == pSprite->y && IsPlayerSprite(pSprite)) {
                        pStartZone = NULL;
                        break;
                    }
                }
            }

            if (pStartZone != NULL)
                break;
        }
    
    }
    #endif
    else {
        pStartZone = &gStartZone[Random(8)];
    }

    if (!VanillaMode())
        sfxKillSpriteSounds(pPlayer->pSprite);

    spritetype *pSprite = actSpawnSprite(pStartZone->sectnum, pStartZone->x, pStartZone->y, pStartZone->z, 6, 1);
    dassert(pSprite->extra > 0 && pSprite->extra < kMaxXSprites);
    XSPRITE *pXSprite = &xsprite[pSprite->extra];
    pPlayer->pSprite = pSprite;
    pPlayer->pXSprite = pXSprite;
    pPlayer->nSprite = pSprite->index;
    DUDEINFO *pDudeInfo = &dudeInfo[kDudePlayer1 + nPlayer - kDudeBase];
    pPlayer->pDudeInfo = pDudeInfo;
    playerSetRace(pPlayer, kModeHuman);
    playerResetPosture(pPlayer);
    seqSpawn(pDudeInfo->seqStartID, 3, pSprite->extra, -1);
    if (pPlayer == gMe)
        SetBitString(show2dsprite, pSprite->index);
    int top, bottom;
    GetSpriteExtents(pSprite, &top, &bottom);
    pSprite->z -= bottom - pSprite->z;
    pSprite->pal = 11+(pPlayer->teamId&3);
    pPlayer->angold = pSprite->ang = pStartZone->ang;
    pPlayer->q16ang = fix16_from_int(pSprite->ang);
    pSprite->type = kDudePlayer1+nPlayer;
    pSprite->clipdist = pDudeInfo->clipdist;
    pSprite->flags = 15;
    pXSprite->burnTime = 0;
    pXSprite->burnSource = -1;
    pPlayer->pXSprite->health = pDudeInfo->startHealth<<4;
    pPlayer->pSprite->cstat &= (unsigned short)~32768;
    pPlayer->bloodlust = 0;
    pPlayer->q16horiz = 0;
    pPlayer->q16slopehoriz = 0;
    pPlayer->q16look = 0;
    pPlayer->slope = 0;
    pPlayer->fraggerId = -1;
    pPlayer->underwaterTime = 1200;
    pPlayer->bloodTime = 0;
    pPlayer->gooTime = 0;
    pPlayer->wetTime = 0;
    pPlayer->bubbleTime = 0;
    pPlayer->at306 = 0;
    pPlayer->restTime = 0;
    pPlayer->kickPower = 0;
    pPlayer->laughCount = 0;
    pPlayer->spin = 0;
    pPlayer->posture = kPostureStand;
    pPlayer->voodooTarget = -1;
    pPlayer->voodooTargets = 0;
    pPlayer->voodooVar1 = 0;
    pPlayer->vodooVar2 = 0;
    playerResetInertia(pPlayer);
    pPlayer->zWeaponVel = 0;
    pPlayer->relAim.dx = 0x4000;
    pPlayer->relAim.dy = 0;
    pPlayer->relAim.dz = 0;
    pPlayer->aimTarget = -1;
    pPlayer->zViewVel = pPlayer->zWeaponVel;
    if (!(gGameOptions.nGameType == kGameTypeCoop && gGameOptions.bPlayerKeys > PLAYERKEYSMODE::LOSTONDEATH && !bNewLevel))
    {
        for (int i = 0; i < 8; i++)
            pPlayer->hasKey[i] = gGameOptions.nGameType >= kGameTypeBloodBath;
    }
    pPlayer->hasFlag = 0;
    for (int i = 0; i < 8; i++)
        pPlayer->used2[i] = -1;
    for (int i = 0; i < kDamageMax; i++)
        pPlayer->damageControl[i] = 0;
    if (pPlayer->godMode)
        playerSetGodMode(pPlayer, 1);
    gInfiniteAmmo = 0;
    gFullMap = 0;
    pPlayer->throwPower = 0;
    pPlayer->deathTime = 0;
    pPlayer->nextWeapon = kWeaponNone;
    xvel[pSprite->index] = yvel[pSprite->index] = zvel[pSprite->index] = 0;
    pInput->q16turn = 0;
    pInput->keyFlags.word = 0;
    pInput->forward = 0;
    pInput->strafe = 0;
    pInput->q16mlook = 0;
    pInput->buttonFlags.byte = 0;
    pInput->useFlags.byte = 0;
    pPlayer->flickerEffect = 0;
    pPlayer->quakeEffect = 0;
    pPlayer->tiltEffect = 0;
    pPlayer->visibility = 0;
    pPlayer->painEffect = 0;
    pPlayer->blindEffect = 0;
    pPlayer->chokeEffect = 0;
    pPlayer->handTime = 0;
    pPlayer->weaponTimer = 0;
    pPlayer->weaponState = 0;
    pPlayer->weaponQav = -1;
    #ifdef NOONE_EXTENSIONS
    playerQavSceneReset(pPlayer); // reset qav scene
    
    // assign or update player's sprite index for conditions
    if (gModernMap)
    {
        for (int nSprite = headspritestat[kStatModernPlayerLinker]; nSprite >= 0; nSprite = nextspritestat[nSprite])
        {
            XSPRITE* pXCtrl = &xsprite[sprite[nSprite].extra];
            if (!pXCtrl->data1 || pXCtrl->data1 == pPlayer->nPlayer + 1)
            {
                int nSpriteOld = pXCtrl->sysData1;
                trPlayerCtrlLink(pXCtrl, pPlayer, (nSpriteOld < 0) ? true : false);
                if (nSpriteOld > 0)
                    conditionsUpdateIndex(OBJ_SPRITE, nSpriteOld, pXCtrl->sysData1);
            }
        }
    }

    #endif
    pPlayer->hand = 0;
    pPlayer->nWaterPal = 0;
    playerResetPowerUps(pPlayer);

    if (pPlayer == gMe)
    {
        viewInitializePrediction();
        gViewLook = pPlayer->q16look;
        gViewAngle = pPlayer->q16ang;
        gViewMap.x = pPlayer->pSprite->x;
        gViewMap.y = pPlayer->pSprite->y;
        gViewMap.angle = pPlayer->pSprite->ang;
        if (!VanillaMode())
            sfxResetListener(); // player is listener, update ear position/reset ear velocity so audio pitch of surrounding sfx does not freak out when respawning player
    }
    if (IsUnderwaterSector(pSprite->sectnum))
    {
        pPlayer->posture = kPostureSwim;
        pPlayer->pXSprite->medium = kMediumWater;
    }
}

void playerReset(PLAYER *pPlayer)
{
    static int dword_136400[] = {
        3, 4, 2, 8, 9, 10, 7, 1, 1, 1, 1, 1, 1, 1
    };
    static int dword_136438[] = {
        3, 4, 2, 8, 9, 10, 7, 1, 1, 1, 1, 1, 1, 1
    };
    dassert(pPlayer != NULL);
    for (int i = 0; i < kWeaponMax; i++)
    {
        pPlayer->hasWeapon[i] = gInfiniteAmmo;
        pPlayer->weaponMode[i] = 0;
    }
    pPlayer->hasWeapon[kWeaponPitchfork] = 1;
    pPlayer->curWeapon = kWeaponNone;
    pPlayer->qavCallback = -1;
    pPlayer->input.newWeapon = kWeaponPitchfork;
    for (int i = 0; i < kWeaponMax; i++)
    {
        pPlayer->weaponOrder[0][i] = dword_136400[i];
        pPlayer->weaponOrder[1][i] = dword_136438[i];
    }
    for (int i = 0; i < 12; i++)
    {
        if (gInfiniteAmmo)
            pPlayer->ammoCount[i] = gAmmoInfo[i].max;
        else
            pPlayer->ammoCount[i] = 0;
    }
    for (int i = 0; i < 3; i++)
        pPlayer->armor[i] = 0;
    pPlayer->weaponTimer = 0;
    pPlayer->weaponState = 0;
    pPlayer->weaponQav = -1;
    pPlayer->qavLoop = 0;
    pPlayer->packItemId = -1;

    for (int i = 0; i < kPackMax; i++) {
        pPlayer->packSlots[i].isActive = 0;
        pPlayer->packSlots[i].curAmount = 0;
    }
    #ifdef NOONE_EXTENSIONS
    playerQavSceneReset(pPlayer);
    #endif
    // reset posture (mainly required for resetting movement speed and jump height)
    playerResetPosture(pPlayer);
}

void playerResetScores(int nPlayer)
{
    PLAYER *pPlayer = &gPlayer[nPlayer];
    pPlayer->fragCount = 0;
    memset(pPlayer->fragInfo, 0, sizeof(pPlayer->fragInfo));
    memset(gPlayerScores, 0, sizeof(gPlayerScores));
    memset((void *)gPlayerScoreTicks, 0, sizeof(gPlayerScoreTicks));
}

void playerInit(int nPlayer, unsigned int a2)
{
    PLAYER *pPlayer = &gPlayer[nPlayer];
    if (!(a2&1))
        memset((void *)pPlayer, 0, sizeof(PLAYER));
    pPlayer->nPlayer = nPlayer;
    pPlayer->teamId = nPlayer;
    if (gGameOptions.nGameType == kGameTypeTeams)
        pPlayer->teamId = nPlayer&1;
    playerResetScores(nPlayer);

    if (!(a2&1))
        playerReset(pPlayer);
}

char findDroppedLeech(PLAYER *a1, spritetype *a2)
{
    for (int nSprite = headspritestat[kStatThing]; nSprite >= 0; nSprite = nextspritestat[nSprite])
    {
        if (a2 && a2->index == nSprite)
            continue;
        spritetype *pSprite = &sprite[nSprite];
        if (pSprite->type == kThingDroppedLifeLeech && actOwnerIdToSpriteId(pSprite->owner) == a1->nSprite)
            return 1;
    }
    return 0;
}

char PickupItem(PLAYER *pPlayer, spritetype *pItem) {
    
    spritetype *pSprite = pPlayer->pSprite; XSPRITE *pXSprite = pPlayer->pXSprite;
    char buffer[80]; int pickupSnd = 775; int nType = pItem->type - kItemBase;

    switch (pItem->type) {
        case kItemShadowCloak:
            #ifdef NOONE_EXTENSIONS
            if (isGrown(pPlayer->pSprite) || !powerupActivate(pPlayer, nType)) return false;
            #else
            if (!powerupActivate(pPlayer, nType)) return false;
            #endif
            break;
        #ifdef NOONE_EXTENSIONS
        case kItemShroomShrink:
        case kItemShroomGrow:
            
            if (gModernMap) {
                switch (pItem->type) {
                    case kItemShroomShrink:
                        if (isShrinked(pSprite)) return false;
                        break;
                    case kItemShroomGrow:
                        if (isGrown(pSprite)) return false;
                        break;
                }

                powerupActivate(pPlayer, nType);
            }
            
            break;
        #endif
        case kItemFlagABase:
        case kItemFlagBBase: {
            if (gGameOptions.nGameType != kGameTypeTeams || pItem->extra <= 0) return 0;
            XSPRITE * pXItem = &xsprite[pItem->extra];
            if (pItem->type == kItemFlagABase) {
                if (pPlayer->teamId == 1) {
                    if ((pPlayer->hasFlag & 1) == 0 && pXItem->state) {
                        pPlayer->hasFlag |= 1;
                        pPlayer->used2[0] = pItem->index;
                        trTriggerSprite(pItem->index, pXItem, kCmdOff, pPlayer->nSprite);
                        sprintf(buffer, "%s stole Blue Flag", gProfile[pPlayer->nPlayer].name);
                        sndStartSample(8007, 255, 2, 0);
                        viewSetMessage(buffer);
                    }
                }

                if (pPlayer->teamId == 0) {

                    if ((pPlayer->hasFlag & 1) != 0 && !pXItem->state) {
                        pPlayer->hasFlag &= ~1;
                        pPlayer->used2[0] = -1;
                        trTriggerSprite(pItem->index, pXItem, kCmdOn, pPlayer->nSprite);
                        sprintf(buffer, "%s returned Blue Flag", gProfile[pPlayer->nPlayer].name);
                        sndStartSample(8003, 255, 2, 0);
                        viewSetMessage(buffer);
                    }

                    if ((pPlayer->hasFlag & 2) != 0 && pXItem->state) {
                        pPlayer->hasFlag &= ~2;
                        pPlayer->used2[1] = -1;
                        gPlayerScores[pPlayer->teamId] += 10;
                        gPlayerScoreTicks[pPlayer->teamId] += 240;
                        evSend(0, 0, 81, kCmdOn, pPlayer->nSprite);
                        sprintf(buffer, "%s captured Red Flag!", gProfile[pPlayer->nPlayer].name);
                        sndStartSample(8001, 255, 2, 0);
                        viewSetMessage(buffer);
#if 0
                        if (dword_28E3D4 == 3 && myconnectindex == connecthead)
                        {
                            sprintf(buffer, "frag A killed B\n");
                            netBroadcastFrag(buffer);
                        }
#endif
                    }
                }

            }
            else if (pItem->type == kItemFlagBBase) {

                if (pPlayer->teamId == 0) {
                    if ((pPlayer->hasFlag & 2) == 0 && pXItem->state) {
                        pPlayer->hasFlag |= 2;
                        pPlayer->used2[1] = pItem->index;
                        trTriggerSprite(pItem->index, pXItem, kCmdOff, pPlayer->nSprite);
                        sprintf(buffer, "%s stole Red Flag", gProfile[pPlayer->nPlayer].name);
                        sndStartSample(8006, 255, 2, 0);
                        viewSetMessage(buffer);
                    }
                }

                if (pPlayer->teamId == 1) {
                    if ((pPlayer->hasFlag & 2) != 0 && !pXItem->state)
                    {
                        pPlayer->hasFlag &= ~2;
                        pPlayer->used2[1] = -1;
                        trTriggerSprite(pItem->index, pXItem, kCmdOn, pPlayer->nSprite);
                        sprintf(buffer, "%s returned Red Flag", gProfile[pPlayer->nPlayer].name);
                        sndStartSample(8002, 255, 2, 0);
                        viewSetMessage(buffer);
                    }
                    if ((pPlayer->hasFlag & 1) != 0 && pXItem->state)
                    {
                        pPlayer->hasFlag &= ~1;
                        pPlayer->used2[0] = -1;
                        gPlayerScores[pPlayer->teamId] += 10;
                        gPlayerScoreTicks[pPlayer->teamId] += 240;
                        evSend(0, 0, 80, kCmdOn, pPlayer->nSprite);
                        sprintf(buffer, "%s captured Blue Flag!", gProfile[pPlayer->nPlayer].name);
                        sndStartSample(8000, 255, 2, 0);
                        viewSetMessage(buffer);
#if 0
                        if (dword_28E3D4 == 3 && myconnectindex == connecthead)
                        {
                            sprintf(buffer, "frag B killed A\n");
                            netBroadcastFrag(buffer);
                        }
#endif
                    }
                }
            }
        }
        return 0;
        case kItemFlagA: {
            if (gGameOptions.nGameType != kGameTypeTeams) return 0;
            evKill(pItem->index, 3, kCallbackReturnFlag);
            gBlueFlagDropped = false;
            pPlayer->hasFlag |= 1;
            pPlayer->used2[0] = pItem->owner;
            const bool enemyTeam = (pPlayer->teamId&1) == 1;
            if (enemyTeam)
            {
                sprintf(buffer, "%s stole Blue Flag", gProfile[pPlayer->nPlayer].name);
                sndStartSample(8007, 255, 2, 0);
                viewSetMessage(buffer);
            }
            break;
        }
        case kItemFlagB: {
            if (gGameOptions.nGameType != kGameTypeTeams) return 0;
            evKill(pItem->index, 3, kCallbackReturnFlag);
            gRedFlagDropped = false;
            pPlayer->hasFlag |= 2;
            pPlayer->used2[1] = pItem->owner;
            const bool enemyTeam = (pPlayer->teamId&1) == 0;
            if (enemyTeam)
            {
                sprintf(buffer, "%s stole Red Flag", gProfile[pPlayer->nPlayer].name);
                sndStartSample(8006, 255, 2, 0);
                viewSetMessage(buffer);
            }
            break;
        }
        case kItemArmorBasic:
        case kItemArmorBody:
        case kItemArmorFire:
        case kItemArmorSpirit:
        case kItemArmorSuper: {
            ARMORDATA *pArmorData = &armorData[pItem->type - kItemArmorBasic]; bool pickedUp = false;
            if (pPlayer->armor[1] < pArmorData->atc) {
                pPlayer->armor[1] = ClipHigh(pPlayer->armor[1]+pArmorData->at8, pArmorData->atc);
                pickedUp = true;
            }
        
            if (pPlayer->armor[0] < pArmorData->at4) {
                pPlayer->armor[0] = ClipHigh(pPlayer->armor[0]+pArmorData->at0, pArmorData->at4);
                pickedUp = true;
            }

            if (pPlayer->armor[2] < pArmorData->at14) {
                pPlayer->armor[2] = ClipHigh(pPlayer->armor[2]+pArmorData->at10, pArmorData->at14);
                pickedUp = true;
            }
        
            if (!pickedUp) return 0;
            pickupSnd = 779;
            break;
        }
        case kItemCrystalBall:
            if (gGameOptions.nGameType == kGameTypeSinglePlayer || !packAddItem(pPlayer, gItemData[nType].packSlot)) return 0;
            break;
        case kItemKeySkull:
        case kItemKeyEye:
        case kItemKeyFire:
        case kItemKeyDagger:
        case kItemKeySpider:
        case kItemKeyMoon:
        case kItemKeyKey7:
            if (pPlayer->hasKey[pItem->type-99]) return 0;
            pPlayer->hasKey[pItem->type-99] = 1;
            pickupSnd = 781;
            if (gGameOptions.bPlayerKeys == PLAYERKEYSMODE::SHARED) {
                for (int i = connecthead; i != -1; i = connectpoint2[i]) {
                    gPlayer[i].hasKey[pItem->type-99] = 1;
                }
            }
            break;
        case kItemHealthMedPouch:
        case kItemHealthLifeEssense:
        case kItemHealthLifeSeed:
        case kItemHealthRedPotion:  {
            int addPower = gPowerUpInfo[nType].bonusTime;
            #ifdef NOONE_EXTENSIONS
            // allow custom amount for item
            if (gModernMap && sprite[pItem->index].extra >= 0 && xsprite[sprite[pItem->index].extra].data1 > 0)
                addPower = xsprite[sprite[pItem->index].extra].data1;
            #endif
        
            if (!actHealDude(pXSprite, addPower, gPowerUpInfo[nType].maxTime)) return 0;
            return 1;
        }
        case kItemHealthDoctorBag:
        case kItemJumpBoots:
        case kItemDivingSuit:
        case kItemBeastVision:
            if (!packAddItem(pPlayer, gItemData[nType].packSlot)) return 0;
            break;
        default:
            if (!powerupActivate(pPlayer, nType)) return 0;
            return 1;
    }
    
    sfxPlay3DSound(pSprite->x, pSprite->y, pSprite->z, pickupSnd, pSprite->sectnum);
    return 1;
}

char PickupAmmo(PLAYER* pPlayer, spritetype* pAmmo) {
    AMMOITEMDATA* pAmmoItemData = &gAmmoItemData[pAmmo->type - kItemAmmoBase];
    int nAmmoType = pAmmoItemData->type;

    if (pPlayer->ammoCount[nAmmoType] >= gAmmoInfo[nAmmoType].max) return 0;
    #ifdef NOONE_EXTENSIONS
    else if (gModernMap && pAmmo->extra >= 0 && xsprite[pAmmo->extra].data1 > 0) // allow custom amount for item
        pPlayer->ammoCount[nAmmoType] = ClipHigh(pPlayer->ammoCount[nAmmoType] + xsprite[pAmmo->extra].data1, gAmmoInfo[nAmmoType].max);
    #endif
    else
        pPlayer->ammoCount[nAmmoType] = ClipHigh(pPlayer->ammoCount[nAmmoType]+pAmmoItemData->count, gAmmoInfo[nAmmoType].max);

    if (pAmmoItemData->weaponType)  pPlayer->hasWeapon[pAmmoItemData->weaponType] = 1;
    sfxPlay3DSound(pPlayer->pSprite, 782, -1, 0);
    return 1;
}

char PickupWeapon(PLAYER *pPlayer, spritetype *pWeapon) {
    WEAPONITEMDATA *pWeaponItemData = &gWeaponItemData[pWeapon->type - kItemWeaponBase];
    int nWeaponType = pWeaponItemData->type;
    int nAmmoType = pWeaponItemData->ammoType;
    if (!pPlayer->hasWeapon[nWeaponType] || gGameOptions.nWeaponSettings == 2 || gGameOptions.nWeaponSettings == 3) {
        if ((pWeapon->type == kItemWeaponLifeLeech) && (gGameOptions.nGameType >= kGameTypeBloodBath) && findDroppedLeech(pPlayer, NULL))
            return 0;
        pPlayer->hasWeapon[nWeaponType] = 1;
        if (nAmmoType == -1) return 0;
        // allow to set custom ammo count for weapon pickups
        #ifdef NOONE_EXTENSIONS
        else if (gModernMap && pWeapon->extra >= 0 && xsprite[pWeapon->extra].data1 > 0)
            pPlayer->ammoCount[nAmmoType] = ClipHigh(pPlayer->ammoCount[nAmmoType] + xsprite[pWeapon->extra].data1, gAmmoInfo[nAmmoType].max);
        #endif
        else
            pPlayer->ammoCount[nAmmoType] = ClipHigh(pPlayer->ammoCount[nAmmoType] + pWeaponItemData->count, gAmmoInfo[nAmmoType].max);

        int nNewWeapon = WeaponUpgrade(pPlayer, nWeaponType);
        if (nNewWeapon != pPlayer->curWeapon) {
            pPlayer->weaponState = 0;
            pPlayer->nextWeapon = nNewWeapon;
        }
        sfxPlay3DSound(pPlayer->pSprite, 777, -1, 0);
        return 1;
    }
    
    if (!actGetRespawnTime(pWeapon) || nAmmoType == -1 || pPlayer->ammoCount[nAmmoType] >= gAmmoInfo[nAmmoType].max) return 0;    
    #ifdef NOONE_EXTENSIONS
        else if (gModernMap && pWeapon->extra >= 0 && xsprite[pWeapon->extra].data1 > 0)
            pPlayer->ammoCount[nAmmoType] = ClipHigh(pPlayer->ammoCount[nAmmoType] + xsprite[pWeapon->extra].data1, gAmmoInfo[nAmmoType].max);
    #endif
    else
        pPlayer->ammoCount[nAmmoType] = ClipHigh(pPlayer->ammoCount[nAmmoType]+pWeaponItemData->count, gAmmoInfo[nAmmoType].max);

    sfxPlay3DSound(pPlayer->pSprite, 777, -1, 0);
    return 1;
}

void PickUp(PLAYER *pPlayer, spritetype *pSprite)
{
    char buffer[256], pickedUp = 0, showMsg = 1, showEff = 1;
    int nType = pSprite->type, customMsg = -1;

#ifdef NOONE_EXTENSIONS
    if (gModernMap) // allow custom INI message instead "Picked up"
    {
        XSPRITE* pXSprite = (pSprite->extra >= 0) ? &xsprite[pSprite->extra] : NULL;
        if (pXSprite != NULL && pXSprite->txID != 3 && pXSprite->lockMsg > 0)
            customMsg = pXSprite->lockMsg;
    }

    if (IsUserItem(nType))
    {
        ITEM* pItem = userItemGet(nType);
        if (pItem && (pickedUp = userItemPickup(pPlayer, pSprite, pItem)) != 0)
        {
            if (customMsg < 0)
            {
                showMsg = !(pItem->flags & kFlagItemNoMessage);

                if (showMsg)
                {
                    if (!pItem->message)                            sprintf(buffer, "Picked up %s", pItem->name);
                    else                                            strcpy(buffer, pItem->message);
                }
            }

            showEff = (!(pItem->flags & kFlagItemNoEffect) && !pItem->numeffects);
        }
    }
    else
#endif
    if (nType >= kItemBase && nType <= kItemMax) {
        pickedUp = PickupItem(pPlayer, pSprite);
        if (pickedUp && customMsg == -1) sprintf(buffer, "Picked up %s", gItemText[nType - kItemBase]);
    
    } else if (nType >= kItemAmmoBase && nType < kItemAmmoMax) {
        pickedUp = PickupAmmo(pPlayer, pSprite);
        if (pickedUp && customMsg == -1) sprintf(buffer, "Picked up %s", gAmmoText[nType - kItemAmmoBase]);
    
    } else if (nType >= kItemWeaponBase && nType < kItemWeaponMax) {
        pickedUp = PickupWeapon(pPlayer, pSprite);
        if (pickedUp && customMsg == -1) sprintf(buffer, "Picked up %s", gWeaponText[nType - kItemWeaponBase]);
    }

    if (!pickedUp) return;
    else if (pSprite->extra > 0) {
        XSPRITE *pXSprite = &xsprite[pSprite->extra];
        if (pXSprite->Pickup)
            trTriggerSprite(pSprite->index, pXSprite, kCmdSpritePickup, pPlayer->nSprite);
    }
        
    if (!actCheckRespawn(pSprite)) 
        actPostSprite(pSprite->index, kStatFree);

    if (showEff)
        pPlayer->pickupEffect = 30;

    if (pPlayer == gMe && showMsg)
    {
        if (customMsg > 0) trTextOver(customMsg - 1);
        else viewSetMessage(buffer, 0, MESSAGE_PRIORITY_PICKUP);
    }
}

void CheckPickUp(PLAYER *pPlayer)
{
    spritetype *pSprite = pPlayer->pSprite;
    int x = pSprite->x;
    int y = pSprite->y;
    int z = pSprite->z;
    int nSector = pSprite->sectnum;
    int nNextSprite;
    for (int nSprite = headspritestat[kStatItem]; nSprite >= 0; nSprite = nNextSprite) {
        spritetype *pItem = &sprite[nSprite];
        nNextSprite = nextspritestat[nSprite];
        if (pItem->flags&32)
            continue;
        int dx = klabs(x-pItem->x)>>4;
        if (dx > 48)
            continue;
        int dy = klabs(y-pItem->y)>>4;
        if (dy > 48)
            continue;
        int top, bottom;
        GetSpriteExtents(pSprite, &top, &bottom);
        int vb = 0;
        if (pItem->z < top)
            vb = (top-pItem->z)>>8;
        else if (pItem->z > bottom)
            vb = (pItem->z-bottom)>>8;
        if (vb > 32)
            continue;
        if (approxDist(dx,dy) > 48)
            continue;
        GetSpriteExtents(pItem, &top, &bottom);
        if (cansee(x, y, z, nSector, pItem->x, pItem->y, pItem->z, pItem->sectnum)
         || cansee(x, y, z, nSector, pItem->x, pItem->y, top, pItem->sectnum)
         || cansee(x, y, z, nSector, pItem->x, pItem->y, bottom, pItem->sectnum))
            PickUp(pPlayer, pItem);
    }
}

int ActionScan(PLAYER *pPlayer, int *a2, int *a3)
{
    *a2 = 0;
    *a3 = 0;
    spritetype *pSprite = pPlayer->pSprite;
    int x = Cos(pSprite->ang)>>16;
    int y = Sin(pSprite->ang)>>16;
    int z = pPlayer->slope;
    int hit = HitScan(pSprite, pPlayer->zView, x, y, z, 0x10000040, 128);
    int hitDist = approxDist(pSprite->x-gHitInfo.hitx, pSprite->y-gHitInfo.hity)>>4;
    if (hitDist < 64)
    {
        switch (hit)
        {
        case 3:
            *a2 = gHitInfo.hitsprite;
            *a3 = sprite[*a2].extra;
            if (*a3 > 0 && sprite[*a2].statnum == kStatThing)
            {
                spritetype *pSprite = &sprite[*a2];
                XSPRITE *pXSprite = &xsprite[*a3];
                if (pSprite->type == kThingDroppedLifeLeech)
                {
                    if ((gGameOptions.nGameType >= kGameTypeBloodBath) && findDroppedLeech(pPlayer, pSprite))
                        return -1;
                    pXSprite->data4 = pPlayer->nPlayer;
                    pXSprite->isTriggered = 0;
                }
            }
            if (*a3 > 0 && xsprite[*a3].Push)
                return 3;
            if (sprite[*a2].statnum == kStatDude)
            {
                spritetype *pSprite = &sprite[*a2];
                XSPRITE *pXSprite = &xsprite[*a3];
                int nMass = getDudeInfo(pSprite->type)->mass;
                if (nMass)
                {
                    int t2 = divscale8(0xccccc, nMass);
                    xvel[*a2] += mulscale16(x, t2);
                    yvel[*a2] += mulscale16(y, t2);
                    zvel[*a2] += mulscale16(z, t2);
                }
                if (pXSprite->Push && !pXSprite->state && !pXSprite->isTriggered)
                    trTriggerSprite(*a2, pXSprite, kCmdSpritePush, pPlayer->nSprite);
            }
            break;
        case 0:
        case 4:
            *a2 = gHitInfo.hitwall;
            *a3 = wall[*a2].extra;
            if (*a3 > 0 && xwall[*a3].triggerPush)
                return 0;
            if (wall[*a2].nextsector >= 0)
            {
                *a2 = wall[*a2].nextsector;
                *a3 = sector[*a2].extra;
                if (*a3 > 0 && xsector[*a3].Wallpush)
                    return 6;
            }
            break;
        case 1:
        case 2:
            *a2 = gHitInfo.hitsect;
            *a3 = sector[*a2].extra;
            if (*a3 > 0 && xsector[*a3].Push)
                return 6;
            break;
        }
    }
    *a2 = pSprite->sectnum;
    *a3 = sector[*a2].extra;
    if (*a3 > 0 && xsector[*a3].Push)
        return 6;
    return -1;
}

inline void playerDropHand(PLAYER *pPlayer)
{
    spritetype *pSprite2 = actSpawnDude(pPlayer->pSprite, kDudeHand, pPlayer->pSprite->clipdist<<1, 0);
    pSprite2->ang = (pPlayer->pSprite->ang+1024)&2047;
    int nSprite = pPlayer->pSprite->index;
    int x = Cos(pPlayer->pSprite->ang)>>16;
    int y = Sin(pPlayer->pSprite->ang)>>16;
    xvel[pSprite2->index] = xvel[nSprite] + mulscale14(0x155555, x);
    yvel[pSprite2->index] = yvel[nSprite] + mulscale14(0x155555, y);
    zvel[pSprite2->index] = zvel[nSprite];
    pPlayer->hand = 0;
}

void ProcessInput(PLAYER *pPlayer)
{
    spritetype *pSprite = pPlayer->pSprite;
    XSPRITE *pXSprite = pPlayer->pXSprite;
    int nSprite = pPlayer->nSprite;
    POSTURE *pPosture = &pPlayer->pPosture[pPlayer->lifeMode][pPlayer->posture];
    GINPUT *pInput = &pPlayer->input;

    if (pPlayer == gMe && numplayers == 1)
    {
        gViewAngleAdjust = 0.f;
        gViewLookRecenter = false;
        gViewLookAdjust = 0.f;
    }

    pPlayer->isRunning = pInput->syncFlags.run;
    if (pInput->buttonFlags.byte || pInput->forward || pInput->strafe || pInput->q16turn)
        pPlayer->restTime = 0;
    else if (pPlayer->restTime >= 0)
        pPlayer->restTime += kTicsPerFrame;
    if (VanillaMode() || pXSprite->health == 0)
        WeaponProcess(pPlayer);
    if (pXSprite->health == 0)
    {
        char bSeqStat = playerSeqPlaying(pPlayer, 16);
        if (pPlayer->fraggerId != -1)
        {
            pPlayer->angold = pSprite->ang = getangle(sprite[pPlayer->fraggerId].x - pSprite->x, sprite[pPlayer->fraggerId].y - pSprite->y);
            pPlayer->q16ang = fix16_from_int(pSprite->ang);
        }
        pPlayer->deathTime += kTicsPerFrame;
        if (!bSeqStat)
        {
            if (VanillaMode())
                pPlayer->q16horiz = fix16_from_int(mulscale16(0x8000-(Cos(ClipHigh(pPlayer->deathTime*8, 1024))>>15), 120));
            else
                pPlayer->q16horiz = mulscale16(0x8000-(Cos(ClipHigh(pPlayer->deathTime*8, 1024))>>15), F16(120));
        }
        if (pPlayer->curWeapon)
            pInput->newWeapon = pPlayer->curWeapon;
        if (pInput->keyFlags.action)
        {
            if (bSeqStat)
            {
                if (pPlayer->deathTime > 360)
                    seqSpawn(pPlayer->pDudeInfo->seqStartID+14, 3, pPlayer->pSprite->extra, nPlayerSurviveClient);
            }
            else if (seqGetStatus(3, pPlayer->pSprite->extra) < 0)
            {
                if (pPlayer->pSprite)
                    pPlayer->pSprite->type = kThingBloodChunks;
                actPostSprite(pPlayer->nSprite, kStatThing);
                seqSpawn(pPlayer->pDudeInfo->seqStartID+15, 3, pPlayer->pSprite->extra, -1);
                playerReset(pPlayer);
                if (gGameOptions.nGameType == kGameTypeSinglePlayer && numplayers == 1)
                {
                    if (gDemo.at0)
                        gDemo.Close();
                    pInput->keyFlags.restart = 1;
                }
                else
                    playerStart(pPlayer->nPlayer);
            }
            pInput->keyFlags.action = 0;
        }
        return;
    }
    if (pPlayer->posture == kPostureSwim)
    {
        int x = Cos(pSprite->ang);
        int y = Sin(pSprite->ang);
        if (pInput->forward)
        {
            int forward = pInput->forward;
            if (forward > 0)
                forward = mulscale8(pPosture->frontAccel, forward);
            else
                forward = mulscale8(pPosture->backAccel, forward);
            xvel[nSprite] += mulscale30(forward, x);
            yvel[nSprite] += mulscale30(forward, y);
        }
        if (pInput->strafe)
        {
            int strafe = pInput->strafe;
            strafe = mulscale8(pPosture->sideAccel, strafe);
            xvel[nSprite] += mulscale30(strafe, y);
            yvel[nSprite] -= mulscale30(strafe, x);
        }
    }
    else if (pXSprite->height < 256)
    {
        int speed = 0x10000;
        if (pXSprite->height > 0)
            speed -= divscale16(pXSprite->height, 256);
        int x = Cos(pSprite->ang);
        int y = Sin(pSprite->ang);
        if (pInput->forward)
        {
            int forward = pInput->forward;
            if (forward > 0)
                forward = mulscale8(pPosture->frontAccel, forward);
            else
                forward = mulscale8(pPosture->backAccel, forward);
            if (pXSprite->height)
                forward = mulscale16(forward, speed);
            xvel[nSprite] += mulscale30(forward, x);
            yvel[nSprite] += mulscale30(forward, y);
        }
        if (pInput->strafe)
        {
            int strafe = pInput->strafe;
            strafe = mulscale8(pPosture->sideAccel, strafe);
            if (pXSprite->height)
                strafe = mulscale16(strafe, speed);
            xvel[nSprite] += mulscale30(strafe, y);
            yvel[nSprite] -= mulscale30(strafe, x);
        }
    }
    if (pInput->q16turn)
    {
        if (VanillaMode())
            pPlayer->q16ang = ((pPlayer->q16ang&0x7ff0000)+(pInput->q16turn&0x7ff0000))&0x7ffffff;
        else
            pPlayer->q16ang = (pPlayer->q16ang+pInput->q16turn)&0x7ffffff;
    }
    if (pInput->keyFlags.spin180)
    {
        if (!pPlayer->spin)
            pPlayer->spin = -kAng180;
        pInput->keyFlags.spin180 = 0;
    }
    if (pPlayer->spin < 0)
    {
        const int speed = (pPlayer->posture == kPostureSwim) ? 64 : 128;
        pPlayer->spin = min(pPlayer->spin+speed, 0);
        pPlayer->q16ang += fix16_from_int(speed);
        if (pPlayer == gMe && numplayers == 1)
            gViewAngleAdjust += float(ClipHigh(-pPlayer->spin, speed)); // don't overturn when nearing end of spin
    }
    if (pPlayer == gMe && numplayers == 1)
    {
        int nDeltaAngle = pSprite->ang - pPlayer->angold;
        if (nDeltaAngle > kAng180) // handle unsigned overflow
            nDeltaAngle += -kAng360;
        else if (nDeltaAngle < -kAng180)
            nDeltaAngle += kAng360;
        gViewAngleAdjust += float(nDeltaAngle);
    }
    pPlayer->q16ang = (pPlayer->q16ang+fix16_from_int(pSprite->ang-pPlayer->angold))&0x7ffffff;
    pPlayer->angold = pSprite->ang = fix16_to_int(pPlayer->q16ang);
    if (!pInput->buttonFlags.jump)
        pPlayer->cantJump = 0;

    switch (pPlayer->posture) {
    case kPostureSwim:
        if (pInput->buttonFlags.jump)
            zvel[nSprite] -= pPosture->normalJumpZ;//0x5b05;
        if (pInput->buttonFlags.crouch)
            zvel[nSprite] += pPosture->normalJumpZ;//0x5b05;
        break;
    case kPostureCrouch:
        if (!pInput->buttonFlags.crouch)
            pPlayer->posture = kPostureStand;
        break;
    default:
        if (!pPlayer->cantJump && pInput->buttonFlags.jump && pXSprite->height == 0) {
            #ifdef NOONE_EXTENSIONS
            if ((packItemActive(pPlayer, kPackJumpBoots) && pPosture->pwupJumpZ != 0) || pPosture->normalJumpZ != 0)
            #endif
                sfxPlay3DSound(pSprite, 700, 0, 0);

            if (packItemActive(pPlayer, kPackJumpBoots)) zvel[nSprite] = pPosture->pwupJumpZ; //-0x175555;
            else zvel[nSprite] = pPosture->normalJumpZ; //-0xbaaaa;
            pPlayer->cantJump = 1;
        }

        if (pInput->buttonFlags.crouch)
            pPlayer->posture = kPostureCrouch;
        break;
    }
    if (pInput->keyFlags.action)
    {
        int a2, a3;
        int hit = ActionScan(pPlayer, &a2, &a3);
        switch (hit)
        {
        case 6:
            if (a3 > 0 && a3 < kMaxXSectors)
            {
                XSECTOR *pXSector = &xsector[a3];
                int key = pXSector->Key;
                if (pXSector->locked && pPlayer == gMe)
                {
                    viewSetMessage("It's locked");
                    sndStartSample(3062, 255, 2, 0);
                }
                if (!key || pPlayer->hasKey[key])
                    trTriggerSector(a2, pXSector, kCmdSpritePush, pPlayer->nSprite);
                else if (pPlayer == gMe)
                {
                    viewSetMessage("That requires a key.");
                    sndStartSample(3063, 255, 2, 0);
                }
            }
            break;
        case 0:
        {
            XWALL *pXWall = &xwall[a3];
            int key = pXWall->key;
            if (pXWall->locked && pPlayer == gMe)
            {
                viewSetMessage("It's locked");
                sndStartSample(3062, 255, 2, 0);
            }
            if (!key || pPlayer->hasKey[key])
            {
                trTriggerWall(a2, pXWall, kCmdWallPush, pPlayer->nSprite);
            }
            else if (pPlayer == gMe)
            {
                viewSetMessage("That requires a key.");
                sndStartSample(3063, 255, 2, 0);
            }
            break;
        }
        case 3:
        {
            XSPRITE *pXSprite = &xsprite[a3];
            int key = pXSprite->key;
            if (pXSprite->locked && pPlayer == gMe && pXSprite->lockMsg)
                trTextOver(pXSprite->lockMsg);
            if (!key || pPlayer->hasKey[key])
                trTriggerSprite(a2, pXSprite, kCmdSpritePush, pPlayer->nSprite);
            else if (pPlayer == gMe)
            {
                viewSetMessage("That requires a key.");
                sndStartSample(3063, 255, 2, 0);
            }
            break;
        }
        }
        if (pPlayer->handTime > 0)
            pPlayer->handTime = ClipLow(pPlayer->handTime-kTicsPerFrame*(6-gGameOptions.nDifficulty), 0);
        if (pPlayer->handTime <= 0 && pPlayer->hand) // if hand enemy successfully thrown off
            playerDropHand(pPlayer);
        pInput->keyFlags.action = 0;
    }
    if (gDemo.VanillaDemo())
    {
        if (pInput->keyFlags.lookCenter && !pInput->buttonFlags.lookUp && !pInput->buttonFlags.lookDown)
        {
            if (pPlayer->q16look < 0)
                pPlayer->q16look = fix16_min(pPlayer->q16look+F16(4), F16(0));
            if (pPlayer->q16look > 0)
                pPlayer->q16look = fix16_max(pPlayer->q16look-F16(4), F16(0));
            if (!pPlayer->q16look)
                pInput->keyFlags.lookCenter = 0;
        }
        else
        {
            if (pInput->buttonFlags.lookUp)
                pPlayer->q16look = fix16_min(pPlayer->q16look+F16(4), F16(60));
            if (pInput->buttonFlags.lookDown)
                pPlayer->q16look = fix16_max(pPlayer->q16look-F16(4), F16(-60));
        }
        pPlayer->q16look = fix16_clamp(pPlayer->q16look+pInput->q16mlook, F16(-60), F16(60));
        if (pPlayer->q16look > 0)
            pPlayer->q16horiz = fix16_from_int(mulscale30(120, Sin(fix16_to_int(pPlayer->q16look)<<3)));
        else if (pPlayer->q16look < 0)
            pPlayer->q16horiz = fix16_from_int(mulscale30(180, Sin(fix16_to_int(pPlayer->q16look)<<3)));
        else
            pPlayer->q16horiz = 0;
    }
    else
    {
        CONSTEXPR int upAngle = 289;
        CONSTEXPR int downAngle = -347;
        CONSTEXPR double lookStepUp = 4.0*upAngle/60.0;
        CONSTEXPR double lookStepDown = -4.0*downAngle/60.0;
        if (pInput->keyFlags.lookCenter && !pInput->buttonFlags.lookUp && !pInput->buttonFlags.lookDown)
        {
            if (pPlayer->q16look < 0)
                pPlayer->q16look = fix16_min(pPlayer->q16look+F16(lookStepDown), F16(0));
            if (pPlayer->q16look > 0)
                pPlayer->q16look = fix16_max(pPlayer->q16look-F16(lookStepUp), F16(0));
            if (!pPlayer->q16look)
                pInput->keyFlags.lookCenter = 0;
        }
        else
        {
            if (pInput->buttonFlags.lookUp)
                pPlayer->q16look = fix16_min(pPlayer->q16look+F16(lookStepUp), F16(upAngle));
            if (pInput->buttonFlags.lookDown)
                pPlayer->q16look = fix16_max(pPlayer->q16look-F16(lookStepDown), F16(downAngle));
        }
        if (pPlayer == gMe && numplayers == 1)
        {
            if (pInput->buttonFlags.lookUp)
            {
                gViewLookAdjust += float(lookStepUp);
            }
            if (pInput->buttonFlags.lookDown)
            {
                gViewLookAdjust -= float(lookStepDown);
            }
            gViewLookRecenter = pInput->keyFlags.lookCenter && !pInput->buttonFlags.lookUp && !pInput->buttonFlags.lookDown;
        }
        pPlayer->q16look = fix16_clamp(pPlayer->q16look+(pInput->q16mlook<<3), F16(downAngle), F16(upAngle));
        pPlayer->q16horiz = fix16_from_float(100.f*tanf(fix16_to_float(pPlayer->q16look)*fPI/1024.f));
    }
    int nSector = pSprite->sectnum;
    int florhit = gSpriteHit[pSprite->extra].florhit & 0xc000;
    char va;
    if (pXSprite->height < 16 && (florhit == 0x4000 || florhit == 0))
        va = 1;
    else
        va = 0;
    if (va && (sector[nSector].floorstat&2))
    {
        int z1 = getflorzofslope(nSector, pSprite->x, pSprite->y);
        int x2 = pSprite->x+mulscale30(64, Cos(pSprite->ang));
        int y2 = pSprite->y+mulscale30(64, Sin(pSprite->ang));
        short nSector2 = nSector;
        updatesector(x2, y2, &nSector2);
        if (nSector2 == nSector)
        {
            int z2 = getflorzofslope(nSector2, x2, y2);
            pPlayer->q16slopehoriz = interpolate(pPlayer->q16slopehoriz, fix16_from_int(z1-z2)>>3, 0x4000);
        }
    }
    else
    {
        pPlayer->q16slopehoriz = interpolate(pPlayer->q16slopehoriz, F16(0), 0x4000);
        if (klabs(pPlayer->q16slopehoriz) < 4)
            pPlayer->q16slopehoriz = 0;
    }
    pPlayer->slope = (-fix16_to_int(pPlayer->q16horiz))<<7;
    if (!VanillaMode())
        WeaponProcess(pPlayer);
    if (pInput->keyFlags.prevItem)
    {
        pInput->keyFlags.prevItem = 0;
        packPrevItem(pPlayer);
    }
    if (pInput->keyFlags.nextItem)
    {
        pInput->keyFlags.nextItem = 0;
        packNextItem(pPlayer);
    }
    if (pInput->keyFlags.useItem)
    {
        pInput->keyFlags.useItem = 0;
        if (pPlayer->packSlots[pPlayer->packItemId].curAmount > 0)
            packUseItem(pPlayer, pPlayer->packItemId);
    }
    if (pInput->useFlags.useBeastVision)
    {
        pInput->useFlags.useBeastVision = 0;
        if (pPlayer->packSlots[kPackBeastVision].curAmount > 0)
            packUseItem(pPlayer, kPackBeastVision);
    }
    if (pInput->useFlags.useCrystalBall)
    {
        pInput->useFlags.useCrystalBall = 0;
        if (pPlayer->packSlots[kPackCrystalBall].curAmount > 0)
            packUseItem(pPlayer, kPackCrystalBall);
    }
    if (pInput->useFlags.useJumpBoots)
    {
        pInput->useFlags.useJumpBoots = 0;
        if (pPlayer->packSlots[kPackJumpBoots].curAmount > 0)
            packUseItem(pPlayer, kPackJumpBoots);
    }
    if (pInput->useFlags.useMedKit)
    {
        pInput->useFlags.useMedKit = 0;
        if (pPlayer->packSlots[kPackMedKit].curAmount > 0)
            packUseItem(pPlayer, kPackMedKit);
    }
    if (pInput->keyFlags.holsterWeapon)
    {
        pInput->keyFlags.holsterWeapon = 0;
        if (pPlayer->curWeapon)
        {
            WeaponLower(pPlayer);
            viewSetMessage("Holstering weapon");
        }
    }
    CheckPickUp(pPlayer);
}

void playerProcess(PLAYER *pPlayer)
{
    spritetype *pSprite = pPlayer->pSprite;
    int nSprite = pPlayer->nSprite;
    int nXSprite = pSprite->extra;
    XSPRITE *pXSprite = pPlayer->pXSprite;
    POSTURE* pPosture = &pPlayer->pPosture[pPlayer->lifeMode][pPlayer->posture];
    powerupProcess(pPlayer);
    int top, bottom;
    GetSpriteExtents(pSprite, &top, &bottom);
    int dzb = (bottom-pSprite->z)/4;
    int dzt = (pSprite->z-top)/4;
    int dw = pSprite->clipdist<<2;
    if (!gNoClip)
    {
        short nSector = pSprite->sectnum;
        if (pushmove_old(&pSprite->x, &pSprite->y, &pSprite->z, &nSector, dw, dzt, dzb, CLIPMASK0) == -1)
            actDamageSprite(nSprite, pSprite, kDamageFall, 500<<4);
        if (pSprite->sectnum != nSector)
        {
            if (nSector == -1)
            {
                nSector = pSprite->sectnum;
                actDamageSprite(nSprite, pSprite, kDamageFall, 500<<4);
            }
            dassert(nSector >= 0 && nSector < kMaxSectors);
            ChangeSpriteSect(nSprite, nSector);
        }
    }
    ProcessInput(pPlayer);
    int nSpeed = approxDist(xvel[nSprite], yvel[nSprite]);
    pPlayer->zViewVel = interpolate(pPlayer->zViewVel, zvel[nSprite], 0x7000);
    int dz = pPlayer->pSprite->z-pPosture->eyeAboveZ-pPlayer->zView;
    if (dz > 0)
        pPlayer->zViewVel += mulscale16(dz<<8, 0xa000);
    else
        pPlayer->zViewVel += mulscale16(dz<<8, 0x1800);
    pPlayer->zView += pPlayer->zViewVel>>8;
    pPlayer->zWeaponVel = interpolate(pPlayer->zWeaponVel, zvel[nSprite], 0x5000);
    dz = pPlayer->pSprite->z-pPosture->weaponAboveZ-pPlayer->zWeapon;
    if (dz > 0)
        pPlayer->zWeaponVel += mulscale16(dz<<8, 0x8000);
    else
        pPlayer->zWeaponVel += mulscale16(dz<<8, 0xc00);
    pPlayer->zWeapon += pPlayer->zWeaponVel>>8;
    pPlayer->bobPhase = ClipLow(pPlayer->bobPhase-kTicsPerFrame, 0);
    nSpeed >>= 16;
    if (pPlayer->posture == kPostureSwim)
    {
        pPlayer->bobAmp = (pPlayer->bobAmp+17)&2047;
        pPlayer->swayAmp = (pPlayer->swayAmp+17)&2047;
        pPlayer->bobHeight = mulscale30(pPosture->bobV*10, Sin(pPlayer->bobAmp*2));
        pPlayer->bobWidth = mulscale30(pPosture->bobH*pPlayer->bobPhase, Sin(pPlayer->bobAmp-256));
        pPlayer->swayHeight = mulscale30(pPosture->swayV*pPlayer->bobPhase, Sin(pPlayer->swayAmp*2));
        pPlayer->swayWidth = mulscale30(pPosture->swayH*pPlayer->bobPhase, Sin(pPlayer->swayAmp-0x155));
    }
    else
    {
        if (pXSprite->height < 256)
        {
            pPlayer->bobAmp = (pPlayer->bobAmp+pPosture->pace[pPlayer->isRunning]*4) & 2047;
            pPlayer->swayAmp = (pPlayer->swayAmp+(pPosture->pace[pPlayer->isRunning]*4)/2) & 2047;
            if (pPlayer->isRunning)
            {
                if (pPlayer->bobPhase < 60)
                    pPlayer->bobPhase = ClipHigh(pPlayer->bobPhase+nSpeed, 60);
            }
            else
            {
                if (pPlayer->bobPhase < 30)
                    pPlayer->bobPhase = ClipHigh(pPlayer->bobPhase+nSpeed, 30);
            }
        }
        pPlayer->bobHeight = mulscale30(pPosture->bobV*pPlayer->bobPhase, Sin(pPlayer->bobAmp*2));
        pPlayer->bobWidth = mulscale30(pPosture->bobH*pPlayer->bobPhase, Sin(pPlayer->bobAmp-256));
        pPlayer->swayHeight = mulscale30(pPosture->swayV*pPlayer->bobPhase, Sin(pPlayer->swayAmp*2));
        pPlayer->swayWidth = mulscale30(pPosture->swayH*pPlayer->bobPhase, Sin(pPlayer->swayAmp-0x155));
    }
    pPlayer->flickerEffect = 0;
    pPlayer->quakeEffect = ClipLow(pPlayer->quakeEffect-kTicsPerFrame, 0);
    pPlayer->tiltEffect = ClipLow(pPlayer->tiltEffect-kTicsPerFrame, 0);
    pPlayer->visibility = ClipLow(pPlayer->visibility-kTicsPerFrame, 0);
    pPlayer->painEffect = ClipLow(pPlayer->painEffect-kTicsPerFrame, 0);
    pPlayer->blindEffect = ClipLow(pPlayer->blindEffect-kTicsPerFrame, 0);
    pPlayer->pickupEffect = ClipLow(pPlayer->pickupEffect-kTicsPerFrame, 0);
    if (!pXSprite->health)
    {
        if (pPlayer->hand && gGameOptions.nGameType != kGameTypeSinglePlayer && !VanillaMode())
            playerDropHand(pPlayer);
        else if (!VanillaMode() || pPlayer == gMe)
            pPlayer->hand = 0;
        return;
    }
    pPlayer->isUnderwater = 0;
    if (pPlayer->posture == kPostureSwim)
    {
        pPlayer->isUnderwater = 1;
        int nSector = pSprite->sectnum;
        int nLink = gLowerLink[nSector];
        if (nLink > 0 && (sprite[nLink].type == kMarkerLowGoo || sprite[nLink].type == kMarkerLowWater))
        {
            if (getceilzofslope(nSector, pSprite->x, pSprite->y) > pPlayer->zView)
                pPlayer->isUnderwater = 0;
        }
    }
    if (!pPlayer->isUnderwater)
    {
        pPlayer->underwaterTime = 1200;
        pPlayer->chokeEffect = 0;
        if (packItemActive(pPlayer, kPackDivingSuit))
            packUseItem(pPlayer, kPackDivingSuit);
    }
    int nType = kDudePlayer1-kDudeBase;
    switch (pPlayer->posture)
    {
    case kPostureSwim:
        seqSpawn(dudeInfo[nType].seqStartID+9, 3, nXSprite, -1);
        break;
    case kPostureCrouch:
        seqSpawn(dudeInfo[nType].seqStartID+10, 3, nXSprite, -1);
        break;
    default:
        if (!nSpeed)
            seqSpawn(dudeInfo[nType].seqStartID, 3, nXSprite, -1);
        else
            seqSpawn(dudeInfo[nType].seqStartID+8, 3, nXSprite, -1);
        break;
    }
}

spritetype *playerFireMissile(PLAYER *pPlayer, int a2, int a3, int a4, int a5, int a6)
{
    return actFireMissile(pPlayer->pSprite, a2, pPlayer->zWeapon-pPlayer->pSprite->z, a3, a4, a5, a6);
}

spritetype * playerFireThing(PLAYER *pPlayer, int a2, int a3, int thingType, int a5)
{
    dassert(thingType >= kThingBase && thingType < kThingMax);
    return actFireThing(pPlayer->pSprite, a2, pPlayer->zWeapon-pPlayer->pSprite->z, pPlayer->slope+a3, thingType, a5);
}

void playerFrag(PLAYER *pKiller, PLAYER *pVictim)
{
    dassert(pKiller != NULL);
    dassert(pVictim != NULL);
    
    char buffer[128] = "";
    int nKiller = pKiller->pSprite->type-kDudePlayer1;
    dassert(nKiller >= 0 && nKiller < kMaxPlayers);
    int nVictim = pVictim->pSprite->type-kDudePlayer1;
    dassert(nVictim >= 0 && nVictim < kMaxPlayers);
    if (myconnectindex == connecthead)
    {
        sprintf(buffer, "frag %d killed %d\n", pKiller->nPlayer+1, pVictim->nPlayer+1);
        netBroadcastFrag(buffer);
        buffer[0] = 0;
    }
    if (nKiller == nVictim)
    {
        pVictim->fraggerId = -1;
        if (VanillaMode() || gGameOptions.nGameType != kGameTypeCoop)
        {
            pVictim->fragCount--;
            pVictim->fragInfo[nVictim]--;
        }
        if (gGameOptions.nGameType == kGameTypeTeams)
            gPlayerScores[pVictim->teamId]--;
        int nMessage = Random(5);
        int nSound = gSuicide[nMessage].at4;
        if (pVictim == gMe && gMe->handTime <= 0)
        {
            sprintf(buffer, "You killed yourself!");
            if (gGameOptions.nGameType != kGameTypeSinglePlayer && nSound >= 0)
                sndStartSample(nSound, 255, 2, 0);
        }
        else
        {
            sprintf(buffer, gSuicide[nMessage].at0, gProfile[nVictim].name);
        }
    }
    else
    {
        if (VanillaMode() || gGameOptions.nGameType != kGameTypeCoop)
        {
            pKiller->fragCount++;
            pKiller->fragInfo[nVictim]++;
        }
        if (gGameOptions.nGameType == kGameTypeTeams)
        {
            if (pKiller->teamId == pVictim->teamId)
                gPlayerScores[pKiller->teamId]--;
            else
            {
                gPlayerScores[pKiller->teamId]++;
                gPlayerScoreTicks[pKiller->teamId]+=120;
            }
        }
        int nMessage = Random(25);
        int nSound = gVictory[nMessage].at4;
        const char* pzMessage = gVictory[nMessage].at0;
        sprintf(buffer, pzMessage, gProfile[nKiller].name, gProfile[nVictim].name);
        if (gGameOptions.nGameType != kGameTypeSinglePlayer && nSound >= 0 && pKiller == gMe)
            sndStartSample(nSound, 255, 2, 0);
    }
    viewSetMessage(buffer);
}

void FragPlayer(PLAYER *pPlayer, int nSprite)
{
    spritetype *pSprite = NULL;
    if (nSprite >= 0)
        pSprite = &sprite[nSprite];
    if (pSprite && IsPlayerSprite(pSprite))
    {
        PLAYER *pKiller = &gPlayer[pSprite->type - kDudePlayer1];
        playerFrag(pKiller, pPlayer);
        int nTeam1 = pKiller->teamId&1;
        int nTeam2 = pPlayer->teamId&1;
        if (nTeam1 == 0)
        {
            if (nTeam1 != nTeam2)
                evSend(0, 0, 15, kCmdToggle, pPlayer->nSprite);
            else
                evSend(0, 0, 16, kCmdToggle, pPlayer->nSprite);
        }
        else
        {
            if (nTeam1 == nTeam2)
                evSend(0, 0, 16, kCmdToggle, pPlayer->nSprite);
            else
                evSend(0, 0, 15, kCmdToggle, pPlayer->nSprite);
        }
    }
}

int playerDamageArmor(PLAYER *pPlayer, DAMAGE_TYPE nType, int nDamage)
{
    DAMAGEINFO *pDamageInfo = &damageInfo[nType];
    int nArmorType = pDamageInfo->at0;
    if (nArmorType >= 0 && pPlayer->armor[nArmorType])
    {
#if 0
        int vbp = (nDamage*7)/8-nDamage/4;
        int v8 = pPlayer->at33e[nArmorType];
        int t = nDamage/4 + vbp * v8 / 3200;
        v8 -= t;
#endif
        int v8 = pPlayer->armor[nArmorType];
        int t = scale(v8, 0, 3200, nDamage/4, (nDamage*7)/8);
        v8 -= t;
        nDamage -= t;
        pPlayer->armor[nArmorType] = ClipLow(v8, 0);
    }
    return nDamage;
}

spritetype *playerDropFlag(PLAYER *pPlayer, int a2)
{
    char buffer[80];
    spritetype *pSprite = NULL;
    switch (a2)
    {
    case kItemFlagA:
        pPlayer->hasFlag &= ~1;
        pSprite = actDropObject(pPlayer->pSprite, kItemFlagA);
        if (pSprite)
            pSprite->owner = pPlayer->used2[0];
        gBlueFlagDropped = true;
        sprintf(buffer, "%s dropped Blue Flag", gProfile[pPlayer->nPlayer].name);
        sndStartSample(8005, 255, 2, 0);
        viewSetMessage(buffer);
        break;
    case kItemFlagB:
        pPlayer->hasFlag &= ~2;
        pSprite = actDropObject(pPlayer->pSprite, kItemFlagB);
        if (pSprite)
            pSprite->owner = pPlayer->used2[1];
        gRedFlagDropped = true;
        sprintf(buffer, "%s dropped Red Flag", gProfile[pPlayer->nPlayer].name);
        sndStartSample(8004, 255, 2, 0);
        viewSetMessage(buffer);
        break;
    }
    return pSprite;
}

int playerDamageSprite(int nSource, PLAYER *pPlayer, DAMAGE_TYPE nDamageType, int nDamage)
{
    dassert(nSource < kMaxSprites);
    dassert(pPlayer != NULL);
    if (pPlayer->damageControl[nDamageType])
        return 0;
    nDamage = playerDamageArmor(pPlayer, nDamageType, nDamage);
    pPlayer->painEffect = ClipHigh(pPlayer->painEffect+(nDamage>>3), 600);
    if (pPlayer == gMe)
        ctrlJoystickRumble(pPlayer->painEffect);

    spritetype *pSprite = pPlayer->pSprite;
    XSPRITE *pXSprite = pPlayer->pXSprite;
    int nXSprite = pSprite->extra;
    int nXSector = sector[pSprite->sectnum].extra;
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    int nDeathSeqID = -1;
    int nKneelingPlayer = -1;
    int nSprite = pSprite->index;
    char va = playerSeqPlaying(pPlayer, 16);
    if (!pXSprite->health)
    {
        if (va)
        {
            switch (nDamageType)
            {
            case kDamageSpirit:
                nDeathSeqID = 18;
                sfxPlay3DSound(pSprite, 716, 0, 0);
                break;
            case kDamageExplode:
                GibSprite(pSprite, GIBTYPE_7, NULL, NULL);
                GibSprite(pSprite, GIBTYPE_15, NULL, NULL);
                pPlayer->pSprite->cstat |= 32768;
                nDeathSeqID = 17;
                break;
            default:
            {
                int top, bottom;
                GetSpriteExtents(pSprite, &top, &bottom);
                CGibPosition gibPos(pSprite->x, pSprite->y, top);
                CGibVelocity gibVel(xvel[pSprite->index]>>1, yvel[pSprite->index]>>1, -0xccccc);
                GibSprite(pSprite, GIBTYPE_27, &gibPos, &gibVel);
                GibSprite(pSprite, GIBTYPE_7, NULL, NULL);
                fxSpawnBlood(pSprite, nDamage<<4);
                fxSpawnBlood(pSprite, nDamage<<4);
                nDeathSeqID = 17;
                break;
            }
            }
        }
    }
    else
    {
        int nHealth = pXSprite->health-nDamage;
        pXSprite->health = ClipLow(nHealth, 0);
        if (pXSprite->health > 0 && pXSprite->health < 16)
        {
            nDamageType = kDamageBullet;
            pXSprite->health = 0;
            nHealth = -25;
        }
        if (pXSprite->health > 0)
        {
            DAMAGEINFO *pDamageInfo = &damageInfo[nDamageType];
            int nSound;
            if (nDamage >= (10<<4))
                nSound = pDamageInfo->at4[0];
            else
                nSound = pDamageInfo->at4[Random(3)];
            if (nDamageType == kDamageDrown && pXSprite->medium == kMediumWater && !pPlayer->hand)
                nSound = 714;
            sfxPlay3DSound(pSprite, nSound, 0, 6);
            return nDamage;
        }
        sfxKill3DSound(pPlayer->pSprite, -1, 441);
        if (gGameOptions.nGameType == kGameTypeTeams && pPlayer->hasFlag) {
            if (pPlayer->hasFlag&1) playerDropFlag(pPlayer, kItemFlagA);
            if (pPlayer->hasFlag&2) playerDropFlag(pPlayer, kItemFlagB);
        }
        pPlayer->deathTime = 0;
        pPlayer->qavLoop = 0;
        pPlayer->curWeapon = kWeaponNone;
        pPlayer->fraggerId = nSource;
        pPlayer->voodooTargets = 0;
        if (nDamageType == kDamageExplode && nDamage < (9<<4))
            nDamageType = kDamageFall;
        switch (nDamageType)
        {
        case kDamageExplode:
            sfxPlay3DSound(pSprite, 717, 0, 0);
            GibSprite(pSprite, GIBTYPE_7, NULL, NULL);
            GibSprite(pSprite, GIBTYPE_15, NULL, NULL);
            pPlayer->pSprite->cstat |= 32768;
            nDeathSeqID = 2;
            break;
        case kDamageBurn:
            sfxPlay3DSound(pSprite, 718, 0, 0);
            nDeathSeqID = 3;
            break;
        case kDamageDrown:
            nDeathSeqID = 1;
            break;
        default:
            if (nHealth < -20 && gGameOptions.nGameType >= kGameTypeBloodBath && Chance(0x4000))
            {
                DAMAGEINFO *pDamageInfo = &damageInfo[nDamageType];
                sfxPlay3DSound(pSprite, pDamageInfo->at10[0], 0, 2);
                nDeathSeqID = 16;
                nKneelingPlayer = nPlayerKneelClient;
                powerupActivate(pPlayer, kPwUpDeliriumShroom);
                pXSprite->target = nSource;
                evPost(pSprite->index, 3, 15, kCallbackFinishHim);
            }
            else
            {
                sfxPlay3DSound(pSprite, 716, 0, 0);
                nDeathSeqID = 1;
            }
            break;
        }
    }
    if (nDeathSeqID < 0)
        return nDamage;
    if (nDeathSeqID != 16)
    {
        powerupClear(pPlayer);
        if (nXSector > 0 && xsector[nXSector].Exit)
            trTriggerSector(pSprite->sectnum, &xsector[nXSector], kCmdSectorExit, nSource);
        pSprite->flags |= 7;
        for (int p = connecthead; p >= 0; p = connectpoint2[p])
        {
            if (gPlayer[p].fraggerId == nSprite && gPlayer[p].deathTime > 0)
                gPlayer[p].fraggerId = -1;
        }
        FragPlayer(pPlayer, nSource);
        trTriggerSprite(nSprite, pXSprite, kCmdOff, nSource);

        #ifdef NOONE_EXTENSIONS
        // allow drop items and keys in multiplayer
        if (gModernMap && gGameOptions.nGameType != kGameTypeSinglePlayer && pPlayer->pXSprite->health <= 0) {
            
            spritetype* pItem = NULL;
            if (pPlayer->pXSprite->dropMsg && (pItem = actDropItem(pPlayer->pSprite, pPlayer->pXSprite->dropMsg)) != NULL)
                evPost(pItem->index, OBJ_SPRITE, 500, kCallbackRemove);

            if (pPlayer->pXSprite->key) {
                
                int i; // if all players have this key, don't drop it
                for (i = connecthead; i >= 0; i = connectpoint2[i]) {
                    if (!gPlayer[i].hasKey[pPlayer->pXSprite->key])
                        break;
                }
                
                if (i == 0 && (pItem = actDropKey(pPlayer->pSprite, (pPlayer->pXSprite->key + kItemKeyBase) - 1)) != NULL)
                    evPost(pItem->index, OBJ_SPRITE, 500, kCallbackRemove);

            }


        }
        #endif

    }
    dassert(gSysRes.Lookup(pDudeInfo->seqStartID + nDeathSeqID, "SEQ") != NULL);
    seqSpawn(pDudeInfo->seqStartID+nDeathSeqID, 3, nXSprite, nKneelingPlayer);
    return nDamage;
}

int UseAmmo(PLAYER *pPlayer, int nAmmoType, int nDec)
{
    if (gInfiniteAmmo)
        return 9999;
    if (nAmmoType == -1)
        return 9999;
    pPlayer->ammoCount[nAmmoType] = ClipLow(pPlayer->ammoCount[nAmmoType]-nDec, 0);
    return pPlayer->ammoCount[nAmmoType];
}

void voodooTarget(PLAYER *pPlayer)
{
    int v4 = pPlayer->aim.dz;
    int dz = pPlayer->zWeapon-pPlayer->pSprite->z;
    if (UseAmmo(pPlayer, 9, 0) < 8)
    {
        pPlayer->voodooTargets = 0;
        return;
    }
    for (int i = 0; i < 4; i++)
    {
        int ang1 = (pPlayer->voodooVar1+pPlayer->vodooVar2)&2047;
        actFireVector(pPlayer->pSprite, 0, dz, Cos(ang1)>>16, Sin(ang1)>>16, v4, kVectorVoodoo10);
        int ang2 = (pPlayer->voodooVar1+2048-pPlayer->vodooVar2)&2047;
        actFireVector(pPlayer->pSprite, 0, dz, Cos(ang2)>>16, Sin(ang2)>>16, v4, kVectorVoodoo10);
    }
    pPlayer->voodooTargets = ClipLow(pPlayer->voodooTargets-1, 0);
}

int playerEffectGet(PLAYER* pPlayer, int nEffect)
{
    switch (nEffect)
    {
        case kPlayerEffectTilt:     return pPlayer->tiltEffect;
        case kPlayerEffectPain:     return pPlayer->painEffect;
        case kPlayerEffectBlind:    return pPlayer->blindEffect;
        case kPlayerEffectPickup:   return pPlayer->pickupEffect;
        case kPlayerEffectQuake:    return pPlayer->quakeEffect;
        case kPlayerEffectBright:   return pPlayer->visibility;
        case kPlayerEffectDelirium: return pPlayer->pwUpTime[kPwUpDeliriumShroom];
        case kPlayerEffectFlicker:  return pPlayer->flickerEffect;
        case kPlayerEffectFlash:    return (pPlayer->flashEffect) ? pPlayer->visibility : 0;
    }

    return 0;
}

void playerEffectSet(PLAYER* pPlayer, int nEffect, int nTime)
{
    switch (nEffect)
    {
        case kPlayerEffectTilt:     pPlayer->tiltEffect = nTime;                        break;
        case kPlayerEffectPain:     pPlayer->painEffect = nTime;                        break;
        case kPlayerEffectBlind:    pPlayer->blindEffect = nTime;                       break;
        case kPlayerEffectPickup:   pPlayer->pickupEffect = nTime;                      break;
        case kPlayerEffectQuake:    pPlayer->quakeEffect = nTime;                       break;
        case kPlayerEffectBright:   pPlayer->visibility = nTime;                        break;
        case kPlayerEffectDelirium: pPlayer->pwUpTime[kPwUpDeliriumShroom] = nTime;     break;
        case kPlayerEffectFlicker:  pPlayer->flickerEffect = nTime;                     break;
        case kPlayerEffectFlash:
            pPlayer->visibility  = nTime;
            pPlayer->flashEffect = 1;
            break;
    }
}

void playerLandingSound(PLAYER *pPlayer)
{
    static int surfaceSound[] = {
        -1,
        600,
        601,
        602,
        603,
        604,
        605,
        605,
        605,
        600,
        605,
        605,
        605,
        604,
        603
    };
    spritetype *pSprite = pPlayer->pSprite;
    SPRITEHIT *pHit = &gSpriteHit[pSprite->extra];
    if (pHit->florhit)
    {
        if ((pPlayer == gMe) && gCenterViewOnDrop)
            gCenterViewOnDrop = 2;
        if (!gGameOptions.bFriendlyFire && IsTargetTeammate(pPlayer, &sprite[pHit->florhit & 0x3fff]))
            return;
        char nSurf = tileGetSurfType(pHit->florhit);
        if (nSurf)
            sfxPlay3DSound(pSprite, surfaceSound[nSurf], -1, 0);
    }
}

void PlayerSurvive(int, int nXSprite)
{
    char buffer[80];
    XSPRITE *pXSprite = &xsprite[nXSprite];
    int nSprite = pXSprite->reference;
    spritetype *pSprite = &sprite[nSprite];
    actHealDude(pXSprite, 1, 2);
    if (gGameOptions.nGameType != kGameTypeSinglePlayer && numplayers > 1)
    {
        sfxPlay3DSound(pSprite, 3009, 0, 6);
        if (IsPlayerSprite(pSprite))
        {
            PLAYER *pPlayer = &gPlayer[pSprite->type-kDudePlayer1];
            if (pPlayer == gMe)
                viewSetMessage("I LIVE...AGAIN!!");
            else
            {
                sprintf(buffer, "%s lives again!", gProfile[pPlayer->nPlayer].name);
                viewSetMessage(buffer);
            }
            pPlayer->input.newWeapon = kWeaponPitchfork;
        }
    }
}

void PlayerKneelsOver(int, int nXSprite)
{
    XSPRITE *pXSprite = &xsprite[nXSprite];
    for (int p = connecthead; p >= 0; p = connectpoint2[p])
    {
        if (gPlayer[p].pXSprite == pXSprite)
        {
            PLAYER *pPlayer = &gPlayer[p];
            playerDamageSprite(pPlayer->fraggerId, pPlayer, kDamageSpirit, 500<<4);
            return;
        }
    }
}

void playerHandChoke(PLAYER *pPlayer)
{
    int t = gGameOptions.nDifficulty+2;
    if (pPlayer->handTime < 64)
        pPlayer->handTime = ClipHigh(pPlayer->handTime+t, 64);
    if (pPlayer->handTime > (7-gGameOptions.nDifficulty)*5)
        pPlayer->blindEffect = ClipHigh(pPlayer->blindEffect+t*4, 128);
}

class PlayerLoadSave : public LoadSave
{
public:
    virtual void Load(void);
    virtual void Save(void);
};

void PlayerLoadSave::Load(void)
{

    Read(gPlayerScores, sizeof(gPlayerScores));
    Read(&gNetPlayers, sizeof(gNetPlayers));
    Read(&gProfile, sizeof(gProfile));
    Read(&gPlayer, sizeof(gPlayer));
    #ifdef NOONE_EXTENSIONS
        Read(&gPlayerCtrl, sizeof(gPlayerCtrl));
    #endif
    for (int i = 0; i < gNetPlayers; i++) {
        gPlayer[i].pSprite = &sprite[gPlayer[i].nSprite];
        gPlayer[i].pXSprite = &xsprite[gPlayer[i].pSprite->extra];
        gPlayer[i].pDudeInfo = &dudeInfo[gPlayer[i].pSprite->type-kDudeBase];
        
    #ifdef NOONE_EXTENSIONS
        // load qav scene
        if (gPlayer[i].sceneQav != -1) {
            if (gPlayerCtrl[i].qavScene.qavResrc == NULL) 
                gPlayer[i].sceneQav = -1;
            else {
                QAV* pQav = playerQavSceneLoad(gPlayer[i].sceneQav);
                if (pQav) {
                    gPlayerCtrl[i].qavScene.qavResrc = pQav;
                    gPlayerCtrl[i].qavScene.qavResrc->Preload();
                } else {
                    gPlayer[i].sceneQav = -1;
                }
            }
        }
    #endif

    }
    gCrouchToggleState = 0; // reset crouch toggle state
}

void PlayerLoadSave::Save(void)
{
    Write(gPlayerScores, sizeof(gPlayerScores));
    Write(&gNetPlayers, sizeof(gNetPlayers));
    Write(&gProfile, sizeof(gProfile));
    Write(&gPlayer, sizeof(gPlayer));
    
    #ifdef NOONE_EXTENSIONS
    Write(&gPlayerCtrl, sizeof(gPlayerCtrl));
    #endif
}

static PlayerLoadSave *myLoadSave;

void PlayerLoadSaveConstruct(void)
{
    myLoadSave = new PlayerLoadSave();
}
