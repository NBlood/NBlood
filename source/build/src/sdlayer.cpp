// SDL interface layer for the Build Engine
// Use SDL 1.2 or 2.0 from http://www.libsdl.org

#include <signal.h>

#include "a.h"
#include "build.h"
#include "cache1d.h"
#include "compat.h"
#include "build_cpuid.h"
#include "engine_priv.h"
#include "osd.h"
#include "palette.h"
#include "renderlayer.h"
#include "sdl_inc.h"
#include "softsurface.h"

#if SDL_MAJOR_VERSION >= 2
# include "imgui.h"
# include "imgui_impl_sdl2.h"
#ifdef USE_OPENGL
# include "imgui_impl_opengl3.h"
#endif
#endif

#ifdef USE_OPENGL
# include "glad/glad.h"
# include "glbuild.h"
# include "glsurface.h"
# include "vk.h"
# include "vksurface.h"
#endif

#if defined HAVE_GTK2
# include "gtkbits.h"
#endif

#ifdef __ANDROID__
# include <android/log.h>
#elif defined __APPLE__
# include "osxbits.h"
# include <mach/mach.h>
# include <mach/mach_time.h>
#elif defined GEKKO
# include "wiibits.h"
# include <ogc/lwp.h>
# include <ogc/lwp_watchdog.h>
#elif defined _WIN32
# include "winbits.h"
#endif

#include "vfs.h"
#include "communityapi.h"

#define MICROPROFILE_IMPL
#include "microprofile.h"

#if SDL_MAJOR_VERSION >= 2
static SDL_version linked;
#else
#define SDL_JoystickNameForIndex(x) SDL_JoystickName(x)
#endif

#if !defined STARTUP_SETUP_WINDOW
int32_t startwin_open(void) { return 0; }
int32_t startwin_close(void) { return 0; }
int32_t startwin_puts(const char *s) { UNREFERENCED_PARAMETER(s); return 0; }
int32_t startwin_idle(void *s) { UNREFERENCED_PARAMETER(s); return 0; }
int32_t startwin_settitle(const char *s) { UNREFERENCED_PARAMETER(s); return 0; }
int32_t startwin_run(void) { return 0; }
bool startwin_isopen(void) { return false; }
#endif

/// These can be useful for debugging sometimes...
//#define SDL_WM_GrabInput(x) SDL_WM_GrabInput(SDL_GRAB_OFF)
//#define SDL_ShowCursor(x) SDL_ShowCursor(SDL_ENABLE)

// undefine to restrict windowed resolutions to conventional sizes
#define ANY_WINDOWED_SIZE

// fix for mousewheel
int32_t inputchecked = 0;

char quitevent=0, appactive=1, novideo=0;

// video
static SDL_Surface *sdl_surface/*=NULL*/;

static vec2_t sdl_resize;
static int sdl_minimized;

#if SDL_MAJOR_VERSION >= 2
static SDL_Window *sdl_window;
static SDL_GLContext sdl_context;
static int vsync_unsupported;
static VkSurfaceKHR sdl_vksurface;
#endif

static int32_t vsync_renderlayer;
int32_t maxrefreshfreq=0;
int32_t xres=-1, yres=-1, bpp=0, fullscreen=0, bytesperline;
double refreshfreq = 59.0;
intptr_t frameplace=0;
int32_t lockcount=0;
char modechange=1;
char offscreenrendering=0;
char videomodereset = 0;
int32_t nofog=0;
#ifndef EDUKE32_GLES
static uint16_t sysgamma[3][256];
#endif
#ifdef USE_OPENGL
// OpenGL stuff
char nogl=0;
char usevulkan=0;
#endif
// last gamma, contrast
static float lastvidgcb[2];

//#define KEY_PRINT_DEBUG

#include "sdlkeytrans.cpp"

static SDL_Surface *appicon = NULL;
#if !defined __APPLE__ && !defined EDUKE32_TOUCH_DEVICES
static SDL_Surface *loadappicon(void);
#endif

static mutex_t m_initprintf;
#if SDL_MAJOR_VERSION >= 2
static ImGuiIO *g_ImGui_IO;
bool g_ImGuiCaptureInput = true;
#endif
uint8_t g_ImGuiCapturedDevices;
#ifdef _WIN32
# if SDL_MAJOR_VERSION >= 2
//
// win_gethwnd() -- gets the window handle
//
HWND win_gethwnd(void)
{
    struct SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if (SDL_GetWindowWMInfo(sdl_window, &wmInfo) != SDL_TRUE)
        return 0;

    if (wmInfo.subsystem == SDL_SYSWM_WINDOWS)
        return wmInfo.info.win.window;

    LOG_F(ERROR, "Unknown WM subsystem?!");

    return 0;
}
# endif
//
// win_gethinstance() -- gets the application instance
//
HINSTANCE win_gethinstance(void)
{
    return (HINSTANCE)GetModuleHandle(NULL);
}
#endif


int32_t wm_msgbox(const char *name, const char *fmt, ...)
{
    char *buf = (char *)Balloca(MSGBOX_PRINTF_MAX);
    va_list va;

    UNREFERENCED_PARAMETER(name);

    va_start(va,fmt);
    Bvsnprintf(buf,MSGBOX_PRINTF_MAX,fmt,va);
    va_end(va);
    buf[MSGBOX_PRINTF_MAX-1] = 0;
#if defined EDUKE32_OSX
    auto result = osx_msgbox(name, buf);
    return result;
#elif defined _WIN32
    MessageBox(win_gethwnd(),buf,name,MB_OK|MB_TASKMODAL);
    return 0;
#elif defined EDUKE32_TOUCH_DEVICES
    LOG_F(INFO, "wm_msgbox called. Message: %s: %s",name,buf);
    return 0;
#elif defined GEKKO
    puts(buf);
    return 0;
#else
# if defined HAVE_GTK2
    if (gtkbuild_msgbox(name, buf) >= 0)
        return 0;
# endif
# if SDL_MAJOR_VERSION >= 2
#  if !defined _WIN32
    // Replace all tab chars with spaces because the hand-rolled SDL message
    // box diplays the former as N/L instead of whitespace.
    for (size_t i=0; i<MSGBOX_PRINTF_MAX; i++)
        if (buf[i] == '\t')
            buf[i] = ' ';
#  endif
    auto result = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, name, buf, NULL);
    return result;
# else
    puts(buf);
    puts("   (press Return or Enter to continue)");
    getchar();
    return 0;
# endif
#endif
}

int32_t wm_ynbox(const char *name, const char *fmt, ...)
{
    char *buf = (char *)Balloca(MSGBOX_PRINTF_MAX);
    va_list va;

    UNREFERENCED_PARAMETER(name);

    va_start(va,fmt);
    Bvsnprintf(buf,MSGBOX_PRINTF_MAX,fmt,va);
    va_end(va);
    buf[MSGBOX_PRINTF_MAX-1] = 0;
#if defined EDUKE32_OSX
    auto result = osx_ynbox(name, buf);
    return result;
#elif defined _WIN32
    auto result = MessageBox(win_gethwnd(),buf,name,MB_YESNO|MB_ICONQUESTION|MB_TASKMODAL);
    return result == IDYES;
#elif defined EDUKE32_TOUCH_DEVICES
    LOG_F(WARNING, "wm_ynbox called, this is bad! Message: %s: %s",name,buf);
    LOG_F(INFO, "Returning false..");
    return 0;
#elif defined GEKKO
    puts(buf);
    puts("Assuming yes...");
    return 1;
#else
# if defined HAVE_GTK2
    int ret = gtkbuild_ynbox(name, buf);
    if (ret >= 0)
        return ret;
# endif
# if SDL_MAJOR_VERSION >= 2
    int r = -1;

    const SDL_MessageBoxButtonData buttons[] = {
        {
            SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,
            0,
            "No"
        },{
            SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
            1,
            "Yes"
        },
    };

    SDL_MessageBoxData data = {
        SDL_MESSAGEBOX_INFORMATION,
        NULL, /* no parent window */
        name,
        buf,
        2,
        buttons,
        NULL /* Default color scheme */
    };

    SDL_ShowMessageBox(&data, &r);
    return r;
# else
    char c;

    puts(buf);
    puts("   (type 'Y' or 'N', and press Return or Enter to continue)");
    do c = getchar(); while (c != 'Y' && c != 'y' && c != 'N' && c != 'n');
    return c == 'Y' || c == 'y';
# endif
#endif
}

void wm_setapptitle(const char *name)
{
#ifndef EDUKE32_TOUCH_DEVICES
    if (name != apptitle)
        Bstrncpyz(apptitle, name, sizeof(apptitle));

#if !defined(__APPLE__)
    if (!appicon)
        appicon = loadappicon();
#endif

#if SDL_MAJOR_VERSION >= 2
    if (sdl_window)
    {
        SDL_SetWindowTitle(sdl_window, apptitle);

        if (appicon)
        {
#if defined _WIN32
        if (!EDUKE32_SDL_LINKED_PREREQ(linked, 2, 0, 5))
#endif
            SDL_SetWindowIcon(sdl_window, appicon);
        }
    }
#else
    SDL_WM_SetCaption(apptitle, NULL);

    if (appicon && sdl_surface)
        SDL_WM_SetIcon(appicon, 0);
#endif

    startwin_settitle(apptitle);
#else
    UNREFERENCED_PARAMETER(name);
#endif
}

//
//
// ---------------------------------------
//
// System
//
// ---------------------------------------
//
//

/* XXX: libexecinfo could be used on systems without gnu libc. */
#if !defined _WIN32 && defined __GNUC__ && !defined __OpenBSD__ && !(defined __APPLE__ && defined __BIG_ENDIAN__) && !defined GEKKO && !defined EDUKE32_TOUCH_DEVICES && !defined __OPENDINGUX__
# define PRINTSTACKONSEGV 1
# include <execinfo.h>
#endif

static inline char grabmouse_low(char a);

#ifndef __ANDROID__
static void attach_debugger_here(void)
{
#ifdef DEBUGGINGAIDS
    debug_break();
#endif
}

static void sighandler(int signum)
{
    UNREFERENCED_PARAMETER(signum);
    //    if (signum==SIGSEGV)
    {
        grabmouse_low(0);

#if PRINTSTACKONSEGV
        {
            void *addr[32];
            int32_t errfd = fileno(stderr);
            int32_t n=backtrace(addr, ARRAY_SIZE(addr));
            backtrace_symbols_fd(addr, n, errfd);
        }
        // This is useful for attaching the debugger post-mortem. For those pesky
        // cases where the program runs through happily when inspected from the start.
        //        usleep(15000000);
#endif
        attach_debugger_here();
        app_crashhandler();
        Bexit(EXIT_FAILURE);
    }
}
#endif

#ifdef __ANDROID__
int mobile_halted = 0;
#ifdef __cplusplus
extern "C"
{
#endif
void G_Shutdown(void);
#ifdef __cplusplus
}
#endif

