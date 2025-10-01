// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)
// by the EDuke32 team (development@voidpoint.com)

#include "baselayer.h"
#include "build.h"
#include "cache1d.h"
#include "compat.h"
#include "crc32.h"
#include "engine_priv.h"
#include "lz4.h"
#include "texcache.h"
#include "vfs.h"

static void *g_vm_data;

// The tile file number (tilesXXX <- this) of each tile:
// 0 <= . < MAXARTFILES_BASE: tile is in a "base" ART file
// MAXARTFILES_BASE <= . < MAXARTFILES_TOTAL: tile is in a map-specific ART file
EDUKE32_STATIC_ASSERT(MAXARTFILES_TOTAL <= 256);

static int32_t tilefileoffs[MAXTILES];

// Backup tilefilenum[] and tilefileoffs[]. These get allocated only when
// necessary (have per-map ART files).
static uint8_t *g_bakTileFileNum;
static int32_t *g_bakTileFileOffs;
static vec2_16_t *g_bakTileSiz;
static picanm_t *g_bakPicAnm;
static char * g_bakFakeTile;
static char ** g_bakFakeTileData;
static rottile_t *g_bakRottile;
// NOTE: picsiz[] is not backed up, but recalculated when necessary.

//static int32_t artsize = 0;
static int32_t g_vm_size = 0;

static char artfilename[BMAX_PATH];
static char artfilenameformat[BMAX_PATH];
static char mapartfilename[BMAX_PATH];  // map-specific ART file name
static int32_t mapartfnXXofs;  // byte offset to 'XX' (the number part) in the above
static int32_t artfilnum, artfilplc;
static buildvfs_kfd artfil;

////////// Per-map ART file loading //////////

// Some forward declarations.
static const char *artGetIndexedFileName(int32_t tilefilei);
static int32_t artReadIndexedFile(int32_t tilefilei);

static inline void artClearMapArtFilename(void)
{
    Bmemset(mapartfilename, 0, sizeof(mapartfilename));
    mapartfnXXofs = 0;
}

static inline void artUpdateManifest(void)
{
    for (bssize_t i=0; i<MAXTILES; i++)
        tileUpdatePicSiz(i);
}

template <typename origar_t, typename bakar_t>
static inline void RESTORE_MAPART_ARRAY(origar_t & origar, bakar_t & bakar)
{
    EDUKE32_STATIC_ASSERT(sizeof(origar[0]) == sizeof(bakar[0]));
    for (size_t i=0; i < ARRAY_SIZE(origar); i++)
        if (tilefilenum[i] >= MAXARTFILES_BASE)
            origar[i] = bakar[i];
    DO_FREE_AND_NULL(bakar);
}

template <typename origar_t, typename bakar_t>
static inline void ALLOC_MAPART_ARRAY(origar_t & origar, bakar_t & bakar)
{
    bakar = (bakar_t) Xmalloc(ARRAY_SIZE(origar) * sizeof(origar[0]));
    Bmemcpy(bakar, origar, ARRAY_SIZE(origar) * sizeof(origar[0]));
}

void artClearMapArt(void)
{
    if (g_bakTileFileNum == NULL)
        return;  // per-map ART N/A

    artClearMapArtFilename();

    if (artfilnum >= MAXARTFILES_BASE)
    {
        kclose(artfil);

        artfil = buildvfs_kfd_invalid;
        artfilnum = -1;
        artfilplc = 0L;
    }

    for (bssize_t i=0; i<MAXTILES; i++)
    {
        if (tilefilenum[i] >= MAXARTFILES_BASE)
        {
            // XXX: OK way to free it? Better: cache1d API. CACHE1D_FREE
            walock[i] = CACHE1D_FREE;
            waloff[i] = 0;
        }
    }

    // Restore original per-tile arrays
    RESTORE_MAPART_ARRAY(tilefileoffs, g_bakTileFileOffs);
    RESTORE_MAPART_ARRAY(tilesiz, g_bakTileSiz);
    RESTORE_MAPART_ARRAY(picanm, g_bakPicAnm);
    RESTORE_MAPART_ARRAY(rottile, g_bakRottile);

    // restore entire faketile array as it can cause problems otherwise
    EDUKE32_STATIC_ASSERT(sizeof(faketile[0]) == sizeof(g_bakFakeTile[0]));
    Bmemcpy(faketile, g_bakFakeTile, ARRAY_SIZE(faketile) * sizeof(faketile[0]));
    DO_FREE_AND_NULL(g_bakFakeTile);

    for (size_t i = 0; i < MAXUSERTILES; ++i)
    {
        if (tilefilenum[i] >= MAXARTFILES_BASE && faketiledata[i] != g_bakFakeTileData[i])
        {
            Xfree(faketiledata[i]);
            faketiledata[i] = g_bakFakeTileData[i];
        }
    }
    DO_FREE_AND_NULL(g_bakFakeTileData);

    // must be restored last
    RESTORE_MAPART_ARRAY(tilefilenum, g_bakTileFileNum);

    artUpdateManifest();
#ifdef USE_OPENGL
    //POGOTODO: review this to ensure we're not invalidating more than we have to
    gltexinvalidatetype(INVALIDATE_ART);
# ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER)
        polymer_texinvalidate();
# endif
#endif
}

