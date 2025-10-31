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
#include "compat.h"
#include "mmulti.h"
#include "common_game.h"
#include "fx_man.h"
#include "music.h"
#include "blood.h"
#include "demo.h"
#include "config.h"
#include "gamemenu.h"
#include "globals.h"
#include "loadsave.h"
#include "menu.h"
#include "messages.h"
#include "network.h"
#include "osdcmds.h"
#include "sfx.h"
#include "screen.h"
#include "sound.h"
#include "view.h"

void SaveGame(CGameMenuItemZEditBitmap *, CGameMenuEvent *);

void SaveGameProcess(CGameMenuItemChain *);
void ShowDifficulties();
void SetDifficultyAndStart(CGameMenuItemChain *);
void SetDifficultyCustomAndStart(CGameMenuItemChain *);
void SetDetail(CGameMenuItemSlider *);
void SetVoxels(CGameMenuItemZBool *);
void SetGamma(CGameMenuItemSlider *);
void SetMusicVol(CGameMenuItemSlider *);
void SetSoundVol(CGameMenuItemSlider *);
void SetCDVol(CGameMenuItemSlider *);
void SetMonoStereo(CGameMenuItemZBool *);
void SetCrosshair(CGameMenuItemZBool *);
void SetCenterHoriz(CGameMenuItemZBool *);
void SetShowPlayerNames(CGameMenuItemZBool *);
void SetShowWeapons(CGameMenuItemZCycle *);

void SetWeaponsV10X(CGameMenuItemZBool*);
void SetVanillaMode(CGameMenuItemZBool *pItem);

void SetSlopeTilting(CGameMenuItemZBool *);
void SetViewBobbing(CGameMenuItemZBool *);
void SetViewSwaying(CGameMenuItemZBool *);
void SetMouseSensitivity(CGameMenuItemSliderFloat *);
void SetMouseAimFlipped(CGameMenuItemZBool *);
void SetTurnSpeed(CGameMenuItemSlider *);
void SetTurnAcceleration(CGameMenuItemZCycle *);
void SetCenterView(CGameMenuItemZBool *);
void SetJoystickRumble(CGameMenuItemZBool *pItem);
void ResetKeys(CGameMenuItemChain *);
void ResetKeysClassic(CGameMenuItemChain *);
void SetMessages(CGameMenuItemZBool *);
void LoadGame(CGameMenuItemZEditBitmap *, CGameMenuEvent *);
void SetupNetLevels(CGameMenuItemZCycle *);
void StartNetGame(CGameMenuItemChain *);
void SetParentalLock(CGameMenuItemZBool *);
void TenProcess(CGameMenuItem7EA1C *);
void SetupLevelMenuItem(int);
void SetupVideoModeMenu(CGameMenuItemChain *);
void SetVideoMode(CGameMenuItemChain *);
void SetWidescreen(CGameMenuItemZBool *);
void SetFOV(CGameMenuItemSlider *);
void UpdateVideoModeMenuFrameLimit(CGameMenuItemZCycle *pItem);
void UpdateVideoColorMenu(CGameMenuItemSliderFloat *);
void ResetVideoColor(CGameMenuItemChain *);
void ClearUserMapNameOnLevelChange(CGameMenuItemZCycle *);
#ifdef USE_OPENGL
void SetupVideoPolymostMenu(CGameMenuItemChain *);
#endif

char strRestoreGameStrings[][16] = 
{
    "<Empty>",
    "<Empty>",
    "<Empty>",
    "<Empty>",
    "<Empty>",
    "<Empty>",
    "<Empty>",
    "<Empty>",
    "<Empty>",
    "<Empty>",
};