int sdlayer_mobilefilter(void *userdata, SDL_Event *event)
{
    switch (event->type)
    {
        case SDL_APP_TERMINATING:
            // yes, this calls into the game, ugh
            if (mobile_halted == 1)
                G_Shutdown();

            mobile_halted = 1;
            return 0;
        case SDL_APP_LOWMEMORY:
            gltexinvalidatetype(INVALIDATE_ALL);
            return 0;
        case SDL_APP_WILLENTERBACKGROUND:
            mobile_halted = 1;
            return 0;
        case SDL_APP_DIDENTERBACKGROUND:
            gltexinvalidatetype(INVALIDATE_ALL);
            // tear down video?
            return 0;
        case SDL_APP_WILLENTERFOREGROUND:
            // restore video?
            return 0;
        case SDL_APP_DIDENTERFOREGROUND:
            mobile_halted = 0;
            return 0;
        default:
            return 1;//!halt;
    }

    UNREFERENCED_PARAMETER(userdata);
}

# include <setjmp.h>
static jmp_buf eduke32_exit_jmp_buf;
static int eduke32_return_value;

void eduke32_exit_return(int retval)
{
    eduke32_return_value = retval;
    longjmp(eduke32_exit_jmp_buf, 1);
    EDUKE32_UNREACHABLE_SECTION(return);
}
#endif

void sdlayer_sethints()
{
#if defined _WIN32 && !defined _MSC_VER
    // Thread naming interferes with debugging using MinGW-w64's GDB.
#if defined SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING
    SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
#endif
#if defined SDL_HINT_XINPUT_ENABLED
    if (Bgetenv("EDUKE32_NO_XINPUT"))
        SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
#endif
#endif

#if defined SDL_HINT_NO_SIGNAL_HANDLERS
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
#endif
#if defined SDL_HINT_VIDEO_HIGHDPI_DISABLED
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "1");
#endif
#if defined SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "0");
#endif
#if defined SDL_HINT_AUDIO_RESAMPLING_MODE
    SDL_SetHint(SDL_HINT_AUDIO_RESAMPLING_MODE, "3");
#endif
#if defined SDL_HINT_MOUSE_RELATIVE_SCALING
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0");
#endif
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
#elif defined __ANDROID__
# ifdef __cplusplus
extern "C" int eduke32_android_main(int argc, char const *argv[]);
# endif
int eduke32_android_main(int argc, char const *argv[])
#elif defined GEKKO
int SDL_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
#ifdef _WIN32
    char * argvbuf;
    int    argc = windowsGetCommandLine(&argvbuf);
    char * wp   = argvbuf;
    char **argv = (char **)Bmalloc(sizeof(char *)*(argc+1));

    for (int i = 0; i < argc; i++, wp++)
    {
        argv[i] = wp;
        while (*wp) wp++;
    }
    argv[argc] = NULL;
#endif

    engineSetupAllocator();

#ifndef __ANDROID__
    signal(SIGSEGV, sighandler);
    signal(SIGILL, sighandler);  /* clang -fcatch-undefined-behavior uses an ill. insn */
    signal(SIGABRT, sighandler);
    signal(SIGFPE, sighandler);
#endif

    engineSetupLogging(argc, argv);

#if SDL_VERSION_ATLEAST(2, 0, 8)
    if (EDUKE32_SDL_LINKED_PREREQ(linked, 2, 0, 8))
        SDL_SetMemoryFunctions(_xmalloc, _xcalloc, _xrealloc, _xfree);
#endif

    MicroProfileOnThreadCreate("Main");
    MicroProfileSetForceEnable(true);
    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceMetaCounters(true);

#ifdef __ANDROID__
    if (setjmp(eduke32_exit_jmp_buf))
    {
        return eduke32_return_value;
    }
#endif

    sdlayer_sethints();

#ifdef USE_OPENGL
    char *argp;

    if ((argp = Bgetenv("EDUKE32_NO_OPENGL_FOG")) != NULL)
        nofog = Batol(argp);

#if defined __linux__ || defined EDUKE32_BSD
    if (!Bgetenv("EDUKE32_NO_GL_THREADED_OPTIMIZATIONS"))
    {
        if (!Bgetenv("EDUKE32_NO_NVIDIA_THREADED_OPTIMIZATIONS"))
            setenv("__GL_THREADED_OPTIMIZATIONS", "1", 0);

        if (!Bgetenv("EDUKE32_NO_MESA_THREADED_OPTIMIZATIONS"))
            setenv("mesa_glthread", "true", 0);
    }
#endif
#endif

    buildkeytranslationtable();

#ifdef __ANDROID__
    SDL_SetEventFilter(sdlayer_mobilefilter, NULL);
#endif

#ifdef _WIN32
    UNREFERENCED_PARAMETER(hInst);
    UNREFERENCED_PARAMETER(hPrevInst);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    if (windowsPreInit())
        return -1;

#elif defined(GEKKO)
    wii_open();
#elif defined(HAVE_GTK2)
    // Pre-initialize SDL video system in order to make sure XInitThreads() is called
    // before GTK starts talking to X11.
    if (SDL_WasInit(SDL_INIT_VIDEO) != SDL_INIT_VIDEO)
        SDL_InitSubSystem(SDL_INIT_VIDEO);

    gtkbuild_init(&argc, &argv);
#endif

#ifdef EDUKE32_OSX
    osx_preopen();
#endif
    startwin_open();
#ifdef EDUKE32_OSX
    osx_postopen();
#endif
    maybe_redirect_outputs();

#ifdef USE_PHYSFS
    int pfsi = PHYSFS_init(argv[0]);
    Bassert(pfsi != 0);
    PHYSFS_setWriteDir(PHYSFS_getUserDir());
#endif
    int const r = app_main(argc, argv);

    startwin_close();

#if defined(HAVE_GTK2)
    gtkbuild_exit(r);
#endif

    return r;
}


#if SDL_MAJOR_VERSION >= 2
#ifdef USE_OPENGL
static int sdlayer_getswapinterval(int const syncMode)
{
    static int intervals[] = { -1, 0, 1, 0};
    Bassert((unsigned)(syncMode + 1) < ARRAY_SIZE(intervals));
    return intervals[syncMode + 1];
}

static int sdlayer_checkvsync(int checkSync)
{
    int const actualSync = SDL_GL_GetSwapInterval();
    if (actualSync != sdlayer_getswapinterval(checkSync))
    {
        LOG_F(WARNING, "Video driver enforcing SwapInterval %d, unable to configure VSync!", actualSync);
        checkSync = actualSync;
        vsync_unsupported = true;
    }
    return checkSync;
}
#endif

int32_t videoSetVsync(int32_t newSync)
{
    if (newSync != 0 && vsync_unsupported)
    {
        LOG_F(WARNING, "VSync configuration locked by video driver.");
        return vsync_renderlayer;
    }

    if (vsync_renderlayer == newSync)
        return newSync;

#ifdef USE_OPENGL
    if (sdl_context)
    {
        int result = SDL_GL_SetSwapInterval(sdlayer_getswapinterval(newSync));

        if (result == -1)
        {
            if (newSync == -1)
            {
                LOG_F(WARNING, "Video driver rejected SwapInterval %d, unable to configure adaptive VSync!", sdlayer_getswapinterval(newSync));
                newSync = 1;
                result  = SDL_GL_SetSwapInterval(sdlayer_getswapinterval(newSync));
            }

            if (result == -1)
            {
                LOG_F(WARNING, "Video driver rejected SwapInterval %d, unable to configure VSync!", sdlayer_getswapinterval(newSync));
                newSync = 0;
            }
        }

        vsync_renderlayer = sdlayer_checkvsync(newSync);
    }
    else
#endif
    {
        vsync_renderlayer = newSync;

        videoResetMode();

        if (videoSetGameMode(fullscreen, xres, yres, bpp, upscalefactor))
            LOG_F(ERROR, "Failed to set video mode!");
    }

    return newSync;
}
#endif

int32_t sdlayer_checkversion(void);
#if SDL_MAJOR_VERSION >= 2
int32_t sdlayer_checkversion(void)
{
    SDL_version compiled;
    auto str = (char*)Balloca(256);
    str[0] = 0;

    SDL_GetVersion(&linked);

    // string is in the format "https://github.com/libsdl-org/SDL.git@bfd2f8993f173535efe436f8e60827cc44351bea"
    char const *rev;

    if ((rev = SDL_GetRevision()) && Bstrlen(rev) > 0)
    {
        Bsnprintf(str, 256, "Initializing SDL %d.%d.%d (%s)", linked.major, linked.minor, linked.patch, rev);
    }
    else
    {
        SDL_VERSION(&compiled);

        if (!Bmemcmp(&compiled, &linked, sizeof(SDL_version)))
            Bsnprintf(str, 256, "Initializing SDL %d.%d.%d", compiled.major, compiled.minor, compiled.patch);
        else
            Bsnprintf(str, 256, "Initializing SDL %d.%d.%d (built against version %d.%d.%d)",
                linked.major, linked.minor, linked.patch, compiled.major, compiled.minor, compiled.patch);
    }

    LOG_F(INFO, "%s", str);

    if (SDL_VERSIONNUM(linked.major, linked.minor, linked.patch) < SDL2_REQUIREDVERSION)
    {
        /*reject running under SDL versions older than what is stated in sdl_inc.h */
        LOG_F(ERROR, "You need SDL %d.%d.%d or newer to run %s.",SDL2_MIN_X,SDL2_MIN_Y,SDL2_MIN_Z,apptitle);
        return -1;
    }

    return 0;
}

//
// initsystem() -- init SDL systems
//
int32_t initsystem(void)
{
    mutex_init(&m_initprintf);

#ifdef _WIN32
    windowsPlatformInit();
#endif
    sysReadCPUID();

    if (sdlayer_checkversion())
        return -1;

    int32_t err = 0;

    if (SDL_WasInit(SDL_INIT_VIDEO) != SDL_INIT_VIDEO)
        err = SDL_InitSubSystem(SDL_INIT_VIDEO);

    if (err)
    {
        LOG_F(ERROR, "SDL initialization failed: %s.", SDL_GetError());
        LOG_F(WARNING, "Non-interactive mode enabled; this is probably not what you want.");
        VLOG_F(LOG_GFX, "Video output disabled.");

        novideo = 1;
#ifdef USE_OPENGL
        nogl = 1;
#endif
    }

#if SDL_MAJOR_VERSION >= 2
    SDL_StopTextInput();
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#endif

    timerInit(CLOCKTICKSPERSECOND);

    frameplace = 0;
    lockcount = 0;

    if (!novideo)
    {
#ifdef USE_OPENGL
        if (SDL_GL_LoadLibrary(0))
        {
            LOG_F(ERROR, "Failed loading OpenGL driver: %s; all OpenGL modes are unavailable.", SDL_GetError());
            nogl = 1;
        }
#endif

#ifndef _WIN32
        const char *drvname = SDL_GetCurrentVideoDriver();

        if (drvname)
            LOG_F(INFO, "Using '%s' video driver.", drvname);
#endif
        wm_setapptitle(apptitle);
        g_numdisplays = SDL_GetNumVideoDisplays();
    }

    return 0;
}
#endif


//
// uninitsystem() -- uninit SDL systems
//
void uninitsystem(void)
{
    uninitinput();
    timerUninit();

#ifdef _WIN32
    windowsPlatformCleanup();
#endif

    if (appicon)
    {
        SDL_FreeSurface(appicon);
        appicon = NULL;
    }

#ifdef USE_OPENGL
# if SDL_MAJOR_VERSION >= 2
    SDL_GL_UnloadLibrary();
# endif
#endif
    SDL_Quit();
}


