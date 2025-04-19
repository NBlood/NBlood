/*
 * control.c
 * MACT library controller handling
 *
 * Derived from MACT386.LIB disassembly by Jonathon Fowler
 *
 */

#include "control.h"

#include "_control.h"
#include "baselayer.h"
#include "build.h"
#include "common.h"
#include "compat.h"
#include "joystick.h"
#include "keyboard.h"
#include "mouse.h"
#include "osd.h"
#include "pragmas.h"

#ifdef __ANDROID__
#include "android.h"
#endif

// TODO: add mact cvars and make this user configurable
#define USERINPUTDELAY 500
#define USERINPUTFASTDELAY 60

bool CONTROL_Started         = false;
bool CONTROL_MouseEnabled    = false;
bool CONTROL_MousePresent    = false;
bool CONTROL_JoyPresent      = false;
bool CONTROL_JoystickEnabled = false;

uint64_t CONTROL_ButtonState     = 0;
uint64_t CONTROL_ButtonHeldState = 0;

LastSeenInput CONTROL_LastSeenInput;

float CONTROL_MouseAxesSensitivity[2] = { DEFAULTMOUSESENSITIVITY, DEFAULTMOUSESENSITIVITY };
float CONTROL_MouseSensitivity     = DEFAULTMOUSESENSITIVITY;
float CONTROL_MouseSensitivityUnit = DEFAULTMOUSEUNIT;
float CONTROL_JoySensitivityUnit   = DEFAULTJOYUNIT;

static int32_t CONTROL_NumMouseButtons  = 0;
static int32_t CONTROL_NumJoyButtons    = 0;
static int32_t CONTROL_NumJoyAxes       = 0;

static ControlFunctionFlags_t CONTROL_Flags[CONTROL_NUM_FLAGS];

static ControlKeyMap_t CONTROL_KeyMapping[CONTROL_NUM_FLAGS];

static ControllerAxis_t joyAxes[MAXJOYAXES];

static ControlButtonState_t mouseButtons[MAXMOUSEBUTTONS];
static ControlButtonState_t joyButtons[MAXJOYBUTTONS];

static UserInputState_t userInput;

static int32_t (*ExtGetTime)(void);
static int32_t ticrate;
static uint8_t CONTROL_DoubleClickSpeed;

int32_t CONTROL_ButtonFlags[CONTROL_NUM_FLAGS];
ConsoleKeyBind_t CONTROL_KeyBinds[MAXBOUNDKEYS + MAXMOUSEBUTTONS];
bool CONTROL_BindsEnabled = 0;

#define CONTROL_CheckRange(which) ((unsigned)which >= (unsigned)CONTROL_NUM_FLAGS)
#define BIND(x, s, r, k) do { Xfree(x.cmdstr); x.cmdstr = s; x.repeat = r; x.key = k; } while (0)

void CONTROL_ClearAllBinds(void)
{
    for (int i=0; i<MAXBOUNDKEYS; i++)
        CONTROL_FreeKeyBind(i);
    for (int i=0; i<MAXMOUSEBUTTONS; i++)
        CONTROL_FreeMouseBind(i);
}

void CONTROL_BindKey(int i, char const * const cmd, int repeat, char const * const keyname)
{
    BIND(CONTROL_KeyBinds[i], Xstrdup(cmd), repeat, keyname);
}

void CONTROL_BindMouse(int i, char const * const cmd, int repeat, char const * const keyname)
{
    BIND(CONTROL_KeyBinds[MAXBOUNDKEYS + i], Xstrdup(cmd), repeat, keyname);
}

void CONTROL_FreeKeyBind(int i)
{
    BIND(CONTROL_KeyBinds[i], NULL, 0, NULL);
}

void CONTROL_FreeMouseBind(int i)
{
    BIND(CONTROL_KeyBinds[MAXBOUNDKEYS + i], NULL, 0, NULL);
}

