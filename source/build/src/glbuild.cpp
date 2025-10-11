/*
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 */

#include "glbuild.h"

#include "baselayer.h"
#include "build.h"

#if defined USE_OPENGL
# ifdef RENDERTYPESDL
#  include "sdlayer.h"
# endif
# if !defined _WIN32
#  include <dlfcn.h>
# endif

BuildGLState gl;
//GLuint samplerObjectIDs[NUM_SAMPLERS];

void buildgl_outputDebugMessage(uint8_t severity, const char* format, ...)
{
    if (!glinfo.debugoutput || r_polymostDebug < severity)
        return;

    size_t size = max(PRINTF_INITIAL_BUFFER_SIZE >> 1, nextPow2(Bstrlen(format)));
    char *buf = nullptr;
    int len;

    do
    {
        va_list va;
        buf = (char *)Xrealloc(buf, (size <<= 1));
        va_start(va, format);
        len = Bvsnprintf(buf, size, format, va);
        va_end(va);
    } while ((unsigned)len >= size);

    buildgl_debugMessageInsert(GL_DEBUG_SOURCE_APPLICATION_ARB,
                               GL_DEBUG_TYPE_OTHER_ARB,
                               0,
                               GL_DEBUG_SEVERITY_HIGH_ARB+severity-1,
                               -1,
                               buf);
    Xfree(buf);
}


void buildgl_resetStateAccounting()
{
    for (auto i=GL_TEXTURE0;i<MAXTEXUNIT;i++)
    {
        //buildgl_bindSamplerObject(TEXUNIT_INDEX_FROM_NAME(i), 0);
        inthash_free(&gl.state[TEXUNIT_INDEX_FROM_NAME(i)]);
    }

    Bmemset(&gl, 0, sizeof(BuildGLState));

    for (auto i=GL_TEXTURE0;i<MAXTEXUNIT;i++)
    {
        //gl.currentBoundSampler[TEXUNIT_INDEX_FROM_NAME(i)] = SAMPLER_INVALID;
        gl.state[TEXUNIT_INDEX_FROM_NAME(i)].count = 64;
        inthash_init(&gl.state[TEXUNIT_INDEX_FROM_NAME(i)]);
    }

    gl.fullReset = 1;
}

void buildgl_setViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (x == gl.x && y == gl.y && width == gl.width && height == gl.height)
        return;

    gl.x = x;
    gl.y = y;
    gl.width = width;
    gl.height = height;

    glViewport(x, y, width, height);
}

void buildgl_setDepthFunc(GLenum func)
{
    if ((GLenum)inthash_find(&gl.state[0], GL_DEPTH_FUNC) == func)
        return;

    glDepthFunc(func);
    inthash_add(&gl.state[0], GL_DEPTH_FUNC, func, 1);
}

void buildgl_setAlphaFunc(GLenum func, GLfloat ref)
{
    if ((GLenum)inthash_find(&gl.state[0], GL_ALPHA_TEST_FUNC) == func && inthash_find(&gl.state[0], GL_ALPHA_TEST_REF) == *(int32_t *)&ref)
        return;

    glAlphaFunc(func, ref);
    inthash_add(&gl.state[0], GL_ALPHA_TEST_FUNC, func, 1);
    inthash_add(&gl.state[0], GL_ALPHA_TEST_REF, *(int32_t *)&ref, 1);
}

void buildgl_setEnabled(GLenum key)
{
    if (inthash_find(&gl.state[0], key) == GL_TRUE)
        return;

    glEnable(key);

    inthash_add(&gl.state[0], key, GL_TRUE, 1);
}

void buildgl_setDisabled(GLenum key)
{
    if (inthash_find(&gl.state[0], key) == GL_FALSE)
        return;

    glDisable(key);

    inthash_add(&gl.state[0], key, GL_FALSE, 1);
}

void buildgl_useShaderProgram(uint32_t shaderID)
{
    glUseProgram(shaderID);
    gl.currentShaderProgramID = shaderID;
}

//POGOTODO: these wrappers won't be needed down the line -- remove them once proper draw call organization is finished
void buildgl_activeTexture(GLenum texture)
{
    if (gl.currentActiveTexture != texture)
    {
        gl.currentActiveTexture = texture;
        glActiveTexture(texture);
    }
}