//
// system_getcvars() -- propagate any cvars that are read post-initialization
//
void system_getcvars(void)
{
# ifdef _WIN32
    windowsDwmSetupComposition(false);
# endif

    vsync = videoSetVsync(vsync);
}

//
// debugprintf() -- prints a formatted debug string to stderr
//
int debugprintf(const char *f, ...)
{
#if defined DEBUGGINGAIDS && !(defined __APPLE__ && defined __BIG_ENDIAN__)
    va_list va;

    va_start(va,f);
    int len = Bvfprintf(stderr, f, va);
    va_end(va);
    return len;
#else
    UNREFERENCED_PARAMETER(f);
    return 0;
#endif
}


//
//
// ---------------------------------------
//
// All things Input
//
// ---------------------------------------
//
//

// static int32_t joyblast=0;
static SDL_Joystick *joydev = NULL;
#if SDL_MAJOR_VERSION >= 2
static SDL_GameController *controller = NULL;
static bool gameControllerDBLoaded;

static void LoadSDLControllerDB()
{
    if (gameControllerDBLoaded)
        return;

    buildvfs_kfd fh = kopen4load("gamecontrollerdb.txt", 0);
    if (fh == buildvfs_kfd_invalid)
        return;

    int const flen = kfilelength(fh);
    if (flen <= 0)
    {
        kclose(fh);
        return;
    }

    char * dbuf = (char *)Xaligned_alloc(16, flen + 1);
    if (!dbuf)
    {
        kclose(fh);
        return;
    }

    if (kread_and_test(fh, dbuf, flen))
    {
        Xaligned_free(dbuf);
        kclose(fh);
        return;
    }

    dbuf[flen] = '\0';
    kclose(fh);

    auto rwops = SDL_RWFromConstMem(dbuf, flen);
    if (!rwops)
    {
error:
        LOG_F(ERROR, "Failed loading game controller database: %s.", SDL_GetError());
        Xaligned_free(dbuf);
        return;
    }

    int i = SDL_GameControllerAddMappingsFromRW(rwops, 1);

    if (i == -1)
        goto error;
    else
        VLOG_F(LOG_INPUT, "Loaded game controller database.");

    Xaligned_free(dbuf);

    gameControllerDBLoaded = true;
}
#endif

static int numjoysticks;

void joyScanDevices()
{
    inputdevices &= ~DEV_JOYSTICK;

#if SDL_MAJOR_VERSION >= 2
    if (controller)
    {
        SDL_GameControllerClose(controller);
        controller = nullptr;
    }
#endif
    if (joydev)
    {
        SDL_JoystickClose(joydev);
        joydev = nullptr;
    }

    numjoysticks = SDL_NumJoysticks();

    if (numjoysticks < 1)
        VLOG_F(LOG_INPUT, "No game controllers found.");
    else
    {
        VLOG_F(LOG_INPUT, "Game controllers:");

        char name[128];

        for (int i = 0; i < numjoysticks; i++)
        {
#if SDL_MAJOR_VERSION >= 2
            if (SDL_IsGameController(i))
                Bstrncpyz(name, SDL_GameControllerNameForIndex(i), sizeof(name));
            else
#endif
                Bstrncpyz(name, SDL_JoystickNameForIndex(i), sizeof(name));

            VLOG_F(LOG_INPUT, "  %d. %s", i + 1, name);
        }
#if SDL_MAJOR_VERSION >= 2
        for (int i = 0; i < numjoysticks; i++)
        {
            if ((controller = SDL_GameControllerOpen(i)))
            {
#if SDL_VERSION_ATLEAST(2, 0, 14)
                if (EDUKE32_SDL_LINKED_PREREQ(linked, 2, 0, 14))
                    Bsnprintf(name, sizeof(name), "%s [%s]", SDL_GameControllerName(controller), SDL_GameControllerGetSerial(controller));
                else
#endif
                    Bsnprintf(name, sizeof(name), "%s", SDL_GameControllerName(controller));

                VLOG_F(LOG_INPUT, "Using controller: %s.", name);

                joystick.flags      = 0;
                joystick.numBalls   = 0;
                joystick.numHats    = 0;
                joystick.numAxes    = SDL_CONTROLLER_AXIS_MAX;
                joystick.numButtons = SDL_CONTROLLER_BUTTON_MAX;

                joystick.validButtons = UINT32_MAX;
#if SDL_VERSION_ATLEAST(2, 0, 14)
                if (EDUKE32_SDL_LINKED_PREREQ(linked, 2, 0, 14))
                {
                    joystick.numAxes = 0;
                    for (int j = 0; j < SDL_CONTROLLER_AXIS_MAX; ++j)
                        if (SDL_GameControllerHasAxis(controller, (SDL_GameControllerAxis)j))
                            joystick.numAxes = j + 1;

                    joystick.validButtons = 0;
                    joystick.numButtons = 0;
                    for (int j = 0; j < SDL_CONTROLLER_BUTTON_MAX; ++j)
                        if (SDL_GameControllerHasButton(controller, (SDL_GameControllerButton)j))
                        {
                            joystick.numButtons = j + 1;
                            joystick.validButtons |= (1 << j);
                        }
                }
#endif
                joystick.isGameController = 1;

                if (gameControllerDBLoaded == false)
                    LoadSDLControllerDB();

                Xfree(joystick.pAxis);
                joystick.pAxis = (int32_t *)Xcalloc(joystick.numAxes, sizeof(int32_t));
                DO_FREE_AND_NULL(joystick.pHat);

                inputdevices |= DEV_JOYSTICK;

#if SDL_VERSION_ATLEAST(2, 0, 18)
                if (EDUKE32_SDL_LINKED_PREREQ(linked, 2, 0, 18))
                {
                    if (SDL_GameControllerHasRumble(controller))
                        joystick.hasRumble = 1;
                    else DVLOG_F(LOG_INPUT, "Couldn't init controller rumble: %s.", SDL_GetError());
                }
                else
#endif
#if SDL_VERSION_ATLEAST(2, 0, 9)
                if (EDUKE32_SDL_LINKED_PREREQ(linked, 2, 0, 9))
                {
                    if (!SDL_GameControllerRumble(controller, 1, 1, 1))
                        joystick.hasRumble = 1;
                    else DVLOG_F(LOG_INPUT, "Couldn't init controller rumble: %s.", SDL_GetError());
                }
#endif
                return;
            }
        }
#endif

        for (int i = 0; i < numjoysticks; i++)
        {
            if ((joydev = SDL_JoystickOpen(i)))
            {
                VLOG_F(LOG_INPUT, "Using joystick: %s", SDL_JoystickNameForIndex(i));

                // KEEPINSYNC duke3d/src/gamedefs.h, mact/include/_control.h
                joystick.flags      = 0;
                joystick.numAxes    = min(9, SDL_JoystickNumAxes(joydev));
                joystick.numBalls   = SDL_JoystickNumBalls(joydev);
                joystick.numButtons = min(32, SDL_JoystickNumButtons(joydev));
                joystick.numHats    = min((36 - joystick.numButtons) / 4, SDL_JoystickNumHats(joydev));

                joystick.validButtons = UINT32_MAX;
                joystick.isGameController = 0;

                VLOG_F(LOG_INPUT, "Joystick %d has %d axes, %d buttons, %d hats, and %d balls.", i+1, joystick.numAxes, joystick.numButtons, joystick.numHats, joystick.numBalls);

                Xfree(joystick.pAxis);
                joystick.pAxis = (int32_t *)Xcalloc(joystick.numAxes, sizeof(int32_t));

                Xfree(joystick.pHat);
                if (joystick.numHats)
                    joystick.pHat = (int32_t *)Xcalloc(joystick.numHats, sizeof(int32_t));
                else
                    joystick.pHat = nullptr;

                for (int j = 0; j < joystick.numHats; j++)
                    joystick.pHat[j] = -1; // center

                SDL_JoystickEventState(SDL_ENABLE);
                inputdevices |= DEV_JOYSTICK;

#if SDL_VERSION_ATLEAST(2, 0, 18)
                if (EDUKE32_SDL_LINKED_PREREQ(linked, 2, 0, 18))
                {
                    if (SDL_JoystickHasRumble(joydev))
                        joystick.hasRumble = 1;
                    else DVLOG_F(LOG_INPUT, "Couldn't init joystick rumble: %s.", SDL_GetError());
                }
                else
#endif
#if SDL_VERSION_ATLEAST(2, 0, 9)
                if (EDUKE32_SDL_LINKED_PREREQ(linked, 2, 0, 9))
                {
                    if (!SDL_JoystickRumble(joydev, 1, 1, 1))
                        joystick.hasRumble = 1;
                    else DVLOG_F(LOG_INPUT, "Couldn't init joystick rumble: %s.", SDL_GetError());
                }
#endif
                return;
            }
        }

        VLOG_F(LOG_INPUT, "No controllers are usable.");
    }
}

//
// initinput() -- init input system
//
int32_t initinput(void(*hotplugCallback)(void) /*= nullptr*/)
{
    int32_t i;

#if SDL_MAJOR_VERSION >= 2
    if (hotplugCallback)
        g_controllerHotplugCallback = hotplugCallback;
#else
    UNREFERENCED_PARAMETER(hotplugCallback);
#endif

#if defined EDUKE32_OSX
    // force OS X to operate in >1 button mouse mode so that LMB isn't adulterated
    if (!getenv("SDL_HAS3BUTTONMOUSE"))
    {
        static char sdl_has3buttonmouse[] = "SDL_HAS3BUTTONMOUSE=1";
        putenv(sdl_has3buttonmouse);
    }
#endif

    inputdevices = DEV_KEYBOARD | DEV_MOUSE;
    g_mouseGrabbed = false;

    memset(g_keyNameTable, 0, sizeof(g_keyNameTable));

#if SDL_MAJOR_VERSION < 2
#define SDL_SCANCODE_TO_KEYCODE(x) (SDLKey)(x)
#define SDL_NUM_SCANCODES SDLK_LAST
    if (SDL_EnableKeyRepeat(250, 30))
        LOG_F(ERROR, "Unable to configure keyboard repeat.");
    SDL_EnableUNICODE(1);  // let's hope this doesn't hit us too hard
#endif

    for (i = SDL_NUM_SCANCODES - 1; i >= 0; i--)
    {
        if (!keytranslation[i])
            continue;

        Bstrncpyz(g_keyNameTable[keytranslation[i]], SDL_GetKeyName(SDL_SCANCODE_TO_KEYCODE(i)), sizeof(g_keyNameTable[0]));
    }

    if ((g_controllerSupportFlags & CONTROLLER_DISABLED) == 0)
    {
#if SDL_MAJOR_VERSION >= 2
# if defined SDL_HINT_DIRECTINPUT_ENABLED
        if (g_controllerSupportFlags & CONTROLLER_NO_DINPUT)
            SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED, "0");
# endif
# if defined SDL_HINT_XINPUT_ENABLED
        if (g_controllerSupportFlags & CONTROLLER_NO_XINPUT)
            SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
# endif
        if (!SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER))
#else
        if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK))
#endif
            joyScanDevices();