void artSetupMapArt(const char *filename)
{
    artClearMapArt();

    if (Bstrlen(filename) + 7 >= sizeof(mapartfilename))
        return;

    Bstrcpy(mapartfilename, filename);
    append_ext_UNSAFE(mapartfilename, "_XX.art");
    mapartfnXXofs = Bstrlen(mapartfilename) - 6;

    // Check for first per-map ART file: if that one doesn't exist, don't load any.
    buildvfs_kfd fil = kopen4loadfrommod(artGetIndexedFileName(MAXARTFILES_BASE), 0);

    if (fil == buildvfs_kfd_invalid)
    {
        artClearMapArtFilename();
        return;
    }

    kclose(fil);

    // Allocate backup arrays.
    ALLOC_MAPART_ARRAY(tilefilenum, g_bakTileFileNum);
    ALLOC_MAPART_ARRAY(tilefileoffs, g_bakTileFileOffs);
    ALLOC_MAPART_ARRAY(tilesiz, g_bakTileSiz);
    ALLOC_MAPART_ARRAY(picanm, g_bakPicAnm);
    ALLOC_MAPART_ARRAY(faketile, g_bakFakeTile);
    ALLOC_MAPART_ARRAY(faketiledata, g_bakFakeTileData);
    ALLOC_MAPART_ARRAY(rottile, g_bakRottile);

    for (bssize_t i=MAXARTFILES_BASE; i<MAXARTFILES_TOTAL; i++)
    {
        int ret = artReadIndexedFile(i);

        if (ret != 0)
        {
            // NOTE: i == MAXARTFILES_BASE && ret == -1 can only have happened
            // if the file was deleted between the above file existence check
            // and now.  Very cornerly... but I like my code to be prepared to
            // any eventuality.
            if (i == MAXARTFILES_BASE || ret != -1)
                artClearMapArt();
            break;
        }
    }

    artUpdateManifest();
#ifdef USE_OPENGL
    //POGOTODO: review this to ensure we're not invalidating more than we have to
    gltexinvalidatetype(INVALIDATE_ART);
# ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER)
        polymer_texinvalidate();
# endif
#endif
}

//
// ART loading
//

void tileSetupDummy(int32_t const tile)
{
    bitmap_set(faketile, tile);
    DO_FREE_AND_NULL(faketiledata[tile]);
}

static void tileSetDataSafe(int32_t const tile, int32_t tsiz, char const * const buffer)
{
    int const compressed_tsiz = LZ4_compressBound(tsiz);
    char * newtile = (char *) Xmalloc(compressed_tsiz);

    if ((tsiz = LZ4_compress_default(buffer, newtile, tsiz, compressed_tsiz)) != -1)
    {
        faketilesize[tile] = tsiz;
        faketiledata[tile] = (char *) Xrealloc(newtile, tsiz);
        bitmap_set(faketile, tile);
        tilefilenum[tile] = MAXARTFILES_TOTAL;
    }
    else
    {
        Xfree(newtile);
    }
}

void tileSetData(int32_t const tile, int32_t tsiz, char const * const buffer)
{
    int const compressed_tsiz = LZ4_compressBound(tsiz);
    faketiledata[tile] = (char *) Xrealloc(faketiledata[tile], compressed_tsiz);

    if ((tsiz = LZ4_compress_default(buffer, faketiledata[tile], tsiz, compressed_tsiz)) != -1)
    {
        faketilesize[tile] = tsiz;
        faketiledata[tile] = (char *) Xrealloc(faketiledata[tile], tsiz);
        bitmap_set(faketile, tile);
        tilefilenum[tile] = MAXARTFILES_TOTAL;
    }
    else
    {
        DO_FREE_AND_NULL(faketiledata[tile]);
        bitmap_clear(faketile, tile);
    }
}