void buildgl_bindBuffer(GLenum target, uint32_t bufferID)
{
    if ((uint32_t)inthash_find(&gl.state[ACTIVETEX], target) == bufferID)
        return;

    glBindBuffer(target, bufferID);

    if (bufferID == 0)
        inthash_delete(&gl.state[ACTIVETEX], target);
    else
        inthash_add(&gl.state[ACTIVETEX], target, bufferID, 1);
}

//POGOTODO: replace this and buildgl_activeTexture with proper draw call organization
void buildgl_bindTexture(GLenum target, uint32_t textureID)
{
    if (/*textureID == 0 ||*/
        /*gl.currentActiveTexture != GL_TEXTURE0 ||*/
        (uint32_t)inthash_find(&gl.state[ACTIVETEX], target) != textureID /*||
        videoGetRenderMode() != REND_POLYMOST*/)
    {
        glBindTexture(target, textureID);
//        if (gl.currentActiveTexture == GL_TEXTURE0)
        {
            if (textureID == 0)
                inthash_delete(&gl.state[ACTIVETEX], target);
            else
                inthash_add(&gl.state[ACTIVETEX], target, textureID, 1);
        }
    }
}

void buildgl_resetSamplerObjects(void)
{
#if 0
    glanisotropy    = clamp<int>(glanisotropy, 1, glinfo.maxanisotropy);
    gltexfiltermode = clamp(gltexfiltermode, 0, NUMGLFILTERMODES-1);

    if (!glinfo.samplerobjects)
        return;

    auto &f = glfiltermodes[gltexfiltermode];

    if (!glIsSampler(samplerObjectIDs[1]))
        glGenSamplers(NUM_SAMPLERS-1, &samplerObjectIDs[1]);

    // common properties
    for (int i = 1; i < ARRAY_SSIZE(samplerObjectIDs); i++)
    {
        auto &s = samplerObjectIDs[i];

        glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, f.mag);
        glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, f.min);
        //glSamplerParameteri(s, GL_TEXTURE_BASE_LEVEL, 0);
        //glSamplerParameteri(s, GL_TEXTURE_MAX_LEVEL, 0);
        glSamplerParameterf(s, GL_TEXTURE_MAX_ANISOTROPY_EXT, polymost_getanisotropy());
    }

    GLuint s = samplerObjectIDs[SAMPLER_CLAMP];
    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    s = samplerObjectIDs[SAMPLER_WRAP_T];
    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

    s = samplerObjectIDs[SAMPLER_WRAP_S];
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    s = samplerObjectIDs[SAMPLER_NEAREST_CLAMP];
    glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);

    s = samplerObjectIDs[SAMPLER_NEAREST_WRAP];
    glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(s, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);

    s = samplerObjectIDs[SAMPLER_LINEAR_CLAMP];
    glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    s = samplerObjectIDs[SAMPLER_LINEAR_WRAP];
    glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    s = samplerObjectIDs[SAMPLER_NEAREST_NEAREST_CLAMP];
    glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);

    s = samplerObjectIDs[SAMPLER_NEAREST_NEAREST_WRAP];
    glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glSamplerParameteri(s, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);

    s = samplerObjectIDs[SAMPLER_DEPTH];
    glSamplerParameteri(s, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    glSamplerParameteri(s, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
#endif
}

#if 0
void buildgl_bindSamplerObject(int texunit, int32_t pth_method)
{
    if (!buildgl_samplerObjectsEnabled())
    {
        gl.currentBoundSampler[texunit] = SAMPLER_NONE;

        if (glinfo.samplerobjects)
            glBindSampler(texunit, 0);

        return;
    }

    glsamplertype samplerid = SAMPLER_NEAREST_WRAP;

    switch (pth_method & (PTH_INDEXED|PTH_CLAMPED|PTH_HIGHTILE|PTH_FORCEFILTER|PTH_TEMP_SKY_HACK|PTH_DEPTH_SAMPLER))
    {
        case PTH_DEPTH_SAMPLER:
            samplerid = SAMPLER_DEPTH;
            break;
        case PTH_TEMP_SKY_HACK:
            samplerid = SAMPLER_WRAP_T;
            break;
        case PTH_INDEXED:
            samplerid = videoGetRenderMode() == REND_POLYMER ? SAMPLER_NEAREST_WRAP : SAMPLER_NEAREST_NEAREST_WRAP;
            break;
        case PTH_INDEXED|PTH_CLAMPED:
        case PTH_CLAMPED:
            samplerid = videoGetRenderMode() == REND_POLYMER ? SAMPLER_NEAREST_CLAMP : SAMPLER_NEAREST_NEAREST_CLAMP;
            break;
        case PTH_HIGHTILE:
            samplerid = SAMPLER_WRAP_BOTH;
            break;
        case PTH_HIGHTILE|PTH_CLAMPED:
            samplerid = SAMPLER_CLAMP;
            break;
        case PTH_HIGHTILE|PTH_FORCEFILTER:
            samplerid = SAMPLER_LINEAR_WRAP;
            break;
        case PTH_HIGHTILE|PTH_FORCEFILTER|PTH_CLAMPED:
            samplerid = SAMPLER_LINEAR_CLAMP;
            break;
        case 0:
            samplerid = SAMPLER_NONE;
            break;
    }

    if (gl.currentBoundSampler[texunit] != samplerid)
    {
        gl.currentBoundSampler[texunit] = samplerid;
        glBindSampler(texunit, samplerObjectIDs[samplerid]);
    }
}
#endif

static inline void buildgl_multMatrix4f(const float *a, const float *b, float *r)
{
    for (int i=0, j; i<4; i++)
    {
        for (j=0; j<4; j++)
        {
            r[(i<<2)+j] = a[(i<<2)+0] * b[j]
                        + a[(i<<2)+1] * b[4+j]
                        + a[(i<<2)+2] * b[8+j]
                        + a[(i<<2)+3] * b[12+j];
        }
    }
}

static int buildgl_invertMatrix4f(const float *m, float *invOut)
{
    float const inv[16] = { m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
                          + m[9]*m[7]*m[14]  + m[13]*m[6]*m[11] - m[13]*m[7]*m[10],
                           -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
                          - m[9]*m[3]*m[14]  - m[13]*m[2]*m[11] + m[13]*m[3]*m[10],
                            m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15]
                          + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6],
                           -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11]
                          - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]  + m[9]*m[3]*m[6],
                           -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
                          - m[8]*m[7]*m[14]  - m[12]*m[6]*m[11] + m[12]*m[7]*m[10],
                            m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
                          + m[8]*m[3]*m[14]  + m[12]*m[2]*m[11] - m[12]*m[3]*m[10],
                           -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15]
                          - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6],
                            m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11]
                          + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]  - m[8]*m[3]*m[6],
                            m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
                          + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9],
                           -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
                          - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9],
                            m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15]
                          + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5],
                           -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11]
                          - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7] + m[8]*m[3]*m[5],
                           -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
                          - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9],
                            m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
                          + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9],
                           -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14]
                          - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5],
                            m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10]
                          + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6] - m[8]*m[2]*m[5]};

    float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];

    if (!det)
        return 0;

    det = 1.f/det;

    for (int i = 0; i < 16; i++)
        invOut[i] = inv[i] * det;

    return 1;
}

