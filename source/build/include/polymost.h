#ifndef polymost_h_
# define polymost_h_

#ifdef USE_OPENGL

#include "baselayer.h"  // glinfo
#include "glad/glad.h"
#include "hightile.h"
#include "rhi.h"

extern int32_t rendmode;
extern float gtang;
extern int polymost2d;
extern double gxyaspect;
extern float grhalfxdown10x;
extern float gcosang, gsinang, gcosang2, gsinang2;
extern float gchang, gshang, gctang, gstang, gvisibility;
extern float gvrcorrection;

struct glfiltermodes {
    const char *name;
    int32_t min,mag;
};
#define NUMGLFILTERMODES 6
extern struct glfiltermodes glfiltermodes[NUMGLFILTERMODES];

extern void Polymost_prepare_loadboard(void);


//void phex(char v, char *s);
void polymost_setuptexture(const int32_t dameth, int filter);
int calc_texture_mipcount(vec2_t siz, int32_t dameth, vec2_t& texsiz);
void uploadtexture(rhiTexture rhitex, vec2_t siz, int32_t texfmt, coltype *pic, vec2_t tsiz, int32_t dameth);
void uploadtextureindexed(rhiTexture rhitex, vec2_t offset, vec2_t siz, intptr_t tile);
void uploadbasepalette(int32_t basepalnum);
void uploadpalswap(int32_t palookupnum);
void polymost_drawsprite(int32_t snum);
void polymost_drawmaskwall(int32_t damaskwallcnt);
void polymost_dorotatesprite(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum,
                             int8_t dashade, char dapalnum, int32_t dastat, uint8_t daalpha, uint8_t dablend, int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2, int32_t uniqid);
void polymost_fillpolygon(int32_t npoints);
void polymost_initosdfuncs(void);
void polymost_drawrooms(void);
void polymost_prepareMirror(int32_t dax, int32_t day, int32_t daz, fix16_t daang, fix16_t dahoriz, int16_t mirrorWall);
void polymost_completeMirror();
void polymost_startBufferedDrawing(int nn);
void polymost_bufferVert(vec3f_t const v, vec2f_t const t);
void polymost_finishBufferedDrawing(int mode);

int32_t polymost_maskWallHasTranslucency(uwalltype const * const wall);
int32_t polymost_spriteHasTranslucency(tspritetype const * const tspr);
int32_t polymost_spriteIsModelOrVoxel(tspritetype const * const tspr);

void polymost_disableProgram(void);
char polymost_getClamp(void);
void polymost_resetState(void);
void polymost_resetProgram(void);
void polymost_resetVertexPointers(void);
void polymost_setClamp(char clamp);
void polymost_setFogEnabled(char fogEnabled);
void polymost_setHalfTexelSize(vec2f_t const &halfTexelSize);
void polymost_setTexturePosSize(vec4f_t const &texturePosSize);
void polymost_setVisibility(float visibility);
void polymost_updatePalette(void);
void polymost_useColorOnly(char useColorOnly);
void polymost_useDetailMapping(char useDetailMapping);
void polymost_useGlowMapping(char useGlowMapping);
void polymost_usePaletteIndexing(char usePaletteIndexing);
void polymost_setColorCorrection(vec4f_t const& colorCorrection);

extern vec4f_t g_glColorCorrection;

GLuint polymost2_compileShader(GLenum shaderType, const char* const source, int* pLength = nullptr);

float* multiplyMatrix4f(float m0[4*4], const float m1[4*4]);

void polymost_initdrawpoly(void);
void polymost_glinit(void);
void polymost_glreset(void);

void polymost_init(void);

enum {
    INVALIDATE_ALL,
    INVALIDATE_ART,
    INVALIDATE_ALL_NON_INDEXED,
    INVALIDATE_ART_NON_INDEXED
};

void gltexinvalidate(int32_t dapicnum, int32_t dapalnum, int32_t dameth);
void gltexinvalidatetype(int32_t type);
int32_t polymost_printtext256(int32_t xpos, int32_t ypos, int16_t col, int16_t backcol, const char *name, char fontsize);

extern float curpolygonoffset;

extern float shadescale;
extern int32_t shadescale_unbounded;
extern uint8_t alphahackarray[MAXTILES];

#ifdef POLYMOST2
extern int32_t r_enablepolymost2;
#endif // POLYMOST2

#ifdef USE_GLEXT
extern int32_t r_vbocount;
extern int32_t r_glowmapping;
extern int32_t r_detailmapping;
#endif