static void controlUpdateMouseState(ControlInfo *const info)
{
    vec2_t input;
    mouseReadPos(&input.x, &input.y);

    vec2f_t finput = { input.x * CONTROL_MouseSensitivityUnit * CONTROL_MouseSensitivity * CONTROL_MouseAxesSensitivity[0],
                       input.y * CONTROL_MouseSensitivityUnit * CONTROL_MouseSensitivity * CONTROL_MouseAxesSensitivity[1] };

    info->mousex = Blrintf(clamp(finput.x, -MAXSCALEDCONTROLVALUE, MAXSCALEDCONTROLVALUE));
    info->mousey = Blrintf(clamp(finput.y, -MAXSCALEDCONTROLVALUE, MAXSCALEDCONTROLVALUE));
}

static int32_t controlGetTime(void)
{
    static int32_t t = 0;
    t += 5;
    return t;
}

static void controlSetFlag(int which, int active)
{
    if (CONTROL_CheckRange(which)) return;

    ControlFunctionFlags_t &flags = CONTROL_Flags[which];

    if (flags.toggle == INSTANT_ONOFF)
        flags.active = active;
    else if (active)
        flags.buttonheld = FALSE;
    else if (flags.buttonheld == FALSE)
    {
        flags.buttonheld = TRUE;
        flags.active = (flags.active ? FALSE : TRUE);
    }
}

static int32_t controlKeyboardFunctionPressed(int32_t which)
{
    if (CONTROL_CheckRange(which) || !CONTROL_Flags[which].used)
        return FALSE;

    int r = 0;
    auto &mapped = CONTROL_KeyMapping[which];

    if (mapped.keyPrimary != KEYUNDEFINED && !CONTROL_KeyBinds[mapped.keyPrimary].cmdstr)
        r = !!KB_KeyDown[mapped.keyPrimary];

    if (mapped.keySecondary != KEYUNDEFINED && !CONTROL_KeyBinds[mapped.keySecondary].cmdstr)
        r |= !!KB_KeyDown[mapped.keySecondary];

    return r;
}

#if 0
void CONTROL_ClearKeyboardFunction(int32_t which)
{
    if (CONTROL_CheckRange(which) || !CONTROL_Flags[which].used)
        return;

    auto &mapped = CONTROL_KeyMapping[which];

    if (mapped.key1 != KEYUNDEFINED)
        KB_KeyDown[mapped.key1] = 0;

    if (mapped.key2 != KEYUNDEFINED)
        KB_KeyDown[mapped.key2] = 0;
}
#endif

void CONTROL_DefineFlag(int which, int toggle)
{
    if (CONTROL_CheckRange(which)) return;

    ControlFunctionFlags_t &flags = CONTROL_Flags[which];

    flags.active     = FALSE;
    flags.buttonheld = FALSE;
    flags.cleared    = 0;
    flags.toggle     = toggle;
    flags.used       = TRUE;
}

void CONTROL_MapKey(int32_t which, kb_scancode key1, kb_scancode key2)
{
    if (CONTROL_CheckRange(which)) return;

    CONTROL_KeyMapping[which].keyPrimary = key1 ? key1 : KEYUNDEFINED;
    CONTROL_KeyMapping[which].keySecondary = key2 ? key2 : KEYUNDEFINED;
}

#if 0
void CONTROL_PrintKeyMap(void)
{
    int32_t i;

    for (i=0; i<CONTROL_NUM_FLAGS; i++)
    {
        initprintf("function %2d key1=%3x key2=%3x\n",
                   i, CONTROL_KeyMapping[i].key1, CONTROL_KeyMapping[i].key2);
    }
}

void CONTROL_PrintControlFlag(int32_t which)
{
    initprintf("function %2d active=%d used=%d toggle=%d buttonheld=%d cleared=%d\n",
               which, CONTROL_Flags[which].active, CONTROL_Flags[which].used,
               CONTROL_Flags[which].toggle, CONTROL_Flags[which].buttonheld,
               CONTROL_Flags[which].cleared);
}

