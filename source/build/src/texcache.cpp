#ifdef USE_OPENGL

#include "baselayer.h"
#include "build.h"
#include "engine_priv.h"
#include "lz4.h"
#include "hightile.h"
#include "polymost.h"
#include "texcache.h"
#include "dxtfilter.h"
#include "scriptfile.h"
#include "xxhash_config.h"
#include "kplib.h"

#include "vfs.h"

#include <fcntl.h>
#ifdef _WIN32
# include <io.h>
#else
# include <unistd.h>
#endif
#include <sys/stat.h>

#include "mio.hpp"

// mio uses OS file pointers on Windows and regular int file descriptors elsewhere
#ifdef _WIN32
# define MIO_HANDLE_FROM_FP(f) (mio::file_handle_type)(_get_osfhandle(_fileno(f)))
#else
# define MIO_HANDLE_FROM_FP(f) (mio::file_handle_type)(fileno(f))
#endif

#define CLEAR_GL_ERRORS() while(glGetError() != GL_NO_ERROR) { }
#define TEXCACHE_FREEBUFS() { Xfree(pic), Xfree(packbuf), Xfree(midbuf); }

globaltexcache texcache;

char TEXCACHEFILE[BMAX_PATH] = "texturecache";

static const char *texcache_errors[TEXCACHEERRORS] = {
    "no error",
    "out of memory",
    "read too few bytes from cache file",
    "dedxtfilter failed",
    "glCompressedTexImage2DARB failed",
    "glGetTexLevelParameteriv failed",
};

void (*gloadtile_n64)(int32_t dapic, int32_t dapal, int32_t tintpalnum, int32_t dashade, int32_t dameth, pthtyp *pth, int32_t doalloc) = nullptr;

static pthtyp *texcache_tryart(int32_t const dapicnum, int32_t const dapalnum, int32_t const dashade, int32_t dameth)
{
    const int32_t j = dapicnum&(GLTEXCACHEADSIZ-1);
    pthtyp *pth;
    int32_t tintpalnum = -1;
    int32_t searchpalnum = dapalnum;
    polytintflags_t const tintflags = hictinting[dapalnum].f;

    if (tintflags & (HICTINT_USEONART|HICTINT_ALWAYSUSEART))
    {
        tintpalnum = dapalnum;
        dameth &= ~DAMETH_INDEXED;
        if (!(tintflags & HICTINT_APPLYOVERPALSWAP))
            searchpalnum = 0;
    }

    if (dameth & DAMETH_N64)
    {
        // load from art
        for (pth=texcache.list[j]; pth; pth=pth->next)
            if (pth->picnum == dapicnum && (pth->flags & PTH_N64)
                && (pth->flags & (PTH_CLAMPED | PTH_N64_INTENSIVITY | PTH_N64_SCALED)) == (TO_PTH_CLAMPED(dameth) | TO_PTH_N64_INTENSIVITY(dameth) | TO_PTH_N64_SCALED(dameth)))
            {
                if (pth->flags & PTH_INVALIDATED)
                {
                    pth->flags &= ~PTH_INVALIDATED;
                    gloadtile_n64(dapicnum, searchpalnum, tintpalnum, dashade, dameth, pth, 0);
                    pth->palnum = dapalnum;
                }

                return pth;
            }

        pth = (pthtyp *)Xcalloc(1,sizeof(pthtyp));

        gloadtile_n64(dapicnum, searchpalnum, tintpalnum, dashade, dameth, pth, 1);

        pth->palnum = dapalnum;
        pth->next = texcache.list[j];
        texcache.list[j] = pth;

        return pth;
    }

    // load from art
    for (pth=texcache.list[j]; pth; pth=pth->next)
        if (pth->picnum == dapicnum &&
            (dameth & DAMETH_INDEXED ? (pth->flags & PTH_INDEXED) &&
                                       (pth->flags & PTH_CLAMPED) == TO_PTH_CLAMPED(dameth) :
                 (pth->palnum == dapalnum && pth->shade == dashade &&
                 !(pth->flags & PTH_INDEXED) &&
                 (pth->flags & (PTH_CLAMPED | PTH_HIGHTILE | PTH_NOTRANSFIX)) ==
                     (TO_PTH_CLAMPED(dameth) | TO_PTH_NOTRANSFIX(dameth)) &&
                 polymost_want_npotytex(dameth, tilesiz[dapicnum].y) == !!(pth->flags&PTH_NPOTWALL))))
        {
            if (pth->flags & PTH_INVALIDATED)
            {
                pth->flags &= ~PTH_INVALIDATED;

                // this fixes a problem where per-map art would not be refreshed properly in Polymost between maps, where both maps used mapart
                int32_t ismapart = (tilefilenum[dapicnum] >= MAXARTFILES_BASE) ? 1 : 0;
                gloadtile_art(dapicnum, searchpalnum, tintpalnum, dashade, dameth, pth/*, ismapart*/);
                pth->palnum = dapalnum;
            }

            return pth;
        }

    pth = (pthtyp *)Xcalloc(1,sizeof(pthtyp));

    gloadtile_art(dapicnum, searchpalnum, tintpalnum, dashade, dameth, pth);

    pth->palnum = dapalnum;
    pth->next = texcache.list[j];
    texcache.list[j] = pth;

    return pth;
}

