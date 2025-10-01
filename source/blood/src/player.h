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
#pragma once
#include "actor.h"
#include "blood.h"
#include "build.h"
#include "common_game.h"
#include "compat.h"
#include "config.h"
#include "controls.h"
#include "db.h"
#include "dude.h"
#include "fix16.h"
#include "levels.h"
#include "qav.h"

// life modes of the player
enum
{
    kModeHuman       = 0,
    kModeBeast       = 1,
    kModeHumanShrink = 2,
    kModeHumanGrown  = 3,
    kModeMax         = 4,
};

// postures
enum
{
    kPostureStand  = 0,
    kPostureSwim   = 1,
    kPostureCrouch = 2,
    kPostureMax    = 3,
};

// inventory pack
enum
{
    kPackBase        = 0,
    kPackMedKit      = kPackBase,
    kPackDivingSuit  = 1,
    kPackCrystalBall = 2,
    kPackBeastVision = 3,
    kPackJumpBoots   = 4,
    kPackMax         = 5,
};

// screen effects
enum
{
    kPlayerEffectPickup             = 0,
    kPlayerEffectPain,
    kPlayerEffectChoke,
    kPlayerEffectBlind,
    kPlayerEffectTilt,
    kPlayerEffectQuake,
    kPlayerEffectBright,
    kPlayerEffectDelirium,
    kPlayerEffectFlicker,
    kPlayerEffectFlash,
    kPlayerEffectMax,
};

struct PACKINFO
{
    bool isActive;       // is active (0/1)
    int  curAmount = 0;  // remaining percent
};

struct POSTURE
{
    int frontAccel;
    int sideAccel;
    int backAccel;
    int pace[2];
    int bobV;
    int bobH;
    int swayV;
    int swayH;
    int eyeAboveZ;
    int weaponAboveZ;
    int xOffset;
    int zOffset;
    int normalJumpZ;
    int pwupJumpZ;
};

extern POSTURE gPostureDefaults[kModeMax][kPostureMax];

enum
{
    kWeaponNone         = 0,
    kWeaponPitchfork    = 1,
    kWeaponFlare        = 2,
    kWeaponShotgun      = 3,
    kWeaponTommy        = 4,
    kWeaponNapalm       = 5,
    kWeaponTNT          = 6,
    kWeaponSprayCan     = 7,
    kWeaponTesla        = 8,
    kWeaponLifeLeech    = 9,
    kWeaponVoodoo       = 10,
    kWeaponProxyTNT     = 11,
    kWeaponRemoteTNT    = 12,
    kWeaponBeast        = 13,
    kWeaponMax          = 14,
};