void CONTROL_PrintAxes(void)
{
    int32_t i;

    initprintf("numjoyaxes=%d\n", CONTROL_NumJoyAxes);
    for (i=0; i<CONTROL_NumJoyAxes; i++)
    {
        initprintf("axis=%d analog=%d digital1=%d digital2=%d\n",
                   i, CONTROL_JoyAxesMap[i].analogmap,
                   CONTROL_JoyAxesMap[i].minmap, CONTROL_JoyAxesMap[i].maxmap);
    }
}
#endif

void CONTROL_MapButton(int whichfunction, int whichbutton, int doubleclicked, controldevice device)
{
    ControlButtonMap_t *set;

    if (CONTROL_CheckRange(whichfunction)) whichfunction = BUTTONUNDEFINED;

    switch (device)
    {
    case controldevice_mouse:
        if ((unsigned)whichbutton >= (unsigned)MAXMOUSEBUTTONS)
            return;
        set = &mouseButtons[whichbutton].mapping;
        break;

    case controldevice_joystick:
        if ((unsigned)whichbutton >= (unsigned)MAXJOYBUTTONS)
            return;
        set = &joyButtons[whichbutton].mapping;
        break;

    default:
        return;
    }

    if (doubleclicked)
        set->doubleclicked = whichfunction;
    else
        set->singleclicked = whichfunction;
}

void CONTROL_MapAnalogAxis(int whichaxis, int whichanalog)
{
    ControllerAxisMap_t &set = joyAxes[whichaxis].mapping;

    if ((unsigned)whichanalog >= (unsigned)analog_maxtype && whichanalog != -1)
        return;

    set.analogmap = whichanalog;
}

void JOYSTICK_SetDeadZone(int32_t axis, uint16_t dead, uint16_t satur)
{
    joyAxes[axis].deadzone = dead;
    joyAxes[axis].saturation = satur;
}

void JOYSTICK_SetAxisSoloDeadZone(int32_t axis, bool dead)
{
    joyAxes[axis].solodeadzone = dead;
}

void CONTROL_SetAnalogAxisScale(int32_t whichaxis, int32_t axisscale, controldevice device)
{
    float *set;

    switch (device)
    {
    case controldevice_mouse:
        if ((unsigned) whichaxis >= ARRAY_SIZE(CONTROL_MouseAxesSensitivity))
            return;

        set = &CONTROL_MouseAxesSensitivity[whichaxis];
        break;

    case controldevice_joystick:
        if ((unsigned) whichaxis >= (unsigned) MAXJOYAXES)
            return;

        set = &joyAxes[whichaxis].sensitivity;
        break;

    default:
        return;
    }

    *set = (float)axisscale / 24576.f;
}

void CONTROL_SetAnalogAxisSensitivity(int32_t whichaxis, float axissens, controldevice device)
{
    float *set;

    switch (device)
    {
    case controldevice_mouse:
        if ((unsigned) whichaxis >= ARRAY_SIZE(CONTROL_MouseAxesSensitivity))
            return;

        set = &CONTROL_MouseAxesSensitivity[whichaxis];
        break;

    case controldevice_joystick:
        if ((unsigned) whichaxis >= (unsigned) MAXJOYAXES)
            return;

        set = &joyAxes[whichaxis].sensitivity;
        break;

    default:
        return;
    }

    *set = axissens;
}

void CONTROL_SetAnalogAxisInvert(int32_t whichaxis, int32_t invert)
{
    bool *set = &joyAxes[whichaxis].invert;
    *set = invert;
}

void CONTROL_MapDigitalAxis(int32_t whichaxis, int32_t whichfunction, int32_t direction)
{
    ControllerAxisMap_t &set = joyAxes[whichaxis].mapping;

    if (CONTROL_CheckRange(whichfunction)) whichfunction = AXISUNDEFINED;

    switch (direction)  	// JBF: this is all very much a guess. The ASM puzzles me.
    {
    case axis_up:
    case axis_left:
        set.minmap = whichfunction;
        break;
    case axis_down:
    case axis_right:
        set.maxmap = whichfunction;
        break;
    default:
        break;
    }
}

