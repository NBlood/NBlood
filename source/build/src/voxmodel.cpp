//--------------------------------------- VOX LIBRARY BEGINS ---------------------------------------

#ifdef USE_OPENGL

#include "baselayer.h"
#include "build.h"
#include "cache1d.h"
#include "compat.h"
#include "engine_priv.h"
#include "glad/glad.h"
#include "hightile.h"
#include "kplib.h"
#include "mdsprite.h"
#include "palette.h"
#include "polymost.h"
#include "pragmas.h"
#include "texcache.h"
#include "vfs.h"

//For loading/conversion only
static vec3_t voxsiz;
static int32_t yzsiz, *vbit = 0; //vbit: 1 bit per voxel: 0=air,1=solid
static vec3f_t voxpiv;

static int32_t *vcolhashead = 0, vcolhashsizm1;
typedef struct { int32_t p, c, n; } voxcol_t;
static voxcol_t *vcol = 0; int32_t vnum = 0, vmax = 0;

static vec2_u16_t *shp;
static int32_t *shcntmal, *shcnt = 0, shcntp;

static int32_t mytexo5, *zbit, gmaxx, gmaxy, garea, pow2m1[33];
static voxmodel_t *gvox;
static voxrect_t *gquad;
static int32_t gqfacind[7];


//pitch must equal xsiz*4
rhiTexture gloadtex_indexed(const int32_t *picbuf, int32_t xsiz, int32_t ysiz)
{
    const coltype *const pic = (const coltype *)picbuf;
    char *pic2 = (char *)Xmalloc(xsiz*ysiz*sizeof(char));

    for (int i=0; i < ysiz; i++)
    {
        for (int j=0; j < xsiz; j++)
        {
            pic2[j*ysiz+i] = pic[i*xsiz+j].a;
        }
    }

#if 0
    uint32_t rtexid;
    glGenTextures(1, (GLuint *) &rtexid);
    buildgl_bindTexture(GL_TEXTURE_2D, rtexid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.f);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ysiz, xsiz, 0, GL_RED, GL_UNSIGNED_BYTE, (char *) pic2);
#endif

    rhiTextureCreateInfo info = {};
    info.width = ysiz;
    info.height = xsiz;
    info.format = RHI_FORMAT_R8_UNORM;
    info.levels = 1;
    rhiTexture tex = rhi->texture_create(&info);
    rhiTextureUploadInfo info_upload = {};
    info_upload.width = ysiz;
    info_upload.height = xsiz;
    info_upload.data = pic2;
    rhi->texture_update(tex, &info_upload);

    Xfree(pic2);

    return rhi;
}

rhiTexture gloadtex(const int32_t *picbuf, int32_t xsiz, int32_t ysiz, int32_t is8bit, int32_t dapal)
{
    const char *const cptr = &britable[gammabrightness ? 0 : curbrightness][0];

    // Correct for GL's RGB order; also apply gamma here:
    const coltype *const pic = (const coltype *)picbuf;
    int32_t const cnt = xsiz * ysiz;
    coltype *pic2 = (coltype *)Xmalloc(cnt * sizeof(coltype));

    if (!is8bit)
    {
        for (int32_t i = 0; i < cnt; ++i)
        {
            coltype &tcol = pic2[i];
            tcol.b = cptr[pic[i].r];
            tcol.g = cptr[pic[i].g];
            tcol.r = cptr[pic[i].b];
            tcol.a = 255;

            hictinting_applypixcolor(&tcol, dapal, false);
        }
    }
    else
    {
        if (palookup[dapal] == NULL)
            dapal = 0;

        for (int32_t i = 0; i < cnt; ++i)
        {
            const int32_t ii = palookup[dapal][pic[i].a];

            pic2[i].b = cptr[curpalette[ii].b];
            pic2[i].g = cptr[curpalette[ii].g];
            pic2[i].r = cptr[curpalette[ii].r];
            pic2[i].a = 255;
        }
    }

#if 0
    uint32_t rtexid;
    glGenTextures(1, (GLuint *) &rtexid);
    buildgl_bindTexture(GL_TEXTURE_2D, rtexid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, xsiz, ysiz, 0, GL_RGBA, GL_UNSIGNED_BYTE, (char *) pic2);
#endif

    rhiTextureCreateInfo info = {};
    info.width = xsiz;
    info.height = ysiz;
    info.format = RHI_FORMAT_R8G8B8A8_UNORM;
    info.levels = 1;
    rhiTexture tex = rhi->texture_create(&info);
    rhiTextureUploadInfo info_upload = {};
    info_upload.width = xsiz;
    info_upload.height = ysiz;
    info_upload.data = pic2;
    rhi->texture_update(tex, &info_upload);

    Xfree(pic2);

    return tex;
}

static int32_t getvox(int32_t x, int32_t y, int32_t z)
{
    z += x*yzsiz + y*voxsiz.z;

    for (x=vcolhashead[(z*214013LL)&vcolhashsizm1]; x>=0; x=vcol[x].n)
        if (vcol[x].p == z)
            return vcol[x].c;

    return 0x808080;
}

static void putvox(int32_t x, int32_t y, int32_t z, int32_t col)
{
    if (vnum >= vmax)
    {
        vmax = max(vmax<<1, 4096);
        vcol = (voxcol_t *)Xrealloc(vcol, vmax*sizeof(voxcol_t));
    }

    z += x*yzsiz + y*voxsiz.z;

    vcol[vnum].p = z; z = (z*214013LL)&vcolhashsizm1;
    vcol[vnum].c = col;
    vcol[vnum].n = vcolhashead[z]; vcolhashead[z] = vnum++;
}

//Set all bits in vbit from (x,y,z0) to (x,y,z1-1) to 0's
#if 0
static void setzrange0(int32_t *lptr, int32_t z0, int32_t z1)
{
    int32_t const m0 = ~(~0u<<SHIFTMOD32(z0));
    int32_t const m1 = ~0u<<SHIFTMOD32(z1);
    if (!((z0^z1)&~31)) { lptr[z0>>5] &= m0|m1; return; }
    int32_t z = (z0>>5), ze = (z1>>5);
    lptr[z] &= m0;
    for (z++; z<ze; z++) lptr[z] = 0;
    lptr[z] &= m1;
}
#endif
//Set all bits in vbit from (x,y,z0) to (x,y,z1-1) to 1's
static void setzrange1(int32_t *lptr, int32_t z0, int32_t z1)
{
    int32_t const m0 = ~0u<<SHIFTMOD32(z0);
    int32_t const m1 = ~(~0u<<SHIFTMOD32(z1));
    if (!((z0^z1)&~31)) { lptr[z0>>5] |= m1&m0; return; }
    int32_t z = (z0>>5), ze = (z1>>5);
    lptr[z] |= m0;
    for (z++; z<ze; z++) lptr[z] = -1;
    lptr[z] |= m1;
}

