// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)
// by the EDuke32 team (development@voidpoint.com)

#pragma once

#ifndef build_h_
#define build_h_

#if !defined __cplusplus || (__cplusplus < 201103L && !defined _MSC_VER)
# error C++11 or greater is required.
#endif

#if defined _MSC_VER && _MSC_VER < 1800
# error Visual Studio 2013 is the minimum supported version.
#endif

#include "collections.h"
#include "compat.h"
#include "glad/glad.h"
#include "glbuild.h"
#include "palette.h"
#include "pragmas.h"
#include "random.h"

#include "vfs.h"
#include "cache1d.h"

#ifdef __cplusplus
extern "C" {
#endif

enum rendmode_t {
    REND_CLASSIC,
    REND_POLYMOST = 3,
    REND_POLYMER
};

#define PI 3.14159265358979323846
#define fPI 3.14159265358979323846f

#define BANG2RAD (fPI * (1.f/1024.f))

#define MAXSECTORSV8 4096
#define MAXWALLSV8 16384
#define MAXSPRITESV8 16384

#define MAXSECTORSV7 1024
#define MAXWALLSV7 8192
#define MAXSPRITESV7 4096

#define MAXVOXMIPS 5

#if !defined GEKKO && !defined __OPENDINGUX__
# define MAXSECTORS MAXSECTORSV8
# define MAXWALLS MAXWALLSV8
# define MAXSPRITES MAXSPRITESV8

# define MAXXDIM 10240
# define MAXYDIM 4320
# define MINXDIM 640
# define MINYDIM 480

// additional space beyond wall, in walltypes:
# define M32_FIXME_WALLS 512
# define M32_FIXME_SECTORS 2
#else
# define MAXSECTORS MAXSECTORSV7
# define MAXWALLS MAXWALLSV7
# define MAXSPRITES MAXSPRITESV7

#ifdef GEKKO
#  define MAXXDIM 860
#  define MAXYDIM 490
# else
#  define MAXXDIM 320
#  define MAXYDIM 200
# endif

# define MINXDIM MAXXDIM
# define MINYDIM MAXYDIM
# define M32_FIXME_WALLS 0
# define M32_FIXME_SECTORS 0
#endif

//define NEW_MAP_FORMAT

#define MAXWALLSB (MAXWALLS>>1)

#define MAXTILES 30720
#define MAXUSERTILES (MAXTILES-256)  // reserve 256 tiles at the end

#define MAXVOXELS 2048
#define MAXSTATUS 1024
#define MAXPLAYERS 16
// Maximum number of component tiles in a multi-psky:
#define MAXPSKYTILES 16
#define MAXSPRITESONSCREEN 4096
#define MAXUNIQHUDID 256 //Extra slots so HUD models can store animation state without messing game sprites

#define TSPR_TEMP 99

#define PR_LIGHT_PRIO_MAX       0
#define PR_LIGHT_PRIO_MAX_GAME  1
#define PR_LIGHT_PRIO_HIGH      2
#define PR_LIGHT_PRIO_HIGH_GAME 3
#define PR_LIGHT_PRIO_LOW       4
#define PR_LIGHT_PRIO_LOW_GAME  5

#define CLOCKTICKSPERSECOND 120

// Convenient sprite iterators, must not be used if any sprites inside the loop
// are potentially deleted or their sector changed...
#define SPRITES_OF(Statnum, Iter)  Iter=headspritestat[Statnum]; Iter>=0; Iter=nextspritestat[Iter]
#define SPRITES_OF_SECT(Sectnum, Iter)  Iter=headspritesect[Sectnum]; Iter>=0; Iter=nextspritesect[Iter]
// ... in which case this iterator may be used:
#define SPRITES_OF_SECT_SAFE(Sectnum, Iter, Next)  Iter=headspritesect[Sectnum]; \
    Iter>=0 && (Next=nextspritesect[Iter], 1); Iter=Next
#define SPRITES_OF_STAT_SAFE(Statnum, Iter, Next)  Iter=headspritestat[Statnum]; \
    Iter>=0 && (Next=nextspritestat[Iter], 1); Iter=Next

#define CLEARLINES2D(Startline, Numlines, Color) \
    clearbuf((char *)(frameplace + ((Startline)*bytesperline)), (bytesperline*(Numlines))>>2, (Color))


////////// True Room over Room (YAX == rot -17 of "PRO") //////////
#define YAX_ENABLE
//#define YAX_DEBUG
//#define ENGINE_SCREENSHOT_DEBUG

#ifdef YAX_ENABLE
# if !defined NEW_MAP_FORMAT
#  define YAX_ENABLE__COMPAT
# endif
#endif

////////// yax defs //////////
#define SECTORFLD(Sect,Fld, Cf) (*((Cf) ? (&sector[Sect].floor##Fld) : (&sector[Sect].ceiling##Fld)))

#define YAX_CEILING 0  // don't change!
#define YAX_FLOOR 1  // don't change!

# ifdef NEW_MAP_FORMAT
#  define YAX_MAXBUNCHES 512
#  define YAX_BIT__COMPAT 1024
#  define YAX_NEXTWALLBIT__COMPAT(Cf) (1<<(10+Cf))
#  define YAX_NEXTWALLBITS__COMPAT (YAX_NEXTWALLBIT__COMPAT(0)|YAX_NEXTWALLBIT__COMPAT(1))
# else
#  define YAX_MAXBUNCHES 256
#  define YAX_BIT 1024
   // "has next wall when constrained"-bit (1<<10: ceiling, 1<<11: floor)
#  define YAX_NEXTWALLBIT(Cf) (1<<(10+Cf))
#  define YAX_NEXTWALLBITS (YAX_NEXTWALLBIT(0)|YAX_NEXTWALLBIT(1))
# endif

int32_t get_alwaysshowgray(void);  // editor only
int32_t get_skipgraysectors(void);
void yax_updategrays(int32_t posze);

#ifdef YAX_ENABLE
# ifdef NEW_MAP_FORMAT
   // New map format -- no hijacking of otherwise used members.
#  define YAX_PTRNEXTWALL(Ptr, Wall, Cf) (*(&Ptr[Wall].upwall + Cf))
#  define YAX_NEXTWALLDEFAULT(Cf) (-1)
# else
   // More user tag hijacking: lotag/extra. :/
#  define YAX_PTRNEXTWALL(Ptr, Wall, Cf) (*(int16_t *)(&Ptr[Wall].lotag + (bloodhack ? 1 : 2)*Cf))
#  define YAX_NEXTWALLDEFAULT(Cf) (bloodhack ? 0 : ((Cf)==YAX_CEILING) ? 0 : -1)
   extern int16_t yax_bunchnum[MAXSECTORS][2];
   extern int16_t yax_nextwall[MAXWALLS][2];
# endif

# define YAX_NEXTWALL(Wall, Cf) YAX_PTRNEXTWALL(wall, Wall, Cf)

# define YAX_ITER_WALLS(Wal, Itervar, Cfvar) Cfvar=0, Itervar=(Wal); Itervar!=-1; \
    Itervar=yax_getnextwall(Itervar, Cfvar), \
        (void)(Itervar==-1 && Cfvar==0 && (Cfvar=1) && (Itervar=yax_getnextwall((Wal), Cfvar)))

# define SECTORS_OF_BUNCH(Bunchnum, Cf, Itervar) Itervar = headsectbunch[Cf][Bunchnum]; \
    Itervar != -1; Itervar = nextsectbunch[Cf][Itervar]

extern int32_t r_tror_nomaskpass;

# ifdef NEW_MAP_FORMAT
// Moved below declarations of sector, wall, sprite.
# else
int16_t yax_getbunch(int16_t i, int16_t cf);
static FORCE_INLINE void yax_getbunches(int16_t i, int16_t *cb, int16_t *fb)
{
    *cb = yax_getbunch(i, YAX_CEILING);
    *fb = yax_getbunch(i, YAX_FLOOR);
}
int16_t yax_getnextwall(int16_t wal, int16_t cf);
void yax_setnextwall(int16_t wal, int16_t cf, int16_t thenextwall);
# endif

void yax_setbunch(int16_t i, int16_t cf, int16_t bunchnum);
void yax_setbunches(int16_t i, int16_t cb, int16_t fb);
int16_t yax_vnextsec(int16_t line, int16_t cf);
void yax_update(int32_t resetstat);
int32_t yax_getneighborsect(int32_t x, int32_t y, int32_t sectnum, int32_t cf);

static FORCE_INLINE CONSTEXPR int32_t yax_waltosecmask(int32_t const walclipmask)
{
    // blocking: walstat&1 --> secstat&512
    // hitscan: walstat&64 --> secstat&2048
    return ((walclipmask&1)<<9) | ((walclipmask&64)<<5);
}
void yax_preparedrawrooms(void);
void yax_drawrooms(void (*SpriteAnimFunc)(int32_t,int32_t,int32_t,int32_t,int32_t),
                   int16_t sectnum, int32_t didmirror, int32_t smoothr);
# define YAX_SKIPSECTOR(i) if (bitmap_test(graysectbitmap, (i))) continue
# define YAX_SKIPWALL(i) if (bitmap_test(graywallbitmap, (i))) continue
#else
# define yax_preparedrawrooms()
# define yax_drawrooms(SpriteAnimFunc, sectnum, didmirror, smoothr)
# define YAX_SKIPSECTOR(i) (i)=(i)
# define YAX_SKIPWALL(i) (i)=(i)
#endif

#define CLIPMASK0 (((1L)<<16)+1L)
#define CLIPMASK1 (((256L)<<16)+64L)

#define NEXTWALL(i) (wall[wall[i].nextwall])
#define POINT2(i) (wall[wall[i].point2])

// max x/y val (= max editorgridextent in Mapster32)
#define BXY_MAX 524288

// rotatesprite 'orientation' (actually much more) bits
enum {
    RS_TRANS1 = 1,
    RS_AUTO = 2,
    RS_YFLIP = 4,
    RS_NOCLIP = 8,
    RS_TOPLEFT = 16,
    RS_TRANS2 = 32,
    RS_TRANS_MASK = RS_TRANS1|RS_TRANS2,
    RS_NOMASK = 64,
    RS_PERM = 128,

    RS_ALIGN_L = 256,
    RS_ALIGN_R = 512,
    RS_ALIGN_MASK = RS_ALIGN_L|RS_ALIGN_R,
    RS_STRETCH = 1024,

    ROTATESPRITE_FULL16 = 2048,
    RS_LERP = 4096,
    RS_FORCELERP = 8192,
    RS_NOPOSLERP = 16384,
    RS_NOZOOMLERP = 32768,
    RS_NOANGLERP = 65536,

    // ROTATESPRITE_MAX-1 is the mask of all externally available orientation bits
    ROTATESPRITE_MAX = 131072,

    RS_CENTERORIGIN = (1<<30),
};

    //Make all variables in BUILD.H defined in the ENGINE,
    //and externed in GAME
#ifdef engine_c_
#  define EXTERN
#else
#  define EXTERN extern
#endif

#ifdef __cplusplus
}
#endif

