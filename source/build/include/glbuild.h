
#ifndef BGLBUILD_H_INCLUDED_
#define BGLBUILD_H_INCLUDED_

#include "baselayer.h"
#include "build.h"
#include "compat.h"
#include "glad/glad.h"
#include "hash.h"

#ifdef USE_OPENGL

#if !defined GEKKO && !defined EDUKE32_GLES
# define DYNAMIC_GL
# define DYNAMIC_GLEXT
# define USE_GLEXT
#endif

# ifdef _WIN32
#  define PR_CALLBACK __stdcall
# else
#  define PR_CALLBACK
# endif

#define MAXTEXUNIT GL_TEXTURE16

#if 0
enum glsamplertype
{
    SAMPLER_INVALID = -1,
    SAMPLER_NONE,
    SAMPLER_NEAREST_CLAMP,
    SAMPLER_NEAREST_WRAP,
    SAMPLER_LINEAR_CLAMP,
    SAMPLER_LINEAR_WRAP,
    SAMPLER_NEAREST_NEAREST_CLAMP,
    SAMPLER_NEAREST_NEAREST_WRAP,
    SAMPLER_CLAMP,
    SAMPLER_WRAP_T,
    SAMPLER_WRAP_S,
    SAMPLER_WRAP_BOTH,
    SAMPLER_DEPTH,
    NUM_SAMPLERS
};

enum glsamplerflags {
    SAMPLER_NEAREST,
    SAMPLER_CLAMPED,
};
#endif

struct BuildGLState
{
    GLuint currentShaderProgramID;
    GLenum currentActiveTexture;

    // glsamplertype currentBoundSampler[MAXTEXUNIT - GL_TEXTURE0];

    GLint x, y;
    GLsizei width, height;

    inthashtable_t state[MAXTEXUNIT - GL_TEXTURE0];

    int fullReset;
};

extern BuildGLState gl;
//extern GLuint samplerObjectIDs[NUM_SAMPLERS];

#define TEXUNIT_INDEX_FROM_NAME(x) (x - GL_TEXTURE0)
#define ACTIVETEX (gl.currentActiveTexture ? TEXUNIT_INDEX_FROM_NAME(gl.currentActiveTexture) : 0)

#ifndef EDUKE32_GLES
# define buildgl_debugMessageCallback glDebugMessageCallbackARB
# define buildgl_debugMessageControl glDebugMessageControlARB
# define buildgl_debugMessageInsert glDebugMessageInsertARB
#else
# define buildgl_debugMessageCallback glDebugMessageCallbackKHR
# define buildgl_debugMessageControl glDebugMessageControlKHR
# define buildgl_debugMessageInsert glDebugMessageInsertKHR
#endif

extern void buildgl_activeTexture(GLenum texture);
extern void buildgl_bindBuffer(GLenum target, uint32_t bufferID);
//extern void buildgl_bindSamplerObject(int texunit, int32_t pth_method);
extern void buildgl_bindTexture(GLenum target, uint32_t textureID);
extern void buildgl_outputDebugMessage(uint8_t severity, const char *format, ...);
//extern void buildgl_resetSamplerObjects(void);
extern void buildgl_resetStateAccounting(void);
extern void buildgl_setAlphaFunc(GLenum func, GLfloat ref);
extern void buildgl_setDepthFunc(GLenum func);
extern void buildgl_setDisabled(GLenum key);
extern void buildgl_setEnabled(GLenum key);
extern void buildgl_setPerspective(float fovy, float aspect, float zNear, float zFar);
extern void buildgl_setViewport(GLint x, GLint y, GLsizei width, GLsizei height);
extern void buildgl_uLookAt(vec3f_t v_eye, vec3f_t v_center, vec3f_t v_up);
extern void buildgl_useShaderProgram(uint32_t shaderID);

extern void bind_2d_texture(GLuint texture, int filter);