pthtyp *texcache_fetchmulti(pthtyp *pth, hicreplctyp *si, int32_t dapicnum, int32_t dameth)
{
    const int32_t j = dapicnum&(GLTEXCACHEADSIZ-1);
    int32_t i;

    for (i = 0; i <= (GLTEXCACHEADSIZ - 1); i++)
    {
        const pthtyp *pth2;

        for (pth2=texcache.list[i]; pth2; pth2=pth2->next)
        {
            if (pth2->hicr && pth2->hicr->filename && si->filename && filnamcmp(pth2->hicr->filename, si->filename) == 0)
            {
                Bmemcpy(pth, pth2, sizeof(pthtyp));
                pth->picnum = dapicnum;
                pth->flags = TO_PTH_CLAMPED(dameth) | TO_PTH_NOTRANSFIX(dameth) |
                             PTH_HIGHTILE | (drawingskybox>0)*PTH_SKYBOX;
                if (pth2->flags & PTH_HASALPHA)
                    pth->flags |= PTH_HASALPHA;
                pth->hicr = si;

                pth->next = texcache.list[j];
                texcache.list[j] = pth;

                return pth;
            }
        }
    }

    return NULL;
}

// <dashade>: ignored if not in Polymost+r_usetileshades
pthtyp *texcache_fetch(int32_t dapicnum, int32_t dapalnum, int32_t dashade, int32_t dameth)
{
    const int32_t j = dapicnum & (GLTEXCACHEADSIZ - 1);
    int indexed = 0;
    hicreplctyp *si = usehightile ? hicfindsubst(dapicnum, dapalnum, hictinting[dapalnum].f & HICTINT_ALWAYSUSEART) : NULL;
    if (usehightile && (dameth & DAMETH_INDEXED) && !(hictinting[dapalnum].f & HICTINT_ALWAYSUSEART))
    {
        hicreplctyp *pal0 = hicfindsubst(dapicnum, 0, 0);
        if (pal0 && pal0->flags & HICR_INDEXED)
        {
            indexed = 1;
            si = pal0;
        }
    }

    if (drawingskybox && usehightile)
        if ((si = hicfindskybox(dapicnum, dapalnum)) == NULL)
            return NULL;

    if (polymost_usetileshades() != TS_TEXTURE || videoGetRenderMode() != REND_POLYMOST)
        dashade = 0;

    if (!si)
    {
        return (dapalnum >= (MAXPALOOKUPS - RESERVEDPALS) || hicprecaching) ?
                NULL : texcache_tryart(dapicnum, dapalnum, dashade, dameth);
    }
    if (!indexed)
        dameth &= ~DAMETH_INDEXED;

    /* if palette > 0 && replacement found
     *    no effects are applied to the texture
     * else if palette > 0 && no replacement found
     *    effects are applied to the palette 0 texture if it exists
     */

    polytintflags_t const tintflags = hictinting[dapalnum].f;

    const int32_t checktintpal = (tintflags & HICTINT_APPLYOVERALTPAL) ? 0 : si->palnum;
    const int32_t checkcachepal = ((tintflags & HICTINT_IN_MEMORY) || ((tintflags & HICTINT_APPLYOVERALTPAL) && si->palnum > 0)) ? dapalnum : si->palnum;

    // load a replacement
    for (pthtyp *pth = texcache.list[j]; pth; pth = pth->next)
    {
        if (pth->picnum == dapicnum && pth->palnum == checkcachepal && (checktintpal > 0 ? 1 : (pth->effects == tintflags))
            && (pth->flags & (PTH_CLAMPED | PTH_HIGHTILE | PTH_SKYBOX | PTH_NOTRANSFIX | PTH_INDEXED))
               == (TO_PTH_CLAMPED(dameth) | TO_PTH_NOTRANSFIX(dameth) | PTH_HIGHTILE | (drawingskybox > 0) * PTH_SKYBOX | indexed * PTH_INDEXED)
            && (drawingskybox > 0 ? (pth->skyface == drawingskybox) : 1))
        {
            if (pth->flags & PTH_INVALIDATED)
            {
                pth->flags &= ~PTH_INVALIDATED;

                int32_t tilestat = gloadtile_hi(dapicnum, dapalnum, drawingskybox, si, dameth, pth,
                                        (checktintpal > 0) ? 0 : tintflags);  // reload tile

                if (!tilestat)
                    continue;

                if (tilestat == -2)  // bad filename
                    hicclearsubst(dapicnum, dapalnum);

                return (drawingskybox || hicprecaching) ? NULL : texcache_tryart(dapicnum, dapalnum, dashade, dameth);
            }

            return pth;
        }
    }

    pthtyp *pth = (pthtyp *)Xcalloc(1, sizeof(pthtyp));

    // possibly fetch an already loaded multitexture :_)
    if (dapalnum == DETAILPAL && texcache_fetchmulti(pth, si, dapicnum, dameth))
        return pth;

    int32_t tilestat =
    gloadtile_hi(dapicnum, dapalnum, drawingskybox, si, dameth, pth, (checktintpal > 0) ? 0 : tintflags);

    if (!tilestat)
    {
        pth->next = texcache.list[j];
        pth->palnum = checkcachepal;
        texcache.list[j] = pth;
        return pth;
    }

    if (tilestat == -2)  // bad filename
        hicclearsubst(dapicnum, dapalnum);

    Xfree(pth);

    return (drawingskybox || hicprecaching) ? NULL : texcache_tryart(dapicnum, dapalnum, dashade, dameth);
}