#if defined __cplusplus && (defined USE_OPENGL || defined POLYMER)
# define USE_STRUCT_TRACKERS
#endif

#ifdef USE_STRUCT_TRACKERS

extern "C" {
static FORCE_INLINE void sector_tracker_hook__(intptr_t address);
static FORCE_INLINE void wall_tracker_hook__(intptr_t address);
static FORCE_INLINE void sprite_tracker_hook__(intptr_t address);
}

#define TRACKER_NAME__ SectorTracker
#define TRACKER_HOOK_ sector_tracker_hook__
#include "tracker.hpp"
#undef TRACKER_NAME__
#undef TRACKER_HOOK_

#define TRACKER_NAME__ WallTracker
#define TRACKER_HOOK_ wall_tracker_hook__
#include "tracker.hpp"
#undef TRACKER_NAME__
#undef TRACKER_HOOK_

#define TRACKER_NAME__ SpriteTracker
#define TRACKER_HOOK_ sprite_tracker_hook__
#include "tracker.hpp"
#undef TRACKER_NAME__
#undef TRACKER_HOOK_

#define Tracker(Container, Type) Container##Tracker<Type>
#define TrackerCast(x) x.cast()

#else

#define Tracker(Container, Type) Type
#define TrackerCast(x) x

#endif // __cplusplus

// Links to various ABIs specifying (or documenting non-normatively) the
// alignment requirements of aggregates:
//
//  System V AMD64: http://www.x86-64.org/documentation/abi-0.99.pdf
//   (x86-64.org down as of 2013-02-02?)
//  "An array uses the same alignment as its elements, except that a local or global
//   array variable of length at least 16 bytes or a C99 variable-length array variable
//   always has alignment of at least 16 bytes."
//   (Not reproducible with GCC or LuaJIT on Ubuntu)
//
//  Win64: http://msdn.microsoft.com/en-us/library/9dbwhz68.aspx
//
//  x86: http://en.wikipedia.org/wiki/Data_structure_alignment#Typical_alignment_of_C_structs_on_x86

#define UNTRACKED_STRUCTS__
#include "buildtypes.h"
#undef UNTRACKED_STRUCTS__
#undef buildtypes_h__
#include "buildtypes.h"

#if !defined NEW_MAP_FORMAT
using sectortype  = sectortypev7;
using usectortype = usectortypev7;

using walltype  = walltypev7;
using uwalltype = uwalltypev7;
#else
using sectortype  = sectortypevx;
using usectortype = usectortypevx;

using walltype  = walltypevx;
using uwalltype = uwalltypevx;
#endif

using spritetype  = spritetypev7;
using uspritetype = uspritetypev7;

using uspriteptr_t = uspritetype const *;
using uwallptr_t   = uwalltype const *;
using usectorptr_t = usectortype const *;
using tspriteptr_t = tspritetype *;

// this is probably never going to be necessary
EDUKE32_STATIC_ASSERT(sizeof(sectortype) == sizeof(usectortype));
EDUKE32_STATIC_ASSERT(sizeof(walltype) == sizeof(uwalltype));
EDUKE32_STATIC_ASSERT(sizeof(spritetype) == sizeof(uspritetype));

#ifdef NEW_MAP_FORMAT

# define SECTORVX_SZ1 offsetof(sectortypevx, ceilingpicnum)
# define SECTORVX_SZ4 sizeof(sectortypevx)-offsetof(sectortypevx, visibility)

static inline void copy_v7_from_vx_sector(usectortypev7 *v7sec, const sectortypevx *vxsec)
{
    /* [wallptr..wallnum] */
    Bmemcpy(v7sec, vxsec, SECTORVX_SZ1);

    /* ceiling* */
    v7sec->ceilingpicnum = vxsec->ceilingpicnum;
    v7sec->ceilingheinum = vxsec->ceilingheinum;
    v7sec->ceilingstat = vxsec->ceilingstat;
    v7sec->ceilingz = vxsec->ceilingz;
    v7sec->ceilingshade = vxsec->ceilingshade;
    v7sec->ceilingpal = vxsec->ceilingpal;
    v7sec->ceilingxpanning = vxsec->ceilingxpanning;
    v7sec->ceilingypanning = vxsec->ceilingypanning;

    /* floor* */
    v7sec->floorpicnum = vxsec->floorpicnum;
    v7sec->floorheinum = vxsec->floorheinum;
    v7sec->floorstat = vxsec->floorstat;
    v7sec->floorz = vxsec->floorz;
    v7sec->floorshade = vxsec->floorshade;
    v7sec->floorpal = vxsec->floorpal;
    v7sec->floorxpanning = vxsec->floorxpanning;
    v7sec->floorypanning = vxsec->floorypanning;

    /* [visibility..extra] */
    Bmemcpy(&v7sec->visibility, &vxsec->visibility, SECTORVX_SZ4);

    /* Clear YAX_BIT of ceiling and floor. (New-map format build saves TROR
     * maps as map-text.) */
    v7sec->ceilingstat &= ~YAX_BIT__COMPAT;
    v7sec->floorstat &= ~YAX_BIT__COMPAT;
}

static inline void inplace_vx_from_v7_sector(sectortypevx *vxsec)
{
    const sectortypev7 *v7sec = (sectortypev7 *)vxsec;
    usectortypev7 bakv7sec;

    // Can't do this in-place since the members were rearranged.
    Bmemcpy(&bakv7sec, v7sec, sizeof(sectortypev7));

    /* [wallptr..wallnum] is already at the right place */

    /* ceiling* */
    vxsec->ceilingpicnum = bakv7sec.ceilingpicnum;
    vxsec->ceilingheinum = bakv7sec.ceilingheinum;
    vxsec->ceilingstat = bakv7sec.ceilingstat;
    vxsec->ceilingz = bakv7sec.ceilingz;
    vxsec->ceilingshade = bakv7sec.ceilingshade;
    vxsec->ceilingpal = bakv7sec.ceilingpal;
    vxsec->ceilingxpanning = bakv7sec.ceilingxpanning;
    vxsec->ceilingypanning = bakv7sec.ceilingypanning;

    /* floor* */
    vxsec->floorpicnum = bakv7sec.floorpicnum;
    vxsec->floorheinum = bakv7sec.floorheinum;
    vxsec->floorstat = bakv7sec.floorstat;
    vxsec->floorz = bakv7sec.floorz;
    vxsec->floorshade = bakv7sec.floorshade;
    vxsec->floorpal = bakv7sec.floorpal;
    vxsec->floorxpanning = bakv7sec.floorxpanning;
    vxsec->floorypanning = bakv7sec.floorypanning;

    /* [visibility..extra] */
    Bmemmove(&vxsec->visibility, &bakv7sec.visibility, SECTORVX_SZ4);
}

static inline void inplace_vx_tweak_sector(sectortypevx *vxsec, int32_t yaxp)
{
    if (yaxp)
    {
        int32_t cisext = (vxsec->ceilingstat&YAX_BIT__COMPAT);
        int32_t fisext = (vxsec->floorstat&YAX_BIT__COMPAT);

        vxsec->ceilingbunch = cisext ? vxsec->ceilingxpanning : -1;
        vxsec->floorbunch = fisext ? vxsec->floorxpanning : -1;

        if (cisext)
            vxsec->ceilingxpanning = 0;
        if (fisext)
            vxsec->floorxpanning = 0;
    }
    else
    {
        vxsec->ceilingbunch = vxsec->floorbunch = -1;
    }

    /* Clear YAX_BIT of ceiling and floor (map-int VX doesn't use it). */
    vxsec->ceilingstat &= ~YAX_BIT__COMPAT;
    vxsec->floorstat &= ~YAX_BIT__COMPAT;
}

# undef SECTORVX_SZ1
# undef SECTORVX_SZ4

# define WALLVX_SZ2 offsetof(walltypevx, blend)-offsetof(walltypevx, cstat)

static inline void copy_v7_from_vx_wall(uwalltypev7 *v7wal, const walltypevx *vxwal)
{
    /* [x..nextsector] */
    Bmemcpy(v7wal, vxwal, offsetof(walltypevx, upwall));
    /* [cstat..extra] */
    Bmemcpy(&v7wal->cstat, &vxwal->cstat, WALLVX_SZ2);
    /* Clear YAX_NEXTWALLBITS. */
    v7wal->cstat &= ~YAX_NEXTWALLBITS__COMPAT;
}

static inline void inplace_vx_from_v7_wall(walltypevx *vxwal)
{
    const walltypev7 *v7wal = (walltypev7 *)vxwal;

    /* [cstat..extra] */
    Bmemmove(&vxwal->cstat, &v7wal->cstat, WALLVX_SZ2);

    vxwal->blend = vxwal->filler_ = 0;
}