extern int32_t r_animsmoothing;
extern int32_t r_downsize;
extern int32_t r_downsizevar;
extern int32_t r_drawpolyVertsBufferLength;
extern int32_t r_flatsky;
extern int32_t r_fullbrights;
extern int32_t r_npotwallmode;
extern int32_t r_parallaxskyclamping;
extern int32_t r_parallaxskypanning;
extern int32_t r_polygonmode;
extern int32_t r_polymostDebug;
extern int32_t r_shadeinterpolate;
extern int32_t r_skyzbufferhack;
extern int32_t r_useindexedcolortextures;
extern int32_t r_usenewshading;
extern int32_t r_usesamplerobjects;
extern int32_t r_usetileshades;
extern int32_t r_vertexarrays;
extern int32_t r_yshearing;
extern int32_t r_persistentStreamBuffer;

extern int32_t r_brightnesshack;

extern int32_t polymostcenterhoriz;
extern GLuint drawpolyVertsID;

enum tileshademodes
{
    TS_NONE,
    TS_SHADETABLE,
    TS_TEXTURE
};

extern float fogfactor[MAXPALOOKUPS];

// Compare with polymer_eligible_for_artmap()
static FORCE_INLINE int32_t eligible_for_tileshades(int32_t const picnum, int32_t const pal)
{
    return !usehightile || !hicfindsubst(picnum, pal, hictinting[pal].f & HICTINT_ALWAYSUSEART);
}

static FORCE_INLINE int polymost_useindexedtextures(void)
{
    return videoGetRenderMode() == REND_POLYMOST && r_useindexedcolortextures && gltexfiltermode == 0 && !duke64;
}

static FORCE_INLINE int polymost_usetileshades(void)
{
    int const idx = polymost_useindexedtextures();
    int const shd = idx || (r_usetileshades && !(globalflags & GLOBAL_NO_GL_TILESHADES));
    return idx && shd ? TS_SHADETABLE : shd ? TS_TEXTURE : TS_NONE;
}

static FORCE_INLINE float polymost_getanisotropy(int filtered = 0)
{
    return filtered == 0 && polymost_useindexedtextures() ? 1.f : clamp<float>(glanisotropy, 1.f, glinfo.maxanisotropy);
}

static inline float getshadefactor(int32_t const shade, int32_t const pal)
{
    // 8-bit tiles, i.e. non-hightiles and non-models, don't get additional
    // glColor() shading with r_usetileshades!
    if (videoGetRenderMode() == REND_POLYMOST && polymost_usetileshades() != TS_NONE && eligible_for_tileshades(globalpicnum, globalpal))
        return 1.f;

    float const fshade = fogfactor[pal] != 0.f ? (float)shade / fogfactor[pal] : 0.f;

    if (r_usenewshading == 4)
        return max(min(1.f - (fshade * shadescale / frealmaxshade), 1.f), 0.f);

    float const shadebound = (float)((shadescale_unbounded || shade>=numshades) ? numshades : numshades-1);
    float const scaled_shade = fshade*shadescale;
    float const clamped_shade = min(max(scaled_shade, 0.f), shadebound);

    return ((float)(numshades-clamped_shade))/(float)numshades;
}

#define POLYMOST_CHOOSE_FOG_PAL(fogpal, pal) \
    ((fogpal) ? (fogpal) : (pal))
static FORCE_INLINE int32_t get_floor_fogpal(usectorptr_t const sec)
{
    return POLYMOST_CHOOSE_FOG_PAL(sec->fogpal, sec->floorpal);
}
static FORCE_INLINE int32_t get_ceiling_fogpal(usectorptr_t const sec)
{
    return POLYMOST_CHOOSE_FOG_PAL(sec->fogpal, sec->ceilingpal);
}
static FORCE_INLINE int32_t fogshade(int32_t const shade, int32_t const pal)
{
    polytintflags_t const tintflags = hictinting[pal].f;
    return (globalflags & GLOBAL_NO_GL_FOGSHADE || tintflags & HICTINT_NOFOGSHADE) ? 0 : shade;
}

static FORCE_INLINE int check_nonpow2(int32_t const x)
{
    return (x > 1 && (x&(x-1)));
}

// Are we using the mode that uploads non-power-of-two wall textures like they
// render in classic?
static FORCE_INLINE int polymost_is_npotmode(void)
{
    // The glinfo.texnpot check is so we don't have to deal with that case in
    // gloadtile_art().
    return glinfo.texnpot &&
        // r_npotwallmode is NYI for hightiles. We require r_hightile off
        // because in calc_ypanning(), the repeat would be multiplied by a
        // factor even if no modified texture were loaded.
        !usehightile &&
#ifdef NEW_MAP_FORMAT
        g_loadedMapVersion < 10 &&
#endif
        r_npotwallmode == 1;
}

// Flags of the <dameth> argument of various functions
enum {
    DAMETH_NOMASK = 0,
    DAMETH_MASK = 1,
    DAMETH_TRANS1 = 2,
    DAMETH_TRANS2 = 3,