static bool isrectfree(int32_t x0, int32_t y0, int32_t dx, int32_t dy)
{
    int32_t i = y0*mytexo5 + (x0>>5);
    dx += x0-1;
    const int32_t c = (dx>>5) - (x0>>5);

    int32_t m = ~pow2m1[x0&31];
    const int32_t m1 = pow2m1[(dx&31)+1];

    if (!c)
    {
        for (m &= m1; dy; dy--, i += mytexo5)
            if (zbit[i]&m)
                return 0;
    }
    else
    {
        for (; dy; dy--, i += mytexo5)
        {
            if (zbit[i]&m)
                return 0;

            int32_t x;
            for (x=1; x<c; x++)
                if (zbit[i+x])
                    return 0;

            if (zbit[i+x]&m1)
                return 0;
        }
    }
    return 1;
}

static void setrect(int32_t x0, int32_t y0, int32_t dx, int32_t dy)
{
    int32_t i = y0*mytexo5 + (x0>>5);
    dx += x0-1;
    const int32_t c = (dx>>5) - (x0>>5);

    int32_t m = ~pow2m1[x0&31];
    const int32_t m1 = pow2m1[(dx&31)+1];

    if (!c)
    {
        for (m &= m1; dy; dy--, i += mytexo5)
            zbit[i] |= m;
    }
    else
    {
        for (; dy; dy--, i += mytexo5)
        {
            zbit[i] |= m;

            int32_t x;
            for (x=1; x<c; x++)
                zbit[i+x] = -1;

            zbit[i+x] |= m1;
        }
    }
}

static void cntquad(int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1,
                    int32_t x2, int32_t y2, int32_t z2, int32_t face)
{
    UNREFERENCED_PARAMETER(x1);
    UNREFERENCED_PARAMETER(y1);
    UNREFERENCED_PARAMETER(z1);
    UNREFERENCED_PARAMETER(face);

    int32_t x = labs(x2-x0), y = labs(y2-y0), z = labs(z2-z0);

    if (x == 0)
        x = z;
    else if (y == 0)
        y = z;

    if (x < y) { z = x; x = y; y = z; }

    shcnt[y*shcntp+x]++;

    if (x > gmaxx) gmaxx = x;
    if (y > gmaxy) gmaxy = y;

    garea += (x+(VOXBORDWIDTH<<1)) * (y+(VOXBORDWIDTH<<1));
    gvox->qcnt++;
}

static void addquad(int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1,
                    int32_t x2, int32_t y2, int32_t z2, int32_t face)
{
    int32_t i;
    int32_t x = labs(x2-x0), y = labs(y2-y0), z = labs(z2-z0);

    if (x == 0) { x = y; y = z; i = 0; }
    else if (y == 0) { y = z; i = 1; }
    else i = 2;

    if (x < y) { z = x; x = y; y = z; i += 3; }

    z = shcnt[y*shcntp+x]++;
    int32_t *lptr = &gvox->mytex[(shp[z].y+VOXBORDWIDTH)*gvox->mytexx +
                                 (shp[z].x+VOXBORDWIDTH)];
    int32_t nx = 0, ny = 0, nz = 0;

    switch (face)
    {
    case 0:
        ny = y1; x2 = x0; x0 = x1; x1 = x2; break;
    case 1:
        ny = y0; y0++; y1++; y2++; break;
    case 2:
        nz = z1; y0 = y2; y2 = y1; y1 = y0; z0++; z1++; z2++; break;
    case 3:
        nz = z0; break;
    case 4:
        nx = x1; y2 = y0; y0 = y1; y1 = y2; x0++; x1++; x2++; break;
    case 5:
        nx = x0; break;
    }

    for (int yy=0; yy<y; yy++, lptr+=gvox->mytexx)
        for (int xx=0; xx<x; xx++)
        {
            switch (face)
            {
            case 0:
                if (i < 3) { nx = x1+x-1-xx; nz = z1+yy; } //back
                else { nx = x1+y-1-yy; nz = z1+xx; }
                break;
            case 1:
                if (i < 3) { nx = x0+xx;     nz = z0+yy; } //front
                else { nx = x0+yy;     nz = z0+xx; }
                break;
            case 2:
                if (i < 3) { nx = x1-x+xx;   ny = y1-1-yy; } //bot
                else { nx = x1-1-yy;   ny = y1-1-xx; }
                break;
            case 3:
                if (i < 3) { nx = x0+xx;     ny = y0+yy; } //top
                else { nx = x0+yy;     ny = y0+xx; }
                break;
            case 4:
                if (i < 3) { ny = y1+x-1-xx; nz = z1+yy; } //right
                else { ny = y1+y-1-yy; nz = z1+xx; }
                break;
            case 5:
                if (i < 3) { ny = y0+xx;     nz = z0+yy; } //left
                else { ny = y0+yy;     nz = z0+xx; }
                break;
            }

            lptr[xx] = getvox(nx, ny, nz);
        }

    //Extend borders horizontally
    for (int xx=0; xx<VOXBORDWIDTH; xx++)
        for (int yy=VOXBORDWIDTH; yy<y+VOXBORDWIDTH; yy++)
        {
            lptr = &gvox->mytex[(shp[z].y+yy)*gvox->mytexx + shp[z].x];
            lptr[xx] = lptr[VOXBORDWIDTH];
            lptr[xx+x+VOXBORDWIDTH] = lptr[x-1+VOXBORDWIDTH];
        }

    //Extend borders vertically
    for (int yy=0; yy<VOXBORDWIDTH; yy++)
    {
        Bmemcpy(&gvox->mytex[(shp[z].y+yy)*gvox->mytexx + shp[z].x],
                &gvox->mytex[(shp[z].y+VOXBORDWIDTH)*gvox->mytexx + shp[z].x],
                (x+(VOXBORDWIDTH<<1))<<2);
        Bmemcpy(&gvox->mytex[(shp[z].y+y+yy+VOXBORDWIDTH)*gvox->mytexx + shp[z].x],
                &gvox->mytex[(shp[z].y+y-1+VOXBORDWIDTH)*gvox->mytexx + shp[z].x],
                (x+(VOXBORDWIDTH<<1))<<2);
    }

    voxrect_t *const qptr = &gquad[gvox->qcnt];

    qptr->v[0].x = x0; qptr->v[0].y = y0; qptr->v[0].z = z0;
    qptr->v[1].x = x1; qptr->v[1].y = y1; qptr->v[1].z = z1;
    qptr->v[2].x = x2; qptr->v[2].y = y2; qptr->v[2].z = z2;

    constexpr vec2_u16_t vbw = { VOXBORDWIDTH, VOXBORDWIDTH };

    for (int j=0; j<3; j++)
        qptr->v[j].uv = shp[z]+vbw;

    if (i < 3)
        qptr->v[1].u += x;
    else
        qptr->v[1].v += y;

    qptr->v[2].u += x;
    qptr->v[2].v += y;

    qptr->v[3].uv  = qptr->v[0].uv  - qptr->v[1].uv  + qptr->v[2].uv;
    qptr->v[3].xyz = qptr->v[0].xyz - qptr->v[1].xyz + qptr->v[2].xyz;

    if (gqfacind[face] < 0)
        gqfacind[face] = gvox->qcnt;

    gvox->qcnt++;
}

