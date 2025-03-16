//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment

This file is NOT part of Duke Nukem 3D version 1.5 - Atomic Edition
However, it is either an older version of a file that is, or is
some test code written during the development of Duke Nukem 3D.
This file is provided purely for educational interest.

Duke Nukem 3D is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

Prepared for public release: 03/21/2003 - Charlie Wiederhold, 3D Realms
*/
//-------------------------------------------------------------------------

//****************************************************************************
//
// Private header for CONTROL.C
//
//****************************************************************************

#pragma once

#ifndef control_private_h_
#define control_private_h_

#include "compat.h"
#include "control.h"
#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXISUNDEFINED   0x7f
#define BUTTONUNDEFINED 0x7f
#define KEYUNDEFINED    0x7f

#define DIGITALAXISANALOGTHRESHOLD    (0x200 * 32767 / 10000)
#define MINDIGITALAXISANALOGTHRESHOLD (0x80 * 32767 / 10000)

#define DEFAULTMOUSESENSITIVITY  (25.f)
#define DEFAULTMOUSEUNIT (.031337f)
#define DEFAULTJOYUNIT (.2f)

#define INSTANT_ONOFF       0
#define TOGGLE_ONOFF        1

// this is higher than the -32767 to 32767 range of the axis
#define MAXSCALEDCONTROLVALUE  0x1ffff

#define DEFAULTAXISSCALE 65536
#define DEFAULTAXISDEADZONE 1000
#define DEFAULTAXISSATURATE 9500
#define DEFAULTAXISSENSITIVITY (DEFAULTAXISSCALE/16384.f)

#define BUTTONSET(x, value)     (CONTROL_ButtonState |= ((uint64_t)value << ((uint64_t)(x))))
#define BUTTONCLEAR(x)          (CONTROL_ButtonState &= ~((uint64_t)1 << ((uint64_t)(x))))
#define BUTTONHELDSET(x, value) (CONTROL_ButtonHeldState |= (uint64_t)(value << ((uint64_t)(x))))

typedef struct ControlFunctionFlags
{
    uint8_t active;
    uint8_t used;
    uint8_t toggle;
    uint8_t buttonheld;
    int32_t cleared;
} ControlFunctionFlags_t;

typedef struct ControlKeyMap
{
    kb_scancode keyPrimary;
    kb_scancode keySecondary;
} ControlKeyMap_t;

typedef struct ControlButtonMap
{
    uint8_t singleclicked;
    uint8_t doubleclicked;
} ControlButtonMap_t;

typedef struct ControlButtonState
{
    ControlButtonMap_t mapping;

    int     clicked;
    int     clickedState;
    int32_t clickedTime;
    int     state;
    int     clickedCount;
} ControlButtonState_t;

typedef struct ControllerAxisMap
{
    uint8_t analogmap;
    uint8_t minmap;
    uint8_t maxmap;
} ControllerAxisMap_t;

typedef struct ControllerAxisState
{
    int32_t analog;
    int8_t digital;
    int8_t digitalCleared;
} ControllerAxisState_t;

typedef struct ControllerAxis
{
    ControllerAxisMap_t mapping;
    ControllerAxisState_t axis;
    ControllerAxisState_t last;
    float sensitivity;
    uint16_t deadzone;
    uint16_t saturation;
    bool invert;
    bool solodeadzone;
} ControllerAxis_t;

typedef struct UserInputState
{
    UserInput local;
    int32_t   clock;
    direction repeat;
    bool      buttonCleared[4];
} UserInputState_t;

#ifdef __cplusplus
}
#endif
#endif