static inline void buildgl_multMatrixVec4f(const float *matrix, const float *in, float *out)
{
    for (int i=0; i<4; i++)
    {
        out[i] = in[0] * matrix[0*4+i]
               + in[1] * matrix[1*4+i]
               + in[2] * matrix[2*4+i]
               + in[3] * matrix[3*4+i];
    }
}

int buildgl_unprojectMatrixToViewport(vec3f_t win, const float *modelMatrix, const float *projMatrix, const int *viewport, float *objx, float *objy, float *objz)
{
    float finalMatrix[16];
    buildgl_multMatrix4f(modelMatrix, projMatrix, finalMatrix);

    if (!buildgl_invertMatrix4f(finalMatrix, finalMatrix))
        return 0;

    float in[4] = { win.x, win.y, win.z, 1.0 };

    /* Map x and y from window coordinates */
    in[0] = (in[0] - viewport[0]) / viewport[2];
    in[1] = (in[1] - viewport[1]) / viewport[3];

    /* Map to range -1 to 1 */
    in[0] = in[0] * 2 - 1;
    in[1] = in[1] * 2 - 1;
    in[2] = in[2] * 2 - 1;

    float out[4];
    buildgl_multMatrixVec4f(finalMatrix, in, out);

    if (!out[3])
        return 0;

    out[0] /= out[3];
    out[1] /= out[3];
    out[2] /= out[3];

    *objx = out[0];
    *objy = out[1];
    *objz = out[2];

    return 1;
}