void CONTROL_ClearAssignments(void)
{
    memset(joyAxes, 0, sizeof(joyAxes));
    memset(joyButtons, 0, sizeof(joyButtons));

    for (auto &i : joyButtons)
    {
        i.mapping.doubleclicked = BUTTONUNDEFINED;
        i.mapping.singleclicked = BUTTONUNDEFINED;
    }

    memset(mouseButtons, 0, sizeof(mouseButtons));

    for (auto &i : mouseButtons)
    {
        i.mapping.doubleclicked = BUTTONUNDEFINED;
        i.mapping.singleclicked = BUTTONUNDEFINED;
    }

    memset(CONTROL_KeyMapping, KEYUNDEFINED, sizeof(CONTROL_KeyMapping));

    for (auto & i : CONTROL_MouseAxesSensitivity)
        i = DEFAULTMOUSESENSITIVITY;

    for (auto & i : joyAxes)
        i.sensitivity = DEFAULTAXISSENSITIVITY;
}

static int controlHandleClickStates(int32_t bits, int32_t tm, int32_t NumButtons, ControlButtonState_t *const b)
{
    int32_t i=NumButtons-1;
    int retval = 0;

    for (; i>=0; i--)
    {
        int const bs = (bits >> i) & 1;

        b[i].state  = bs;
        b[i].clickedState = FALSE;

        if (bs)
        {
            retval = 1;

            if (b[i].clicked == FALSE)
            {
                b[i].clicked = TRUE;

                if (b[i].clickedCount == 0 || tm > b[i].clickedTime)
                {
                    b[i].clickedTime  = tm + CONTROL_DoubleClickSpeed;
                    b[i].clickedCount = 1;
                }
                else if (tm < b[i].clickedTime)
                {
                    b[i].clickedState = TRUE;
                    b[i].clickedTime  = 0;
                    b[i].clickedCount = 2;
                }
            }
            else if (b[i].clickedCount == 2)
            {
                b[i].clickedState = TRUE;
            }

            continue;
        }

        if (b[i].clickedCount == 2)
            b[i].clickedCount = 0;

        b[i].clicked = FALSE;
    }

    return retval;
}

static void controlUpdateButtonStates(void)
{
    int32_t const t = ExtGetTime();

    if (CONTROL_MouseEnabled)
        controlHandleClickStates(MOUSE_GetButtons(), t, CONTROL_NumMouseButtons, mouseButtons);

    if (CONTROL_JoystickEnabled)
    {
        if (controlHandleClickStates(JOYSTICK_GetButtons(), t, CONTROL_NumJoyButtons, joyButtons))
            CONTROL_LastSeenInput = LastSeenInput::Joystick;
    }
}

static int controllerDigitizeAxis(int axis)
{
    ControllerAxisState_t &set     = joyAxes[axis].axis;
    ControllerAxisState_t &lastset = joyAxes[axis].last;

    set.digitalCleared = lastset.digitalCleared;

    if (set.analog > 0)
    {
        if (set.analog > DIGITALAXISANALOGTHRESHOLD || (set.analog > MINDIGITALAXISANALOGTHRESHOLD && lastset.digital == 1))
            set.digital = 1;
        else
            set.digitalCleared = 0;

        return 1;
    }
    else if (set.analog < 0)
    {
        if (set.analog < -DIGITALAXISANALOGTHRESHOLD || (set.analog < -MINDIGITALAXISANALOGTHRESHOLD && lastset.digital == -1))
            set.digital = -1;
        else
            set.digitalCleared = 0;

        return 1;
    }
    else
        set.digitalCleared = 0;

    return 0;
}

static inline int32_t joydist(int x, int y) { return ksqrt(x * x + y * y); }