char restoreGameDifficulty[] = 
{
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

const char *zNetGameTypes[] =
{
    "Cooperative",
    "Bloodbath",
    "Teams",
};

const char *zMonsterStrings[] =
{
    "None",
    "Bring 'em on",
    "Respawn",
};

const char *zWeaponStrings[] =
{
    "Do not Respawn",
    "Are Permanent",
    "Respawn",
    "Respawn with Markers",
};

const char *zItemStrings[] =
{
    "Do not Respawn",
    "Respawn",
    "Respawn with Markers",
};

const char *zRespawnStrings[] =
{
    "At Random Locations",
    "Close to Weapons",
    "Away from Enemies",
};

const char *zDiffStrings[] =
{
    "STILL KICKING",
    "PINK ON THE INSIDE",
    "LIGHTLY BROILED",
    "WELL DONE",
    "EXTRA CRISPY",
};

const char *pzShowWeaponStrings[] = {
    "OFF",
    "SPRITE",
    "VOXEL"
};

const char *zPlayerKeysStrings[] = {
    "LOST ON DEATH",
    "KEPT ON RESPAWN",
    "SHARED"
};

char zUserMapName[BMAX_PATH];
const char *zEpisodeNames[6];
const char *zLevelNames[6][kMaxLevels];

static char MenuGameFuncs[NUMGAMEFUNCTIONS][MAXGAMEFUNCLEN];
static char const *MenuGameFuncNone = "  -None-";
static char const *pzGamefuncsStrings[NUMGAMEFUNCTIONS + 1];
static int nGamefuncsValues[NUMGAMEFUNCTIONS + 1];
static int nGamefuncsNum;

CGameMenu menuMain;
CGameMenu menuMainWithSave;
CGameMenu menuNetMain;
CGameMenu menuNetStart;
CGameMenu menuEpisode;
CGameMenu menuDifficulty;
CGameMenu menuDifficultyCustom;
CGameMenu menuOptionsOld;
CGameMenu menuControls;
CGameMenu menuMessages;
CGameMenu menuKeys;
CGameMenu menuSaveGame;
CGameMenu menuLoadGame;
CGameMenu menuLoading;
CGameMenu menuSounds;
CGameMenu menuQuit;
CGameMenu menuRestart;
CGameMenu menuCredits;
CGameMenu menuOrder;
CGameMenu menuPlayOnline;
CGameMenu menuParentalLock;
CGameMenu menuSorry;
CGameMenu menuSorry2;
CGameMenu menuNetwork;
CGameMenu menuNetworkHost;
CGameMenu menuNetworkJoin;

CGameMenuItemQAV itemBloodQAV("", 3, 160, 100, "BDRIP", true);
CGameMenuItemQAV itemCreditsQAV("", 3, 160, 100, "CREDITS", false, true);
CGameMenuItemQAV itemHelp3QAV("", 3, 160, 100, "HELP3", false, false);
CGameMenuItemQAV itemHelp3BQAV("", 3, 160, 100, "HELP3B", false, false);
CGameMenuItemQAV itemHelp4QAV("", 3, 160, 100, "HELP4", false, true);
CGameMenuItemQAV itemHelp5QAV("", 3, 160, 100, "HELP5", false, true);

CGameMenuItemTitle itemMainTitle(gGameTitle, 1, 160, 20, 2038);
CGameMenuItemChain itemMain1("NEW GAME", 1, 0, 45, 320, 1, &menuEpisode, -1, NULL, 0);
//CGameMenuItemChain itemMain2("PLAY ONLINE", 1, 0, 65, 320, 1, &menuPlayOnline, -1, NULL, 0);
CGameMenuItemChain itemMain2("MULTIPLAYER", 1, 0, 65, 320, 1, &menuNetwork, -1, NULL, 0);
CGameMenuItemChain itemMain3("OPTIONS", 1, 0, 85, 320, 1, &menuOptions, -1, NULL, 0);
CGameMenuItemChain itemMain4("LOAD GAME", 1, 0, 105, 320, 1, &menuLoadGame, -1, NULL, 0);
CGameMenuItemChain itemMain5("HELP", 1, 0, 125, 320, 1, &menuOrder, -1, NULL, 0);
CGameMenuItemChain itemMain6("CREDITS", 1, 0, 145, 320, 1, &menuCredits, -1, NULL, 0);
CGameMenuItemChain itemMain7("QUIT", 1, 0, 165, 320, 1, &menuQuit, -1, NULL, 0);

CGameMenuItemTitle itemMainSaveTitle(gGameTitle, 1, 160, 20, 2038);
CGameMenuItemChain itemMainSave1("NEW GAME", 1, 0, 45, 320, 1, &menuEpisode, -1, NULL, 0);
//CGameMenuItemChain itemMainSave2("PLAY ONLINE", 1, 0, 60, 320, 1, &menuPlayOnline, -1, NULL, 0);
CGameMenuItemChain itemMainSave2("OPTIONS", 1, 0, 60, 320, 1, &menuOptions, -1, NULL, 0);
CGameMenuItemChain itemMainSave3("SAVE GAME", 1, 0, 75, 320, 1, &menuSaveGame, -1, SaveGameProcess, 0);
CGameMenuItemChain itemMainSave4("LOAD GAME", 1, 0, 90, 320, 1, &menuLoadGame, -1, NULL, 0);
CGameMenuItemChain itemMainSave5("HELP", 1, 0, 105, 320, 1, &menuOrder, -1, NULL, 0);
CGameMenuItemChain itemMainSave6("CREDITS", 1, 0, 120, 320, 1, &menuCredits, -1, NULL, 0);
CGameMenuItemChain itemMainSave7("END GAME", 1, 0, 135, 320, 1, &menuRestart, -1, NULL, 0);
CGameMenuItemChain itemMainSave8("QUIT", 1, 0, 150, 320, 1, &menuQuit, -1, NULL, 0);

CGameMenuItemTitle itemEpisodesTitle("EPISODES", 1, 160, 20, 2038);
CGameMenuItemChain7F2F0 itemEpisodes[kMaxEpisodes-1];

CGameMenu menuUserMap;
CGameMenuItemChain itemUserMap("USER MAP", 1, 0, 60, 320, 1, &menuUserMap, 0, NULL, 0);
CGameMenuItemTitle itemUserMapTitle("USER MAP", 1, 160, 20, 2038);
CGameMenuFileSelect itemUserMapList("", 3, 0, 0, 0, "./", "*.map", gGameOptions.szUserMap, ShowDifficulties, 0);

CGameMenuItemTitle itemDifficultyTitle("DIFFICULTY", 1, 160, 20, 2038);
CGameMenuItemChain itemDifficulty1("STILL KICKING", 1, 0, 50, 320, 1, NULL, -1, SetDifficultyAndStart, 0);
CGameMenuItemChain itemDifficulty2("PINK ON THE INSIDE", 1, 0, 70, 320, 1, NULL, -1, SetDifficultyAndStart, 1);
CGameMenuItemChain itemDifficulty3("LIGHTLY BROILED", 1, 0, 90, 320, 1, NULL, -1, SetDifficultyAndStart, 2);
CGameMenuItemChain itemDifficulty4("WELL DONE", 1, 0, 110, 320, 1, NULL, -1, SetDifficultyAndStart, 3);
CGameMenuItemChain itemDifficulty5("EXTRA CRISPY", 1, 0, 130, 320, 1, 0, -1, SetDifficultyAndStart, 4);
CGameMenuItemChain itemDifficulty6("CUSTOM", 1, 0, 150, 320, 1, &menuDifficultyCustom, -1, NULL, 0);

CGameMenuItemTitle itemDifficultyCustomTitle("CUSTOM", 1, 160, 20, 2038);
CGameMenuItemSlider sliderDifficultyCustomQuantity("ENEMIES QUANTITY:", 3, 66, 50, 180, 2, 0, 4, 1, NULL, -1, -1);
CGameMenuItemSlider sliderDifficultyCustomHealth("ENEMIES HEALTH:", 3, 66, 62, 180, 2, 0, 4, 1, NULL, -1, -1);
CGameMenuItemSlider sliderDifficultyCustomDifficulty("ENEMIES DIFFICULTY:", 3, 66, 74, 180, 2, 0, 4, 1, NULL, -1, -1);
CGameMenuItemSlider sliderDifficultyCustomDamage("PLAYER DAMAGE SCALE:", 3, 66, 86, 180, 2, 0, 4, 1, NULL, -1, -1);
CGameMenuItemChain itemDifficultyCustomStart("START GAME", 1, 0, 150, 320, 1, NULL, -1, SetDifficultyCustomAndStart, 0);

CGameMenuItemTitle itemOptionsOldTitle("OPTIONS", 1, 160, 20, 2038);
CGameMenuItemChain itemOption1("CONTROLS...", 3, 0, 40, 320, 1, &menuControls, -1, NULL, 0);
CGameMenuItemSlider sliderDetail("DETAIL:", 3, 66, 50, 180, gDetail, 0, 4, 1, SetDetail, -1, -1);
CGameMenuItemSlider sliderGamma("GAMMA:", 3, 66, 60, 180, gGamma, 0, 15, 2, SetGamma, -1, -1);
CGameMenuItemSlider sliderMusic("MUSIC:", 3, 66, 70, 180, MusicVolume, 0, 255, 48, SetMusicVol, -1, -1);
CGameMenuItemSlider sliderSound("SOUND:", 3, 66, 80, 180, FXVolume, 0, 255, 48, SetSoundVol, -1, -1);
CGameMenuItemSlider sliderCDAudio("CD AUDIO:", 3, 66, 90, 180, CDVolume, 0, 255, 48, SetCDVol, -1, -1);
CGameMenuItemZBool bool3DAudio("3D AUDIO:", 3, 66, 100, 180, gStereo, SetMonoStereo, NULL, NULL);
CGameMenuItemZBool boolCrosshair("CROSSHAIR:", 3, 66, 110, 180, gAimReticle, SetCrosshair, NULL, NULL);
CGameMenuItemZCycle itemCycleShowWeapons("SHOW WEAPONS:", 3, 66, 120, 180, 0, SetShowWeapons, pzShowWeaponStrings, ARRAY_SSIZE(pzShowWeaponStrings), 0);
CGameMenuItemZBool boolSlopeTilting("SLOPE TILTING:", 3, 66, 130, 180, gSlopeTilting, SetSlopeTilting, NULL, NULL);
CGameMenuItemZBool boolViewBobbing("VIEW BOBBING:", 3, 66, 140, 180, gViewVBobbing, SetViewBobbing, NULL, NULL);
CGameMenuItemZBool boolViewSwaying("VIEW SWAYING:", 3, 66, 150, 180, gViewHBobbing, SetViewSwaying, NULL, NULL);
CGameMenuItem7EE34 itemOption2("VIDEO MODE...", 3, 0, 160, 320, 1);
CGameMenuItemChain itemChainParentalLock("PARENTAL LOCK", 3, 0, 170, 320, 1, &menuParentalLock, -1, NULL, 0);

CGameMenuItemTitle itemControlsTitle("CONTROLS", 1, 160, 20, 2038);
CGameMenuItemSliderFloat sliderMouseSpeed("Mouse Sensitivity:", 1, 10, 70, 300, CONTROL_MouseSensitivity, .1f, 100.f, 50.f, SetMouseSensitivity, -1,-1);
CGameMenuItemZBool boolMouseFlipped("Invert Mouse Aim:", 1, 10, 90, 300, gMouseAimingFlipped, SetMouseAimFlipped, NULL, NULL);
CGameMenuItemSlider sliderTurnSpeed("Key Turn Speed:", 1, 10, 110, 300, gTurnSpeed, 64, 128, 4, SetTurnSpeed, -1, -1);
CGameMenuItemChain itemChainKeyList("Configure Keys...", 1, 0, 130, 320, 1, &menuKeys, -1, NULL, 0);
CGameMenuItemChain itemChainKeyReset("Reset Keys (default)...", 1, 0, 150, 320, 1, &menuKeys, -1, ResetKeys, 0);
CGameMenuItemChain itemChainKeyResetClassic("Reset Keys (classic)...", 1, 0, 170, 320, 1, &menuKeys, -1, ResetKeysClassic, 0);

CGameMenuItemTitle itemMessagesTitle("MESSAGES", 1, 160, 20, 2038);
CGameMenuItemZBool boolMessages("MESSAGES:", 3, 66, 70, 180, 0, SetMessages, NULL, NULL);
CGameMenuItemSlider sliderMsgCount("MESSAGE COUNT:", 3, 66, 80, 180, gMessageCount, 1, 16, 1, NULL, -1, -1);
CGameMenuItemSlider sliderMsgTime("MESSAGE TIME:", 3, 66, 90, 180, gMessageTime, 1, 8, 1, NULL, -1, -1);
CGameMenuItemZBool boolMsgFont("LARGE FONT:", 3, 66, 100, 180, 0, 0, NULL, NULL);
CGameMenuItemZBool boolMsgIncoming("INCOMING:", 3, 66, 110, 180, 0, 0, NULL, NULL);
CGameMenuItemZBool boolMsgSelf("SELF PICKUP:", 3, 66, 120, 180, 0, 0, NULL, NULL);
CGameMenuItemZBool boolMsgOther("OTHER PICKUP:", 3, 66, 130, 180, 0, 0, NULL, NULL);
CGameMenuItemZBool boolMsgRespawn("RESPAWN:", 3, 66, 140, 180, 0, 0, NULL, NULL);

CGameMenuItemTitle itemKeysTitle("KEY SETUP", 1, 160, 20, 2038);
CGameMenuItemKeyList itemKeyList("", 3, 56, 40, 200, 16, NUMGAMEFUNCTIONS, 0);

CGameMenuItemTitle itemSaveTitle("Save Game", 1, 160, 20, 2038);
CGameMenuItemZEditBitmap itemSaveGame1(NULL, 3, 20, 60, 320, strRestoreGameStrings[0], 16, 1, SaveGame, 0);
CGameMenuItemZEditBitmap itemSaveGame2(NULL, 3, 20, 70, 320, strRestoreGameStrings[1], 16, 1, SaveGame, 1);
CGameMenuItemZEditBitmap itemSaveGame3(NULL, 3, 20, 80, 320, strRestoreGameStrings[2], 16, 1, SaveGame, 2);
CGameMenuItemZEditBitmap itemSaveGame4(NULL, 3, 20, 90, 320, strRestoreGameStrings[3], 16, 1, SaveGame, 3);
CGameMenuItemZEditBitmap itemSaveGame5(NULL, 3, 20, 100, 320, strRestoreGameStrings[4], 16, 1, SaveGame, 4);
CGameMenuItemZEditBitmap itemSaveGame6(NULL, 3, 20, 110, 320, strRestoreGameStrings[5], 16, 1, SaveGame, 5);
CGameMenuItemZEditBitmap itemSaveGame7(NULL, 3, 20, 120, 320, strRestoreGameStrings[6], 16, 1, SaveGame, 6);
CGameMenuItemZEditBitmap itemSaveGame8(NULL, 3, 20, 130, 320, strRestoreGameStrings[7], 16, 1, SaveGame, 7);
CGameMenuItemZEditBitmap itemSaveGame9(NULL, 3, 20, 140, 320, strRestoreGameStrings[8], 16, 1, SaveGame, 8);
CGameMenuItemZEditBitmap itemSaveGame10(NULL, 3, 20, 150, 320, strRestoreGameStrings[9], 16, 1, SaveGame, 9);
CGameMenuItemBitmapLS itemSaveGamePic(NULL, 3, 0, 0, 2050);

CGameMenuItemTitle itemLoadTitle("Load Game", 1, 160, 20, 2038);
CGameMenuItemZEditBitmap itemLoadGame1(NULL, 3, 20, 60, 320, strRestoreGameStrings[0], 16, 1, LoadGame, 0);
CGameMenuItemZEditBitmap itemLoadGame2(NULL, 3, 20, 70, 320, strRestoreGameStrings[1], 16, 1, LoadGame, 1);
CGameMenuItemZEditBitmap itemLoadGame3(NULL, 3, 20, 80, 320, strRestoreGameStrings[2], 16, 1, LoadGame, 2);
CGameMenuItemZEditBitmap itemLoadGame4(NULL, 3, 20, 90, 320, strRestoreGameStrings[3], 16, 1, LoadGame, 3);
CGameMenuItemZEditBitmap itemLoadGame5(NULL, 3, 20, 100, 320, strRestoreGameStrings[4], 16, 1, LoadGame, 4);
CGameMenuItemZEditBitmap itemLoadGame6(NULL, 3, 20, 110, 320, strRestoreGameStrings[5], 16, 1, LoadGame, 5);
CGameMenuItemZEditBitmap itemLoadGame7(NULL, 3, 20, 120, 320, strRestoreGameStrings[6], 16, 1, LoadGame, 6);
CGameMenuItemZEditBitmap itemLoadGame8(NULL, 3, 20, 130, 320, strRestoreGameStrings[7], 16, 1, LoadGame, 7);
CGameMenuItemZEditBitmap itemLoadGame9(NULL, 3, 20, 140, 320, strRestoreGameStrings[8], 16, 1, LoadGame, 8);
CGameMenuItemZEditBitmap itemLoadGame10(NULL, 3, 20, 150, 320, strRestoreGameStrings[9], 16, 1, LoadGame, 9);
CGameMenuItemBitmapLS itemLoadGamePic(NULL, 3, 0, 0, 2518);

CGameMenu menuMultiUserMaps;

CGameMenuItemTitle itemNetStartUserMapTitle("USER MAP", 1, 160, 20, 2038);
CGameMenuFileSelect menuMultiUserMap("", 3, 0, 0, 0, "./", "*.map", zUserMapName);

CGameMenuItemMultiplayerTitle itemNetStartTitle("MULTIPLAYER", 1, 160, 20, 2038);
CGameMenuItemZCycle itemNetStart1("GAME:", 3, 66, 60, 180, 0, 0, zNetGameTypes, 3, 0);
CGameMenuItemZCycle itemNetStart2("EPISODE:", 3, 66, 70, 180, 0, SetupNetLevels, NULL, 0, 0);
CGameMenuItemZCycle itemNetStart3("LEVEL:", 3, 66, 80, 180, 0, ClearUserMapNameOnLevelChange, NULL, 0, 0);
CGameMenuItemZCycle itemNetStart4("DIFFICULTY:", 3, 66, 90, 180, 0, 0, zDiffStrings, 5, 0);
CGameMenuItemZCycle itemNetStart5("MONSTERS:", 3, 66, 100, 180, 0, 0, zMonsterStrings, 3, 0);
CGameMenuItemZCycle itemNetStart6("WEAPONS:", 3, 66, 110, 180, 0, 0, zWeaponStrings, 4, 0);
CGameMenuItemZCycle itemNetStart7("ITEMS:", 3, 66, 120, 180, 0, 0, zItemStrings, 3, 0);
CGameMenuItemZBool itemNetStart8("FRIENDLY FIRE:", 3, 66, 130, 180, true, 0, NULL, NULL);
CGameMenuItemZCycle itemNetStart9("PLAYER KEYS:", 3, 66, 140, 180, 0, 0, zPlayerKeysStrings, 3, 0);
CGameMenuItemZBool itemNetStart10("V1.0x WEAPONS BALANCE:", 3, 66, 150, 180, false, 0, NULL, NULL);
CGameMenuItemChain itemNetStart11("USER MAP", 3, 66, 160, 180, 0, &menuMultiUserMaps, 0, NULL, 0);
CGameMenuItemChain itemNetStart12("START GAME", 1, 66, 175, 280, 0, 0, -1, StartNetGame, 0);

CGameMenuItemText itemLoadingText("LOADING...", 1, 160, 100, 1);

CGameMenuItemTitle itemSoundsTitle("SOUNDS", 1, 160, 20, 2038);
CGameMenuItemSlider itemSoundsMusic("MUSIC:", 3, 40, 60, 180, MusicVolume, 0, 255, 48, SetMusicVol, -1, -1);
CGameMenuItemSlider itemSoundsSound("SOUND:", 3, 40, 70, 180, FXVolume, 0, 255, 48, SetSoundVol, -1, -1);
CGameMenuItemSlider itemSoundsCDAudio("CD AUDIO:", 3, 40, 80, 180, CDVolume, 0, 255, 48, SetCDVol, -1, -1);
CGameMenuItemZBool itemSounds3DAudio("3D SOUND:", 3, 40, 90, 180, gStereo, SetMonoStereo, NULL, NULL);

CGameMenuItemTitle itemQuitTitle("QUIT", 1, 160, 20, 2038);
CGameMenuItemText itemQuitText1("Do you really want to quit?", 0, 160, 100, 1);
CGameMenuItemYesNoQuit itemQuitYesNo("[Y/N]", 0, 20, 110, 280, 1, 0);

CGameMenuItemTitle itemRestartTitle("RESTART GAME", 1, 160, 20, 2038);
CGameMenuItemText itemRestartText1("Do you really want to restart game?", 0, 160, 100, 1);
CGameMenuItemYesNoQuit itemRestartYesNo("[Y/N]", 0, 20, 110, 280, 1, 1);

CGameMenuItemPicCycle itemCreditsPicCycle(0, 0, NULL, NULL, 0, 0);
CGameMenuItemPicCycle itemOrderPicCycle(0, 0, NULL, NULL, 0, 0);

CGameMenuItemTitle itemParentalLockTitle("PARENTAL LOCK", 1, 160, 20, 2038);
CGameMenuItemZBool itemParentalLockToggle("LOCK:", 3, 66, 70, 180, 0, SetParentalLock, NULL, NULL);
CGameMenuItemPassword itemParentalLockPassword("SET PASSWORD:", 3, 160, 80);

CGameMenuItemPicCycle itemSorryPicCycle(0, 0, NULL, NULL, 0, 0);
CGameMenuItemText itemSorryText1("Loading and saving games", 0, 160, 90, 1);
CGameMenuItemText itemSorryText2("not supported", 0, 160, 100, 1);
CGameMenuItemText itemSorryText3("in this demo version of Blood.", 0, 160, 110, 1);

CGameMenuItemText itemSorry2Text1("Buy the complete version of", 0, 160, 90, 1);
CGameMenuItemText itemSorry2Text2("Blood for three new episodes", 0, 160, 100, 1);
CGameMenuItemText itemSorry2Text3("plus eight BloodBath-only levels!", 0, 160, 110, 1);

CGameMenuItemTitle unk_26E06C(" ONLINE ", 1, 160, 20, 2038);
CGameMenuItem7EA1C unk_26E090("DWANGO", 1, 0, 45, 320, "matt", "DWANGO", 1, -1, NULL, 0);
CGameMenuItem7EA1C unk_26E0E8("RTIME", 1, 0, 65, 320, "matt", "RTIME", 1, -1, NULL, 0);
CGameMenuItem7EA1C unk_26E140("HEAT", 1, 0, 85, 320, "matt", "HEAT", 1, -1, NULL, 0);
CGameMenuItem7EA1C unk_26E198("KALI", 1, 0, 105, 320, "matt", "KALI", 1, -1, NULL, 0);
CGameMenuItem7EA1C unk_26E1F0("MPATH", 1, 0, 125, 320, "matt", "MPATH", 1, -1, NULL, 0);
CGameMenuItem7EA1C unk_26E248("TEN", 1, 0, 145, 320, "matt", "TEN", 1, -1, TenProcess, 0);


// static int32_t newresolution, newrendermode, newfullscreen, newvsync;

enum resflags_t {
    RES_FS = 0x1,
    RES_WIN = 0x2,
};

#define MAXRESOLUTIONSTRINGLENGTH 19

struct resolution_t {
    int32_t xdim, ydim;
    int32_t flags;
    int32_t bppmax;
    char name[MAXRESOLUTIONSTRINGLENGTH];
};

resolution_t gResolution[MAXVALIDMODES];
int gResolutionNum;
const char *gResolutionName[MAXVALIDMODES];

CGameMenu menuOptions;
CGameMenu menuOptionsGame;
CGameMenu menuOptionsDisplay;
CGameMenu menuOptionsDisplayColor;
CGameMenu menuOptionsDisplayMode;
#ifdef USE_OPENGL
CGameMenu menuOptionsDisplayPolymost;
#endif
CGameMenu menuOptionsSound;
CGameMenu menuOptionsPlayer;
CGameMenu menuOptionsControl;

void SetupOptionsSound(CGameMenuItemChain *pItem);
void SetupPollJoystick(CGameMenuItemChain *pItem);

CGameMenuItemTitle itemOptionsTitle("OPTIONS", 1, 160, 20, 2038);
CGameMenuItemChain itemOptionsChainGame("GAME SETUP", 1, 0, 50, 320, 1, &menuOptionsGame, -1, NULL, 0);
CGameMenuItemChain itemOptionsChainDisplay("DISPLAY SETUP", 1, 0, 70, 320, 1, &menuOptionsDisplay, -1, NULL, 0);
CGameMenuItemChain itemOptionsChainSound("SOUND SETUP", 1, 0, 90, 320, 1, &menuOptionsSound, -1, SetupOptionsSound, 0);
CGameMenuItemChain itemOptionsChainPlayer("PLAYER SETUP", 1, 0, 110, 320, 1, &menuOptionsPlayer, -1, NULL, 0);
CGameMenuItemChain itemOptionsChainControl("CONTROL SETUP", 1, 0, 130, 320, 1, &menuOptionsControl, -1, SetupPollJoystick, 0);
CGameMenuItemChain itemOptionsChainOld("OLD MENU", 1, 0, 170, 320, 1, &menuOptionsOld, -1, NULL, 0);

const char *pzAutoAimStrings[] = {
    "NEVER",
    "ALWAYS",
    "HITSCAN ONLY",
    "PITCHFORK ONLY"
};

const char *pzWeaponSwitchStrings[] = {
    "NEVER",
    "IF NEW",
    "BY RATING"
};

const char *pzLevelStatsStrings[] = {
    "OFF",
    "ON",
    "AUTOMAP ONLY"
};

void SetAutoAim(CGameMenuItemZCycle *);
void SetAutoRun(CGameMenuItemZBool *);
void SetLevelStats(CGameMenuItemZCycle *);
void SetPowerupDuration(CGameMenuItemZBool *);
void SetShowMapTitle(CGameMenuItemZBool*);
void SetWeaponSwitch(CGameMenuItemZCycle *pItem);

CGameMenuItemTitle itemOptionsGameTitle("GAME SETUP", 1, 160, 20, 2038);

CGameMenuItemZBool itemOptionsGameBoolShowPlayerNames("SHOW PLAYER NAMES:", 3, 66, 60, 180, gShowPlayerNames, SetShowPlayerNames, NULL, NULL);
CGameMenuItemZCycle itemOptionsGameShowWeapons("SHOW WEAPONS:", 3, 66, 70, 180, 0, SetShowWeapons, pzShowWeaponStrings, ARRAY_SSIZE(pzShowWeaponStrings), 0);
CGameMenuItemZBool itemOptionsGameBoolSlopeTilting("SLOPE TILTING:", 3, 66, 80, 180, gSlopeTilting, SetSlopeTilting, NULL, NULL);
CGameMenuItemZBool itemOptionsGameBoolViewBobbing("VIEW BOBBING:", 3, 66, 90, 180, gViewVBobbing, SetViewBobbing, NULL, NULL);
CGameMenuItemZBool itemOptionsGameBoolViewSwaying("VIEW SWAYING:", 3, 66, 100, 180, gViewHBobbing, SetViewSwaying, NULL, NULL);
CGameMenuItemZCycle itemOptionsGameCycleAutoAim("AUTO AIM:", 3, 66, 110, 180, 0, SetAutoAim, pzAutoAimStrings, ARRAY_SSIZE(pzAutoAimStrings), 0);
CGameMenuItemZBool itemOptionsGameBoolAutoRun("AUTO RUN:", 3, 66, 120, 180, gAutoRun, SetAutoRun, NULL, NULL);
CGameMenuItemZCycle itemOptionsGameWeaponSwitch("EQUIP PICKUPS:", 3, 66, 130, 180, 0, SetWeaponSwitch, pzWeaponSwitchStrings, ARRAY_SSIZE(pzWeaponSwitchStrings), 0);
//CGameMenuItemChain itemOptionsGameChainParentalLock("PARENTAL LOCK", 3, 0, 140, 320, 1, &menuParentalLock, -1, NULL, 0);

///////////////
CGameMenuItemZBool itemOptionsGameBoolWeaponsV10X("V1.0x WEAPONS BALANCE:", 3, 66, 140, 180, gWeaponsV10x, SetWeaponsV10X, NULL, NULL);
CGameMenuItemZBool itemOptionsGameBoolVanillaMode("STRICT VANILLA MODE:", 3, 66, 150, 180, gVanilla, SetVanillaMode, NULL, NULL);
///////////////////

CGameMenuItemTitle itemOptionsDisplayTitle("DISPLAY SETUP", 1, 160, 20, 2038);
CGameMenuItemChain itemOptionsDisplayColor("COLOR CORRECTION", 3, 66, 60, 180, 0, &menuOptionsDisplayColor, -1, NULL, 0);
CGameMenuItemChain itemOptionsDisplayMode("VIDEO MODE", 3, 66, 70, 180, 0, &menuOptionsDisplayMode, -1, SetupVideoModeMenu, 0);
CGameMenuItemSlider itemOptionsDisplaySliderDetail("DETAIL:", 3, 66, 80, 180, &gDetail, 0, 4, 1, SetDetail, -1, -1);
CGameMenuItemZBool itemOptionsDisplayBoolVoxels("VOXELS:", 3, 66, 90, 180, 0, SetVoxels, NULL, NULL);
CGameMenuItemZBool itemOptionsDisplayBoolCrosshair("CROSSHAIR:", 3, 66, 100, 180, gAimReticle, SetCrosshair, NULL, NULL);
CGameMenuItemZBool itemOptionsDisplayBoolCenterHoriz("CENTER HORIZON LINE:", 3, 66, 110, 180, gCenterHoriz, SetCenterHoriz, NULL, NULL);
CGameMenuItemZCycle itemOptionsDisplayCycleLevelStats("LEVEL STATS:", 3, 66, 120, 180, gLevelStats, SetLevelStats, pzLevelStatsStrings, ARRAY_SSIZE(pzLevelStatsStrings), 0);
CGameMenuItemZBool itemOptionsDisplayBoolPowerupDuration("POWERUP DURATION:", 3, 66, 130, 180, gPowerupDuration, SetPowerupDuration, NULL, NULL);
CGameMenuItemZBool itemOptionsDisplayBoolShowMapTitle("MAP TITLE:", 3, 66, 140, 180, gShowMapTitle, SetShowMapTitle, NULL, NULL);
CGameMenuItemZBool itemOptionsDisplayBoolMessages("MESSAGES:", 3, 66, 150, 180, gMessageState, SetMessages, NULL, NULL);
CGameMenuItemZBool itemOptionsDisplayBoolWidescreen("WIDESCREEN:", 3, 66, 160, 180, r_usenewaspect, SetWidescreen, NULL, NULL);
CGameMenuItemSlider itemOptionsDisplayFOV("FOV:", 3, 66, 170, 180, &gFov, 75, 140, 5, SetFOV, -1, -1, kMenuSliderValue);
#ifdef USE_OPENGL
CGameMenuItemChain itemOptionsDisplayPolymost("POLYMOST SETUP", 3, 66, 180, 180, 0, &menuOptionsDisplayPolymost, -1, SetupVideoPolymostMenu, 0);
#endif

const char *pzRendererStrings[] = {
    "CLASSIC",
    "POLYMOST"
};

const int nRendererValues[] = {
    REND_CLASSIC,
    REND_POLYMOST
};

const char *pzVSyncStrings[] = {
    "ADAPTIVE",
    "OFF",
    "ON",
#if defined USE_OPENGL && defined _WIN32 && defined RENDERTYPESDL
    "KMT"
#endif
};

const int nVSyncValues[] = {
    -1,
    0,
    1,
#if defined USE_OPENGL && defined _WIN32 && defined RENDERTYPESDL
    2
#endif
};

const char *pzFrameLimitStrings[] = {
    "30 FPS",
    "60 FPS",
    "75 FPS",
    "100 FPS",
    "120 FPS",
    "144 FPS",
    "165 FPS",
    "240 FPS"
};

const int nFrameLimitValues[] = {
    30,
    60,
    75,
    100,
    120,
    144,
    165,
    240
};


void PreDrawVideoModeMenu(CGameMenuItem *);

CGameMenuItemTitle itemOptionsDisplayModeTitle("VIDEO MODE", 1, 160, 20, 2038);
CGameMenuItemZCycle itemOptionsDisplayModeResolution("RESOLUTION:", 3, 66, 60, 180, 0, NULL, NULL, 0, 0, true);
CGameMenuItemZCycle itemOptionsDisplayModeRenderer("RENDERER:", 3, 66, 70, 180, 0, NULL, pzRendererStrings, 2, 0);
CGameMenuItemZBool itemOptionsDisplayModeFullscreen("FULLSCREEN:", 3, 66, 80, 180, 0, NULL, NULL, NULL);
CGameMenuItemZCycle itemOptionsDisplayModeVSync("VSYNC:", 3, 66, 90, 180, 0, NULL, pzVSyncStrings, ARRAY_SSIZE(pzVSyncStrings), 0);
CGameMenuItemZCycle itemOptionsDisplayModeFrameLimit("FRAMERATE LIMIT:", 3, 66, 100, 180, 0, UpdateVideoModeMenuFrameLimit, pzFrameLimitStrings, 8, 0);
CGameMenuItemChain itemOptionsDisplayModeApply("APPLY CHANGES", 3, 66, 125, 180, 0, NULL, 0, SetVideoMode, 0);

void PreDrawDisplayColor(CGameMenuItem *);

CGameMenuItemTitle itemOptionsDisplayColorTitle("COLOR CORRECTION", 1, 160, 20, -1);
CGameMenuItemSliderFloat itemOptionsDisplayColorGamma("GAMMA:", 3, 66, 140, 180, &g_videoGamma, MIN_GAMMA, MAX_GAMMA, 0.1f, UpdateVideoColorMenu, -1, -1, kMenuSliderValue);
CGameMenuItemSliderFloat itemOptionsDisplayColorContrast("CONTRAST:", 3, 66, 150, 180, &g_videoContrast, MIN_CONTRAST, MAX_CONTRAST, 0.05f, UpdateVideoColorMenu, -1, -1, kMenuSliderValue);
CGameMenuItemSliderFloat itemOptionsDisplayColorSaturation("SATURATION:", 3, 66, 160, 180, &g_videoSaturation, MIN_SATURATION, MAX_SATURATION, 0.05f, UpdateVideoColorMenu, -1, -1, kMenuSliderValue);
CGameMenuItemSliderFloat itemOptionsDisplayColorVisibility("VISIBILITY:", 3, 66, 170, 180, &r_ambientlight, 0.125f, 4.f, 0.125f, UpdateVideoColorMenu, -1, -1, kMenuSliderValue);
CGameMenuItemChain itemOptionsDisplayColorReset("RESET TO DEFAULTS", 3, 66, 180, 180, 0, NULL, 0, ResetVideoColor, 0);

#ifdef USE_OPENGL
const char *pzTextureModeStrings[] = {
    "CLASSIC",
    "FILTERED"
};

int nTextureModeValues[] = {
    TEXFILTER_OFF,
    TEXFILTER_ON
};
#endif

const char *pzAnisotropyStrings[] = {
    "MAX",
    "NONE",
    "2X",
    "4X",
    "8X",
    "16X"
};

int nAnisotropyValues[] = {
    0,
    1,
    2,
    4,
    8,
    16
};

const char *pzTexQualityStrings[] = {
    "FULL",
    "HALF",
    "BARF"
};

const char *pzTexCacheStrings[] = {
    "OFF",
    "ON",
    "COMPRESSED"
};

void UpdateTextureMode(CGameMenuItemZCycle *pItem);
void UpdateAnisotropy(CGameMenuItemZCycle *pItem);
void UpdateTrueColorTextures(CGameMenuItemZBool *pItem);
void UpdateTexQuality(CGameMenuItemZCycle *pItem);
void UpdatePreloadCache(CGameMenuItemZBool *pItem);
void UpdateTexCache(CGameMenuItemZCycle *pItem);
void UpdateDetailTex(CGameMenuItemZBool *pItem);
void UpdateGlowTex(CGameMenuItemZBool *pItem);
void Update3DModels(CGameMenuItemZBool *pItem);
void UpdateDeliriumBlur(CGameMenuItemZBool *pItem);
void UpdateYShrearing(CGameMenuItemZBool *pItem);
#ifdef USE_OPENGL
void PreDrawDisplayPolymost(CGameMenuItem *pItem);
CGameMenuItemTitle itemOptionsDisplayPolymostTitle("POLYMOST SETUP", 1, 160, 20, 2038);
CGameMenuItemZCycle itemOptionsDisplayPolymostTextureMode("TEXTURE MODE:", 3, 66, 60, 180, 0, UpdateTextureMode, pzTextureModeStrings, 2, 0);
CGameMenuItemZCycle itemOptionsDisplayPolymostAnisotropy("ANISOTROPY:", 3, 66, 70, 180, 0, UpdateAnisotropy, pzAnisotropyStrings, 6, 0);
CGameMenuItemZBool itemOptionsDisplayPolymostTrueColorTextures("TRUE COLOR TEXTURES:", 3, 66, 80, 180, 0, UpdateTrueColorTextures, NULL, NULL);
CGameMenuItemZCycle itemOptionsDisplayPolymostTexQuality("GL TEXTURE QUALITY:", 3, 66, 90, 180, 0, UpdateTexQuality, pzTexQualityStrings, 3, 0);
CGameMenuItemZBool itemOptionsDisplayPolymostPreloadCache("PRE-LOAD MAP TEXTURES:", 3, 66, 100, 180, 0, UpdatePreloadCache, NULL, NULL);
CGameMenuItemZCycle itemOptionsDisplayPolymostTexCache("ON-DISK TEXTURE CACHE:", 3, 66, 110, 180, 0, UpdateTexCache, pzTexCacheStrings, 3, 0);
CGameMenuItemZBool itemOptionsDisplayPolymostDetailTex("DETAIL TEXTURES:", 3, 66, 120, 180, 0, UpdateDetailTex, NULL, NULL);
CGameMenuItemZBool itemOptionsDisplayPolymostGlowTex("GLOW TEXTURES:", 3, 66, 130, 180, 0, UpdateGlowTex, NULL, NULL);
CGameMenuItemZBool itemOptionsDisplayPolymost3DModels("3D MODELS:", 3, 66, 140, 180, 0, Update3DModels, NULL, NULL);
CGameMenuItemZBool itemOptionsDisplayPolymostDeliriumBlur("DELIRIUM EFFECT BLUR:", 3, 66, 150, 180, 0, UpdateDeliriumBlur, NULL, NULL);
CGameMenuItemZBool itemOptionsDisplayPolymostYShearing("Y-SHEARING:", 3, 66, 160, 180, 0, UpdateYShrearing, NULL, NULL);
#endif

void UpdateSoundToggle(CGameMenuItemZBool *pItem);
void UpdateMusicToggle(CGameMenuItemZBool *pItem);
void Update3DToggle(CGameMenuItemZBool *pItem);
void UpdateCDToggle(CGameMenuItemZBool *pItem);
void UpdateSoundVolume(CGameMenuItemSlider *pItem);
void UpdateMusicVolume(CGameMenuItemSlider *pItem);
void UpdateSoundRate(CGameMenuItemZCycle *pItem);
void UpdateNumVoices(CGameMenuItemSlider *pItem);
void UpdateMusicDevice(CGameMenuItemZCycle *pItem);
void SetSound(CGameMenuItemChain *pItem);
void PreDrawSound(CGameMenuItem *pItem);
const char *pzSoundRateStrings[] = {
    "22050HZ",
    "44100HZ",
    "48000HZ"
};

int nSoundRateValues[] = {
    22050,
    44100,
    48000
};

int nMusicDeviceValues[] = {
    ASS_OPL3,
#ifdef _WIN32
    ASS_WinMM,
#endif
    ASS_SF2,
};

const char *pzMusicDeviceStrings[] = {
    "OPL3(SB/ADLIB)",
#ifdef _WIN32
    "SYSTEM MIDI",
#endif
    ".SF2 SYNTH",
};
static char sf2bankfile[BMAX_PATH];

CGameMenu menuOptionsSoundSF2;

CGameMenuItemTitle itemOptionsSoundSF2Title("SELECT SF2 BANK", 1, 160, 20, 2038);
CGameMenuFileSelect itemOptionsSoundSF2FS("", 3, 0, 0, 0, "./", "*.sf2", sf2bankfile);

CGameMenuItemTitle itemOptionsSoundTitle("SOUND SETUP", 1, 160, 20, 2038);
CGameMenuItemZBool itemOptionsSoundSoundToggle("SOUND:", 3, 66, 60, 180, false, UpdateSoundToggle, NULL, NULL);
CGameMenuItemZBool itemOptionsSoundMusicToggle("MUSIC:", 3, 66, 70, 180, false, UpdateMusicToggle, NULL, NULL);
CGameMenuItemZBool itemOptionsSound3DToggle("3D AUDIO:", 3, 66, 80, 180, false, Update3DToggle, NULL, NULL);
CGameMenuItemSlider itemOptionsSoundSoundVolume("SOUND VOLUME:", 3, 66, 90, 180, &FXVolume, 0, 256, 48, UpdateSoundVolume, -1, -1, kMenuSliderPercent);
CGameMenuItemSlider itemOptionsSoundMusicVolume("MUSIC VOLUME:", 3, 66, 100, 180, &MusicVolume, 0, 256, 48, UpdateMusicVolume, -1, -1, kMenuSliderPercent);
CGameMenuItemZCycle itemOptionsSoundSampleRate("SAMPLE RATE:", 3, 66, 110, 180, 0, UpdateSoundRate, pzSoundRateStrings, 3, 0);
CGameMenuItemSlider itemOptionsSoundNumVoices("VOICES:", 3, 66, 120, 180, NumVoices, 16, 255, 16, UpdateNumVoices, -1, -1, kMenuSliderValue);
CGameMenuItemZBool itemOptionsSoundCDToggle("REDBOOK AUDIO:", 3, 66, 130, 180, false, UpdateCDToggle, NULL, NULL);
CGameMenuItemZCycle itemOptionsSoundMusicDevice("MIDI DRIVER:", 3, 66, 140, 180, 0, UpdateMusicDevice, pzMusicDeviceStrings, ARRAY_SIZE(pzMusicDeviceStrings), 0);
CGameMenuItemChain itemOptionsSoundSF2Bank("SF2 BANK", 3, 66, 150, 180, 0, &menuOptionsSoundSF2, 0, NULL, 0);
CGameMenuItemChain itemOptionsSoundApplyChanges("APPLY CHANGES", 3, 66, 160, 180, 0, NULL, 0, SetSound, 0);


void UpdatePlayerName(CGameMenuItemZEdit *pItem, CGameMenuEvent *pEvent);

CGameMenuItemTitle itemOptionsPlayerTitle("PLAYER SETUP", 1, 160, 20, 2038);
CGameMenuItemZEdit itemOptionsPlayerName("PLAYER NAME:", 3, 66, 60, 180, szPlayerName, MAXPLAYERNAME, 0, UpdatePlayerName, 0);

#define JOYSTICKITEMSPERPAGE 16 // this must be an even value, as double tap inputs rely on odd index position
#define MAXJOYSTICKBUTTONPAGES (max(1, (MAXJOYBUTTONSANDHATS*2 / JOYSTICKITEMSPERPAGE))) // we double all buttons/hats so each input can be bind for double tap

CGameMenu menuOptionsControlKeyboard;
CGameMenu menuOptionsControlMouse;
CGameMenu menuOptionsControlMouseButtonAssignment;
CGameMenu menuOptionsControlJoystickButtonAssignment[MAXJOYSTICKBUTTONPAGES];
CGameMenu menuOptionsControlJoystickListAxes; // contains list of editable joystick axes
CGameMenu menuOptionsControlJoystickAxis[MAXJOYAXES]; // options menu for each joystick axis
CGameMenu menuOptionsControlJoystickMisc;

void SetupMouseMenu(CGameMenuItemChain *pItem);
void SetupJoystickButtonsMenu(CGameMenuItemChain *pItem);
void SetupJoystickAxesMenu(CGameMenuItemChain *pItem);
void SetJoystickScale(CGameMenuItemSlider* pItem);
void SetJoystickAnalogue(CGameMenuItemZCycle* pItem);
void SetJoystickAnalogueInvert(CGameMenuItemZBool* pItem);
void SetJoystickDigitalPos(CGameMenuItemZCycle* pItem);
void SetJoystickDigitalNeg(CGameMenuItemZCycle* pItem);
void SetJoystickDeadzone(CGameMenuItemSlider* pItem);
void SetJoystickSaturate(CGameMenuItemSlider* pItem);

const char *pzTurnAccelerationStrings[] = {
    "OFF",
    "ON RUNNING",
    "ALWAYS ON",
};

CGameMenuItemTitle itemOptionsControlTitle("CONTROL SETUP", 1, 160, 20, 2038);
CGameMenuItemChain itemOptionsControlKeyboard("KEYBOARD SETUP", 1, 0, 60, 320, 1, &menuOptionsControlKeyboard, -1, NULL, 0);
CGameMenuItemChain itemOptionsControlMouse("MOUSE SETUP", 1, 0, 80, 320, 1, &menuOptionsControlMouse, -1, SetupMouseMenu, 0);
CGameMenuItemChain itemOptionsControlJoystickButtons("JOYSTICK BUTTONS SETUP", 1, 0, 120, 320, 1, &menuOptionsControlJoystickButtonAssignment[0], -1, SetupJoystickButtonsMenu, 0);
CGameMenuItemChain itemOptionsControlJoystickAxes("JOYSTICK AXES SETUP", 1, 0, 140, 320, 1, &menuOptionsControlJoystickListAxes, -1, SetupJoystickAxesMenu, 0);
CGameMenuItemChain itemOptionsControlJoystickMisc("JOYSTICK MISC SETUP", 1, 0, 160, 320, 1, &menuOptionsControlJoystickMisc, -1, NULL, 0);

CGameMenuItemTitle itemOptionsControlKeyboardTitle("KEYBOARD SETUP", 1, 160, 20, 2038);
CGameMenuItemSlider itemOptionsControlKeyboardSliderTurnSpeed("Key Turn Speed:", 1, 18, 50, 280, &gTurnSpeed, 64, 128, 4, SetTurnSpeed, -1, -1);
CGameMenuItemZCycle itemOptionsControlKeyboardCycleTurnAcceleration("Key Turn Acceleration:", 1, 18, 70, 280, 0, SetTurnAcceleration, pzTurnAccelerationStrings, ARRAY_SIZE(pzTurnAccelerationStrings), 0);
CGameMenuItemChain itemOptionsControlKeyboardList("Configure Keys...", 1, 0, 110, 320, 1, &menuKeys, -1, NULL, 0);
CGameMenuItemChain itemOptionsControlKeyboardReset("Reset Keys (default)...", 1, 0, 135, 320, 1, &menuKeys, -1, ResetKeys, 0);
CGameMenuItemChain itemOptionsControlKeyboardResetClassic("Reset Keys (classic)...", 1, 0, 155, 320, 1, &menuKeys, -1, ResetKeysClassic, 0);

void SetMouseAimMode(CGameMenuItemZBool *pItem);
void SetMouseVerticalAim(CGameMenuItemZBool *pItem);
void SetMouseXSensitivity(CGameMenuItemSliderFloat *pItem);
void SetMouseYSensitivity(CGameMenuItemSliderFloat*pItem);

void PreDrawControlMouse(CGameMenuItem *pItem);
void SetMouseButton(CGameMenuItemZCycle *pItem);
void SetJoyButton(CGameMenuItemZCycle *pItem);

void SetupMouseButtonMenu(CGameMenuItemChain *pItem);

CGameMenuItemTitle itemOptionsControlMouseTitle("MOUSE SETUP", 1, 160, 20, 2038);
CGameMenuItemChain itemOptionsControlMouseButton("BUTTON ASSIGNMENT", 3, 66, 60, 180, 0, &menuOptionsControlMouseButtonAssignment, 0, SetupMouseButtonMenu, 0);
CGameMenuItemSliderFloat itemOptionsControlMouseSensitivity("SENSITIVITY:", 3, 66, 70, 180, &CONTROL_MouseSensitivity, 1.f, 100.f, 1.f, SetMouseSensitivity, -1, -1, kMenuSliderValue);
CGameMenuItemZBool itemOptionsControlMouseAimFlipped("INVERT AIMING:", 3, 66, 80, 180, false, SetMouseAimFlipped, NULL, NULL);
CGameMenuItemZBool itemOptionsControlMouseAimMode("AIMING TYPE:", 3, 66, 90, 180, false, SetMouseAimMode, "HOLD", "TOGGLE");
CGameMenuItemZBool itemOptionsControlMouseVerticalAim("VERTICAL AIMING:", 3, 66, 100, 180, false, SetMouseVerticalAim, NULL, NULL);
CGameMenuItemSliderFloat itemOptionsControlMouseXSensitivity("HORIZ SENS:", 3, 66, 110, 180, &CONTROL_MouseAxesSensitivity[0], 0.f, 100.f, 1.f, SetMouseXSensitivity, -1, -1, kMenuSliderValue);
CGameMenuItemSliderFloat itemOptionsControlMouseYSensitivity("VERT SENS:", 3, 66, 120, 180, &CONTROL_MouseAxesSensitivity[1], 0.f, 100.f, 1.f, SetMouseYSensitivity, -1, -1, kMenuSliderValue);

void SetupNetworkMenu(void);
void SetupNetworkHostMenu(CGameMenuItemChain *pItem);
void SetupNetworkJoinMenu(CGameMenuItemChain *pItem);
void NetworkHostGame(CGameMenuItemChain *pItem);
void NetworkJoinGame(CGameMenuItemChain *pItem);

char zNetAddressBuffer[16] = "localhost";
char zNetPortBuffer[6] = "23513";

CGameMenuItemTitle itemNetworkTitle("MULTIPLAYER", 1, 160, 20, 2038);
CGameMenuItemChain itemNetworkHost("HOST A GAME", 1, 0, 80, 320, 1, &menuNetworkHost, -1, SetupNetworkHostMenu, 0);
CGameMenuItemChain itemNetworkJoin("JOIN A GAME", 1, 0, 100, 320, 1, &menuNetworkJoin, -1, SetupNetworkJoinMenu, 0);

CGameMenuItemTitle itemNetworkHostTitle("HOST A GAME", 1, 160, 20, 2038);
CGameMenuItemSlider itemNetworkHostPlayerNum("PLAYER NUMBER:", 3, 66, 70, 180, 1, 2, kMaxPlayers, 1, NULL, -1, -1, kMenuSliderValue);
CGameMenuItemZEdit itemNetworkHostPort("NETWORK PORT:", 3, 66, 80, 180, zNetPortBuffer, 6, 0, NULL, 0);
CGameMenuItemChain itemNetworkHostHost("HOST A GAME", 3, 66, 100, 180, 1, NULL, -1, NetworkHostGame, 0);

CGameMenuItemTitle itemNetworkJoinTitle("JOIN A GAME", 1, 160, 20, 2038);
CGameMenuItemZEdit itemNetworkJoinAddress("NETWORK ADDRESS:", 3, 66, 70, 180, zNetAddressBuffer, 16, 0, NULL, 0);
CGameMenuItemZEdit itemNetworkJoinPort("NETWORK PORT:", 3, 66, 80, 180, zNetPortBuffer, 6, 0, NULL, 0);
CGameMenuItemChain itemNetworkJoinJoin("JOIN A GAME", 3, 66, 100, 180, 1, NULL, -1, NetworkJoinGame, 0);

// There is no better way to do this than manually.

#define MENUMOUSEFUNCTIONS 12

static char const *MenuMouseNames[MENUMOUSEFUNCTIONS] = {
    "Button 1",
    "Double Button 1",
    "Button 2",
    "Double Button 2",
    "Button 3",
    "Double Button 3",

    "Wheel Up",
    "Wheel Down",

    "Button 4",
    "Double Button 4",
    "Button 5",
    "Double Button 5",
};

static int32_t MenuMouseDataIndex[MENUMOUSEFUNCTIONS][2] = {
    { 0, 0, },
    { 0, 1, },
    { 1, 0, },
    { 1, 1, },
    { 2, 0, },
    { 2, 1, },

    // note the mouse wheel
    { 4, 0, },
    { 5, 0, },

    { 3, 0, },
    { 3, 1, },
    { 6, 0, },
    { 6, 1, },
};

CGameMenuItemZCycle *pItemOptionsControlMouseButton[MENUMOUSEFUNCTIONS];

char MenuJoyButtonNames[MAXJOYBUTTONSANDHATS*2][64] = {""};

const char *zJoystickAnalogue[] =
{
    "-None-",
    "Turning",
    "Strafing",
    "Moving",
    "Look Up/Down",
};

CGameMenuItemTitle itemJoyButtonsTitle("JOYSTICK SETUP", 1, 160, 20, 2038);
CGameMenuItemZCycle *pItemOptionsControlJoyButton[MAXJOYSTICKBUTTONPAGES][JOYSTICKITEMSPERPAGE];
CGameMenuItemChain *pItemOptionsControlJoyButtonNextPage[MAXJOYSTICKBUTTONPAGES];

char MenuJoyAxisNames[MAXJOYAXES][64] = {""};

CGameMenuItemTitle itemJoyAxesTitle("JOYSTICK AXES", 1, 160, 20, 2038);
CGameMenuItemChain *pItemOptionsControlJoystickAxis[MAXJOYAXES]; // dynamic list for each axis

CGameMenuItemTitle *pItemOptionsControlJoystickAxisName[MAXJOYAXES];
CGameMenuItemSlider *pItemOptionsControlJoystickAxisScale[MAXJOYAXES];
CGameMenuItemZCycle *pItemOptionsControlJoystickAxisAnalogue[MAXJOYAXES];
CGameMenuItemZBool *pItemOptionsControlJoystickAxisAnalogueInvert[MAXJOYAXES];
CGameMenuItemZCycle *pItemOptionsControlJoystickAxisDigitalPos[MAXJOYAXES];
CGameMenuItemZCycle *pItemOptionsControlJoystickAxisDigitalNeg[MAXJOYAXES];
CGameMenuItemSlider *pItemOptionsControlJoystickAxisDeadzone[MAXJOYAXES];
CGameMenuItemSlider *pItemOptionsControlJoystickAxisSaturate[MAXJOYAXES];

CGameMenuItemTitle itemOptionsControlJoystickMiscTitle("JOYSTICK MISC", 1, 160, 20, 2038);
CGameMenuItemZBool itemOptionsControlJoystickMiscCenterView("CENTER VIEW ON DROP:", 1, 18, 60, 280, gCenterViewOnDrop, SetCenterView, NULL, NULL);
CGameMenuItemZBool itemOptionsControlJoystickMiscRumble("RUMBLE CONTROLLER:", 1, 18, 80, 280, 0, SetJoystickRumble, NULL, NULL);

void SetupLoadingScreen(void)
{
    menuLoading.Add(&itemLoadingText, true);
}

void SetupKeyListMenu(void)
{
    menuKeys.Add(&itemKeysTitle, false);
    menuKeys.Add(&itemKeyList, true);
    menuKeys.Add(&itemBloodQAV, false);
}

void SetupMessagesMenu(void)
{
    menuMessages.Add(&itemMessagesTitle, false);
    menuMessages.Add(&boolMessages, true);
    menuMessages.Add(&sliderMsgCount, false);
    menuMessages.Add(&sliderMsgTime, false);
    menuMessages.Add(&boolMsgFont, false);
    menuMessages.Add(&boolMsgIncoming, false);
    menuMessages.Add(&boolMsgSelf, false);
    menuMessages.Add(&boolMsgOther, false);
    menuMessages.Add(&boolMsgRespawn, false);
    menuMessages.Add(&itemBloodQAV, false);
}

void SetupControlsOldMenu(void)
{
    sliderMouseSpeed.fValue = ClipRangeF(CONTROL_MouseSensitivity, sliderMouseSpeed.fRangeLow, sliderMouseSpeed.fRangeHigh);
    sliderTurnSpeed.nValue = ClipRange(gTurnSpeed, sliderTurnSpeed.nRangeLow, sliderTurnSpeed.nRangeHigh);
    boolMouseFlipped.at20 = gMouseAimingFlipped;
    menuControls.Add(&itemControlsTitle, false);
    menuControls.Add(&sliderMouseSpeed, true);
    menuControls.Add(&boolMouseFlipped, false);
    menuControls.Add(&sliderTurnSpeed, false);
    menuControls.Add(&itemChainKeyList, false);
    menuControls.Add(&itemChainKeyReset, false);
    menuControls.Add(&itemChainKeyResetClassic, false);
    menuControls.Add(&itemBloodQAV, false);
}

void SetupOptionsOldMenu(void)
{
    sliderDetail.nValue = ClipRange(gDetail, sliderDetail.nRangeLow, sliderDetail.nRangeHigh);
    sliderGamma.nValue = ClipRange(gGamma, sliderGamma.nRangeLow, sliderGamma.nRangeHigh);
    sliderMusic.nValue = ClipRange(MusicVolume, sliderMusic.nRangeLow, sliderMusic.nRangeHigh);
    sliderSound.nValue = ClipRange(FXVolume, sliderSound.nRangeLow, sliderSound.nRangeHigh);
    bool3DAudio.at20 = gStereo;
    boolCrosshair.at20 = gAimReticle;
    itemCycleShowWeapons.m_nFocus = gShowWeapon;
    boolSlopeTilting.at20 = gSlopeTilting;
    boolViewBobbing.at20 = gViewVBobbing;
    boolViewSwaying.at20 = gViewHBobbing;
    boolMessages.at20 = gGameMessageMgr.state;
    menuOptionsOld.Add(&itemOptionsTitle, false);
    menuOptionsOld.Add(&itemOption1, true);
    menuOptionsOld.Add(&sliderDetail, false);
    menuOptionsOld.Add(&sliderGamma, false);
    menuOptionsOld.Add(&sliderMusic, false);
    menuOptionsOld.Add(&sliderSound, false);
    menuOptionsOld.Add(&sliderCDAudio, false);
    menuOptionsOld.Add(&bool3DAudio, false);
    menuOptionsOld.Add(&boolCrosshair, false);
    menuOptionsOld.Add(&itemCycleShowWeapons, false);
    menuOptionsOld.Add(&boolSlopeTilting, false);
    menuOptionsOld.Add(&boolViewBobbing, false);
    menuOptionsOld.Add(&boolViewSwaying, false);
    menuOptionsOld.Add(&itemOption2, false);
    menuOptionsOld.Add(&itemChainParentalLock, false);
    menuOptionsOld.Add(&itemBloodQAV, false);
}

void SetupDifficultyMenu(void)
{
    menuDifficulty.Add(&itemDifficultyTitle, false);
    menuDifficulty.Add(&itemDifficulty1, false);
    menuDifficulty.Add(&itemDifficulty2, false);
    menuDifficulty.Add(&itemDifficulty3, true);
    menuDifficulty.Add(&itemDifficulty4, false);
    menuDifficulty.Add(&itemDifficulty5, false);
    menuDifficulty.Add(&itemDifficulty6, false);
    menuDifficulty.Add(&itemBloodQAV, false);

    menuDifficultyCustom.Add(&itemDifficultyCustomTitle, false);
    menuDifficultyCustom.Add(&sliderDifficultyCustomQuantity, true);
    menuDifficultyCustom.Add(&sliderDifficultyCustomHealth, false);
    menuDifficultyCustom.Add(&sliderDifficultyCustomDifficulty, false);
    menuDifficultyCustom.Add(&sliderDifficultyCustomDamage, false);
    menuDifficultyCustom.Add(&itemDifficultyCustomStart, false);
    menuDifficultyCustom.Add(&itemBloodQAV, false);
}

void SetupEpisodeMenu(void)
{
    menuEpisode.Add(&itemEpisodesTitle, false);
    int height;
    gMenuTextMgr.GetFontInfo(1, NULL, NULL, &height);
    int nOffset = 100;
    for (int i = 0; i < gEpisodeCount; i++)
    {
        EPISODEINFO *pEpisode = &gEpisodeInfo[i];
        if (!pEpisode->bloodbath || gGameOptions.nGameType != kGameTypeSinglePlayer)
            nOffset -= 10;
    }
    nOffset = max(min(nOffset, 55), 35);
    int j = 0;
    for (int i = 0; i < gEpisodeCount; i++)
    {
        EPISODEINFO *pEpisode = &gEpisodeInfo[i];
        if (!pEpisode->bloodbath || gGameOptions.nGameType != kGameTypeSinglePlayer)
        {
            if (j >= ARRAY_SSIZE(itemEpisodes))
                ThrowError("Too many ini episodes to display (max %d).\n", ARRAY_SSIZE(itemEpisodes));
            CGameMenuItemChain7F2F0 *pEpisodeItem = &itemEpisodes[j];
            pEpisodeItem->m_nFont = 1;
            pEpisodeItem->m_nX = 0;
            pEpisodeItem->m_nWidth = 320;
            pEpisodeItem->at20 = 1;
            pEpisodeItem->m_pzText = pEpisode->title;
            pEpisodeItem->m_nY = nOffset+(height+8)*j;
            pEpisodeItem->at34 = i;
            pEpisodeItem = &itemEpisodes[j];
            pEpisodeItem->at24 = &menuDifficulty;
            pEpisodeItem->at28 = 3;
            pEpisodeItem = &itemEpisodes[j];
            pEpisodeItem->bCanSelect = 1;
            pEpisodeItem->bEnable = 1;
            bool first = j == 0;
            menuEpisode.Add(&itemEpisodes[j], first);
            if (first)
                SetupLevelMenuItem(j);
            j++;
        }
    }

    if (j < 5) // if menu slots are not all filled, add space for user maps item
        itemUserMap.m_nY = 50+(height+8)*5;
    else
        itemUserMap.m_nY = nOffset+(height+8)*j;
    menuEpisode.Add(&itemUserMap, false);
    menuEpisode.Add(&itemBloodQAV, false);

    menuUserMap.Add(&itemUserMapTitle, true);
    menuUserMap.Add(&itemUserMapList, true);
}

void SetupMainMenu(void)
{
    menuMain.Add(&itemMainTitle, false);
    menuMain.Add(&itemMain1, true);
    if (gGameOptions.nGameType != kGameTypeSinglePlayer)
    {
        itemMain1.at24 = &menuNetStart;
        itemMain1.at28 = 2;
    }
    else
    {
        itemMain1.at24 = &menuEpisode;
        itemMain1.at28 = -1;
    }
    menuMain.Add(&itemMain2, false);
    menuMain.Add(&itemMain3, false);
    menuMain.Add(&itemMain4, false);
    menuMain.Add(&itemMain5, false);
    menuMain.Add(&itemMain6, false);
    menuMain.Add(&itemMain7, false);
    menuMain.Add(&itemBloodQAV, false);

#ifdef NETCODE_DISABLE
    itemMain2.bEnable = 0; // disable multiplayer menu item for non-netcode build
#endif
}

void SetupMainMenuWithSave(void)
{
    menuMainWithSave.Add(&itemMainSaveTitle, false);
    menuMainWithSave.Add(&itemMainSave1, true);
    if (gGameOptions.nGameType != kGameTypeSinglePlayer)
    {
        itemMainSave1.at24 = &menuNetStart;
        itemMainSave1.at28 = 2;
    }
    else
    {
        itemMainSave1.at24 = &menuEpisode;
        itemMainSave1.at28 = -1;
    }
    menuMainWithSave.Add(&itemMainSave2, false);
    menuMainWithSave.Add(&itemMainSave3, false);
    menuMainWithSave.Add(&itemMainSave4, false);
    menuMainWithSave.Add(&itemMainSave5, false);
    menuMainWithSave.Add(&itemMainSave6, false);
    menuMainWithSave.Add(&itemMainSave7, false);
    menuMainWithSave.Add(&itemMainSave8, false);
    menuMainWithSave.Add(&itemBloodQAV, false);
}

void SetupNetStartMenu(void)
{
    bool oneEpisode = false;
    menuNetStart.Add(&itemNetStartTitle, false);
    menuNetStart.Add(&itemNetStart1, false);
    for (int i = 0; i < (oneEpisode ? 1 : 6); i++)
    {
        EPISODEINFO *pEpisode = &gEpisodeInfo[i];
        if (i < gEpisodeCount)
            itemNetStart2.Add(pEpisode->title, i == 0);
    }
    menuNetStart.Add(&itemNetStart2, false);
    menuNetStart.Add(&itemNetStart3, false);
    menuNetStart.Add(&itemNetStart4, false);
    menuNetStart.Add(&itemNetStart5, false);
    menuNetStart.Add(&itemNetStart6, false);
    menuNetStart.Add(&itemNetStart7, false);
    menuNetStart.Add(&itemNetStart8, false);
    menuNetStart.Add(&itemNetStart9, false);
    menuNetStart.Add(&itemNetStart10, false);
    menuNetStart.Add(&itemNetStart11, false);
    menuNetStart.Add(&itemNetStart12, false);
    menuMultiUserMaps.Add(&itemNetStartUserMapTitle, true);
    menuMultiUserMaps.Add(&menuMultiUserMap, true);
    itemNetStart1.SetTextIndex(1);
    itemNetStart4.SetTextIndex(2);
    itemNetStart5.SetTextIndex(0);
    itemNetStart6.SetTextIndex(1);
    itemNetStart7.SetTextIndex(1);
    menuNetStart.Add(&itemBloodQAV, false);
}

void SetupSaveGameMenu(void)
{
    menuSaveGame.Add(&itemSaveTitle, false);
    menuSaveGame.Add(&itemSaveGame1, true);
    menuSaveGame.Add(&itemSaveGame2, false);
    menuSaveGame.Add(&itemSaveGame3, false);
    menuSaveGame.Add(&itemSaveGame4, false);
    menuSaveGame.Add(&itemSaveGame5, false);
    menuSaveGame.Add(&itemSaveGame6, false);
    menuSaveGame.Add(&itemSaveGame7, false);
    menuSaveGame.Add(&itemSaveGame8, false);
    menuSaveGame.Add(&itemSaveGame9, false);
    menuSaveGame.Add(&itemSaveGame10, false);
    menuSaveGame.Add(&itemSaveGamePic, false);
    menuSaveGame.Add(&itemBloodQAV, false);

    itemSaveGame1.at2c = &itemSaveGamePic;
    if (!strcmp(strRestoreGameStrings[0], "<Empty>"))
        itemSaveGame1.at37 = 1;

    itemSaveGame2.at2c = &itemSaveGamePic;
    if (!strcmp(strRestoreGameStrings[1], "<Empty>"))
        itemSaveGame2.at37 = 1;

    itemSaveGame3.at2c = &itemSaveGamePic;
    if (!strcmp(strRestoreGameStrings[2], "<Empty>"))
        itemSaveGame3.at37 = 1;

    itemSaveGame4.at2c = &itemSaveGamePic;
    if (!strcmp(strRestoreGameStrings[3], "<Empty>"))
        itemSaveGame4.at37 = 1;

    itemSaveGame5.at2c = &itemSaveGamePic;
    if (!strcmp(strRestoreGameStrings[4], "<Empty>"))
        itemSaveGame5.at37 = 1;

    itemSaveGame6.at2c = &itemSaveGamePic;
    if (!strcmp(strRestoreGameStrings[5], "<Empty>"))
        itemSaveGame6.at37 = 1;

    itemSaveGame7.at2c = &itemSaveGamePic;
    if (!strcmp(strRestoreGameStrings[6], "<Empty>"))
        itemSaveGame7.at37 = 1;

    itemSaveGame8.at2c = &itemSaveGamePic;
    if (!strcmp(strRestoreGameStrings[7], "<Empty>"))
        itemSaveGame8.at37 = 1;

    itemSaveGame9.at2c = &itemSaveGamePic;
    if (!strcmp(strRestoreGameStrings[8], "<Empty>"))
        itemSaveGame9.at37 = 1;

    itemSaveGame10.at2c = &itemSaveGamePic;
    if (!strcmp(strRestoreGameStrings[9], "<Empty>"))
        itemSaveGame10.at37 = 1;
}

void SetupLoadGameMenu(void)
{
    menuLoadGame.Add(&itemLoadTitle, false);
    menuLoadGame.Add(&itemLoadGame1, true);
    menuLoadGame.Add(&itemLoadGame2, false);
    menuLoadGame.Add(&itemLoadGame3, false);
    menuLoadGame.Add(&itemLoadGame4, false);
    menuLoadGame.Add(&itemLoadGame5, false);
    menuLoadGame.Add(&itemLoadGame6, false);
    menuLoadGame.Add(&itemLoadGame7, false);
    menuLoadGame.Add(&itemLoadGame8, false);
    menuLoadGame.Add(&itemLoadGame9, false);
    menuLoadGame.Add(&itemLoadGame10, false);
    menuLoadGame.Add(&itemLoadGamePic, false);
    itemLoadGamePic.at28 = gMenuPicnum;
    itemLoadGame1.at35 = 0;
    itemLoadGame2.at35 = 0;
    itemLoadGame3.at35 = 0;
    itemLoadGame4.at35 = 0;
    itemLoadGame5.at35 = 0;
    itemLoadGame6.at35 = 0;
    itemLoadGame7.at35 = 0;
    itemLoadGame8.at35 = 0;
    itemLoadGame9.at35 = 0;
    itemLoadGame10.at35 = 0;
    itemLoadGame1.at2c = &itemLoadGamePic;
    itemLoadGame2.at2c = &itemLoadGamePic;
    itemLoadGame3.at2c = &itemLoadGamePic;
    itemLoadGame4.at2c = &itemLoadGamePic;
    itemLoadGame5.at2c = &itemLoadGamePic;
    itemLoadGame6.at2c = &itemLoadGamePic;
    itemLoadGame7.at2c = &itemLoadGamePic;
    itemLoadGame8.at2c = &itemLoadGamePic;
    itemLoadGame9.at2c = &itemLoadGamePic;
    itemLoadGame10.at2c = &itemLoadGamePic;
    menuLoadGame.Add(&itemBloodQAV, false);
}

void SetupSoundsMenu(void)
{
    itemSoundsMusic.nValue = ClipRange(MusicVolume, itemSoundsMusic.nRangeLow, itemSoundsMusic.nRangeHigh);
    itemSoundsSound.nValue = ClipRange(FXVolume, itemSoundsSound.nRangeLow, itemSoundsSound.nRangeHigh);
    menuSounds.Add(&itemSoundsTitle, false);
    menuSounds.Add(&itemSoundsMusic, true);
    menuSounds.Add(&itemSoundsSound, false);
    menuSounds.Add(&itemSoundsCDAudio, false);
    menuSounds.Add(&itemSounds3DAudio, false);
    menuSounds.Add(&itemBloodQAV, false);
}

void SetupQuitMenu(void)
{
    menuQuit.Add(&itemQuitTitle, false);
    menuQuit.Add(&itemQuitText1, false);
    menuQuit.Add(&itemQuitYesNo, true);
    menuQuit.Add(&itemBloodQAV, false);

    menuRestart.Add(&itemRestartTitle, false);
    menuRestart.Add(&itemRestartText1, false);
    menuRestart.Add(&itemRestartYesNo, true);
    menuRestart.Add(&itemBloodQAV, false);
}

void SetupHelpOrderMenu(void)
{
    menuOrder.Add(&itemHelp4QAV, true);
    menuOrder.Add(&itemHelp5QAV, false);
    menuOrder.Add(&itemHelp3QAV, false);
    menuOrder.Add(&itemHelp3BQAV, false);
    itemHelp4QAV.bEnable = 1;
    itemHelp4QAV.bNoDraw = 1;
    itemHelp5QAV.bEnable = 1;
    itemHelp5QAV.bNoDraw = 1;
    itemHelp3QAV.bEnable = 1;
    itemHelp3QAV.bNoDraw = 1;
    itemHelp3BQAV.bEnable = 1;
    itemHelp3BQAV.bNoDraw = 1;
}

void SetupCreditsMenu(void)
{
    menuCredits.Add(&itemCreditsQAV, true);
    itemCreditsQAV.bEnable = 1;
    itemCreditsQAV.bNoDraw = 1;
}

void SetupParentalLockMenu(void)
{
    itemParentalLockToggle.at20 = gbAdultContent;
    strcpy(itemParentalLockPassword.at20, gzAdultPassword);
    menuParentalLock.Add(&itemParentalLockTitle, false);
    menuParentalLock.Add(&itemParentalLockToggle, true);
    menuParentalLock.Add(&itemParentalLockPassword, false);
    menuParentalLock.Add(&itemBloodQAV, false);
}

void SetupSorry3Menu(void)
{
    menuPlayOnline.Add(&unk_26E06C, false);
    menuPlayOnline.Add(&unk_26E090, true);
    menuPlayOnline.Add(&unk_26E0E8, false);
    menuPlayOnline.Add(&unk_26E140, false);
    menuPlayOnline.Add(&unk_26E198, false);
    menuPlayOnline.Add(&unk_26E1F0, false);
    menuPlayOnline.Add(&unk_26E248, false);
    menuPlayOnline.Add(&itemBloodQAV, false);
}

void SetupSorryMenu(void)
{
    menuSorry.Add(&itemSorryPicCycle, true);
    menuSorry.Add(&itemSorryText1, false);
    menuSorry.Add(&itemSorryText3, false);
    menuSorry.Add(&itemBloodQAV, false);
}

void SetupSorry2Menu(void)
{
    menuSorry2.Add(&itemSorryPicCycle, true);
    menuSorry2.Add(&itemSorry2Text1, false);
    menuSorry2.Add(&itemSorry2Text2, false);
    menuSorry2.Add(&itemSorry2Text3, false);
    menuSorry2.Add(&itemBloodQAV, false);
}

void SetupOptionsMenu(void)
{
    menuOptions.Add(&itemOptionsTitle, false);
    menuOptions.Add(&itemOptionsChainGame, true);
    menuOptions.Add(&itemOptionsChainDisplay, false);
    menuOptions.Add(&itemOptionsChainSound, false);
    menuOptions.Add(&itemOptionsChainPlayer, false);
    menuOptions.Add(&itemOptionsChainControl, false);
    //menuOptions.Add(&itemOptionsChainOld, false);
    menuOptions.Add(&itemBloodQAV, false);

    menuOptionsGame.Add(&itemOptionsGameTitle, false);
    menuOptionsGame.Add(&itemOptionsGameBoolShowPlayerNames, true);
    menuOptionsGame.Add(&itemOptionsGameShowWeapons, false);
    menuOptionsGame.Add(&itemOptionsGameBoolSlopeTilting, false);
    menuOptionsGame.Add(&itemOptionsGameBoolViewBobbing, false);
    menuOptionsGame.Add(&itemOptionsGameBoolViewSwaying, false);
    menuOptionsGame.Add(&itemOptionsGameCycleAutoAim, false);
    menuOptionsGame.Add(&itemOptionsGameBoolAutoRun, false);
    menuOptionsGame.Add(&itemOptionsGameWeaponSwitch, false);

    //////////////////////
    menuOptionsGame.Add(&itemOptionsGameBoolWeaponsV10X, false);
    menuOptionsGame.Add(&itemOptionsGameBoolVanillaMode, false);
    /////////////////////

    //menuOptionsGame.Add(&itemOptionsGameChainParentalLock, false);
    menuOptionsGame.Add(&itemBloodQAV, false);
    itemOptionsGameBoolShowPlayerNames.at20 = gShowPlayerNames;
    itemOptionsGameShowWeapons.m_nFocus = gShowWeapon;
    itemOptionsGameBoolSlopeTilting.at20 = gSlopeTilting;
    itemOptionsGameBoolViewBobbing.at20 = gViewVBobbing;
    itemOptionsGameBoolViewSwaying.at20 = gViewHBobbing;
    itemOptionsGameCycleAutoAim.m_nFocus = gAutoAim;
    itemOptionsGameBoolAutoRun.at20 = gAutoRun;
    itemOptionsGameWeaponSwitch.m_nFocus = (gWeaponSwitch&1) ? ((gWeaponSwitch&2) ? 1 : 2) : 0;

    ///////
    itemOptionsGameBoolWeaponsV10X.at20 = gWeaponsV10x;
    itemOptionsGameBoolVanillaMode.at20 = gVanilla;
    ///////

    menuOptionsDisplay.Add(&itemOptionsDisplayTitle, false);
    menuOptionsDisplay.Add(&itemOptionsDisplayColor, true);
    menuOptionsDisplay.Add(&itemOptionsDisplayMode, false);
    menuOptionsDisplay.Add(&itemOptionsDisplaySliderDetail, false);
    menuOptionsDisplay.Add(&itemOptionsDisplayBoolVoxels, false);
    menuOptionsDisplay.Add(&itemOptionsDisplayBoolCrosshair, false);
    menuOptionsDisplay.Add(&itemOptionsDisplayBoolCenterHoriz, false);
    menuOptionsDisplay.Add(&itemOptionsDisplayCycleLevelStats, false);
    menuOptionsDisplay.Add(&itemOptionsDisplayBoolPowerupDuration, false);
    menuOptionsDisplay.Add(&itemOptionsDisplayBoolShowMapTitle, false);
    menuOptionsDisplay.Add(&itemOptionsDisplayBoolMessages, false);
    menuOptionsDisplay.Add(&itemOptionsDisplayBoolWidescreen, false);
    menuOptionsDisplay.Add(&itemOptionsDisplayFOV, false);
#ifdef USE_OPENGL
    menuOptionsDisplay.Add(&itemOptionsDisplayPolymost, false);
#endif
    menuOptionsDisplay.Add(&itemBloodQAV, false);
    itemOptionsDisplayBoolVoxels.at20 = usevoxels;
    itemOptionsDisplayBoolCrosshair.at20 = gAimReticle;
    itemOptionsDisplayBoolCenterHoriz.at20 = gCenterHoriz;
    itemOptionsDisplayCycleLevelStats.m_nFocus = gLevelStats;
    itemOptionsDisplayBoolPowerupDuration.at20 = gPowerupDuration;
    itemOptionsDisplayBoolShowMapTitle.at20 = gShowMapTitle;
    itemOptionsDisplayBoolMessages.at20 = gMessageState;
    itemOptionsDisplayBoolWidescreen.at20 = r_usenewaspect;

    menuOptionsDisplayMode.Add(&itemOptionsDisplayModeTitle, false);
    menuOptionsDisplayMode.Add(&itemOptionsDisplayModeResolution, true);
    // prepare video setup
    for (int i = 0; i < validmodecnt; ++i)
    {
        int j;
        for (j = 0; j < gResolutionNum; ++j)
        {
            if (validmode[i].xdim == gResolution[j].xdim && validmode[i].ydim == gResolution[j].ydim)
            {
                gResolution[j].flags |= validmode[i].fs ? RES_FS : RES_WIN;
                Bsnprintf(gResolution[j].name, MAXRESOLUTIONSTRINGLENGTH, "%d x %d%s", gResolution[j].xdim, gResolution[j].ydim, (gResolution[j].flags & RES_FS) ? "" : "Win");
                gResolutionName[j] = gResolution[j].name;
                if (validmode[i].bpp > gResolution[j].bppmax)
                    gResolution[j].bppmax = validmode[i].bpp;
                break;
            }
        }

        if (j == gResolutionNum) // no match found
        {
            gResolution[j].xdim = validmode[i].xdim;
            gResolution[j].ydim = validmode[i].ydim;
            gResolution[j].bppmax = validmode[i].bpp;
            gResolution[j].flags = validmode[i].fs ? RES_FS : RES_WIN;
            Bsnprintf(gResolution[j].name, MAXRESOLUTIONSTRINGLENGTH, "%d x %d%s", gResolution[j].xdim, gResolution[j].ydim, (gResolution[j].flags & RES_FS) ? "" : "Win");
            gResolutionName[j] = gResolution[j].name;
            ++gResolutionNum;
        }
    }
    SetupVideoModeMenu(NULL);
    itemOptionsDisplayModeResolution.SetTextArray(gResolutionName, gResolutionNum, 0);
#ifdef USE_OPENGL
    menuOptionsDisplayMode.Add(&itemOptionsDisplayModeRenderer, false);
#endif
    menuOptionsDisplayMode.Add(&itemOptionsDisplayModeFullscreen, false);
#ifdef USE_OPENGL
    menuOptionsDisplayMode.Add(&itemOptionsDisplayModeVSync, false);
#endif
    menuOptionsDisplayMode.Add(&itemOptionsDisplayModeFrameLimit, false);
    menuOptionsDisplayMode.Add(&itemOptionsDisplayModeApply, false);
    menuOptionsDisplayMode.Add(&itemBloodQAV, false);

#ifdef USE_OPENGL
    itemOptionsDisplayModeRenderer.pPreDrawCallback = PreDrawVideoModeMenu;
#endif
    itemOptionsDisplayModeFullscreen.pPreDrawCallback = PreDrawVideoModeMenu;

    menuOptionsDisplayColor.Add(&itemOptionsDisplayColorTitle, false);
    menuOptionsDisplayColor.Add(&itemOptionsDisplayColorGamma, true);
    menuOptionsDisplayColor.Add(&itemOptionsDisplayColorContrast, false);
    menuOptionsDisplayColor.Add(&itemOptionsDisplayColorSaturation, false);
    menuOptionsDisplayColor.Add(&itemOptionsDisplayColorVisibility, false);
    menuOptionsDisplayColor.Add(&itemOptionsDisplayColorReset, false);
    menuOptionsDisplayColor.Add(&itemBloodQAV, false);

    itemOptionsDisplayColorContrast.pPreDrawCallback = PreDrawDisplayColor;
    itemOptionsDisplayColorSaturation.pPreDrawCallback = PreDrawDisplayColor;

#ifdef USE_OPENGL
    menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostTitle, false);
    //menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostTextureMode, true);
    //menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostAnisotropy, false);
    menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostTrueColorTextures, true);
    menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostTexQuality, false);
    menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostPreloadCache, false);
    menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostTexCache, false);
    menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostDetailTex, false);
    menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostGlowTex, false);
    menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymost3DModels, false);
    menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostDeliriumBlur, false);
    menuOptionsDisplayPolymost.Add(&itemOptionsDisplayPolymostYShearing, false);
    menuOptionsDisplayPolymost.Add(&itemBloodQAV, false);

    itemOptionsDisplayPolymostTexQuality.pPreDrawCallback = PreDrawDisplayPolymost;
    itemOptionsDisplayPolymostPreloadCache.pPreDrawCallback = PreDrawDisplayPolymost;
    itemOptionsDisplayPolymostTexCache.pPreDrawCallback = PreDrawDisplayPolymost;
    itemOptionsDisplayPolymostDetailTex.pPreDrawCallback = PreDrawDisplayPolymost;
    itemOptionsDisplayPolymostGlowTex.pPreDrawCallback = PreDrawDisplayPolymost;