struct PLAYER
{
    spritetype*         pSprite;
    XSPRITE*            pXSprite;
    DUDEINFO*           pDudeInfo;
    GINPUT              input;
    //short             input;                      // INPUT
    //char              moveFunc;                        // forward
    //short             at11;                       // turn
    //char              hearDist;                    // strafe
    //int               bobV;                         // buttonFlags
    //unsigned int      bobH;                // keyFlags
    //char              swayV;                       // useFlags;
    //char              swayH;                       // newWeapon
    //char              at21;                        // mlook
    int                 used1;  // something related to game checksum
    int                 weaponQav;
    int                 qavCallback;
    bool                isRunning;
    int                 posture;   // stand, crouch, swim
    int                 sceneQav;  // by NoOne: used to keep qav id
    int                 bobPhase;
    int                 bobAmp;
    int                 bobHeight;
    int                 bobWidth;
    int                 swayPhase;
    int                 swayAmp;
    int                 swayHeight;
    int                 swayWidth;
    int                 nPlayer;  // Connect id
    int                 nSprite;
    int                 lifeMode;
    int                 bloodlust;  // ---> useless
    int                 zView;
    int                 zViewVel;
    int                 zWeapon;
    int                 zWeaponVel;
    fix16_t             q16look;
    fix16_t             q16horiz;       // horiz
    fix16_t             q16slopehoriz;  // horizoff
    int                 slope;
    bool                isUnderwater;
    bool                hasKey[8];
    char                hasFlag;
    short               used2[8];  // ??
    int                 damageControl[kDamageMax];
    char                curWeapon;
    char                nextWeapon;
    int                 weaponTimer;
    int                 weaponState;
    int                 weaponAmmo;  //rename
    bool                hasWeapon[kWeaponMax];
    int                 weaponMode[kWeaponMax];
    int                 weaponOrder[2][kWeaponMax];
    //int               at149[14];
    int                 ammoCount[12];
    bool                qavLoop;
    int                 fuseTime;
    int                 throwTime;
    int                 throwPower;
    Aim                 aim;  // world
    //int               at1c6;
    Aim                 relAim;  // relative
    //int               relAim;
    //int               at1ce;
    //int               at1d2;
    int                 aimTarget;  // aim target sprite
    int                 aimTargetsCount;
    short               aimTargets[16];
    int                 deathTime;
    int                 pwUpTime[kMaxPowerUps];
    int                 fragCount;
    int                 fragInfo[kMaxPlayers];
    int                 teamId;
    int                 fraggerId;
    int                 underwaterTime;
    int                 bloodTime;  // --> useless
    int                 gooTime;    // --> useless
    int                 wetTime;    // --> useless
    int                 bubbleTime;
    int                 lastWeapon; // at306;
    int                 restTime;
    int                 kickPower;
    int                 laughCount;
    int                 spin;  // turning around
    bool                godMode;
    bool                fallScream;
    bool                cantJump;
    int                 packItemTime;  // pack timer
    int                 packItemId;    // pack id 1: diving suit, 2: crystal ball, 3: beast vision 4: jump boots
    PACKINFO            packSlots[kPackMax];
    int                 armor[3];      // armor
    //int               at342;
    //int               at346;
    int                 voodooTarget;
    int                 voodooTargets;  // --> useless
    int                 voodooVar1;     // --> useless
    int                 vodooVar2;      // --> useless
    int                 flickerEffect;
    int                 tiltEffect;
    int                 visibility;
    int                 painEffect;
    int                 blindEffect;
    int                 chokeEffect;
    int                 handTime;
    bool                hand;  // if true, there is hand start choking the player
    int                 pickupEffect;
    bool                flashEffect;  // if true, reduce pPlayer->visibility counter; originally 32-bit
    int                 quakeEffect;
    fix16_t             q16ang;
    int                 angold;
    int                 player_par;
    int                 nWaterPal;
    POSTURE             pPosture[kModeMax][kPostureMax];

    // Calculates checksum for multiplayer games. Skips data referenced
    // by pointers, like XSPRITE structs. Other bits might further be skipped.
    uint32_t CalcNonSpriteChecksum(void);
};

struct PROFILE
{
    int  nAutoAim;
    int  nWeaponSwitch;
    int  skill;
    char name[MAXPLAYERNAME];
};

struct AMMOINFO
{
    int         max;
    signed char vectorType;
};

struct POWERUPINFO
{
    short picnum;
    bool  pickupOnce;
    int   bonusTime;
    int   maxTime;
};

void playerResetPosture(PLAYER* pPlayer);

extern PLAYER  gPlayer[kMaxPlayers];
extern PLAYER *gMe, *gView;

extern PROFILE gProfile[kMaxPlayers];

extern bool gBlueFlagDropped;
extern bool gRedFlagDropped;

extern int         gPlayerScores[kMaxPlayers];
extern ClockTicks  gPlayerScoreTicks[kMaxPlayers];
extern AMMOINFO    gAmmoInfo[];
extern POWERUPINFO gPowerUpInfo[kMaxPowerUps];

inline bool IsTargetTeammate(PLAYER *pSourcePlayer, spritetype *pTargetSprite)
{
    if (pSourcePlayer == NULL)
        return false;
    if (!IsPlayerSprite(pTargetSprite))
        return false;
    if (gGameOptions.nGameType == kGameTypeCoop || gGameOptions.nGameType == kGameTypeTeams)
    {
        PLAYER *pTargetPlayer = &gPlayer[pTargetSprite->type - kDudePlayer1];
        if (pSourcePlayer != pTargetPlayer)
        {
            if (gGameOptions.nGameType == kGameTypeCoop)
                return true;
            if (gGameOptions.nGameType == kGameTypeTeams && (pSourcePlayer->teamId & 3) == (pTargetPlayer->teamId & 3))
                return true;
        }
    }

    return false;
}