static FORCE_INLINE void  buildgl_crossproduct(const GLfloat* in_a, const GLfloat* in_b, GLfloat* out)
{
    out[0] = in_a[1] * in_b[2] - in_a[2] * in_b[1];
    out[1] = in_a[2] * in_b[0] - in_a[0] * in_b[2];
    out[2] = in_a[0] * in_b[1] - in_a[1] * in_b[0];
}

static FORCE_INLINE void  buildgl_normalize(float* vec)
{
    float norm = vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];

    norm = 1.f/Bsqrtf(norm);

    vec[0] *= norm;
    vec[1] *= norm;
    vec[2] *= norm;
}

extern int  buildgl_unprojectMatrixToViewport(vec3f_t win, const float *modelMatrix, const float *projMatrix, const int *viewport, float *objx, float *objy,
                                              float *objz);

extern int32_t r_usesamplerobjects; // FIXME: nasty circular include dependency issue
//static FORCE_INLINE bool buildgl_samplerObjectsEnabled(void) { return glinfo.samplerobjects && r_usesamplerobjects; }

//////// dynamic/static API wrapping ////////

#if !defined RENDERTYPESDL && defined _WIN32 && defined DYNAMIC_GL
typedef HGLRC (WINAPI * bwglCreateContextProcPtr)(HDC);
extern bwglCreateContextProcPtr bwglCreateContext;
#define wglCreateContext bwglCreateContext
typedef BOOL (WINAPI * bwglDeleteContextProcPtr)(HGLRC);
extern bwglDeleteContextProcPtr bwglDeleteContext;
#define wglDeleteContext bwglDeleteContext
typedef PROC (WINAPI * bwglGetProcAddressProcPtr)(LPCSTR);
extern bwglGetProcAddressProcPtr bwglGetProcAddress;
#define wglGetProcAddress bwglGetProcAddress
typedef BOOL (WINAPI * bwglMakeCurrentProcPtr)(HDC,HGLRC);
extern bwglMakeCurrentProcPtr bwglMakeCurrent;
#define wglMakeCurrent bwglMakeCurrent

typedef int32_t (WINAPI * bwglChoosePixelFormatProcPtr)(HDC,CONST PIXELFORMATDESCRIPTOR*);
extern bwglChoosePixelFormatProcPtr bwglChoosePixelFormat;
#define wglChoosePixelFormat bwglChoosePixelFormat
typedef int32_t (WINAPI * bwglDescribePixelFormatProcPtr)(HDC,int32_t,UINT,LPPIXELFORMATDESCRIPTOR);
extern bwglDescribePixelFormatProcPtr bwglDescribePixelFormat;
#define wglDescribePixelFormat bwglDescribePixelFormat
typedef int32_t (WINAPI * bwglGetPixelFormatProcPtr)(HDC);
extern bwglGetPixelFormatProcPtr bwglGetPixelFormat;
#define wglGetPixelFormat bwglGetPixelFormat
typedef BOOL (WINAPI * bwglSetPixelFormatProcPtr)(HDC,int32_t,const PIXELFORMATDESCRIPTOR*);
extern bwglSetPixelFormatProcPtr bwglSetPixelFormat;
#define wglSetPixelFormat bwglSetPixelFormat
#endif

//////// glGenTextures/glDeleteTextures debugging ////////
void texdbg_bglGenTextures(GLsizei n, GLuint *textures, const char *srcfn);
void texdbg_bglDeleteTextures(GLsizei n, const GLuint *textures, const char *srcfn);

//#define DEBUG_TEXTURE_NAMES

#if defined DEBUGGINGAIDS && defined DEBUG_TEXTURE_NAMES
# define glGenTextures(numtexs, texnamear) texdbg_bglGenTextures(numtexs, texnamear, __FILE__)
# define glDeleteTextures(numtexs, texnamear) texdbg_bglDeleteTextures(numtexs, texnamear, __FILE__)
#endif
#endif //USE_OPENGL

#if !defined RENDERTYPESDL && defined _WIN32 && defined DYNAMIC_GL
extern char *gldriver;

int32_t loadwgl(const char *driver);
int32_t unloadwgl(void);
#endif

#endif