static inline void inplace_vx_tweak_wall(walltypevx *vxwal, int32_t yaxp)
{
    if (yaxp)
    {
        int32_t haveupwall = (vxwal->cstat & YAX_NEXTWALLBIT__COMPAT(YAX_CEILING));
        int32_t havednwall = (vxwal->cstat & YAX_NEXTWALLBIT__COMPAT(YAX_FLOOR));

        vxwal->upwall = haveupwall ? vxwal->lotag : -1;
        vxwal->dnwall = havednwall ? vxwal->extra : -1;

        if (haveupwall)
            vxwal->lotag = 0;
        if (havednwall)
            vxwal->extra = -1;
    }
    else
    {
        vxwal->upwall = vxwal->dnwall = -1;
    }

    /* Clear YAX_NEXTWALLBITS (map-int VX doesn't use it). */
    vxwal->cstat &= ~YAX_NEXTWALLBITS__COMPAT;
}

# undef WALLVX_SZ2

#endif

#include "clip.h"

int32_t getwalldist(vec2_t const in, int const wallnum);
int32_t getwalldist(vec2_t const in, int const wallnum, vec2_t * const out);

#include "screenshot.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push,1)
typedef struct {
    union
    {
        tspriteptr_t tspr;
#if !defined UINTPTR_MAX
# error Need UINTPTR_MAX define to select between 32- and 64-bit structs
#endif
#if UINTPTR_MAX != UINT64_MAX
        /* On a 32-bit build, pad the struct so it has the same size everywhere. */
        uint64_t ptrfill;
#endif
    };

    float    alpha;
    uint16_t flags;

    // this is organized so that most of the bytes that will usually be zero are adjacent
    int16_t  mdanimcur;
    uint32_t mdanimtims;
    int16_t  mdangoff;
    uint8_t  xpanning, ypanning;
    int16_t  mdpitch, mdroll;
    vec3_t   mdpivot_offset, mdposition_offset;

    // since this goes into savegames, pad to 64 bytes for future usage
    uint8_t  filler[12];
} spriteext_t;

EDUKE32_STATIC_ASSERT(offsetof(spriteext_t, flags) % 4 == 0);
EDUKE32_STATIC_ASSERT(offsetof(spriteext_t, mdangoff) % 4 == 0);
EDUKE32_STATIC_ASSERT(offsetof(spriteext_t, mdanimtims) % 4 == 0);
EDUKE32_STATIC_ASSERT(offsetof(spriteext_t, mdposition_offset) % 4 == 0);
EDUKE32_STATIC_ASSERT(sizeof(spriteext_t) == 64);

typedef struct {
    float smoothduration;
    int16_t mdcurframe, mdoldframe;
    int16_t mdsmooth;
} spritesmooth_t;

#ifndef NEW_MAP_FORMAT
typedef struct {
    uint8_t blend;
} wallext_t;
#endif
#pragma pack(pop)

#define SPREXT_NOTMD 1
#define SPREXT_NOMDANIM 2
#define SPREXT_AWAY1 4
#define SPREXT_AWAY2 8
#define SPREXT_TSPRACCESS 16
#define SPREXT_TEMPINVISIBLE 32

#define NEG_ALPHA_TO_BLEND(alpha, blend, orientation) do { \
    if ((alpha) < 0) { (blend) = -(alpha); (alpha) = 0; (orientation) |= RS_TRANS1; } \
} while (0)

// using the clipdist field
enum
{
    TSPR_FLAGS_MDHACK = 1u<<0u,
    TSPR_FLAGS_DRAW_LAST = 1u<<1u,
    TSPR_FLAGS_NO_SHADOW = 1u<<2u,
    TSPR_FLAGS_INVISIBLE_WITH_SHADOW = 1u<<3u,
    TSPR_FLAGS_SLAB = 1u<<4u,
    TSPR_FLAGS_NO_GLOW = 1u<<5u,
};

EXTERN int32_t guniqhudid;
EXTERN int32_t spritesortcnt;
extern int32_t g_loadedMapVersion;

typedef struct {
    char *mhkfile;
    char *title;
    char *mapart;
    uint8_t md4[16];
} usermaphack_t;

extern usermaphack_t g_loadedMapHack;
extern int compare_usermaphacks(const void *, const void *);
extern usermaphack_t *usermaphacks;
extern int32_t num_usermaphacks;
extern usermaphack_t *find_usermaphack();

#if !defined DEBUG_MAIN_ARRAYS
EXTERN spriteext_t *spriteext;
EXTERN spritesmooth_t *spritesmooth;
# ifndef NEW_MAP_FORMAT
EXTERN wallext_t *wallext;
# endif

EXTERN sectortype *sector;
EXTERN walltype *wall;
EXTERN spritetype *sprite;
EXTERN tspriteptr_t tsprite;
#else
EXTERN spriteext_t spriteext[MAXSPRITES+MAXUNIQHUDID];
EXTERN spritesmooth_t spritesmooth[MAXSPRITES+MAXUNIQHUDID];
# ifndef NEW_MAP_FORMAT
EXTERN wallext_t wallext[MAXWALLS];
# endif

EXTERN sectortype sector[MAXSECTORS + M32_FIXME_SECTORS];
EXTERN walltype wall[MAXWALLS + M32_FIXME_WALLS];
EXTERN spritetype sprite[MAXSPRITES];
EXTERN tspritetype tsprite[MAXSPRITESONSCREEN];
#endif

#ifdef USE_STRUCT_TRACKERS
EXTERN uint32_t sectorchanged[MAXSECTORS + M32_FIXME_SECTORS];
EXTERN uint32_t wallchanged[MAXWALLS + M32_FIXME_WALLS];
EXTERN uint32_t spritechanged[MAXSPRITES];
#endif

#ifdef NEW_MAP_FORMAT
static FORCE_INLINE int16_t yax_getbunch(int16_t i, int16_t cf)
{
    return cf ? sector[i].floorbunch : sector[i].ceilingbunch;
}

static FORCE_INLINE void yax_getbunches(int16_t i, int16_t *cb, int16_t *fb)
{
    *cb = yax_getbunch(i, YAX_CEILING);
    *fb = yax_getbunch(i, YAX_FLOOR);
}

static FORCE_INLINE int16_t yax_getnextwall(int16_t wal, int16_t cf)
{
    return cf ? wall[wal].dnwall : wall[wal].upwall;
}

static FORCE_INLINE void yax_setnextwall(int16_t wal, int16_t cf, int16_t thenextwall)
{
    YAX_NEXTWALL(wal, cf) = thenextwall;
}
#endif

#ifdef USE_STRUCT_TRACKERS
static FORCE_INLINE void sector_tracker_hook__(intptr_t const address)
{
    intptr_t const sectnum = (address - (intptr_t)sector) / sizeof(sectortype);

#if DEBUGGINGAIDS>=2
    Bassert((unsigned)sectnum < ((MAXSECTORS + M32_FIXME_SECTORS)));
#endif

    ++sectorchanged[sectnum];
}

static FORCE_INLINE void wall_tracker_hook__(intptr_t const address)
{
    intptr_t const wallnum = (address - (intptr_t)wall) / sizeof(walltype);

#if DEBUGGINGAIDS>=2
    Bassert((unsigned)wallnum < ((MAXWALLS + M32_FIXME_WALLS)));
#endif

    ++wallchanged[wallnum];
}

static FORCE_INLINE void sprite_tracker_hook__(intptr_t const address)
{
    intptr_t const spritenum = (address - (intptr_t)sprite) / sizeof(spritetype);

#if DEBUGGINGAIDS>=2
    Bassert((unsigned)spritenum < MAXSPRITES);
#endif

    ++spritechanged[spritenum];
}
#endif

static inline tspriteptr_t renderMakeTSpriteFromSprite(tspriteptr_t const tspr, uint16_t const spritenum)
{
    auto const spr = (uspriteptr_t)&sprite[spritenum];

    tspr->xyz = spr->xyz;
    tspr->cstat = spr->cstat;
    tspr->picnum = spr->picnum;
    tspr->shade = spr->shade;
    tspr->pal = spr->pal;
    tspr->blend = spr->blend;
    tspr->xrepeat = spr->xrepeat;
    tspr->yrepeat = spr->yrepeat;
    tspr->xoffset = spr->xoffset;
    tspr->yoffset = spr->yoffset;
    tspr->sectnum = spr->sectnum;
    tspr->statnum = spr->statnum;
    tspr->ang = spr->ang;
    tspr->vel = spr->vel;
    tspr->lotag = spr->lotag;
    tspr->hitag = spr->hitag;
    tspr->extra = spr->extra;

    tspr->clipdist = 0;
    tspr->owner = spritenum;

    return tspr;
}

static inline tspriteptr_t renderAddTSpriteFromSprite(uint16_t const spritenum)
{
    return renderMakeTSpriteFromSprite(&tsprite[spritesortcnt++], spritenum);
}

static inline void spriteSetSlope(uint16_t const spritenum, int16_t const heinum)
{
    auto const spr = &sprite[spritenum];
    uint16_t const cstat = spr->cstat;
    if (!(cstat & CSTAT_SPRITE_ALIGNMENT_FLOOR))
        return;

    spr->xoffset = heinum & 255;
    spr->yoffset = (heinum >> 8) & 255;
    spr->cstat &= ~CSTAT_SPRITE_ALIGNMENT_MASK;
    spr->cstat |= heinum != 0 ? CSTAT_SPRITE_ALIGNMENT_SLOPE : CSTAT_SPRITE_ALIGNMENT_FLOOR;
}

static inline int16_t spriteGetSlope(uint16_t const spritenum)
{
    auto const spr = (uspriteptr_t)&sprite[spritenum];
    uint16_t const cstat = spr->cstat & CSTAT_SPRITE_ALIGNMENT_MASK;
    if (cstat != CSTAT_SPRITE_ALIGNMENT_SLOPE)
        return 0;
    return uint8_t(spr->xoffset) + (uint8_t(spr->yoffset) << 8);
}

extern int32_t animateoffs(int const tilenum, int fakevar);

EXTERN int16_t maskwall[MAXWALLSB], maskwallcnt;
EXTERN int16_t thewall[MAXWALLSB];
EXTERN tspriteptr_t tspriteptr[MAXSPRITESONSCREEN + 1];

