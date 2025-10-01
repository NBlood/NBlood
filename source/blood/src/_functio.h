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

// _functio.h

// file created by makehead.exe
// these headers contain default key assignments, as well as
// default button assignments and game function names
// axis defaults are also included

#include "_control.h"
#include "control.h"

#ifndef function_private_h_
#define function_private_h_
#ifdef __cplusplus
extern "C" {
#endif
// KEEPINSYNC lunatic/con_lang.lua
char gamefunctions[NUMGAMEFUNCTIONS][MAXGAMEFUNCLEN] =
   {
   "Move_Forward",
   "Move_Backward",
   "Turn_Left",
   "Turn_Right",
   "Turn_Around",
   "Strafe",
   "Strafe_Left",
   "Strafe_Right",
   "Jump",
   "Crouch",
   "Crouch_Toggle",
   "Run",
   "AutoRun",
   "Open",
   "Weapon_Fire",
   "Weapon_Special_Fire",
   "Aim_Up",
   "Aim_Down",
   "Aim_Center",
   "Look_Up",
   "Look_Down",
   "Tilt_Left",
   "Tilt_Right",
   "Weapon_1",
   "Weapon_2",
   "Weapon_3",
   "Weapon_4",
   "Weapon_5",
   "Weapon_6",
   "Weapon_7",
   "Weapon_8",
   "Weapon_9",
   "Weapon_10",
   "Inventory_Use",
   "Inventory_Left",
   "Inventory_Right",
   "Map_Toggle",
   "Map_Follow_Mode",
   "Shrink_Screen",
   "Enlarge_Screen",
   "Send_Message",
   "See_Coop_View",
   "See_Chase_View",
   "Mouse_Aiming",
   "Toggle_Crosshair",
   "Next_Weapon",
   "Previous_Weapon",
   "Last_Weapon",
   "Holster_Weapon",
   "Show_Opponents_Weapon",
   "BeastVision",
   "CrystalBall",
   "JumpBoots",
   "MedKit",
   "ProximityBombs",
   "RemoteBombs",
   "Show_Console",
   "Quick_Save",
   "Quick_Load",
   };

#ifdef __SETUP__

const char keydefaults[NUMGAMEFUNCTIONS*2][MAXGAMEFUNCLEN] =
   {
   "W", "Kpad8",
   "S", "Kpad2",
   "Left", "Kpad4",
   "Right", "KPad6",
   "BakSpc", "",
   "LAlt", "RAlt",
   "A", "",
   "D", "",
   "Space", "/",
   "LCtrl", "",
   "", "",
   "LShift", "RShift",
   "CapLck", "",
   "E", "",
   "RCtrl", "",
   "X", "",
   "Home", "KPad7",
   "End", "Kpad1",
   "KPad5", "",
   "PgUp", "Kpad9",
   "PgDn", "Kpad3",
   "Insert", "Kpad0",
   "Delete", "Kpad.",
   "1", "",
   "2", "",
   "3", "",
   "4", "",
   "5", "",
   "6", "",
   "7", "",
   "8", "",
   "9", "",
   "0", "",
   "Enter", "KpdEnt",
   "[", "",
   "]", "",
   "Tab", "",
   "F", "",
   "-", "Kpad-",
   "=", "Kpad+",
   "T", "",
   "K", "",
   "F7", "",
   "U", "",
   "I", "",
   "'", "",
   ";", "",
   "Q", "",
   "ScrLck", "",
   "Y", "",
   "B", "",
   "C", "",
   "J", "",
   "M", "",
   "P", "",
   "R", "",
   "`", "",
   "F6", "",
   "F9", "",
   };

const char oldkeydefaults[NUMGAMEFUNCTIONS*2][MAXGAMEFUNCLEN] =
   {
   "Up", "Kpad8",
   "Down", "Kpad2",
   "Left", "Kpad4",
   "Right", "KPad6",
   "BakSpc", "",
   "LAlt", "RAlt",
   ",", "",
   ".", "",
   "A", "/",
   "Z", "",
   "", "",
   "LShift", "RShift",
   "CapLck", "",
   "Space", "",
   "LCtrl", "RCtrl",
   "X", "",
   "Home", "KPad7",
   "End", "Kpad1",
   "KPad5", "",
   "PgUp", "Kpad9",
   "PgDn", "Kpad3",
   "Insert", "Kpad0",
   "Delete", "Kpad.",
   "1", "",
   "2", "",
   "3", "",
   "4", "",
   "5", "",
   "6", "",
   "7", "",
   "8", "",
   "9", "",
   "0", "",
   "Enter", "KpdEnt",
   "[", "",
   "]", "",
   "Tab", "",
   "F", "",
   "-", "Kpad-",
   "=", "Kpad+",
   "T", "",
   "K", "",
   "F7", "",
   "U", "",
   "I", "",
   "'", "",
   ";", "",
   "", "",
   "ScrLck", "",
   "W", "",
   "B", "",
   "C", "",
   "J", "",
   "M", "",
   "P", "",
   "R", "",
   "`", "",
   "F6", "",
   "F9", "",
   };

static const char * mousedefaults[MAXMOUSEBUTTONS] =
   {
   "Weapon_Fire",
   "Weapon_Special_Fire",
   "",
   "",
   "Previous_Weapon",
   "Next_Weapon",
   };


static const char * mouseclickeddefaults[MAXMOUSEBUTTONS] =
   {
   };

#if 0
static const char * mouseanalogdefaults[MAXMOUSEAXES] =
   {
   "analog_turning",
   "analog_moving",
   };
#endif

#if defined(GEKKO)
static const char * joystickdefaults[MAXJOYBUTTONSANDHATS] =
   {
   "Open", // A
   "Fire", // B
   "Run", // 1
   "Map", // 2
   "Previous_Weapon", // -
   "Next_Weapon", // +
   "", // Home
   "Jump", // Z
   "Crouch_Toggle", // C
   "Map", // X
   "Run", // Y
   "Jump", // L
   "Quick_Kick", // R
   "Crouch_Toggle", // ZL
   "Fire", // ZR
   "Quick_Kick", // D-Pad Up
   "Inventory_Right", // D-Pad Right
   "Inventory", // D-Pad Down
   "Inventory_Left", // D-Pad Left
   };


static const char * joystickclickeddefaults[MAXJOYBUTTONSANDHATS] =
   {
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "Inventory",
   };


static const char * joystickanalogdefaults[MAXJOYAXES] =
   {
   "analog_strafing",
   "analog_moving",
   "analog_turning",
   "analog_lookingupanddown",
   };


static const char * joystickdigitaldefaults[MAXJOYDIGITAL] =
   {
   };
#else
static const char * joystickdefaults[MAXJOYBUTTONSANDHATS] =
   {
   "Jump",
   "Crouch_Toggle",
   "Open",
   "Last_Weapon",
   "",
   "",
   "",
   "",
   "Turn_Around",
   "Previous_Weapon",
   "Next_Weapon",
   "Last_Weapon",
   "Inventory_Use",
   "Inventory_Left",
   "Inventory_Right",
   };


static const char * joystickclickeddefaults[MAXJOYBUTTONSANDHATS] =
   {
   };


static const char * joystickanalogdefaults[MAXJOYAXES] =
   {
   "analog_strafing",
   "analog_moving",
   "analog_turning",
   "analog_lookingupanddown",
   };


static const int32_t joystickanalogscaledefaults[MAXJOYAXES] =
   {
   DEFAULTJOYSTICKANALOGUESCALE/2,
   DEFAULTJOYSTICKANALOGUESCALE,
   DEFAULTJOYSTICKANALOGUESCALE/3*2,
   DEFAULTJOYSTICKANALOGUESCALE/4,
   };


static const int32_t joystickanalogdeaddefaults[MAXJOYAXES] =
   {
   DEFAULTJOYSTICKANALOGUEDEAD*5,
   DEFAULTJOYSTICKANALOGUEDEAD*4,
   DEFAULTJOYSTICKANALOGUEDEAD*2,
   DEFAULTJOYSTICKANALOGUEDEAD*2,
   };


static const int32_t joystickanalogsaturatedefaults[MAXJOYAXES] =
   {
   DEFAULTJOYSTICKANALOGUESATURATE*3,
   DEFAULTJOYSTICKANALOGUESATURATE*3,
   DEFAULTJOYSTICKANALOGUESATURATE*2,
   DEFAULTJOYSTICKANALOGUESATURATE*2,
   };


static const char * joystickdigitaldefaults[MAXJOYDIGITAL] =
   {
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "Weapon_Special_Fire",
   "",
   "Weapon_Fire",
   };
#endif

#endif
#ifdef __cplusplus
}
#endif
#endif