static void texcache_closefiles(void)
{
    MAYBE_FCLOSE_AND_NULL(texcache.dataFilePtr);
    MAYBE_FCLOSE_AND_NULL(texcache.indexFilePtr);

    texcache_freeptrs();
}

void texcache_freeptrs(void)
{
    texcache.entrybufsiz = 0;

    if (!texcache.entries)
        return;

    for (bssize_t i = 0; i < texcache.numentries; i++)
        if (texcache.entries[i])
        {
            for (bssize_t ii = texcache.numentries - 1; ii >= 0; ii--)
                if (i != ii && texcache.entries[ii] == texcache.entries[i])
                {
                    /*OSD_Printf("removing duplicate cacheptr %d\n",ii);*/
                    texcache.entries[ii] = NULL;
                }

            DO_FREE_AND_NULL(texcache.entries[i]->name);
            DO_FREE_AND_NULL(texcache.entries[i]);
        }

    DO_FREE_AND_NULL(texcache.entries);
}

static inline void texcache_clearmemcache(void)
{
    texcache.rw_mmap.unmap();
}

void texcache_syncmemcache(void)
{
    if (!texcache.dataFilePtr || buildvfs_flength(texcache.dataFilePtr) <= 0)
        return;

    std::error_code error;

    if (texcache.rw_mmap.is_mapped())
    {
        texcache.rw_mmap.sync(error);

        if (error)
        {
            LOG_F(ERROR, "Failed syncing mmap texcache: %s (%d).", error.message().c_str(), error.value());
            texcache.rw_mmap.unmap();
        }
    }
}

void texcache_init(void)
{
    if (!texcache.indexFilePtr)
    {
        if (texcache.dataFilePtr)
            Bfclose((FILE *)texcache.dataFilePtr);

        texcache.dataFilePtr = nullptr;
    }

    texcache_closefiles();
    texcache_clearmemcache();
    texcache_freeptrs();

    texcache.current = texcache.first = (texcacheindex *)Xcalloc(1, sizeof(texcacheindex));
    texcache.numentries = 0;

    //    Bmemset(&firstcacheindex, 0, sizeof(texcacheindex));
    //    Bmemset(&cacheptrs[0], 0, sizeof(cacheptrs));

    texcache.hashes.size = TEXCACHEHASHSIZE;
    hash_init(&texcache.hashes);
}

static void texcache_deletefiles(void)
{
    Bassert(!texcache.indexFilePtr && !texcache.dataFilePtr);

    unlink(TEXCACHEFILE);
    Bstrcpy(ptempbuf, TEXCACHEFILE);
    Bstrcat(ptempbuf, ".index");
    unlink(ptempbuf);
}

int32_t texcache_enabled(void)
{
#if defined USE_GLEXT && !defined EDUKE32_GLES
    if (!glinfo.texcompr || !glusetexcompr)
        return 0;
#endif

    if (!glusetexcache)
        return 0;

    if (!texcache.indexFilePtr || !texcache.dataFilePtr)
    {
        LOG_F(WARNING, "No active texcache!");
        return 0;
    }

    return 1;
}