#if SDL_VERSION_ATLEAST(2, 0, 9)
        if (EDUKE32_SDL_LINKED_PREREQ(linked, 2, 0, 9))
        {
            if (joystick.flags & JOY_RUMBLE)
            {
                switch (joystick.flags & JOY_RUMBLE)
                {
                case JOY_RUMBLE:
                    VLOG_F(LOG_INPUT, "Controller supports rumble.");
                    break;
                }
            }
        }
#endif
    }

    return 0;
}

//
// uninitinput() -- uninit input system
//
void uninitinput(void)
{
    mouseUninit();

#if SDL_MAJOR_VERSION >= 2
    if (controller)
    {
        SDL_GameControllerClose(controller);
        controller = nullptr;
    }
#endif

    if (joydev)
    {
        SDL_JoystickClose(joydev);
        joydev = nullptr;
    }
}

#ifndef GEKKO
const char *joyGetName(int32_t what, int32_t num)
{
    static char tmp[64];

    switch (what)
    {
        case 0:  // axis
            if ((unsigned)num > (unsigned)joystick.numAxes)
                return NULL;

#if SDL_MAJOR_VERSION >= 2
            if (controller)
            {
                static char const * axisStrings[] =
                {
                    "Left Stick X-Axis",
                    "Left Stick Y-Axis",
                    "Right Stick X-Axis",
                    "Right Stick Y-Axis",
                    "Left Trigger",
                    "Right Trigger",
                    NULL
                };

                return num < ARRAY_SSIZE(axisStrings) - 1 ? axisStrings[num] : SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)num);
            }
#endif

            Bsprintf(tmp, "Axis %d", num);
            return (char *)tmp;

        case 1:  // button
            if ((unsigned)num > (unsigned)joystick.numButtons)
                return NULL;

#if SDL_MAJOR_VERSION >= 2
            if (controller)
            {
                static char const * buttonStrings[] =
                {
                    "A",
                    "B",
                    "X",
                    "Y",
                    "Back",
                    "Guide",
                    "Start",
                    "Left Stick",
                    "Right Stick",
                    "Left Shoulder",
                    "Right Shoulder",
                    "D-Pad Up",
                    "D-Pad Down",
                    "D-Pad Left",
                    "D-Pad Right",
                    "Misc",
                    "Paddle 1",
                    "Paddle 2",
                    "Paddle 3",
                    "Paddle 4",
                    "Touchpad",
                    NULL
                };

                return num < ARRAY_SSIZE(buttonStrings) - 1 ? buttonStrings[num] : SDL_GameControllerGetStringForButton((SDL_GameControllerButton)num);
            }
#endif

            Bsprintf(tmp, "Button %d", num);
            return (char *)tmp;

        case 2:  // hat
            if ((unsigned)num > (unsigned)joystick.numHats)
                return NULL;
            Bsprintf(tmp, "Hat %d", num);
            return (char *)tmp;

        default: return NULL;
    }
}
#endif


//
// initmouse() -- init mouse input
//
void mouseInit(void)
{
    mouseGrabInput(g_mouseEnabled = g_mouseLockedToWindow);  // FIXME - SA
}

//
// uninitmouse() -- uninit mouse input
//
void mouseUninit(void)
{
    mouseGrabInput(0);
    g_mouseEnabled = 0;
}


#if SDL_MAJOR_VERSION >= 2
//
// grabmouse_low() -- show/hide mouse cursor, lower level (doesn't check state).
//                    furthermore return 0 if successful.
//

#if defined _WIN32 && !SDL_VERSION_ATLEAST(2, 0, 13)
// bypass SDL_SetWindowGrab--see https://github.com/libsdl-org/SDL/issues/3353
static void SetWindowGrab(SDL_Window *pWindow, int const clipToWindow)
{
    UNREFERENCED_PARAMETER(pWindow);
    RECT rect { g_windowPos.x, g_windowPos.y, g_windowPos.x + xdim, g_windowPos.y + ydim };
    ClipCursor(clipToWindow ? &rect : nullptr);
}
#define SDL_SetWindowGrab SetWindowGrab
#endif
static inline char grabmouse_low(char a)
{
#if !defined EDUKE32_TOUCH_DEVICES
    /* FIXME: Maybe it's better to make sure that grabmouse_low
       is called only when a window is ready?                */
    if (sdl_window)
        SDL_SetWindowGrab(sdl_window, a ? SDL_TRUE : SDL_FALSE);
    return SDL_SetRelativeMouseMode(a ? SDL_TRUE : SDL_FALSE);
#else
    UNREFERENCED_PARAMETER(a);
    return 0;
#endif
}
#undef SDL_SetWindowGrab
#endif

//
// grabmouse() -- show/hide mouse cursor
//
void mouseGrabInput(bool grab)
{
    if (grab != g_mouseGrabbed)
        g_mousePos.x = g_mousePos.y = 0;

    if (appactive && g_mouseEnabled)
    {
#if !defined EDUKE32_TOUCH_DEVICES
        if ((grab != g_mouseGrabbed) && !grabmouse_low(grab))
#endif
            g_mouseGrabbed = grab;
    }
    else
        g_mouseGrabbed = grab;

}

void mouseLockToWindow(char a)
{
#if SDL_MAJOR_VERSION >= 2
    if (!g_ImGui_IO || !g_ImGui_IO->WantCaptureMouse)
#endif
    if (!(a & 2))
    {
        mouseGrabInput(a);
        g_mouseLockedToWindow = g_mouseGrabbed;
    }

    int newstate = (osd && ((osd->flags & OSD_CAPTURE) || (g_ImGuiCapturedDevices & DEV_MOUSE))) ? SDL_ENABLE : SDL_DISABLE;

    SDL_ShowCursor(newstate);

#if SDL_MAJOR_VERSION >= 2
    if (g_ImGui_IO)
    {
        if (newstate)
            g_ImGui_IO->ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        else
            g_ImGui_IO->ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }
#endif
}

void mouseMoveToCenter(void)
{
#if SDL_MAJOR_VERSION != 1
    if (sdl_window)
    {
        g_mouseAbs = { xres >> 1, yres >> 1 };
        SDL_WarpMouseInWindow(sdl_window, g_mouseAbs.x, g_mouseAbs.y);
    }
#endif
}


//
//
// ---------------------------------------
//
// All things Video
//
// ---------------------------------------
//
//


//
// getvalidmodes() -- figure out what video modes are available
//
static int sortmodes(const void *a_, const void *b_)
{
    auto a = (const struct validmode_t *)b_;
    auto b = (const struct validmode_t *)a_;

    int x;

    if ((x = a->fs   - b->fs)   != 0) return x;
    if ((x = a->bpp  - b->bpp)  != 0) return x;
    if ((x = a->xdim - b->xdim) != 0) return x;
    if ((x = a->ydim - b->ydim) != 0) return x;

    return 0;
}

static char modeschecked=0;

#if SDL_MAJOR_VERSION >= 2
void videoGetModes(int display)
{
    char *dummy = NULL;
    int32_t i, maxx = 0, maxy = 0;
    SDL_DisplayMode dispmode;

    if (display < 0 || display >= g_numdisplays)
        display = r_displayindex < SDL_GetNumVideoDisplays() ? r_displayindex : 0;

    if (modeschecked || novideo)
        return;
    else
    {
        auto name = Xstrdup(videoGetDisplayName(display)), shortened = Bstrtoken(name, "(", &dummy, 1);
        if (!shortened) shortened = name;
        VLOG_F(LOG_GFX, "Detecting video modes for display %d (%s)...", display, shortened);
        DO_FREE_AND_NULL(name);
    }

    validmodecnt = 0;

    // do fullscreen modes first
    for (i = 0; i < SDL_GetNumDisplayModes(display); i++)
    {
        SDL_GetDisplayMode(display, i, &dispmode);

        if (!SDL_CHECKMODE(dispmode.w, dispmode.h) ||
            (maxrefreshfreq && (dispmode.refresh_rate > maxrefreshfreq)))
            continue;

        // HACK: 8-bit == Software, 32-bit == OpenGL
        SDL_ADDMODE(dispmode.w, dispmode.h, 8, 1);
#ifdef USE_OPENGL
        if (!nogl)
            SDL_ADDMODE(dispmode.w, dispmode.h, 32, 1);
#endif
        if ((dispmode.w > maxx) || (dispmode.h > maxy))
        {
            maxx = dispmode.w;
            maxy = dispmode.h;
        }
    }

    SDL_CHECKFSMODES(maxx, maxy);

    // add windowed modes next
    // SDL sorts display modes largest to smallest, so we can just compare with mode 0
    // to make sure we aren't adding modes that are larger than the actual screen res
    SDL_GetDisplayMode(display, 0, &dispmode);

    for (i = 0; g_defaultVideoModes[i].x; i++)
    {
        auto const &mode = g_defaultVideoModes[i];

        if (mode.x > dispmode.w || mode.y > dispmode.h || !SDL_CHECKMODE(mode.x, mode.y))
            continue;

        // 8-bit == Software, 32-bit == OpenGL
        SDL_ADDMODE(mode.x, mode.y, 8, 0);

#ifdef USE_OPENGL
        if (nogl)
            continue;

        SDL_ADDMODE(mode.x, mode.y, 32, 0);
#endif
    }

    qsort((void *)validmode, validmodecnt, sizeof(struct validmode_t), &sortmodes);

    modeschecked = 1;
}
#endif

//
// checkvideomode() -- makes sure the video mode passed is legal
//
int32_t videoCheckMode(int32_t *x, int32_t *y, int32_t c, int32_t fs, int32_t forced)
{
    int32_t i, nearest=-1, dx, dy, odx=9999, ody=9999;

    videoGetModes();

    if (c>8
#ifdef USE_OPENGL
            && nogl
#endif
       ) return -1;

    // fix up the passed resolution values to be multiples of 8
    // and at least 320x200 or at most MAXXDIMxMAXYDIM
    *x = clamp(*x & ~1, 320, MAXXDIM);
    *y = clamp(*y & ~1, 200, MAXYDIM);

    for (i = 0; i < validmodecnt; i++)
    {
        if (validmode[i].bpp != c || validmode[i].fs != fs)
            continue;

        dx = klabs(validmode[i].xdim - *x);
        dy = klabs(validmode[i].ydim - *y);

        if (!(dx | dy))
        {
            // perfect match
            nearest = i;
            break;
        }

        if ((dx <= odx) && (dy <= ody))
        {
            nearest = i;
            odx = dx;
            ody = dy;
        }
    }

#ifdef ANY_WINDOWED_SIZE
    if (!forced && (fs&1) == 0 && (nearest < 0 || (validmode[nearest].xdim!=*x || validmode[nearest].ydim!=*y)))
        return 0x7fffffffl;
#endif

    if (nearest < 0)
        return -1;

    *x = validmode[nearest].xdim;
    *y = validmode[nearest].ydim;

    return nearest;
}

char const *videoGetDisplayName(int display)
{
#if SDL_MAJOR_VERSION >= 2
    return SDL_GetDisplayName(display);
#else
    UNREFERENCED_PARAMETER(display);
    return "Primary display";
#endif
}

static void destroy_window_resources()
{
/* We should NOT destroy the window surface. This is done automatically
   when SDL_DestroyWindow or SDL_SetVideoMode is called.             */

#if MICROPROFILE_ENABLED != 0
    MicroProfileGpuShutdown();
#endif

#if SDL_MAJOR_VERSION >= 2
    if (g_ImGui_IO)
    {
#ifdef USE_OPENGL
        ImGui_ImplOpenGL3_Shutdown();
#endif
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        g_ImGui_IO = NULL;
    }

    if (sdl_context)
        SDL_GL_DeleteContext(sdl_context);
    sdl_context = NULL;
    if (sdl_window)
        SDL_DestroyWindow(sdl_window);
    sdl_window = NULL;
#endif
}