inline bool IsTargetTeammate(spritetype *pSourceSprite, spritetype *pTargetSprite)
{
    if (!IsPlayerSprite(pSourceSprite))
        return false;
    PLAYER *pSourcePlayer = &gPlayer[pSourceSprite->type - kDudePlayer1];
    return IsTargetTeammate(pSourcePlayer, pTargetSprite);
}

int         powerupCheck(PLAYER *pPlayer, int nPowerUp);
char        powerupActivate(PLAYER *pPlayer, int nPowerUp);
void        powerupDeactivate(PLAYER *pPlayer, int nPowerUp);
void        powerupSetState(PLAYER *pPlayer, int nPowerUp, char bState);
void        powerupProcess(PLAYER *pPlayer);
void        powerupClear(PLAYER *pPlayer);
void        powerupInit(void);
int         packItemToPowerup(int nPack);
int         powerupToPackItem(int nPowerUp);
char        packAddItem(PLAYER *pPlayer, unsigned int nPack);
int         packCheckItem(PLAYER *pPlayer, int nPack);
char        packItemActive(PLAYER *pPlayer, int nPack);
void        packUseItem(PLAYER *pPlayer, int nPack);
void        packPrevItem(PLAYER *pPlayer);
void        packNextItem(PLAYER *pPlayer);
char        playerSeqPlaying(PLAYER *pPlayer, int nSeq);
void        playerSetRace(PLAYER *pPlayer, int nLifeMode);
void        playerSetGodMode(PLAYER *pPlayer, char bGodMode);
void        playerResetInertia(PLAYER *pPlayer);
void        playerCorrectInertia(PLAYER *pPlayer, vec3_t const *oldpos);
void        playerStart(int nPlayer, int bNewLevel = 0);
void        playerResetScores(int nPlayer);
void        playerReset(PLAYER *pPlayer);
void        playerInit(int nPlayer, unsigned int a2);
char        findDroppedLeech(PLAYER *a1, spritetype *a2);
char        PickupItem(PLAYER *pPlayer, spritetype *pItem);
char        PickupAmmo(PLAYER *pPlayer, spritetype *pAmmo);
char        PickupWeapon(PLAYER *pPlayer, spritetype *pWeapon);
void        PickUp(PLAYER *pPlayer, spritetype *pSprite);
void        CheckPickUp(PLAYER *pPlayer);
int         ActionScan(PLAYER *pPlayer, int *a2, int *a3);
void        ProcessInput(PLAYER *pPlayer);
void        playerProcess(PLAYER *pPlayer);
spritetype *playerFireMissile(PLAYER *pPlayer, int a2, int a3, int a4, int a5, int a6);
spritetype *playerFireThing(PLAYER *pPlayer, int a2, int a3, int thingType, int a5);
void        playerFrag(PLAYER *pKiller, PLAYER *pVictim);
void        FragPlayer(PLAYER *pPlayer, int nSprite);
int         playerDamageArmor(PLAYER *pPlayer, DAMAGE_TYPE nType, int nDamage);
spritetype *playerDropFlag(PLAYER *pPlayer, int a2);
int         playerDamageSprite(int nSource, PLAYER *pPlayer, DAMAGE_TYPE nDamageType, int nDamage);
int         UseAmmo(PLAYER *pPlayer, int nAmmoType, int nDec);
void        voodooTarget(PLAYER *pPlayer);
int         playerEffectGet(PLAYER* pPlayer, int nEffect);
void        playerEffectSet(PLAYER* pPlayer, int nEffect, int nTime);
void        playerLandingSound(PLAYER *pPlayer);
void        PlayerSurvive(int, int nXSprite);
void        PlayerKneelsOver(int, int nXSprite);
void        playerHandChoke(PLAYER *pPlayer);