#endif

    menuOptionsSound.Add(&itemOptionsSoundTitle, false);
    menuOptionsSound.Add(&itemOptionsSoundSoundToggle, true);
    menuOptionsSound.Add(&itemOptionsSoundMusicToggle, false);
    menuOptionsSound.Add(&itemOptionsSound3DToggle, false);
    menuOptionsSound.Add(&itemOptionsSoundSoundVolume, false);
    menuOptionsSound.Add(&itemOptionsSoundMusicVolume, false);
    menuOptionsSound.Add(&itemOptionsSoundSampleRate, false);
    menuOptionsSound.Add(&itemOptionsSoundNumVoices, false);
    menuOptionsSound.Add(&itemOptionsSoundCDToggle, false);
    menuOptionsSound.Add(&itemOptionsSoundMusicDevice, false);
    menuOptionsSound.Add(&itemOptionsSoundSF2Bank, false);

    menuOptionsSound.Add(&itemOptionsSoundApplyChanges, false);
    menuOptionsSound.Add(&itemBloodQAV, false);

    menuOptionsSoundSF2.Add(&itemOptionsSoundSF2Title, true);
    menuOptionsSoundSF2.Add(&itemOptionsSoundSF2FS, true);

    menuOptionsPlayer.Add(&itemOptionsPlayerTitle, false);
    menuOptionsPlayer.Add(&itemOptionsPlayerName, true);
    menuOptionsPlayer.Add(&itemBloodQAV, false);
}