static inline int32_t isolid(int32_t x, int32_t y, int32_t z)
{
    if (((uint32_t)x >= (uint32_t)voxsiz.x) | ((uint32_t)y >= (uint32_t)voxsiz.y) | ((uint32_t)z >= (uint32_t)voxsiz.z))
        return 0;

    z += x*yzsiz + y*voxsiz.z;

    return (vbit[z>>5] & (1<<SHIFTMOD32(z))) != 0;
}

static FORCE_INLINE int isair(int const i)
{
    return !(vbit[i>>5] & (1<<SHIFTMOD32(i)));
}

#ifdef USE_GLEXT
void voxvboalloc(voxmodel_t *vm)
{
    glGenBuffers(1, &vm->vbo);
    glGenBuffers(1, &vm->vboindex);

    GLint prevVBO = 0;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevVBO);
    buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, vm->vboindex);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * 3 * 2 * vm->qcnt, vm->index, GL_STATIC_DRAW);
    buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevVBO);

    prevVBO = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVBO);
    buildgl_bindBuffer(GL_ARRAY_BUFFER, vm->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 5 * 4 * vm->qcnt, vm->vertex, GL_STATIC_DRAW);
    buildgl_bindBuffer(GL_ARRAY_BUFFER, prevVBO);
}

void voxvbofree(voxmodel_t *vm)
{
    if (!vm->vbo)
        return;
    glDeleteBuffers(1, &vm->vbo);
    glDeleteBuffers(1, &vm->vboindex);
    vm->vbo = 0;
}
#endif

