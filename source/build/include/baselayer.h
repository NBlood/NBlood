// Base services interface declaration
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)

#pragma once

#ifndef baselayer_h_
#define baselayer_h_

#include "compat.h"
#include "log.h"
#include "osd.h"
#include "timer.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int app_main(int argc, char const * const * argv);
extern const char* AppProperName;
extern const char* AppTechnicalName;

void engineSetupAllocator(void);

#ifdef DEBUGGINGAIDS
# define DEBUG_MASK_DRAWING
extern int32_t g_maskDrawMode;
#endif

#define PRINTF_INITIAL_BUFFER_SIZE 32
#define MSGBOX_PRINTF_MAX          1536

extern char quitevent, appactive;
extern char modechange;
#ifdef USE_OPENGL
extern char nogl;
extern char usevulkan;
#else
#define nogl (1)
#endif

extern int32_t vsync;
extern int32_t r_finishbeforeswap;
extern int32_t r_glfinish;
extern int32_t r_borderless;
extern int32_t r_displayindex;

extern void app_crashhandler(void);

// NOTE: these are implemented in game-land so they may be overridden in game specific ways
extern int32_t startwin_open(void);
extern int32_t startwin_close(void);
extern int32_t startwin_puts(const char *);
extern int32_t startwin_settitle(const char *);
extern int32_t startwin_idle(void *);
extern int32_t startwin_run(void);
extern bool startwin_isopen(void);

// video
extern int32_t r_rotatespriteinterp;
extern int32_t r_usenewaspect, newaspect_enable;
extern int32_t r_fpgrouscan;
extern int32_t setaspect_new_use_dimen;
extern uint32_t r_screenxy;
extern int32_t xres, yres, bpp, fullscreen, bytesperline;
extern double refreshfreq;
extern intptr_t frameplace;
extern char offscreenrendering;
extern int32_t nofog;

extern int32_t r_maxfps;
extern int32_t g_numdisplays;
extern int32_t g_displayindex;

extern bool g_ImGuiCaptureInput;
extern bool g_ImGuiFrameActive;
extern uint8_t g_ImGuiCapturedDevices;
extern void engineBeginImGuiFrame(void);
extern void engineEndImGuiInput(void);
extern void engineBeginImGuiInput(void);

void calc_ylookup(int32_t bpl, int32_t lastyidx);

int32_t videoCheckMode(int32_t *x, int32_t *y, int32_t c, int32_t fs, int32_t forced);
int32_t videoSetMode(int32_t x, int32_t y, int32_t c, int32_t fs);
void    videoGetModes(int display = -1);
void    videoResetMode(void);
void    videoEndDrawing(void);
void    videoShowFrame(int32_t);
int32_t videoUpdatePalette(int32_t start, int32_t num);
int32_t videoSetGamma(void);
int32_t videoSetVsync(int32_t newSync);
char const* videoGetDisplayName(int display);
//#define DEBUG_FRAME_LOCKING
#if !defined DEBUG_FRAME_LOCKING
void videoBeginDrawing(void);
#else
void begindrawing_real(void);
# define BEGINDRAWING_SIZE 256
extern uint32_t begindrawing_line[BEGINDRAWING_SIZE];
extern const char *begindrawing_file[BEGINDRAWING_SIZE];
extern int32_t lockcount;
# define videoBeginDrawing() do {                     \
    if (lockcount < BEGINDRAWING_SIZE) {         \
        begindrawing_line[lockcount] = __LINE__; \
        begindrawing_file[lockcount] = __FILE__; \
    }                                            \
    begindrawing_real();                         \
} while(0)
#endif

extern float g_videoGamma, g_videoContrast, g_videoSaturation;

#define DEFAULT_GAMMA      1.0f
#define DEFAULT_CONTRAST   1.0f
#define DEFAULT_SATURATION 1.0f

#define MAX_GAMMA      1.25f
#define MAX_CONTRAST   1.5f
#define MAX_SATURATION 2.0f

#define MIN_GAMMA      0.75f
#define MIN_CONTRAST   0.5f
#define MIN_SATURATION 0.0f

#define GAMMA_CALC ((int32_t)(min(max((float)((g_videoGamma - 1.0f) * 10.0f), 0.f), 15.f)))

struct glinfo_t {
    const char *vendor;
    const char *renderer;
    const char *version;
    const char *extensions;

    float maxanisotropy;

    int maxTextureSize;

    int filled;