void SetupControlsMenu(void)
{
    menuOptionsControl.Add(&itemOptionsControlTitle, false);
    menuOptionsControl.Add(&itemOptionsControlKeyboard, true);
    menuOptionsControl.Add(&itemOptionsControlMouse, false);
    menuOptionsControl.Add(&itemOptionsControlJoystickButtons, false);
    menuOptionsControl.Add(&itemOptionsControlJoystickAxes, false);
    menuOptionsControl.Add(&itemOptionsControlJoystickMisc, false);
    menuOptionsControl.Add(&itemBloodQAV, false);

    menuOptionsControlKeyboard.Add(&itemOptionsControlKeyboardTitle, false);
    menuOptionsControlKeyboard.Add(&itemOptionsControlKeyboardSliderTurnSpeed, true);
    menuOptionsControlKeyboard.Add(&itemOptionsControlKeyboardCycleTurnAcceleration, false);
    menuOptionsControlKeyboard.Add(&itemOptionsControlKeyboardList, false);
    menuOptionsControlKeyboard.Add(&itemOptionsControlKeyboardReset, false);
    menuOptionsControlKeyboard.Add(&itemOptionsControlKeyboardResetClassic, false);
    menuOptionsControlKeyboard.Add(&itemBloodQAV, false);

    itemOptionsControlKeyboardSliderTurnSpeed.nValue = gTurnSpeed;
    itemOptionsControlKeyboardCycleTurnAcceleration.m_nFocus = gTurnAcceleration;

    menuOptionsControlMouse.Add(&itemOptionsControlMouseTitle, false);
    menuOptionsControlMouse.Add(&itemOptionsControlMouseButton, true);
    menuOptionsControlMouse.Add(&itemOptionsControlMouseSensitivity, false);
    menuOptionsControlMouse.Add(&itemOptionsControlMouseAimFlipped, false);
    menuOptionsControlMouse.Add(&itemOptionsControlMouseAimMode, false);
    menuOptionsControlMouse.Add(&itemOptionsControlMouseVerticalAim, false);
    menuOptionsControlMouse.Add(&itemOptionsControlMouseXSensitivity, false);
    menuOptionsControlMouse.Add(&itemOptionsControlMouseYSensitivity, false);
    menuOptionsControlMouse.Add(&itemBloodQAV, false);

    itemOptionsControlMouseVerticalAim.pPreDrawCallback = PreDrawControlMouse;

    menuOptionsControlMouseButtonAssignment.Add(&itemOptionsControlMouseTitle, false);
    for (int i = 0, y = 60; i < MENUMOUSEFUNCTIONS; i++)
    {
        pItemOptionsControlMouseButton[i] = new CGameMenuItemZCycle(MenuMouseNames[i], 3, 66, y, 180, 0, SetMouseButton, pzGamefuncsStrings, NUMGAMEFUNCTIONS+1, 0, true);
        dassert(pItemOptionsControlMouseButton[i] != NULL);
        menuOptionsControlMouseButtonAssignment.Add(pItemOptionsControlMouseButton[i], i == 0);
        y += 10;
    }
    menuOptionsControlMouseButtonAssignment.Add(&itemBloodQAV, false);
}