static voxmodel_t *vox2poly()
{
    gvox = (voxmodel_t *)Xcalloc(1, sizeof(voxmodel_t));

    //x is largest dimension, y is 2nd largest dimension
    int32_t x = voxsiz.x, y = voxsiz.y, z = voxsiz.z;

    if (x < y && x < z)
        x = z;
    else if (y < z)
        y = z;

    if (x < y)
    {
        z = x;
        x = y;
        y = z;
    }

    shcntp = x;
    int32_t i = x*y*sizeof(int32_t);

    shcntmal = (int32_t *)Xmalloc(i);
    memset(shcntmal, 0, i);
    shcnt = &shcntmal[-shcntp-1];

    gmaxx = gmaxy = garea = 0;

    if (pow2m1[32] != -1)
    {
        for (i=0; i<32; i++)
            pow2m1[i] = (1u<<i)-1;
        pow2m1[32] = -1;
    }

    for (i=0; i<7; i++)
        gqfacind[i] = -1;

    i = (max(voxsiz.y, voxsiz.z)+1)<<2;
    int32_t *const bx0 = (int32_t *)Xmalloc(i<<1);
    int32_t *const by0 = (int32_t *)(((intptr_t)bx0)+i);

    int32_t ov, oz=0;

    for (int cnt=0; cnt<2; cnt++)
    {
        void (*daquad)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t) =
            cnt == 0 ? cntquad : addquad;

        gvox->qcnt = 0;

        memset(by0, -1, (max(voxsiz.y, voxsiz.z)+1)<<2);
        int32_t v = 0;

        for (i=-1; i<=1; i+=2)
            for (int y=0; y<voxsiz.y; y++)
                for (int x=0; x<=voxsiz.x; x++)
                    for (int z=0; z<=voxsiz.z; z++)
                    {
                        ov = v; v = (isolid(x, y, z) && (!isolid(x, y+i, z)));
                        if ((by0[z] >= 0) && ((by0[z] != oz) || (v >= ov)))
                        {
                            daquad(bx0[z], y, by0[z], x, y, by0[z], x, y, z, i>=0);
                            by0[z] = -1;
                        }

                        if (v > ov) oz = z;
                        else if ((v < ov) && (by0[z] != oz)) { bx0[z] = x; by0[z] = oz; }
                    }

        for (i=-1; i<=1; i+=2)
            for (int z=0; z<voxsiz.z; z++)
                for (int x=0; x<=voxsiz.x; x++)
                    for (int y=0; y<=voxsiz.y; y++)
                    {
                        ov = v; v = (isolid(x, y, z) && (!isolid(x, y, z-i)));
                        if ((by0[y] >= 0) && ((by0[y] != oz) || (v >= ov)))
                        {
                            daquad(bx0[y], by0[y], z, x, by0[y], z, x, y, z, (i>=0)+2);
                            by0[y] = -1;
                        }

                        if (v > ov) oz = y;
                        else if ((v < ov) && (by0[y] != oz)) { bx0[y] = x; by0[y] = oz; }
                    }

        for (i=-1; i<=1; i+=2)
            for (int x=0; x<voxsiz.x; x++)
                for (int y=0; y<=voxsiz.y; y++)
                    for (int z=0; z<=voxsiz.z; z++)
                    {
                        ov = v; v = (isolid(x, y, z) && (!isolid(x-i, y, z)));
                        if ((by0[z] >= 0) && ((by0[z] != oz) || (v >= ov)))
                        {
                            daquad(x, bx0[z], by0[z], x, y, by0[z], x, y, z, (i>=0)+4);
                            by0[z] = -1;
                        }

                        if (v > ov) oz = z;
                        else if ((v < ov) && (by0[z] != oz)) { bx0[z] = y; by0[z] = oz; }
                    }

        if (!cnt)
        {
            shp = (vec2_u16_t *)Xmalloc(gvox->qcnt*sizeof(vec2_u16_t));

            int32_t sc = 0;

            for (int y=gmaxy; y; y--)
                for (int x=gmaxx; x>=y; x--)
                {
                    i = shcnt[y*shcntp+x]; shcnt[y*shcntp+x] = sc; //shcnt changes from counter to head index

                    for (; i>0; i--)
                    {
                        shp[sc].x = x;
                        shp[sc].y = y;
                        sc++;
                    }
                }

            for (gvox->mytexx=32; gvox->mytexx<(gmaxx+(VOXBORDWIDTH<<1)); gvox->mytexx<<=1)
                /* do nothing */;

            for (gvox->mytexy=32; gvox->mytexy<(gmaxy+(VOXBORDWIDTH<<1)); gvox->mytexy<<=1)
                /* do_nothing */;

            while (gvox->mytexx*gvox->mytexy*8 < garea*9) //This should be sufficient to fit most skins...
            {
skindidntfit:
                if (gvox->mytexx <= gvox->mytexy)
                    gvox->mytexx <<= 1;
                else
                    gvox->mytexy <<= 1;
            }

            mytexo5 = gvox->mytexx>>5;

            i = ((gvox->mytexx*gvox->mytexy+31)>>5)<<2;
            zbit = (int32_t *)Xmalloc(i);
            memset(zbit, 0, i);

            v = gvox->mytexx*gvox->mytexy;
            constexpr vec2_u16_t vbw = { (VOXBORDWIDTH<<1), (VOXBORDWIDTH<<1) };

            for (int z=0; z<sc; z++)
            {
                auto d = shp[z] + vbw;
                i = v;

                int32_t x0, y0;

                do
                {
#if (VOXUSECHAR != 0)
                    x0 = ((rand()&32767)*(min(gvox->mytexx, 255)-d.x))>>15;
                    y0 = ((rand()&32767)*(min(gvox->mytexy, 255)-d.y))>>15;
#else
                    x0 = ((rand()&32767)*(gvox->mytexx+1-d.x))>>15;
                    y0 = ((rand()&32767)*(gvox->mytexy+1-d.y))>>15;
#endif
                    i--;
                    if (i < 0) //Time-out! Very slow if this happens... but at least it still works :P
                    {
                        Xfree(zbit);

                        //Re-generate shp[].x/y (box sizes) from shcnt (now head indices) for next pass :/
                        int j = 0;

                        for (int y=gmaxy; y; y--)
                            for (int x=gmaxx; x>=y; x--)
                            {
                                i = shcnt[y*shcntp+x];

                                for (; j<i; j++)
                                {
                                    shp[j].x = x0;
                                    shp[j].y = y0;
                                }

                                x0 = x;
                                y0 = y;
                            }

                        for (; j<sc; j++)
                        {
                            shp[j].x = x0;
                            shp[j].y = y0;
                        }

                        goto skindidntfit;
                    }
                } while (!isrectfree(x0, y0, d.x, d.y));

                while (y0 && isrectfree(x0, y0-1, d.x, 1))
                    y0--;
                while (x0 && isrectfree(x0-1, y0, 1, d.y))
                    x0--;

                setrect(x0, y0, d.x, d.y);
                shp[z].x = x0; shp[z].y = y0; //Overwrite size with top-left location
            }

            gquad = (voxrect_t *)Xrealloc(gquad, gvox->qcnt*sizeof(voxrect_t));
            gvox->mytex = (int32_t *)Xmalloc(gvox->mytexx*gvox->mytexy*sizeof(int32_t));
        }
    }

    Xfree(shp);
    Xfree(zbit);
    Xfree(bx0);

    const float phack[2] = { 0, 1.f / 256.f };

    gvox->vertex = (GLfloat *)Xmalloc(sizeof(GLfloat) * 5 * 4 * gvox->qcnt);
    gvox->index = (GLuint *)Xmalloc(sizeof(GLuint) * 3 * 2 * gvox->qcnt);

    const float ru = 1.f / ((float)gvox->mytexx);
    const float rv = 1.f / ((float)gvox->mytexy);

    for (int i = 0; i < gvox->qcnt; i++)
    {
        auto const vptr = &gquad[i].v[0];
        auto const vsum = vptr[0].xyz + vptr[2].xyz;

        for (int j=0; j<4; j++)
        {
            gvox->vertex[((i<<2)+j)*5+0] = ((float)vptr[j].x) - phack[vsum.x>(vptr[j].x<<1)] + phack[vsum.x<(vptr[j].x<<1)];
            gvox->vertex[((i<<2)+j)*5+1] = ((float)vptr[j].y) - phack[vsum.y>(vptr[j].y<<1)] + phack[vsum.y<(vptr[j].y<<1)];
            gvox->vertex[((i<<2)+j)*5+2] = ((float)vptr[j].z) - phack[vsum.z>(vptr[j].z<<1)] + phack[vsum.z<(vptr[j].z<<1)];

            gvox->vertex[((i<<2)+j)*5+3] = ((float)vptr[j].u)*ru;
            gvox->vertex[((i<<2)+j)*5+4] = ((float)vptr[j].v)*rv;
        }

        gvox->index[(i<<1)*3+0] = (i<<2)+0;
        gvox->index[(i<<1)*3+1] = (i<<2)+1;
        gvox->index[(i<<1)*3+2] = (i<<2)+2;

        gvox->index[((i<<1)+1)*3+0] = (i<<2)+0;
        gvox->index[((i<<1)+1)*3+1] = (i<<2)+2;
        gvox->index[((i<<1)+1)*3+2] = (i<<2)+3;
    }

    DO_FREE_AND_NULL(gquad);

    return gvox;
}

static void alloc_vcolhashead(void)
{
    vcolhashead = (int32_t *)Xmalloc((vcolhashsizm1+1)*sizeof(int32_t));
    memset(vcolhashead, -1, (vcolhashsizm1+1)*sizeof(int32_t));
}

static void alloc_vbit(void)
{
    yzsiz = voxsiz.y*voxsiz.z;
    int32_t i = ((voxsiz.x*yzsiz+31)>>3)+1;

    vbit = (int32_t *)Xmalloc(i);
    memset(vbit, 0, i);
}