    DAMETH_MASKPROPS = 3,

    DAMETH_CLAMPED = 4,

    DAMETH_WALL = 32,  // signals a texture for a wall (for r_npotwallmode)

    DAMETH_INDEXED = 512,

    DAMETH_N64 = 1024,
    DAMETH_N64_INTENSIVITY = 2048,
    DAMETH_N64_SCALED = 2097152,

    // used internally by polymost_domost
    DAMETH_BACKFACECULL = -1,

    // used internally by uploadtexture
    DAMETH_NODOWNSIZE = 4096,
    DAMETH_HI = 8192,
    DAMETH_NOFIX = 16384,
    DAMETH_NOTEXCOMPRESS = 32768,
    DAMETH_HASALPHA = 65536,
    DAMETH_ONEBITALPHA = 131072,
    DAMETH_ARTIMMUNITY = 262144,

    DAMETH_HASFULLBRIGHT = 524288,
    DAMETH_NPOTWALL = 1048576,

    DAMETH_UPLOADTEXTURE_MASK =
        DAMETH_HI |
        DAMETH_NODOWNSIZE |
        DAMETH_NOFIX |
        DAMETH_NOTEXCOMPRESS |
        DAMETH_HASALPHA |
        DAMETH_ONEBITALPHA |
        DAMETH_ARTIMMUNITY |
        DAMETH_HASFULLBRIGHT |
        DAMETH_NPOTWALL,
};

#define DAMETH_NARROW_MASKPROPS(dameth) (((dameth)&(~DAMETH_TRANS1))|(((dameth)&DAMETH_TRANS1)>>1))
EDUKE32_STATIC_ASSERT(DAMETH_NARROW_MASKPROPS(DAMETH_MASKPROPS) == DAMETH_MASK);
EDUKE32_STATIC_ASSERT(DAMETH_NARROW_MASKPROPS(DAMETH_CLAMPED) == DAMETH_CLAMPED);

#define TO_DAMETH_NODOWNSIZE(hicr_flags) (((hicr_flags)&HICR_NODOWNSIZE)<<8)
EDUKE32_STATIC_ASSERT(TO_DAMETH_NODOWNSIZE(HICR_NODOWNSIZE) == DAMETH_NODOWNSIZE);
#define TO_DAMETH_NOTEXCOMPRESS(hicr_flags) (((hicr_flags)&HICR_NOTEXCOMPRESS)<<15)
EDUKE32_STATIC_ASSERT(TO_DAMETH_NOTEXCOMPRESS(HICR_NOTEXCOMPRESS) == DAMETH_NOTEXCOMPRESS);
#define TO_DAMETH_ARTIMMUNITY(hicr_flags) (((hicr_flags)&HICR_ARTIMMUNITY)<<13)
EDUKE32_STATIC_ASSERT(TO_DAMETH_ARTIMMUNITY(HICR_ARTIMMUNITY) == DAMETH_ARTIMMUNITY);
#define TO_DAMETH_INDEXED(hicr_flags) (((hicr_flags)&HICR_INDEXED)<<3)
EDUKE32_STATIC_ASSERT(TO_DAMETH_INDEXED(HICR_INDEXED) == DAMETH_INDEXED);

// Do we want a NPOT-y-as-classic texture for this <dameth> and <ysiz>?
static FORCE_INLINE int polymost_want_npotytex(int32_t dameth, int32_t ysiz)
{
    return videoGetRenderMode() != REND_POLYMER &&  // r_npotwallmode NYI in Polymer
        polymost_is_npotmode() && (dameth&DAMETH_WALL) && check_nonpow2(ysiz);
}

// pthtyp pth->flags bits
enum pthtyp_flags {
    PTH_CLAMPED = 1,
    PTH_HIGHTILE = 2,
    PTH_SKYBOX = 4,
    PTH_HASALPHA = 8,
    PTH_HASFULLBRIGHT = 16,
    PTH_NPOTWALL = DAMETH_WALL,  // r_npotwallmode=1 generated texture
    PTH_FORCEFILTER = 64,

    PTH_INVALIDATED = 128,

    PTH_NOTRANSFIX = 256, // fixtransparency() bypassed

    PTH_INDEXED = 512,
    PTH_ONEBITALPHA = 1024,

    PTH_N64 = 2048,
    PTH_N64_INTENSIVITY = 4096,
    PTH_N64_SCALED = 8192,

    // temporary until I create separate flags for samplers
    PTH_DEPTH_SAMPLER = 16384,
    PTH_TEMP_SKY_HACK = 32768
};