void SetupJoystickMenu(void)
{
    if (!CONTROL_JoystickEnabled) // joystick disabled, don't bother populating joystick menus
    {
        itemOptionsControlJoystickButtons.bEnable = 0;
        itemOptionsControlJoystickAxes.bEnable = 0;
        itemOptionsControlJoystickMisc.bEnable = 0;
        return;
    }

    menuOptionsControlJoystickMisc.Add(&itemOptionsControlJoystickMiscTitle, false);
    menuOptionsControlJoystickMisc.Add(&itemOptionsControlJoystickMiscCenterView, true);
    menuOptionsControlJoystickMisc.Add(&itemOptionsControlJoystickMiscRumble, false);
    menuOptionsControlJoystickMisc.Add(&itemBloodQAV, false);

    itemOptionsControlJoystickMiscCenterView.at20 = gCenterViewOnDrop;
    itemOptionsControlJoystickMiscRumble.at20 = gSetup.joystickrumble;

    int i = 0, y = 0;
    for (int nButton = 0; nButton < joystick.numButtons; nButton++) // store every joystick button/hat name for button list at launch
    {
        const char *pzButtonName = joyGetName(1, nButton);
        if (pzButtonName == NULL) // if we've ran out of button names, store joystick hat names
        {
            for (int nHats = 0; nHats < (joystick.numHats > 0) * 4; nHats++)
            {
                const char *pzHatName = joyGetName(2, nHats);
                if (pzHatName == NULL)
                    break;
                Bsnprintf(MenuJoyButtonNames[i++], 64, "%s", pzHatName);
                Bsnprintf(MenuJoyButtonNames[i++], 64, "Double %s", pzHatName);
            }
            break;
        }
        Bsnprintf(MenuJoyButtonNames[i++], 64, "%s", pzButtonName);
        Bsnprintf(MenuJoyButtonNames[i++], 64, "Double %s", pzButtonName);
    }
    const int nMaxJoyButtons = i;

    i = 0;
    for (int nAxis = 0; nAxis < joystick.numAxes; nAxis++) // store every joystick axes for axes list at launch
    {
        const char *pzAxisName = joyGetName(0, nAxis);
        if (pzAxisName == NULL) // if we've ran out of axes names, stop
            break;
        Bsnprintf(MenuJoyAxisNames[i++], 64, "%s", pzAxisName);
    }
    const int nMaxJoyAxes = i;

    if (nMaxJoyButtons <= 0) // joystick has no buttons, disable button menu
    {
        itemOptionsControlJoystickButtons.bEnable = 0;
    }
    else
    {
        i = 0;
        for (int nPage = 0; nPage < MAXJOYSTICKBUTTONPAGES; nPage++) // create lists of joystick button items
        {
            y = 35;
            menuOptionsControlJoystickButtonAssignment[nPage].Add(&itemJoyButtonsTitle, false);
            for (int nButton = 0; nButton < JOYSTICKITEMSPERPAGE; nButton++) // populate button list
            {
                pItemOptionsControlJoyButton[nPage][nButton] = new CGameMenuItemZCycle(MenuJoyButtonNames[i], 3, 66, y, 180, 0, SetJoyButton, pzGamefuncsStrings, NUMGAMEFUNCTIONS+1, 0, true);
                dassert(pItemOptionsControlJoyButton[nPage][nButton] != NULL);
                menuOptionsControlJoystickButtonAssignment[nPage].Add(pItemOptionsControlJoyButton[nPage][nButton], nButton == 0);
                y += 9;
                i++;
                if (i >= nMaxJoyButtons) // if we've reached the total number of buttons, stop populating list
                    break;
            }
            if (i < nMaxJoyButtons) // if we still have more buttons to list, add next page menu item at bottom of page
            {
                pItemOptionsControlJoyButtonNextPage[nPage] = new CGameMenuItemChain("NEXT PAGE", 3, 0, 182, 320, 1, &menuOptionsControlJoystickButtonAssignment[nPage+1], -1, NULL, 0);
                dassert(pItemOptionsControlJoyButtonNextPage[nPage] != NULL);
                menuOptionsControlJoystickButtonAssignment[nPage].Add(pItemOptionsControlJoyButtonNextPage[nPage], false);
            }
            menuOptionsControlJoystickButtonAssignment[nPage].Add(&itemBloodQAV, false);
        }
    }

    if (nMaxJoyAxes <= 0) // joystick has no axes, disable axis menu
    {
        itemOptionsControlJoystickAxes.bEnable = 0;
        return;
    }

    for (int nAxis = 0; nAxis < (int)ARRAY_SIZE(pItemOptionsControlJoystickAxis); nAxis++) // set all possible axis items to null (used for button setup)
        pItemOptionsControlJoystickAxis[nAxis] = NULL;

    y = 40;
    const char bUseBigFont = nMaxJoyAxes <= 6;
    menuOptionsControlJoystickListAxes.Add(&itemJoyAxesTitle, false);
    for (int nAxis = 0; nAxis < nMaxJoyAxes; nAxis++) // create list of axes for joystick axis menu
    {
        pItemOptionsControlJoystickAxis[nAxis] = new CGameMenuItemChain(MenuJoyAxisNames[nAxis], bUseBigFont ? 1 : 3, 66, y, 180, 0, &menuOptionsControlJoystickAxis[nAxis], -1, NULL, 0);
        dassert(pItemOptionsControlJoystickAxis[nAxis] != NULL);
        menuOptionsControlJoystickListAxes.Add(pItemOptionsControlJoystickAxis[nAxis], nAxis == 0);
        y += bUseBigFont ? 20 : 10;
    }
    menuOptionsControlJoystickListAxes.Add(&itemBloodQAV, false);

    for (int nAxis = 0; nAxis < nMaxJoyAxes; nAxis++) // create settings for each listed joystick axis
    {
        y = 40;
        menuOptionsControlJoystickAxis[nAxis].Add(&itemJoyAxesTitle, false);
        pItemOptionsControlJoystickAxisName[nAxis] = new CGameMenuItemTitle(MenuJoyAxisNames[nAxis], 3, 160, y, -1); // get axis name
        dassert(pItemOptionsControlJoystickAxisName[nAxis] != NULL);
        menuOptionsControlJoystickAxis[nAxis].Add(pItemOptionsControlJoystickAxisName[nAxis], false);
        y += 12;
        pItemOptionsControlJoystickAxisScale[nAxis] = new CGameMenuItemSlider("AXIS SCALE:", 1, 18, y, 280, &JoystickAnalogueScale[nAxis], fix16_from_int(0), fix16_from_float(2.f), fix16_from_float(0.025f), SetJoystickScale, -1, -1, kMenuSliderQ16); // get axis scale
        dassert(pItemOptionsControlJoystickAxisScale[nAxis] != NULL);
        menuOptionsControlJoystickAxis[nAxis].Add(pItemOptionsControlJoystickAxisScale[nAxis], true);
        y += 25;
        pItemOptionsControlJoystickAxisAnalogue[nAxis] = new CGameMenuItemZCycle("ANALOG:", 1, 18, y, 280, 0, SetJoystickAnalogue, zJoystickAnalogue, ARRAY_SSIZE(zJoystickAnalogue), 0); // get analog function
        dassert(pItemOptionsControlJoystickAxisAnalogue[nAxis] != NULL);
        menuOptionsControlJoystickAxis[nAxis].Add(pItemOptionsControlJoystickAxisAnalogue[nAxis], false);
        y += 17;
        pItemOptionsControlJoystickAxisAnalogueInvert[nAxis] = new CGameMenuItemZBool("ANALOG INVERT:", 1, 18, y, 280, false, SetJoystickAnalogueInvert, NULL, NULL); // get analog function
        dassert(pItemOptionsControlJoystickAxisAnalogueInvert[nAxis] != NULL);
        menuOptionsControlJoystickAxis[nAxis].Add(pItemOptionsControlJoystickAxisAnalogueInvert[nAxis], false);
        y += 17;
        pItemOptionsControlJoystickAxisDigitalPos[nAxis] = new CGameMenuItemZCycle("DIGITAL +:", 1, 18, y, 280, 0, SetJoystickDigitalPos, pzGamefuncsStrings, NUMGAMEFUNCTIONS+1, 0, true); // get digital function
        dassert(pItemOptionsControlJoystickAxisDigitalPos[nAxis] != NULL);
        menuOptionsControlJoystickAxis[nAxis].Add(pItemOptionsControlJoystickAxisDigitalPos[nAxis], false);
        y += 17;
        pItemOptionsControlJoystickAxisDigitalNeg[nAxis] = new CGameMenuItemZCycle("DIGITAL -:", 1, 18, y, 280, 0, SetJoystickDigitalNeg, pzGamefuncsStrings, NUMGAMEFUNCTIONS+1, 0, true); // get digital function
        dassert(pItemOptionsControlJoystickAxisDigitalNeg[nAxis] != NULL);
        menuOptionsControlJoystickAxis[nAxis].Add(pItemOptionsControlJoystickAxisDigitalNeg[nAxis], false);
        y += 25;
        pItemOptionsControlJoystickAxisDeadzone[nAxis] = new CGameMenuItemSlider("DEAD ZONE:", 1, 18, y, 280, &JoystickAnalogueDead[nAxis], fix16_from_int(0), fix16_from_float(0.5f), fix16_from_float(0.025f), SetJoystickDeadzone, -1, -1, kMenuSliderPercent); // get dead size
        dassert(pItemOptionsControlJoystickAxisDeadzone[nAxis] != NULL);
        menuOptionsControlJoystickAxis[nAxis].Add(pItemOptionsControlJoystickAxisDeadzone[nAxis], false);
        y += 17;
        pItemOptionsControlJoystickAxisSaturate[nAxis] = new CGameMenuItemSlider("SATURATE:", 1, 18, y, 280, &JoystickAnalogueSaturate[nAxis], fix16_from_int(0), fix16_from_float(0.5f), fix16_from_float(0.025f), SetJoystickSaturate, -1, -1, kMenuSliderPercent); // get saturate
        dassert(pItemOptionsControlJoystickAxisSaturate[nAxis] != NULL);
        menuOptionsControlJoystickAxis[nAxis].Add(pItemOptionsControlJoystickAxisSaturate[nAxis], false);
        menuOptionsControlJoystickAxis[nAxis].Add(&itemBloodQAV, false);
    }
}