void texcache_openfiles(void)
{
    Bassert(!texcache.indexFilePtr && !texcache.dataFilePtr);

    Bstrcpy(ptempbuf, TEXCACHEFILE);
    Bstrcat(ptempbuf, ".index");

    bool const texcache_exists = buildvfs_exists(ptempbuf);

    texcache.indexFilePtr = buildvfs_fopen_append(ptempbuf);
    texcache.dataFilePtr  = buildvfs_fopen_append(TEXCACHEFILE);

    if (!texcache.indexFilePtr || !texcache.dataFilePtr)
    {
        LOG_F(ERROR, "Unable to open cache file %s or %s: %s.", TEXCACHEFILE, ptempbuf, strerror(errno));
        texcache_closefiles();
        glusetexcache = 0;
        return;
    }

    if (!texcache_exists)
    {
        buildvfs_fputstr(texcache.indexFilePtr, "// automatically generated by the engine, DO NOT MODIFY!\n");
    }

    LOG_F(INFO, "Opened %s as cache file.", TEXCACHEFILE);
}


void texcache_checkgarbage(void)
{
    if (!texcache_enabled())
        return;

    texcache.current = texcache.first;

    int32_t bytes = 0;

    while (texcache.current->next)
    {
        bytes += texcache.current->len;
        texcache.current = texcache.current->next;
    }

    buildvfs_fseek_end(texcache.dataFilePtr);
    bytes = buildvfs_ftell(texcache.dataFilePtr)-bytes;

    if (bytes)
        LOG_F(INFO, "Cache contains %d bytes of garbage data", bytes);
}

void texcache_invalidate(void)
{
    DVLOG_F(LOG_DEBUG, "Invalidating texcache...");

    r_downsizevar = r_downsize; // update the cvar representation when the menu changes r_downsize

    polymost_glreset();

    texcache_init();
    texcache_deletefiles();
    texcache_openfiles();
}

int texcache_loadoffsets(void)
{
    Bstrcpy(ptempbuf, TEXCACHEFILE);
    Bstrcat(ptempbuf, ".index");
    scriptfile *script = scriptfile_fromfile(ptempbuf);

    if (!script) return -1;

    int32_t foffset, fsize;
    char *fname;

    while (!scriptfile_eof(script))
    {
        if (scriptfile_getstring(script, &fname)) goto CLEAN_TEXCACHE;     // hashed filename
        if (scriptfile_getnumber(script, &foffset)) goto CLEAN_TEXCACHE;   // offset in cache
        if (scriptfile_getnumber(script, &fsize)) goto CLEAN_TEXCACHE;     // size

        int const i = hash_find(&texcache.hashes,fname);

        if (i > -1)
        {
            // update an existing entry
            texcacheindex *t = texcache.entries[i];
            t->offset = foffset;
            t->len = fsize;
            /*initprintf("%s %d got a match for %s offset %d\n",__FILE__, __LINE__, fname,foffset);*/
        }
        else
        {
            texcacheindex * const index = texcache.current;

            index->name   = Xstrdup(fname);
            index->offset = foffset;
            index->len    = fsize;
            index->next   = (texcacheindex *) Xcalloc(1, sizeof(texcacheindex));
            hash_add(&texcache.hashes, fname, texcache.numentries, 1);
            if (++texcache.numentries > texcache.entrybufsiz)
            {
                texcache.entrybufsiz += 512;
                texcache.entries = (texcacheindex **) Xrealloc(texcache.entries, sizeof(intptr_t) * texcache.entrybufsiz);
            }
            texcache.entries[texcache.numentries-1] = texcache.current;
            texcache.current = index->next;
        }
    }

    scriptfile_close(script);
    return 0;

CLEAN_TEXCACHE:
    LOG_F(ERROR, "Corrupted texcache index detected, invalidating texcache files.");
    scriptfile_close(script);
    texcache_invalidate();
    return -2;
}

// Read from on-disk texcache or its in-memory cache.
int texcache_readdata(void *outBuf, int32_t len)
{
    const bsize_t ofilepos = texcache.dataFilePos;

    texcache.dataFilePos += len;

    if (texcache.rw_mmap.is_mapped() && texcache.rw_mmap.length() >= ofilepos + len)
    {
        Bmemcpy(outBuf, texcache.rw_mmap.data() + ofilepos, len);
        return 0;
    }

    if (buildvfs_fseek_abs(texcache.dataFilePtr, ofilepos) || buildvfs_fread(outBuf, len,1,texcache.dataFilePtr) != 1)
        return 1;

    return 0;
}

char const * texcache_calcid(char *outbuf, const char *filename, const int32_t len, const int32_t dameth, const char effect)
{
    // Assert that BMAX_PATH is a multiple of 4 so that struct texcacheid_t
    // gets no padding inserted by the compiler.
    EDUKE32_STATIC_ASSERT((BMAX_PATH & 3) == 0);

    struct texcacheid_t {
        int32_t len, method;
        char effect, name[BMAX_PATH+3];  // +3: pad to a multiple of 4
    } id = { len, dameth, effect, "" };

    EDUKE32_STATIC_ASSERT((sizeof(struct texcacheid_t) & 3) == 0);

    size_t const fnlen = Bstrlen(filename);

    Bstrcpy(id.name, filename);
    Bsprintf(outbuf, "%08" PRIx64, XXH3_64bits_withSeed((uint8_t *)&id, offsetof(struct texcacheid_t, name) + fnlen, TEXCACHEMAGIC[3]));

    return outbuf;
}