static void tileSoftDelete(int32_t const tile)
{
    tilesiz[tile].x = 0;
    tilesiz[tile].y = 0;
    picsiz[tile] = 0;

    // CACHE1D_FREE
    walock[tile] = CACHE1D_FREE;
    waloff[tile] = 0;

    bitmap_clear(faketile, tile);

    Bmemset(&picanm[tile], 0, sizeof(picanm_t));
}

void tileDelete(int32_t const tile)
{
    tileSoftDelete(tile);

    DO_FREE_AND_NULL(faketiledata[tile]);

    vox_undefine(tile);

#ifdef USE_OPENGL
    for (ssize_t i=MAXPALOOKUPS-1; i>=0; --i)
        hicclearsubst(tile, i);

    md_undefinetile(tile);
#endif
}

void tileUpdatePicSiz(int32_t picnum)
{
    int j = 15;

    while ((j > 1) && (pow2long[j] > tilesiz[picnum].x))
        j--;
    picsiz[picnum] = j;

    j = 15;
    while ((j > 1) && (pow2long[j] > tilesiz[picnum].y))
        j--;
    picsiz[picnum] |= j<<4;
}

void tileSetSize(int32_t picnum, int16_t dasizx, int16_t dasizy)
{
    tilesiz[picnum].x = dasizx;
    tilesiz[picnum].y = dasizy;

    tileUpdatePicSiz(picnum);
}

int32_t artReadHeader(buildvfs_kfd const fil, char const * const fn, artheader_t * const local)
{
    int32_t artversion;
    kread(fil, &artversion, 4); artversion = B_LITTLE32(artversion);

    if (artversion == B_LITTLE32(0x4c495542))
    {
        kread(fil, &artversion, 4); artversion = B_LITTLE32(artversion);
        if (artversion == B_LITTLE32(0x54524144))
        {
            kread(fil, &artversion, 4); artversion = B_LITTLE32(artversion);
        }
        else
        {
            LOG_F(ERROR, "Unable to load %s: bad version.", fn);
            kclose(fil);
            return 1;
        }
    }

    if (artversion != 1)
    {
        LOG_F(ERROR, "Unable to load %s: bad version.", fn);
        kclose(fil);
        return 1;
    }

    int32_t numtiles_dummy;
    kread(fil, &numtiles_dummy, 4);

    kread(fil, &local->tilestart, 4); local->tilestart = B_LITTLE32(local->tilestart);
    kread(fil, &local->tileend, 4);   local->tileend   = B_LITTLE32(local->tileend);

    if (EDUKE32_PREDICT_FALSE((uint32_t) local->tilestart >= MAXUSERTILES || (uint32_t) local->tileend >= MAXUSERTILES || local->tileend < local->tilestart))
    {
        LOG_F(ERROR, "Unable to load %s: tile range is reversed, negative, or exceeds MAXTILES: file likely corrupt.", fn);
        kclose(fil);
        return 1;
    }

    local->numtiles = (local->tileend-local->tilestart+1);

    return 0;
}

int32_t artReadHeaderFromBuffer(uint8_t const * const buf, artheader_t * const local)
{
    int const artversion = B_LITTLE32(B_UNBUF32(&buf[0]));
    if (EDUKE32_PREDICT_FALSE(artversion != 1))
    {
        LOG_F(ERROR, "Unable to load art: bad version.");
        return 1;
    }

    local->tilestart = B_LITTLE32(B_UNBUF32(&buf[8]));
    local->tileend   = B_LITTLE32(B_UNBUF32(&buf[12]));

    if (EDUKE32_PREDICT_FALSE((unsigned) local->tilestart >= MAXUSERTILES || (unsigned) local->tileend >= MAXUSERTILES || local->tileend < local->tilestart))
    {
        LOG_F(ERROR, "Unable to load art: tile range is reversed, negative, or exceeds MAXTILES: data likely corrupt.");
        return 1;
    }

    local->numtiles = (local->tileend-local->tilestart+1);

    return 0;
}