void buildgl_setPerspective(float fovy, float aspect, float zNear, float zFar)
{
    float const deltaZ = zFar-zNear;
    float const sine   = sinf((fovy *= .5f*fPI*(1.f/180.f)));

    if (!deltaZ || !sine || !aspect)
        return;

    float const cotangent = cosf(fovy)/sine;

    float m[4][4];
    Bmemset(m,0,sizeof(m));

    float const rdeltaZ = 1.f/deltaZ;

    m[0][0] = cotangent/aspect;
    m[1][1] = cotangent;
    m[2][2] = -(zFar+zNear)*rdeltaZ;
    m[2][3] = -1;
    m[3][2] = -2*zNear*zFar*rdeltaZ;

    glMultMatrixf(&m[0][0]);
}

void buildgl_uLookAt(vec3f_t v_eye, vec3f_t v_center, vec3f_t v_up)
{
    float up[3]      = { v_up.x, v_up.y, v_up.z };
    float forward[3] = { v_center.x - v_eye.x,
                         v_center.y - v_eye.y,
                         v_center.z - v_eye.z };

    buildgl_normalize(forward);

    float side[3];
    /* Side = forward x up */
    buildgl_crossproduct(forward, up, side);
    buildgl_normalize(side);

    /* Recompute up as: up = side x forward */
    buildgl_crossproduct(side, forward, up);

    GLfloat m[4][4];
    Bmemset(m, 0, sizeof(m));

    m[0][0] = side[0];
    m[1][0] = side[1];
    m[2][0] = side[2];

    m[0][1] = up[0];
    m[1][1] = up[1];
    m[2][1] = up[2];

    m[0][2] = -forward[0];
    m[1][2] = -forward[1];
    m[2][2] = -forward[2];

    m[3][3] = 1;

    glMultMatrixf(&m[0][0]);
    glTranslatef(-v_eye.x, -v_eye.y, -v_eye.z);
}

#if defined DYNAMIC_GL || defined DYNAMIC_GLEXT
# if !defined RENDERTYPESDL && defined _WIN32
bwglCreateContextProcPtr bwglCreateContext;
bwglDeleteContextProcPtr bwglDeleteContext;
bwglGetProcAddressProcPtr bwglGetProcAddress;
bwglMakeCurrentProcPtr bwglMakeCurrent;

bwglChoosePixelFormatProcPtr bwglChoosePixelFormat;
bwglDescribePixelFormatProcPtr bwglDescribePixelFormat;
bwglGetPixelFormatProcPtr bwglGetPixelFormat;
bwglSetPixelFormatProcPtr bwglSetPixelFormat;

static HMODULE hGLDLL;
char *gldriver = NULL;

static void *getproc_(const char *s, int32_t *err, int32_t fatal, int32_t extension)
{
    void *t;
#if defined RENDERTYPESDL
    UNREFERENCED_PARAMETER(extension);
    t = (void *)SDL_GL_GetProcAddress(s);
#elif defined _WIN32
    if (extension) t = (void *)bwglGetProcAddress(s);
    else t = (void *)GetProcAddress(hGLDLL,s);
#else
#error Need a dynamic loader for this platform...
#endif
    if (!t && fatal)
    {
        LOG_F(ERROR, "Failed to find %s in %s", s, gldriver);
        *err = 1;
    }
    return t;
}
#define GETPROC(s)        getproc_(s,&err,1,0)

int32_t loadwgl(const char *driver)
{
    int32_t err=0;

    if (hGLDLL) return 0;

    if (!driver)
    {
        driver = "opengl32.dll";
    }

    hGLDLL = LoadLibrary(driver);
    if (!hGLDLL)
    {
        LOG_F(ERROR, "Failed loading \"%s\"", driver);
        return -1;
    }

    gldriver = Xstrdup(driver);

    bwglCreateContext = (bwglCreateContextProcPtr) GETPROC("wglCreateContext");
    bwglDeleteContext = (bwglDeleteContextProcPtr) GETPROC("wglDeleteContext");
    bwglGetProcAddress = (bwglGetProcAddressProcPtr) GETPROC("wglGetProcAddress");
    bwglMakeCurrent = (bwglMakeCurrentProcPtr) GETPROC("wglMakeCurrent");

    bwglChoosePixelFormat = (bwglChoosePixelFormatProcPtr) GETPROC("wglChoosePixelFormat");
    bwglDescribePixelFormat = (bwglDescribePixelFormatProcPtr) GETPROC("wglDescribePixelFormat");
    bwglGetPixelFormat = (bwglGetPixelFormatProcPtr) GETPROC("wglGetPixelFormat");
    bwglSetPixelFormat = (bwglSetPixelFormatProcPtr) GETPROC("wglSetPixelFormat");

    if (err) unloadwgl();
    return err;
}
int32_t unloadwgl(void)
{
    if (!hGLDLL) return 0;

    DO_FREE_AND_NULL(gldriver);

    FreeLibrary(hGLDLL);
    hGLDLL = NULL;

    bwglCreateContext = (bwglCreateContextProcPtr) NULL;
    bwglDeleteContext = (bwglDeleteContextProcPtr) NULL;
    bwglGetProcAddress = (bwglGetProcAddressProcPtr) NULL;
    bwglMakeCurrent = (bwglMakeCurrentProcPtr) NULL;

    bwglChoosePixelFormat = (bwglChoosePixelFormatProcPtr) NULL;
    bwglDescribePixelFormat = (bwglDescribePixelFormatProcPtr) NULL;
    bwglGetPixelFormat = (bwglGetPixelFormatProcPtr) NULL;
    bwglSetPixelFormat = (bwglSetPixelFormatProcPtr) NULL;
    return 0;
}
# endif
#endif