EXTERN int32_t wx1, wy1, wx2, wy2;
EXTERN int32_t xdim, ydim, numpages, upscalefactor;
EXTERN int32_t yxaspect, viewingrange;
extern int32_t oxyaspect;
EXTERN intptr_t *ylookup;

EXTERN int32_t rotatesprite_y_offset;
EXTERN int32_t rotatesprite_yxaspect;

#ifndef GEKKO
#define MAXVALIDMODES 256
#else
#define MAXVALIDMODES 16
#endif
EXTERN int32_t validmodecnt;
struct validmode_t {
    int32_t xdim,ydim;
    char bpp;
    char fs;    // bit 0 = fullscreen flag
    char filler[2];
    int32_t extra; // internal use
};
EXTERN struct validmode_t validmode[MAXVALIDMODES];

EXTERN int32_t numyaxbunches;
#ifdef YAX_ENABLE
// Singly-linked list of sectnums grouped by bunches and ceiling (0)/floor (1)
// Usage e.g.:
//   int16_t bunchnum = yax_getbunch(somesector, YAX_CEILING);
// Iteration over all sectors whose floor bunchnum equals 'bunchnum' (i.e. "all
// floors of the other side"):
//   for (i=headsectbunch[1][bunchnum]; i!=-1; i=nextsectbunch[1][i])
//       <do stuff with sector i...>

EXTERN int16_t headsectbunch[2][YAX_MAXBUNCHES], nextsectbunch[2][MAXSECTORS];
#endif

EXTERN int32_t Numsprites;
EXTERN int16_t numsectors, numwalls;
EXTERN int32_t display_mirror;
// totalclocklock: the totalclock value that is backed up once on each
// drawrooms() and is used for animateoffs().
EXTERN ClockTicks totalclock, totalclocklock;
static inline int32_t BGetTime(void) { return (int32_t) totalclock; }
EXTERN int32_t rotatespritesmoothratio;
EXTERN int32_t numframes;
EXTERN int16_t sintable[2048];

EXTERN uint8_t palette[768];
EXTERN int16_t numshades;
EXTERN char *palookup[MAXPALOOKUPS];
extern uint8_t *basepaltable[MAXBASEPALS];
EXTERN uint8_t paletteloaded;
EXTERN char *blendtable[MAXBLENDTABS];
EXTERN uint8_t whitecol, redcol, blackcol;

EXTERN int32_t maxspritesonscreen;

enum {
    PALETTE_MAIN = 1<<0,
    PALETTE_SHADE = 1<<1,
    PALETTE_TRANSLUC = 1<<2,
};

EXTERN char showinvisibility;
EXTERN int32_t g_visibility, parallaxvisibility;
EXTERN int32_t g_rotatespriteNoWidescreen;

// blendtable[1] to blendtable[numalphatabs] are considered to be
// alpha-blending tables:
EXTERN uint8_t numalphatabs;

EXTERN vec2_t windowxy1, windowxy2;
EXTERN int16_t *startumost, *startdmost;

// The maximum tile offset ever used in any tiled parallaxed multi-sky.
#define PSKYOFF_MAX 16
#define DEFAULTPSKY -1

typedef struct {
    // The proportion at which looking up/down affects the apparent 'horiz' of
    // a parallaxed sky, scaled by 65536 (so, a value of 65536 makes it align
    // with the drawn surrounding scene):
    int32_t horizfrac;

    // The texel index offset in the y direction of a parallaxed sky:
    // XXX: currently always 0.
    int32_t yoffs;

    int8_t lognumtiles;  // 1<<lognumtiles: number of tiles in multi-sky
    int8_t tileofs[MAXPSKYTILES];  // for 0 <= j < (1<<lognumtiles): tile offset relative to basetile

    int32_t yscale;
} psky_t;

// Index of map-global (legacy) multi-sky:
EXTERN int32_t g_pskyidx;
// New multi-psky
EXTERN int32_t pskynummultis;
EXTERN psky_t * multipsky;
// Mapping of multi-sky index to base sky tile number:
EXTERN int32_t * multipskytile;

static FORCE_INLINE int32_t getpskyidx(int32_t picnum)
{
    int32_t j;

    for (j=pskynummultis-1; j>0; j--)  // NOTE: j==0 on non-early loop end
        if (picnum == multipskytile[j])
            break;  // Have a match.

    return j;
}

EXTERN psky_t * tileSetupSky(int32_t tilenum);

EXTERN char parallaxtype;
EXTERN int32_t parallaxyoffs_override, parallaxyscale_override;
extern int16_t pskybits_override;

// last sprite in the freelist, that is the spritenum for which
//   .statnum==MAXSTATUS && nextspritestat[spritenum]==-1
// (or -1 if freelist is empty):
EXTERN int16_t tailspritefree;

EXTERN int16_t headspritesect[MAXSECTORS+1], headspritestat[MAXSTATUS+1];
EXTERN int16_t prevspritesect[MAXSPRITES], prevspritestat[MAXSPRITES];
EXTERN int16_t nextspritesect[MAXSPRITES], nextspritestat[MAXSPRITES];

EXTERN vec2_16_t tilesiz[MAXTILES];

EXTERN char picsiz[MAXTILES];
EXTERN char walock[MAXTILES];

extern const char pow2char_[];
static CONSTEXPR const int32_t pow2long[32] =
{
    1, 2, 4, 8,
    16, 32, 64, 128,
    256, 512, 1024, 2048,
    4096, 8192, 16384, 32768,
    65536, 131072, 262144, 524288,
    1048576, 2097152, 4194304, 8388608,
    16777216, 33554432, 67108864, 134217728,
    268435456, 536870912, 1073741824, 2147483647 /* INT32_MAX */
};

static CONSTEXPR const uint32_t pow2ulong[32] =
{
    1, 2, 4, 8,
    16, 32, 64, 128,
    256, 512, 1024, 2048,
    4096, 8192, 16384, 32768,
    65536, 131072, 262144, 524288,
    1048576, 2097152, 4194304, 8388608,
    16777216, 33554432, 67108864, 134217728,
    268435456, 536870912, 1073741824, 2147483648
};

static CONSTEXPR const int64_t pow264[64] =
{
    1, 2, 4, 8,
    16, 32, 64, 128,
    256, 512, 1024, 2048,
    4096, 8192, 16384, 32768,
    65536, 131072, 262144, 524288,
    1048576, 2097152, 4194304, 8388608,
    16777216, 33554432, 67108864, 134217728,
    268435456, 536870912, 1073741824, 2147483648,
    4294967296, 8589934592, 17179869184, 34359738368,
    68719476736, 137438953472, 274877906944, 549755813888,
    1099511627776, 2199023255552, 4398046511104, 8796093022208,
    17592186044416, 35184372088832, 70368744177664, 140737488355328,
    281474976710656, 562949953421312, 1125899906842624, 2251799813685248,
    4503599627370496, 9007199254740992, 18014398509481984, 36028797018963968,
    72057594037927936, 144115188075855872, 288230376151711744, 576460752303423488,
    1152921504606846976, 2305843009213693952, 4611686018427387904, INT64_MAX
};

// picanm[].sf:
// |bit(1<<7)
// |animtype|animtype|texhitscan|nofullbright|speed|speed|speed|speed|
enum {
    PICANM_ANIMTYPE_NONE = 0,
    PICANM_ANIMTYPE_OSC = (1<<6),
    PICANM_ANIMTYPE_FWD = (2<<6),
    PICANM_ANIMTYPE_BACK = (3<<6),

    PICANM_ANIMTYPE_SHIFT = 6,
    PICANM_ANIMTYPE_MASK = (3<<6),  // must be 192
    PICANM_MISC_MASK = (3<<4),
    PICANM_TEXHITSCAN_BIT = (2<<4),
    PICANM_NOFULLBRIGHT_BIT = (1<<4),
    PICANM_ANIMSPEED_MASK = 15,  // must be 15
};

enum {
    TILEFLAGS_NONE = 0,
    TILEFLAGS_TRUENPOT = (1<<1),
};

// NOTE: If the layout of this struct is changed, loadpics() must be modified
// accordingly.
typedef struct {
    uint8_t num;  // animate number
    int8_t xofs, yofs;
    uint8_t sf;  // anim. speed and flags
    uint8_t tileflags; // tile-specific flags, such as true non-power-of-2 drawing.
    uint8_t extra;
} picanm_t;
EXTERN picanm_t picanm[MAXTILES];
typedef struct { int16_t newtile; int16_t owner; } rottile_t;
EXTERN rottile_t rottile[MAXTILES];
EXTERN intptr_t waloff[MAXTILES];  // stores pointers to cache  -- SA

EXTERN vec2_t g_windowPos;
EXTERN bool g_windowPosValid;


    //These variables are for auto-mapping with the draw2dscreen function.
    //When you load a new board, these bits are all set to 0 - since
    //you haven't mapped out anything yet.  Note that these arrays are
    //bit-mapped.
    //If you want draw2dscreen() to show sprite #54 then you say:
    //   spritenum = 54;
    //   bitmap_set(show2dsprite, spritenum);
    //And if you want draw2dscreen() to not show sprite #54 then you say:
    //   spritenum = 54;
    //   bitmap_clear(show2dsprite, spritenum);

EXTERN int automapping;
EXTERN char show2dsector[bitmap_size(MAXSECTORS)];
EXTERN char show2dwall[bitmap_size(MAXWALLS)];
EXTERN char show2dsprite[bitmap_size(MAXSPRITES)];

struct classicht_t
{
    intptr_t ptr;
    vec2_16_t upscale;
    char lock;
};

EXTERN classicht_t classicht[MAXTILES];

// In the editor, gotpic is only referenced from inline assembly;
// the compiler needs that hint or building with LTO will discard it.
#if !defined __clang__ && !defined NOASM
# define GOTPIC_USED ATTRIBUTE((used))
#else
# define GOTPIC_USED
#endif