#define FAIL(x) { err = x; goto failure; }

// returns 1 on success
int texcache_readtexheader(char const * cacheid, texcacheheader *head, int32_t modelp)
{
    if (!texcache_enabled())
        return 0;

    int32_t i = hash_find(&texcache.hashes, cacheid);

    if (i < 0 || !texcache.entries[i])
        return 0;  // didn't find it

    texcache.dataFilePos = texcache.entries[i]->offset;
//    initprintf("%s %d got a match for %s offset %d\n",__FILE__, __LINE__, cachefn,offset);

    int err = 0;

    if (texcache_readdata(head, sizeof(texcacheheader)))
        FAIL(0);

    if (Bmemcmp(head->magic, TEXCACHEMAGIC, 4))
        FAIL(1);

    // native (little-endian) -> internal
    head->xdim    = B_LITTLE32(head->xdim);
    head->ydim    = B_LITTLE32(head->ydim);
    head->flags   = B_LITTLE32(head->flags);
    head->quality = B_LITTLE32(head->quality);

    if (modelp && head->quality != r_downsize)
        FAIL(2);
    if ((head->flags & CACHEAD_COMPRESSED) && glusetexcache != 2)
        FAIL(3);
    if (!(head->flags & CACHEAD_COMPRESSED) && glusetexcache == 2)
        FAIL(4);

    // handle nodownsize
    if (!modelp && !(head->flags & CACHEAD_NODOWNSIZE) && head->quality != r_downsize)
        return 0;

    if (gltexmaxsize && (head->xdim > (1<<gltexmaxsize) || head->ydim > (1<<gltexmaxsize)))
        FAIL(5);
    if (!glinfo.texnpot && (head->flags & CACHEAD_NONPOW2))
        FAIL(6);

    return 1;

failure:
    {
        static const char *error_msgs[] = {
            "failed reading texture cache header",
            "header magic string doesn't match",
            "r_downsize doesn't match",  // (skins only)
            "compression doesn't match: cache contains compressed data",
            "compression doesn't match: cache contains uncompressed data",
            "texture in cache exceeds maximum supported size",
            "texture in cache has non-power-of-two size, unsupported",
        };

        LOG_F(ERROR, "Got bad %s data from cache: %s", modelp ? "skin" : "texture", error_msgs[err]);
    }

    return 0;
}

#undef READTEXHEADER_FAILURE

void texcache_prewritetex(texcacheheader *head)
{
    Bmemcpy(head->magic, TEXCACHEMAGIC, 4);   // sizes are set by caller

    if (glusetexcache == 2)
        head->flags |= CACHEAD_COMPRESSED;

    // native -> external (little-endian)
    head->xdim    = B_LITTLE32(head->xdim);
    head->ydim    = B_LITTLE32(head->ydim);
    head->flags   = B_LITTLE32(head->flags);
    head->quality = B_LITTLE32(head->quality);
}

#if defined USE_GLEXT && !defined EDUKE32_GLES

#define WRITEX_FAIL_ON_ERROR() if (glGetError() != GL_NO_ERROR) goto failure

void texcache_writetex_fromdriver(char const * const cacheid, texcacheheader *head)
{
    if (!texcache_enabled()) return;

    GLint gi = GL_FALSE;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &gi);
    if (gi != GL_TRUE)
    {
        static GLint glGetTexLevelParameterivOK = GL_TRUE;
        if (glGetTexLevelParameterivOK == GL_TRUE)
        {
            LOG_F(ERROR, "glGetTexLevelParameteriv returned GL_FALSE!");
            glGetTexLevelParameterivOK = GL_FALSE;
        }
        return;
    }

    texcache_prewritetex(head);
    buildvfs_fseek_end(texcache.dataFilePtr);
    size_t const offset = buildvfs_ftell(texcache.dataFilePtr);

    texcachepicture pict;

    char *pic     = nullptr;
    char *packbuf = nullptr;
    void *midbuf  = nullptr;
    size_t alloclen = 0;

    //    OSD_Printf("Caching %s, offset 0x%x\n", cachefn, offset);
    if (buildvfs_fwrite(head, sizeof(texcacheheader), 1, texcache.dataFilePtr) != 1) goto failure;

    CLEAR_GL_ERRORS();

    for (int level = 0, padx = 0, pady = 0; level == 0 || (padx > 1 || pady > 1); ++level)
    {
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_COMPRESSED, &gi);
        WRITEX_FAIL_ON_ERROR();
        if (gi != GL_TRUE)
            goto failure;  // an uncompressed mipmap

        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_INTERNAL_FORMAT, &gi);
        WRITEX_FAIL_ON_ERROR();