static void read_pal(buildvfs_kfd fil, int32_t pal[256])
{
    klseek(fil, -768, SEEK_END);

    for (int i=0; i<256; i++)
    {
        char c[3];
        kread(fil, c, sizeof(c));
//#if B_BIG_ENDIAN != 0
        pal[i] = B_LITTLE32((c[0]<<18) + (c[1]<<10) + (c[2]<<2) + (i<<24));
//#endif
    }
}

static int32_t loadvox(const char *filnam)
{
    const buildvfs_kfd fil = kopen4load(filnam, 0);
    if (fil == buildvfs_kfd_invalid)
        return -1;

    kread(fil, &voxsiz, sizeof(vec3_t));
#if B_BIG_ENDIAN != 0
    voxsiz.x = B_LITTLE32(voxsiz.x);
    voxsiz.y = B_LITTLE32(voxsiz.y);
    voxsiz.z = B_LITTLE32(voxsiz.z);
#endif
    voxpiv.x = (float)voxsiz.x * .5f;
    voxpiv.y = (float)voxsiz.y * .5f;
    voxpiv.z = (float)voxsiz.z * .5f;

    int32_t pal[256];
    read_pal(fil, pal);
    pal[255] = -1;

    vcolhashsizm1 = 8192-1;
    alloc_vcolhashead();
    alloc_vbit();

    char *const tbuf = (char *)Xmalloc(voxsiz.z*sizeof(uint8_t));

    klseek(fil, sizeof(vec3_t), SEEK_SET);
    for (int x=0; x<voxsiz.x; x++)
    {
        int32_t j = x * yzsiz;
        for (int y=0; y<voxsiz.y; y++)
        {
            kread(fil, tbuf, voxsiz.z);

            for (int32_t z = 0; z < voxsiz.z; ++z)
                if (tbuf[z] != 255)
                {
                    const int32_t i = j+z;
                    vbit[i>>5] |= (1<<SHIFTMOD32(i));
                }

            j += voxsiz.z;
        }
    }

    klseek(fil, sizeof(vec3_t), SEEK_SET);
    for (int x=0; x<voxsiz.x; x++)
    {
        int32_t j = x * yzsiz;
        for (int y=0; y<voxsiz.y; y++)
        {
            kread(fil, tbuf, voxsiz.z);

            for (int z=0; z<voxsiz.z; z++)
            {
                if (tbuf[z] == 255)
                    continue;

                if (!x | !y | !z | (x == voxsiz.x-1) | (y == voxsiz.y-1) | (z == voxsiz.z-1))
                {
                    putvox(x, y, z, pal[tbuf[z]]);
                    continue;
                }

                const int32_t k = j+z;

                if (isair(k-yzsiz) | isair(k+yzsiz) |
                    isair(k-voxsiz.z) | isair(k+voxsiz.z) |
                    isair(k-1) | isair(k+1))
                {
                    putvox(x, y, z, pal[tbuf[z]]);
                    continue;
                }
            }

            j += voxsiz.z;
        }
    }

    Xfree(tbuf);
    kclose(fil);

    return 0;
}

static int32_t loadkvx(const char *filnam)
{
    const buildvfs_kfd fil = kopen4load(filnam, 0);
    if (fil == buildvfs_kfd_invalid)
        return -1;

    int32_t mip1leng;
    kread(fil, &mip1leng, sizeof(int32_t));
    kread(fil, &voxsiz, sizeof(vec3_t));
#if B_BIG_ENDIAN != 0
    mip1leng = B_LITTLE32(mip1leng);
    voxsiz.x = B_LITTLE32(voxsiz.x);
    voxsiz.y = B_LITTLE32(voxsiz.y);
    voxsiz.z = B_LITTLE32(voxsiz.z);
#endif

    vec3_t v;
    kread(fil, &v, sizeof(vec3_t));
    voxpiv.x = (float)B_LITTLE32(v.x)*(1.f/256.f);
    voxpiv.y = (float)B_LITTLE32(v.y)*(1.f/256.f);
    voxpiv.z = (float)B_LITTLE32(v.z)*(1.f/256.f);
    klseek(fil, (voxsiz.x+1)<<2, SEEK_CUR);

    const int32_t ysizp1 = voxsiz.y+1;
    int32_t const xyoffscnt = voxsiz.x * ysizp1;
    int32_t const xyoffssiz = xyoffscnt * sizeof(uint16_t);

    uint16_t *xyoffs = (uint16_t *)Xmalloc(xyoffssiz);
    kread(fil, xyoffs, xyoffssiz);
#if B_BIG_ENDIAN != 0
    for (int32_t i = 0; i < xyoffscnt; ++i)
        xyoffs[i] = B_LITTLE16(xyoffs[i]);
#endif

    int32_t pal[256];
    read_pal(fil, pal);

    alloc_vbit();

    for (vcolhashsizm1=4096; vcolhashsizm1<(mip1leng>>1); vcolhashsizm1<<=1)
    {
        /* do nothing */
    }
    vcolhashsizm1--; //approx to numvoxs!
    alloc_vcolhashead();

    klseek(fil, (7 * sizeof(int32_t)) + ((voxsiz.x+1)<<2) + ((ysizp1*voxsiz.x)<<1), SEEK_SET);

    int32_t const tbufsiz = kfilelength(fil)-ktell(fil);
    char *const tbuf = (char *)Xmalloc(tbufsiz);

    kread(fil, tbuf, tbufsiz);
    kclose(fil);

    char *cptr = tbuf;

    for (int x=0; x<voxsiz.x; x++) //Set surface voxels to 1 else 0
    {
        int32_t j = x * yzsiz;
        for (int y=0; y<voxsiz.y; y++)
        {
            int32_t const idx = x*ysizp1+y;
            int32_t i = xyoffs[idx+1] - xyoffs[idx];
            int32_t z1 = 0;

            while (i)
            {
                const int32_t z0 = cptr[0];
                const int32_t k = cptr[1];
                cptr += 3;

                if (!(cptr[-1]&16))
                    setzrange1(vbit, j+z1, j+z0);

                i -= k+3;
                z1 = z0+k;

                setzrange1(vbit, j+z0, j+z1);  // PK: oob in AMC TC dev if vbit alloc'd w/o +1

                for (int z=z0; z<z1; z++)
                    putvox(x, y, z, pal[*cptr++]);
            }

            j += voxsiz.z;
        }
    }

    Xfree(tbuf);
    Xfree(xyoffs);

    return 0;
}