typedef struct pthtyp_t
{
    struct pthtyp_t *next;
    struct pthtyp_t *ofb; // fullbright pixels
    hicreplctyp     *hicr;

    rhiTexture      rhipic;
    vec2f_t         scale;
    vec2_t          siz;
    int16_t         picnum;

    uint16_t        flags; // see pthtyp_flags
    polytintflags_t effects;
    char            palnum;
    char            shade;
    char            skyface;
} pthtyp;

// DAMETH -> PTH conversions
#define TO_PTH_CLAMPED(dameth) (((dameth)&DAMETH_CLAMPED)>>2)
EDUKE32_STATIC_ASSERT(TO_PTH_CLAMPED(DAMETH_CLAMPED) == PTH_CLAMPED);
#define TO_PTH_NOTRANSFIX(dameth) ((((~(dameth))&DAMETH_MASK)<<8)&(((~(dameth))&DAMETH_TRANS1)<<7))
EDUKE32_STATIC_ASSERT(TO_PTH_NOTRANSFIX(DAMETH_NOMASK) == PTH_NOTRANSFIX);
EDUKE32_STATIC_ASSERT(TO_PTH_NOTRANSFIX(DAMETH_MASK) == 0);
EDUKE32_STATIC_ASSERT(TO_PTH_NOTRANSFIX(DAMETH_TRANS1) == 0);
EDUKE32_STATIC_ASSERT(TO_PTH_NOTRANSFIX(DAMETH_MASKPROPS) == 0);
#define TO_PTH_INDEXED(dameth) ((dameth)&DAMETH_INDEXED)
EDUKE32_STATIC_ASSERT(TO_PTH_INDEXED(DAMETH_INDEXED) == PTH_INDEXED);
#define TO_PTH_N64(dameth) (((dameth)&DAMETH_N64)<<1)
EDUKE32_STATIC_ASSERT(TO_PTH_N64(DAMETH_N64) == PTH_N64);
#define TO_PTH_N64_INTENSIVITY(dameth) (((dameth)&DAMETH_N64_INTENSIVITY)<<1)
EDUKE32_STATIC_ASSERT(TO_PTH_N64_INTENSIVITY(DAMETH_N64_INTENSIVITY) == PTH_N64_INTENSIVITY);
#define TO_PTH_N64_SCALED(dameth) (((dameth)&DAMETH_N64_SCALED)>>8)
EDUKE32_STATIC_ASSERT(TO_PTH_N64_SCALED(DAMETH_N64_SCALED) == PTH_N64_SCALED);

extern void gloadtile_art(int32_t,int32_t,int32_t,int32_t,int32_t,pthtyp *);
extern int32_t gloadtile_hi(int32_t,int32_t,int32_t,hicreplctyp *,int32_t,pthtyp *,polytintflags_t);
extern coltype *gloadtruecolortile_mdloadskin_shared(char* fn, int32_t picfillen, vec2_t * tsiz, vec2_t * siz, char * onebitalpha, polytintflags_t effect, int32_t dapalnum, char* al);
extern int32_t drawingskybox;
extern int32_t hicprecaching;
extern float fcosglobalang, fsinglobalang;
extern float fxdim, fydim, fydimen, fviewingrange;
extern float fsearchx, fsearchy;

extern char ptempbuf[MAXWALLSB<<1];

extern hitdata_t polymost_hitdata;

typedef struct
{
    uint32_t rev;
    vec2f_t fsin;
    int16_t wall;
    int8_t dist;
    int8_t invalid;
} wallspriteinfo_t;

// needs to be this high for XXX-Stacy because the bad sprite in that one isn't even properly ornamented onto the wall!
#define MAXINTERSECTIONANGDIFF (4.5f)
#define MAXINTERSECTIONLINEDIST 1

extern wallspriteinfo_t ornament[MAXSPRITES];
extern void polymost_checkornamentedsprite(tspriteptr_t tspr, int16_t *wallnum, wallspriteinfo_t* ws);
extern int32_t polymost_findintersectingwall(tspritetype const &tspr);
extern int32_t polymost_lintersect(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, int32_t x4, int32_t y4);

static FORCE_INLINE bool polymost_testintersection(vec3_t const &pos, vec2_t const &v, int16_t wallnum)
{
    return ((pos.x - v.x) + (pos.x + v.x)) == (wall[wallnum].x + POINT2(wallnum).x)
        || ((pos.y - v.y) + (pos.y + v.y)) == (wall[wallnum].y + POINT2(wallnum).y)
        || polymost_lintersect(pos.x - v.x, pos.y - v.y, pos.x + v.x, pos.y + v.y, wall[wallnum].x, wall[wallnum].y, POINT2(wallnum).x, POINT2(wallnum).y);
}

extern void polymost_setupglowtexture(const int32_t texunit, const rhiTexture rhipic, const int32_t flags);
extern void polymost_setupdetailtexture(const int32_t texunit, const rhiTexture rhipic, const int32_t flags);

#endif

#endif