#if defined __APPLE__ && defined POLYMER
        if (pr_ati_textureformat_one && gi == 1) gi = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
#endif
        // native -> external (little endian)
        pict.format = B_LITTLE32(gi);

        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &gi);
        WRITEX_FAIL_ON_ERROR();
        padx      = gi;
        pict.xdim = B_LITTLE32(gi);

        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &gi);
        WRITEX_FAIL_ON_ERROR();
        pady      = gi;
        pict.ydim = B_LITTLE32(gi);

        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_BORDER, &gi);
        WRITEX_FAIL_ON_ERROR();
        pict.border = B_LITTLE32(gi);

        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_DEPTH, &gi);
        WRITEX_FAIL_ON_ERROR();
        pict.depth = B_LITTLE32(gi);

        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &gi);
        WRITEX_FAIL_ON_ERROR();
        uint32_t miplen = gi;
        pict.size       = B_LITTLE32(gi);

        if (alloclen < miplen)
        {
            alloclen = miplen;
            pic      = (char *)Xrealloc(pic, miplen);
            packbuf  = (char *)Xrealloc(packbuf, miplen);
            midbuf   = (void *)Xrealloc(midbuf, miplen);
        }

        glGetCompressedTexImage(GL_TEXTURE_2D, level, pic);
        WRITEX_FAIL_ON_ERROR();

        if (buildvfs_fwrite(&pict, sizeof(texcachepicture), 1, texcache.dataFilePtr) != 1) goto failure;
        if (dxtfilter(&pict, pic, midbuf, packbuf, miplen)) goto failure;
    }

    texcache_postwritetex(cacheid, offset);
    TEXCACHE_FREEBUFS();
    return;

failure:
    LOG_F(ERROR, "texcache mystery error");
    texcache.current->offset = 0;
    Xfree(texcache.current->name);
    TEXCACHE_FREEBUFS();
}

#undef WRITEX_FAIL_ON_ERROR

#endif

void texcache_postwritetex(char const * const cacheid, int32_t const offset)
{
    int32_t const i = hash_find(&texcache.hashes, cacheid);

    texcacheindex *t;

    if (i > -1)
    {
        // update an existing entry
        t = texcache.entries[i];

        t->offset = offset;
        buildvfs_fseek_end(texcache.dataFilePtr);
        t->len    = buildvfs_ftell(texcache.dataFilePtr) - t->offset;
        /*initprintf("%s %d got a match for %s offset %d\n",__FILE__, __LINE__, cachefn,offset);*/
    }
    else
    {
        t = texcache.current;

        Xfree(t->name);
        t->name   = Xstrdup(cacheid);
        t->offset = offset;
        buildvfs_fseek_end(texcache.dataFilePtr);
        t->len    = buildvfs_ftell(texcache.dataFilePtr) - t->offset;
        t->next   = (texcacheindex *)Xcalloc(1, sizeof(texcacheindex));

        hash_add(&texcache.hashes, cacheid, texcache.numentries, 0);

        if (++texcache.numentries > texcache.entrybufsiz)
        {
            texcache.entrybufsiz += 512;
            texcache.entries = (texcacheindex **)Xrealloc(texcache.entries, sizeof(intptr_t) * texcache.entrybufsiz);
        }

        texcache.entries[texcache.numentries - 1] = t;
        texcache.current = t->next;
    }

    if (texcache.indexFilePtr)
    {
        char buf[64];
        buildvfs_fputstrptr(texcache.indexFilePtr, t->name);
        snprintf(buf, sizeof(buf), " %d %d\n", t->offset, t->len);
        buildvfs_fputstrptr(texcache.indexFilePtr, buf);
    }
    else
        LOG_F(ERROR, "fatal error in texcache: no indexFilePtr");
}

static void texcache_setuptexture(int32_t *doalloc, GLuint *glpic)
{
    if (*doalloc&1)
    {
        glGenTextures(1, glpic);  //# of textures (make OpenGL allocate structure)
        *doalloc |= 2;	// prevents glGenTextures being called again if we fail in here
    }

    buildgl_bindTexture(GL_TEXTURE_2D, *glpic);
}

#if defined USE_GLEXT && !defined EDUKE32_GLES
static char const* texcache_format_to_name(GLint format)
{
    switch (format)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            return "GL_COMPRESSED_RGB_S3TC_DXT1";
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            return "GL_COMPRESSED_RGBA_S3TC_DXT1";
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            return "GL_COMPRESSED_RGBA_S3TC_DXT3";
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            return "GL_COMPRESSED_RGBA_S3TC_DXT5";
        default:
            return "UNKNOWN/INVALID";
    }
}
#endif