int32_t artCheckUnitFileHeader(uint8_t const * const buf, int32_t length)
{
    if (EDUKE32_PREDICT_FALSE(length <= ARTv1_UNITOFFSET))
        return -1;

    artheader_t local;
    if (EDUKE32_PREDICT_FALSE(artReadHeaderFromBuffer(buf, &local) != 0))
        return -2;

    if (EDUKE32_PREDICT_FALSE(local.numtiles != 1))
        return -3;

    return 0;
}

void tileConvertAnimFormat(int32_t const picnum, uint32_t const picanmdisk)
{
    EDUKE32_STATIC_ASSERT(PICANM_ANIMTYPE_MASK == 192);

    picanm_t * const thispicanm = &picanm[picnum];
    thispicanm->num = picanmdisk&63;
    thispicanm->xofs = (picanmdisk>>8)&255;
    thispicanm->yofs = (picanmdisk>>16)&255;
    thispicanm->sf = ((picanmdisk>>24)&15) | (picanmdisk&192);
    thispicanm->extra = (picanmdisk>>28)&15;
}

void artReadManifest(buildvfs_kfd const fil, artheader_t * const local)
{
    int16_t *tilesizx = (int16_t *) Xmalloc(local->numtiles * sizeof(int16_t));
    int16_t *tilesizy = (int16_t *) Xmalloc(local->numtiles * sizeof(int16_t));
    kread(fil, tilesizx, local->numtiles*sizeof(int16_t));
    kread(fil, tilesizy, local->numtiles*sizeof(int16_t));

    local->tileread = (uint8_t*)Xcalloc(1, bitmap_size(local->numtiles));

    for (bssize_t i=local->tilestart; i<=local->tileend; i++)
    {
        int32_t const localIndex = i - local->tilestart;
        int16_t const tilex = B_LITTLE16(tilesizx[localIndex]);
        int16_t const tiley = B_LITTLE16(tilesizy[localIndex]);
        int32_t const dasiz = tilex * tiley;

        uint32_t picanmdisk;
        kread(fil, &picanmdisk, sizeof(uint32_t));

        if (dasiz == 0)
            continue;

        tilesiz[i].x = tilex;
        tilesiz[i].y = tiley;

        bitmap_set(local->tileread, localIndex);

        picanmdisk = B_LITTLE32(picanmdisk);
        tileConvertAnimFormat(i, picanmdisk);
    }

    DO_FREE_AND_NULL(tilesizx);
    DO_FREE_AND_NULL(tilesizy);
}

void artPreloadFile(buildvfs_kfd const fil, artheader_t * const local)
{
    char *buffer = NULL;
    int32_t buffersize = 0;

    for (bssize_t i=local->tilestart; i<=local->tileend; i++)
    {
        int32_t const localIndex = i - local->tilestart;
        if (!bitmap_test(local->tileread, localIndex))
            continue;

        int const dasiz = tilesiz[i].x * tilesiz[i].y;
        if (dasiz == 0)
        {
            tileDelete(i);
            continue;
        }

        maybe_grow_buffer(&buffer, &buffersize, dasiz);
        kread(fil, buffer, dasiz);
        tileSetData(i, dasiz, buffer);
    }

    DO_FREE_AND_NULL(buffer);
    DO_FREE_AND_NULL(local->tileread);
}

static void artPreloadFileSafe(buildvfs_kfd const fil, artheader_t * const local)
{
    char *buffer = NULL;
    int32_t buffersize = 0;

    for (bssize_t i=local->tilestart; i<=local->tileend; i++)
    {
        int32_t const localIndex = i - local->tilestart;
        if (!bitmap_test(local->tileread, localIndex))
            continue;

        int const dasiz = tilesiz[i].x * tilesiz[i].y;
        if (dasiz == 0)
        {
            tileSoftDelete(i);
            continue;
        }

        maybe_grow_buffer(&buffer, &buffersize, dasiz);
        kread(fil, buffer, dasiz);
        tileSetDataSafe(i, dasiz, buffer);
    }

    DO_FREE_AND_NULL(buffer);
    DO_FREE_AND_NULL(local->tileread);
}

static const char *artGetIndexedFileName(int32_t tilefilei)
{
    if (tilefilei >= MAXARTFILES_BASE)
    {
        int32_t o = mapartfnXXofs;
        tilefilei -= MAXARTFILES_BASE;

        mapartfilename[o+1] = '0' + tilefilei%10;
        mapartfilename[o+0] = '0' + (tilefilei/10)%10;

        return mapartfilename;
    }
    else
    {
        Bsnprintf(artfilename, sizeof(artfilename), artfilenameformat, tilefilei);

        return artfilename;
    }
}