    union {
        uint32_t features;
        struct
        {
            unsigned int bgra               : 1;
            unsigned int bufferstorage      : 1;
            unsigned int debugoutput        : 1;
            unsigned int depthclamp         : 1;
            unsigned int depthtex           : 1;
            unsigned int fbos               : 1;
            unsigned int glsl               : 1;
            unsigned int multitex           : 1;
            unsigned int occlusionqueries   : 1;
            unsigned int rect               : 1;
            unsigned int reset_notification : 1;
            unsigned int samplerobjects     : 1;
            unsigned int shadow             : 1;
            unsigned int sync               : 1;
            unsigned int texcompr           : 1;
            unsigned int texnpot            : 1;
            unsigned int vsync              : 1;
        };
    };
};

extern struct glinfo_t glinfo;

#ifdef USE_OPENGL
extern int32_t (*baselayer_osdcmd_vidmode_func)(osdcmdptr_t parm);
extern int osdcmd_glinfo(osdcmdptr_t parm);
extern void fill_glinfo(void);
#endif

vec2_t CONSTEXPR const g_defaultVideoModes []
= { { 2560, 1440 }, { 2560, 1200 }, { 2560, 1080 }, { 1920, 1440 }, { 1920, 1200 }, { 1920, 1080 }, { 1680, 1050 },
    { 1600, 1200 }, { 1600, 900 },  { 1366, 768 },  { 1280, 1024 }, { 1280, 960 },  { 1280, 720 },  { 1152, 864 },
    { 1024, 768 },  { 1024, 600 },  { 800, 600 },   { 640, 480 },   { 640, 400 },   { 512, 384 },   { 480, 360 },
    { 400, 300 },   { 320, 240 },   { 320, 200 },   { 0, 0 } };

extern char inputdevices;

#define DEV_KEYBOARD 0x1
#define DEV_MOUSE    0x2
#define DEV_JOYSTICK 0x4

// keys
#define NUMKEYS 256
#define KEYFIFOSIZ 64