static int32_t texcache_loadmips(const texcacheheader *head, GLenum *glerr)
{
    texcachepicture pict;

    char *pic     = nullptr;
    char *packbuf = nullptr;
    void *midbuf  = nullptr;

    int32_t alloclen=0;

#if !defined USE_GLEXT && defined EDUKE32_GLES
    UNREFERENCED_PARAMETER(glerr);
    UNREFERENCED_PARAMETER(head);
#endif

    for (bssize_t level = 0; level==0 || (pict.xdim > 1 || pict.ydim > 1); level++)
    {
        if (texcache_readdata(&pict, sizeof(texcachepicture)))
        {
            TEXCACHE_FREEBUFS();
            return TEXCACHERR_BUFFERUNDERRUN;
        }

        // external (little endian) -> native
        pict.size   = B_LITTLE32(pict.size);
        pict.format = B_LITTLE32(pict.format);
        pict.xdim   = B_LITTLE32(pict.xdim);
        pict.ydim   = B_LITTLE32(pict.ydim);
        pict.border = B_LITTLE32(pict.border);
        pict.depth  = B_LITTLE32(pict.depth);

        if (alloclen < pict.size)
        {
            alloclen = pict.size;
            pic      = (char *)Xrealloc(pic, pict.size);
            packbuf  = (char *)Xrealloc(packbuf, pict.size + 16);
            midbuf   = (void *)Xrealloc(midbuf, pict.size);
        }

#if defined USE_GLEXT && !defined EDUKE32_GLES
        if (dedxtfilter(&pict, pic, midbuf, packbuf, (head->flags & CACHEAD_COMPRESSED) != 0))
        {
            TEXCACHE_FREEBUFS();
            return TEXCACHERR_DEDXT;
        }

        glCompressedTexImage2D(GL_TEXTURE_2D, level, pict.format, pict.xdim, pict.ydim, pict.border, pict.size, pic);
        if ((*glerr=glGetError()) != GL_NO_ERROR)
        {
            TEXCACHE_FREEBUFS();
            return TEXCACHERR_COMPTEX;
        }

        GLint format;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_INTERNAL_FORMAT, &format);
        if ((*glerr = glGetError()) != GL_NO_ERROR)
        {
            TEXCACHE_FREEBUFS();
            return TEXCACHERR_GETTEXLEVEL;
        }

        if (pict.format != format)
        {
            LOG_F(ERROR, "Invalid texcache format, have format %s but need format %s.",
                                    texcache_format_to_name(pict.format), texcache_format_to_name(format));
            TEXCACHE_FREEBUFS();
            return -1;
        }
#endif
    }

    TEXCACHE_FREEBUFS();
    return 0;
}

int32_t texcache_loadskin(const texcacheheader *head, int32_t *doalloc, GLuint *glpic, vec2_t *siz)
{
    int32_t err=0;
    GLenum glerr=GL_NO_ERROR;

    texcache_setuptexture(doalloc, glpic);

    siz->x = head->xdim;
    siz->y = head->ydim;

    CLEAR_GL_ERRORS();

    if ((err = texcache_loadmips(head, &glerr)))
    {
        if (err > 0)
            LOG_F(ERROR, "Unable to load cached data: %s (error %x)", texcache_errors[err], glerr);

        return -1;
    }

    return 0;
}

int32_t texcache_loadtile(const texcacheheader *head, int32_t *doalloc, pthtyp *pth)
{
#if 0
    int32_t err   = 0;
    GLenum  glerr = GL_NO_ERROR;

    texcache_setuptexture(doalloc, &pth->glpic);

    pth->siz.x = head->xdim;
    pth->siz.y = head->ydim;

    CLEAR_GL_ERRORS();

    if ((err = texcache_loadmips(head, &glerr)))
    {
        if (err > 0)
            LOG_F(ERROR, "Unable to load cached data: %s (error %x)", texcache_errors[err], glerr);

        return -1;
    }

    return 0;
#endif
    return -1;
}

void texcache_setupmemcache(void)
{
    if (!glusememcache || !texcache_enabled())
        return;

    if (!texcache.dataFilePtr || buildvfs_flength(texcache.dataFilePtr) <= 0)
        return;

    std::error_code error;

    texcache.rw_mmap = mio::make_mmap_sink(MIO_HANDLE_FROM_FP(texcache.dataFilePtr), 0, mio::map_entire_file, error);

    if (error || !texcache.rw_mmap.is_mapped())
    {
        if (error)
            LOG_F(ERROR, "Failed mapping texcache! Error %d: %s", error.value(), error.message().c_str());

        texcache_clearmemcache();
        return;
    }
    else
    {
        LOG_F(INFO, "Mapped %d byte texcache", (int)texcache.rw_mmap.length());
    }
}

