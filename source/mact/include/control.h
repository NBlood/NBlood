//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment

This file is part of Duke Nukem 3D version 1.5 - Atomic Edition

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

Original Source: 1996 - Todd Replogle
Prepared for public release: 03/21/2003 - Charlie Wiederhold, 3D Realms
Modifications for JonoF's port by Jonathon Fowler (jf@jonof.id.au)
*/
//-------------------------------------------------------------------------

//***************************************************************************
//
// Public header for CONTROL.C.
//
//***************************************************************************

#pragma once

#ifndef control_public_h_
#define control_public_h_

#include "joystick.h"
#include "keyboard.h"
#include "mouse.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAXGAMEBUTTONS      64

#define BUTTON(x)     ((CONTROL_ButtonState >> ((uint64_t)(x))) & 1)
#define BUTTONHELD(x) ((CONTROL_ButtonHeldState >> ((uint64_t)(x))) & 1)

#define BUTTONJUSTPRESSED(x)  (BUTTON(x) && !BUTTONHELD(x))
#define BUTTONRELEASED(x)     (!BUTTON(x) && BUTTONHELD(x))
#define BUTTONSTATECHANGED(x) (BUTTON(x) != BUTTONHELD(x))

typedef enum
{
    axis_up,
    axis_down,
    axis_left,
    axis_right
} axisdirection;

typedef enum
{
    analog_turning,
    analog_strafing,
    analog_lookingupanddown,
    analog_elevation,
    analog_rolling,
    analog_moving,
    analog_maxtype
} analogcontrol;

typedef enum
{
    dir_North,
    dir_Up = dir_North,
    dir_NorthEast,
    dir_East,
    dir_Right = dir_East,
    dir_SouthEast,
    dir_South,
    dir_Down = dir_South,
    dir_SouthWest,
    dir_West,
    dir_Left = dir_West,
    dir_NorthWest,
    dir_None
} direction;

typedef struct
{
    union { bool button0, b_advance; };
    union { bool button1, b_return; };
    union { bool button2, b_escape; };
    direction dir;
} UserInput;

typedef struct
{
    int32_t dx;
    int32_t dy;
    int32_t dz;
    int32_t dyaw;
    int32_t dpitch;
    int32_t droll;
    int32_t mousex;
    int32_t mousey;
} ControlInfo;

typedef enum
{
    controltype_keyboard,
    controltype_keyboardandmouse,
    controltype_keyboardandjoystick
} controltype;

typedef enum
{
    controldevice_keyboard,
    controldevice_mouse,
    controldevice_joystick
} controldevice;

enum GameControllerButton : int
{
    CONTROLLER_BUTTON_INVALID = -1,
    CONTROLLER_BUTTON_A,
    CONTROLLER_BUTTON_B,
    CONTROLLER_BUTTON_X,
    CONTROLLER_BUTTON_Y,
    CONTROLLER_BUTTON_BACK,
    CONTROLLER_BUTTON_GUIDE,
    CONTROLLER_BUTTON_START,
    CONTROLLER_BUTTON_LEFTSTICK,
    CONTROLLER_BUTTON_RIGHTSTICK,
    CONTROLLER_BUTTON_LEFTSHOULDER,
    CONTROLLER_BUTTON_RIGHTSHOULDER,
    CONTROLLER_BUTTON_DPAD_UP,
    CONTROLLER_BUTTON_DPAD_DOWN,
    CONTROLLER_BUTTON_DPAD_LEFT,
    CONTROLLER_BUTTON_DPAD_RIGHT,
    CONTROLLER_BUTTON_MISC,
    CONTROLLER_BUTTON_PADDLE1,
    CONTROLLER_BUTTON_PADDLE2,
    CONTROLLER_BUTTON_PADDLE3,
    CONTROLLER_BUTTON_PADDLE4,
    CONTROLLER_BUTTON_MAX
};