void SetupMenus(void)
{
    // prepare gamefuncs and keys
    pzGamefuncsStrings[0] = MenuGameFuncNone;
    nGamefuncsValues[0] = -1;
    int k = 1;
    for (int i = 0; i < NUMGAMEFUNCTIONS; ++i)
    {
        Bstrcpy(MenuGameFuncs[i], gamefunctions[i]);

        for (int j = 0; j < MAXGAMEFUNCLEN; ++j)
            if (MenuGameFuncs[i][j] == '_')
                MenuGameFuncs[i][j] = ' ';

        if (gamefunctions[i][0] != '\0')
        {
            pzGamefuncsStrings[k] = MenuGameFuncs[i];
            nGamefuncsValues[k] = i;
            ++k;
        }
    }

    nGamefuncsNum = k;

    SetupLoadingScreen();
    SetupKeyListMenu();
    SetupMessagesMenu();
    SetupControlsOldMenu();
    SetupSaveGameMenu();
    SetupLoadGameMenu();
    SetupOptionsOldMenu();
    SetupCreditsMenu();
    SetupHelpOrderMenu();
    SetupSoundsMenu();
    SetupDifficultyMenu();
    SetupEpisodeMenu();
    SetupMainMenu();
    SetupMainMenuWithSave();
    SetupNetStartMenu();
    SetupQuitMenu();
    SetupParentalLockMenu();
    SetupSorryMenu();
    SetupSorry2Menu();
    SetupSorry3Menu();

    SetupOptionsMenu();
    SetupControlsMenu();
    SetupJoystickMenu();
    SetupNetworkMenu();
}

void UpdateNetworkMenus(void)
{
    if (gGameOptions.nGameType != kGameTypeSinglePlayer)
    {
        itemMain1.at24 = &menuNetStart;
        itemMain1.at28 = 2;
    }
    else
    {
        itemMain1.at24 = &menuEpisode;
        itemMain1.at28 = -1;
    }
    if (gGameOptions.nGameType != kGameTypeSinglePlayer)
    {
        itemMainSave1.at24 = &menuNetStart;
        itemMainSave1.at28 = 2;
    }
    else
    {
        itemMainSave1.at24 = &menuEpisode;
        itemMainSave1.at28 = -1;
    }
}

void SetMonoStereo(CGameMenuItemZBool *pItem)
{
    gStereo = pItem->at20;
}

void SetCrosshair(CGameMenuItemZBool *pItem)
{
    gAimReticle = pItem->at20;
}

void SetCenterHoriz(CGameMenuItemZBool *pItem)
{
    gCenterHoriz = pItem->at20;
}

void ResetKeys(CGameMenuItemChain *)
{
    CONFIG_SetDefaultKeys(keydefaults);
}

void ResetKeysClassic(CGameMenuItemChain *)
{
    CONFIG_SetDefaultKeys(oldkeydefaults);
}

////
void SetWeaponsV10X(CGameMenuItemZBool* pItem)
{
    if (gGameOptions.nGameType == kGameTypeSinglePlayer) {
        gWeaponsV10x = pItem->at20;
        gGameOptions.weaponsV10x = pItem->at20;
    }
}

void SetVanillaMode(CGameMenuItemZBool* pItem)
{
    gVanilla = pItem->at20;
    if ((gGameOptions.nGameType == kGameTypeSinglePlayer) && (numplayers == 1))
    {
        VanillaModeUpdate();
        viewClearInterpolations();
        viewResizeView(gViewSize);
        gGameMessageMgr.Clear();
    }
    else
        viewSetMessage("Vanilla mode is disabled for multiplayer");
}
////

void SetShowPlayerNames(CGameMenuItemZBool *pItem)
{
    gShowPlayerNames = pItem->at20;
}

void SetShowWeapons(CGameMenuItemZCycle *pItem)
{
    gShowWeapon = pItem->m_nFocus;
}

void SetSlopeTilting(CGameMenuItemZBool *pItem)
{
    gSlopeTilting = pItem->at20;
}

void SetViewBobbing(CGameMenuItemZBool *pItem)
{
    gViewVBobbing = pItem->at20;
}

void SetViewSwaying(CGameMenuItemZBool *pItem)
{
    gViewHBobbing = pItem->at20;
}

void SetDetail(CGameMenuItemSlider *pItem)
{
    gDetail = pItem->nValue;
}

void SetVoxels(CGameMenuItemZBool *pItem)
{
    usevoxels = pItem->at20;
}

void SetGamma(CGameMenuItemSlider *pItem)
{
    gGamma = pItem->nValue;
    scrSetGamma(gGamma);
}

void SetMusicVol(CGameMenuItemSlider *pItem)
{
    sndSetMusicVolume(pItem->nValue);
}

void SetSoundVol(CGameMenuItemSlider *pItem)
{
    sndSetFXVolume(pItem->nValue);
}

void SetCDVol(CGameMenuItemSlider *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    // NUKE-TODO:
}

void SetMessages(CGameMenuItemZBool *pItem)
{
    gMessageState = pItem->at20;
    gGameMessageMgr.SetState(gMessageState);
}

void SetMouseSensitivity(CGameMenuItemSliderFloat *pItem)
{
	CONTROL_MouseSensitivity = pItem->fValue;
}

void SetMouseAimFlipped(CGameMenuItemZBool *pItem)
{
    gMouseAimingFlipped = pItem->at20;
}

void SetTurnSpeed(CGameMenuItemSlider *pItem)
{
    gTurnSpeed = pItem->nValue;
}

void SetTurnAcceleration(CGameMenuItemZCycle *pItem)
{
    gTurnAcceleration = pItem->m_nFocus;
}

void SetCenterView(CGameMenuItemZBool *pItem)
{
    gCenterViewOnDrop = pItem->at20;
}

void SetJoystickRumble(CGameMenuItemZBool *pItem)
{
    gSetup.joystickrumble = pItem->at20;
}

void SetAutoAim(CGameMenuItemZCycle *pItem)
{
    gAutoAim = pItem->m_nFocus;
    if (!gDemo.at0 && !gDemo.at1)
    {
        gProfile[myconnectindex].nAutoAim = gAutoAim;
        netBroadcastPlayerInfo(myconnectindex);
    }
}

void SetAutoRun(CGameMenuItemZBool *pItem)
{
    gAutoRun = pItem->at20;
}

void SetLevelStats(CGameMenuItemZCycle *pItem)
{
    gLevelStats = pItem->m_nFocus;
}

void SetPowerupDuration(CGameMenuItemZBool* pItem)
{
    gPowerupDuration = pItem->at20;
}

void SetShowMapTitle(CGameMenuItemZBool* pItem)
{
    gShowMapTitle = pItem->at20;
}

void SetWeaponSwitch(CGameMenuItemZCycle *pItem)
{
    gWeaponSwitch &= ~(1|2);
    switch (pItem->m_nFocus)
    {
    case 0:
        break;
    case 1:
        gWeaponSwitch |= 2;
        fallthrough__;
    case 2:
    default:
        gWeaponSwitch |= 1;
        break;
    }
    if (!gDemo.at0 && !gDemo.at1)
    {
        gProfile[myconnectindex].nWeaponSwitch = gWeaponSwitch;
        netBroadcastPlayerInfo(myconnectindex);
    }
}

extern bool gStartNewGame;

void ShowDifficulties()
{
    gGameMenuMgr.Push(&menuDifficulty, 3);
}

void SetDifficultyAndStart(CGameMenuItemChain *pItem)
{
    gGameOptions.nDifficulty = pItem->at30;
    gGameOptions.nDifficultyQuantity = pItem->at30;
    gGameOptions.nDifficultyHealth = pItem->at30;
    gSkill = pItem->at30;
    gGameOptions.nLevel = 0;
    gGameOptions.uGameFlags = kGameFlagNone;
    if (gDemo.at1)
        gDemo.StopPlayback();
    gStartNewGame = true;
    gCheatMgr.ResetCheats();
    if (Bstrlen(gGameOptions.szUserMap))
    {
        levelAddUserMap(gGameOptions.szUserMap);
        levelSetupOptions(gGameOptions.nEpisode, gGameOptions.nLevel);
        StartLevel(&gGameOptions);
        viewResizeView(gViewSize);
    }
    gGameMenuMgr.Deactivate();
}

void SetDifficultyCustomAndStart(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    gGameOptions.nDifficulty = ClipRange(sliderDifficultyCustomDifficulty.nValue, 0, 4);
    gGameOptions.nDifficultyQuantity = ClipRange(sliderDifficultyCustomQuantity.nValue, 0, 4);
    gGameOptions.nDifficultyHealth = ClipRange(sliderDifficultyCustomHealth.nValue, 0, 4);
    gSkill = sliderDifficultyCustomDamage.nValue;
    gGameOptions.nLevel = 0;
    gGameOptions.uGameFlags = kGameFlagNone;
    if (gDemo.at1)
        gDemo.StopPlayback();
    gStartNewGame = true;
    gCheatMgr.ResetCheats();
    if (Bstrlen(gGameOptions.szUserMap))
    {
        levelAddUserMap(gGameOptions.szUserMap);
        levelSetupOptions(gGameOptions.nEpisode, gGameOptions.nLevel);
        StartLevel(&gGameOptions);
        viewResizeView(gViewSize);
    }
    gGameMenuMgr.Deactivate();
}

void SetVideoModeOld(CGameMenuItemChain *pItem)
{
    if (pItem->at30 == validmodecnt)
    {
        gSetup.fullscreen = 0;
        gSetup.xdim = 640;
        gSetup.ydim = 480;
    }
    else
    {
        gSetup.fullscreen = 0;
        gSetup.xdim = validmode[pItem->at30].xdim;
        gSetup.ydim = validmode[pItem->at30].ydim;
    }
    scrSetGameMode(gSetup.fullscreen, gSetup.xdim, gSetup.ydim, gSetup.bpp);
    scrSetDac();
    viewResizeView(gViewSize);
}