static void controlUpdateAxisState(int index, ControlInfo *const info)
{
    int const  in  = joystick.pAxis[index];
    auto &     a   = joyAxes[index];
    auto const out = &a.axis;

    a.last = a.axis;
    *out = {};

    int axisScaled10k = klabs(in * 10000 / 32767);

    if (axisScaled10k >= a.saturation)
        out->analog = 32767 * ksgn(in);
    else
    {
        // this assumes there are two sticks comprised of axes 0 and 1, and 2 and 3... because when isGameController is true, there are
        if (!a.solodeadzone && (index <= CONTROLLER_AXIS_LEFTY || (joystick.isGameController && (index <= CONTROLLER_AXIS_RIGHTY))))
            axisScaled10k = min(10000, joydist(joystick.pAxis[index & ~1], joystick.pAxis[index | 1]) * 10000 / 32767);

        if (axisScaled10k < a.deadzone)
            out->analog = 0;
        else
            out->analog = in * (axisScaled10k - a.deadzone) / a.saturation;
    }

    if (controllerDigitizeAxis(index))
        CONTROL_LastSeenInput = LastSeenInput::Joystick;

    int const invert = !!a.invert;
    int const clamped = Blrintf(clamp<float>(a.axis.analog * CONTROL_JoySensitivityUnit * a.sensitivity, -MAXSCALEDCONTROLVALUE, MAXSCALEDCONTROLVALUE));
    a.axis.analog  = (clamped ^ -invert) + invert;

    switch (a.mapping.analogmap)
    {
        case analog_turning:          info->dyaw   += a.axis.analog; break;
        case analog_strafing:         info->dx     += a.axis.analog; break;
        case analog_lookingupanddown: info->dpitch += a.axis.analog; break;
        case analog_elevation:        info->dy     += a.axis.analog; break;
        case analog_rolling:          info->droll  += a.axis.analog; break;
        case analog_moving:           info->dz     += a.axis.analog; break;
        default: break;
    }
}

static void controlPollDevices(ControlInfo *const info)
{
    memset(info, 0, sizeof(ControlInfo));
    handleevents();

#ifdef __ANDROID__
    CONTROL_Android_PollDevices(info);
#endif

    if (CONTROL_MouseEnabled)
        controlUpdateMouseState(info);

    if (CONTROL_JoystickEnabled)
    {
        for (int i=joystick.numAxes-1; i>=0; i--)
            controlUpdateAxisState(i, info);
    }

    controlUpdateButtonStates();
}

static void controlUpdateFlagsFromAxes(int32_t *const p1)
{
    if (CONTROL_NumJoyAxes)
    {
        int axis = CONTROL_NumJoyAxes - 1;
        int retval = 0;

        do
        {
            auto &a = joyAxes[axis];
            if (!a.axis.digital)
                continue;

            int const j = (a.axis.digital < 0) ? a.mapping.minmap : a.mapping.maxmap;

            if (j != AXISUNDEFINED)
            {
                p1[j] = 1;
                retval = 1;
            }
        }
        while (axis--);

        if (retval)
            CONTROL_LastSeenInput = LastSeenInput::Joystick;
    }
}

static void controlUpdateFlagsFromButtons(int32_t *const p1)
{
    if (CONTROL_NumMouseButtons)
    {
        int i = CONTROL_NumMouseButtons-1, j;

        do
        {
            if (!CONTROL_KeyBinds[MAXBOUNDKEYS + i].cmdstr)
            {
                j = mouseButtons[i].mapping.doubleclicked;
                if (j != KEYUNDEFINED)
                    p1[j] |= mouseButtons[i].clickedState;

                j = mouseButtons[i].mapping.singleclicked;
                if (j != KEYUNDEFINED)
                    p1[j] |= mouseButtons[i].state;
            }

            if (!CONTROL_BindsEnabled)
                continue;

            if (CONTROL_KeyBinds[MAXBOUNDKEYS + i].cmdstr && mouseButtons[i].state)
            {
                if (CONTROL_KeyBinds[MAXBOUNDKEYS + i].repeat || (CONTROL_KeyBinds[MAXBOUNDKEYS + i].laststate == 0))
                    OSD_Dispatch(CONTROL_KeyBinds[MAXBOUNDKEYS + i].cmdstr, true);
            }
            CONTROL_KeyBinds[MAXBOUNDKEYS + i].laststate = mouseButtons[i].state;
        }
        while (i--);
    }

    if (CONTROL_NumJoyButtons)
    {
        int i=CONTROL_NumJoyButtons-1, j;
        int retval = 0;

        do
        {
            j = joyButtons[i].mapping.doubleclicked;
            if (j != KEYUNDEFINED)
            {
                auto const state = joyButtons[i].clickedState;
                p1[j] |= state;
                retval |= state;
            }

            j = joyButtons[i].mapping.singleclicked;
            if (j != KEYUNDEFINED)
            {
                auto const state = joyButtons[i].state;
                p1[j] |= state;
                retval |= state;
            }
        }
        while (i--);

        if (retval)
            CONTROL_LastSeenInput = LastSeenInput::Joystick;
    }
}