static int32_t loadkv6(const char *filnam)
{
    const buildvfs_kfd fil = kopen4load(filnam, 0);
    if (fil == buildvfs_kfd_invalid)
        return -1;

    int32_t magic;
    kread(fil, &magic, sizeof(int32_t));
    if (magic != B_LITTLE32(0x6c78764b)) // "Kvxl"
    {
        kclose(fil);
        return -1;
    }

    kread(fil, &voxsiz, sizeof(vec3_t));
#if B_BIG_ENDIAN != 0
    voxsiz.x = B_LITTLE32(voxsiz.x);
    voxsiz.y = B_LITTLE32(voxsiz.y);
    voxsiz.z = B_LITTLE32(voxsiz.z);
#endif

    vec3_t v;
    kread(fil, &v, sizeof(vec3_t));
#if B_BIG_ENDIAN != 0
    v.x = B_LITTLE32(v.x);
    v.y = B_LITTLE32(v.y);
    v.z = B_LITTLE32(v.z);
#endif
    EDUKE32_STATIC_ASSERT(sizeof(vec3_t) == sizeof(vec3f_t));
    memcpy(&voxpiv, &v, sizeof(vec3_t));

    int32_t numvoxs;
    kread(fil, &numvoxs, sizeof(int32_t));
    numvoxs = B_LITTLE32(numvoxs);

    int32_t const ylencnt = voxsiz.x * voxsiz.y;
    int32_t const ylensiz = ylencnt * sizeof(uint16_t);
    uint16_t *const ylen = (uint16_t *)Xmalloc(ylensiz);

    klseek(fil, (8 * sizeof(int32_t)) + (numvoxs<<3) + (voxsiz.x<<2), SEEK_SET);
    kread(fil, ylen, ylensiz);
#if B_BIG_ENDIAN != 0
    for (int32_t i = 0; i < ylencnt; ++i)
        ylen[i] = B_LITTLE16(ylen[i]);
#endif

    klseek(fil, 8 * sizeof(int32_t), SEEK_SET);

    alloc_vbit();

    for (vcolhashsizm1=4096; vcolhashsizm1<numvoxs; vcolhashsizm1<<=1)
    {
        /* do nothing */
    }
    vcolhashsizm1--;
    alloc_vcolhashead();

    for (int x=0; x<voxsiz.x; x++)
    {
        int32_t j = x * yzsiz;
        for (int y=0; y<voxsiz.y; y++)
        {
            int32_t z1 = voxsiz.z;

            for (int32_t i = 0, i_end = ylen[x*voxsiz.y+y]; i < i_end; ++i)
            {
                char c[8];
                kread(fil, c, sizeof(c)); //b,g,r,a,z_lo,z_hi,vis,dir

                const int32_t z0 = B_LITTLE16(B_UNBUF16(&c[4]));

                if (!(c[6]&16))
                    setzrange1(vbit, j+z1, j+z0);

                vbit[(j+z0)>>5] |= (1<<SHIFTMOD32(j+z0));

                putvox(x, y, z0, B_LITTLE32(B_UNBUF32(&c[0]))&0xffffff);
                z1 = z0+1;
            }

            j += voxsiz.z;
        }
    }

    Xfree(ylen);
    kclose(fil);

    return 0;
}

void voxfree(voxmodel_t *m)
{
    if (!m)
        return;

#ifdef USE_GLEXT
    voxvbofree(m);
#endif

    DO_FREE_AND_NULL(m->mytex);
    DO_FREE_AND_NULL(m->vertex);
    DO_FREE_AND_NULL(m->index);
    DO_FREE_AND_NULL(m->texid);

    Xfree(m);
}

voxmodel_t *voxload(const char *filnam)
{
    int32_t is8bit, ret;

    const int32_t i = Bstrlen(filnam)-4;
    if (i < 0)
        return NULL;

    if (!Bstrcasecmp(&filnam[i], ".vox")) { ret = loadvox(filnam); is8bit = 1; }
    else if (!Bstrcasecmp(&filnam[i], ".kvx")) { ret = loadkvx(filnam); is8bit = 1; }
    else if (!Bstrcasecmp(&filnam[i], ".kv6")) { ret = loadkv6(filnam); is8bit = 0; }
    //else if (!Bstrcasecmp(&filnam[i],".vxl")) { ret = loadvxl(filnam); is8bit = 0; }
    else return NULL;

    voxmodel_t* vm = NULL;
    if (ret >= 0)
    {
        // file presence is guaranteed
        buildvfs_kfd filh = kopen4load(filnam, 0);
        int32_t const filelen = kfilelength(filh);
        kclose(filh);

        char voxcacheid[BMAX_PATH];
        texcache_calcid(voxcacheid, filnam, filelen, -1, -1);

        // static variable 'gvox' is normally defined by 'vox2poly' -- do same here for safety
        if (!(gvox = vm = voxcache_fetchvoxmodel(voxcacheid)))
        {
            vm = vox2poly();
            voxcache_writevoxmodel(voxcacheid, vm);
        }

        if (vm)
        {
            vm->mdnum = 1; //VOXel model id
            vm->scale = vm->bscale = 1.f;
            vm->siz = voxsiz;
            vm->piv = voxpiv;
            vm->is8bit = is8bit;

            vm->texid = (rhiTexture*)Xcalloc(MAXPALOOKUPS, sizeof(rhiTexture));
        }
    }

    DO_FREE_AND_NULL(shcntmal);
    DO_FREE_AND_NULL(vbit);
    DO_FREE_AND_NULL(vcol);
    vnum = vmax = 0;
    DO_FREE_AND_NULL(vcolhashead);

    return vm;
}