void SetVideoMode(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    resolution_t p = { xres, yres, fullscreen, bpp, 0 };
    int32_t prend = videoGetRenderMode();
    int32_t pvsync = vsync;

    int32_t nResolution = itemOptionsDisplayModeResolution.m_nFocus;
    resolution_t n = { gResolution[nResolution].xdim, gResolution[nResolution].ydim,
                       (gResolution[nResolution].flags & RES_FS) ? itemOptionsDisplayModeFullscreen.at20 : 0,
                       (nRendererValues[itemOptionsDisplayModeRenderer.m_nFocus] == REND_CLASSIC) ? 8 : gResolution[nResolution].bppmax, 0 };
    int32_t UNUSED(nrend) = nRendererValues[itemOptionsDisplayModeRenderer.m_nFocus];
    int32_t nvsync = nVSyncValues[itemOptionsDisplayModeVSync.m_nFocus];

    if (videoSetGameMode(n.flags, n.xdim, n.ydim, n.bppmax, upscalefactor) < 0)
    {
        if (videoSetGameMode(p.flags, p.xdim, p.ydim, p.bppmax, upscalefactor) < 0)
        {
            videoSetRenderMode(prend);
            ThrowError("Failed restoring old video mode.");
        }
        else
        {
            onvideomodechange(p.bppmax > 8);
            vsync = videoSetVsync(pvsync);
        }
    }
    else onvideomodechange(n.bppmax > 8);

    viewResizeView(gViewSize);
    vsync = videoSetVsync(nvsync);
    gSetup.fullscreen = fullscreen;
    gSetup.xdim = xres;
    gSetup.ydim = yres;
    gSetup.bpp = bpp;
}

void SetWidescreen(CGameMenuItemZBool *pItem)
{
    r_usenewaspect = pItem->at20;
}

void SetFOV(CGameMenuItemSlider *pItem)
{
    gFov = pItem->nValue;
}

void SetupVideoModeMenu(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    for (int i = 0; i < gResolutionNum; i++)
    {
        if (gSetup.xdim == gResolution[i].xdim && gSetup.ydim == gResolution[i].ydim)
        {
            itemOptionsDisplayModeResolution.m_nFocus = i;
            break;
        }
    }
    itemOptionsDisplayModeFullscreen.at20 = gSetup.fullscreen;
#ifdef USE_OPENGL
    for (int i = 0; i < 2; i++)
    {
        if (videoGetRenderMode() == nRendererValues[i])
        {
            itemOptionsDisplayModeRenderer.m_nFocus = i;
            break;
        }
    }
#endif
    for (int i = 0; i < itemOptionsDisplayModeVSync.m_nItems; i++)
    {
        if (vsync == nVSyncValues[i])
        {
            itemOptionsDisplayModeVSync.m_nFocus = i;
            break;
        }
    }
    const int kMaxFps = r_maxfps == -1 ? round(refreshfreq) : r_maxfps;
    for (int i = 0; i < 8; i++)
    {
        if (kMaxFps == nFrameLimitValues[i])
        {
            itemOptionsDisplayModeFrameLimit.m_nFocus = i;
            break;
        }
    }
}

void PreDrawVideoModeMenu(CGameMenuItem *pItem)
{
    if (pItem == &itemOptionsDisplayModeFullscreen)
        pItem->bEnable = !!(gResolution[itemOptionsDisplayModeResolution.m_nFocus].flags & RES_FS);
#ifdef USE_OPENGL
    else if (pItem == &itemOptionsDisplayModeRenderer)
        pItem->bEnable = gResolution[itemOptionsDisplayModeResolution.m_nFocus].bppmax > 8;
#endif
}

void UpdateVideoModeMenuFrameLimit(CGameMenuItemZCycle *pItem)
{
    r_maxfps = nFrameLimitValues[pItem->m_nFocus];
    g_frameDelay = calcFrameDelay(r_maxfps);
}

void UpdateVideoColorMenu(CGameMenuItemSliderFloat *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    g_videoGamma = itemOptionsDisplayColorGamma.fValue;
    g_videoContrast = itemOptionsDisplayColorContrast.fValue;
    g_videoSaturation = itemOptionsDisplayColorSaturation.fValue;
    r_ambientlight = itemOptionsDisplayColorVisibility.fValue;
    r_ambientlightrecip = 1.f/r_ambientlight;
    gBrightness = GAMMA_CALC<<2;
    videoSetPalette(gBrightness>>2, gLastPal, 0);
}

void PreDrawDisplayColor(CGameMenuItem *pItem)
{
    if (pItem == &itemOptionsDisplayColorContrast)
        pItem->bEnable = gammabrightness;
    else if (pItem == &itemOptionsDisplayColorSaturation)
        pItem->bEnable = gammabrightness;
}

void ResetVideoColor(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    g_videoGamma = DEFAULT_GAMMA;
    g_videoContrast = DEFAULT_CONTRAST;
    g_videoSaturation = DEFAULT_SATURATION;
    gBrightness = 0;
    r_ambientlight = r_ambientlightrecip = 1.f;
    videoSetPalette(gBrightness>>2, gLastPal, 0);
}

#ifdef USE_OPENGL
void SetupVideoPolymostMenu(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    itemOptionsDisplayPolymostTextureMode.m_nFocus = 0;
    for (int i = 0; i < 2; i++)
    {
        if (nTextureModeValues[i] == gltexfiltermode)
        {
            itemOptionsDisplayPolymostTextureMode.m_nFocus = i;
            break;
        }
    }
    itemOptionsDisplayPolymostAnisotropy.m_nFocus = 0;
    for (int i = 0; i < 6; i++)
    {
        if (nAnisotropyValues[i] == glanisotropy)
        {
            itemOptionsDisplayPolymostAnisotropy.m_nFocus = i;
            break;
        }
    }
    itemOptionsDisplayPolymostTrueColorTextures.at20 = usehightile;
    itemOptionsDisplayPolymostTexQuality.m_nFocus = r_downsize;
    itemOptionsDisplayPolymostPreloadCache.at20 = useprecache;
    itemOptionsDisplayPolymostTexCache.m_nFocus = glusetexcache;
    itemOptionsDisplayPolymostDetailTex.at20 = r_detailmapping;
    itemOptionsDisplayPolymostGlowTex.at20 = r_glowmapping;
    itemOptionsDisplayPolymost3DModels.at20 = usemodels;
    itemOptionsDisplayPolymostDeliriumBlur.at20 = gDeliriumBlur;
    itemOptionsDisplayPolymostYShearing.at20 = r_yshearing;
}

void UpdateTextureMode(CGameMenuItemZCycle *pItem)
{
    gltexfiltermode = nTextureModeValues[pItem->m_nFocus];
    gltexapplyprops();
}

void UpdateAnisotropy(CGameMenuItemZCycle *pItem)
{
    glanisotropy = nAnisotropyValues[pItem->m_nFocus];
    gltexapplyprops();
}

void UpdateTrueColorTextures(CGameMenuItemZBool *pItem)
{
    usehightile = pItem->at20;
}
#endif

void DoModeChange(void)
{
    videoResetMode();
    if (videoSetGameMode(fullscreen, xres, yres, bpp, upscalefactor))
        OSD_Printf("restartvid: Reset failed...\n");
    onvideomodechange(gSetup.bpp > 8);
}

#ifdef USE_OPENGL
void UpdateTexQuality(CGameMenuItemZCycle *pItem)
{
    r_downsize = pItem->m_nFocus;
    texcache_invalidate();
    r_downsizevar = r_downsize;
    DoModeChange();
}

void UpdatePreloadCache(CGameMenuItemZBool *pItem)
{
    useprecache = pItem->at20;
}

void UpdateTexCache(CGameMenuItemZCycle *pItem)
{
    glusetexcache = pItem->m_nFocus;
}

void UpdateDetailTex(CGameMenuItemZBool *pItem)
{
    r_detailmapping = pItem->at20;
}

void UpdateGlowTex(CGameMenuItemZBool *pItem)
{
    r_glowmapping = pItem->at20;
}

void Update3DModels(CGameMenuItemZBool *pItem)
{
    usemodels = pItem->at20;
}

void UpdateDeliriumBlur(CGameMenuItemZBool *pItem)
{
    gDeliriumBlur = pItem->at20;
}

void UpdateYShrearing(CGameMenuItemZBool *pItem)
{
    r_yshearing = pItem->at20;
}

void PreDrawDisplayPolymost(CGameMenuItem *pItem)
{
    if (pItem == &itemOptionsDisplayPolymostTexQuality)
        pItem->bEnable = usehightile;
    else if (pItem == &itemOptionsDisplayPolymostPreloadCache)
        pItem->bEnable = usehightile;
    else if (pItem == &itemOptionsDisplayPolymostTexCache)
        pItem->bEnable = glusetexcompr && usehightile;
    else if (pItem == &itemOptionsDisplayPolymostDetailTex)
        pItem->bEnable = usehightile;
    else if (pItem == &itemOptionsDisplayPolymostGlowTex)
        pItem->bEnable = usehightile;
}
#endif

void UpdateSoundToggle(CGameMenuItemZBool *pItem)
{
    SoundToggle = pItem->at20;
    if (!SoundToggle)
        FX_StopAllSounds();
}

void UpdateMusicToggle(CGameMenuItemZBool *pItem)
{
    MusicToggle = pItem->at20;
    if (!MusicToggle)
        sndStopSong();
    else
    {
        if (gGameStarted || gDemo.at1)
            sndPlaySong(gGameOptions.zLevelSong, true);
    }
}

void Update3DToggle(CGameMenuItemZBool *pItem)
{
    gStereo = pItem->at20;
}

void UpdateCDToggle(CGameMenuItemZBool *pItem)
{
    CDAudioToggle = pItem->at20;
    if (gGameStarted || gDemo.at1)
        levelTryPlayMusicOrNothing(gGameOptions.nEpisode, gGameOptions.nLevel);
}

void UpdateSoundVolume(CGameMenuItemSlider *pItem)
{
    sndSetFXVolume(pItem->nValue);
}

void UpdateMusicVolume(CGameMenuItemSlider *pItem)
{
    sndSetMusicVolume(pItem->nValue);
}

void UpdateSoundRate(CGameMenuItemZCycle *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
}

void UpdateNumVoices(CGameMenuItemSlider *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
}

void UpdateMusicDevice(CGameMenuItemZCycle *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    itemOptionsSoundSF2Bank.bEnable = 0;
    itemOptionsSoundSF2Bank.bNoDraw = 1;
    switch (nMusicDeviceValues[itemOptionsSoundMusicDevice.m_nFocus])
    {
    case ASS_SF2:
        itemOptionsSoundSF2Bank.bEnable = 1;
        itemOptionsSoundSF2Bank.bNoDraw = 0;
        break;
    }
}

void SetSound(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    MixRate = nSoundRateValues[itemOptionsSoundSampleRate.m_nFocus];
    NumVoices = itemOptionsSoundNumVoices.nValue;
    MusicDevice = nMusicDeviceValues[itemOptionsSoundMusicDevice.m_nFocus];
    Bstrcpy(SF2_BankFile, sf2bankfile);
    sfxTerm();
    sndTerm();

    sndInit();
    sfxInit();

    if (MusicToggle && (gGameStarted || gDemo.at1))
        sndPlaySong(gGameOptions.zLevelSong, true);
}

void PreDrawSound(CGameMenuItem *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
}

void SetupOptionsSound(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    itemOptionsSoundSoundToggle.at20 = SoundToggle;
    itemOptionsSoundMusicToggle.at20 = MusicToggle;
    itemOptionsSound3DToggle.at20 = gStereo;
    itemOptionsSoundCDToggle.at20 = CDAudioToggle;
    itemOptionsSoundSampleRate.m_nFocus = 0;
    for (int i = 0; i < 3; i++)
    {
        if (nSoundRateValues[i] == MixRate)
        {
            itemOptionsSoundSampleRate.m_nFocus = i;
            break;
        }
    }
    itemOptionsSoundNumVoices.nValue = NumVoices;
    itemOptionsSoundMusicDevice.m_nFocus = 0;
    for (int i = 0; i < (int)ARRAY_SIZE(nMusicDeviceValues); i++)
    {
        if (nMusicDeviceValues[i] == MusicDevice)
        {
            itemOptionsSoundMusicDevice.m_nFocus = i;
            break;
        }
    }

    UpdateMusicDevice(NULL);
}

void SetupPollJoystick(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    if (CONTROL_JoyPresent && CONTROL_JoystickEnabled && !itemOptionsControlJoystickButtons.bEnable) // if joysticks menu was never initialized, and a controller is now detected, setup the menu
    {
        itemOptionsControlJoystickButtons.bEnable = 1;
        itemOptionsControlJoystickAxes.bEnable = 1;
        itemOptionsControlJoystickMisc.bEnable = 1;
        SetupJoystickMenu();
    }
}

void UpdatePlayerName(CGameMenuItemZEdit *pItem, CGameMenuEvent *pEvent)
{
    UNREFERENCED_PARAMETER(pItem);
    if (pEvent->at0 == kMenuEventEnter)
        netBroadcastPlayerInfo(myconnectindex);
}

void SetMouseAimMode(CGameMenuItemZBool *pItem)
{
    gMouseAiming = pItem->at20;
}

void SetMouseVerticalAim(CGameMenuItemZBool *pItem)
{
    gMouseAim = pItem->at20;
}

void SetMouseXSensitivity(CGameMenuItemSliderFloat *pItem)
{
    CONTROL_MouseAxesSensitivity[0] = pItem->fValue;
}

void SetMouseYSensitivity(CGameMenuItemSliderFloat*pItem)
{
    CONTROL_MouseAxesSensitivity[1] = pItem->fValue;
}

void SetupMouseMenu(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    itemOptionsControlMouseAimFlipped.at20 = gMouseAimingFlipped;
    itemOptionsControlMouseAimMode.at20 = gMouseAiming;
    itemOptionsControlMouseVerticalAim.at20 = gMouseAim;
    // itemOptionsControlMouseXScale.nValue = CONTROL_MouseAxesScale[0];
    // itemOptionsControlMouseYScale.nValue = CONTROL_MouseAxesScale[1];
}

void SetupJoystickButtonsMenu(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    const int nMaxJoyButtons = (joystick.numButtons * 2) + ((joystick.numHats > 0) * 4);
    for (int nPage = 0; nPage < MAXJOYSTICKBUTTONPAGES; nPage++) // go through each axis and setup binds
    {
        for (int nButton = 0; nButton < JOYSTICKITEMSPERPAGE; nButton++)
        {
            if (nButton >= nMaxJoyButtons) // reached end of button list
                return;
            const char bDoubleTap = nButton & 1;
            const int nJoyButton = ((nPage * JOYSTICKITEMSPERPAGE)>>1) + (nButton>>1); // we halve the button index because button lists are listed in pairs of single tap/double tap inputs
            auto pButton = pItemOptionsControlJoyButton[nPage][nButton];
            if (!pButton)
                break;
            pButton->m_nFocus = 0;
            for (int j = 0; j < NUMGAMEFUNCTIONS+1; j++)
            {
                if (JoystickFunctions[nJoyButton][bDoubleTap] == nGamefuncsValues[j])
                {
                    pButton->m_nFocus = j;
                    break;
                }
            }
        }
    }
}

void SetupJoystickAxesMenu(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    for (int nAxis = 0; nAxis < MAXJOYAXES; nAxis++) // set settings for each listed joystick axis
    {
        if (pItemOptionsControlJoystickAxis[nAxis] == NULL) // reached end of list, stop
            return;
        pItemOptionsControlJoystickAxisScale[nAxis]->nValue = JoystickAnalogueScale[nAxis];
        switch (JoystickAnalogueAxes[nAxis])
        {
        case analog_lookingupanddown:
            pItemOptionsControlJoystickAxisAnalogue[nAxis]->m_nFocus = 4;
            break;
        case analog_moving:
            pItemOptionsControlJoystickAxisAnalogue[nAxis]->m_nFocus = 3;
            break;
        case analog_strafing:
            pItemOptionsControlJoystickAxisAnalogue[nAxis]->m_nFocus = 2;
            break;
        case analog_turning:
            pItemOptionsControlJoystickAxisAnalogue[nAxis]->m_nFocus = 1;
            break;
        default: // unsupported/none
            pItemOptionsControlJoystickAxisAnalogue[nAxis]->m_nFocus = 0;
            break;
        }
        pItemOptionsControlJoystickAxisAnalogueInvert[nAxis]->at20 = JoystickAnalogueInvert[nAxis];
        pItemOptionsControlJoystickAxisDigitalPos[nAxis]->m_nFocus = 0;
        for (int j = 0; j < NUMGAMEFUNCTIONS+1; j++)
        {
            if (JoystickDigitalFunctions[nAxis][0] == nGamefuncsValues[j])
            {
                pItemOptionsControlJoystickAxisDigitalPos[nAxis]->m_nFocus = j;
                break;
            }
        }
        pItemOptionsControlJoystickAxisDigitalNeg[nAxis]->m_nFocus = 0;
        for (int j = 0; j < NUMGAMEFUNCTIONS+1; j++)
        {
            if (JoystickDigitalFunctions[nAxis][1] == nGamefuncsValues[j])
            {
                pItemOptionsControlJoystickAxisDigitalNeg[nAxis]->m_nFocus = j;
                break;
            }
        }
        pItemOptionsControlJoystickAxisDeadzone[nAxis]->nValue = JoystickAnalogueDead[nAxis];
        pItemOptionsControlJoystickAxisSaturate[nAxis]->nValue = JoystickAnalogueSaturate[nAxis];
    }
}

void SetJoystickScale(CGameMenuItemSlider* pItem)
{
    for (int nAxis = 0; nAxis < MAXJOYAXES; nAxis++)
    {
        if (pItem == pItemOptionsControlJoystickAxisScale[nAxis])
        {
            JoystickAnalogueScale[nAxis] = pItem->nValue;
            CONTROL_SetAnalogAxisScale(nAxis, JoystickAnalogueScale[nAxis], controldevice_joystick);
            break;
        }
    }
}