bool g_ImGuiFrameActive;

void engineBeginImGuiFrame(void)
{
    Bassert(g_ImGuiFrameActive == false);
#if SDL_MAJOR_VERSION >= 2
#ifdef USE_OPENGL
    ImGui_ImplOpenGL3_NewFrame();
#endif
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    g_ImGuiFrameActive = true;
#endif
}

void engineEndImGuiInput(void)
{
    keyFlushChars();
    keyFlushScans();
#if SDL_MAJOR_VERSION >= 2
    ImGui::GetIO().ClearInputKeys();
//    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    SDL_StopTextInput();
#endif
}

void engineBeginImGuiInput(void)
{
    keyFlushChars();
    keyFlushScans();
#if SDL_MAJOR_VERSION >= 2
    SDL_StartTextInput();
//    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
#endif
}

void engineSetupImGui(void)
{
#if SDL_MAJOR_VERSION >= 2
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    g_ImGui_IO = &ImGui::GetIO();
    g_ImGui_IO->IniFilename = nullptr;
    g_ImGui_IO->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    g_ImGui_IO->ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    g_ImGui_IO->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

#ifdef USE_OPENGL
    ImGui_ImplSDL2_InitForOpenGL(sdl_window, sdl_context);
    ImGui_ImplOpenGL3_Init();
#endif

    g_ImGui_IO->Fonts->AddFontDefault();
    //ImFont* font = g_ImGui_IO->Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\consola.ttf", 12.0f);
    //IM_ASSERT(font != NULL);
#endif
}

#ifdef USE_OPENGL
void sdlayer_setvideomode_opengl(void)
{
    glsurface_destroy();

    glShadeModel(GL_SMOOTH);  // GL_FLAT
    glClearColor(0, 0, 0, 1.0);  // Black Background
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  // Use FASTEST for ortho!
//    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glViewport(0,0,xres,yres);

#ifndef EDUKE32_GLES
    glDisable(GL_DITHER);
#endif

    fill_glinfo();
}
#endif  // defined USE_OPENGL

//
// setvideomode() -- set SDL video mode
//

int32_t setvideomode_sdlcommon(int32_t *x, int32_t *y, int32_t c, int32_t fs, int32_t *regrab)
{
    if ((r_displayindex == g_displayindex) && (fs == fullscreen) && (*x == xres) && (*y == yres) && (c == bpp) && !videomodereset)
        return 0;

    if (videoCheckMode(x, y, c, fs, 0) < 0)
        return -1;

#ifdef GEKKO
    if (!sdl_surface) // only run this the first time we set a video mode
        wii_initgamevideo();
#endif

    startwin_close();

    if (g_mouseGrabbed)
    {
        *regrab = 1;
        mouseGrabInput(0);
    }

    while (lockcount) videoEndDrawing();

#ifdef USE_OPENGL
    if (sdl_surface)
    {
        if (bpp > 8)
            polymost_glreset();
    }
    if (usevulkan)
    {
        if (bpp == 8)
            vksurface_destroy();

        vk_destroy_swapchain();
    }
    else if (!nogl)
    {
        if (bpp == 8)
            glsurface_destroy();
        if ((r_displayindex == g_displayindex) && (fs == fullscreen) && (*x == xres) && (*y == yres) && (bpp != 0) && !videomodereset)
            return 0;
    }
    else
#endif
    {
       softsurface_destroy();
    }

    // clear last gamma/contrast/brightness so that it will be set anew
    lastvidgcb[0] = lastvidgcb[1] = 0.0f;

    return 1;
}

int setvideomode_sdlcommonpost(int32_t x, int32_t y, int32_t c, int32_t fs, int32_t regrab)
{
    wm_setapptitle(apptitle);

    xres = x;
    yres = y;
    bpp = c;
    fullscreen = fs;
    // bytesperline = sdl_surface->pitch;
    numpages = c > 8 ? 2 : 1;
    frameplace = 0;
    lockcount = 0;
    modechange = 1;
    videomodereset = 0;

#ifdef USE_OPENGL
    if (!nogl)
    {
        if (usevulkan)
        {
            VkResult result;
            if ((result = vk_initialize_swapchain(sdl_vksurface, x, y, vsync_renderlayer)) < 0)
            {
                LOG_F(ERROR, "Unable to set video mode: vulkan swapchain creation failed: %d.", result);
                usevulkan = 0;
            }
        }
        else
            sdlayer_setvideomode_opengl();
    }
#endif

    // save the current system gamma to determine if gamma is available
#ifndef EDUKE32_GLES
    if (!gammabrightness)
    {
        if (nogl)
        {
#if SDL_MAJOR_VERSION >= 2
            if (SDL_GetWindowGammaRamp(sdl_window, sysgamma[0], sysgamma[1], sysgamma[2]) == 0)
#else
            if (SDL_GetGammaRamp(sysgamma[0], sysgamma[1], sysgamma[2]) >= 0)
#endif
                gammabrightness = 1;

            // see if gamma really is working by trying to set the brightness
            if (gammabrightness && videoSetGamma() < 0)
                gammabrightness = 0;  // nope
        }
        else gammabrightness = 1;
    }
#endif

#if SDL_MAJOR_VERSION >= 2
    int const displayindex = SDL_GetWindowDisplayIndex(sdl_window);
    int const newdisplayindex = r_displayindex < SDL_GetNumVideoDisplays() ? r_displayindex : displayindex;

    SDL_DisplayMode desktopmode;
    if (SDL_GetDesktopDisplayMode(newdisplayindex, &desktopmode))
    {
        LOG_F(ERROR, "Unable to query desktop display mode: %s.", SDL_GetError());
        return -1;
    }

    if (displayindex != newdisplayindex || g_windowPos.x + x < 0 || g_windowPos.y + y < 0 || g_windowPos.x > desktopmode.w || g_windowPos.y > desktopmode.h)
        g_windowPosValid = false;

#ifdef _WIN32
    if (timingInfo.rateRefresh.uiNumerator && timingInfo.rateRefresh.uiDenominator)
        refreshfreq = (double)timingInfo.rateRefresh.uiNumerator / timingInfo.rateRefresh.uiDenominator;
    else
#endif
        refreshfreq = desktopmode.refresh_rate;

    int const matchedResolution = (desktopmode.w == x && desktopmode.h == y);
    int const borderless = (r_borderless == 1 || (r_borderless == 2 && matchedResolution)) ? SDL_WINDOW_BORDERLESS : 0;

    if (fs)
    {
        SDL_DisplayMode dispmode = { 0, x, y, maxrefreshfreq, nullptr }, newmode;

        if (SDL_GetClosestDisplayMode(newdisplayindex, &dispmode, &newmode) == nullptr)
        {
            LOG_F(ERROR, "Unable to find a fullscreen video mode suitable for or similar to %dx%d at %dHz: %s.", x, y, maxrefreshfreq, SDL_GetError());
            return -1;
        }
        else
        {
            if (SDL_SetWindowDisplayMode(sdl_window, &newmode))
            {
                LOG_F(ERROR, "Unable to set video mode: %s.", SDL_GetError());
                return -1;
            }
            else
                refreshfreq = newmode.refresh_rate;
        }
    }

    static double lastrefreshfreq;

    if (refreshfreq != lastrefreshfreq)
    {
        VLOG_F(LOG_GFX, "Refresh rate: %.2fHz.", refreshfreq);
        lastrefreshfreq = refreshfreq;
    }

    SDL_SetWindowSize(sdl_window, x, y);

    if (fs)
    {
        if (SDL_SetWindowFullscreen(sdl_window, SDL_WINDOW_FULLSCREEN))
        {
            LOG_F(ERROR, "Unable to set window fullscreen: %s.", SDL_GetError());
            return -1;
        }
        else SDL_SetWindowPosition(sdl_window, (int)SDL_WINDOWPOS_UNDEFINED_DISPLAY(newdisplayindex), (int)SDL_WINDOWPOS_UNDEFINED_DISPLAY(newdisplayindex));
    }
    else
    {
        if (SDL_SetWindowFullscreen(sdl_window, 0))
        {
            LOG_F(ERROR, "Unable to set windowed mode: %s.", SDL_GetError());
            return -1;
        }
        else
        {
            SDL_SetWindowBordered(sdl_window, borderless ? SDL_FALSE : SDL_TRUE);
            SDL_SetWindowPosition(sdl_window, g_windowPosValid ? g_windowPos.x : (int)SDL_WINDOWPOS_CENTERED_DISPLAY(newdisplayindex),
                                              g_windowPosValid ? g_windowPos.y : (int)SDL_WINDOWPOS_CENTERED_DISPLAY(newdisplayindex));
        }
    }

    SDL_FlushEvent(SDL_WINDOWEVENT);
#endif

    videoFadePalette(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);

    if (regrab)
        mouseGrabInput(g_mouseLockedToWindow);

#if SDL_MAJOR_VERSION >= 2
    g_displayindex = newdisplayindex;
#endif

    return 0;
}