void CONTROL_ClearButton(int whichbutton)
{
    if (CONTROL_CheckRange(whichbutton)) return;

#ifdef __ANDROID__
    CONTROL_Android_ClearButton(whichbutton);
#endif

    BUTTONCLEAR(whichbutton);
    CONTROL_Flags[whichbutton].cleared = TRUE;
}

void CONTROL_ClearAllButtons(void)
{
    CONTROL_ButtonHeldState = 0;
    CONTROL_ButtonState = 0;

    for (auto & c : CONTROL_Flags)
        c.cleared = TRUE;
}

void CONTROL_ProcessBinds(void)
{
    if (!CONTROL_BindsEnabled)
        return;

    int i = MAXBOUNDKEYS-1;

    do
    {
        if (CONTROL_KeyBinds[i].cmdstr)
        {
            auto const keyPressed = KB_KeyPressed(i);

            if (keyPressed && (CONTROL_KeyBinds[i].repeat || (CONTROL_KeyBinds[i].laststate == 0)))
            {
                CONTROL_LastSeenInput = LastSeenInput::Keyboard;
                OSD_Dispatch(CONTROL_KeyBinds[i].cmdstr, true);
            }

            CONTROL_KeyBinds[i].laststate = keyPressed;
        }
    }
    while (i--);
}

static void controlUpdateGameFunctions(void)
{
    controlUpdateFlagsFromButtons(CONTROL_ButtonFlags);
    controlUpdateFlagsFromAxes(CONTROL_ButtonFlags);

    CONTROL_ButtonHeldState = CONTROL_ButtonState;
    CONTROL_ButtonState = 0;

    int i = CONTROL_NUM_FLAGS-1;

    do
    {
        controlSetFlag(i, controlKeyboardFunctionPressed(i) | CONTROL_ButtonFlags[i]);

        if (CONTROL_Flags[i].cleared == FALSE) BUTTONSET(i, CONTROL_Flags[i].active);
        else if (CONTROL_Flags[i].active == FALSE) CONTROL_Flags[i].cleared = 0;
    }
    while (i--);

    memset(CONTROL_ButtonFlags, 0, sizeof(CONTROL_ButtonFlags));
}

void CONTROL_GetInput(ControlInfo *info)
{
#ifdef __ANDROID__
    CONTROL_Android_PollDevices(info);
#endif
    controlPollDevices(info);
    controlUpdateGameFunctions();
    inputchecked = 1;
}

static void CONTROL_ResetJoystickValues()
{
    CONTROL_NumJoyAxes      = min(MAXJOYAXES, joystick.numAxes);
    CONTROL_NumJoyButtons   = min(MAXJOYBUTTONS, joystick.numButtons + 4 * (joystick.numHats > 0));
    CONTROL_JoystickEnabled = CONTROL_JoyPresent = !!((inputdevices & DEV_JOYSTICK) >> 2);
}

void CONTROL_ScanForControllers()
{
    joyScanDevices();
    CONTROL_ResetJoystickValues();
}