voxmodel_t *loadkvxfrombuf(const char *kvxbuffer, int32_t length)
{
    int32_t i, mip1leng;

    if (!kvxbuffer)
        return NULL;

    char *buffer = (char*)Xmalloc(length);
    if (!buffer)
        return NULL;

    Bmemcpy(buffer, kvxbuffer, length);

    int32_t *longptr = (int32_t*)buffer;

    mip1leng = *longptr++; mip1leng = B_LITTLE32(mip1leng);
    if (mip1leng > length - 4)
    {
        // Invalid KVX file
        Xfree(buffer);
        return NULL;
    }
    memcpy(&voxsiz, longptr, sizeof(vec3_t));
    longptr += 3;
#if B_BIG_ENDIAN != 0
    voxsiz.x = B_LITTLE32(voxsiz.x);
    voxsiz.y = B_LITTLE32(voxsiz.y);
    voxsiz.z = B_LITTLE32(voxsiz.z);
#endif
    i = *longptr++; voxpiv.x = (float)B_LITTLE32(i)*(1.f/256.f);
    i = *longptr++; voxpiv.y = (float)B_LITTLE32(i)*(1.f/256.f);
    i = *longptr++; voxpiv.z = (float)B_LITTLE32(i)*(1.f/256.f);
    longptr += voxsiz.x+1;

    const int32_t ysizp1 = voxsiz.y+1;
    i = voxsiz.x*ysizp1*sizeof(int16_t);
    uint16_t *xyoffs = (uint16_t*)longptr;

    for (i=i/sizeof(int16_t)-1; i>=0; i--)
        xyoffs[i] = B_LITTLE16(xyoffs[i]);

    const char *paloff = buffer+length-768;
    int32_t pal[256];

    for (bssize_t i=0; i<256; i++)
    {
        const char *c = &paloff[i*3];
//#if B_BIG_ENDIAN != 0
        pal[i] = B_LITTLE32((c[0]<<18) + (c[1]<<10) + (c[2]<<2) + (i<<24));
//#endif
    }

    alloc_vbit();

    for (vcolhashsizm1=4096; vcolhashsizm1<(mip1leng>>1); vcolhashsizm1<<=1)
    {
        /* do nothing */
    }
    vcolhashsizm1--; //approx to numvoxs!
    alloc_vcolhashead();

    const char *cptr = buffer+28+((voxsiz.x+1)<<2)+((ysizp1*voxsiz.x)<<1);

    for (bssize_t x=0; x<voxsiz.x; x++) //Set surface voxels to 1 else 0
        for (bssize_t y=0, j=x*yzsiz; y<voxsiz.y; y++, j+=voxsiz.z)
        {
            i = xyoffs[x*ysizp1+y+1] - xyoffs[x*ysizp1+y];
            if (!i)
                continue;

            int32_t z1 = 0;

            while (i)
            {
                const int32_t z0 = cptr[0];
                const int32_t k = cptr[1];
                cptr += 3;

                if (!(cptr[-1]&16))
                    setzrange1(vbit, j+z1, j+z0);

                i -= k+3;
                z1 = z0+k;

                setzrange1(vbit, j+z0, j+z1);  // PK: oob in AMC TC dev if vbit alloc'd w/o +1

                for (bssize_t z=z0; z<z1; z++)
                    putvox(x, y, z, pal[*cptr++]);
            }
        }

    voxmodel_t *const vm = vox2poly();

    if (vm)
    {
        vm->mdnum = 1; //VOXel model id
        vm->scale = vm->bscale = 1.f;
        vm->siz.x = voxsiz.x; vm->siz.y = voxsiz.y; vm->siz.z = voxsiz.z;
        vm->piv.x = voxpiv.x; vm->piv.y = voxpiv.y; vm->piv.z = voxpiv.z;
        vm->is8bit = 1;

        vm->texid = (rhiTexture*)Xcalloc(MAXPALOOKUPS, sizeof(rhiTexture));
    }

    DO_FREE_AND_NULL(shcntmal);
    DO_FREE_AND_NULL(vbit);
    DO_FREE_AND_NULL(vcol);
    vnum = vmax = 0;
    DO_FREE_AND_NULL(vcolhashead);
    Xfree(buffer);

    return vm;
}