//////// glGenTextures/glDeleteTextures debugging ////////
# if defined DEBUGGINGAIDS && defined DEBUG_TEXTURE_NAMES
static uint8_t *texnameused;  // bitmap
static uint32_t *texnamefromwhere;  // hash of __FILE__
static uint32_t texnameallocsize;

// djb3 algorithm
static inline uint32_t texdbg_getcode(const char *s)
{
    uint32_t h = 5381;
    int32_t ch;

    while ((ch = *s++) != '\0')
        h = ((h << 5) + h) ^ ch;

    return h;
}

static void texdbg_realloc(uint32_t maxtexname)
{
    uint32_t newsize = texnameallocsize ? texnameallocsize : 64;

    if (maxtexname < texnameallocsize)
        return;

    while (maxtexname >= newsize)
        newsize <<= 1;
//    initprintf("texdebug: new size %u\n", newsize);

    texnameused = Xrealloc(texnameused, newsize>>3);
    texnamefromwhere = Xrealloc(texnamefromwhere, newsize*sizeof(uint32_t));

    Bmemset(texnameused + (texnameallocsize>>3), 0, (newsize-texnameallocsize)>>3);
    Bmemset(texnamefromwhere + texnameallocsize, 0, (newsize-texnameallocsize)*sizeof(uint32_t));

    texnameallocsize = newsize;
}

#undef bglGenTextures
void texdbg_bglGenTextures(GLsizei n, GLuint *textures, const char *srcfn)
{
    int32_t i;
    uint32_t hash = srcfn ? texdbg_getcode(srcfn) : 0;

    for (i=0; i<n; i++)
    {
        GLuint const t = textures[i];
        if (t < texnameallocsize && bitmap_test(texnameused, t))
            DLOG_F(INFO, "texdebug %x Gen: overwriting used tex name %u from %x",
                         hash, t, texnamefromwhere[t]);
    }

    bglGenTextures(n, textures);

    {
        GLuint maxtexname = 0;

        for (i=0; i<n; i++)
            maxtexname = max(maxtexname, textures[i]);

        texdbg_realloc(maxtexname);

        for (i=0; i<n; i++)
        {
            GLuint const t = textures[i];
            bitmap_set(texnameused, t);
            texnamefromwhere[t] = hash;
        }
    }
}

#undef bglDeleteTextures
void texdbg_bglDeleteTextures(GLsizei n, const GLuint *textures, const char *srcfn)
{
    int32_t i;
    uint32_t hash = srcfn ? texdbg_getcode(srcfn) : 0;

    for (i=0; i<n; i++)
    {
        GLuint const t = textures[i];
        if (t < texnameallocsize)
        {
            if (!bitmap_test(texnameused, t))
                DLOG_F(INFO, "texdebug %x Del: deleting unused tex name %u", hash, t);
            else if (bitmap_test(texnameused, t) && texnamefromwhere[t] != hash)
                DLOG_F(INFO, "texdebug %x Del: deleting foreign tex name %u from %x",
                             hash, t, texnamefromwhere[t]);
        }
    }

    bglDeleteTextures(n, textures);

    if (texnameallocsize)
        for (i=0; i<n; i++)
        {
            GLuint const t = textures[i];
            bitmap_clear(texnameused, t);
            texnamefromwhere[t] = 0;
        }
}
# endif  // defined DEBUGGINGAIDS

#endif