EXTERN char GOTPIC_USED gotpic[bitmap_size(MAXTILES)];
EXTERN char gotsector[bitmap_size(MAXSECTORS)];

EXTERN char editorcolors[256];
EXTERN char editorcolorsdef[256];

EXTERN char faketile[bitmap_size(MAXTILES)];
EXTERN char *faketiledata[MAXTILES];
EXTERN int faketilesize[MAXTILES];

EXTERN uint8_t tilefilenum[MAXTILES];

EXTERN char spritecol2d[MAXTILES][2];
EXTERN uint8_t tilecols[MAXTILES];

EXTERN char editwall[bitmap_size(MAXWALLS)];

extern uint8_t vgapal16[4*256];

extern uint32_t drawlinepat;

extern void faketimerhandler(void);

extern char apptitle[256];

extern int32_t novoxmips;

#ifdef DEBUGGINGAIDS
extern float debug1, debug2;
#endif

extern int16_t tiletovox[MAXTILES];
extern int32_t usevoxels, voxscale[MAXVOXELS];
extern uint8_t voxflags[MAXVOXELS];
extern char g_haveVoxels;

enum
{
    VF_NOTRANS = 1<<0,
    // begin downstream
    VF_ROTATE  = 1<<6,
    VF_RESERVE = 1<<7,
    // end downstream
};

extern int32_t usehightile;

#ifdef USE_OPENGL
extern int32_t usemodels;
extern int32_t rendmode;
#endif
extern uint8_t globalr, globalg, globalb;
EXTERN uint16_t h_xsize[MAXTILES], h_ysize[MAXTILES];
EXTERN int8_t h_xoffs[MAXTILES], h_yoffs[MAXTILES];

EXTERN char *globalpalwritten;
EXTERN int16_t globalpicnum;

enum {
    GLOBAL_NO_GL_TILESHADES = 1<<0,
    GLOBAL_NO_GL_FULLBRIGHT = 1<<1,
    GLOBAL_NO_GL_FOGSHADE = 1<<2,
};

extern int32_t globalflags;

extern const char *engineerrstr;

EXTERN int32_t editorzrange[2];

static FORCE_INLINE int32_t videoGetRenderMode(void)
{
#ifndef USE_OPENGL
    return REND_CLASSIC;
#else
    return rendmode;
#endif
}

extern int32_t bloodhack;
enum
{
    ENGINE_19950829 = 19950829,  // Powerslave/Exhumed
    ENGINE_19960925 = 19960925,  // Blood v1.21
    ENGINE_19961112 = 19961112,  // Duke 3D v1.5, Redneck Rampage
    ENGINE_EDUKE32  = INT_MAX,
};

#ifndef EDUKE32_STANDALONE
extern int32_t enginecompatibilitymode;
#else
static int32_t enginecompatibilitymode = ENGINE_EDUKE32;
#endif

EXTERN int32_t crctab16[256];

EXTERN int32_t duke64;
extern bool (*rt_tileload_callback)(int16_t tileNum);

/*************************************************************************
POSITION VARIABLES:

        POSX is your x - position ranging from 0 to 65535
        POSY is your y - position ranging from 0 to 65535
            (the length of a side of the grid in EDITBORD would be 1024)
        POSZ is your z - position (height) ranging from 0 to 65535, 0 highest.
        ANG is your angle ranging from 0 to 2047.  Instead of 360 degrees, or
             2 * PI radians, I use 2048 different angles, so 90 degrees would
             be 512 in my system.

SPRITE VARIABLES:

    EXTERN short headspritesect[MAXSECTORS+1], headspritestat[MAXSTATUS+1];
    EXTERN short prevspritesect[MAXSPRITES], prevspritestat[MAXSPRITES];
    EXTERN short nextspritesect[MAXSPRITES], nextspritestat[MAXSPRITES];

    Example: if the linked lists look like the following:
         ????????????????
               Sector lists:               Status lists:
         ????????????????J
           Sector0:  4, 5, 8             Status0:  2, 0, 8
           Sector1:  16, 2, 0, 7         Status1:  4, 5, 16, 7, 3, 9
           Sector2:  3, 9
         ????????????????
    Notice that each number listed above is shown exactly once on both the
        left and right side.  This is because any sprite that exists must
        be in some sector, and must have some kind of status that you define.


Coding example #1:
    To go through all the sprites in sector 1, the code can look like this:

        sectnum = 1;
        i = headspritesect[sectnum];
        while (i != -1)
        {
            nexti = nextspritesect[i];

            //your code goes here
            //ex: printf("Sprite %d is in sector %d\n",i,sectnum);

            i = nexti;
        }

Coding example #2:
    To go through all sprites with status = 1, the code can look like this:

        statnum = 1;        //status 1
        i = headspritestat[statnum];
        while (i != -1)
        {
            nexti = nextspritestat[i];

            //your code goes here
            //ex: printf("Sprite %d has a status of 1 (active)\n",i,statnum);

            i = nexti;
        }

             insertsprite(short sectnum, short statnum);
             deletesprite(short spritenum);
             changespritesect(short spritenum, short newsectnum);
             changespritestat(short spritenum, short newstatnum);

TILE VARIABLES:
        NUMTILES - the number of tiles found TILES.DAT.
        TILESIZX[MAXTILES] - simply the x-dimension of the tile number.
        TILESIZY[MAXTILES] - simply the y-dimension of the tile number.
        WALOFF[MAXTILES] - the actual address pointing to the top-left
                                 corner of the tile.
        PICANM[MAXTILES] - flags for animating the tile.

TIMING VARIABLES:
        TOTALCLOCK - When the engine is initialized, TOTALCLOCK is set to zero.
            From then on, it is incremented 120 times a second by 1.  That
            means that the number of seconds elapsed is totalclock / 120.
        NUMFRAMES - The number of times the draw3dscreen function was called
            since the engine was initialized.  This helps to determine frame
            rate.  (Frame rate = numframes * 120 / totalclock.)

OTHER VARIABLES:

        STARTUMOST[320] is an array of the highest y-coordinates on each column
                that my engine is allowed to write to.  You need to set it only
                once.
        STARTDMOST[320] is an array of the lowest y-coordinates on each column
                that my engine is allowed to write to.  You need to set it only
                once.
        SINTABLE[2048] is a sin table with 2048 angles rather than the
            normal 360 angles for higher precision.  Also since SINTABLE is in
            all integers, the range is multiplied by 16383, so instead of the
            normal -1<sin(x)<1, the range of sintable is -16383<sintable[]<16383
            If you use this sintable, you can possibly speed up your code as
            well as save space in memory.  If you plan to use sintable, 2
            identities you may want to keep in mind are:
                sintable[ang&2047]       = sin(ang * (3.141592/1024)) * 16383
                sintable[(ang+512)&2047] = cos(ang * (3.141592/1024)) * 16383
        NUMSECTORS - the total number of existing sectors.  Modified every time
            you call the loadboard function.
***************************************************************************/

typedef struct {
    union { struct { int32_t x, y, z; }; vec3_t xyz; vec2_t xy; };
    int16_t sprite, wall, sect;
} hitdata_t;

typedef struct artheader_t {
    int32_t tilestart, tileend, numtiles;
    uint8_t* tileread;
} artheader_t;
#define ARTv1_UNITOFFSET ((signed)(4*sizeof(int32_t) + 2*sizeof(int16_t) + sizeof(uint32_t)))

int32_t    enginePreInit(void);	// a partial setup of the engine used for launch windows
int32_t    engineInit(void);
int32_t enginePostInit(void);
void   engineUnInit(void);
void   initspritelists(void);
int32_t engineFatalError(char const * msg);

int32_t   engineLoadBoard(const char *filename, char flags, vec3_t *dapos, int16_t *daang, int16_t *dacursectnum);
int32_t   engineLoadMHK(const char *filename);
void engineClearLightsFromMHK();
#ifdef HAVE_CLIPSHAPE_FEATURE
int32_t engineLoadClipMaps(void);
#endif
int32_t   saveboard(const char *filename, const vec3_t *dapos, int16_t daang, int16_t dacursectnum);

void    tileSetupDummy(int32_t tile);
void    tileSetData(int32_t tile, int32_t tsiz, char const *buffer);
void    tileDelete(int32_t tile);
void    tileSetSize(int32_t picnum, int16_t dasizx, int16_t dasizy);
int32_t artReadHeader(buildvfs_kfd fil, char const *fn, artheader_t *local);
int32_t artReadHeaderFromBuffer(uint8_t const *buf, artheader_t *local);
int32_t artCheckUnitFileHeader(uint8_t const *buf, int32_t length);
void    tileConvertAnimFormat(int32_t picnum, uint32_t const picanmdisk);
void    artReadManifest(buildvfs_kfd fil, artheader_t * const local);
void    artPreloadFile(buildvfs_kfd fil, artheader_t * const local);
int32_t artLoadFiles(const char *filename, int32_t askedsize);
void    artClearMapArt(void);
void    artSetupMapArt(const char *filename);
bool    tileLoad(int16_t tilenume);
void    tileLoadData(int16_t tilenume, int32_t dasiz, char *buffer);
intptr_t tileLoadScaled(int const picnum, vec2_16_t* upscale = nullptr);
int32_t tileGetCRC32(int16_t tileNum);
vec2_16_t tileGetSize(int16_t tileNum);
void    artConvertRGB(palette_t *pic, uint8_t const *buf, int32_t bufsizx, int32_t sizx, int32_t sizy);
void    tileUpdatePicSiz(int32_t picnum);

int32_t   qloadkvx(int32_t voxindex, const char *filename);
void vox_clearid(int32_t const);
void vox_undefine(int32_t const);
intptr_t   tileCreate(int16_t tilenume, int32_t xsiz, int32_t ysiz);
void   tileCopySection(int32_t tilenume1, int32_t sx1, int32_t sy1, int32_t xsiz, int32_t ysiz, int32_t tilenume2, int32_t sx2, int32_t sy2);
void   squarerotatetile(int16_t tilenume);