void SetJoystickAnalogue(CGameMenuItemZCycle* pItem)
{
    for (int nAxis = 0; nAxis < MAXJOYAXES; nAxis++)
    {
        if (pItem == pItemOptionsControlJoystickAxisAnalogue[nAxis])
        {
            switch (pItem->m_nFocus)
            {
            case 4: // looking up/down
                JoystickAnalogueAxes[nAxis] = analog_lookingupanddown;
                break;
            case 3: // moving
                JoystickAnalogueAxes[nAxis] = analog_moving;
                break;
            case 2: // strafing
                JoystickAnalogueAxes[nAxis] = analog_strafing;
                break;
            case 1: // turning
                JoystickAnalogueAxes[nAxis] = analog_turning;
                break;
            case 0: // none
            default:
                JoystickAnalogueAxes[nAxis] = -1;
                break;
            }
            CONTROL_MapAnalogAxis(nAxis, JoystickAnalogueAxes[nAxis]);
            break;
        }
    }
}

void SetJoystickAnalogueInvert(CGameMenuItemZBool* pItem)
{
    for (int nAxis = 0; nAxis < MAXJOYAXES; nAxis++)
    {
        if (pItem == pItemOptionsControlJoystickAxisAnalogueInvert[nAxis])
        {
            JoystickAnalogueInvert[nAxis] = pItem->at20;
            CONTROL_SetAnalogAxisInvert(nAxis, JoystickAnalogueInvert[nAxis]);
            break;
        }
    }
}

void SetJoystickDigitalPos(CGameMenuItemZCycle* pItem)
{
    for (int nAxis = 0; nAxis < MAXJOYAXES; nAxis++)
    {
        if (pItem == pItemOptionsControlJoystickAxisDigitalPos[nAxis])
        {
            for (int j = 0; j < NUMGAMEFUNCTIONS+1; j++)
            {
                if (JoystickDigitalFunctions[nAxis][0] == nGamefuncsValues[j])
                {
                    int nFunc = nGamefuncsValues[pItem->m_nFocus];
                    JoystickDigitalFunctions[nAxis][0] = nFunc;
                    CONTROL_MapDigitalAxis(nAxis, JoystickDigitalFunctions[nAxis][0], 0);
                    return;
                }
            }
        }
    }
}

void SetJoystickDigitalNeg(CGameMenuItemZCycle* pItem)
{
    for (int nAxis = 0; nAxis < MAXJOYAXES; nAxis++)
    {
        if (pItem == pItemOptionsControlJoystickAxisDigitalNeg[nAxis])
        {
            for (int j = 0; j < NUMGAMEFUNCTIONS+1; j++)
            {
                if (JoystickDigitalFunctions[nAxis][1] == nGamefuncsValues[j])
                {
                    int nFunc = nGamefuncsValues[pItem->m_nFocus];
                    JoystickDigitalFunctions[nAxis][1] = nFunc;
                    CONTROL_MapDigitalAxis(nAxis, JoystickDigitalFunctions[nAxis][1], 1);
                    return;
                }
            }
        }
    }
}

void SetJoystickDeadzone(CGameMenuItemSlider* pItem)
{
    for (int nAxis = 0; nAxis < MAXJOYAXES; nAxis++)
    {
        if (pItem == pItemOptionsControlJoystickAxisDeadzone[nAxis])
        {
            JoystickAnalogueDead[nAxis] = pItem->nValue;
            JOYSTICK_SetDeadZone(nAxis, JoystickAnalogueDead[nAxis], JoystickAnalogueSaturate[nAxis]);
            break;
        }
    }
}

void SetJoystickSaturate(CGameMenuItemSlider* pItem)
{
    for (int nAxis = 0; nAxis < MAXJOYAXES; nAxis++)
    {
        if (pItem == pItemOptionsControlJoystickAxisSaturate[nAxis])
        {
            JoystickAnalogueSaturate[nAxis] = pItem->nValue;
            JOYSTICK_SetDeadZone(nAxis, JoystickAnalogueDead[nAxis], JoystickAnalogueSaturate[nAxis]);
            break;
        }
    }
}

void PreDrawControlMouse(CGameMenuItem *pItem)
{
    if (pItem == &itemOptionsControlMouseVerticalAim)
        pItem->bEnable = !gMouseAiming;
}

void SetMouseButton(CGameMenuItemZCycle *pItem)
{
    for (int i = 0; i < MENUMOUSEFUNCTIONS; i++)
    {
        if (pItem == pItemOptionsControlMouseButton[i])
        {
            int nFunc = nGamefuncsValues[pItem->m_nFocus];
            MouseFunctions[MenuMouseDataIndex[i][0]][MenuMouseDataIndex[i][1]] = nFunc;
            CONTROL_MapButton(nFunc, MenuMouseDataIndex[i][0], MenuMouseDataIndex[i][1], controldevice_mouse);
            CONTROL_FreeMouseBind(MenuMouseDataIndex[i][0]);
            break;
        }
    }
}

void SetJoyButton(CGameMenuItemZCycle *pItem)
{
    for (int nPage = 0; nPage < MAXJOYSTICKBUTTONPAGES; nPage++) // find selected menu item used for this bind
    {
        for (int nButton = 0; nButton < JOYSTICKITEMSPERPAGE; nButton++)
        {
            if (pItem == pItemOptionsControlJoyButton[nPage][nButton]) // found menu item, now bind function to joystick button
            {
                const char bDoubleTap = nButton & 1;
                const int nJoyButton = ((nPage * JOYSTICKITEMSPERPAGE)>>1) + (nButton>>1); // we halve the button index because button lists are listed in pairs of single tap/double tap inputs
                int nFunc = nGamefuncsValues[pItem->m_nFocus];
                JoystickFunctions[nJoyButton][bDoubleTap] = nFunc;
                CONTROL_MapButton(nFunc, nJoyButton, bDoubleTap, controldevice_joystick);
                return;
            }
        }
    }
}

void SetupMouseButtonMenu(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    for (int i = 0; i < MENUMOUSEFUNCTIONS; i++)
    {
        auto pItem = pItemOptionsControlMouseButton[i];
        pItem->m_nFocus = 0;
        for (int j = 0; j < NUMGAMEFUNCTIONS+1; j++)
        {
            if (MouseFunctions[MenuMouseDataIndex[i][0]][MenuMouseDataIndex[i][1]] == nGamefuncsValues[j])
            {
                pItem->m_nFocus = j;
                break;
            }
        }
    }
}

void SetupNetworkMenu(void)
{
    if (strlen(gNetAddress) > 0)
        strncpy(zNetAddressBuffer, gNetAddress, sizeof(zNetAddressBuffer)-1);

    menuNetwork.Add(&itemNetworkTitle, false);
    menuNetwork.Add(&itemNetworkHost, true);
    menuNetwork.Add(&itemNetworkJoin, false);
    menuNetwork.Add(&itemBloodQAV, false);

    menuNetworkHost.Add(&itemNetworkHostTitle, false);
    menuNetworkHost.Add(&itemNetworkHostPlayerNum, true);
    menuNetworkHost.Add(&itemNetworkHostPort, false);
    menuNetworkHost.Add(&itemNetworkHostHost, false);
    menuNetworkHost.Add(&itemBloodQAV, false);

    menuNetworkJoin.Add(&itemNetworkJoinTitle, false);
    menuNetworkJoin.Add(&itemNetworkJoinAddress, true);
    menuNetworkJoin.Add(&itemNetworkJoinPort, false);
    menuNetworkJoin.Add(&itemNetworkJoinJoin, false);
    menuNetworkJoin.Add(&itemBloodQAV, false);
}

void SetupNetworkHostMenu(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
}

void SetupNetworkJoinMenu(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
}

void NetworkHostGame(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    sndStopSong();
    FX_StopAllSounds();
    UpdateDacs(0, true);
    gNetPlayers = itemNetworkHostPlayerNum.nValue;
    gNetPort = strtoul(zNetPortBuffer, NULL, 10);
    if (!gNetPort)
        gNetPort = kNetDefaultPort;
    gNetMode = NETWORK_SERVER;
    netInitialize(false);
    gGameMenuMgr.Deactivate();
    gQuitGame = gRestartGame = true;
}

void NetworkJoinGame(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    sndStopSong();
    FX_StopAllSounds();
    UpdateDacs(0, true);
    strcpy(gNetAddress, zNetAddressBuffer);
    gNetPort = strtoul(zNetPortBuffer, NULL, 10);
    if (!gNetPort)
        gNetPort = kNetDefaultPort;
    gNetMode = NETWORK_CLIENT;
    netInitialize(false);
    gGameMenuMgr.Deactivate();
    gQuitGame = gRestartGame = true;
}

void SaveGameProcess(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
}

void TenProcess(CGameMenuItem7EA1C *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
}

static void UpdateSaveGameItemText(int nSlot)
{
    switch (nSlot) // set save slot text flag
    {
    case 0:
        itemSaveGame1.at37 = 0;
        break;
    case 1:
        itemSaveGame2.at37 = 0;
        break;
    case 2:
        itemSaveGame3.at37 = 0;
        break;
    case 3:
        itemSaveGame4.at37 = 0;
        break;
    case 4:
        itemSaveGame5.at37 = 0;
        break;
    case 5:
        itemSaveGame6.at37 = 0;
        break;
    case 6:
        itemSaveGame7.at37 = 0;
        break;
    case 7:
        itemSaveGame8.at37 = 0;
        break;
    case 8:
        itemSaveGame9.at37 = 0;
        break;
    case 9:
        itemSaveGame10.at37 = 0;
        break;
    default:
        break;
    }
}

short gQuickLoadSlot = -1;
short gQuickSaveSlot = -1;

void SaveGame(CGameMenuItemZEditBitmap *pItem, CGameMenuEvent *event)
{
    char strSaveGameName[BMAX_PATH];
    int nSlot = pItem->at28;
    if (gGameOptions.nGameType != kGameTypeSinglePlayer || !gGameStarted)
        return;
    if (event->at0 != 6/* || strSaveGameName[0]*/)
    {
        gGameMenuMgr.Deactivate();
        return;
    }
    if (G_ModDirSnprintf(strSaveGameName, BMAX_PATH, "game00%02d.sav", nSlot))
        return;
    memset(gGameOptions.szUserGameName, 0, sizeof(gGameOptions.szSaveGameName));
    strcpy(gGameOptions.szUserGameName, strRestoreGameStrings[nSlot]);
    memset(gGameOptions.szSaveGameName, 0, sizeof(gGameOptions.szSaveGameName));
    sprintf(gGameOptions.szSaveGameName, "%s", strSaveGameName);
    gGameOptions.nSaveGameSlot = nSlot;
    viewLoadingScreen(gMenuPicnum, "Saving", "Saving Your Game", strRestoreGameStrings[nSlot]);
    videoNextPage();
    gSaveGameNum = nSlot;
    gQuickSaveSlot = nSlot;
    LoadSave::SaveGame(strSaveGameName);
    gGameOptions.picEntry = gSavedOffset;
    gSaveGameOptions[nSlot] = gGameOptions;
    UpdateSavedInfo(nSlot);
    UpdateSaveGameItemText(nSlot);
    gGameMenuMgr.Deactivate();
    viewSetMessage("Game saved");
    viewUpdatePages();
}

void QuickSaveGame(void)
{
    char strSaveGameName[BMAX_PATH];
    if (gGameOptions.nGameType != kGameTypeSinglePlayer || !gGameStarted)
        return;
    /*if (strSaveGameName[0])
    {
        gGameMenuMgr.Deactivate();
        return;
    }*/
    if (G_ModDirSnprintf(strSaveGameName, BMAX_PATH, "game00%02d.sav", gQuickSaveSlot))
        return;
    memset(gGameOptions.szUserGameName, 0, sizeof(gGameOptions.szSaveGameName));
    strcpy(gGameOptions.szUserGameName, strRestoreGameStrings[gQuickSaveSlot]);
    memset(gGameOptions.szSaveGameName, 0, sizeof(gGameOptions.szSaveGameName));
    sprintf(gGameOptions.szSaveGameName, "%s", strSaveGameName);
    gGameOptions.nSaveGameSlot = gQuickSaveSlot;
    viewLoadingScreen(gMenuPicnum, "Saving", "Saving Your Game", strRestoreGameStrings[gQuickSaveSlot]);
    videoNextPage();
    LoadSave::SaveGame(strSaveGameName);
    gGameOptions.picEntry = gSavedOffset;
    gSaveGameOptions[gQuickSaveSlot] = gGameOptions;
    UpdateSavedInfo(gQuickSaveSlot);
    UpdateSaveGameItemText(gQuickSaveSlot);
    gGameMenuMgr.Deactivate();
    viewSetMessage("Game saved");
    viewUpdatePages();
}

void LoadGame(CGameMenuItemZEditBitmap *pItem, CGameMenuEvent *event)
{
    UNREFERENCED_PARAMETER(event);
    char strLoadGameName[BMAX_PATH];
    int nSlot = pItem->at28;
    if (gGameOptions.nGameType != kGameTypeSinglePlayer)
        return;
    if (G_ModDirSnprintf(strLoadGameName, BMAX_PATH, "game00%02d.sav", nSlot))
        return;
    int const bakpathsearchmode = pathsearchmode;
    pathsearchmode = 1;
    if (!testkopen(strLoadGameName, 0))
    {
        pathsearchmode = bakpathsearchmode;
        return;
    }
    viewLoadingScreen(gMenuPicnum, "Loading", "Loading Saved Game", strRestoreGameStrings[nSlot]);
    videoNextPage();
    LoadSave::LoadGame(strLoadGameName);
    gGameMenuMgr.Deactivate();
    gQuickLoadSlot = nSlot;
    pathsearchmode = bakpathsearchmode;
}

void QuickLoadGame(void)
{
    char strLoadGameName[BMAX_PATH];
    if (gGameOptions.nGameType != kGameTypeSinglePlayer)
        return;
    if (G_ModDirSnprintf(strLoadGameName, BMAX_PATH, "game00%02d.sav", gQuickLoadSlot))
        return;
    int const bakpathsearchmode = pathsearchmode;
    pathsearchmode = 1;
    if (!testkopen(strLoadGameName, 0))
    {
        pathsearchmode = bakpathsearchmode;
        return;
    }
    viewLoadingScreen(gMenuPicnum, "Loading", "Loading Saved Game", strRestoreGameStrings[gQuickLoadSlot]);
    videoNextPage();
    LoadSave::LoadGame(strLoadGameName);
    gGameMenuMgr.Deactivate();
    pathsearchmode = bakpathsearchmode;
}

void ClearUserMapNameOnLevelChange(CGameMenuItemZCycle *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    memset(zUserMapName, 0, sizeof(zUserMapName));
}

void SetupLevelMenuItem(int nEpisode)
{
    dassert(nEpisode >= 0 && nEpisode < gEpisodeCount);
    itemNetStart3.SetTextArray(zLevelNames[nEpisode], gEpisodeInfo[nEpisode].nLevels, 0);
}

void SetupNetLevels(CGameMenuItemZCycle *pItem)
{
    memset(zUserMapName, 0, sizeof(zUserMapName));
    SetupLevelMenuItem(pItem->m_nFocus);
}

void StartNetGame(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    gPacketStartGame.gameType = itemNetStart1.m_nFocus+1;
    if (gPacketStartGame.gameType == 0)
        gPacketStartGame.gameType = 2;
    gPacketStartGame.episodeId = itemNetStart2.m_nFocus;
    gPacketStartGame.levelId = itemNetStart3.m_nFocus;
    gPacketStartGame.difficulty = itemNetStart4.m_nFocus;
    gPacketStartGame.monsterSettings = itemNetStart5.m_nFocus;
    gPacketStartGame.weaponSettings = itemNetStart6.m_nFocus;
    gPacketStartGame.itemSettings = itemNetStart7.m_nFocus;
    gPacketStartGame.respawnSettings = 0;
    gPacketStartGame.bFriendlyFire = itemNetStart8.at20;
    gPacketStartGame.bPlayerKeys = (PLAYERKEYSMODE) itemNetStart9.m_nFocus;
    ////
    gPacketStartGame.weaponsV10x = itemNetStart10.at20;
    ////
    gPacketStartGame.unk = 0;
    Bstrncpy(gPacketStartGame.userMapName, zUserMapName, sizeof(zUserMapName));
    gPacketStartGame.userMap = gPacketStartGame.userMapName[0] != 0;

    netBroadcastNewGame();
    gStartNewGame = 1;
    gGameMenuMgr.Deactivate();
}

void Restart(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    if (gGameOptions.nGameType == kGameTypeSinglePlayer || numplayers == 1)
    {
        gQuitGame = true;
        gRestartGame = true;
    }
    else
        gQuitRequest = 2;
    gGameMenuMgr.Deactivate();
}

void Quit(CGameMenuItemChain *pItem)
{
    UNREFERENCED_PARAMETER(pItem);
    if (gGameOptions.nGameType == kGameTypeSinglePlayer || numplayers == 1)
        gQuitGame = true;
    else
        gQuitRequest = 1;
    gGameMenuMgr.Deactivate();
}

void SetParentalLock(CGameMenuItemZBool *pItem)
{
    if (!pItem->at20)
    {
        pItem->at20 = true;
        pItem->Draw();
        if (strcmp(itemParentalLockPassword.at20, ""))
        {
            itemParentalLockPassword.pMenu->FocusNextItem();
            itemParentalLockPassword.at32 = 0;
            itemParentalLockPassword.at37 = 1;
            itemParentalLockPassword.at5f = pItem;
            itemParentalLockPassword.at29[0] = 0;
            return;
        }
        else
        {
            itemParentalLockPassword.at20[0] = 0;
            pItem->Draw();
            gbAdultContent = false;
        }
    }
    else
        gbAdultContent = true;
    // NUKE-TODO: CONFIG_WriteAdultMode();
}

void MenuSetupEpisodeInfo(void)
{
    memset(zEpisodeNames, 0, sizeof(zEpisodeNames));
    memset(zLevelNames, 0, sizeof(zLevelNames));
    for (int i = 0; i < 6; i++)
    {
        if (i < gEpisodeCount)
        {
            EPISODEINFO *pEpisode = &gEpisodeInfo[i];
            zEpisodeNames[i] = pEpisode->title;
            for (int j = 0; j < kMaxLevels; j++)
            {
                if (j < pEpisode->nLevels)
                {
                    zLevelNames[i][j] = pEpisode->levelsInfo[j].Title;
                }
            }
        }
    }
}

void drawLoadingScreen(int nTile)
{
    char buffer[80];
    if (gGameOptions.nGameType == kGameTypeSinglePlayer)
    {
        if (gDemo.at1)
            sprintf(buffer, "Loading Demo");
        else
            sprintf(buffer, "Loading Level");
    }
    else
        sprintf(buffer, "%s", zNetGameTypes[gGameOptions.nGameType-1]);
    viewLoadingScreen(nTile, buffer, levelGetTitle(), NULL);
}