bool CONTROL_Startup(controltype which, int32_t(*TimeFunction)(void), int32_t ticspersecond)
{
    UNREFERENCED_PARAMETER(which);

    if (CONTROL_Started) return false;

    static osdcvardata_t cvars_mact [] =
    {
        { "in_mousexsens", "horizontal mouse sensitivity multiplier", (void *)&CONTROL_MouseAxesSensitivity[0], CVAR_FLOAT, 0, 100 },
        { "in_mouseysens", "vertical mouse sensitivity multiplier",   (void *)&CONTROL_MouseAxesSensitivity[1], CVAR_FLOAT, 0, 100 },
        { "in_mouseunit",  "base mouse input unit",                   (void *)&CONTROL_MouseSensitivityUnit,    CVAR_FLOAT, 0, 1 },
        { "in_joyunit",    "base controller input unit",              (void *)&CONTROL_JoySensitivityUnit,      CVAR_FLOAT, 0, 1 },
        { "sensitivity",   "master mouse sensitivity multiplier",     (void *)&CONTROL_MouseSensitivity,        CVAR_FLOAT, 0, 100 },
    };

    for (auto& cv : cvars_mact)
        OSD_RegisterCvar(&cv, osdcmd_cvar_set);

    ExtGetTime = TimeFunction ? TimeFunction : controlGetTime;

    // what the fuck???
    ticrate = ticspersecond;
    CONTROL_DoubleClickSpeed = (ticspersecond * 57) / 100;

    if (CONTROL_DoubleClickSpeed <= 0)
        CONTROL_DoubleClickSpeed = 1;

    if (initinput(CONTROL_ScanForControllers))
        return true;

    KB_Startup();

    CONTROL_NumMouseButtons = MAXMOUSEBUTTONS;
    CONTROL_MousePresent    = MOUSE_Startup();

    if (!(CONTROL_MouseEnabled = CONTROL_MousePresent))
        DVLOG_F(LOG_INPUT, "No mice found.");

    CONTROL_ResetJoystickValues();

#ifdef GEKKO
    if (!CONTROL_JoyPresent)
        DVLOG_F(LOG_INPUT, "No controllers found.");
#endif

    CONTROL_ButtonState     = 0;
    CONTROL_ButtonHeldState = 0;

    for (auto & CONTROL_Flag : CONTROL_Flags)
        CONTROL_Flag.used = FALSE;

    CONTROL_Started = TRUE;

    return false;
}

void CONTROL_Shutdown(void)
{
    if (!CONTROL_Started)
        return;

    CONTROL_ClearAllBinds();

    MOUSE_Shutdown();
    uninitinput();

    CONTROL_Started = FALSE;
}

// temporary hack until input is unified

#define SCALEAXIS(x) (joyAxes[CONTROLLER_AXIS_ ## x].axis.analog * 10000 / 32767)
#define SATU(x) (joyAxes[CONTROLLER_AXIS_ ## x].saturation)