#if SDL_MAJOR_VERSION >= 2
int32_t videoSetMode(int32_t x, int32_t y, int32_t c, int32_t fs)
{
    int32_t regrab = 0, ret;

    ret = setvideomode_sdlcommon(&x, &y, c, fs, &regrab);

    if (ret != 1)
    {
        if (ret == 0)
        {
            if (setvideomode_sdlcommonpost(x, y, c, fs, regrab))
                return -1;
        }
        return ret;
    }

    VLOG_F(LOG_GFX, "Setting video mode %dx%d (%d-bpp %s).", x, y, c, ((fs & 1) ? "fullscreen" : "windowed"));

    if (sdl_window)
        return setvideomode_sdlcommonpost(x, y, c, fs, regrab);

    int const display = r_displayindex < SDL_GetNumVideoDisplays() ? r_displayindex : 0;

    SDL_DisplayMode desktopmode;
    SDL_GetDesktopDisplayMode(display, &desktopmode);

    int const matchedResolution = (desktopmode.w == x && desktopmode.h == y);
    int const borderless = (r_borderless == 1 || (r_borderless == 2 && matchedResolution)) ? SDL_WINDOW_BORDERLESS : 0;

#ifdef USE_OPENGL
    if (c > 8 || !nogl)
    {
        int32_t i;

        if (nogl)
            return -1;

        if (usevulkan)
        {
            SDL_Vulkan_LoadLibrary(NULL);

            sdl_window = SDL_CreateWindow("", g_windowPosValid ? g_windowPos.x : (int)SDL_WINDOWPOS_CENTERED_DISPLAY(display),
                                              g_windowPosValid ? g_windowPos.y : (int)SDL_WINDOWPOS_CENTERED_DISPLAY(display), x, y,
                                            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | borderless);

            if (!sdl_window)
            {
                LOG_F(ERROR, "Unable to set video mode: SDL_CreateWindow failed: %s.", SDL_GetError());
                usevulkan = 0;
            }

            unsigned int ext_count;
            SDL_Vulkan_GetInstanceExtensions(sdl_window, &ext_count, nullptr);
            const char** ext = (const char**)Bmalloc(sizeof(const char*) * ext_count);
            SDL_Vulkan_GetInstanceExtensions(sdl_window, &ext_count, ext);
            PFN_vkGetInstanceProcAddr proc = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();

            VkResult result;
            if ((result = vk_initialize_instance(ext_count, ext, proc)) < 0)
            {
                LOG_F(ERROR, "Unable to set video mode: vulkan initialization failed: %d.", result);
                usevulkan = 0;
            }
            Bfree(ext);
            if (usevulkan && !SDL_Vulkan_CreateSurface(sdl_window, vk_instance, &sdl_vksurface))
            {
                LOG_F(ERROR, "Unable to set video mode: sdl surface creation failed: %s.", SDL_GetError());
                usevulkan = 0;
            }
        }
        if (!usevulkan)
        {
            struct glattribs
            {
                SDL_GLattr attr;
                int32_t value;
            } sdlayer_gl_attributes[] =
            {
                  { SDL_GL_CONTEXT_FLAGS,
#ifndef NDEBUG
                  SDL_GL_CONTEXT_DEBUG_FLAG |
#endif
                  SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG },
                  { SDL_GL_CONTEXT_RESET_NOTIFICATION, SDL_GL_CONTEXT_RESET_LOSE_CONTEXT },
                  { SDL_GL_DOUBLEBUFFER, 1 },

                  { SDL_GL_STENCIL_SIZE, 1 },
                  { SDL_GL_ACCELERATED_VISUAL, 1 },
                  { SDL_GL_DEPTH_SIZE, 24 },
              };

            SDL_GL_ATTRIBUTES(i, sdlayer_gl_attributes);

            /* HACK: changing SDL GL attribs only works before surface creation,
                so we have to create a new surface in a different format first
                to force the surface we WANT to be recreated instead of reused. */

            sdl_window = SDL_CreateWindow("", g_windowPosValid ? g_windowPos.x : (int)SDL_WINDOWPOS_CENTERED_DISPLAY(display),
                                              g_windowPosValid ? g_windowPos.y : (int)SDL_WINDOWPOS_CENTERED_DISPLAY(display), x, y,
                                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | borderless);

            if (sdl_window)
                sdl_context = SDL_GL_CreateContext(sdl_window);

            if (!sdl_window || !sdl_context)
            {
                LOG_F(ERROR, "Unable to set video mode: %s failed: %s.", sdl_window ? "SDL_GL_CreateContext" : "SDL_GL_CreateWindow",  SDL_GetError());
                nogl = 1;
            }

#ifndef EDUKE32_GLES
            gladLoadGLLoader(SDL_GL_GetProcAddress);
#else
            gladLoadGLES2Loader(SDL_GL_GetProcAddress);
#endif
            if (GLVersion.major < 2)
            {
                LOG_F(ERROR, "Video driver does not support OpenGL version 2 or greater; all OpenGL modes are unavailable.");
                nogl = 1;
            }

            if (nogl)
            {
                destroy_window_resources();
                // If c == 8, retry without hardware accelaration
                return videoSetMode(x, y, c, fs);
            }

            SDL_GL_SetSwapInterval(sdlayer_getswapinterval(vsync_renderlayer));
            vsync_renderlayer = sdlayer_checkvsync(vsync_renderlayer);

            engineSetupImGui();
        }
    }
    else
#endif  // defined USE_OPENGL
    {
        // init
        sdl_window = SDL_CreateWindow("", g_windowPosValid ? g_windowPos.x : (int)SDL_WINDOWPOS_CENTERED_DISPLAY(display),
                                          g_windowPosValid ? g_windowPos.y : (int)SDL_WINDOWPOS_CENTERED_DISPLAY(display), x, y,
                                      SDL_WINDOW_RESIZABLE | borderless);
        if (!sdl_window)
            SDL2_VIDEO_ERR("SDL_CreateWindow");

        sdl_surface = SDL_GetWindowSurface(sdl_window);
        if (!sdl_surface)
            SDL2_VIDEO_ERR("SDL_GetWindowSurface");
    }

    setvideomode_sdlcommonpost(x, y, c, fs, regrab);

#if MICROPROFILE_ENABLED != 0
    if (!nogl)
        MicroProfileGpuInitGL();
#endif

#ifdef _WIN32
    windowsHandleFocusChange(1);
#endif

    return 0;
}
#endif

//
// resetvideomode() -- resets the video system
//
void videoResetMode(void)
{
    videomodereset = 1;
    modeschecked = 0;
}

//
// begindrawing() -- locks the framebuffer for drawing
//

#ifdef DEBUG_FRAME_LOCKING
uint32_t begindrawing_line[BEGINDRAWING_SIZE];
const char *begindrawing_file[BEGINDRAWING_SIZE];
void begindrawing_real(void)
#else
void videoBeginDrawing(void)
#endif
{
    if (bpp > 8)
    {
        if (offscreenrendering) return;
        frameplace = 0;
        bytesperline = 0;
        modechange = 0;
        return;
    }

    // lock the frame
    if (lockcount++ > 0)
        return;

    static intptr_t backupFrameplace = 0;

    if (inpreparemirror)
    {
        //POGO: if we are offscreenrendering and we need to render a mirror
        //      or we are rendering a mirror and we start offscreenrendering,
        //      backup our offscreen target so we can restore it later
        //      (but only allow one level deep,
        //       i.e. no viewscreen showing a camera showing a mirror that reflects the same viewscreen and recursing)
        if (offscreenrendering)
        {
            if (!backupFrameplace)
                backupFrameplace = frameplace;
            else if (frameplace != (intptr_t)mirrorBuffer &&
                     frameplace != backupFrameplace)
                return;
        }

        frameplace = (intptr_t)mirrorBuffer;

        if (offscreenrendering)
            return;
    }
    else if (offscreenrendering)
    {
        if (backupFrameplace)
        {
            frameplace = backupFrameplace;
            backupFrameplace = 0;
        }
        return;
    }
    else
#ifdef USE_OPENGL
    if (usevulkan)
    {
        frameplace = (intptr_t)vksurface_getBuffer();
    }
    else if (!nogl)
    {
        frameplace = (intptr_t)glsurface_getBuffer();
    }
    else
#endif
    {
        frameplace = (intptr_t)softsurface_getBuffer();
    }

    if (modechange)
    {
        bytesperline = xdim;
        calc_ylookup(bytesperline, ydim);
        modechange=0;
    }
}


//
// enddrawing() -- unlocks the framebuffer
//
void videoEndDrawing(void)
{
    if (bpp > 8)
    {
        if (!offscreenrendering) frameplace = 0;
        return;
    }

    if (!frameplace) return;
    if (lockcount > 1) { lockcount--; return; }
    if (!offscreenrendering) frameplace = 0;
    if (lockcount == 0) return;
    lockcount = 0;
}

//
// showframe() -- update the display
//
#ifdef __ANDROID__
extern "C" void AndroidDrawControls();
#endif

void videoShowFrame(int32_t w)
{
    UNREFERENCED_PARAMETER(w);

    // if we're minimized or about to resize, just throw away the frame
    if (sdl_resize.x || sdl_minimized)
        return;

#ifdef __ANDROID__
    if (mobile_halted) return;
#endif

#ifdef USE_OPENGL
    if (usevulkan)
    {
        if (bpp == 8)
            vksurface_blitBuffer();

        vk_next_page();

        MicroProfileFlip();
        return;
    }
    if (!nogl)
    {
        if (bpp > 8)
        {
            if (palfadedelta)
                fullscreen_tint_gl(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);
            fullscreen_tint_gl_blood();

#ifdef __ANDROID__
            AndroidDrawControls();
#endif
        }
        else
        {
            glsurface_blitBuffer();
        }
#if SDL_MAJOR_VERSION >= 2
        if (g_ImGuiFrameActive)
        {
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            g_ImGuiFrameActive = false;
        }
#endif
        if ((r_glfinish == 1 && r_finishbeforeswap == 1) || vsync_renderlayer == 2)
        {
            MICROPROFILE_SCOPEI("Engine", "glFinish", MP_GREEN);
            glFinish();
        }

#ifdef _WIN32
        if (vsync_renderlayer == 2)
        {
            static uint64_t nextSwapTime = timerGetPerformanceCounter();
            uint64_t const  swapInterval = (timerGetPerformanceFrequency() / refreshfreq);
            uint64_t const  swapTime     = timerGetPerformanceCounter();

            // TODO: use timing information to determine swap time and just busy loop ourselves for more timing control
            if (swapTime < nextSwapTime)
            {
                MICROPROFILE_SCOPEI("Engine", "waitForVBlank", MP_GREEN2);
                windowsWaitForVBlank();
            }
            else if (swapTime - nextSwapTime >= swapInterval)
                nextSwapTime += swapInterval;

            nextSwapTime += swapInterval;
        }
#endif

        {
            MICROPROFILE_SCOPEI("Engine", "SDL_GL_SwapWindow", MP_GREEN3);
#if SDL_MAJOR_VERSION >= 2
            SDL_GL_SwapWindow(sdl_window);
#else
            SDL_GL_SwapBuffers();
#endif
        }

        if (r_glfinish == 1 && r_finishbeforeswap == 0 && vsync_renderlayer != 2)
        {
            MICROPROFILE_SCOPEI("Engine", "glFinish2", MP_GREEN4);
            glFinish();
        }

        MicroProfileFlip();

#if 0
        if (glinfo.reset_notification)
        {
            static const auto glGetGraphicsReset = glGetGraphicsResetStatus ? glGetGraphicsResetStatus : glGetGraphicsResetStatusKHR;
            auto status = glGetGraphicsReset();
            if (status != GL_NO_ERROR)
            {
                do
                {
                    switch (status)
                    {
                    case GL_GUILTY_CONTEXT_RESET:
                        LOG_F(ERROR, "OPENGL CONTEXT LOST: GUILTY!");
                        break;
                    case GL_INNOCENT_CONTEXT_RESET:
                        LOG_F(ERROR, "OPENGL CONTEXT LOST: INNOCENT!");
                        break;
                    case GL_UNKNOWN_CONTEXT_RESET:
                        LOG_F(ERROR, "OPENGL CONTEXT LOST!");
                        break;
                    }
                } while ((status = glGetGraphicsReset()) != GL_NO_ERROR);

                videoResetMode();

                if (videoSetGameMode(fullscreen, xres, yres, bpp, upscalefactor))
                {
                    LOG_F(ERROR, "Failed to reset video mode after lost OpenGL context; terminating.");
                    Bexit(EXIT_FAILURE);
                }
            }
        }
#endif
        // attached overlays and streaming hooks tend to change the GL state without setting it back

        if (w != -1)
        {
            if (bpp > 8)
                polymost_resetState();
            else
                glsurface_refresh();
        }

        return;
    }
#endif

    if (offscreenrendering) return;

    if (lockcount)
    {
        LOG_F(WARNING, "Frame was still locked at a depth of %d when videoShowFrame() was called!", lockcount);
        while (lockcount) videoEndDrawing();
    }

    if (SDL_MUSTLOCK(sdl_surface)) SDL_LockSurface(sdl_surface);
    softsurface_blitBuffer((uint32_t*) sdl_surface->pixels, sdl_surface->format->BitsPerPixel);
    if (SDL_MUSTLOCK(sdl_surface)) SDL_UnlockSurface(sdl_surface);
#if SDL_MAJOR_VERSION >= 2
    if (SDL_UpdateWindowSurface(sdl_window))
    {
        // If a fullscreen X11 window is minimized then this may be required.
        // FIXME: What to do if this fails...
        sdl_surface = SDL_GetWindowSurface(sdl_window);
        SDL_UpdateWindowSurface(sdl_window);
    }
#else
    SDL_Flip(sdl_surface);
#endif
    MicroProfileFlip();
}