int32_t   videoSetGameMode(char davidoption, int32_t daupscaledxdim, int32_t daupscaledydim, int32_t dabpp, int32_t daupscalefactor);
void   videoNextPage(void);
void   videoSetCorrectedAspect();
void   videoSetViewableArea(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void   renderSetAspect(int32_t daxrange, int32_t daaspect);
void   renderFlushPerms(void);

void plotlines2d(const int32_t *xx, const int32_t *yy, int32_t numpoints, int col) ATTRIBUTE((nonnull(1,2)));

void   plotpixel(int32_t x, int32_t y, char col);
void   renderSetTarget(int16_t tilenume, int32_t xsiz, int32_t ysiz);
void   renderRestoreTarget(void);
void   renderPrepareMirror(int32_t dax, int32_t day, int32_t daz, fix16_t daang, fix16_t dahoriz, int16_t dawall,
                           int32_t *tposx, int32_t *tposy, fix16_t *tang);
void   renderCompleteMirror(void);

int32_t renderDrawRoomsQ16(int32_t daposx, int32_t daposy, int32_t daposz, fix16_t daang, fix16_t dahoriz, int16_t dacursectnum);

static FORCE_INLINE int32_t drawrooms(int32_t daposx, int32_t daposy, int32_t daposz, int16_t daang, int16_t dahoriz, int16_t dacursectnum)
{
    return renderDrawRoomsQ16(daposx, daposy, daposz, fix16_from_int(daang), fix16_from_int(dahoriz), dacursectnum);
}

void   renderDrawMasks(void);
void   videoClearViewableArea(int32_t dacol);
void   videoClearScreen(int32_t dacol);
void   renderDrawMapView(int32_t dax, int32_t day, int32_t zoome, int16_t ang);
void   rotatesprite_(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum,
                     int8_t dashade, char dapalnum, int32_t dastat, uint8_t daalpha, uint8_t dablend,
                     int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2);
void   renderDrawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2, char col);
void   drawlinergb(int32_t x1, int32_t y1, int32_t x2, int32_t y2, palette_t p);
int32_t    printext16(int32_t xpos, int32_t ypos, int16_t col, int16_t backcol,
                      const char *name, char fontsize) ATTRIBUTE((nonnull(5)));
void   printext256(int32_t xpos, int32_t ypos, int16_t col, int16_t backcol,
                   const char *name, char fontsize) ATTRIBUTE((nonnull(5)));
void   Buninitart(void);

void   initcrc16(void);
#define updatecrc16(crc,dat) (crc = (((crc<<8)&65535)^crctab16[((((uint16_t)crc)>>8)&65535)^(dat)]))
static FORCE_INLINE uint16_t getcrc16(void const* buffer, int bufleng, int crc = 0)
{
    for (int i = 0; i < bufleng; i++) updatecrc16(crc, ((char const*)buffer)[i]);
    return((uint16_t)(crc & 65535));
}


////////// specialized rotatesprite wrappers for (very) often used cases //////////
static FORCE_INLINE void rotatesprite(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum,
                                int8_t dashade, char dapalnum, int32_t dastat,
                                int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2)
{
    rotatesprite_(sx, sy, z, a, picnum, dashade, dapalnum, dastat, 0, 0, cx1, cy1, cx2, cy2);
}
// Don't clip at all, i.e. the whole screen real estate is available:
static FORCE_INLINE void rotatesprite_fs(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum,
                                   int8_t dashade, char dapalnum, int32_t dastat)
{
    rotatesprite_(sx, sy, z, a, picnum, dashade, dapalnum, dastat, 0, 0, 0,0,xdim-1,ydim-1);
}

static FORCE_INLINE void rotatesprite_fs_id(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum,
                                   int8_t dashade, char dapalnum, int32_t dastat, int16_t uniqid)
{
    int restore = guniqhudid;
    if (uniqid) guniqhudid = uniqid;
    rotatesprite_(sx, sy, z, a, picnum, dashade, dapalnum, dastat, 0, 0, 0,0,xdim-1,ydim-1);
    guniqhudid = restore;
}

static FORCE_INLINE void rotatesprite_fs_alpha(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum,
                                  int8_t dashade, char dapalnum, int32_t dastat, uint8_t alpha)
{
    rotatesprite_(sx, sy, z, a, picnum, dashade, dapalnum, dastat, alpha, 0, 0, 0, xdim-1, ydim-1);
}

static FORCE_INLINE void rotatesprite_win(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum,
                                    int8_t dashade, char dapalnum, int32_t dastat)
{
    rotatesprite_(sx, sy, z, a, picnum, dashade, dapalnum, dastat, 0, 0, windowxy1.x,windowxy1.y,windowxy2.x,windowxy2.y);
}

void   getzrange(const vec3_t *pos, int16_t sectnum, int32_t *ceilz, int32_t *ceilhit, int32_t *florz,
                 int32_t *florhit, int32_t walldist, uint32_t cliptype) ATTRIBUTE((nonnull(1,3,4,5,6)));
extern vec2_t hitscangoal;
int32_t   hitscan(const vec3_t *sv, int16_t sectnum, int32_t vx, int32_t vy, int32_t vz,
                  hitdata_t *hitinfo, uint32_t cliptype) ATTRIBUTE((nonnull(1,6)));
void   neartag(int32_t xs, int32_t ys, int32_t zs, int16_t sectnum, int16_t ange,
               int16_t *neartagsector, int16_t *neartagwall, int16_t *neartagsprite,
               int32_t *neartaghitdist, int32_t neartagrange, uint8_t tagsearch,
               int32_t (*blacklist_sprite_func)(int32_t)) ATTRIBUTE((nonnull(6,7,8)));
int32_t   cansee(int32_t x1, int32_t y1, int32_t z1, int16_t sect1,
                 int32_t x2, int32_t y2, int32_t z2, int16_t sect2, int32_t wallmask = CSTAT_WALL_1WAY);
int32_t   inside(int32_t x, int32_t y, int16_t sectnum);
void   calc_sector_reachability(void);
int    sectorsareconnected(int const, int const);
void   dragpoint(int16_t pointhighlight, int32_t dax, int32_t day, uint8_t flags);
void   setfirstwall(int16_t sectnum, int16_t newfirstwall);
int32_t try_facespr_intersect(uspriteptr_t const spr, vec3_t const in,
                                     int32_t vx, int32_t vy, int32_t vz,
                                     vec3_t * const intp, int32_t strictly_smaller_than_p);

#define MAXUPDATESECTORDIST 1536
#define INITIALUPDATESECTORDIST 256
void updatesector(int32_t const x, int32_t const y, int16_t * const sectnum) ATTRIBUTE((nonnull(3)));
void updatesectorexclude(int32_t const x, int32_t const y, int16_t * const sectnum,
                         const uint8_t * const excludesectbitmap) ATTRIBUTE((nonnull(3,4)));
void updatesectorz_compat(int32_t const x, int32_t const y, int32_t const z, int16_t * const sectnum) ATTRIBUTE((nonnull(4)));
void updatesectorz(int32_t const x, int32_t const y, int32_t const z, int16_t * const sectnum) ATTRIBUTE((nonnull(4)));
void updatesectorneighbor(int32_t const x, int32_t const y, int16_t * const sectnum, int32_t initialMaxDistance = INITIALUPDATESECTORDIST, int32_t maxDistance = MAXUPDATESECTORDIST) ATTRIBUTE((nonnull(3)));
void updatesectorneighborz(int32_t const x, int32_t const y, int32_t const z, int16_t * const sectnum, int32_t initialMaxDistance = INITIALUPDATESECTORDIST, int32_t maxDistance = MAXUPDATESECTORDIST) ATTRIBUTE((nonnull(4)));

int findwallbetweensectors(int sect1, int sect2);
static FORCE_INLINE int sectoradjacent(int sect1, int sect2) { return findwallbetweensectors(sect1, sect2) != -1; }
int32_t getsectordist(vec2_t const in, int const sectnum, vec2_t * const out = nullptr);
extern const int16_t *chsecptr_onextwall;
int32_t checksectorpointer(int16_t i, int16_t sectnum);

void   mouseGetValues(int32_t *mousx, int32_t *mousy, int32_t *bstatus) ATTRIBUTE((nonnull(1,2,3)));

int32_t   __fastcall ksqrtasm_old(uint32_t n);
int32_t   __fastcall ksqrt(uint32_t num);
int32_t   __fastcall getangle(int32_t xvect, int32_t yvect);
fix16_t   __fastcall gethiq16angle(int32_t xvect, int32_t yvect);
fix16_t   __fastcall getq16angledelta(fix16_t first, fix16_t second);

static FORCE_INLINE fix16_t __fastcall getq16angle(int32_t xvect, int32_t yvect)
{
    return fix16_from_int(getangle(xvect, yvect));
}

static FORCE_INLINE CONSTEXPR uint32_t uhypsq(int32_t const dx, int32_t const dy)
{
    return (uint32_t)dx*dx + (uint32_t)dy*dy;
}

static FORCE_INLINE int32_t logapproach(int32_t const val, int32_t const targetval)
{
    int32_t const dif = targetval - val;
    return (dif>>1) ? val + (dif>>1) : targetval;
}

void rotatepoint(vec2_t const pivot, vec2_t p, int16_t const daang, vec2_t * const p2) ATTRIBUTE((nonnull(4)));

static inline void rotatevec(vec2_t p, int16_t const daang, vec2_t * const p2)
{
    int const dacos = sintable[(daang+2560)&2047];
    int const dasin = sintable[(daang+2048)&2047];
    *p2 = { dmulscale14(p.x, dacos, -p.y, dasin), dmulscale14(p.y, dacos, p.x, dasin) };
}


int32_t   lastwall(int16_t point);
int32_t   nextsectorneighborz(int16_t sectnum, int32_t refz, int16_t topbottom, int16_t direction);