// Returns:
//  0: successfully read ART file
// >0: error with the ART file
// -1: ART file does not exist
//<-1: per-map ART issue
static int32_t artReadIndexedFile(int32_t tilefilei)
{
    const char *fn = artGetIndexedFileName(tilefilei);
    const int32_t permap = (tilefilei >= MAXARTFILES_BASE);  // is it a per-map ART file?
    buildvfs_kfd fil;

    if ((fil = kopen4load(fn, 0)) != buildvfs_kfd_invalid)
    {
        artheader_t local;
        int const headerval = artReadHeader(fil, fn, &local);
        if (headerval != 0)
        {
            kclose(fil);
            return headerval;
        }

        if (permap)
        {
            // Check whether we can evict existing tiles to make place for
            // per-map ART ones.
            for (int i=local.tilestart; i<=local.tileend; i++)
            {
                // Tiles having dummytile replacements or those that are
                // cache1d-locked can't be replaced.
                if (bitmap_test(faketile, i) || walock[i] >= CACHE1D_LOCKED)
                {
                    LOG_F(WARNING, "Per-map .art file could not be loaded %s: tile %d is locked by %s.", fn, i, walock[i] >= CACHE1D_LOCKED ? "cache1d" : "dummytile");
                    kclose(fil);
                    return -3;
                }
            }

            // Free existing tiles from the cache1d. CACHE1D_FREE
            Bmemset(&waloff[local.tilestart], 0, local.numtiles*sizeof(intptr_t));
            Bmemset(&walock[local.tilestart], 1, local.numtiles*sizeof(walock[0]));
        }

        artReadManifest(fil, &local);

#ifndef USE_PHYSFS
        if (cache1d_file_fromzip(fil))
#else
        if (1)
#endif
        {
            if (permap)
                artPreloadFileSafe(fil, &local);
            else
                artPreloadFile(fil, &local);
        }
        else
        {
            int offscount = ktell(fil);

            for (bssize_t i=local.tilestart; i<=local.tileend; ++i)
            {
                int32_t const localIndex = i - local.tilestart;
                if (!bitmap_test(local.tileread, localIndex))
                    continue;

                int const dasiz = tilesiz[i].x * tilesiz[i].y;

                tilefilenum[i] = tilefilei;
                tilefileoffs[i] = offscount;

                offscount += dasiz;
                // artsize += ((dasiz+15)&0xfffffff0);
            }

            DO_FREE_AND_NULL(local.tileread);
        }

        if (permap)
            LOG_F(INFO, "Per-map .art file %s loaded", fn);

        kclose(fil);
        return 0;
    }

    return -1;
}

//
// loadpics
//
int32_t artLoadFiles(const char *filename, int32_t askedsize)
{
    Bstrncpyz(artfilenameformat, filename, sizeof(artfilenameformat));

    Bmemset(&tilesiz[0], 0, sizeof(vec2_16_t) * MAXTILES);
    Bmemset(picanm, 0, sizeof(picanm));

    for (auto &rot : rottile)
        rot = { -1, -1 };

    //    artsize = 0;

    for (int tilefilei=0; tilefilei<MAXARTFILES_BASE; tilefilei++)
        artReadIndexedFile(tilefilei);

    Bmemset(gotpic, 0, sizeof(gotpic));
    //cachesize = min((int32_t)((Bgetsysmemsize()/100)*60),max(artsize,askedsize));
    g_vm_size = (Bgetsysmemsize() <= (uint32_t)askedsize) ? (int32_t)((Bgetsysmemsize() / 100) * 60) : askedsize;
    g_vm_data = Xmalloc(g_vm_size);
    g_cache.initBuffer((intptr_t) g_vm_data, g_vm_size);

    artUpdateManifest();

    artfil = buildvfs_kfd_invalid;
    artfilnum = -1;
    artfilplc = 0L;

    return 0;
}


//
// loadtile
//
static void tilePostLoad(int16_t tilenume);

bool (*rt_tileload_callback)(int16_t tileNum) = nullptr;