enum GameControllerAxis : int
{
    CONTROLLER_AXIS_INVALID = -1,
    CONTROLLER_AXIS_LEFTX,
    CONTROLLER_AXIS_LEFTY,
    CONTROLLER_AXIS_RIGHTX,
    CONTROLLER_AXIS_RIGHTY,
    CONTROLLER_AXIS_TRIGGERLEFT,
    CONTROLLER_AXIS_TRIGGERRIGHT,
    CONTROLLER_AXIS_MAX
};

enum class LastSeenInput : unsigned char
{
    Keyboard,
    Joystick,
};

// GLOBALS
extern bool CONTROL_Started;
extern bool CONTROL_MousePresent;
extern bool CONTROL_JoyPresent;
extern bool CONTROL_MouseEnabled;
extern bool CONTROL_JoystickEnabled;

extern uint64_t CONTROL_ButtonState;
extern uint64_t CONTROL_ButtonHeldState;

extern LastSeenInput CONTROL_LastSeenInput;


// PROTOTYPES
void CONTROL_MapKey( int32_t which, kb_scancode key1, kb_scancode key2 );
void CONTROL_MapButton(int whichfunction, int whichbutton, int doubleclicked, controldevice device);
void CONTROL_DefineFlag( int which, int toggle );
void CONTROL_ClearAssignments( void );
bool CONTROL_GetControllerAxisIsTwinAxisStick(int32_t whichaxis);
void CONTROL_GetAxisHeatMap( uint8_t *tilePtr, int32_t nWidth, int32_t nHeight, int32_t nPalBase, int32_t nPalRange, bool bDithering, int32_t nAxis );
// void CONTROL_GetFunctionInput( void );
void CONTROL_GetInput( ControlInfo *info );
void CONTROL_ClearButton( int whichbutton );
void CONTROL_ClearAllButtons( void );
bool CONTROL_Startup(controltype which, int32_t ( *TimeFunction )( void ), int32_t ticspersecond);
void CONTROL_Shutdown( void );

void CONTROL_MapAnalogAxis(int whichaxis, int whichanalog);
void CONTROL_MapDigitalAxis(int32_t whichaxis, int32_t whichfunction, int32_t direction);
void CONTROL_SetAnalogAxisScale(int32_t whichaxis, int32_t axisscale, controldevice device);
void CONTROL_SetAnalogAxisSensitivity(int32_t whichaxis, float axissens, controldevice device);
void CONTROL_SetAnalogAxisInvert(int32_t whichaxis, int32_t invert);

void CONTROL_ScanForControllers(void);

//void CONTROL_PrintKeyMap(void);
//void CONTROL_PrintControlFlag(int32_t which);
//void CONTROL_PrintAxes( void );


////////// KEY/MOUSE BIND STUFF //////////

#define MAXBOUNDKEYS MAXKEYBOARDSCAN
#define MAXMOUSEBUTTONS 10

typedef struct ConsoleKeyBind
{
    const char *key;
    char *cmdstr;
    char repeat;
    char laststate;
} ConsoleKeyBind_t;

// Direct use DEPRECATED:
extern ConsoleKeyBind_t CONTROL_KeyBinds[MAXBOUNDKEYS+MAXMOUSEBUTTONS];
extern bool CONTROL_BindsEnabled;

void CONTROL_ClearAllBinds(void);
void CONTROL_BindKey(int i, char const * cmd, int repeat, char const * keyname);
void CONTROL_BindMouse(int i, char const * cmd, int repeat, char const * keyname);
void CONTROL_FreeKeyBind(int i);
void CONTROL_FreeMouseBind(int i);

static inline int CONTROL_KeyIsBound(int const key)
{
    auto &bind = CONTROL_KeyBinds[key];
    return bind.cmdstr && bind.key;
}

void CONTROL_ProcessBinds(void);

UserInput *CONTROL_GetUserInput(UserInput *);
void CONTROL_ClearUserInput(UserInput *);

////////////////////

#define CONTROL_NUM_FLAGS   64
extern int32_t CONTROL_ButtonFlags[CONTROL_NUM_FLAGS];

#ifdef __cplusplus
}
#endif
#endif