UserInput *CONTROL_GetUserInput(UserInput *info)
{
    if (info == nullptr)
        info = &userInput.local;

    direction newdir = dir_None;

    if ((joyAxes[CONTROLLER_AXIS_LEFTY].axis.digital == -1 && SCALEAXIS(LEFTY) <= -SATU(LEFTY))
        || (JOYSTICK_GetControllerButtons() & (1 << CONTROLLER_BUTTON_DPAD_UP)))
        newdir = dir_Up;
    else if ((joyAxes[CONTROLLER_AXIS_LEFTY].axis.digital == 1 && SCALEAXIS(LEFTY) >= SATU(LEFTY))
                || (JOYSTICK_GetControllerButtons() & (1 << CONTROLLER_BUTTON_DPAD_DOWN)))
        newdir = dir_Down;
    else if ((joyAxes[CONTROLLER_AXIS_LEFTX].axis.digital == -1 && SCALEAXIS(LEFTX) <= -SATU(LEFTX))
                || (JOYSTICK_GetControllerButtons() & (1 << CONTROLLER_BUTTON_DPAD_LEFT)))
        newdir = dir_Left;
    else if ((joyAxes[CONTROLLER_AXIS_LEFTX].axis.digital == 1 && SCALEAXIS(LEFTX) >= SATU(LEFTX))
                || (JOYSTICK_GetControllerButtons() & (1 << CONTROLLER_BUTTON_DPAD_RIGHT)))
        newdir = dir_Right;

    // allow the user to press the dpad as fast as they like without being rate limited
    if (newdir == dir_None)
    {
        userInput.clock = -1;
        userInput.repeat = dir_None;
    }

    info->dir = (ExtGetTime() >= userInput.clock) ? newdir : dir_None;

    if (KB_KeyDown[sc_kpad_8] || KB_KeyDown[sc_UpArrow])
        info->dir = dir_Up;
    else if (KB_KeyDown[sc_kpad_2] || KB_KeyDown[sc_DownArrow])
        info->dir = dir_Down;
    else if (KB_KeyDown[sc_kpad_4] || KB_KeyDown[sc_LeftArrow])
        info->dir = dir_Left;
    else if (KB_KeyDown[sc_kpad_6] || KB_KeyDown[sc_RightArrow])
        info->dir = dir_Right;

    info->b_advance = KB_KeyPressed(sc_Enter) || KB_KeyPressed(sc_kpad_Enter) || (MOUSE_GetButtons() & M_LEFTBUTTON)
                    || (JOYSTICK_GetControllerButtons() & (1 << CONTROLLER_BUTTON_A));
    info->b_return   = KB_KeyPressed(sc_Escape) || (MOUSE_GetButtons() & M_RIGHTBUTTON) || (JOYSTICK_GetControllerButtons() & (1 << CONTROLLER_BUTTON_B));
    info->b_escape = KB_KeyPressed(sc_Escape) || (JOYSTICK_GetControllerButtons() & (1 << CONTROLLER_BUTTON_START));

#if defined(GEKKO)
    if (JOYSTICK_GetButtons()&(WII_A))
        info->b_advance = true;

    if (JOYSTICK_GetButtons()&(WII_B|WII_HOME))
        info->b_return = true;

    if (JOYSTICK_GetButtons() & WII_HOME)
        info->b_escape = true;
#endif

    if (userInput.buttonCleared[1])
    {
        if (!info->b_advance)
            userInput.buttonCleared[1] = false;
        else
            info->b_advance = false;
    }

    if (userInput.buttonCleared[2])
    {
        if (!info->b_return)
            userInput.buttonCleared[2] = false;
        else
            info->b_return = false;
    }

    if (userInput.buttonCleared[3])
    {
        if (!info->b_escape)
            userInput.buttonCleared[3] = false;
        else
            info->b_escape = false;
    }

    return info;
}

#undef SCALEAXIS
#undef SATU

void CONTROL_ClearUserInput(UserInput * info)
{
    if (info == nullptr)
        info = &userInput.local;

    // for keyboard keys we want the OS repeat rate, so just clear them
    KB_ClearKeyDown(sc_UpArrow);
    KB_ClearKeyDown(sc_kpad_8);
    KB_ClearKeyDown(sc_DownArrow);
    KB_ClearKeyDown(sc_kpad_2);
    KB_ClearKeyDown(sc_LeftArrow);
    KB_ClearKeyDown(sc_kpad_4);
    KB_ClearKeyDown(sc_RightArrow);
    KB_ClearKeyDown(sc_kpad_6);

    // the OS doesn't handle repeat for joystick inputs so we have to do it ourselves
    if (info->dir != dir_None)
    {
        auto const clk = ExtGetTime();

        if (userInput.repeat == info->dir)
            userInput.clock = clk + ((ticrate * USERINPUTFASTDELAY) / 1000);
        else
        {
            userInput.repeat = info->dir;
            userInput.clock = clk + ((ticrate * USERINPUTDELAY) / 1000);
        }

        userInput.buttonCleared[0] = true;
    }

    if (info->b_advance)
    {
        KB_ClearKeyDown(sc_kpad_Enter);
        KB_ClearKeyDown(sc_Enter);
        MOUSE_ClearButton(M_LEFTBUTTON);
        userInput.buttonCleared[1] = true;
    }

    if (info->b_return)
    {
        KB_ClearKeyDown(sc_Escape);
        MOUSE_ClearButton(M_RIGHTBUTTON);
        userInput.buttonCleared[2] = true;
    }

    if (info->b_escape)
    {
        KB_ClearKeyDown(sc_Escape);
        userInput.buttonCleared[3] = true;
    }
    inputchecked = 1;
}