bool tileLoad(int16_t tileNum)
{
    if ((unsigned) tileNum >= (unsigned) MAXTILES) return 0;
    int const dasiz = tilesiz[tileNum].x*tilesiz[tileNum].y;
    if (dasiz <= 0) return 0;

    // Allocate storage if necessary.
    if (waloff[tileNum] == 0)
    {
        walock[tileNum] = CACHE1D_UNLOCKED;
        g_cache.allocateBlock(&waloff[tileNum], dasiz, &walock[tileNum]);
    }

    if (!duke64 || !rt_tileload_callback || !rt_tileload_callback(tileNum))
        tileLoadData(tileNum, dasiz, (char *) waloff[tileNum]);

#ifdef USE_OPENGL
    if (videoGetRenderMode() >= REND_POLYMOST &&
        in3dmode() && !usevulkan)
    {
        //POGOTODO: this type stuff won't be necessary down the line -- review this
        int type;
        for (type = 0; type <= 1; ++type)
        {
            gltexinvalidate(tileNum, 0, (type ? DAMETH_CLAMPED : DAMETH_MASK) | DAMETH_INDEXED);
            texcache_fetch(tileNum, 0, 0, (type ? DAMETH_CLAMPED : DAMETH_MASK) | DAMETH_INDEXED);
        }
    }
#endif

    tilePostLoad(tileNum);

    return (waloff[tileNum] != 0 && tilesiz[tileNum].x > 0 && tilesiz[tileNum].y > 0);
}

void tileMaybeRotate(int16_t tilenume)
{
    auto &rot = rottile[tilenume];
    auto &siz = tilesiz[rot.owner];

    auto src = (char *)waloff[rot.owner];
    auto dst = (char *)waloff[tilenume];

    // the engine has a squarerotatetile() we could call, but it mirrors at the same time
    for (int x = 0; x < siz.x; ++x)
    {
        int const xofs = siz.x - x - 1;
        int const yofs = siz.y * x;

        for (int y = 0; y < siz.y; ++y)
            *(dst + y * siz.x + xofs) = *(src + y + yofs);
    }

    tileSetSize(tilenume, siz.y, siz.x);
}

void tileLoadData(int16_t tilenume, int32_t dasiz, char *buffer)
{
    int const owner = rottile[tilenume].owner;

    if (owner != -1)
    {
        if (!waloff[owner])
            tileLoad(owner);

        if (waloff[owner] && waloff[tilenume])
            tileMaybeRotate(tilenume);

        return;
    }

    int const tfn = tilefilenum[tilenume];

    // dummy tiles for highres replacements and tilefromtexture definitions
    if (bitmap_test(faketile, tilenume))
    {
        if (faketiledata[tilenume] != NULL)
            LZ4_decompress_safe(faketiledata[tilenume], buffer, faketilesize[tilenume], dasiz);

        faketimerhandler();
        return;
    }

    // Potentially switch open ART file.
    if (tfn != artfilnum)
    {
        if (artfil != buildvfs_kfd_invalid)
            kclose(artfil);

        char const *fn = artGetIndexedFileName(tfn);

        artfil = kopen4loadfrommod(fn, 0);

        if (artfil == buildvfs_kfd_invalid)
        {
            LOG_F(ERROR, "Failed opening .art file %s!", fn);
            return;
        }

        artfilnum = tfn;
        artfilplc = 0L;

        faketimerhandler();
    }

    // Seek to the right position.
    if (artfilplc != tilefileoffs[tilenume])
    {
        klseek(artfil, tilefileoffs[tilenume], BSEEK_SET);
        faketimerhandler();
    }

    kread(artfil, buffer, dasiz);
    faketimerhandler();
    artfilplc = tilefileoffs[tilenume]+dasiz;
}

static void tilePostLoad(int16_t tilenume)
{
#if !defined DEBUG_TILESIZY_512 && !defined DEBUG_TILEOFFSETS
    UNREFERENCED_PARAMETER(tilenume);
#endif
#ifdef DEBUG_TILESIZY_512
    if (tilesizy[tilenume] >= 512)
    {
        int32_t i;
        char *p = (char *) waloff[tilenume];
        for (i=0; i<tilesizx[tilenume]*tilesizy[tilenume]; i++)
            p[i] = i;
    }
#endif
#ifdef DEBUG_TILEOFFSETS
    // Add some dark blue marker lines to STEAM and CEILINGSTEAM.
    // See test_tileoffsets.map.
    if (tilenume==1250 || tilenume==1255)
    {
        char *p = (char *) waloff[tilenume];
        p[0] = p[1] = p[2] = p[3] = 254;
    }

    // Add some offset to the cocktail glass neon sign. It's more asymmetric
    // than the steam, and thus more suited to debugging the spatial
    // orientation of drawn sprites.
    if (tilenume==1008)
    {
        picanm[tilenume].xofs = 8;
        picanm[tilenume].yofs = 12;
    }
#endif
}