// ---------------------------------------
// Voxel 2 Poly Disk Caching
// ---------------------------------------

#define INITVARS_VOXSIZES(_vs, _is, _ts, _total) do {\
    _vs = 5 * 4 * ((size_t)vm->qcnt) * sizeof(GLfloat);\
    _is = 3 * 2 * ((size_t)vm->qcnt) * sizeof(GLuint);\
    _ts = ((size_t)vm->mytexx) * ((size_t)vm->mytexy) * sizeof(int32_t);\
    _total = _vs + _is + _ts;\
} while(0);

struct voxcachedat_t {
    int32_t qcnt, mytexx, mytexy;
    int32_t compressed_size;
};

voxmodel_t* voxcache_fetchvoxmodel(const char* const cacheid)
{
    if (!texcache_enabled()) return NULL;

    int32_t i = hash_find(&texcache.hashes, cacheid);
    if (i < 0 || !texcache.entries[i])
        return NULL;  // didn't find it

    voxcachedat_t voxd = {};
    size_t vertexsize, indexsize, mytexsize, totalsize;
    voxmodel_t* vm = (voxmodel_t*)Xcalloc(1, sizeof(voxmodel_t));

    texcache.dataFilePos = texcache.entries[i]->offset;

    if (texcache_readdata(&voxd, sizeof(voxd)) || voxd.compressed_size <= 0)
    {
        Xfree(vm);
        return NULL;
    }

    vm->mytexx = voxd.mytexx;
    vm->mytexy = voxd.mytexy;
    vm->qcnt = voxd.qcnt;
    INITVARS_VOXSIZES(vertexsize, indexsize, mytexsize, totalsize);

    char* compressed_data = (char*)Xmalloc(voxd.compressed_size);
    texcache_readdata(compressed_data, voxd.compressed_size);

    char* decompressed_data = (char*)Xmalloc(totalsize);
    auto bytes = LZ4_decompress_safe(compressed_data, decompressed_data, voxd.compressed_size, totalsize);
    Xfree(compressed_data);

    Bassert(bytes > 0);

    if (bytes <= 0)
    {
        Xfree(decompressed_data);
        Xfree(vm);
        return NULL;
    }

    vm->vertex = (GLfloat*)Xmalloc(vertexsize);
    Bmemcpy(vm->vertex, decompressed_data, vertexsize);

    vm->index = (GLuint*)Xmalloc(indexsize);
    Bmemcpy(vm->index, &decompressed_data[vertexsize], indexsize);

    vm->mytex = (int32_t*)Xmalloc(mytexsize);
    Bmemcpy(vm->mytex, &decompressed_data[vertexsize + indexsize], mytexsize);

    Xfree(decompressed_data);
    return vm;
}


void voxcache_writevoxmodel(const char* const cacheid, voxmodel_t* vm)
{
    if (!vm || !texcache_enabled()) return;

    size_t vertexsize, indexsize, mytexsize, totalsize;
    voxcachedat_t vxdat = { vm->qcnt, vm->mytexx, vm->mytexy, 0 };
    INITVARS_VOXSIZES(vertexsize, indexsize, mytexsize, totalsize);

    char* srcdata = (char*)Xmalloc(totalsize);
    Bmemcpy(srcdata, vm->vertex, vertexsize);
    Bmemcpy(&srcdata[vertexsize], vm->index, indexsize);
    Bmemcpy(&srcdata[vertexsize + indexsize], vm->mytex, mytexsize);

    int const max_compressed_size = LZ4_compressBound(totalsize);
    char* targetdata = (char*) Xmalloc(max_compressed_size);
    int32_t const actual_compressed_size = LZ4_compress_default((const char*)srcdata, targetdata, totalsize, max_compressed_size);
    Xfree(srcdata);

    Bassert(actual_compressed_size > 0);
    vxdat.compressed_size = actual_compressed_size;
    targetdata = (char*)Xrealloc(targetdata, actual_compressed_size);

    buildvfs_fseek_end(texcache.dataFilePtr);
    size_t const offset = buildvfs_ftell(texcache.dataFilePtr);
    if (buildvfs_fwrite(&vxdat, sizeof(voxcachedat_t), 1, texcache.dataFilePtr) != 1)
        goto vxstore_failure;
    if (buildvfs_fwrite(targetdata, actual_compressed_size, 1, texcache.dataFilePtr) != 1)
        goto vxstore_failure;

    texcache_postwritetex(cacheid, offset);
    Xfree(targetdata);
    return;

vxstore_failure:
    LOG_F(ERROR, "voxcache mystery error");
    texcache.current->offset = 0;
    Xfree(texcache.current->name);
    Xfree(targetdata);
}

#endif