//Draw voxel model as perfect cubes
int32_t polymost_voxdraw(voxmodel_t *m, tspriteptr_t const tspr)
{
    // float clut[6] = {1.02,1.02,0.94,1.06,0.98,0.98};
    float f, g, k0, zoff;

    if ((intptr_t)m == (intptr_t)(-1)) // hackhackhack
        return 0;

    if (tspr->cstat & CSTAT_SPRITE_ALIGNMENT_FLOOR)
        return 0;

    buildgl_outputDebugMessage(3, "polymost_voxdraw(m:%p, tspr:%p)", m, tspr);

    //updateanimation((md2model *)m,tspr);

    auto const tsprflags = tspr->clipdist;

    vec3f_t m0 = { m->scale, m->scale, m->scale };
    vec3f_t a0 = { 0, 0, m->zadd*m->scale };

    k0 = m->bscale / 64.f;
    f = (float) tspr->xrepeat * (256.f/320.f) * k0;
    if ((sprite[tspr->owner].cstat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_WALL)
        f *= 1.25f;
    a0.y -= tspr->xoffset*sintable[(spriteext[tspr->owner].mdangoff+512)&2047]*(1.f/(64.f*16384.f));
    a0.x += tspr->xoffset*sintable[(spriteext[tspr->owner].mdangoff)&2047]*(1.f/(64.f*16384.f));

    if (globalorientation&8) { m0.z = -m0.z; a0.z = -a0.z; } //y-flipping
    if (globalorientation&4) { m0.x = -m0.x; a0.x = -a0.x; a0.y = -a0.y; } //x-flipping

    m0.x *= f; a0.x *= f; f = -f;
    m0.y *= f; a0.y *= f;
    f = (float) tspr->yrepeat * k0;
    m0.z *= f; a0.z *= f;

    k0 = (float) (tspr->z+spriteext[tspr->owner].mdposition_offset.z);
    f = ((globalorientation&8) && (sprite[tspr->owner].cstat & CSTAT_SPRITE_ALIGNMENT) != CSTAT_SPRITE_ALIGNMENT_FACING) ? -4.f : 4.f;
    k0 -= (tspr->yoffset*tspr->yrepeat)*f*m->bscale;
    zoff = m->siz.z*.5f;
    if (!(tspr->cstat&128))
        zoff += m->piv.z;
    else if (!(tsprflags & TSPR_FLAGS_SLAB))
    {
        zoff += m->piv.z;
        zoff -= m->siz.z*.5f;
    }
    if (globalorientation&8) zoff = m->siz.z-zoff;

    f = (65536.f*512.f) / ((float)xdimen*viewingrange);
    g = 32.f / ((float)xdimen*gxyaspect);

    int const shadowHack = !!(tsprflags & TSPR_FLAGS_MDHACK);

    m0.y *= f; a0.y = (((float)(tspr->x+spriteext[tspr->owner].mdposition_offset.x-globalposx)) * (1.f/1024.f) + a0.y) * f;
    m0.x *=-f; a0.x = (((float)(tspr->y+spriteext[tspr->owner].mdposition_offset.y-globalposy)) * -(1.f/1024.f) + a0.x) * -f;
    m0.z *= g; a0.z = (((float)(k0     -globalposz - shadowHack)) * -(1.f/16384.f) + a0.z) * g;

    float mat[16];
    md3_vox_calcmat_common(tspr, &a0, f, mat);

    //Mirrors
    if (grhalfxdown10x < 0)
    {
        mat[0] = -mat[0];
        mat[4] = -mat[4];
        mat[8] = -mat[8];
        mat[12] = -mat[12];
    }

    if ((grhalfxdown10x >= 0) ^ ((globalorientation&8) != 0) ^ ((globalorientation&4) != 0))
        glFrontFace(GL_CW);
    else
        glFrontFace(GL_CCW);

    buildgl_setEnabled(GL_CULL_FACE);
    glCullFace(GL_BACK);

    float pc[4];

    pc[0] = pc[1] = pc[2] = ((float)numshades - min(max((globalshade * shadescale) + m->shadeoff, 0.f), (float)numshades)) / (float)numshades;
    polytintflags_t const tintflags = hictinting[globalpal].f;
    if (!(tintflags & HICTINT_PRECOMPUTED))
    {
        if (!(m->flags & 1))
            hictinting_apply(pc, globalpal);
        else globalnoeffect = 1;
    }

    // global tinting
    if (have_basepal_tint())
        hictinting_apply(pc, MAXPALOOKUPS - 1);

    int32_t const voxid = (tsprflags & TSPR_FLAGS_SLAB)
                        ? tspr->picnum
                        : tiletovox[tspr->picnum];
    if (!shadowHack && !(voxflags[voxid] & VF_NOTRANS))
    {
        pc[3] = (tspr->cstat & CSTAT_SPRITE_TRANSLUCENT) ? glblend[tspr->blend].def[!!(tspr->cstat & CSTAT_SPRITE_TRANSLUCENT_INVERT)].alpha : 1.0f;
        pc[3] *= 1.0f - spriteext[tspr->owner].alpha;

        handle_blend(!!(tspr->cstat & CSTAT_SPRITE_TRANSLUCENT), tspr->blend, !!(tspr->cstat & CSTAT_SPRITE_TRANSLUCENT_INVERT));
    }
    else
    {
        pc[3] = 1.f;

        if (shadowHack)
            handle_blend(0, 0, 0);
    }

    if (!(tspr->cstat & CSTAT_SPRITE_TRANSLUCENT) || spriteext[tspr->owner].alpha > 0.f || pc[3] < 1.0f)
        buildgl_setEnabled(GL_BLEND);
    else buildgl_setDisabled(GL_BLEND);

    //transform to Build coords
    float omat[16];
    Bmemcpy(omat, mat, sizeof(omat));

    f = 1.f/64.f;
    g = m0.x*f; mat[0] *= g; mat[1] *= g; mat[2] *= g;
    g = m0.y*f; mat[4] = omat[8]*g; mat[5] = omat[9]*g; mat[6] = omat[10]*g;
    g =-m0.z*f; mat[8] = omat[4]*g; mat[9] = omat[5]*g; mat[10] = omat[6]*g;
    //
    mat[12] -= (m->piv.x*mat[0] + m->piv.y*mat[4] + zoff*mat[8]);
    mat[13] -= (m->piv.x*mat[1] + m->piv.y*mat[5] + zoff*mat[9]);
    mat[14] -= (m->piv.x*mat[2] + m->piv.y*mat[6] + zoff*mat[10]);
    //
    glMatrixMode(GL_MODELVIEW); //Let OpenGL (and perhaps hardware :) handle the matrix rotation
    mat[3] = mat[7] = mat[11] = 0.f; mat[15] = 1.f;

    glLoadMatrixf(mat);

    char prevClamp = polymost_getClamp();
    polymost_setClamp(0);
    polymost_setFogEnabled(false);

    if (m->is8bit && polymost_useindexedtextures())
    {
        if (!m->texid8bit)
            m->texid8bit = gloadtex_indexed(m->mytex, m->mytexx, m->mytexy);
        else
            rhi->texture_bind(m->texid8bit);

        //buildgl_bindSamplerObject(0, PTH_INDEXED);
        
        float yp = (tspr->x-globalposx)*gcosang2+(tspr->y-globalposy)*gsinang2;
        int visShade = int(fabsf(yp*float(globvis2)*float(xdimscale)*(1.f/(256.f*128.f*65536.f))));

        if (polymost_usetileshades())
        {
            globalshade = max(min(globalshade+visShade, numshades-1), 0);
            pc[0] = pc[1] = pc[2] = 1.f;
        }
        polymost_usePaletteIndexing(true);
        polymost_updatePalette();
        polymost_setTexturePosSize({ 0.f, 0.f, 1.f, 1.f });
        polymost_setHalfTexelSize({ 0.f, 0.f });
        polymost_setVisibility(0.f);
    }
    else
    {
        if (!m->texid[globalpal])
            m->texid[globalpal] = gloadtex(m->mytex, m->mytexx, m->mytexy, m->is8bit, globalpal);
        else
            rhi->texture_bind(m->texid[globalpal]);

        // buildgl_bindSamplerObject(0, PTH_CLAMPED);

        polymost_usePaletteIndexing(false);
        polymost_setTexturePosSize({ 0.f, 0.f, 1.f, 1.f });
    }

    glColor4f(pc[0], pc[1], pc[2], pc[3]);
    buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->vboindex);
    buildgl_bindBuffer(GL_ARRAY_BUFFER, m->vbo);

    glVertexPointer(3, GL_FLOAT, 5*sizeof(float), 0);

    glClientActiveTexture(GL_TEXTURE0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 5*sizeof(float), (GLvoid*) (3*sizeof(float)));

    glDrawElements(GL_TRIANGLES, m->qcnt*2*3, GL_UNSIGNED_INT, 0);

    buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);
    buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    polymost_setClamp(prevClamp);
    polymost_usePaletteIndexing(true);
    if (!nofog) polymost_setFogEnabled(true);
    polymost_resetVertexPointers();

    //------------
    buildgl_setDisabled(GL_CULL_FACE);
    glLoadIdentity();

    globalnoeffect = 0;

    return 1;
}
#endif

//---------------------------------------- VOX LIBRARY ENDS ----------------------------------------