int32_t   getceilzofslopeptr(usectorptr_t sec, int32_t dax, int32_t day) ATTRIBUTE((nonnull(1)));
int32_t   getflorzofslopeptr(usectorptr_t sec, int32_t dax, int32_t day) ATTRIBUTE((nonnull(1)));
void   getzsofslopeptr(usectorptr_t sec, int32_t dax, int32_t day,
                       int32_t *ceilz, int32_t *florz) ATTRIBUTE((nonnull(1,4,5)));
void yax_getzsofslope(int sectNum, int playerX, int playerY, int32_t* pCeilZ, int32_t* pFloorZ);

int32_t yax_getceilzofslope(int const sectnum, vec2_t const vect);
int32_t yax_getflorzofslope(int const sectnum, vec2_t const vect);

static FORCE_INLINE int32_t getceilzofslope(int16_t sectnum, int32_t dax, int32_t day)
{
    return getceilzofslopeptr((usectorptr_t)&sector[sectnum], dax, day);
}

static FORCE_INLINE int32_t getflorzofslope(int16_t sectnum, int32_t dax, int32_t day)
{
    return getflorzofslopeptr((usectorptr_t)&sector[sectnum], dax, day);
}

static FORCE_INLINE void getzsofslope(int16_t sectnum, int32_t dax, int32_t day, int32_t *ceilz, int32_t *florz)
{
    getzsofslopeptr((usectorptr_t)&sector[sectnum], dax, day, ceilz, florz);
}

static FORCE_INLINE void getcorrectzsofslope(int16_t sectnum, int32_t dax, int32_t day, int32_t *ceilz, int32_t *florz)
{
    vec2_t closest = { dax, day };
    getsectordist(closest, sectnum, &closest);
    getzsofslopeptr((usectorptr_t)&sector[sectnum], closest.x, closest.y, ceilz, florz);
}

static FORCE_INLINE int32_t getcorrectceilzofslope(int16_t sectnum, int32_t dax, int32_t day)
{
    vec2_t closest = { dax, day };
    getsectordist(closest, sectnum, &closest);
    return getceilzofslopeptr((usectorptr_t)&sector[sectnum], closest.x, closest.y);
}

static FORCE_INLINE int32_t getcorrectflorzofslope(int16_t sectnum, int32_t dax, int32_t day)
{
    vec2_t closest = { dax, day };
    getsectordist(closest, sectnum, &closest);
    return getflorzofslopeptr((usectorptr_t)&sector[sectnum], closest.x, closest.y);
}

// Is <wal> a red wall in a safe fashion, i.e. only if consistency invariant
// ".nextsector >= 0 iff .nextwall >= 0" holds.
static FORCE_INLINE CONSTEXPR int32_t redwallp(uwallptr_t wal)
{
    return (wal->nextwall >= 0 && wal->nextsector >= 0);
}

static FORCE_INLINE CONSTEXPR int32_t E_SpriteIsValid(const int32_t i)
{
    return ((unsigned)i < MAXSPRITES && sprite[i].statnum != MAXSTATUS);
}

int clipshape_idx_for_sprite(uspriteptr_t curspr, int curidx);

void   alignceilslope(int16_t dasect, int32_t x, int32_t y, int32_t z);
void   alignflorslope(int16_t dasect, int32_t x, int32_t y, int32_t z);
int32_t sectorofwall(int16_t wallNum);
int32_t sectorofwall_noquick(int16_t wallNum);
int32_t wallength(int16_t wallNum);
int32_t   loopnumofsector(int16_t sectnum, int16_t wallnum);
void setslope(int32_t sectnum, int32_t cf, int16_t slope);

int32_t lintersect(int32_t originX, int32_t originY, int32_t originZ,
                   int32_t destX, int32_t destY, int32_t destZ,
                   int32_t lineStartX, int32_t lineStartY, int32_t lineEndX, int32_t lineEndY,
                   int32_t *intersectionX, int32_t *intersectionY, int32_t *intersectionZ);

int32_t rayintersect(int32_t x1, int32_t y1, int32_t z1, int32_t vx, int32_t vy, int32_t vz, int32_t x3,
                     int32_t y3, int32_t x4, int32_t y4, int32_t *intx, int32_t *inty, int32_t *intz);
#if !defined NETCODE_DISABLE
void do_insertsprite_at_headofstat(int16_t spritenum, int16_t statnum);
int32_t insertspritestat(int16_t statnum);
void do_deletespritestat(int16_t deleteme);
void do_insertsprite_at_headofsect(int16_t spritenum, int16_t sectnum);
void do_deletespritesect(int16_t deleteme);
#endif
int32_t insertsprite(int16_t sectnum, int16_t statnum);
int32_t deletesprite(int16_t spritenum);

int32_t   changespritesect(int16_t spritenum, int16_t newsectnum);
int32_t   changespritestat(int16_t spritenum, int16_t newstatnum);
int32_t   setsprite(int16_t spritenum, const vec3_t *) ATTRIBUTE((nonnull(2)));
int32_t   setspritez(int16_t spritenum, const vec3_t *) ATTRIBUTE((nonnull(2)));

int32_t spriteheightofsptr(uspriteptr_t spr, int32_t *height, int32_t alsotileyofs);
static FORCE_INLINE int32_t spriteheightofs(int16_t i, int32_t *height, int32_t alsotileyofs)
{
    return spriteheightofsptr((uspriteptr_t)&sprite[i], height, alsotileyofs);
}

struct OutputFileCounter {
    uint16_t count = 0;
    buildvfs_FILE opennextfile(char *, char *);
    buildvfs_FILE opennextfile_withext(char *, const char *);
};

// PLAG: line utility functions
typedef struct s_equation
{
    float a, b, c;
} _equation;

int32_t wallvisible(int32_t const x, int32_t const y, int16_t const wallnum);

#define STATUS2DSIZ 144
#define STATUS2DSIZ2 26

//void   qsetmode640350(void);
//void   qsetmode640480(void);
void videoSet2dMode(char davidoption, int32_t daupscaledxdim, int32_t daupscaledydim, int32_t daupscalefactor = 1);
void   clear2dscreen(void);
void   editorDraw2dGrid(int32_t posxe, int32_t posye, int32_t posze, int16_t cursectnum,
                  int16_t ange, int32_t zoome, int16_t gride);
void   editorDraw2dScreen(const vec3_t *pos, int16_t cursectnum,
                    int16_t ange, int32_t zoome, int16_t gride) ATTRIBUTE((nonnull(1)));
int32_t   editorDraw2dLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int col);
void   editorDraw2dCircle(int32_t x1, int32_t y1, int32_t r, int32_t eccen, char col);

int32_t   videoSetRenderMode(int32_t renderer);

#ifdef USE_OPENGL
void    renderSetRollAngle(int32_t rolla);
#endif

//  pal: pass -1 to invalidate all palettes for the tile, or >=0 for a particular palette
//  how: pass -1 to invalidate all instances of the tile in texture memory, or a bitfield
//         bit 0: opaque or masked (non-translucent) texture, using repeating
//         bit 1: ignored
//         bit 2: 33% translucence, using repeating
//         bit 3: 67% translucence, using repeating
//         bit 4: opaque or masked (non-translucent) texture, using clamping
//         bit 5: ignored
//         bit 6: 33% translucence, using clamping
//         bit 7: 67% translucence, using clamping
//       clamping is for sprites, repeating is for walls
void tileInvalidate(int16_t tilenume, int32_t pal, int32_t how);

void polymostSet2dView(void);   // sets up GL for 2D drawing

int32_t polymost_drawtilescreen(int32_t tilex, int32_t tiley, int32_t tilenum, int32_t dimen, int32_t tilezoom,
                                int32_t usehitile, uint8_t *loadedhitile);
void polymost_glreset(void);
void polymost_precache(int32_t dapicnum, int32_t dapalnum, int32_t datype);

enum cutsceneflags {
    CUTSCENE_FORCEFILTER = 1,
    CUTSCENE_FORCENOFILTER = 2,
    CUTSCENE_TEXTUREFILTER = 4,
    CUTSCENE_FILTERMASK = CUTSCENE_FORCEFILTER | CUTSCENE_FORCENOFILTER | CUTSCENE_TEXTUREFILTER,
};

extern int32_t benchmarkScreenshot;

#ifdef USE_OPENGL
extern int32_t glanisotropy;
extern int32_t glusetexcompr;
extern int32_t gltexfiltermode;

enum {
    TEXFILTER_OFF = 0, // GL_NEAREST
    TEXFILTER_ON = 5, // GL_LINEAR_MIPMAP_LINEAR
};

extern int32_t glusetexcache, glusememcache;
extern int32_t glprojectionhacks;
extern int32_t gltexmaxsize;
void gltexapplyprops (void);
void texcache_invalidate(void);

extern int32_t mdtims, omdtims;
extern int32_t glrendmode;

extern int32_t r_rortexture;
extern int32_t r_rortexturerange;
extern int32_t r_rorphase;
#endif

int32_t Ptile2tile(int32_t tile, int32_t palette) ATTRIBUTE((pure));
int32_t md_loadmodel(const char *fn);
int32_t md_setmisc(int32_t modelid, float scale, int32_t shadeoff, float zadd, float yoffset, int32_t flags);
// int32_t md_tilehasmodel(int32_t tilenume, int32_t pal);

extern const char *G_DefaultDefFile(void);
extern const char *G_DefFile(void);
extern char *g_defNamePtr;

extern GrowArray<char *> g_defModules;

#ifdef HAVE_CLIPSHAPE_FEATURE
extern GrowArray<char *> g_clipMapFiles;
#endif

EXTERN int32_t nextvoxid;
EXTERN intptr_t voxoff[MAXVOXELS][MAXVOXMIPS]; // used in KenBuild

#ifdef USE_OPENGL
// TODO: dynamically allocate this

typedef struct { vec3f_t add; int16_t angadd, flags, fov; } hudtyp;

typedef struct
{
    // maps build tiles to particular animation frames of a model
    int16_t     modelid;
    int16_t     framenum;   // calculate the number from the name when declaring
    int16_t     nexttile;
    uint16_t    smoothduration;
    hudtyp      *hudmem[2];
    int8_t      skinnum;
    char        pal;
} tile2model_t;