//
// setpalette() -- set palette values
//
int32_t videoUpdatePalette(int32_t start, int32_t num)
{
    UNREFERENCED_PARAMETER(start);
    UNREFERENCED_PARAMETER(num);

    if (bpp > 8)
        return 0;  // no palette in opengl

#ifdef USE_OPENGL
    if (usevulkan)
        vksurface_setPalette(curpalettefaded);
    else if (!nogl)
        glsurface_setPalette(curpalettefaded);
    else
#endif
    {
        if (sdl_surface)
            softsurface_setPalette(curpalettefaded,
                                   sdl_surface->format->Rmask,
                                   sdl_surface->format->Gmask,
                                   sdl_surface->format->Bmask);
    }

    return 0;
}

//
// setgamma
//
int32_t videoSetGamma(void)
{
    if (novideo)
        return 0;

#ifdef USE_OPENGL
    if (!nogl)
    {
        g_glColorCorrection = { g_videoGamma, g_videoContrast, g_videoSaturation, 0.f };

        if (videoGetRenderMode() == REND_POLYMOST)
            polymost_setColorCorrection(g_glColorCorrection);
        return 0;
    }
#endif

    int32_t i;
    uint16_t gammaTable[768];
    float gamma = max(MIN_GAMMA, min(MAX_GAMMA, g_videoGamma));
    float contrast = max(MIN_CONTRAST, min(MAX_CONTRAST, g_videoContrast));

    float invgamma = 1.f / gamma;
    float norm = powf(255.f, invgamma - 1.f);

    if (lastvidgcb[0] == gamma && lastvidgcb[1] == contrast)
        return 0;

    // This formula is taken from Doomsday

    for (i = 0; i < 256; i++)
    {
        float val = max(0.f, i * contrast - (contrast - 1.f) * 127.f);
        if (gamma != 1.f)
            val = powf(val, invgamma) / norm;

        gammaTable[i] = gammaTable[i + 256] = gammaTable[i + 512] = (uint16_t)max(0.f, min(65535.f, val * 256.f));
    }

#if SDL_MAJOR_VERSION >= 2
    i = INT32_MIN;

    if (sdl_window)
        i = SDL_SetWindowGammaRamp(sdl_window, &gammaTable[0], &gammaTable[256], &gammaTable[512]);
    if (i < 0)
    {
#else
    i = SDL_SetGammaRamp(&gammaTable[0], &gammaTable[256], &gammaTable[512]);
    if (/*(i != -1) && */(i < 0))
    {
#endif
        if (i != INT32_MIN)
            DLOG_F(ERROR, "Failed setting window gamma ramp: %s.", SDL_GetError());

        gammabrightness = 0;
    }
    else
    {
        lastvidgcb[0] = gamma;
        lastvidgcb[1] = contrast;

        gammabrightness = 1;
    }

    return i;
}

#if !defined __APPLE__ && !defined EDUKE32_TOUCH_DEVICES
extern "C" struct sdlappicon sdlappicon;
static inline SDL_Surface *loadappicon(void)
{
    SDL_Surface *surf = SDL_CreateRGBSurfaceFrom((void *)sdlappicon.pixels, sdlappicon.width, sdlappicon.height, 32,
                                                 sdlappicon.width * 4, 0xffl, 0xff00l, 0xff0000l, 0xff000000l);
    return surf;
}
#endif

//
//
// ---------------------------------------
//
// Miscellany
//
// ---------------------------------------
//
//

int32_t handleevents_peekkeys(void)
{
    SDL_PumpEvents();

#if SDL_MAJOR_VERSION >= 2
    return SDL_PeepEvents(NULL, 1, SDL_PEEKEVENT, SDL_KEYDOWN, SDL_KEYDOWN);
#else
    return SDL_PeepEvents(NULL, 1, SDL_PEEKEVENT, SDL_EVENTMASK(SDL_KEYDOWN));
#endif
}

void handleevents_updatemousestate(uint8_t state)
{
    g_mouseClickState = state == SDL_RELEASED ? MOUSE_RELEASED : MOUSE_PRESSED;
}


//
// handleevents() -- process the SDL message queue
//   returns !0 if there was an important event worth checking (like quitting)
//

int32_t handleevents_sdlcommon(SDL_Event *ev)
{
    switch (ev->type)
    {
#if !defined EDUKE32_IOS
        case SDL_MOUSEMOTION:
#ifndef GEKKO
            g_mouseAbs.x = ev->motion.x;
            g_mouseAbs.y = ev->motion.y;
            fallthrough__;
#endif
        case SDL_JOYBALLMOTION:
            // SDL <VER> doesn't handle relative mouse movement correctly yet as the cursor still clips to the
            // screen edges
            // so, we call SDL_WarpMouse() to center the cursor and ignore the resulting motion event that occurs
            //  <VER> is 1.3 for PK, 1.2 for tueidj
            if (appactive && g_mouseGrabbed)
            {
# if SDL_MAJOR_VERSION < 2
                if (ev->motion.x != xdim >> 1 || ev->motion.y != ydim >> 1)
# endif
                {
                    g_mousePos.x += ev->motion.xrel;
                    g_mousePos.y += ev->motion.yrel;
# if SDL_MAJOR_VERSION < 2
                    SDL_WarpMouse(xdim>>1, ydim>>1);
# endif
                }
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            int32_t j;

            // some of these get reordered to match winlayer
            switch (ev->button.button)
            {
                default: j = -1; break;
                case SDL_BUTTON_LEFT: j = 0; handleevents_updatemousestate(ev->button.state); break;
                case SDL_BUTTON_RIGHT: j = 1; break;
                case SDL_BUTTON_MIDDLE: j = 2; break;

#if SDL_MAJOR_VERSION < 2
                case SDL_BUTTON_WHEELUP:    // 4
                case SDL_BUTTON_WHEELDOWN:  // 5
                    j = ev->button.button;
                    break;
#endif
                /* Thumb buttons. */
#if SDL_MAJOR_VERSION < 2
                // NOTE: SDL1 does have SDL_BUTTON_X1, but that's not what is
                // generated. (Only tested on Linux and Windows.)
                case 8: j = 3; break;
                case 9: j = 6; break;
#else
                // On SDL2/Windows and SDL >= 2.0.?/Linux, everything is as it should be.
                // If anyone cares about old versions of SDL2 on Linux, patches welcome.
                case SDL_BUTTON_X1: j = 3; break;
                case SDL_BUTTON_X2: j = 6; break;
#endif
            }

            if (j < 0)
                break;

            if (ev->button.state == SDL_PRESSED)
                g_mouseBits |= (1 << j);
            else
#if SDL_MAJOR_VERSION < 2
                if (j != SDL_BUTTON_WHEELUP && j != SDL_BUTTON_WHEELDOWN)
#endif
                g_mouseBits &= ~(1 << j);

            if (g_mouseCallback)
                g_mouseCallback(j+1, ev->button.state == SDL_PRESSED);
            break;
        }
#else
# if SDL_MAJOR_VERSION >= 2
        case SDL_FINGERUP:
            g_mouseClickState = MOUSE_RELEASED;
            break;
        case SDL_FINGERDOWN:
            g_mouseClickState = MOUSE_PRESSED;
        case SDL_FINGERMOTION:
            g_mouseAbs.x = Blrintf(ev->tfinger.x * xdim);
            g_mouseAbs.y = Blrintf(ev->tfinger.y * ydim);
            break;
# endif
#endif
#if SDL_MAJOR_VERSION >= 2
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            if (g_controllerHotplugCallback && SDL_NumJoysticks() != numjoysticks)
                g_controllerHotplugCallback();
            break;
#endif
        case SDL_JOYAXISMOTION:
#if SDL_MAJOR_VERSION >= 2
            if (joystick.isGameController)
                break;
            fallthrough__;
        case SDL_CONTROLLERAXISMOTION:
#endif
            if (appactive && ev->jaxis.axis < joystick.numAxes)
            {
                joystick.pAxis[ev->jaxis.axis] = ev->jaxis.value;
            }
            break;

        case SDL_JOYHATMOTION:
        {
            int32_t hatvals[16] = {
                -1,     // centre
                0,      // up 1
                9000,   // right 2
                4500,   // up+right 3
                18000,  // down 4
                -1,     // down+up!! 5
                13500,  // down+right 6
                -1,     // down+right+up!! 7
                27000,  // left 8
                27500,  // left+up 9
                -1,     // left+right!! 10
                -1,     // left+right+up!! 11
                22500,  // left+down 12
                -1,     // left+down+up!! 13
                -1,     // left+down+right!! 14
                -1,     // left+down+right+up!! 15
            };
            if (appactive && ev->jhat.hat < joystick.numHats)
                joystick.pHat[ev->jhat.hat] = hatvals[ev->jhat.value & 15];
            break;
        }

        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
#if SDL_MAJOR_VERSION >= 2
            if (joystick.isGameController)
                break;
            fallthrough__;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
#endif
            if (appactive && ev->jbutton.button < joystick.numButtons)
            {
                if (ev->jbutton.state == SDL_PRESSED)
                    joystick.bits |= 1 << ev->jbutton.button;
                else
                    joystick.bits &= ~(1 << ev->jbutton.button);

#ifdef GEKKO
                if (ev->jbutton.button == 0) // WII_A
                    handleevents_updatemousestate(ev->jbutton.state);
#endif
            }
            break;

        case SDL_QUIT:
            quitevent = 1;
            return -1;
    }

    return 0;
}

int32_t handleevents_pollsdl(void);
#if SDL_MAJOR_VERSION >= 2

// SDL 2.0 specific event handling
int32_t handleevents_pollsdl(void)
{
    int32_t code, rv=0, j;
    SDL_Event ev;

    g_ImGuiCapturedDevices = 0;

    if (g_ImGui_IO)
    {
        if (g_ImGuiCaptureInput && g_ImGui_IO->WantCaptureKeyboard)
            g_ImGuiCapturedDevices = DEV_KEYBOARD;

        if (g_ImGui_IO->WantCaptureMouse)
            g_ImGuiCapturedDevices |= DEV_MOUSE;
    }

    while (SDL_PollEvent(&ev))
    {
        if (g_ImGui_IO)
        {
            if (g_ImGui_IO->WantCaptureKeyboard && (ev.type == SDL_TEXTINPUT || ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP))
                if (ImGui_ImplSDL2_ProcessEvent(&ev) && ev.type == SDL_TEXTINPUT)
                    continue;

            if (g_ImGui_IO->WantCaptureMouse && (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP || ev.type == SDL_MOUSEWHEEL))
                if (ImGui_ImplSDL2_ProcessEvent(&ev))
                    continue;
        }

        switch (ev.type)
        {
            case SDL_DROPFILE:
                if (g_fileDropCallback && ev.drop.type == SDL_DROPFILE)
                    g_fileDropCallback(ev.drop.file);
                SDL_free(ev.drop.file);
                break;
            case SDL_TEXTINPUT:
                j = 0;
                do
                {
                    code = ev.text.text[j];

                    if (code != g_keyAsciiTable[OSD_OSDKey()] && !keyBufferFull())
                    {
                        if (OSD_HandleChar(code))
                            keyBufferInsert(code);
                    }
                } while (j < SDL_TEXTINPUTEVENT_TEXT_SIZE-1 && ev.text.text[++j]);
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                auto const &sc = ev.key.keysym.scancode;
                code = keytranslation[sc];

                // Modifiers that have to be held down to be effective
                // (excludes KMOD_NUM, for example).
                static const int MODIFIERS =
                    KMOD_LSHIFT|KMOD_RSHIFT|KMOD_LCTRL|KMOD_RCTRL|
                    KMOD_LALT|KMOD_RALT|KMOD_LGUI|KMOD_RGUI;

                // XXX: see osd.c, OSD_HandleChar(), there are more...
                if (ev.key.type == SDL_KEYDOWN && !keyBufferFull() &&
                    (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER ||
                     sc == SDL_SCANCODE_ESCAPE ||
                     sc == SDL_SCANCODE_BACKSPACE ||
                     sc == SDL_SCANCODE_TAB ||
                     (((ev.key.keysym.mod) & MODIFIERS) == KMOD_LCTRL &&
                      (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z))))
                {
                    char keyvalue;
                    switch (sc)
                    {
                        case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: keyvalue = '\r'; break;
                        case SDL_SCANCODE_ESCAPE: keyvalue = 27; break;
                        case SDL_SCANCODE_BACKSPACE: keyvalue = '\b'; break;
                        case SDL_SCANCODE_TAB: keyvalue = '\t'; break;
                        default: keyvalue = sc - SDL_SCANCODE_A + 1; break;  // Ctrl+A --> 1, etc.
                    }
                    if (OSD_HandleChar(keyvalue))
                        keyBufferInsert(keyvalue);
                }
                else if (ev.key.type == SDL_KEYDOWN &&
                         ev.key.keysym.sym != g_keyAsciiTable[OSD_OSDKey()] && !keyBufferFull() &&
                         !SDL_IsTextInputActive())
                {
                    /*
                    Necessary for Duke 3D's method of entering cheats to work without showing IMEs.
                    SDL_TEXTINPUT is preferable overall, but with bitmap fonts it has no advantage.
                    */
                    SDL_Keycode keyvalue = ev.key.keysym.sym;

                    if ('a' <= keyvalue && keyvalue <= 'z')
                    {
                        if (!!(ev.key.keysym.mod & KMOD_SHIFT) ^ !!(ev.key.keysym.mod & KMOD_CAPS))
                            keyvalue -= 'a'-'A';
                    }
                    else if (ev.key.keysym.mod & KMOD_SHIFT)
                    {
                        switch (keyvalue)
                        {
                            case '\'': keyvalue = '"'; break;

                            case ',': keyvalue = '<'; break;
                            case '-': keyvalue = '_'; break;
                            case '.': keyvalue = '>'; break;
                            case '/': keyvalue = '?'; break;
                            case '0': keyvalue = ')'; break;
                            case '1': keyvalue = '!'; break;
                            case '2': keyvalue = '@'; break;
                            case '3': keyvalue = '#'; break;
                            case '4': keyvalue = '$'; break;
                            case '5': keyvalue = '%'; break;
                            case '6': keyvalue = '^'; break;
                            case '7': keyvalue = '&'; break;
                            case '8': keyvalue = '*'; break;
                            case '9': keyvalue = '('; break;

                            case ';': keyvalue = ':'; break;

                            case '=': keyvalue = '+'; break;

                            case '[': keyvalue = '{'; break;
                            case '\\': keyvalue = '|'; break;
                            case ']': keyvalue = '}'; break;

                            case '`': keyvalue = '~'; break;
                        }
                    }
                    else if (ev.key.keysym.mod & KMOD_NUM) // && !(ev.key.keysym.mod & KMOD_SHIFT)
                    {
                        switch (keyvalue)
                        {
                            case SDLK_KP_1: keyvalue = '1'; break;
                            case SDLK_KP_2: keyvalue = '2'; break;
                            case SDLK_KP_3: keyvalue = '3'; break;
                            case SDLK_KP_4: keyvalue = '4'; break;
                            case SDLK_KP_5: keyvalue = '5'; break;
                            case SDLK_KP_6: keyvalue = '6'; break;
                            case SDLK_KP_7: keyvalue = '7'; break;
                            case SDLK_KP_8: keyvalue = '8'; break;
                            case SDLK_KP_9: keyvalue = '9'; break;
                            case SDLK_KP_0: keyvalue = '0'; break;
                            case SDLK_KP_PERIOD: keyvalue = '.'; break;
                            case SDLK_KP_COMMA: keyvalue = ','; break;
                        }
                    }

                    switch (keyvalue)
                    {
                        case SDLK_KP_DIVIDE: keyvalue = '/'; break;
                        case SDLK_KP_MULTIPLY: keyvalue = '*'; break;
                        case SDLK_KP_MINUS: keyvalue = '-'; break;
                        case SDLK_KP_PLUS: keyvalue = '+'; break;
                    }

                    if ((unsigned)keyvalue <= 0x7Fu)
                    {
                        if (OSD_HandleChar(keyvalue))
                            keyBufferInsert(keyvalue);
                    }
                }

                // initprintf("SDL2: got key %d, %d, %u\n", ev.key.keysym.scancode, code, ev.key.type);

                // hook in the osd
                if ((j = OSD_HandleScanCode(code, (ev.key.type == SDL_KEYDOWN))) <= 0)
                {
                    if (j == -1)  // osdkey
                    {
                        for (j = 0; j < NUMKEYS; ++j)
                        {
                            if (keyGetState(j))
                            {
                                if (keypresscallback)
                                    keypresscallback(j, 0);
                            }
                            keySetState(j, 0);
                        }
                    }
                    break;
                }

                if (ev.key.type == SDL_KEYDOWN)
                {
                    if (!keyGetState(code))
                    {
                        if (keypresscallback)
                            keypresscallback(code, 1);
                    }
                    keySetState(code, 1);
                }
                else
                {
# if 1
                    // The pause key generates a release event right after
                    // the pressing one. As a result, it gets unseen
                    // by the game most of the time.
                    if (code == 0x59)  // pause
                        break;
# endif
                    keySetState(code, 0);
                    if (keypresscallback)
                        keypresscallback(code, 0);
                }
                break;
            }

            case SDL_MOUSEWHEEL:
                // initprintf("wheel y %d\n",ev.wheel.y);
                if (ev.wheel.y > 0)
                {
                    g_mouseBits |= 16;
                    if (g_mouseCallback)
                        g_mouseCallback(5, 1);
                }
                if (ev.wheel.y < 0)
                {
                    g_mouseBits |= 32;
                    if (g_mouseCallback)
                        g_mouseCallback(6, 1);
                }
                break;

            case SDL_WINDOWEVENT:
                switch (ev.window.event)
                {
                    case SDL_WINDOWEVENT_MINIMIZED:
                        sdl_minimized = true;
                        break;
                    case SDL_WINDOWEVENT_RESTORED:
                    case SDL_WINDOWEVENT_MAXIMIZED:
                        sdl_minimized = false;
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        appactive = (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED);
                        if (g_mouseGrabbed && g_mouseEnabled)
                            grabmouse_low(appactive);
#ifdef _WIN32
                        windowsHandleFocusChange(appactive);
#endif
                        break;

                    case SDL_WINDOWEVENT_MOVED:
                    {
                        if (fullscreen) break;
                        g_windowPos = { ev.window.data1, ev.window.data2 };
                        g_windowPosValid = true;

                        int displayindex = SDL_GetWindowDisplayIndex(sdl_window);

                        if (r_displayindex != displayindex || !modeschecked)
                        {
                            r_displayindex = displayindex;
                            modeschecked = 0;
                            videoGetModes();
                        }
                        break;
                    }
                    case SDL_WINDOWEVENT_RESIZED:
                        if (fullscreen) break;
                        sdl_resize = { ev.window.data1 & ~1, ev.window.data2 & ~1 };
                        break;
                    case SDL_WINDOWEVENT_ENTER:
                    case SDL_WINDOWEVENT_LEAVE:
                        g_mouseInsideWindow = (ev.window.event == SDL_WINDOWEVENT_ENTER);
                        break;
                }

                break;

            default:
                rv = handleevents_sdlcommon(&ev);
                break;
        }
    }

    return rv;
}
#endif