char CONSTEXPR const g_keyAsciiTable[128] = {
    0  ,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,  0,   'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']', 0,   0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 39, '`', 0,   92,  'z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
    '.', '/', 0,   '*', 0,   32,  0,   0,   0,   0,   0,   0,   0,   0,   0,  0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6',
    '+', '1', '2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0  ,   0,   0,   0,   0,   0, 0,   0,   0,   0,   0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,
};

char CONSTEXPR const g_keyAsciiTableShift[128] = {
    0  ,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,  0,   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '{', '}', 0,   0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<',
    '>', '?', 0,   '*', 0,   32,  0,   0,   0,   0,   0,   0,   0,   0,   0,  0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6',
    '+', '1', '2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0  ,   0,   0,   0,   0,   0, 0,   0,   0,   0,   0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,
};

extern char    keystatus[NUMKEYS];
extern char    g_keyFIFO[KEYFIFOSIZ];
extern char    g_keyAsciiFIFO[KEYFIFOSIZ];
extern uint8_t g_keyAsciiPos;
extern uint8_t g_keyAsciiEnd;
extern uint8_t g_keyFIFOend;
extern char    g_keyRemapTable[NUMKEYS];
extern char    g_keyNameTable[NUMKEYS][24];

extern int32_t keyGetState(int32_t key);
extern void keySetState(int32_t key, int32_t state);

// mouse
extern vec2_t  g_mousePos;
extern vec2_t  g_mouseAbs;
extern int32_t g_mouseBits;
extern uint8_t g_mouseClickState;
extern bool    g_mouseGrabbed;
extern bool    g_mouseEnabled;
extern bool    g_mouseInsideWindow;
extern bool    g_mouseLockedToWindow;

enum
{
    MOUSE_IDLE = 0,
    MOUSE_PRESSED,
    MOUSE_HELD,
    MOUSE_RELEASED,
};
extern int32_t mouseAdvanceClickState(void);

// joystick

typedef struct
{
    int32_t *pAxis;
    int32_t *pHat;
    void (*pCallback)(int32_t, int32_t);
    int32_t  bits;
    int32_t  numAxes;
    int32_t  numBalls;
    int32_t  numButtons;
    int32_t  numHats;
    uint32_t validButtons;
    uint16_t rumbleLow;
    uint16_t rumbleHigh;
    uint16_t rumbleTime;
    union
    {
        uint8_t flags;
        struct
        {
            unsigned int isGameController : 1;
            unsigned int hasRumble        : 1;
        };
    };
} controllerinput_t;

enum
{
    JOY_CONTROLLER = 0x1,
    JOY_RUMBLE     = 0x2,
};

extern controllerinput_t joystick;

extern int32_t qsetmode;

#define in3dmode() (qsetmode==200)

int32_t initsystem(void);
void uninitsystem(void);
void system_getcvars(void);

void initputs(const char *);
#define buildputs initputs
int initprintf(const char *, ...) ATTRIBUTE((format(printf,1,2)));
#define buildprintf initprintf
int debugprintf(const char *,...) ATTRIBUTE((format(printf,1,2)));

int32_t handleevents(void);
int32_t handleevents_peekkeys(void);

extern void (*keypresscallback)(int32_t,int32_t);
extern void (*g_mouseCallback)(int32_t,int32_t);
extern void (*g_controllerHotplugCallback)(void);
extern void (*g_fileDropCallback)(const char*);
extern char g_controllerSupportFlags;
enum
{
    CONTROLLER_DISABLED  = 1,
    CONTROLLER_NO_DINPUT = 2,
    CONTROLLER_NO_XINPUT = 4
};
int32_t initinput(void(*hotplugCallback)(void) = NULL);
void uninitinput(void);
void keySetCallback(void (*callback)(int32_t,int32_t));
void mouseSetCallback(void (*callback)(int32_t,int32_t));
void joySetCallback(void (*callback)(int32_t,int32_t));
const char *keyGetName(int32_t num);
const char *joyGetName(int32_t what, int32_t num); // what: 0=axis, 1=button, 2=hat
void joyScanDevices(void);

char keyGetScan(void);
char keyGetChar(void);
#define keyBufferWaiting() (g_keyAsciiPos != g_keyAsciiEnd)

static FORCE_INLINE int keyBufferFull(void)
{
    return ((g_keyAsciiEnd+1)&(KEYFIFOSIZ-1)) == g_keyAsciiPos;
}

static FORCE_INLINE void keyBufferInsert(char code)
{
    g_keyAsciiFIFO[g_keyAsciiEnd] = code;
    g_keyAsciiEnd = ((g_keyAsciiEnd+1)&(KEYFIFOSIZ-1));
}

void keyFlushScans(void);
void keyFlushChars(void);

void mouseInit(void);
void mouseUninit(void);
int32_t mouseReadAbs(vec2_t *pResult, vec2_t const *pInput);
void mouseGrabInput(bool grab);
void mouseLockToWindow(char a);
void mouseMoveToCenter(void);
int32_t mouseReadButtons(void);
void mouseReadPos(int32_t *x, int32_t *y);

bool joyHasButton(int button);
void joyReadButtons(int32_t *pResult);
extern int32_t inputchecked;

int32_t wm_msgbox(const char *name, const char *fmt, ...) ATTRIBUTE((format(printf,2,3)));
int32_t wm_ynbox(const char *name, const char *fmt, ...) ATTRIBUTE((format(printf,2,3)));
void wm_setapptitle(const char *name);

// baselayer.c
int32_t baselayer_init();

void makeasmwriteable(void);
void maybe_redirect_outputs(void);

extern uint64_t g_frameDelay;
static inline uint64_t calcFrameDelay(int maxFPS)
{
    switch (maxFPS)
    {
        case -2: return 0;
        case -1: maxFPS = refreshfreq; break;
        case 0: maxFPS = 1000; break;
    }

    return tabledivide64(timerGetNanoTickRate(), maxFPS);
}
extern int engineFPSLimit(bool const throttle = false);
#ifdef __cplusplus
}
#endif

static inline int32_t calc_smoothratio(ClockTicks const totalclk, ClockTicks const ototalclk, int gameTicRate)
{
    int const   tfreq = (int)floorf(refreshfreq * 120 / timerGetClockRate());
    int const   clk   = (totalclk - ototalclk).toScale16();
    float const tics  = ((1.f / 65536.f) * (1.f / 120.f)) * tfreq * clk;
    int const   ratio = tabledivide32_noinline((int)(65536 * tics * gameTicRate), tfreq);

    if ((unsigned)ratio > 66048)
        DVLOG_F(LOG_DEBUG+1, "calc_smoothratio: ratio: %d", ratio);

    return clamp(ratio, 0, 65536);
}

static inline void debugThreadName(char const *name)
{
    loguru::set_thread_name(name);

#if defined _WIN32 && !defined NDEBUG
    if (IsDebuggerPresent())
    {
#pragma pack(push, 8)
        typedef struct tagTHREADNAME_INFO
        {
            DWORD  dwType;     /* must be 0x1000 */
            LPCSTR szName;     /* pointer to name (in user addr space) */
            DWORD  dwThreadID; /* thread ID (-1=caller thread) */
            DWORD  dwFlags;    /* reserved for future use, must be zero */
        } THREADNAME_INFO;
#pragma pack(pop)
        THREADNAME_INFO wtf = { 0x1000, name, (DWORD)-1, 0 };
        RaiseException(0x406D1388, 0, sizeof(wtf) / sizeof(ULONG_PTR), (const ULONG_PTR *)&wtf);
    }
#endif
}

#include "print.h"

#endif // baselayer_h_