int32_t tileGetCRC32(int16_t tileNum)
{
    if ((unsigned)tileNum >= (unsigned)MAXTILES)
        return 0;
    int const dasiz = tilesiz[tileNum].x * tilesiz[tileNum].y;
    if (dasiz <= 0)
        return 0;

    auto data = (char *)Xmalloc(dasiz);
    tileLoadData(tileNum, dasiz, data);

    int32_t crc = Bcrc32((unsigned char *)data, (unsigned int)dasiz, 0);

    Xfree(data);

    return crc;
}

vec2_16_t tileGetSize(int16_t tileNum)
{
    if ((unsigned)tileNum >= (unsigned)MAXTILES)
        return vec2_16_t{};

    return tilesiz[tileNum];
}

// Assumes pic has been initialized to zero.
void artConvertRGB(palette_t * const pic, uint8_t const * const buf, int32_t const bufsizx, int32_t const sizx, int32_t const sizy)
{
    for (bssize_t y = 0; y < sizy; ++y)
    {
        palette_t * const picrow = &pic[bufsizx * y];

        for (bssize_t x = 0; x < sizx; ++x)
        {
            uint8_t index = buf[sizy * x + y];

            if (index == 255)
                continue;

            index *= 3;

            // pic is BGRA
            picrow[x].r = palette[index+2];
            picrow[x].g = palette[index+1];
            picrow[x].b = palette[index];
            picrow[x].f = 255;
        }
    }
}

//
// allocatepermanenttile
//
intptr_t tileCreate(int16_t tilenume, int32_t xsiz, int32_t ysiz)
{
    if (xsiz <= 0 || ysiz <= 0 || (unsigned) tilenume >= MAXTILES)
        return 0;

    int const dasiz = xsiz*ysiz;

    walock[tilenume] = CACHE1D_PERMANENT;
    g_cache.allocateBlock(&waloff[tilenume], dasiz, &walock[tilenume]);

    tileSetSize(tilenume, xsiz, ysiz);
    Bmemset(&picanm[tilenume], 0, sizeof(picanm_t));

    return waloff[tilenume];
}

//
// copytilepiece
//
void tileCopySection(int32_t tilenume1, int32_t sx1, int32_t sy1, int32_t xsiz, int32_t ysiz,
    int32_t tilenume2, int32_t sx2, int32_t sy2)
{
    char *ptr1, *ptr2, dat;
    int32_t xsiz1, ysiz1, xsiz2, ysiz2, i, j, x1, y1, x2, y2;

    xsiz1 = tilesiz[tilenume1].x; ysiz1 = tilesiz[tilenume1].y;
    xsiz2 = tilesiz[tilenume2].x; ysiz2 = tilesiz[tilenume2].y;
    if ((xsiz1 > 0) && (ysiz1 > 0) && (xsiz2 > 0) && (ysiz2 > 0))
    {
        if (waloff[tilenume1] == 0) tileLoad(tilenume1);
        if (waloff[tilenume2] == 0) tileLoad(tilenume2);

        x1 = sx1;
        for (i=0; i<xsiz; i++)
        {
            y1 = sy1;
            for (j=0; j<ysiz; j++)
            {
                x2 = sx2+i;
                y2 = sy2+j;
                if ((x2 >= 0) && (y2 >= 0) && (x2 < xsiz2) && (y2 < ysiz2))
                {
                    ptr1 = (char *) (waloff[tilenume1] + x1*ysiz1 + y1);
                    ptr2 = (char *) (waloff[tilenume2] + x2*ysiz2 + y2);
                    dat = *ptr1;
                    if (dat != 255)
                        *ptr2 = *ptr1;
                }

                y1++; if (y1 >= ysiz1) y1 = 0;
            }
            x1++; if (x1 >= xsiz1) x1 = 0;
        }
    }
}

void Buninitart(void)
{
    if (artfil != buildvfs_kfd_invalid)
        kclose(artfil);

    DO_FREE_AND_NULL(g_vm_data);
}