int32_t handleevents(void)
{
#ifdef __ANDROID__
    if (mobile_halted) return 0;
#endif

    int32_t rv;

#if SDL_VERSION_ATLEAST(2, 0, 9)
    if (g_ImGuiFrameActive)
    {
        ImGui::EndFrame();
        g_ImGuiFrameActive = false;
    }

    if (EDUKE32_SDL_LINKED_PREREQ(linked, 2, 0, 9))
    {
        if (joystick.hasRumble)
        {
            auto dorumble = [](uint16_t const low, uint16_t const high, uint32_t const time)
            {
                if (joystick.isGameController)
                    SDL_GameControllerRumble(controller, low, high, time);
                else
                    SDL_JoystickRumble(joydev, low, high, time);
            };

            static uint32_t rumbleZeroTime;

            if (joystick.rumbleLow || joystick.rumbleHigh)
            {
                rumbleZeroTime = timerGetTicks() + joystick.rumbleTime;

                dorumble(joystick.rumbleLow, joystick.rumbleHigh, joystick.rumbleTime);
                joystick.rumbleTime = joystick.rumbleLow = joystick.rumbleHigh = 0;
            }
            else if (rumbleZeroTime && timerGetTicks() >= rumbleZeroTime)
            {
                rumbleZeroTime = 0;
                dorumble(0, 0, 0);
            }
        }
    }
#endif

#if SDL_MAJOR_VERSION >= 2
    if (g_mouseBits & 2 && osd->flags & OSD_CAPTURE && SDL_HasClipboardText())
    {
        auto text = SDL_GetClipboardText();
        OSD_HandleClipboard(text);
        SDL_free(text);
    }
#endif

    if (inputchecked && g_mouseEnabled)
    {
        if (g_mouseCallback)
        {
            if (g_mouseBits & 16)
                g_mouseCallback(5, 0);
            if (g_mouseBits & 32)
                g_mouseCallback(6, 0);
        }

        OSD_HandleWheel();
        g_mouseBits &= ~(16 | 32);
    }

    rv = handleevents_pollsdl();

    inputchecked = 0;
    timerUpdateClock();

    communityapiRunCallbacks();

    if (!frameplace && sdl_resize.x)
    {
        if (in3dmode())
            videoSetGameMode(fullscreen, sdl_resize.x, sdl_resize.y, bpp, upscalefactor);
        else
            videoSet2dMode(fullscreen, sdl_resize.x, sdl_resize.y, upscalefactor);

        sdl_resize = {};
    }

#ifndef _WIN32
    startwin_idle(NULL);
#endif

    return rv;
}

#if SDL_MAJOR_VERSION < 2
# include "sdlayer12.cpp"
#endif