#define EXTRATILES ((INT16_MAX-255)-MAXTILES)

EXTERN int32_t mdinited;
EXTERN tile2model_t tile2model[MAXTILES+EXTRATILES];

static FORCE_INLINE int32_t md_tilehasmodel(int32_t const tilenume, int32_t const pal)
{
    return mdinited ? tile2model[Ptile2tile(tilenume,pal)].modelid : -1;
}
#endif  // defined USE_OPENGL

static FORCE_INLINE int tilehasmodelorvoxel(int const tilenume, int pal)
{
    UNREFERENCED_PARAMETER(pal);
    return
#ifdef USE_OPENGL
    (videoGetRenderMode() >= REND_POLYMOST && mdinited && usemodels && tile2model[Ptile2tile(tilenume, pal)].modelid != -1) ||
#endif
    (videoGetRenderMode() <= REND_POLYMOST && usevoxels && tiletovox[tilenume] != -1);
}

int32_t md_defineframe(int32_t modelid, const char *framename, int32_t tilenume,
                       int32_t skinnum, float smoothduration, int32_t pal);
int32_t md_defineanimation(int32_t modelid, const char *framestart, const char *frameend,
                           int32_t fps, int32_t flags);
int32_t md_defineskin(int32_t modelid, const char *skinfn, int32_t palnum, int32_t skinnum,
                      int32_t surfnum, float param, float specpower, float specfactor, int32_t flags);
int32_t md_definehud (int32_t modelid, int32_t tilex, vec3f_t add,
                      int32_t angadd, int32_t flags, int32_t fov);
int32_t md_undefinetile(int32_t tile);
int32_t md_undefinemodel(int32_t modelid);

int32_t loaddefinitionsfile(const char *fn);

// if loadboard() fails with -2 return, try loadoldboard(). if it fails with
// -2, board is dodgy
int32_t engineLoadBoardV5V6(const char *filename, char fromwhere, vec3_t *dapos, int16_t *daang, int16_t *dacursectnum);

#ifdef __cplusplus
}
#endif

#include "hash.h"

#ifdef POLYMER
# include "polymer.h"
#endif
#ifdef USE_OPENGL
# include "polymost.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

static FORCE_INLINE void renderDisableFog(void)
{
#ifdef USE_OPENGL
    if (videoGetRenderMode() >= REND_POLYMOST)
    {
        polymost_setFogEnabled(false);
    }
#endif
}

static FORCE_INLINE void renderEnableFog(void)
{
#ifdef USE_OPENGL
    if (videoGetRenderMode() >= REND_POLYMOST && !nofog)
        polymost_setFogEnabled(true);
#endif
}

/* Different "is inside" predicates.
 * NOTE: The redundant bound checks are expected to be optimized away in the
 * inlined code. */

static FORCE_INLINE CONSTEXPR int inside_p(int32_t const x, int32_t const y, int const sectnum)
{
    return ((unsigned)sectnum < MAXSECTORS && inside(x, y, sectnum) == 1);
}

static FORCE_INLINE CONSTEXPR int inside_exclude_p(int32_t const x, int32_t const y, int const sectnum, const uint8_t *excludesectbitmap)
{
    return ((unsigned)sectnum < MAXSECTORS && !bitmap_test(excludesectbitmap, sectnum) && inside_p(x, y, sectnum));
}

/* NOTE: no bound check for inside_z_p */
static FORCE_INLINE int inside_z_p(int32_t const x, int32_t const y, int32_t const z, int const sectnum)
{
    int32_t cz, fz;
    getzsofslope(sectnum, x, y, &cz, &fz);
    return (z >= cz && z <= fz && inside_p(x, y, sectnum));
}

static FORCE_INLINE int inside_exclude_z_p(int32_t const x, int32_t const y, int32_t const z, int const sectnum, const uint8_t *excludesectbitmap)
{
    int32_t cz, fz;
    getzsofslope(sectnum, x, y, &cz, &fz);
    return (z >= cz && z <= fz && inside_exclude_p(x, y, sectnum, excludesectbitmap));
}

#define SET_AND_RETURN(Lval, Rval) \
    do                             \
    {                              \
        (Lval) = (Rval);           \
        return;                    \
    } while (0)

static FORCE_INLINE int64_t compat_maybe_truncate_to_int32(int64_t val)
{
    return enginecompatibilitymode != ENGINE_EDUKE32 ? (int32_t)val : val;
}

static inline int32_t clipmove_old(int32_t *x, int32_t *y, int32_t *z, int16_t *sectnum, int32_t xvect, int32_t yvect, int32_t walldist,
                   int32_t ceildist, int32_t flordist, uint32_t cliptype) ATTRIBUTE((nonnull(1,2,3,4)));

static inline int32_t clipmove_old(int32_t *x, int32_t *y, int32_t *z, int16_t *sectnum, int32_t xvect, int32_t yvect, int32_t walldist,
                   int32_t ceildist, int32_t flordist, uint32_t cliptype)
{
    vec3_t vector = { *x, *y, *z };

    int32_t result = clipmove(&vector, sectnum, xvect, yvect, walldist, ceildist, flordist, cliptype);

    *x = vector.x;
    *y = vector.y;
    *z = vector.z;

    return result;
}

static inline int32_t pushmove_old(int32_t *x, int32_t *y, int32_t *z, int16_t *sectnum, int32_t walldist,
                   int32_t ceildist, int32_t flordist, uint32_t cliptype) ATTRIBUTE((nonnull(1,2,3,4)));

static inline int32_t pushmove_old(int32_t *x, int32_t *y, int32_t *z, int16_t *sectnum, int32_t walldist,
                   int32_t ceildist, int32_t flordist, uint32_t cliptype)
{
    vec3_t vector = { *x, *y, *z };

    int32_t result = pushmove(&vector, sectnum, walldist, ceildist, flordist, cliptype);

    *x = vector.x;
    *y = vector.y;
    *z = vector.z;

    return result;
}

static inline void getzrange_old(int32_t x, int32_t y, int32_t z, int16_t sectnum, int32_t *ceilz, int32_t *ceilhit, int32_t *florz,
                 int32_t *florhit, int32_t walldist, uint32_t cliptype) ATTRIBUTE((nonnull(5,6,7,8)));

static inline void getzrange_old(int32_t x, int32_t y, int32_t z, int16_t sectnum, int32_t *ceilz, int32_t *ceilhit, int32_t *florz,
                 int32_t *florhit, int32_t walldist, uint32_t cliptype)
{
    const vec3_t vector = { x, y, z };
    getzrange(&vector, sectnum, ceilz, ceilhit, florz, florhit, walldist, cliptype);
}

static inline int32_t setspritez_old(int16_t spritenum, int32_t x, int32_t y, int32_t z)
{
    const vec3_t vector = { x, y, z };
    return setspritez(spritenum, &vector);
}

extern int32_t rintersect(int32_t x1, int32_t y1, int32_t z1,
    int32_t vx_, int32_t vy_, int32_t vz,
    int32_t x3, int32_t y3, int32_t x4, int32_t y4,
    int32_t *intx, int32_t *inty, int32_t *intz);

extern int32_t(*animateoffs_replace)(int const tilenum, int fakevar);
extern void(*paletteLoadFromDisk_replace)(void);
extern int32_t(*getpalookup_replace)(int32_t davis, int32_t dashade);
extern void(*initspritelists_replace)(void);
extern int32_t(*insertsprite_replace)(int16_t sectnum, int16_t statnum);
extern int32_t(*deletesprite_replace)(int16_t spritenum);
extern int32_t(*changespritesect_replace)(int16_t spritenum, int16_t newsectnum);
extern int32_t(*changespritestat_replace)(int16_t spritenum, int16_t newstatnum);
extern void(*loadvoxel_replace)(int32_t voxel);
extern int32_t(*loadboard_replace)(const char *filename, char flags, vec3_t *dapos, int16_t *daang, int16_t *dacursectnum);
extern int32_t(*saveboard_replace)(const char *filename, const vec3_t *dapos, int16_t daang, int16_t dacursectnum);
#ifdef USE_OPENGL
extern void(*PolymostProcessVoxels_Callback)(void);
#endif

static inline int16_t tspriteGetSlope(tspriteptr_t const tspr)
{
    if ((tspr->cstat & CSTAT_SPRITE_ALIGNMENT) != CSTAT_SPRITE_ALIGNMENT_SLOPE)
        return 0;
    return uint8_t(tspr->xoffset) + (uint8_t(tspr->yoffset) << 8);
}

static inline int32_t spriteGetZOfSlope(uint16_t const spritenum, vec2_t pos)
{
    auto const spr = (uspriteptr_t)&sprite[spritenum];
    int16_t const heinum = spriteGetSlope(spritenum);
    if (heinum == 0)
        return spr->z;

    int const j = dmulscale4(sintable[(spr->ang+1024)&2047], pos.y-spr->y,
                            -sintable[(spr->ang+512)&2047], pos.x-spr->x);
    return spr->z + mulscale18(heinum,j);
}

static inline int32_t tspriteGetZOfSlope(tspriteptr_t const tspr, int32_t dax, int32_t day)
{
    int16_t const heinum = tspriteGetSlope(tspr);
    if (heinum == 0)
        return tspr->z;

    int const j = dmulscale4(sintable[(tspr->ang+1024)&2047], day-tspr->y,
                            -sintable[(tspr->ang+512)&2047], dax-tspr->x);
    return tspr->z + mulscale18(heinum,j);
}

static inline float tspriteGetZOfSlopeFloat(tspriteptr_t const tspr, float dax, float day)
{
    int16_t const heinum = tspriteGetSlope(tspr);
    if (heinum == 0)
        return float(tspr->z);

    float const f = sintable[(tspr->ang+1024)&2047] * (day-tspr->y) - sintable[(tspr->ang+512)&2047] * (dax-tspr->x);
    return float(tspr->z) + heinum * f * (1.f/4194304.f);
}

#ifdef __cplusplus
}
#endif

#endif // build_h_
