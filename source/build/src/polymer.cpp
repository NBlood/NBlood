// blah

#if defined USE_OPENGL && defined POLYMER

#include "compat.h"
#include "common.h"

#define POLYMER_C
#include "polymer.h"
#include "engine_priv.h"
#include "xxhash_config.h"
#include "texcache.h"

#define LIBTESS2_IMPLEMENTATION

#include "libtess2.h"

// CVARS
#ifdef __APPLE__
int32_t pr_ati_textureformat_one = 0;
#endif
int32_t pr_artmapping         = 1;
int32_t pr_ati_fboworkaround  = 0;
int32_t pr_ati_nodepthoffset  = 0;
int32_t pr_billboardingmode   = 1;
int32_t pr_buckets            = 0;
double  pr_customaspect       = 0.0f;
int32_t pr_fov                = 512;
int32_t pr_gpusmoothing       = 1;
int32_t pr_highpalookups      = 1;
int32_t pr_hudangadd          = 0;
int32_t pr_hudfov             = 512;
float   pr_hudxadd            = 0.0f;
float   pr_hudyadd            = 0.0f;
float   pr_hudzadd            = 0.0f;
int32_t pr_lighting           = 1;
int32_t pr_maxlightpasses     = 10;
int32_t pr_maxlightpriority   = PR_MAXLIGHTPRIORITY;
int32_t pr_normalmapping      = 1;
int32_t pr_nullrender = 0;  // 1: no draw, 2: no draw or updates
int32_t pr_overridehud        = 0;
float   pr_overridemodelscale = 0.0f;
int32_t pr_overrideparallax   = 0;
int32_t pr_overridespecular   = 0;
float   pr_parallaxbias       = 0.0f;
float   pr_parallaxscale      = 0.1f;
int32_t pr_shadowcount        = 5;
int32_t pr_shadowdetail       = 4;
int32_t pr_shadowfiltering    = 1;
int32_t pr_shadows            = 1;
float   pr_specularfactor     = 1.0f;
int32_t pr_specularmapping    = 1;
float   pr_specularpower      = 15.0f;
int32_t pr_verbosity          = 1;  // 0: silent, 1: errors and one-times, 2: multiple-times, 3: flood
int32_t pr_wireframe          = 0;
int32_t r_pr_constrained    = 1;
int32_t r_pr_maxlightpasses = 5;  // value of the cvar (not live value), used to detect changes

GLenum mapvbousage   = GL_STREAM_DRAW;
GLenum modelvbousage = GL_STATIC_DRAW;

// BUILD DATA
_prsector *prsectors[MAXSECTORS];
_prwall *  prwalls[MAXWALLS];
_prsprite *prsprites[MAXSPRITES];


_prmaterial     mdspritematerial;
_prhighpalookup *prhighpalookups[MAXBASEPALS];

// One U8 texture per tile
GLuint prartmaps[MAXTILES];
// 256 U8U8U8 values per basepal
GLuint prbasepalmaps[MAXBASEPALS];
// numshades full indirections (32*256) per lookup
GLuint prlookups[MAXPALOOKUPS];

GLuint   prmapvbo;
GLintptr prwalldataoffset;

GLuint   prindexringvbo;
GLuint * prindexring;
GLintptr prindexringoffset;

constexpr const GLsizeiptr proneplanesize      = sizeof(_prvert) * 4;
constexpr const GLintptr   prwalldatasize      = sizeof(_prvert) * 4 * 3;  // wall, over and mask planes for every wall
constexpr const GLsizeiptr prindexringsize     = 65535;
constexpr const GLbitfield prindexringmapflags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

_prbucket *prbuckethead;
int32_t    prcanbucket;

static bool prdidsky;

static inthashtable_t prprogramtable = { nullptr, INTHASH_SIZE(256) };
static GrowArray<_prprograminfo *> prprogramptrs;

static const _prvert  vertsprite[4] =
{
    {
        -0.5f, 0.0f, 0.0f,
        0.0f, 1.0f,
        0xff, 0xff, 0xff, 0xff,
    },
    {
        0.5f, 0.0f, 0.0f,
        1.0f, 1.0f,
        0xff, 0xff, 0xff, 0xff,
    },
    {
        0.5f, 1.0f, 0.0f,
        1.0f, 0.0f,
        0xff, 0xff, 0xff, 0xff,
    },
    {
        -0.5f, 1.0f, 0.0f,
        0.0f, 0.0f,
        0xff, 0xff, 0xff, 0xff,
    },
};

static const _prvert  horizsprite[4] =
{
    {
        -0.5f, 0.0f, 0.5f,
        0.0f, 0.0f,
        0xff, 0xff, 0xff, 0xff,
    },
    {
        0.5f, 0.0f, 0.5f,
        1.0f, 0.0f,
        0xff, 0xff, 0xff, 0xff,
    },
    {
        0.5f, 0.0f, -0.5f,
        1.0f, 1.0f,
        0xff, 0xff, 0xff, 0xff,
    },
    {
        -0.5f, 0.0f, -0.5f,
        0.0f, 1.0f,
        0xff, 0xff, 0xff, 0xff,
    },
};

static const GLfloat  skyboxdata[4 * 5 * 6] =
{
    // -ZY
    -0.5f, -0.5f, 0.5f,
    0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,
    1.0f, 1.0f,
    -0.5f, 0.5f, -0.5f,
    1.0f, 0.0f,
    -0.5f, 0.5f, 0.5f,
    0.0f, 0.0f,

    // XY
    -0.5f, -0.5f, -0.5f,
    0.0f, 1.0f,
    0.5f, -0.5f, -0.5f,
    1.0f, 1.0f,
    0.5f, 0.5f, -0.5f,
    1.0f, 0.0f,
    -0.5f, 0.5f, -0.5f,
    0.0f, 0.0f,

    // ZY
    0.5f, -0.5f, -0.5f,
    0.0f, 1.0f,
    0.5f, -0.5f, 0.5f,
    1.0f, 1.0f,
    0.5f, 0.5f, 0.5f,
    1.0f, 0.0f,
    0.5f, 0.5f, -0.5f,
    0.0f, 0.0f,

    // -XY
    0.5f, -0.5f, 0.5f,
    0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f,
    1.0f, 1.0f,
    -0.5f, 0.5f, 0.5f,
    1.0f, 0.0f,
    0.5f, 0.5f, 0.5f,
    0.0f, 0.0f,

    // XZ
    -0.5f, 0.5f, -0.5f,
    1.0f, 1.0f,
    0.5f, 0.5f, -0.5f,
    1.0f, 0.0f,
    0.5f, 0.5f, 0.5f,
    0.0f, 0.0f,
    -0.5f, 0.5f, 0.5f,
    0.0f, 1.0f,

    // X-Z
    -0.5f, -0.5f, 0.5f,
    0.0f, 0.0f,
    0.5f, -0.5f, 0.5f,
    0.0f, 1.0f,
    0.5f, -0.5f, -0.5f,
    1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,
    1.0f, 0.0f,
};

GLuint          skyboxdatavbo;

GLfloat         artskydata[PSKYOFF_MAX*2];

// LIGHTS
static _prplanelist *plpool;
#pragma pack(push,1)
_prlight        prlights[PR_MAXLIGHTS];
int32_t         lightcount;
int32_t         curlight;
#pragma pack(pop)

static const GLfloat  shadowBias[] =
{
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.5, 0.5, 0.5, 1.0
};

// MATERIALS
static const _prprogrambit   prprogrambits[PR_BIT_COUNT] = {
    {
        //1 << PR_BIT_HEADER,
        // vert_def
        "#version 120\n"
        "#extension GL_ARB_texture_rectangle : enable\n"
        "\n",
        // vert_prog
        "",
        // frag_def
        "#version 120\n"
        "#extension GL_ARB_texture_rectangle : enable\n"
        "\n",
        // frag_prog
        "",
    },
    {
        //1 << PR_BIT_ANIM_INTERPOLATION,
        // vert_def
        "attribute vec4 nextFrameData;\n"
        "attribute vec4 nextFrameNormal;\n"
        "uniform float frameProgress;\n"
        "\n",
        // vert_prog
        "  vec4 currentFramePosition;\n"
        "  vec4 nextFramePosition;\n"
        "\n"
        "  currentFramePosition = curVertex * -(frameProgress - 1.0);\n"
        "  nextFramePosition = nextFrameData * frameProgress;\n"
        "  curVertex = currentFramePosition + nextFramePosition;\n"
        "\n"
        "  currentFramePosition = vec4(curNormal, 1.0) * -(frameProgress - 1.0);\n"
        "  nextFramePosition = nextFrameNormal * frameProgress;\n"
        "  curNormal = vec3(currentFramePosition + nextFramePosition);\n"
        "\n",
        // frag_def
        "",
        // frag_prog
        "",
    },
    {
        //1 << PR_BIT_LIGHTING_PASS,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
        "#define PR_LIGHTING_PASS\n"
        "\n",
        // frag_prog
        "  result = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "\n",
    },
    {
        //1 << PR_BIT_NORMAL_MAP,
        // vert_def
        "attribute vec3 T;\n"
        "attribute vec3 B;\n"
        "attribute vec3 N;\n"
        "uniform vec3 eyePosition;\n"
        "varying vec3 tangentSpaceEyeVec;\n"
        "\n"
        "#define PR_USE_NORMAL_MAP\n"
        "\n",
        // vert_prog
        "  TBN = mat3(T, B, N);\n"
        "  tangentSpaceEyeVec = eyePosition - vec3(curVertex);\n"
        "  tangentSpaceEyeVec = TBN * tangentSpaceEyeVec;\n"
        "\n",
        // frag_def
        "uniform sampler2D normalMap;\n"
        "uniform vec2 normalBias;\n"
        "varying vec3 tangentSpaceEyeVec;\n"
        "\n"
        "#define PR_USE_NORMAL_MAP\n"
        "\n",
        // frag_prog
        "  vec4 normalStep;\n"
        "  float biasedHeight;\n"
        "\n"
        "  eyeVec = normalize(tangentSpaceEyeVec);\n"
        "\n"
        "  for (int i = 0; i < 4; i++) {\n"
        "    normalStep = texture2D(normalMap, commonTexCoord.st);\n"
        "    biasedHeight = normalStep.a * normalBias.x - normalBias.y;\n"
        "    commonTexCoord += (biasedHeight - commonTexCoord.z) * normalStep.z * eyeVec;\n"
        "  }\n"
        "\n"
        "  normalTexel = texture2D(normalMap, commonTexCoord.st);\n"
        "\n",
    },
    {
        //1 << PR_BIT_ART_MAP,
        // vert_def
        "varying vec3 horizDistance;\n"
        "\n",
        // vert_prog
        "  gl_TexCoord[0] = gl_MultiTexCoord0;\n"
        "  horizDistance = vec3(gl_ModelViewMatrix * curVertex);\n"
        "\n",
        // frag_def
        "uniform sampler2D artMap;\n"
        "uniform sampler2D basePalMap;\n"
        "uniform sampler2DRect lookupMap;\n"
        "uniform float shadeOffset;\n"
        "uniform float visibility;\n"
        "varying vec3 horizDistance;\n"
        "\n",
        // frag_prog

        "  float shadeLookup = length(horizDistance) * (1.0 / 1.024) * visibility + shadeOffset;\n"
        "  float colorIndex = texture2D(artMap, commonTexCoord.st).r * 256.0;\n"
        "  float colorIndexNear = texture2DRect(lookupMap, vec2(colorIndex, floor(shadeLookup))).r;\n"
        "  float colorIndexFar = texture2DRect(lookupMap, vec2(colorIndex, floor(shadeLookup + 1.0))).r;\n"
        "  float colorIndexFullbright = texture2DRect(lookupMap, vec2(colorIndex, 0.0)).r;\n"
        "  vec3 texelNear = texture2D(basePalMap, vec2(colorIndexNear, 0.5)).rgb;\n"
        "  vec3 texelFar = texture2D(basePalMap, vec2(colorIndexFar, 0.5)).rgb;\n"
        "  diffuseTexel.rgb = texture2D(basePalMap, vec2(colorIndexFullbright, 0.5)).rgb;\n"
        "\n"
        "#ifndef PR_LIGHTING_PASS\n"
        "  result.rgb = mix(texelNear, texelFar, fract(shadeLookup));\n"
        "  result.a = float(colorIndex != 256.0);\n"
        "#endif\n"
        "\n",
    },
    {
        //1 << PR_BIT_DIFFUSE_MAP,
        // vert_def
        "uniform vec2 diffuseScale;\n"
        "\n",
        // vert_prog
        "  gl_TexCoord[0] = vec4(diffuseScale, 1.0, 1.0) * gl_MultiTexCoord0;\n"
        "\n",
        // frag_def
        "uniform sampler2D diffuseMap;\n"
        "\n",
        // frag_prog
        "  diffuseTexel = texture2D(diffuseMap, commonTexCoord.st);\n"
        "\n",
    },
    {
        //1 << PR_BIT_DIFFUSE_DETAIL_MAP,
        // vert_def
        "uniform vec2 detailScale;\n"
        "varying vec2 fragDetailScale;\n"
        "\n",
        // vert_prog
        "  fragDetailScale = detailScale;\n"
        "#ifndef PR_USE_NORMAL_MAP\n"
        "  gl_TexCoord[1] = vec4(detailScale, 1.0, 1.0) * gl_MultiTexCoord0;\n"
        "#endif\n"
        "\n",
        // frag_def
        "uniform sampler2D detailMap;\n"
        "varying vec2 fragDetailScale;\n"
        "\n",
        // frag_prog
        "#ifndef PR_USE_NORMAL_MAP\n"
        "  diffuseTexel *= texture2D(detailMap, gl_TexCoord[1].st);\n"
        "#else\n"
        "  diffuseTexel *= texture2D(detailMap, commonTexCoord.st * fragDetailScale);\n"
        "#endif\n"
        "  diffuseTexel.rgb *= 2.0;\n"
        "\n",
    },
    {
        //1 << PR_BIT_DIFFUSE_MODULATION,
        // vert_def
        "",
        // vert_prog
        "  gl_FrontColor = gl_Color;\n"
        "\n",
        // frag_def
        "",
        // frag_prog
        "#ifndef PR_LIGHTING_PASS\n"
        "  result *= vec4(gl_Color);\n"
        "#endif\n"
        "\n",
    },
    {
        //1 << PR_BIT_DIFFUSE_MAP2,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
        "",
        // frag_prog
        "#ifndef PR_LIGHTING_PASS\n"
        "  result *= diffuseTexel;\n"
        "#endif\n"
        "\n",
    },
    {
        //1 << PR_BIT_HIGHPALOOKUP_MAP,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
        "uniform sampler3D highPalookupMap;\n"
        "\n",
        // frag_prog
        "  float highPalScale = 0.9921875; // for 6 bits\n"
        "  float highPalBias = 0.00390625;\n"
        "\n"
        "#ifndef PR_LIGHTING_PASS\n"
        "  result.rgb = texture3D(highPalookupMap, result.rgb * highPalScale + highPalBias).rgb;\n"
        "#endif\n"
        "  diffuseTexel.rgb = texture3D(highPalookupMap, diffuseTexel.rgb * highPalScale + highPalBias).rgb;\n"
        "\n",
    },
    {
        //1 << PR_BIT_SPECULAR_MAP,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
        "uniform sampler2D specMap;\n"
        "\n"
        "#define PR_USE_SPECULAR_MAP\n"
        "\n",
        // frag_prog
        "  specTexel = texture2D(specMap, commonTexCoord.st);\n"
        "\n",
    },
    {
        //1 << PR_BIT_SPECULAR_MATERIAL,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
        "uniform vec2 specMaterial;\n"
        "\n",
        // frag_prog
        "  specularMaterial = specMaterial;\n"
        "\n",
    },
    {
        //1 << PR_BIT_MIRROR_MAP,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
        "uniform sampler2DRect mirrorMap;\n"
        "\n",
        // frag_prog
        "  vec2 mirrorCoords = gl_FragCoord.st;\n"
        "#ifdef PR_USE_NORMAL_MAP\n"
        "  mirrorCoords += 100.0 * (normalTexel.rg - 0.5);\n"
        "#endif\n"
        "  vec4 mirrorTexel = texture2DRect(mirrorMap, mirrorCoords);\n"
        "  result = vec4((result.rgb * -(specTexel.a - 1.0)) + (mirrorTexel.rgb * specTexel.rgb * specTexel.a), result.a);\n"
        "\n",
    },
    {
        //1 << PR_BIT_FOG,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
#ifdef PR_LINEAR_FOG
        "uniform bool linearFog;\n"
#endif
        "",
        // frag_prog
        "  float fogFactor;\n"
        "  float fragDepth = gl_FragCoord.z / gl_FragCoord.w;\n"
#ifdef PR_LINEAR_FOG
        "  if (!linearFog) {\n"
#endif
        "    fragDepth *= fragDepth;\n"
        "    fogFactor = exp2(-gl_Fog.density * gl_Fog.density * fragDepth * 1.442695);\n"
#ifdef PR_LINEAR_FOG
        "  } else {\n"
        "    fogFactor = gl_Fog.scale * (gl_Fog.end - fragDepth);\n"
        "    fogFactor = clamp(fogFactor, 0.0, 1.0);"
        "  }\n"
#endif
        "  result.rgb = mix(gl_Fog.color.rgb, result.rgb, fogFactor);\n"
        "\n",
    },
    {
        //1 << PR_BIT_GLOW_MAP,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
        "uniform sampler2D glowMap;\n"
        "\n",
        // frag_prog
        "  vec4 glowTexel = texture2D(glowMap, commonTexCoord.st);\n"
        "  result = vec4((result.rgb * -(glowTexel.a - 1.0)) + (glowTexel.rgb * glowTexel.a), result.a);\n"
        "\n",
    },
    {
        //1 << PR_BIT_PROJECTION_MAP,
        // vert_def
        "uniform mat4 shadowProjMatrix;\n"
        "\n",
        // vert_prog
        "  gl_TexCoord[2] = shadowProjMatrix * curVertex;\n"
        "\n",
        // frag_def
        "",
        // frag_prog
        "",
    },
    {
        //1 << PR_BIT_SHADOW_MAP,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
        "uniform sampler2DShadow shadowMap;\n"
        "\n",
        // frag_prog
        "  shadowResult = shadow2DProj(shadowMap, gl_TexCoord[2]).a;\n"
        "\n",
    },
    {
        //1 << PR_BIT_LIGHT_MAP,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
        "uniform sampler2D lightMap;\n"
        "\n",
        // frag_prog
        "  lightTexel = texture2D(lightMap, vec2(gl_TexCoord[2].s, -gl_TexCoord[2].t) / gl_TexCoord[2].q).rgb;\n"
        "\n",
    },
    {
        //1 << PR_BIT_SPOT_LIGHT,
        // vert_def
        "",
        // vert_prog
        "",
        // frag_def
        "uniform vec3 spotDir;\n"
        "uniform vec2 spotRadius;\n"
        "#define PR_SPOTLIGHT\n"
        "\n",
        // frag_prog
        "  spotVector = spotDir;\n"
        "  spotCosRadius = spotRadius;\n"
        "\n",
    },
    {
        //1 << PR_BIT_POINT_LIGHT,
        // vert_def
        "varying vec3 vertexNormal;\n"
        "varying vec3 eyeVector;\n"
        "varying vec3 lightVector;\n"
        "varying vec3 tangentSpaceLightVector;\n"
        "\n",
        // vert_prog
        "  vec3 vertexPos = vec3(gl_ModelViewMatrix * curVertex);\n"
        "  eyeVector = -vertexPos;\n"
        "  lightVector = gl_LightSource[0].ambient.rgb - vertexPos;\n"
        "\n"
        "#ifdef PR_USE_NORMAL_MAP\n"
        "  tangentSpaceLightVector = gl_LightSource[0].specular.rgb - vec3(curVertex);\n"
        "  tangentSpaceLightVector = TBN * tangentSpaceLightVector;\n"
        "#else\n"
        "  vertexNormal = normalize(gl_NormalMatrix * curNormal);\n"
        "#endif\n"
        "\n",
        // frag_def
        "varying vec3 vertexNormal;\n"
        "varying vec3 eyeVector;\n"
        "varying vec3 lightVector;\n"
        "varying vec3 tangentSpaceLightVector;\n"
        "\n",
        // frag_prog
        "  vec3 L = normalize(lightVector);\n"
        "  float pointLightDistance = dot(lightVector,lightVector);\n"
        "  float lightAttenuation = clamp(-(gl_LightSource[0].linearAttenuation * pointLightDistance - 1.0), 0.0, 1.0);\n"
        "  float spotAttenuation = 1.0;\n"
        "\n"
        "#ifdef PR_SPOTLIGHT\n"
        "  float spotCosAngle = dot(-L, normalize(spotVector));\n"
        "  spotAttenuation = clamp((spotCosAngle - spotCosRadius.x) * spotCosRadius.y, 0.0, 1.0);\n"
        "#endif\n"
        "#ifdef PR_USE_NORMAL_MAP\n"
        "  vec3 E = eyeVec;\n"
        "  vec3 N = normalize(2.0 * (normalTexel.rgb - 0.5));\n"
        "  L = normalize(tangentSpaceLightVector);\n"
        "#else\n"
        "  vec3 E = normalize(eyeVector);\n"
        "  vec3 N = normalize(vertexNormal);\n"
        "#endif\n"
        "\n"
        "  float NdotL = max(dot(N, L), 0.0);\n"
        "  vec3 R = reflect(-L, N);\n"
        "  vec3 lightDiffuse = gl_Color.a * shadowResult * lightTexel *\n"
        "                 gl_LightSource[0].diffuse.rgb * lightAttenuation * spotAttenuation;\n"
        "  result += vec4(lightDiffuse * diffuseTexel.a * diffuseTexel.rgb * NdotL, 0.0);\n"
        "\n"
        "#ifndef PR_USE_SPECULAR_MAP\n"
        "  specTexel.rgb = diffuseTexel.rgb * diffuseTexel.a;\n"
        "#endif\n"
        "\n"
        "  float lightSpecular = pow( max(dot(R, E), 0.0), specularMaterial.x * specTexel.a) * specularMaterial.y;\n"
        "  result += vec4(lightDiffuse * specTexel.rgb * lightSpecular, 0.0);\n"
        "\n",
    },
    {
        //1 << PR_BIT_FOOTER,
        // vert_def
        "void main(void)\n"
        "{\n"
        "  vec4 curVertex = gl_Vertex;\n"
        "  vec3 curNormal = gl_Normal;\n"
        "  mat3 TBN;\n"
        "\n"
        "  gl_TexCoord[0] = gl_MultiTexCoord0;\n"
        "\n",
        // vert_prog
        "  gl_Position = gl_ModelViewProjectionMatrix * curVertex;\n"
        "}\n",
        // frag_def
        "uniform vec4 u_colorCorrection;\n"
        "const vec4 c_vec4_luma_709 = vec4(0.2126, 0.7152, 0.0722, 0.0);\n"
        "const vec2 c_vec2_zero_one = vec2(0.0, 1.0);\n"
        "void main(void)\n"
        "{\n"
        "  vec3 commonTexCoord = vec3(gl_TexCoord[0].st, 0.0);\n"
        "  vec4 result = vec4(1.0, 1.0, 1.0, 1.0);\n"
        "  vec4 diffuseTexel = vec4(1.0, 1.0, 1.0, 1.0);\n"
        "  vec4 specTexel = vec4(1.0, 1.0, 1.0, 1.0);\n"
        "  vec4 normalTexel;\n"
        "  vec3 eyeVec;\n"
        "  vec3 spotVector;\n"
        "  vec2 spotCosRadius;\n"
        "  float shadowResult = 1.0;\n"
        "  vec2 specularMaterial = vec2(15.0, 1.0);\n"
        "  vec3 lightTexel = vec3(1.0, 1.0, 1.0);\n"
        "\n",
        // frag_prog
        "  vec4 v_cc = vec4(u_colorCorrection.x - 1.0, 0.5 * -(u_colorCorrection.y - 1.0), -(u_colorCorrection.z - 1.0), 1.0);\n"
        "  gl_FragColor =\n"
        "               mat4(c_vec2_zero_one.yxxx, c_vec2_zero_one.xyxx, c_vec2_zero_one.xxyx, v_cc.xxxw) *\n"
        "#ifndef PR_LIGHTING_PASS\n"
        "               mat4(u_colorCorrection.ywww, u_colorCorrection.wyww, u_colorCorrection.wwyw, v_cc.yyyw) *\n"
        "#endif\n"
        "               mat4((c_vec4_luma_709.xxxw * v_cc.z) + u_colorCorrection.zwww,\n"
        "                      (c_vec4_luma_709.yyyw * v_cc.z) + u_colorCorrection.wzww,\n"
        "                      (c_vec4_luma_709.zzzw * v_cc.z) + u_colorCorrection.wwzw,\n"
        "                      c_vec2_zero_one.xxxy) *\n"
        "               result;\n"
        "}\n",
    }
};

int32_t         overridematerial;
int32_t         globaloldoverridematerial;

int32_t         rotatespritematerialbits;

// RENDER TARGETS
_prrt           *prrts;

// CONTROL
GLfloat         spritemodelview[16];
GLfloat         mdspritespace[4][4];
GLfloat         rootmodelviewmatrix[16];
GLfloat         *curmodelviewmatrix;
GLfloat         rootskymodelviewmatrix[16];
GLfloat         *curskymodelviewmatrix;

static int16_t  sectorqueue[MAXSECTORS];
static GLuint   queryid[MAXWALLS];
static uint8_t  drawingstate[bitmap_size(MAXSECTORS)];

int16_t         *cursectormasks;
int16_t         *cursectormaskcount;

float           horizang;
fix16_t         viewangle;

int32_t         depth;
_prmirror       mirrors[10];

TESStesselator*  prtess;

static int16_t  cursky;
static char     curskypal;
static int8_t   curskyshade;
static float    curskyangmul = 1;

_pranimatespritesinfo asi;

rorcallback     prorcallback;

int32_t         polymersearching;

int32_t         culledface;

static char     transluctable[2] = { 0x55, 0xAA };

// EXTERNAL FUNCTIONS
int32_t             polymer_init(void)
{
    int32_t         i, j, t = timerGetTicks();

    if (pr_verbosity >= 1) VLOG_F(LOG_PR, "Initializing Polymer subsystem...");

    if (!glinfo.texnpot ||
        !glinfo.depthtex ||
        !glinfo.shadow ||
        !glinfo.fbos ||
        !glinfo.rect ||
        !glinfo.multitex ||
        !glinfo.occlusionqueries ||
        !glinfo.glsl)
    {
        VLOG_F(LOG_PR, "Your video card driver/combo doesn't support the necessary features!");
        VLOG_F(LOG_PR, "Disabling Polymer...");
        return 0;
    }

    buildgl_resetStateAccounting();
    // clean up existing stuff since it will be initialized again if we're re-entering here
    polymer_uninit();

    Bmemset(&prsectors[0], 0, sizeof(prsectors));
    Bmemset(&prwalls[0], 0, sizeof(prwalls));

    prtess = tessNewTess(nullptr);

    if (prtess == 0)
    {
        VLOG_F(LOG_PR, "Tessellation object initialization failed!");
        return 0;
    }

    polymer_loadboard();

    polymer_initartsky();
    skyboxdatavbo = 0;

    i = 0;
    while (i < nextmodelid)
    {
        if (models[i])
        {
            md3model_t* m;

            m = (md3model_t*)models[i];
            m->indices = NULL;
        }
        i++;
    }

    overridematerial = 0xFFFFFFFF;

    polymersearching = FALSE;

    inthash_init(&prprogramtable);

    polymer_initrendertargets(pr_shadowcount + 1);

    // Prime highpalookup maps
    i = 0;
    while (i < MAXBASEPALS)
    {
        if (prhighpalookups[i] == nullptr)
        {
            i++;
            continue;
        }
        j = 0;
        while (j < MAXPALOOKUPS)
        {
            if (prhighpalookups[i][j].data == nullptr)
            {
                j++;
                continue;
            }

            glGenTextures(1, &prhighpalookups[i][j].map);
            buildgl_bindTexture(GL_TEXTURE_3D, prhighpalookups[i][j].map);
            glTexImage3D(GL_TEXTURE_3D,                // target
                            0,                            // mip level
                            GL_RGBA,                      // internalFormat
                            PR_HIGHPALOOKUP_DIM,          // width
                            PR_HIGHPALOOKUP_DIM,          // height
                            PR_HIGHPALOOKUP_DIM,          // depth
                            0,                            // border
                            GL_BGRA,                      // upload format
                            GL_UNSIGNED_BYTE,             // upload component type
                            prhighpalookups[i][j].data);     // data pointer
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            buildgl_bindTexture(GL_TEXTURE_3D, 0);
            j++;
        }
        i++;
    }

    if (bloodhack)
    {
        transluctable[0] = 0xAA;
        transluctable[1] = 0x55;
    }

    //buildgl_resetSamplerObjects();
    polymost_initdrawpoly();

    if (pr_verbosity >= 1) VLOG_F(LOG_PR, "Initialization complete in %d ms.", timerGetTicks()-t);

    return 1;
}

void                polymer_uninit(void)
{
    int32_t         i, j;

    if (prtess)
    {
        tessDeleteTess(prtess);
        prtess = NULL;
    }

    polymer_freeboard();
    polymer_initrendertargets(0);

    if (skyboxdatavbo) glDeleteBuffers(1, &skyboxdatavbo);
    if (prmapvbo) glDeleteBuffers(1, &prmapvbo);
    if (prindexringvbo) glDeleteBuffers(1, &prindexringvbo);

    i = 0;
    while (i < MAXBASEPALS)
    {
        if (prhighpalookups[i] == nullptr)
        {
            i++;
            continue;
        }

        j = 0;
        while (j < MAXPALOOKUPS)
        {
            if (prhighpalookups[i][j].map)
            {
                glDeleteTextures(1, &prhighpalookups[i][j].map);
                prhighpalookups[i][j].map = 0;
            }

            //if (prhighpalookups[i][j].data)
            //    DO_FREE_AND_NULL(prhighpalookups[i][j].data);

            j++;
        }

        //DO_FREE_AND_NULL(prhighpalookups[i]);
        i++;
    }

    i = 0;
    while (plpool)
    {
        _prplanelist*   next = plpool->n;

        Xfree(plpool);
        plpool = next;
        i++;
    }

    if (pr_verbosity >= 3)
        VLOG_F(LOG_PR, "freed %d planelists", i);

    inthash_free(&prprogramtable);

    for (auto &pr : prprogramptrs)
    {
        glDeleteProgram(pr->handle);
        DO_FREE_AND_NULL(pr);
    }

    if (pr_verbosity >= 2)
        VLOG_F(LOG_PR, "freed %" PRIiPTR " programs", prprogramptrs.size());

    prprogramptrs.clear();
}

void                polymer_setaspect(int32_t ang)
{
    float           aspect;
    float fang = (float)ang * atanf(fviewingrange*(1.f/65536.f)) * (4.f/fPI);

    // use horizontal fov instead of vertical
    fang = atanf(tanf(fang * (fPI / 2048.f)) * float(windowxy2.y - windowxy1.y + 1) / float(windowxy2.x - windowxy1.x + 1) *
                      float(xdim) / float(ydim) * (3.f / 4.f)) * (2048.f / fPI);

    if (pr_customaspect != 0.0f)
        aspect = pr_customaspect;
    else
        aspect = (float)(windowxy2.x-windowxy1.x+1) /
                 (float)(windowxy2.y-windowxy1.y+1);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    buildgl_setPerspective(fang * (360.f/2048.f), aspect, 0.01f, 100.0f);
}

void                polymer_glinit(void)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    buildgl_setViewport(windowxy1.x, ydim-(windowxy2.y+1),windowxy2.x-windowxy1.x+1, windowxy2.y-windowxy1.y+1);

    buildgl_setEnabled(GL_DEPTH_TEST);
    buildgl_setDepthFunc(GL_LEQUAL);

    buildgl_setDisabled(GL_BLEND);
    buildgl_setDisabled(GL_ALPHA_TEST);

    if (pr_wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    polymer_setaspect(pr_fov);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glDisable(GL_FOG);

    culledface = GL_BACK;
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    buildgl_setEnabled(GL_CULL_FACE);
}

void                polymer_resetlights(void)
{
    int32_t         i;
    _prsector       *s;
    _prwall         *w;

    i = 0;
    while (i < numsectors)
    {
        s = prsectors[i];

        if (!s) {
            i++;
            continue;
        }

        polymer_resetplanelights(&s->floor);
        polymer_resetplanelights(&s->ceil);

        i++;
    }

    i = 0;
    while (i < numwalls)
    {
        w = prwalls[i];

        if (!w) {
            i++;
            continue;
        }

        polymer_resetplanelights(&w->wall);
        polymer_resetplanelights(&w->over);
        polymer_resetplanelights(&w->mask);

        i++;
    }

    i = 0;
    while (i < PR_MAXLIGHTS)
    {
        prlights[i].flags.active = 0;
        i++;
    }

    lightcount = 0;

    if (!engineLoadMHK(NULL))
        VLOG_F(LOG_PR, "Refreshed maphack data");
}

void                polymer_loadboard(void)
{
    int32_t         i;

    polymer_freeboard();

    // in the big map buffer, sectors have floor and ceiling vertices for each wall first, then walls
    prwalldataoffset = numwalls * 2 * sizeof(_prvert);

    glGenBuffers(1, &prmapvbo);
    buildgl_bindBuffer(GL_ARRAY_BUFFER, prmapvbo);
    glBufferData(GL_ARRAY_BUFFER, prwalldataoffset + (numwalls * prwalldatasize), NULL, mapvbousage);
    buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &prindexringvbo);
    buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, prindexringvbo);

    if (pr_buckets)
    {
        if (prindexring)
            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, prindexringsize * sizeof(GLuint), NULL, prindexringmapflags | GL_DYNAMIC_STORAGE_BIT);
        prindexring = (GLuint*)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, prindexringsize * sizeof(GLuint), prindexringmapflags);
    }

    buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    i = 0;
    while (i < numsectors)
    {
        polymer_initsector(i);
        polymer_updatesector(i);
        i++;
    }

    i = 0;
    while (i < numwalls)
    {
        polymer_initwall(i);
        polymer_updatewall(i);
        i++;
    }

    polymer_resetlights();

    if (pr_verbosity >= 1 && numsectors) VLOG_F(LOG_PR, "Board loaded.");
}

int32_t polymer_printtext256(int32_t xpos, int32_t ypos, int16_t col, int16_t backcol, const char *name, char fontsize)
{
    //POGOTODO: Polymer should implement this so it's no longer coupled with Polymost & reliant on the fixed-function pipeline
    buildgl_setEnabled(GL_TEXTURE_2D);
    int32_t returnVal = polymost_printtext256(xpos, ypos, col, backcol, name, fontsize);
    buildgl_setDisabled(GL_TEXTURE_2D);
    return returnVal;
}

void polymer_fillpolygon(int32_t npoints)
{
    //POGOTODO: Polymer should implement this so it's no longer coupled with Polymost & reliant on the fixed-function pipeline
    buildgl_setEnabled(GL_TEXTURE_2D);
    polymost_fillpolygon(npoints);
    buildgl_setDisabled(GL_TEXTURE_2D);
}

// The parallaxed ART sky angle divisor corresponding to a horizfrac of 32768.
#define DEFAULT_ARTSKY_ANGDIV 4.3027f

void polymer_drawrooms(int32_t daposx, int32_t daposy, int32_t daposz, fix16_t daang, fix16_t dahoriz, int16_t dacursectnum)
{
    int16_t         cursectnum;
    int32_t         i, cursectflorz=0, cursectceilz=0;
    float           skyhoriz, ang, tiltang;
    float           pos[3];
    pthtyp*         pth;

    if (videoGetRenderMode() == REND_CLASSIC) return;

    videoBeginDrawing();

    // TODO: support for screen resizing
    // frameoffset = frameplace + windowxy1.y*bytesperline + windowxy1.x;

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Drawing rooms...");

    // fogcalc_old needs this
    gvisibility = ((float)globalvisibility)*FOGSCALE;

    ang = fix16_to_float(daang) * (360.f/2048.f);
    horizang = -(float)atan2f(fix16_to_float(dahoriz) - 100.f, 128.f) * (180.f * (float)M_1_PI);
    tiltang = (gtang * 90.0f);

    pos[0] = (float)daposy;
    pos[1] = -(float)(daposz) * (1.f/16.f);
    pos[2] = -(float)daposx;

    polymer_updatelights();

//     polymer_resetlights();
//     if (pr_lighting)
//         polymer_applylights();

    depth = 0;

    if (pr_shadows && lightcount && (pr_shadowcount > 0))
        polymer_prepareshadows();

    // hack for parallax skies
    skyhoriz = horizang;
    if (skyhoriz < -180.0f)
        skyhoriz += 360.0f;

    drawingskybox = 1;
    pth = texcache_fetch(cursky, 0, 0, DAMETH_NOMASK);
    drawingskybox = 0;

    // if it's not a skybox, make the sky parallax
    // DEFAULT_ARTSKY_ANGDIV is computed from eyeballed values
    // need to recompute it if we ever change the max horiz amplitude
    if (!pth || !(pth->flags & PTH_SKYBOX))
        skyhoriz *= curskyangmul;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glRotatef(tiltang, 0.0f, 0.0f, -1.0f);
    glRotatef(skyhoriz, 1.0f, 0.0f, 0.0f);
    glRotatef(ang, 0.0f, 1.0f, 0.0f);

    glScalef(1.0f / 1000.0f, 1.0f / 1000.0f, 1.0f / 1000.0f);
    glTranslatef(-pos[0], -pos[1], -pos[2]);

    glGetFloatv(GL_MODELVIEW_MATRIX, rootskymodelviewmatrix);

    curskymodelviewmatrix = rootskymodelviewmatrix;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glRotatef(tiltang, 0.0f, 0.0f, -1.0f);
    glRotatef(horizang, 1.0f, 0.0f, 0.0f);
    glRotatef(ang, 0.0f, 1.0f, 0.0f);

    glScalef(1.0f / 1000.0f, 1.0f / 1000.0f, 1.0f / 1000.0f);
    glTranslatef(-pos[0], -pos[1], -pos[2]);

    glGetFloatv(GL_MODELVIEW_MATRIX, rootmodelviewmatrix);

    cursectnum = dacursectnum;
    updatesector(daposx, daposy, &cursectnum);

    if (cursectnum >= 0 && cursectnum < numsectors)
        dacursectnum = cursectnum;
    else if (pr_verbosity>=2)
        VLOG_F(LOG_PR, "got sector %d after update!", cursectnum);

    // unflag all sectors
    i = numsectors-1;
    while (i >= 0)
    {
        prsectors[i]->flags.uptodate = 0;
        prsectors[i]->wallsproffset = 0.0f;
        prsectors[i]->floorsproffset = 0.0f;
        i--;
    }
    i = numwalls-1;
    while (i >= 0)
    {
        prwalls[i]->flags.uptodate = 0;
        i--;
    }

    if (searchit == 2 && !polymersearching)
    {
        globaloldoverridematerial = overridematerial;
        overridematerial = (1 << PR_BIT_DIFFUSE_MODULATION);
        overridematerial |= (1 << PR_BIT_DIFFUSE_MAP2);
        polymersearching = TRUE;
    }
    if (!searchit && polymersearching) {
        overridematerial = globaloldoverridematerial;
        polymersearching = FALSE;
    }

    if (dacursectnum > -1 && dacursectnum < numsectors)
        getzsofslope(dacursectnum, daposx, daposy, &cursectceilz, &cursectflorz);

    // external view (editor)
    if ((dacursectnum < 0) || (dacursectnum >= numsectors) ||
            (daposz > cursectflorz) ||
            (daposz < cursectceilz))
    {
        prcanbucket = 1;

        if (!editstatus && pr_verbosity>=1)
        {
            if ((unsigned)dacursectnum < (unsigned)numsectors)
                VLOG_F(LOG_PR, "EXT sec=%d  z=%d (%d, %d)", dacursectnum, daposz, cursectflorz, cursectceilz);
            else
                VLOG_F(LOG_PR, "EXT sec=%d  z=%d", dacursectnum, daposz);
        }

        curmodelviewmatrix = rootmodelviewmatrix;
        i = numsectors-1;
        while (i >= 0)
        {
            polymer_updatesector(i);
            polymer_drawsector(i, FALSE);
            polymer_scansprites(i, tsprite, &spritesortcnt);
            i--;
        }

        i = numwalls-1;
        while (i >= 0)
        {
            polymer_updatewall(i);
            polymer_drawwall(wallsect[i], i);
            i--;
        }

        polymer_emptybuckets();

        viewangle = daang;
        videoEndDrawing();
        return;
    }

    // GO!
    polymer_displayrooms(dacursectnum);

    curmodelviewmatrix = rootmodelviewmatrix;

    // build globals used by rotatesprite
    viewangle = daang;
    set_globalang(daang);

    // polymost globals used by polymost_dorotatesprite
    gcosang = fcosglobalang*(1./262144.f);
    gsinang = fsinglobalang*(1./262144.f);
    gcosang2 = gcosang*fviewingrange*(1./65536.f);
    gsinang2 = gsinang*fviewingrange*(1./65536.f);

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Rooms drawn.");
    videoEndDrawing();
}

void                polymer_drawmasks(void)
{
    buildgl_setEnabled(GL_ALPHA_TEST);
    buildgl_setEnabled(GL_BLEND);
//     buildgl_setEnabled(GL_POLYGON_OFFSET_FILL);

//     while (--spritesortcnt)
//     {
//         tspriteptr[spritesortcnt] = &tsprite[spritesortcnt];
//         polymer_drawsprite(spritesortcnt);
//     }

    buildgl_setEnabled(GL_CULL_FACE);

    if (cursectormaskcount) {
        // We (kind of) queue sector masks near to far, so drawing them in reverse
        // order is the sane approach here. Of course impossible cases will arise.
        while (*cursectormaskcount) {
            polymer_drawsector(cursectormasks[--(*cursectormaskcount)], TRUE);
        }

        // This should _always_ be called after a corresponding pr_displayrooms()
        // unless we're in "external view" mode, which was checked above.
        // Both the top-level game drawrooms and the recursive internal passes
        // should be accounted for here. If these free cause corruption, there's
        // an accounting bug somewhere.
        DO_FREE_AND_NULL(cursectormaskcount);
        DO_FREE_AND_NULL(cursectormasks);
    }

    buildgl_setDisabled(GL_CULL_FACE);

//     buildgl_setDisabled(GL_POLYGON_OFFSET_FILL);
    buildgl_setDisabled(GL_BLEND);
    buildgl_setDisabled(GL_ALPHA_TEST);

    //polymost_resetVertexPointers(1);
}


void                polymer_editorpick(void)
{
    GLubyte         picked[3];
    int16_t         num;

    glReadPixels(searchx, ydim - searchy, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, picked);

    num = B_UNBUF16(&picked[1]);

    searchstat = picked[0];

    switch (searchstat) {
    case 0: // wall
    case 5: // botomwall
    case 4: // 1-way/masked wall
        searchsector = wallsect[num];
        searchbottomwall = searchwall = num;
        searchisbottom = (searchstat==5);
        if (searchstat == 5) {
            searchstat = 0;
            if (wall[num].nextwall >= 0 && (wall[num].cstat & 2)) {
                searchbottomwall = wall[num].nextwall;
            }
        }
        break;
    case 1: // floor
    case 2: // ceiling
        searchsector = num;

        // Apologies to Plagman for littering here, but this feature is quite essential
        {
            GLfloat model[16];
            glGetFloatv(GL_MODELVIEW_MATRIX, model);
            GLfloat proj[16];
            glGetFloatv(GL_PROJECTION_MATRIX, proj);
            GLint view[4];
            glGetIntegerv(GL_VIEWPORT, view);

            GLfloat bestwdistsq = (GLfloat)3.4e38, wdistsq;
            GLfloat w1[2], w2[2], w21[2], pw1[2], pw2[2];
            walltype *wal = &wall[sector[searchsector].wallptr];

            GLfloat dadepth;
            glReadPixels(searchx, ydim-searchy, 1,1, GL_DEPTH_COMPONENT, GL_FLOAT, &dadepth);
            GLfloat x,y,z;
            buildgl_unprojectMatrixToViewport({ fsearchx, fydim-fsearchy, dadepth},  model, proj, view,  &x, &y, &z);
            GLfloat scrx,scry,scrz;
            buildgl_unprojectMatrixToViewport({ fsearchx, fydim-fsearchy, 0.0},  model, proj, view,  &scrx, &scry, &scrz);

            GLfloat scr[3]    = { scrx, scry, scrz };
            GLfloat scrv[3]   = { x-scrx, y-scry, z-scrz };
            GLfloat scrvxz[2] = { x-scrx, z-scrz };

            if (prsectors[searchsector]==NULL)
            {
                //OSD_Printf("polymer_editorpick: prsectors[searchsector]==NULL !!!");
                searchwall = sector[num].wallptr;
            }
            else
            {
                auto pl = (searchstat==1) ? prsectors[searchsector]->ceil.plane:prsectors[searchsector]->floor.plane;

                if (pl == NULL)
                {
                    searchwall = sector[num].wallptr;
                    return;
                }

                GLfloat t = dot3f(pl,scrv);
                GLfloat svcoeff = -(dot3f(pl,scr)+pl[3])/t;

                // point on plane (x and z)
                GLfloat p[2] = { scrx + svcoeff*scrv[0], scrz + svcoeff*scrv[2] };
                int16_t bestk=0;
                GLfloat w1d, w2d;

                for (int k=0; k<sector[searchsector].wallnum; k++)
                {
                    w1[1] = -(float)wal[k].x;
                    w1[0] = (float)wal[k].y;
                    w2[1] = -(float)wall[wal[k].point2].x;
                    w2[0] = (float)wall[wal[k].point2].y;

                    GLfloat scrvxznorm = Bsqrtf(dot2f(scrvxz,scrvxz));
                    GLfloat scrvxzn[2] = { scrvxz[1]/scrvxznorm,
                                          -scrvxz[0]/scrvxznorm };
                    relvec2f(p,w1, pw1);
                    relvec2f(p,w2, pw2);
                    relvec2f(w2,w1, w21);

                    w1d = dot2f(scrvxzn,pw1);
                    w2d = dot2f(scrvxzn,pw2);
                    w2d = -w2d;
                    if (w1d <= 0 || w2d <= 0)
                        continue;

                    GLfloat scrpxz[2];
                    GLfloat ptonline[2] = { w2[0]+(w2d/(w1d+w2d))*w21[0],
                                            w2[1]+(w2d/(w1d+w2d))*w21[1] };
                    relvec2f(p,ptonline, scrpxz);
                    if (dot2f(scrvxz,scrpxz)<0)
                        continue;

                    wdistsq = dot2f(scrpxz,scrpxz);
                    if (wdistsq < bestwdistsq)
                    {
                        bestk = k;
                        bestwdistsq = wdistsq;
                    }
                }

                searchwall = sector[searchsector].wallptr + bestk;
            }
        }
        // :P

//        searchwall = sector[num].wallptr;
        break;
    case 3:
        // sprite
        searchsector = sprite[num].sectnum;
        searchwall = num;
        break;
    }

    searchit = 0;
}

void                polymer_inb4rotatesprite(int16_t tilenum, char pal, int8_t shade, int32_t method)
{
    _prmaterial     rotatespritematerial;

    polymer_getbuildmaterial(&rotatespritematerial, tilenum, pal, shade, 0, method);
    rotatespritematerialbits = polymer_bindmaterial(&rotatespritematerial, NULL, 0);
    //buildgl_bindSamplerObject(0, PTH_CLAMPED | ((rotatespritematerialbits & (1 << PR_BIT_ART_MAP)) ? PTH_INDEXED : PTH_HIGHTILE));
    buildgl_bindBuffer(GL_ARRAY_BUFFER, drawpolyVertsID);

    glVertexPointer(3, GL_FLOAT, 5*sizeof(float), 0);
    glTexCoordPointer(2, GL_FLOAT, 5*sizeof(float), (GLvoid*) (3*sizeof(float)));
}

void                polymer_postrotatesprite(void)
{
    polymer_unbindmaterial(rotatespritematerialbits);
    buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);
}

static void         polymer_setupdiffusemodulation(_prplane *plane, GLubyte modulation, const GLubyte *data)
{
    plane->material.diffusemodulation[0] = modulation;
    plane->material.diffusemodulation[1] = ((GLubyte const *) data)[0];
    plane->material.diffusemodulation[2] = ((GLubyte const *) data)[1];
    plane->material.diffusemodulation[3] = 0xFF;
}

static void         polymer_drawsearchplane(_prplane *plane, GLubyte *oldcolor, GLubyte modulation, GLubyte const *data)
{
    Bmemcpy(oldcolor, plane->material.diffusemodulation, sizeof(GLubyte) * 4);

    polymer_setupdiffusemodulation(plane, modulation, data);

    polymer_drawplane(plane);

    Bmemcpy(plane->material.diffusemodulation, oldcolor, sizeof(GLubyte) * 4);
}

void                polymer_drawmaskwall(int32_t damaskwallcnt)
{
    GLubyte         oldcolor[4];

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Masked wall %i", damaskwallcnt);

    int16_t const wallnum = maskwall[damaskwallcnt];

    auto sec = (usectorptr_t)&sector[wallsect[wallnum]];
    auto wal = &wall[wallnum];
    auto w   = prwalls[wallnum];

    buildgl_setEnabled(GL_CULL_FACE);

    if (searchit == 2) {
        polymer_drawsearchplane(&w->mask, oldcolor, 0x04, (GLubyte const *)&wallnum);
    } else {
        calc_and_apply_fog(fogshade(wal->shade, wal->pal), sec->visibility, get_floor_fogpal(sec));
#ifdef NEW_MAP_FORMAT
        uint8_t const blend = wal->blend;
#else
        uint8_t const blend = wallext[wallnum].blend;
#endif
        handle_blend(!!(wal->cstat & CSTAT_WALL_TRANSLUCENT), blend, !!(wal->cstat & CSTAT_WALL_TRANS_FLIP));
        polymer_drawplane(&w->mask);
    }

    buildgl_setDisabled(GL_CULL_FACE);
}

void                polymer_drawsprite(int32_t snum)
{
    int32_t         i, j, cs;
    _prsprite       *s;

    auto const tspr = tspriteptr[snum];
    usectorptr_t sec;

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Sprite %i", snum);

    if (bad_tspr(tspr))
        return;

    auto const tsprflags = tspr->clipdist;

    if ((tsprflags & TSPR_FLAGS_NO_SHADOW) && (depth && !mirrors[depth-1].plane))
        return;

    if ((tsprflags & TSPR_FLAGS_INVISIBLE_WITH_SHADOW) && (!depth || mirrors[depth-1].plane))
        return;

    int const spritenum = tspr->owner;
    Bassert(spritenum < MAXSPRITES+MAXUNIQHUDID);

    tileUpdatePicnum(&tspr->picnum, (unsigned)spritenum < MAXSPRITES ? spritenum+32768 : 0);

    sec = (usectorptr_t)&sector[tspr->sectnum];
    calc_and_apply_fog(fogshade(tspr->shade, tspr->pal), sec->visibility, get_floor_fogpal((usectorptr_t)&sector[tspr->sectnum]));
    handle_blend(!!(tspr->cstat & CSTAT_SPRITE_TRANSLUCENT), tspr->blend, !!(tspr->cstat & CSTAT_SPRITE_TRANSLUCENT_INVERT));

    int32_t pTile = Ptile2tile(tspr->picnum,tspr->pal);

    if (usemodels && tile2model[pTile].modelid >= 0 &&
        tile2model[pTile].framenum >= 0 &&
        !(spriteext[spritenum].flags & SPREXT_NOTMD) &&
        models[tile2model[pTile].modelid]->mdnum != 1)
    {
        buildgl_setEnabled(GL_CULL_FACE);
        SWITCH_CULL_DIRECTION;
        polymer_drawmdsprite(tspr);
        SWITCH_CULL_DIRECTION;
        buildgl_setDisabled(GL_CULL_FACE);
        return;
    }

    cs = tspr->cstat;

    // I think messing with the tspr is safe at this point?
    // If not, change that to modify a temp position in updatesprite itself.
    // I don't think this flags are meant to change on the fly so it'd possibly
    // be safe to cache a plane that has them applied.
    if (spriteext[spritenum].flags & SPREXT_AWAY1)
    {
        tspr->x += sintable[(tspr->ang + 512) & 2047] >> 13;
        tspr->y += sintable[tspr->ang & 2047] >> 13;
    }
    else if (spriteext[spritenum].flags & SPREXT_AWAY2)
    {
        tspr->x -= sintable[(tspr->ang + 512) & 2047] >> 13;
        tspr->y -= sintable[tspr->ang & 2047] >> 13;
    }

    polymer_updatesprite(snum);

    Bassert(spritenum < MAXSPRITES);
    s = prsprites[spritenum];

    if (s == NULL)
        return;

    switch (cs & CSTAT_SPRITE_ALIGNMENT)
    {
    case CSTAT_SPRITE_ALIGNMENT_WALL:
        prsectors[tspr->sectnum]->wallsproffset += 0.5f;
        if (!depth || mirrors[depth-1].plane)
            glPolygonOffset(-(1.f/min(2048, sepldist(globalposx-tspr->x, globalposy-tspr->y)>>2)), -64);
        break;
    case CSTAT_SPRITE_ALIGNMENT_FLOOR:
    case CSTAT_SPRITE_ALIGNMENT_SLOPE:
        prsectors[tspr->sectnum]->floorsproffset += 0.5f;
        if (!depth || mirrors[depth-1].plane)
            glPolygonOffset(-(1.f/min(2048, sepdist(globalposx-tspr->x, globalposy-tspr->y, globalposz-tspr->z)>>5)), -64);
        break;
    }

    if ((cs & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_FACING)
    {
        int32_t curpriority = 0;

        s->plane.lightcount = 0;

        while ((curpriority < pr_maxlightpriority) && (!depth || mirrors[depth-1].plane))
        {
            i = j = 0;
            while (j < lightcount)
            {
                while (!prlights[i].flags.active)
                    i++;

                if (prlights[i].priority == curpriority
                    && sepdist(prlights[i].x-tspr->x, prlights[i].y-tspr->y, prlights[i].z-tspr->z) < (prlights[i].range + 256))
                {
                    if (polymer_planeinlight(s->plane, prlights[i]))
                        s->plane.lights[s->plane.lightcount++] = i;
                }

                i++;
                j++;
            }
            curpriority++;
        }
    }

    if (automapping == 1)
        bitmap_set(show2dsprite, spritenum);

    if ((tspr->cstat & 64) && (tspr->cstat & CSTAT_SPRITE_ALIGNMENT))
    {
        if ((tspr->cstat & CSTAT_SPRITE_ALIGNMENT_FLOOR) && (tspr->cstat & CSTAT_SPRITE_YFLIP))
            SWITCH_CULL_DIRECTION;
        buildgl_setEnabled(GL_CULL_FACE);
    }

    if ((!depth || mirrors[depth-1].plane) && !pr_ati_nodepthoffset)
        buildgl_setEnabled(GL_POLYGON_OFFSET_FILL);

    polymer_drawplane(&s->plane);

    if ((!depth || mirrors[depth-1].plane) && !pr_ati_nodepthoffset)
        buildgl_setDisabled(GL_POLYGON_OFFSET_FILL);

    if ((tspr->cstat & 64) && (tspr->cstat & CSTAT_SPRITE_ALIGNMENT))
    {
        if ((tspr->cstat & CSTAT_SPRITE_ALIGNMENT_FLOOR) && (tspr->cstat & CSTAT_SPRITE_YFLIP))
            SWITCH_CULL_DIRECTION;
        buildgl_setDisabled(GL_CULL_FACE);
    }
}

void                polymer_setanimatesprites(animatespritesptr animatesprites, int32_t x, int32_t y, int32_t z, int32_t a, int32_t smoothratio)
{
    asi.animatesprites = animatesprites;
    asi.x = x;
    asi.y = y;
    asi.z = z;
    asi.a = a;
    asi.smoothratio = smoothratio;
}

int16_t             polymer_addlight(_prlight* light)
{
    int32_t         lighti;

    if (lightcount >= PR_MAXLIGHTS || light->priority > pr_maxlightpriority || !pr_lighting)
        return -1;

    if ((light->sector == -1) || (light->sector >= numsectors))
        return -1;

    lighti = 0;
    while ((lighti < PR_MAXLIGHTS) && (prlights[lighti].flags.active))
        lighti++;

    if (lighti == PR_MAXLIGHTS)
        return -1;
#if 0
    // Spot lights disabled on ATI cards because they cause crashes with
    // Catalyst 12.8 drivers.
    // See: http://forums.duke4.net/topic/5723-hrp-polymer-crash/
    if (pr_ati_fboworkaround && light->radius)
        return -1;
#endif
    Bmemcpy(&prlights[lighti], light, sizeof(_prlight));

    if (light->radius) {
        polymer_processspotlight(&prlights[lighti]);

        // get the texture handle for the lightmap
        if (light->tilenum > 0) {
            int16_t     picnum = light->tilenum;
            pthtyp*     pth;

            tileUpdatePicnum(&picnum, 0);

            if (!waloff[picnum])
                tileLoad(picnum);

            pth = NULL;
            pth = texcache_fetch(picnum, 0, 0, DAMETH_NOMASK);

            if (pth)
                light->lightmap = rhi->texture_handle(pth->rhipic);
        }
    }

    prlights[lighti].flags.isinview = 0;
    prlights[lighti].flags.active = 1;

    prlights[lighti].flags.invalidate = 0;

    prlights[lighti].planecount = 0;
    prlights[lighti].planelist = NULL;

    if (polymer_culllight(lighti))
        prlights[lighti].flags.invalidate = 1;

    lightcount++;

    return lighti;
}

void                polymer_deletelight(int16_t lighti)
{
    if (!prlights[lighti].flags.active)
    {
#ifdef DEBUGGINGAIDS
        if (pr_verbosity >= 2)
            VLOG_F(LOG_PR, "Called polymer_deletelight on inactive light");
        // currently known cases: when reloading maphack lights (didn't set maphacklightcnt=0
        // but did loadmaphack()->delete_maphack_lights() after polymer_resetlights())
#endif
        return;
    }

    polymer_removelight(lighti);

    prlights[lighti].flags.active = 0;

    lightcount--;
}

void                polymer_invalidatelights(void)
{
    int32_t         i = PR_MAXLIGHTS-1;

    do
        prlights[i].flags.invalidate = prlights[i].flags.active;
    while (i--);
}

void                polymer_texinvalidate(void)
{
    int32_t         i;

    i = 0;

    while (i < MAXSPRITES) {
        polymer_invalidatesprite(i);
        i++;
    }

    i = numsectors - 1;

    if (!numsectors || !prsectors[i])
        return;

    do
        prsectors[i--]->flags.invalidtex = 1;
    while (i >= 0);

    i = numwalls - 1;
    do
        prwalls[i--]->flags.invalidtex = 1;
    while (i >= 0);
}

void polymer_definehighpalookup(uint8_t basepalnum, uint8_t palnum, char *data)
{
    if (prhighpalookups[basepalnum] == nullptr)
        prhighpalookups[basepalnum] = (_prhighpalookup *)Xcalloc(sizeof(_prhighpalookup), MAXPALOOKUPS);

    Bassert(prhighpalookups[basepalnum][palnum].data == nullptr);

    prhighpalookups[basepalnum][palnum].data = (char *)Xmalloc(PR_HIGHPALOOKUP_DATA_SIZE);

    Bmemcpy(prhighpalookups[basepalnum][palnum].data, data, PR_HIGHPALOOKUP_DATA_SIZE);
}

bool polymer_havehighpalookup(uint8_t basepalnum, uint8_t palnum)
{
    return prhighpalookups[basepalnum] != nullptr && prhighpalookups[basepalnum][palnum].data != nullptr;
}

void                polymer_setrorcallback(rorcallback callback)
{
    prorcallback = callback;
}

// CORE
static void         polymer_displayrooms(const int16_t dacursectnum)
{
    usectorptr_t      sec;
    int32_t         i;
#ifdef YAX_ENABLE
    int16_t         bunchnum;
    int16_t         ns;
#endif
    GLint           result;
    int16_t         doquery;
    int32_t         front;
    int32_t         back;
    GLfloat         localskymodelviewmatrix[16];
    GLfloat         localmodelviewmatrix[16];
    GLfloat         localprojectionmatrix[16];
    float           frustum[5 * 4];
    int32_t         localspritesortcnt;
    tspritetype     localtsprite[MAXSPRITESONSCREEN];
    int16_t         localmaskwall[MAXWALLSB];
    int16_t         localmaskwallcnt;
    _prmirror       mirrorlist[10];
    int             mirrorcount;
    int16_t         mirrorsect;
    int16_t         *localsectormasks;
    int16_t         *localsectormaskcount;
    int32_t         gx, gy, gz, px, py, pz;
    float           coeff;
    float           pos[3];

    curmodelviewmatrix = localmodelviewmatrix;
    glGetFloatv(GL_MODELVIEW_MATRIX, localmodelviewmatrix);
    glGetFloatv(GL_PROJECTION_MATRIX, localprojectionmatrix);

    polymer_extractfrustum(localmodelviewmatrix, localprojectionmatrix, frustum);

    Bmemset(queryid, 0, sizeof(GLuint) * numwalls);
    Bmemset(drawingstate, 0, sizeof(drawingstate));

    front = 0;
    back = 1;
    sectorqueue[0] = dacursectnum;
    bitmap_set(drawingstate, dacursectnum);

    localspritesortcnt = localmaskwallcnt = 0;

    mirrorcount = 0;

    localsectormasks = (int16_t *)Xmalloc(sizeof(int16_t) * numsectors);
    localsectormaskcount = (int16_t *)Xcalloc(1, sizeof(int16_t));
    cursectormasks = localsectormasks;
    cursectormaskcount = localsectormaskcount;

    buildgl_setDisabled(GL_DEPTH_TEST);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    polymer_drawsky(cursky, curskypal, curskyshade);
    buildgl_setEnabled(GL_DEPTH_TEST);

    // depth-only occlusion testing pass
//     overridematerial = 0;

    prcanbucket = 1;
    prdidsky = false;

    while (front != back)
    {
        sec = (usectorptr_t)&sector[sectorqueue[front]];

        polymer_pokesector(sectorqueue[front]);
        polymer_drawsector(sectorqueue[front], FALSE);
        polymer_scansprites(sectorqueue[front], localtsprite, &localspritesortcnt);

        if (!depth && sec->ceilingpicnum >= r_rortexture && sec->ceilingpicnum < r_rortexture+r_rortexturerange)
        {
            mirrorlist[mirrorcount].plane = &prsectors[sectorqueue[front]]->ceil;
            mirrorlist[mirrorcount].sectnum = sectorqueue[front];
            mirrorlist[mirrorcount].wallnum = -1;
            mirrorlist[mirrorcount].rorstat = 1;
            mirrorcount++;
        }

        if (!depth && sec->floorpicnum >= r_rortexture && sec->floorpicnum < r_rortexture+r_rortexturerange)
        {
            mirrorlist[mirrorcount].plane = &prsectors[sectorqueue[front]]->floor;
            mirrorlist[mirrorcount].sectnum = sectorqueue[front];
            mirrorlist[mirrorcount].rorstat = 2;
            mirrorcount++;
        }

        doquery = 0;

        i = sec->wallnum-1;
        do
        {
            // if we have a level boundary somewhere in the sector,
            // consider these walls as visportals
            if (wall[sec->wallptr + i].nextsector < 0 && pr_buckets == 0)
                doquery = 1;
        }
        while (--i >= 0);

        i = sec->wallnum-1;
        while (i >= 0)
        {
            if ((wall[sec->wallptr + i].nextsector >= 0) &&
                (wallvisible(globalposx, globalposy, sec->wallptr + i)) &&
                (polymer_planeinfrustum(prwalls[sec->wallptr + i]->mask, frustum)))
            {
                if ((prwalls[sec->wallptr + i]->mask.vertcount == 4) &&
                    !(prwalls[sec->wallptr + i]->underover & 4) &&
                    !(prwalls[sec->wallptr + i]->underover & 8))
                {
                    // early exit for closed sectors
                    _prwall         *w;

                    w = prwalls[sec->wallptr + i];

                    if ((w->mask.buffer[0].y >= w->mask.buffer[3].y) &&
                        (w->mask.buffer[1].y >= w->mask.buffer[2].y))
                    {
                        i--;
                        continue;
                    }
                }

                if ((wall[sec->wallptr + i].cstat & 48) == 16)
                {
                    int pic = wall[sec->wallptr + i].overpicnum;

                    if (tilesiz[pic].x > 0 && tilesiz[pic].y > 0)
                        localmaskwall[localmaskwallcnt++] = sec->wallptr + i;
                }

                if (!depth && (overridematerial & (1 << PR_BIT_MIRROR_MAP)) &&
                     wall[sec->wallptr + i].overpicnum == 560 &&
                     wall[sec->wallptr + i].cstat & 32)
                {
                    mirrorlist[mirrorcount].plane = &prwalls[sec->wallptr + i]->mask;
                    mirrorlist[mirrorcount].sectnum = sectorqueue[front];
                    mirrorlist[mirrorcount].wallnum = sec->wallptr + i;
                    mirrorlist[mirrorcount].rorstat = 0;
                    mirrorcount++;
                }

                if (!depth && (overridematerial & (1 << PR_BIT_MIRROR_MAP)) &&
                     wall[sec->wallptr + i].picnum >= r_rortexture && wall[sec->wallptr + i].picnum < r_rortexture + r_rortexturerange)
                {
                    mirrorlist[mirrorcount].plane = &prwalls[sec->wallptr + i]->wall;
                    mirrorlist[mirrorcount].sectnum = sectorqueue[front];
                    mirrorlist[mirrorcount].wallnum = sec->wallptr + i;
                    mirrorlist[mirrorcount].rorstat = 0;
                    mirrorcount++;
                }

                if (!(wall[sec->wallptr + i].cstat & 32)) {
                    if (doquery && !bitmap_test(drawingstate, wall[sec->wallptr + i].nextsector))
                    {
                        float pos[3], sqdist;
                        int32_t oldoverridematerial;

                        pos[0] = fglobalposy;
                        pos[1] = fglobalposz * (-1.f/16.f);
                        pos[2] = -fglobalposx;

                        sqdist = prwalls[sec->wallptr + i]->mask.plane[0] * pos[0] +
                                 prwalls[sec->wallptr + i]->mask.plane[1] * pos[1] +
                                 prwalls[sec->wallptr + i]->mask.plane[2] * pos[2] +
                                 prwalls[sec->wallptr + i]->mask.plane[3];

                        // hack to avoid occlusion querying portals that are too close to the viewpoint
                        // this is needed because of the near z-clipping plane;
                        if (sqdist < 100)
                            queryid[sec->wallptr + i] = 0xFFFFFFFF;
                        else {
                            _prwall         *w;

                            w = prwalls[sec->wallptr + i];

                            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                            glDepthMask(GL_FALSE);

                            //buildgl_bindSamplerObject(0, PTH_DEPTH_SAMPLER);

                            glGenQueries(1, &queryid[sec->wallptr + i]);
                            glBeginQuery(GL_SAMPLES_PASSED, queryid[sec->wallptr + i]);

                            oldoverridematerial = overridematerial;
                            overridematerial = 0;

                            if ((w->underover & 4) && (w->underover & 1))
                                polymer_drawplane(&w->wall);
                            polymer_drawplane(&w->mask);
                            if ((w->underover & 8) && (w->underover & 2))
                                polymer_drawplane(&w->over);

                            overridematerial = oldoverridematerial;

                            glEndQuery(GL_SAMPLES_PASSED);

                            glDepthMask(GL_TRUE);
                            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                        }
                    } else
                        queryid[sec->wallptr + i] = 0xFFFFFFFF;
                }
            }

            i--;
        }

        // Cram as much CPU or GPU work as we can between queuing the
        // occlusion queries and reaping them.
        i = sec->wallnum-1;
        do
        {
            if (wallvisible(globalposx, globalposy, sec->wallptr + i))
                polymer_drawwall(sectorqueue[front], sec->wallptr + i);
        }
        while (--i >= 0);
#ifdef YAX_ENABLE
        // queue ROR neighbors
        if ((bunchnum = yax_getbunch(sectorqueue[front], YAX_FLOOR)) >= 0) {

            for (SECTORS_OF_BUNCH(bunchnum, YAX_CEILING, ns)) {

                if (ns >= 0 && !bitmap_test(drawingstate, ns) &&
                    polymer_planeinfrustum(prsectors[ns]->ceil, frustum)) {

                    sectorqueue[back++] = ns;
                    bitmap_set(drawingstate, ns);
                }
            }
        }

        if ((bunchnum = yax_getbunch(sectorqueue[front], YAX_CEILING)) >= 0) {

            for (SECTORS_OF_BUNCH(bunchnum, YAX_FLOOR, ns)) {

                if (ns >= 0 && !bitmap_test(drawingstate, ns) &&
                    polymer_planeinfrustum(prsectors[ns]->floor, frustum)) {

                    sectorqueue[back++] = ns;
                    bitmap_set(drawingstate, ns);
                }
            }
        }
#endif
        i = sec->wallnum-1;
        do
        {
            if ((queryid[sec->wallptr + i]) && !bitmap_test(drawingstate, wall[sec->wallptr + i].nextsector))
            {
                // REAP
                result = 0;
                if (doquery && (queryid[sec->wallptr + i] != 0xFFFFFFFF))
                {
                    glGetQueryObjectiv(queryid[sec->wallptr + i],
                                           GL_QUERY_RESULT,
                                           &result);
                    glDeleteQueries(1, &queryid[sec->wallptr + i]);
                } else if (queryid[sec->wallptr + i] == 0xFFFFFFFF)
                    result = 1;

                queryid[sec->wallptr + i] = 0;

                if (result || !doquery)
                {
                    sectorqueue[back++] = wall[sec->wallptr + i].nextsector;
                    bitmap_set(drawingstate, wall[sec->wallptr + i].nextsector);
                }
            } else if (queryid[sec->wallptr + i] &&
                       queryid[sec->wallptr + i] != 0xFFFFFFFF)
            {
                glDeleteQueries(1, &queryid[sec->wallptr + i]);
                queryid[sec->wallptr + i] = 0;
            }
        }
        while (--i >= 0);

        front++;
    }

    polymer_emptybuckets();

    // do the actual shaded drawing

    if (pr_buckets)
    {
        overridematerial = 0xFFFFFFFF;

        //go through the sector queue again
        front = 0;
        while (front < back)
        {
            sec = (usectorptr_t)&sector[sectorqueue[front]];

            polymer_drawsector(sectorqueue[front], 0);

            i = 0;
            while (i < sec->wallnum)
            {
                polymer_drawwall(sectorqueue[front], sec->wallptr + i);

                i++;
            }

            front++;
        }
    }

    i = mirrorcount-1;
    while (i >= 0)
    {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, prrts[0].fbo);
        glPushAttrib(GL_VIEWPORT_BIT);
        glViewport(windowxy1.x, ydim-(windowxy2.y+1),windowxy2.x-windowxy1.x+1, windowxy2.y-windowxy1.y+1);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Bmemcpy(localskymodelviewmatrix, curskymodelviewmatrix, sizeof(GLfloat) * 16);
        curskymodelviewmatrix = localskymodelviewmatrix;

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        GLdouble const plane[4] = { mirrorlist[i].plane->plane[0],
                                    mirrorlist[i].plane->plane[1],
                                    mirrorlist[i].plane->plane[2],
                                    mirrorlist[i].plane->plane[3] };

        glClipPlane(GL_CLIP_PLANE0, plane);

        if (mirrorlist[i].rorstat == 0)
        {
            polymer_inb4mirror(mirrorlist[i].plane->buffer, mirrorlist[i].plane->plane);
            SWITCH_CULL_DIRECTION;
        }
        //buildgl_setEnabled(GL_CLIP_PLANE0);

        if (mirrorlist[i].rorstat == 0 && mirrorlist[i].wallnum >= 0)
            renderPrepareMirror(globalposx, globalposy, globalposz, qglobalang, qglobalhoriz,
                                mirrorlist[i].wallnum, &gx, &gy, &viewangle);

        gx = globalposx;
        gy = globalposy;
        gz = globalposz;

        if (mirrorlist[i].rorstat == 0)
        {
            // map the player pos from build to polymer
            px = globalposy;
            py = -globalposz / 16;
            pz = -globalposx;

            // calculate new player position on the other side of the mirror
            // this way the basic build visibility shit can be used (wallvisible)
            coeff = mirrorlist[i].plane->plane[0] * px +
                    mirrorlist[i].plane->plane[1] * py +
                    mirrorlist[i].plane->plane[2] * pz +
                    mirrorlist[i].plane->plane[3];

            coeff /= (float)(mirrorlist[i].plane->plane[0] * mirrorlist[i].plane->plane[0] +
                             mirrorlist[i].plane->plane[1] * mirrorlist[i].plane->plane[1] +
                             mirrorlist[i].plane->plane[2] * mirrorlist[i].plane->plane[2]);

            px = (int32_t)(-coeff*mirrorlist[i].plane->plane[0]*2 + px);
            py = (int32_t)(-coeff*mirrorlist[i].plane->plane[1]*2 + py);
            pz = (int32_t)(-coeff*mirrorlist[i].plane->plane[2]*2 + pz);

            // map back from polymer to build
            set_globalpos(-pz, px, -py * 16);

            mirrorsect = mirrorlist[i].sectnum;
        }
        else
        {
            // map the player pos from build to polymer
            px = globalposx;
            py = globalposy;
            pz = globalposz;

            mirrorsect = mirrorlist[i].sectnum;

            if (prorcallback)
                prorcallback(mirrorlist[i].sectnum, mirrorlist[i].wallnum, mirrorlist[i].rorstat, &mirrorsect, &px, &py, &pz);

            pos[0] = (py-gy);
            pos[1] = -(pz-gz)/16.f;
            pos[2] = -(px-gx);

            glTranslatef(-pos[0], -pos[1], -pos[2]);

            glPushMatrix();
            glLoadMatrixf(curskymodelviewmatrix);
            glTranslatef(-pos[0], -pos[1], -pos[2]);
            glGetFloatv(GL_MODELVIEW_MATRIX, curskymodelviewmatrix);
            glPopMatrix();

            set_globalpos(px, py, pz);
        }

        mirrors[depth++] = mirrorlist[i];
        polymer_displayrooms(mirrorsect);
        depth--;

        cursectormasks = localsectormasks;
        cursectormaskcount = localsectormaskcount;

        set_globalpos(gx, gy, gz);

        buildgl_setDisabled(GL_CLIP_PLANE0);
        if (mirrorlist[i].rorstat == 0)
            SWITCH_CULL_DIRECTION;
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glPopAttrib();
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

        mirrorlist[i].plane->material.mirrormap = prrts[0].color;
        polymer_drawplane(mirrorlist[i].plane);
        mirrorlist[i].plane->material.mirrormap = 0;

        i--;
    }

    spritesortcnt = localspritesortcnt;
    Bmemcpy(tsprite, localtsprite, sizeof(tspritetype) * spritesortcnt);
    maskwallcnt = localmaskwallcnt;
    Bmemcpy(maskwall, localmaskwall, sizeof(int16_t) * maskwallcnt);

    if (depth)
    {
        set_globalang(viewangle);

        if (mirrors[depth - 1].plane)
            display_mirror = 1;
        polymer_animatesprites();
        if (mirrors[depth - 1].plane)
            display_mirror = 0;

        buildgl_setDisabled(GL_CULL_FACE);
        renderDrawMasks();
        buildgl_setEnabled(GL_CULL_FACE);
    }
}

static void         polymer_emptybuckets(void)
{
    _prbucket *bucket = prbuckethead;

    if (pr_buckets == 0)
        return;

    buildgl_bindBuffer(GL_ARRAY_BUFFER, prmapvbo);
    glVertexPointer(3, GL_FLOAT, sizeof(_prvert), NULL);
    glTexCoordPointer(2, GL_FLOAT, sizeof(_prvert), (GLvoid *)(3 * sizeof(GLfloat)));

    buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, prindexringvbo);

    uint32_t indexcount = 0;
    while (bucket != NULL)
    {
        indexcount += bucket->count;

        bucket = bucket->next;
    }

    // ensure space in index ring, wrap otherwise
    if (indexcount + prindexringoffset >= (unsigned)prindexringsize)
    {
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        prindexring = (GLuint *)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, prindexringsize * sizeof(GLuint), GL_MAP_INVALIDATE_BUFFER_BIT | prindexringmapflags);
        prindexringoffset = 0;
    }

    // put indices in the ring, all at once
    bucket = prbuckethead;

    while (bucket != NULL)
    {
        if (bucket->count == 0)
        {
            bucket = bucket->next;
            continue;
        }

        memcpy(&prindexring[prindexringoffset], bucket->indices, bucket->count * sizeof(GLuint));

        bucket->indiceoffset = (GLuint*)(prindexringoffset * sizeof(GLuint));

        prindexringoffset += bucket->count;

        bucket = bucket->next;
    }

    bucket = prbuckethead;

    while (bucket != NULL)
    {
        if (bucket->count == 0)
        {
            bucket = bucket->next;
            continue;
        }

        int32_t materialbits = polymer_bindmaterial(&bucket->material, NULL, 0);
        glDrawElements(GL_TRIANGLES, bucket->count, GL_UNSIGNED_INT, bucket->indiceoffset);

        polymer_unbindmaterial(materialbits);

        bucket->count = 0;

        bucket = bucket->next;
    }

    buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);

    prcanbucket = 0;
}

static hashtable_t h_buckets      = { 2048, NULL };

static _prbucket*   polymer_findbucket(int16_t tilenum, char pal)
{
    char propstr[16];

    Bsprintf(propstr, "%d_%d", tilenum, pal);

    _prbucket *bucketptr = (_prbucket *)(prbuckethead ? hash_find(&h_buckets, propstr) : -1);

    // no buckets or no bucket found, create one
    if ((intptr_t)bucketptr == -1)
    {
        bucketptr = (_prbucket *)Xmalloc(sizeof (_prbucket));

        if (h_buckets.items == NULL)
            hash_init(&h_buckets);

        // insert, since most likely to use same pattern next frame
        // will need to reorder by MRU first every once in a while
        // or move to hashing lookup
        bucketptr->next = prbuckethead;
        prbuckethead = bucketptr;

        bucketptr->tilenum = tilenum;
        bucketptr->pal = pal;

        bucketptr->invalidmaterial = 1;

        bucketptr->count = 0;
        bucketptr->buffersize = 1024;
        bucketptr->indices = (GLuint *)Xmalloc(bucketptr->buffersize * sizeof(GLuint));

        hash_add(&h_buckets, propstr, (intptr_t)bucketptr, 1);
    }

    return bucketptr;
}

static void         polymer_bucketplane(_prplane* plane)
{
    _prbucket *bucketptr = plane->bucket;
    uint32_t neededindicecount;
    int32_t i;

    // we don't keep buffers for quads
    neededindicecount = (plane->indicescount == 0) ? 6 : plane->indicescount;

    // ensure size
    while (bucketptr->count + neededindicecount >= bucketptr->buffersize)
    {
        bucketptr->buffersize *= 2;
        bucketptr->indices = (GLuint *)Xrealloc(bucketptr->indices, bucketptr->buffersize * sizeof(GLuint));
    }

    // queue indices
    i = 0;

    if (plane->indicescount > 0)
    {
        while (i < plane->indicescount)
            bucketptr->indices[bucketptr->count++] = plane->indices[i++] + plane->mapvbo_vertoffset;
    }
    else
    {
        static const uint32_t quadindices[6] = { 0, 1, 2, 0, 2, 3 };

        while (i < 6)
            bucketptr->indices[bucketptr->count++] = quadindices[i++] + plane->mapvbo_vertoffset;
    }
}

static FORCE_INLINE _prprograminfo *polymer_getprogram(int32_t materialbits)
{
    intptr_t progptr = inthash_find(&prprogramtable, materialbits);
    return (progptr != -1) ? (_prprograminfo *)progptr : polymer_compileprogram(materialbits);
}

static void         polymer_drawplane(_prplane* plane)
{
    int32_t         materialbits;

    if (pr_nullrender >= 1) return;

    // debug code for drawing plane inverse TBN
//     glDisable(GL_TEXTURE_2D);
//     glBegin(GL_LINES);
//     glColor4f(1.0, 0.0, 0.0, 1.0);
//     glVertex3f(plane->buffer[0],
//                 plane->buffer[1],
//                 plane->buffer[2]);
//     glVertex3f(plane->buffer[0] + plane->t[0] * 50,
//                 plane->buffer[1] + plane->t[1] * 50,
//                 plane->buffer[2] + plane->t[2] * 50);
//     glColor4f(0.0, 1.0, 0.0, 1.0);
//     glVertex3f(plane->buffer[0],
//                 plane->buffer[1],
//                 plane->buffer[2]);
//     glVertex3f(plane->buffer[0] + plane->b[0] * 50,
//                 plane->buffer[1] + plane->b[1] * 50,
//                 plane->buffer[2] + plane->b[2] * 50);
//     glColor4f(0.0, 0.0, 1.0, 1.0);
//     glVertex3f(plane->buffer[0],
//                 plane->buffer[1],
//                 plane->buffer[2]);
//     glVertex3f(plane->buffer[0] + plane->n[0] * 50,
//                 plane->buffer[1] + plane->n[1] * 50,
//                 plane->buffer[2] + plane->n[2] * 50);
//     glEnd();
//     glEnable(GL_TEXTURE_2D);

    // debug code for drawing plane normals
//     glDisable(GL_TEXTURE_2D);
//     glBegin(GL_LINES);
//     glColor4f(1.0, 1.0, 1.0, 1.0);
//     glVertex3f(plane->buffer[0],
//                 plane->buffer[1],
//                 plane->buffer[2]);
//     glVertex3f(plane->buffer[0] + plane->plane[0] * 50,
//                 plane->buffer[1] + plane->plane[1] * 50,
//                 plane->buffer[2] + plane->plane[2] * 50);
//     glEnd();
//     glEnable(GL_TEXTURE_2D);

    if (pr_buckets && prcanbucket && plane->bucket)
    {
        polymer_bucketplane(plane);
        return;
    }

    glNormal3f((float)(plane->plane[0]), (float)(plane->plane[1]), (float)(plane->plane[2]));

    GLuint planevbo;
    GLintptr geomfbooffset;

    if (plane->mapvbo_vertoffset != (uint32_t)-1)
    {
        planevbo = prmapvbo;
        geomfbooffset = plane->mapvbo_vertoffset * sizeof(_prvert);
    }
    else
    {
        planevbo = plane->vbo;
        geomfbooffset = 0;
    }

    if (planevbo)
    {
        buildgl_bindBuffer(GL_ARRAY_BUFFER, planevbo);
        glVertexPointer(3, GL_FLOAT, sizeof(_prvert), (GLvoid *)(geomfbooffset));
        glTexCoordPointer(2, GL_FLOAT, sizeof(_prvert), (GLvoid *)(geomfbooffset + (3 * sizeof(GLfloat))));
        if (plane->indices)
            buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, plane->ivbo);
    } else {
        glVertexPointer(3, GL_FLOAT, sizeof(_prvert), &plane->buffer->x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(_prvert), &plane->buffer->u);
    }

    curlight = 0;
    do {
        materialbits = polymer_bindmaterial(&plane->material, plane->lights, plane->lightcount);
        auto &prprogram = *polymer_getprogram(materialbits);

        if (materialbits & (1 << PR_BIT_NORMAL_MAP))
        {
            glVertexAttrib3fv(prprogram.attrib_T, &plane->tbn[0][0]);
            glVertexAttrib3fv(prprogram.attrib_B, &plane->tbn[1][0]);
            glVertexAttrib3fv(prprogram.attrib_N, &plane->tbn[2][0]);
        }

        if (plane->indices)
        {
            if (planevbo)
                glDrawElements(GL_TRIANGLES, plane->indicescount, GL_UNSIGNED_SHORT, NULL);
            else
                glDrawElements(GL_TRIANGLES, plane->indicescount, GL_UNSIGNED_SHORT, plane->indices);
        } else
            glDrawArrays(GL_QUADS, 0, 4);

        polymer_unbindmaterial(materialbits);

        if (plane->lightcount && (!depth || mirrors[depth-1].plane))
            prlights[plane->lights[curlight]].flags.isinview = 1;

        curlight++;
    } while ((curlight < plane->lightcount) && (curlight < pr_maxlightpasses) && (!depth || mirrors[depth-1].plane));

    if (planevbo)
    {
        buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);
        if (plane->indices)
            buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

static inline void  polymer_inb4mirror(_prvert* buffer, const GLfloat* plane)
{
    float           pv;
    float           reflectionmatrix[16];

    pv = buffer->x * plane[0] +
         buffer->y * plane[1] +
         buffer->z * plane[2];

    reflectionmatrix[0] = 1 - (2 * plane[0] * plane[0]);
    reflectionmatrix[1] = -2 * plane[0] * plane[1];
    reflectionmatrix[2] = -2 * plane[0] * plane[2];
    reflectionmatrix[3] = 0;

    reflectionmatrix[4] = -2 * plane[0] * plane[1];
    reflectionmatrix[5] = 1 - (2 * plane[1] * plane[1]);
    reflectionmatrix[6] = -2 * plane[1] * plane[2];
    reflectionmatrix[7] = 0;

    reflectionmatrix[8] = -2 * plane[0] * plane[2];
    reflectionmatrix[9] = -2 * plane[1] * plane[2];
    reflectionmatrix[10] = 1 - (2 * plane[2] * plane[2]);
    reflectionmatrix[11] = 0;

    reflectionmatrix[12] = 2 * pv * plane[0];
    reflectionmatrix[13] = 2 * pv * plane[1];
    reflectionmatrix[14] = 2 * pv * plane[2];
    reflectionmatrix[15] = 1;

    glMultMatrixf(reflectionmatrix);

    glPushMatrix();
    glLoadMatrixf(curskymodelviewmatrix);
    glMultMatrixf(reflectionmatrix);
    glGetFloatv(GL_MODELVIEW_MATRIX, curskymodelviewmatrix);
    glPopMatrix();
}

static void         polymer_animatesprites(void)
{
    if (asi.animatesprites)
        asi.animatesprites(globalposx, globalposy, globalposz, fix16_to_int(viewangle), asi.smoothratio);
}

static void         polymer_freeboard(void)
{
    int32_t         i;

    i = 0;
    while (i < MAXSECTORS)
    {
        if (prsectors[i])
        {
            Xfree(prsectors[i]->verts);
            Xfree(prsectors[i]->floor.buffer);
            Xfree(prsectors[i]->ceil.buffer);
            Xfree(prsectors[i]->floor.indices);
            Xfree(prsectors[i]->ceil.indices);
            if (prsectors[i]->ceil.vbo) glDeleteBuffers(1, &prsectors[i]->ceil.vbo);
            if (prsectors[i]->ceil.ivbo) glDeleteBuffers(1, &prsectors[i]->ceil.ivbo);
            if (prsectors[i]->floor.vbo) glDeleteBuffers(1, &prsectors[i]->floor.vbo);
            if (prsectors[i]->floor.ivbo) glDeleteBuffers(1, &prsectors[i]->floor.ivbo);

            DO_FREE_AND_NULL(prsectors[i]);
        }

        i++;
    }

    i = 0;
    while (i < MAXWALLS)
    {
        if (prwalls[i])
        {
            Xfree(prwalls[i]->bigportal);
            Xfree(prwalls[i]->mask.buffer);
            Xfree(prwalls[i]->over.buffer);
            // Xfree(prwalls[i]->cap);
            Xfree(prwalls[i]->wall.buffer);
            if (prwalls[i]->wall.vbo) glDeleteBuffers(1, &prwalls[i]->wall.vbo);
            if (prwalls[i]->over.vbo) glDeleteBuffers(1, &prwalls[i]->over.vbo);
            if (prwalls[i]->mask.vbo) glDeleteBuffers(1, &prwalls[i]->mask.vbo);
            if (prwalls[i]->stuffvbo) glDeleteBuffers(1, &prwalls[i]->stuffvbo);

            DO_FREE_AND_NULL(prwalls[i]);
        }

        i++;
    }

    i = 0;
    while (i < MAXSPRITES)
    {
        if (prsprites[i])
        {
            Xfree(prsprites[i]->plane.buffer);
            if (prsprites[i]->plane.vbo) glDeleteBuffers(1, &prsprites[i]->plane.vbo);
            DO_FREE_AND_NULL(prsprites[i]);
        }

        i++;
    }

    i = 0;
    while (i < MAXTILES)
    {
        polymer_invalidateartmap(i);
        i++;
    }

    i = 0;
    while (i < MAXBASEPALS)
    {
        if (prbasepalmaps[i])
        {
            glDeleteTextures(1, &prbasepalmaps[i]);
            prbasepalmaps[i] = 0;
        }

        i++;
    }

    i = 0;
    while (i < MAXPALOOKUPS)
    {
        if (prlookups[i])
        {
            glDeleteTextures(1, &prlookups[i]);
            prlookups[i] = 0;
        }

        i++;
    }
}

// SECTORS
static int32_t      polymer_initsector(int16_t sectnum)
{
    usectorptr_t sec;
    _prsector*      s;

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Initializing sector %i", sectnum);

    sec = (usectorptr_t)&sector[sectnum];
    s = (_prsector *)Xcalloc(1, sizeof(_prsector));

    s->verts = (GLfloat *)Xcalloc(sec->wallnum, sizeof(GLfloat) * 3);
    s->floor.buffer = (_prvert *)Xcalloc(sec->wallnum, sizeof(_prvert));
    s->floor.vertcount = sec->wallnum;
    s->ceil.buffer = (_prvert *)Xcalloc(sec->wallnum, sizeof(_prvert));
    s->ceil.vertcount = sec->wallnum;

    glGenBuffers(1, &s->floor.vbo);
    glGenBuffers(1, &s->ceil.vbo);
    glGenBuffers(1, &s->floor.ivbo);
    glGenBuffers(1, &s->ceil.ivbo);

    buildgl_bindBuffer(GL_ARRAY_BUFFER, s->floor.vbo);
    glBufferData(GL_ARRAY_BUFFER, sec->wallnum * sizeof(GLfloat) * 5, NULL, mapvbousage);

    buildgl_bindBuffer(GL_ARRAY_BUFFER, s->ceil.vbo);
    glBufferData(GL_ARRAY_BUFFER, sec->wallnum * sizeof(GLfloat) * 5, NULL, mapvbousage);

    buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);

    s->flags.empty = 1; // let updatesector know that everything needs to go

    prsectors[sectnum] = s;

    if (pr_verbosity >= 2) VLOG_F(LOG_PR, "Initialized sector %i.", sectnum);

    return 1;
}

#define NBYTES_SECTOR_CEILINGSTAT_THROUGH_VISIBILITY \
    (offsetof(sectortype, visibility)+sizeof(sector[0].visibility) - offsetof(sectortype, ceilingstat))

static int32_t      polymer_updatesector(int16_t sectnum)
{
    if (pr_nullrender >= 3) return 0;

    _prsector *s = prsectors[sectnum];

    if (s == NULL)
    {
        if (pr_verbosity >= 1) VLOG_F(LOG_PR, "Can't update uninitialized sector %i.", sectnum);
        return -1;
    }
    auto sec = (usectorptr_t)&sector[sectnum];

    float secangsin = 2;
    float secangcos = 2;
    float relscalefactorfloor = 0.f;
    float relscalefactorceil = 0.f;

    int32_t needfloor = 0;
    int32_t wallinvalidate = 0;

    // geometry
    auto wal = (uwallptr_t)&wall[sec->wallptr];
    int i = 0;

    do
    {
        if ((-wal->x != Blrintf(s->verts[(i*3)+2])))
        {
            s->verts[(i*3)+2] = s->floor.buffer[i].z = s->ceil.buffer[i].z = -(float)wal->x;
            needfloor = wallinvalidate = 1;
        }

        if ((wal->y != Blrintf(s->verts[i*3])))
        {
            s->verts[i*3] = s->floor.buffer[i].x = s->ceil.buffer[i].x = (float)wal->y;
            needfloor = wallinvalidate = 1;
        }

        i++;
        wal++;
    }
    while (i < sec->wallnum);

    if (needfloor ||
            (s->flags.empty) ||
            (sec->floorz != s->floorz) ||
            (sec->ceilingz != s->ceilingz) ||
            (sec->floorheinum != s->floorheinum) ||
            (sec->ceilingheinum != s->ceilingheinum))
    {
        wallinvalidate = 1;

        wal = (uwallptr_t)&wall[sec->wallptr];
        i = 0;

        int32_t ceilz, florz;

        while (i < sec->wallnum)
        {
            getzsofslope(sectnum, wal->x, wal->y, &ceilz, &florz);
            s->floor.buffer[i].y = -(float)(florz) * (1.f/16.f);
            s->ceil.buffer[i].y = -(float)(ceilz) * (1.f/16.f);

            i++;
            wal++;
        }

        s->floorz = sec->floorz;
        s->ceilingz = sec->ceilingz;
        s->floorheinum = sec->floorheinum;
        s->ceilingheinum = sec->ceilingheinum;
    }
    else if (sec->visibility != s->visibility)
        wallinvalidate = 1;

    int16_t floorpicnum = sec->floorpicnum;
    tileUpdatePicnum(&floorpicnum, sectnum);

    int16_t ceilingpicnum = sec->ceilingpicnum;
    tileUpdatePicnum(&ceilingpicnum, sectnum);

    if ((!s->flags.empty) && (!needfloor) &&
            (floorpicnum == s->floorpicnum_anim) &&
            (ceilingpicnum == s->ceilingpicnum_anim) &&
#ifdef USE_STRUCT_TRACKERS
            (s->trackedrev == sectorchanged[sectnum]))
#else
            !Bmemcmp(&s->ceilingstat, &sec->ceilingstat, NBYTES_SECTOR_CEILINGSTAT_THROUGH_VISIBILITY))
#endif
        goto attributes;

    if (((sec->floorstat & 64) || (sec->ceilingstat & 64)) &&
        ((secangcos == 2) && (secangsin == 2)))
    {
        // relative-aligned floors apparently have a scaling bug in classic related to length of firstwall
        // lifting polymost code to replicate here
        vec2_t const xy = { wall[wall[sec->wallptr].point2].x - wall[sec->wallptr].x,
                            wall[wall[sec->wallptr].point2].y - wall[sec->wallptr].y };

        float len = Bsqrtf(xy.x * xy.x + xy.y * xy.y);

        if (sec->floorstat & 2 || sec->ceilingstat & 2)
        {
            int i = krecipasm(nsqrtasm(uhypsq(xy.x,xy.y)));
            float relscalefactor = i * (1.f/1073741824.f) * len;

            if (sec->floorstat & 2)
                relscalefactorfloor = relscalefactor;

            if (sec->ceilingstat & 2)
                relscalefactorceil = relscalefactor;
        }

        if (!(sec->floorstat & 2) || !(sec->ceilingstat & 2))
        {
            int i = nsqrtasm(uhypsq(xy.x,xy.y)); if (i == 0) i = 1024; else i = tabledivide32(1048576, i);
            float relscalefactor = i * (1.f/1048576.f) * len;

            if (!(sec->floorstat & 2))
                relscalefactorfloor = relscalefactor;

            if (!(sec->ceilingstat & 2))
                relscalefactorceil = relscalefactor;
        }

        // now sanely compute relative firstwall angle
        float arctan = atan2f(xy.y, xy.x) + (M_PI / 2.f);
        secangcos    = cosf(arctan);
        secangsin    = sinf(arctan);
    }

    wal = (uwallptr_t)&wall[sec->wallptr];
    i = 0;
    while (i < sec->wallnum)
    {
        int j = 2;

        uint16_t curstat   = sec->floorstat;
        auto     curbuffer = s->floor.buffer;
        int16_t  curpicnum = floorpicnum;
        float currelscalefactor = relscalefactorfloor;
        uint8_t curxpanning = sec->floorxpanning;
        uint8_t  curypanning = sec->floorypanning;
        float tex, tey;
        float xpancoef, ypancoef;
        int32_t heidiff;

        while (j)
        {
            if (j == 1)
            {
                curstat   = sec->ceilingstat;
                curbuffer = s->ceil.buffer;
                curpicnum = ceilingpicnum;

                curxpanning = sec->ceilingxpanning;
                curypanning = sec->ceilingypanning;
                currelscalefactor = relscalefactorceil;
            }

            if (!waloff[curpicnum])
                tileLoad(curpicnum);

            // relative texturing
            if (curstat & 64)
            {
                xpancoef = (float)(wal->x - wall[sec->wallptr].x) * currelscalefactor;
                ypancoef = (float)(wall[sec->wallptr].y - wal->y) * currelscalefactor;

                tex = xpancoef * secangsin + ypancoef * secangcos;
                tey = xpancoef * secangcos - ypancoef * secangsin;
            } else {
                tex = wal->x;
                tey = -wal->y;
            }

            if ((curstat & (2+64)) == (2+64))
            {
                heidiff = (int32_t)(curbuffer[i].y - curbuffer[0].y);
                // don't forget the sign, tey could be negative with concave sectors
                if (tey >= 0)
                    tey = Bsqrtf((float)((tey * tey) + (heidiff * heidiff)));
                else
                    tey = -Bsqrtf((float)((tey * tey) + (heidiff * heidiff)));
            }

            if (curstat & 4)
                swapfloat(&tex, &tey);

            if (curstat & 16) tex = -tex;
            if (curstat & 32) tey = -tey;

            float const scalecoef = (curstat & 8) ? 8.0f : 16.0f;

            if (curxpanning)
            {
                xpancoef = (float)(pow2long[picsiz[curpicnum] & 15]);
                xpancoef *= (float)(curxpanning) / (256.0f * (float)(tilesiz[curpicnum].x));
            }
            else
                xpancoef = 0;

            if (curypanning)
            {
                ypancoef = (float)(pow2long[picsiz[curpicnum] >> 4]);
                ypancoef *= (float)(curypanning) / (256.0f * (float)(tilesiz[curpicnum].y));
            }
            else
                ypancoef = 0;

            curbuffer[i].u = (tex / (scalecoef * tilesiz[curpicnum].x)) + xpancoef;
            curbuffer[i].v = (tey / (scalecoef * tilesiz[curpicnum].y)) + ypancoef;

            j--;
        }
        i++;
        wal++;
    }

    s->floorxpanning   = sec->floorxpanning;
    s->ceilingxpanning = sec->ceilingxpanning;
    s->floorypanning   = sec->floorypanning;
    s->ceilingypanning = sec->ceilingypanning;

#ifdef USE_STRUCT_TRACKERS
    s->trackedrev = sectorchanged[sectnum];
#endif

    i = -1;

attributes:
    if (i == -1 || wallinvalidate)
    {
        if (pr_nullrender < 2)
        {
            /*buildgl_bindBuffer(GL_ARRAY_BUFFER, s->floor.vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sec->wallnum * sizeof(GLfloat)* 5, s->floor.buffer);
            buildgl_bindBuffer(GL_ARRAY_BUFFER, s->ceil.vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sec->wallnum * sizeof(GLfloat)* 5, s->ceil.buffer);
            */

            s->floor.mapvbo_vertoffset = sec->wallptr * 2;
            s->ceil.mapvbo_vertoffset = s->floor.mapvbo_vertoffset + sec->wallnum;

            GLintptr sector_offset = s->floor.mapvbo_vertoffset * sizeof(_prvert);
            GLsizeiptr cur_sector_size = sec->wallnum * sizeof(_prvert);
            buildgl_bindBuffer(GL_ARRAY_BUFFER, prmapvbo);
            // floor
            glBufferSubData(GL_ARRAY_BUFFER, sector_offset, cur_sector_size, s->floor.buffer);
            // ceiling
            glBufferSubData(GL_ARRAY_BUFFER, sector_offset + cur_sector_size, cur_sector_size, s->ceil.buffer);
            buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }

    if ((!s->flags.empty) && (!s->flags.invalidtex) &&
            (floorpicnum == s->floorpicnum_anim) &&
            (ceilingpicnum == s->ceilingpicnum_anim) &&
            !Bmemcmp(&s->ceilingstat, &sec->ceilingstat, NBYTES_SECTOR_CEILINGSTAT_THROUGH_VISIBILITY))
        goto finish;

    s->floor.bucket = polymer_getbuildmaterial(&s->floor.material, floorpicnum, sec->floorpal, sec->floorshade, sec->visibility, (sec->floorstat & 384) ? DAMETH_MASK : DAMETH_NOMASK);

    if (sec->floorstat & 256) {
        if (sec->floorstat & 128) {
            s->floor.material.diffusemodulation[3] = transluctable[0];
        } else {
            s->floor.material.diffusemodulation[3] = transluctable[1];
        }
    }

    s->ceil.bucket = polymer_getbuildmaterial(&s->ceil.material, ceilingpicnum, sec->ceilingpal, sec->ceilingshade, sec->visibility, (sec->ceilingstat & 384) ? DAMETH_MASK : DAMETH_NOMASK);

    if (sec->ceilingstat & 256) {
        if (sec->ceilingstat & 128) {
            s->ceil.material.diffusemodulation[3] = transluctable[0];
        } else {
            s->ceil.material.diffusemodulation[3] = transluctable[1];
        }
    }

    s->flags.invalidtex = 0;

    // copy ceilingstat through visibility members
    Bmemcpy((char *)s + offsetof(_prsector, ceilingstat), (char const *)sec + offsetof(sectortype, ceilingstat), NBYTES_SECTOR_CEILINGSTAT_THROUGH_VISIBILITY);
    s->floorpicnum_anim = floorpicnum;
    s->ceilingpicnum_anim = ceilingpicnum;

finish:

    if (needfloor)
    {
        if (polymer_buildfloor(sectnum) != -1)
        {
            if (pr_nullrender < 2)
            {
                if (s->oldindicescount < s->indicescount)
                {
                    buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->floor.ivbo);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, s->indicescount * sizeof(GLushort), NULL, mapvbousage);
                    buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->ceil.ivbo);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, s->indicescount * sizeof(GLushort), NULL, mapvbousage);
                    s->oldindicescount = s->indicescount;
                }
                buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->floor.ivbo);
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, s->indicescount * sizeof(GLushort), s->floor.indices);
                buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->ceil.ivbo);
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, s->indicescount * sizeof(GLushort), s->ceil.indices);
                buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
        }
        else needfloor = -1;
    }

    if (wallinvalidate && needfloor != -1)
    {
        s->invalidid++;
        polymer_invalidatesectorlights(sectnum);
        polymer_computeplane(&s->floor);
        polymer_computeplane(&s->ceil);
    }

    s->flags.empty = 0;
    s->flags.uptodate = 1;

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Updated sector %i.", sectnum);

    return 0;
}

static int32_t      polymer_buildfloor(int16_t sectnum)
{
    // This function tesselates the floor/ceiling of a sector and stores the triangles in a display list.
    if (pr_verbosity >= 2) VLOG_F(LOG_PR, "Tessellating floor of sector %i", sectnum);

    _prsector *s = prsectors[sectnum];
    auto sec = (usectorptr_t)&sector[sectnum];

     if (s == NULL)
        return -1;

    int i=0, j=0, k=0;

    tessSetOption(prtess, TESS_CONSTRAINED_DELAUNAY_TRIANGULATION, r_pr_constrained);

    do
    {
        j++;
        if (wall[sec->wallptr+i].point2 < sec->wallptr+i)
        {
            tessAddContour(prtess, 3, s->verts + (3 * k), sizeof(float) * 3, j);
            k += j;
            j=0;
        }
    }
    while (++i < sec->wallnum);

    if (!tessTesselate(prtess, TESS_WINDING_POSITIVE, TESS_POLYGONS, 3, 3, nullptr))
    {
        debug_break();
        return -1;
    }

    int numvertices = tessGetVertexCount(prtess);
    int numelements = tessGetElementCount(prtess);
    auto elements = tessGetElements(prtess);
    auto index = tessGetVertexIndices(prtess);

    if (!numelements || !numvertices)
        return -1;

    // check for tessellation results that have undefined elements or less indices than we already had
    if (s->floor.indices)
    {
        if (numelements * 3 < s->indicescount)
            return 1;

        for (i = 0; i < numelements * 3; i++)
            if (index[elements[i]] == TESS_UNDEF && numelements * 3 <= s->indicescount)
                    return 1;
    }

    if (s->floor.indices == nullptr || s->indicescount != numelements * 3)
    {
        s->floor.indices = (GLushort *)Xrealloc(s->floor.indices, numelements * 3 * sizeof(GLushort));
        s->ceil.indices  = (GLushort *)Xrealloc(s->ceil.indices, numelements * 3 * sizeof(GLushort));
        s->indicescount  = numelements * 3;
    }

    for (j=0; j < s->indicescount; j++)
    {
        // FIXME: the tessellator is merging invalid features in the input,
        // which causes problems with some of the "Star Trek" doors in Duke.
        // the tessellator's behavior is correct, we just aren't handling it right yet

        s->ceil.indices[j] = index[elements[j]];

        if (index[elements[j]] == TESS_UNDEF)
        {
            LOG_F(WARNING, "sector %d is a very sad sector", sectnum);
            s->ceil.indices[j] = 0;
        }
        s->floor.indices[s->indicescount-j-1] = s->ceil.indices[j];
    }

    s->floor.indicescount = s->ceil.indicescount = s->indicescount;

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Tessellated floor of sector %i.", sectnum);

    return 1;
}

static void polymer_drawsector(int16_t sectnum, int32_t domasks)
{
    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Drawing sector %i", sectnum);

    if (automapping)
        bitmap_set(show2dsector, sectnum);

    bitmap_set(gotsector, sectnum);

    auto sec        = (usectorptr_t)&sector[sectnum];
    auto s          = prsectors[sectnum];
    int  queuedmask = FALSE;

    // If you're thinking of 'optimizing' the following logic, you'd better
    // provide compelling evidence that the generated code is more efficient
    // than what GCC can come up with on its own.

    int32_t draw = TRUE;

    // Draw masks regardless; avoid all non-masks TROR links
    if (sec->floorstat & 384) {
        draw = domasks;
    }
#ifdef YAX_ENABLE
    else if (yax_getbunch(sectnum, YAX_FLOOR) >= 0) {
        draw = FALSE;
    }
#endif

    // Parallaxed
    if (sec->floorstat & 1) {
        draw = FALSE;

        if (!prdidsky)
        {
            polymer_getsky(sec->floorpicnum, sec->floorpal, sec->floorshade);
            prdidsky = true;
        }
    }

    GLubyte oldcolor[4];

    // this relies on getzsofslope() not clamping the x/y to the sector coordinates.
    // by calling getzsofslope with coordinates outside of the sector in question,
    // the slope is extended infinitely and we can test against it to determine visibility.

    int32_t ceilZ, floorZ;
    getzsofslope(sectnum, globalposx, globalposy, &ceilZ, &floorZ);

    if (globalposz <= floorZ) {
        if (draw || (searchit == 2)) {
            if (searchit == 2) {
                polymer_drawsearchplane(&s->floor, oldcolor, 0x02, (GLubyte *) &sectnum);
            }
            else {
                calc_and_apply_fog(fogshade(sec->floorshade, sec->floorpal), sec->visibility,
                    get_floor_fogpal(sec));
                polymer_drawplane(&s->floor);
            }
        } else if (!domasks && cursectormaskcount && sec->floorstat & 384) {
            // If we just skipped a mask, queue it for later
            cursectormasks[(*cursectormaskcount)++] = sectnum;
            // Don't queue it twice if the ceiling is also a mask, though.
            queuedmask = TRUE;
        }
    }

    draw = TRUE;
    // Draw masks regardless; avoid all non-masks TROR links
    if (sec->ceilingstat & 384) {
        draw = domasks;
    }
#ifdef YAX_ENABLE
    else if (yax_getbunch(sectnum, YAX_CEILING) >= 0) {
        draw = FALSE;
    }
#endif

    // Parallaxed
    if (sec->ceilingstat & 1) {
        draw = FALSE;

        if (!prdidsky)
        {
            polymer_getsky(sec->ceilingpicnum, sec->ceilingpal, sec->ceilingshade);
            prdidsky = true;
        }
    }

    if (globalposz >= ceilZ) {
        if (draw || (searchit == 2)) {
            if (searchit == 2) {
                polymer_drawsearchplane(&s->ceil, oldcolor, 0x01, (GLubyte *) &sectnum);
            }
            else {
                calc_and_apply_fog(fogshade(sec->ceilingshade, sec->ceilingpal), sec->visibility,
                                   get_ceiling_fogpal(sec));
                polymer_drawplane(&s->ceil);
            }
        } else if (!domasks && !queuedmask && cursectormaskcount &&
                   (sec->ceilingstat & 384)) {
            // If we just skipped a mask, queue it for later
            cursectormasks[(*cursectormaskcount)++] = sectnum;
        }
    }

    if (pr_verbosity >= 4) VLOG_F(LOG_PR, "Finished drawing sector %i.", sectnum);
}

// WALLS
static int32_t      polymer_initwall(int16_t wallnum)
{
    _prwall         *w;

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Initializing wall %i", wallnum);

    w = (_prwall *)Xcalloc(1, sizeof(_prwall));

    if (w->mask.buffer == NULL) {
        w->mask.buffer = (_prvert *)Xmalloc(4 * sizeof(_prvert));
        w->mask.vertcount = 4;
    }
    if (w->bigportal == NULL)
        w->bigportal = (GLfloat *)Xmalloc(4 * sizeof(GLfloat) * 5);
    //if (w->cap == NULL)
    //    w->cap = (GLfloat *)Xmalloc(4 * sizeof(GLfloat) * 3);

    glGenBuffers(1, &w->wall.vbo);
    glGenBuffers(1, &w->over.vbo);
    glGenBuffers(1, &w->mask.vbo);
    glGenBuffers(1, &w->stuffvbo);

    buildgl_bindBuffer(GL_ARRAY_BUFFER, w->wall.vbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat) * 5, NULL, mapvbousage);

    buildgl_bindBuffer(GL_ARRAY_BUFFER, w->over.vbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat) * 5, NULL, mapvbousage);

    buildgl_bindBuffer(GL_ARRAY_BUFFER, w->mask.vbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat) * 5, NULL, mapvbousage);

    buildgl_bindBuffer(GL_ARRAY_BUFFER, w->stuffvbo);
    glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat) * 5, NULL, mapvbousage);

    buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);

    w->flags.empty = 1;

    prwalls[wallnum] = w;

    if (pr_verbosity >= 2) VLOG_F(LOG_PR, "Initialized wall %i.", wallnum);

    return 1;
}

// TODO: r_npotwallmode. Needs polymost_is_npotmode() handling among others.
#define DAMETH_WALL 0

static float calc_ypancoef(char curypanning, int16_t curpicnum, int32_t dopancor)
{
#ifdef NEW_MAP_FORMAT
    if (g_loadedMapVersion >= 10)
        return curypanning / 256.0f;
#endif
    {
        float ypancoef = (float)(pow2long[picsiz[curpicnum] >> 4]);

        if (ypancoef < tilesiz[curpicnum].y)
            ypancoef *= 2;

        if (dopancor)
        {
            int32_t yoffs = Blrintf((ypancoef - tilesiz[curpicnum].y) * (255.0f / ypancoef));
            if (curypanning > 256 - yoffs)
                curypanning -= yoffs;
        }

        ypancoef *= (float)curypanning / (256.0f * (float)tilesiz[curpicnum].y);

        return ypancoef;
    }
}

#define NBYTES_WALL_CSTAT_THROUGH_YPANNING \
    (offsetof(walltype, ypanning)+sizeof(wall[0].ypanning) - offsetof(walltype, cstat))

static void         polymer_updatewall(int16_t wallnum)
{
    int16_t         nwallnum, nnwallnum, curpicnum, wallpicnum, walloverpicnum, nwallpicnum;
    char            curxpanning, curypanning, underwall, overwall, curpal;
    int8_t          curshade;
    walltype        *wal;
    sectortype      *sec, *nsec;
    _prwall         *w;
    _prsector       *s, *ns;
    int32_t         xref, yref;
    float           ypancoef, dist;
    int32_t         i;
    uint32_t        invalid;
    int32_t         sectofwall = wallsect[wallnum];

    if (pr_nullrender >= 3) return;

    // yes, this function is messy and unefficient
    // it also works, bitches

    if (sectofwall < 0 || sectofwall >= numsectors || wallnum < 0 || wallnum > numwalls)
        return;

    sec = &sector[sectofwall];

    if (sec->wallptr > wallnum || wallnum >= (sec->wallptr + sec->wallnum))
        return; // yay, corrupt map

    if (sec->floorz == sec->ceilingz)
    {
        if (pr_verbosity >= 2)
            VLOG_F(LOG_PR, "Skipped wall %i.", wallnum);
        return;
    }

    wal = &wall[wallnum];
    nwallnum = wal->nextwall;

    w = prwalls[wallnum];
    s = prsectors[sectofwall];
    invalid = s->invalidid;
    if (nwallnum >= 0 && nwallnum < numwalls && wal->nextsector >= 0 && wal->nextsector < numsectors)
    {
        ns = prsectors[wal->nextsector];
        invalid += ns->invalidid;
        nsec = &sector[wal->nextsector];
    }
    else
    {
        ns = NULL;
        nsec = NULL;
    }

    if (w->wall.buffer == NULL) {
        w->wall.buffer = (_prvert *)Xcalloc(4, sizeof(_prvert));  // XXX
        w->wall.vertcount = 4;
    }

    globalorientation = wal->cstat;
    wallpicnum = wal->picnum;
    tileUpdatePicnum(&wallpicnum, wallnum+16384);

    walloverpicnum = wal->overpicnum;
    if (walloverpicnum>=0)
        tileUpdatePicnum(&walloverpicnum, wallnum+16384);

    if (nwallnum >= 0 && nwallnum < numwalls)
    {
        globalorientation = wall[wallnum].cstat;
        nwallpicnum = wall[nwallnum].picnum;
        tileUpdatePicnum(&nwallpicnum, wallnum+16384);
    }
    else
        nwallpicnum = 0;

    if ((!w->flags.empty) && (!w->flags.invalidtex) &&
            (w->invalidid == invalid) &&
            (wallpicnum == w->picnum_anim) &&
            (walloverpicnum == w->overpicnum_anim) &&
#ifdef USE_STRUCT_TRACKERS
            (w->trackedrev == wallchanged[wallnum]) &&
#else
            !Bmemcmp(&wal->cstat, &w->cstat, NBYTES_WALL_CSTAT_THROUGH_YPANNING) &&
#endif
            ((nwallnum < 0 || nwallnum > numwalls) ||
             ((nwallpicnum == w->nwallpicnum) &&
              (wall[nwallnum].xpanning == w->nwallxpanning) &&
              (wall[nwallnum].ypanning == w->nwallypanning) &&
              (wall[nwallnum].cstat == w->nwallcstat) &&
              (wall[nwallnum].shade == w->nwallshade))))
    {
        w->flags.uptodate = 1;
        return; // screw you guys I'm going home
    }
    else
    {
        w->invalidid = invalid;

        Bmemcpy((char *)w + offsetof(_prwall, cstat), (char *)wal + offsetof(walltype, cstat), NBYTES_WALL_CSTAT_THROUGH_YPANNING);

        w->picnum_anim = wallpicnum;
        w->overpicnum_anim = walloverpicnum;
#ifdef USE_STRUCT_TRACKERS
        w->trackedrev = wallchanged[wallnum];
#endif
        if (nwallnum >= 0 && nwallnum < numwalls)
        {
            w->nwallpicnum = nwallpicnum;
            w->nwallxpanning = wall[nwallnum].xpanning;
            w->nwallypanning = wall[nwallnum].ypanning;
            w->nwallcstat = wall[nwallnum].cstat;
            w->nwallshade = wall[nwallnum].shade;
        }
    }

    w->underover = underwall = overwall = 0;

    if (wal->cstat & 8)
        xref = 1;
    else
        xref = 0;

    if ((unsigned)wal->nextsector >= (unsigned)numsectors || !ns)
    {
        Bmemcpy(w->wall.buffer, &s->floor.buffer[wallnum - sec->wallptr], sizeof(GLfloat) * 3);
        Bmemcpy(&w->wall.buffer[1], &s->floor.buffer[wal->point2 - sec->wallptr], sizeof(GLfloat) * 3);
        Bmemcpy(&w->wall.buffer[2], &s->ceil.buffer[wal->point2 - sec->wallptr], sizeof(GLfloat) * 3);
        Bmemcpy(&w->wall.buffer[3], &s->ceil.buffer[wallnum - sec->wallptr], sizeof(GLfloat) * 3);

        if (wal->nextsector < 0)
            curpicnum = wallpicnum;
        else
            curpicnum = walloverpicnum;

        w->wall.bucket = polymer_getbuildmaterial(&w->wall.material, curpicnum, wal->pal, wal->shade, sec->visibility, DAMETH_WALL);

        if (wal->cstat & 4)
            yref = sec->floorz;
        else
            yref = sec->ceilingz;

        if ((wal->cstat & 32) && (wal->nextsector >= 0))
        {
            if ((!(wal->cstat & 2) && (wal->cstat & 4)) || ((wal->cstat & 2) && (wall[nwallnum].cstat & 4)))
                yref = sec->ceilingz;
            else
                yref = nsec->floorz;
        }

        if (wal->ypanning)
            // white (but not 1-way)
            ypancoef = calc_ypancoef(wal->ypanning, curpicnum, !(wal->cstat & 4));
        else
            ypancoef = 0;

        i = 0;
        while (i < 4)
        {
            if ((i == 0) || (i == 3))
                dist = (float)xref;
            else
                dist = (float)(xref == 0);

            w->wall.buffer[i].u = ((dist * 8.0f * wal->xrepeat) + wal->xpanning) / (float)(tilesiz[curpicnum].x);
            w->wall.buffer[i].v = (-(float)(yref + (w->wall.buffer[i].y * 16)) / ((tilesiz[curpicnum].y * 2048.0f) / (float)(wal->yrepeat))) + ypancoef;

            if (wal->cstat & 256) w->wall.buffer[i].v = -w->wall.buffer[i].v;

            i++;
        }

        w->underover |= 1;
    }
    else
    {
        nnwallnum = wall[nwallnum].point2;

        if ((s->floor.buffer[wallnum - sec->wallptr].y < ns->floor.buffer[nnwallnum - nsec->wallptr].y) ||
            (s->floor.buffer[wal->point2 - sec->wallptr].y < ns->floor.buffer[nwallnum - nsec->wallptr].y))
            underwall = 1;

        if ((underwall) || (wal->cstat & 16) || (wal->cstat & 32))
        {
            int32_t refwall;

            if (s->floor.buffer[wallnum - sec->wallptr].y < ns->floor.buffer[nnwallnum - nsec->wallptr].y)
                Bmemcpy(w->wall.buffer, &s->floor.buffer[wallnum - sec->wallptr], sizeof(GLfloat) * 3);
            else
                Bmemcpy(w->wall.buffer, &ns->floor.buffer[nnwallnum - nsec->wallptr], sizeof(GLfloat) * 3);
            Bmemcpy(&w->wall.buffer[1], &s->floor.buffer[wal->point2 - sec->wallptr], sizeof(GLfloat) * 3);
            Bmemcpy(&w->wall.buffer[2], &ns->floor.buffer[nwallnum - nsec->wallptr], sizeof(GLfloat) * 3);
            Bmemcpy(&w->wall.buffer[3], &ns->floor.buffer[nnwallnum - nsec->wallptr], sizeof(GLfloat) * 3);

            if (wal->cstat & 2)
                refwall = nwallnum;
            else
                refwall = wallnum;

            curpicnum = (wal->cstat & 2) ? nwallpicnum : wallpicnum;
            curpal = wall[refwall].pal;
            curshade = wall[refwall].shade;
            curxpanning = wall[refwall].xpanning;
            curypanning = wall[refwall].ypanning;

            w->wall.bucket = polymer_getbuildmaterial(&w->wall.material, curpicnum, curpal, curshade, sec->visibility, DAMETH_WALL);

            if (!(wall[refwall].cstat&4))
                yref = nsec->floorz;
            else
                yref = sec->ceilingz;

            if (curypanning)
                // under
                ypancoef = calc_ypancoef(curypanning, curpicnum, !(wall[refwall].cstat & 4));
            else
                ypancoef = 0;

            i = 0;
            while (i < 4)
            {
                if ((i == 0) || (i == 3))
                    dist = (float)xref;
                else
                    dist = (float)(xref == 0);

                w->wall.buffer[i].u = ((dist * 8.0f * wal->xrepeat) + curxpanning) / (float)(tilesiz[curpicnum].x);
                w->wall.buffer[i].v = (-(float)(yref + (w->wall.buffer[i].y * 16)) / ((tilesiz[curpicnum].y * 2048.0f) / (float)(wal->yrepeat))) + ypancoef;

                if ((!(wal->cstat & 2) && (wal->cstat & 256)) ||
                    ((wal->cstat & 2) && (wall[nwallnum].cstat & 256)))
                    w->wall.buffer[i].v = -w->wall.buffer[i].v;

                i++;
            }

            if (underwall)
                w->underover |= 1;

            Bmemcpy(w->mask.buffer, &w->wall.buffer[3], sizeof(GLfloat) * 5);
            Bmemcpy(&w->mask.buffer[1], &w->wall.buffer[2], sizeof(GLfloat) * 5);
        }
        else
        {
            Bmemcpy(w->mask.buffer, &s->floor.buffer[wallnum - sec->wallptr], sizeof(GLfloat) * 5);
            Bmemcpy(&w->mask.buffer[1], &s->floor.buffer[wal->point2 - sec->wallptr], sizeof(GLfloat) * 5);
        }

        if ((s->ceil.buffer[wallnum - sec->wallptr].y > ns->ceil.buffer[nnwallnum - nsec->wallptr].y) ||
            (s->ceil.buffer[wal->point2 - sec->wallptr].y > ns->ceil.buffer[nwallnum - nsec->wallptr].y))
            overwall = 1;

        if ((overwall) || (wal->cstat & 48))
        {
            if (w->over.buffer == NULL) {
                w->over.buffer = (_prvert *)Xmalloc(4 * sizeof(_prvert));
                w->over.vertcount = 4;
            }

            Bmemcpy(w->over.buffer, &ns->ceil.buffer[nnwallnum - nsec->wallptr], sizeof(GLfloat) * 3);
            Bmemcpy(&w->over.buffer[1], &ns->ceil.buffer[nwallnum - nsec->wallptr], sizeof(GLfloat) * 3);
            if (s->ceil.buffer[wal->point2 - sec->wallptr].y > ns->ceil.buffer[nwallnum - nsec->wallptr].y)
                Bmemcpy(&w->over.buffer[2], &s->ceil.buffer[wal->point2 - sec->wallptr], sizeof(GLfloat) * 3);
            else
                Bmemcpy(&w->over.buffer[2], &ns->ceil.buffer[nwallnum - nsec->wallptr], sizeof(GLfloat) * 3);
            Bmemcpy(&w->over.buffer[3], &s->ceil.buffer[wallnum - sec->wallptr], sizeof(GLfloat) * 3);

            curpicnum = wallpicnum;

            w->over.bucket = polymer_getbuildmaterial(&w->over.material, curpicnum, wal->pal, wal->shade, sec->visibility, DAMETH_WALL);

            if (wal->cstat & 48)
            {
                // mask
                w->mask.bucket = polymer_getbuildmaterial(&w->mask.material, walloverpicnum, wal->pal, wal->shade, sec->visibility, DAMETH_WALL | ((wal->cstat & 48) == 48 ? DAMETH_NOMASK : DAMETH_MASK));

                if (wal->cstat & 128)
                {
                    if (wal->cstat & 512)
                        w->mask.material.diffusemodulation[3] = transluctable[0];
                    else
                        w->mask.material.diffusemodulation[3] = transluctable[1];
                }
            }

            if (wal->cstat & 4)
                yref = sec->ceilingz;
            else
                yref = nsec->ceilingz;

            if (wal->ypanning)
                // over
                ypancoef = calc_ypancoef(wal->ypanning, curpicnum, wal->cstat & 4);
            else
                ypancoef = 0;

            i = 0;
            while (i < 4)
            {
                if ((i == 0) || (i == 3))
                    dist = (float)xref;
                else
                    dist = (float)(xref == 0);

                w->over.buffer[i].u = ((dist * 8.0f * wal->xrepeat) + wal->xpanning) / (float)(tilesiz[curpicnum].x);
                w->over.buffer[i].v = (-(float)(yref + (w->over.buffer[i].y * 16)) / ((tilesiz[curpicnum].y * 2048.0f) / (float)(wal->yrepeat))) + ypancoef;

                if (wal->cstat & 256) w->over.buffer[i].v = -w->over.buffer[i].v;

                i++;
            }

            if (overwall)
                w->underover |= 2;

            Bmemcpy(&w->mask.buffer[2], &w->over.buffer[1], sizeof(GLfloat) * 5);
            Bmemcpy(&w->mask.buffer[3], &w->over.buffer[0], sizeof(GLfloat) * 5);

            if ((wal->cstat & 16) || (wal->cstat & 32))
            {
                const int botSwap = (wal->cstat & 4);

                if (wal->cstat & 32)
                {
                    // 1-sided wall
                    if (nsec)
                        yref = botSwap ? sec->ceilingz : nsec->ceilingz;
                    else
                        yref = botSwap ? sec->floorz : sec->ceilingz;
                }
                else
                {
                    // masked wall
                    if (botSwap)
                        yref = min(sec->floorz, nsec->floorz);
                    else
                        yref = max(sec->ceilingz, nsec->ceilingz);
                }

                curpicnum = walloverpicnum;

                if (wal->ypanning)
                    // mask / 1-way
                    ypancoef = calc_ypancoef(wal->ypanning, curpicnum, 0);
                else
                    ypancoef = 0;

                i = 0;
                while (i < 4)
                {
                    if ((i == 0) || (i == 3))
                        dist = (float)xref;
                    else
                        dist = (float)(xref == 0);

                    w->mask.buffer[i].u = ((dist * 8.0f * wal->xrepeat) + wal->xpanning) / (float)(tilesiz[curpicnum].x);
                    w->mask.buffer[i].v = (-(float)(yref + (w->mask.buffer[i].y * 16)) / ((tilesiz[curpicnum].y * 2048.0f) / (float)(wal->yrepeat))) + ypancoef;

                    if (wal->cstat & 256) w->mask.buffer[i].v = -w->mask.buffer[i].v;

                    i++;
                }
            }
        }
        else
        {
            Bmemcpy(&w->mask.buffer[2], &s->ceil.buffer[wal->point2 - sec->wallptr], sizeof(GLfloat) * 5);
            Bmemcpy(&w->mask.buffer[3], &s->ceil.buffer[wallnum - sec->wallptr], sizeof(GLfloat) * 5);
        }
    }

    // make sure shade color handling is correct below XXX
    if (wal->nextsector < 0)
        Bmemcpy(w->mask.buffer, w->wall.buffer, sizeof(_prvert) * 4);

    Bmemcpy(w->bigportal, &s->floor.buffer[wallnum - sec->wallptr], sizeof(GLfloat) * 3);
    Bmemcpy(&w->bigportal[5], &s->floor.buffer[wal->point2 - sec->wallptr], sizeof(GLfloat) * 3);
    Bmemcpy(&w->bigportal[10], &s->ceil.buffer[wal->point2 - sec->wallptr], sizeof(GLfloat) * 3);
    Bmemcpy(&w->bigportal[15], &s->ceil.buffer[wallnum - sec->wallptr], sizeof(GLfloat) * 3);

    //Bmemcpy(&w->cap[0], &s->ceil.buffer[wallnum - sec->wallptr], sizeof(GLfloat) * 3);
    //Bmemcpy(&w->cap[3], &s->ceil.buffer[wal->point2 - sec->wallptr], sizeof(GLfloat) * 3);
    //Bmemcpy(&w->cap[6], &s->ceil.buffer[wal->point2 - sec->wallptr], sizeof(GLfloat) * 3);
    //Bmemcpy(&w->cap[9], &s->ceil.buffer[wallnum - sec->wallptr], sizeof(GLfloat) * 3);
    //w->cap[7] += 1048576; // this number is the result of 1048574 + 2
    //w->cap[10] += 1048576; // this one is arbitrary

    if (w->underover & 1)
        polymer_computeplane(&w->wall);
    if (w->underover & 2)
        polymer_computeplane(&w->over);
    polymer_computeplane(&w->mask);

    if (pr_nullrender < 2)
    {
        const GLintptr thiswalloffset = prwalldataoffset + (prwalldatasize * wallnum);
        const GLintptr thisoveroffset = thiswalloffset + proneplanesize;
        const GLintptr thismaskoffset = thisoveroffset + proneplanesize;
        buildgl_bindBuffer(GL_ARRAY_BUFFER, prmapvbo);
        glBufferSubData(GL_ARRAY_BUFFER, thiswalloffset, proneplanesize, w->wall.buffer);
        buildgl_bindBuffer(GL_ARRAY_BUFFER, prmapvbo);
        if (w->over.buffer)
            glBufferSubData(GL_ARRAY_BUFFER, thisoveroffset, proneplanesize, w->over.buffer);
        buildgl_bindBuffer(GL_ARRAY_BUFFER, prmapvbo);
        glBufferSubData(GL_ARRAY_BUFFER, thismaskoffset, proneplanesize, w->mask.buffer);
        buildgl_bindBuffer(GL_ARRAY_BUFFER, w->stuffvbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(GLfloat)* 5, w->bigportal);
        //glBufferSubData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat)* 5, 4 * sizeof(GLfloat)* 3, w->cap);
        buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);

        w->wall.mapvbo_vertoffset = thiswalloffset / sizeof(_prvert);
        w->over.mapvbo_vertoffset = thisoveroffset / sizeof(_prvert);
        w->mask.mapvbo_vertoffset = thismaskoffset / sizeof(_prvert);
    }

    w->flags.empty = 0;
    w->flags.uptodate = 1;
    w->flags.invalidtex = 0;

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Updated wall %i.", wallnum);
}

static void         polymer_drawwall(int16_t sectnum, int16_t wallnum)
{
    usectorptr_t sec;
    walltype        *wal;
    _prwall         *w;
    GLubyte         oldcolor[4];
    int32_t         parallaxedfloor = 0, parallaxedceiling = 0;

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Drawing wall %i", wallnum);

    sec = (usectorptr_t)&sector[sectnum];
    wal = &wall[wallnum];
    w = prwalls[wallnum];

    if ((sec->floorstat & 1) && (wal->nextsector >= 0) &&
        (sector[wal->nextsector].floorstat & 1))
        parallaxedfloor = 1;

    if ((sec->ceilingstat & 1) && (wal->nextsector >= 0) &&
        (sector[wal->nextsector].ceilingstat & 1))
        parallaxedceiling = 1;

    calc_and_apply_fog(fogshade(wal->shade, wal->pal), sec->visibility, get_floor_fogpal(sec));

    if ((w->underover & 1) && (!parallaxedfloor || (searchit == 2)))
    {
        if (searchit == 2) {
            polymer_drawsearchplane(&w->wall, oldcolor, 0x05, (GLubyte *) &wallnum);
        }
        else
            polymer_drawplane(&w->wall);
    }

    if ((w->underover & 2) && (!parallaxedceiling || (searchit == 2)))
    {
        if (searchit == 2) {
            polymer_drawsearchplane(&w->over, oldcolor, 0x00, (GLubyte *) &wallnum);
        }
        else
            polymer_drawplane(&w->over);
    }

    if ((wall[wallnum].cstat & 32) && (wall[wallnum].nextsector >= 0))
    {
        if (searchit == 2) {
            polymer_drawsearchplane(&w->mask, oldcolor, 0x04, (GLubyte *) &wallnum);
        }
        else
            polymer_drawplane(&w->mask);
    }

    //if (!searchit && (sector[sectnum].ceilingstat & 1) &&
    //    ((wall[wallnum].nextsector < 0) ||
    //    !(sector[wall[wallnum].nextsector].ceilingstat & 1)))
    //{
    //    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    //    buildgl_bindBuffer(GL_ARRAY_BUFFER, w->stuffvbo);
    //    glVertexPointer(3, GL_FLOAT, 0, (const GLvoid*)(4 * sizeof(GLfloat) * 5));

    //    glDrawArrays(GL_QUADS, 0, 4);

    //    buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);

    //    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    //}

    if (automapping)
        bitmap_set(show2dwall, wallnum);

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Finished drawing wall %i.", wallnum);
}

// HSR
static void         polymer_computeplane(_prplane* p)
{
    GLfloat         vec1[5], vec2[5], norm, r;// BxN[3], NxT[3], TxB[3];
    int32_t         i;
    _prvert*        buffer;
    GLfloat*        plane;

    if (p->indices && (p->indicescount < 3))
        return; // corrupt sector (E3L4, I'm looking at you)

    buffer = p->buffer;
    plane = p->plane;

    i = 0;
    do
    {
        int const imax = (p->indices) ? (p->indicescount) : (p->vertcount);
        int const index0 = INDICE(0);
        if (index0 > imax) return;
        int const index1 = INDICE(1);
        if (index1 > imax) return;
        int const index2 = INDICE(2);
        if (index2 > imax) return;

        vec1[0] = buffer[index1].x - buffer[index0].x; //x1
        vec1[1] = buffer[index1].y - buffer[index0].y; //y1
        vec1[2] = buffer[index1].z - buffer[index0].z; //z1
        vec1[3] = buffer[index1].u - buffer[index0].u; //s1
        vec1[4] = buffer[index1].v - buffer[index0].v; //t1

        vec2[0] = buffer[index2].x - buffer[index1].x; //x2
        vec2[1] = buffer[index2].y - buffer[index1].y; //y2
        vec2[2] = buffer[index2].z - buffer[index1].z; //z2
        vec2[3] = buffer[index2].u - buffer[index1].u; //s2
        vec2[4] = buffer[index2].v - buffer[index1].v; //t2

        buildgl_crossproduct(vec2, vec1, plane);

        norm = plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2];

        // hack to work around a precision issue with slopes
        if (norm >= 15000)
        {
            float tangent[3][3];
            float det;

            // normalize the normal/plane equation and calculate its plane norm
            norm = -1.f/Bsqrtf(norm);
            plane[0] *= norm;
            plane[1] *= norm;
            plane[2] *= norm;
            plane[3] = -(plane[0] * buffer->x + plane[1] * buffer->y + plane[2] * buffer->z);

            // calculate T and B
            r = 1.f / (vec1[3] * vec2[4] - vec2[3] * vec1[4]);

            // tangent
            tangent[0][0] = (vec2[4] * vec1[0] - vec1[4] * vec2[0]) * r;
            tangent[0][1] = (vec2[4] * vec1[1] - vec1[4] * vec2[1]) * r;
            tangent[0][2] = (vec2[4] * vec1[2] - vec1[4] * vec2[2]) * r;

            buildgl_normalize(&tangent[0][0]);

            // bitangent
            tangent[1][0] = (vec1[3] * vec2[0] - vec2[3] * vec1[0]) * r;
            tangent[1][1] = (vec1[3] * vec2[1] - vec2[3] * vec1[1]) * r;
            tangent[1][2] = (vec1[3] * vec2[2] - vec2[3] * vec1[2]) * r;

            buildgl_normalize(&tangent[1][0]);

            // normal
            tangent[2][0] = plane[0];
            tangent[2][1] = plane[1];
            tangent[2][2] = plane[2];

            INVERT_3X3(p->tbn, det, tangent);

            break;
        }
        i+= (p->indices) ? 3 : 1;
    }
    while ((p->indices && i < p->indicescount) ||
          (!p->indices && i < p->vertcount));
}


static FORCE_INLINE void polymer_transformpoint(const float *inpos, float *pos, const float *matrix)
{
    pos[0] = inpos[0] * matrix[0] +
             inpos[1] * matrix[4] +
             inpos[2] * matrix[8] +
                      + matrix[12];
    pos[1] = inpos[0] * matrix[1] +
             inpos[1] * matrix[5] +
             inpos[2] * matrix[9] +
                      + matrix[13];
    pos[2] = inpos[0] * matrix[2] +
             inpos[1] * matrix[6] +
             inpos[2] * matrix[10] +
                      + matrix[14];
}

static inline void  polymer_pokesector(int16_t const sectnum)
{
    auto sec = (usectorptr_t)&sector[sectnum];
    auto s   = prsectors[sectnum];
    auto wal = (uwallptr_t)&wall[sec->wallptr];
    int  i   = 0;

    if (!s->flags.uptodate)
        polymer_updatesector(sectnum);

    do
    {
        if ((wal->nextsector >= 0) && (!prsectors[wal->nextsector]->flags.uptodate))
            polymer_updatesector(wal->nextsector);
        if (!prwalls[sec->wallptr + i]->flags.uptodate)
            polymer_updatewall(sec->wallptr + i);
        wal++;
    }
    while (++i < sec->wallnum);
}

static void         polymer_extractfrustum(GLfloat* modelview, GLfloat* projection, float* frustum)
{
    GLfloat         matrix[16];
    int32_t         i;

    glMatrixMode(GL_TEXTURE);
    glLoadMatrixf(projection);
    glMultMatrixf(modelview);
    glGetFloatv(GL_TEXTURE_MATRIX, matrix);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    i = 0;
    do
    {
        uint32_t ii = i<<2, iii = (i<<2) + 3;

        frustum[i] = matrix[iii] + matrix[ii];               // left
        frustum[i + 4] = matrix[iii] - matrix[ii];           // right
        frustum[i + 8] = matrix[iii] - matrix[ii + 1];     // top
        frustum[i + 12] = matrix[iii] + matrix[ii + 1];    // bottom
        frustum[i + 16] = matrix[iii] - matrix[ii + 2];    // far
    }
    while (++i < 4);

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Frustum extracted.");
}

static inline int polymer_planeinfrustum(_prplane const &plane, const float* frustum)
{
    int i=4, j, k;

    do
    {
        int const ii = i * 4;
        j = k = plane.vertcount - 1;

        do
        {
            k -= ((frustum[ii+0] * plane.buffer[j].x +
                   frustum[ii+1] * plane.buffer[j].y +
                   frustum[ii+2] * plane.buffer[j].z +
                   frustum[ii+3]) < 0.f);
        }
        while (j--);

        if (k == -1)
            return 0; // OUT !
    }
    while (i--);

    return 1;
}

static inline void  polymer_scansprites(int16_t sectnum, tspriteptr_t localtsprite, int32_t* localspritesortcnt)
{
    for (int i = headspritesect[sectnum];i >=0;i = nextspritesect[i])
    {
        auto spr = (uspriteptr_t)&sprite[i];
        if ((((spr->cstat&0x8000) == 0) || (showinvisibility)) &&
                (spr->xrepeat > 0) && (spr->yrepeat > 0) &&
                (*localspritesortcnt < MAXSPRITESONSCREEN))
        {
            // this function's localtsprite is either the tsprite global or
            // polymer_drawroom's locattsprite, so no aliasing
            renderMakeTSpriteFromSprite(&localtsprite[(*localspritesortcnt)++], i);
        }
    }
}

void                polymer_updatesprite(int32_t snum)
{
    int32_t         xsize, ysize, i, j;
    int32_t         tilexoff, tileyoff, xoff, yoff, centeryoff=0, heinum;
    auto const      tspr = tspriteptr[snum];
    float           xratio, yratio, ang;
    float           spos[3];
    const _prvert   *inbuffer;
    uint8_t         flipu, flipv;
    _prsprite       *s;

    const uint32_t cs = tspr->cstat;
    const uint32_t alignmask = (cs & CSTAT_SPRITE_ALIGNMENT);
    const uint8_t flooraligned = !!(cs & CSTAT_SPRITE_ALIGNMENT_FLOOR);

    if (pr_nullrender >= 3) return;

    if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Updating sprite %i", snum);

    int32_t const curpicnum = tspr->picnum;

    if (tspr->owner < 0 || curpicnum < 0) return;

    s = prsprites[tspr->owner];

    if (s == NULL)
    {
        s = prsprites[tspr->owner] = (_prsprite *)Xcalloc(1, sizeof(_prsprite));

        s->plane.buffer = (_prvert *)Xcalloc(4, sizeof(_prvert));  // XXX
        s->plane.vertcount = 4;
        s->plane.mapvbo_vertoffset = -1;
        s->hash = 0xDEADBEEF;
    }

    if ((tspr->cstat & CSTAT_SPRITE_ALIGNMENT) && !s->plane.vbo)
    {
        if (pr_nullrender < 2)
        {
            glGenBuffers(1, &s->plane.vbo);
            buildgl_bindBuffer(GL_ARRAY_BUFFER, s->plane.vbo);
            glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(_prvert), NULL, mapvbousage);
        }
    }

    if ((tspr->cstat & CSTAT_SPRITE_ALIGNMENT) && searchit != 2)
    {
        uint32_t const changed = XXH3_64bits((uint8_t *) tspr, offsetof(spritetype, owner));

        if (changed == s->hash)
            return;

        s->hash = changed;
    }

    polymer_getbuildmaterial(&s->plane.material, curpicnum, tspr->pal, tspr->shade,
                             sector[tspr->sectnum].visibility, DAMETH_MASK | DAMETH_CLAMPED);

    if (tspr->cstat & 2)
    {
        if (tspr->cstat & 512)
            s->plane.material.diffusemodulation[3] = transluctable[0];
        else
            s->plane.material.diffusemodulation[3] = transluctable[1];
    }

    float f = s->plane.material.diffusemodulation[3] * (1.0f - spriteext[tspr->owner].alpha);
    s->plane.material.diffusemodulation[3] = (GLubyte)f;

    if (searchit == 2)
    {
        polymer_setupdiffusemodulation(&s->plane, 0x03, (GLubyte *) &tspr->owner);
        s->hash = 0xDEADBEEF;
    }

    if ((tspr->cstat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_FACING)
        xratio = (float)(tspr->xrepeat) * 0.20f; // 32 / 160
    else
        xratio = (float)(tspr->xrepeat) * 0.25f;

    yratio = (float)(tspr->yrepeat) * 0.25f;

    xsize = tilesiz[curpicnum].x;
    ysize = tilesiz[curpicnum].y;

    if (usehightile && h_xsize[curpicnum])
    {
        xsize = h_xsize[curpicnum];
        ysize = h_ysize[curpicnum];
    }

    xsize = (int32_t)(xsize * xratio);
    ysize = (int32_t)(ysize * yratio);

    tilexoff = (usehightile && h_xsize[curpicnum]) ? h_xoffs[curpicnum] : picanm[curpicnum].xofs;
    tileyoff = (usehightile && h_xsize[curpicnum]) ? h_yoffs[curpicnum] : picanm[curpicnum].yofs;

    heinum = tspriteGetSlope(tspr);

    if (heinum == 0)
    {
        tilexoff += (int32_t)tspr->xoffset;
        tileyoff += (int32_t)tspr->yoffset;
    }

    xoff = (int32_t)(tilexoff * xratio);
    yoff = (int32_t)(tileyoff * yratio);

    if ((tspr->cstat & 128) && !flooraligned)
    {
        if (alignmask == CSTAT_SPRITE_ALIGNMENT_FACING)
            yoff -= ysize / 2;
        else
            centeryoff = ysize / 2;
    }

    spos[0] = (float)tspr->y;
    spos[1] = -(float)(tspr->z) * (1.f/16.f);
    spos[2] = -(float)tspr->x;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    inbuffer = vertsprite;

    {
        const uint8_t xflip = !!(cs & CSTAT_SPRITE_XFLIP);
        const uint8_t yflip = !!(cs & CSTAT_SPRITE_YFLIP);

        // Initially set flipu and flipv.
        flipu = (xflip ^ flooraligned);
        flipv = (yflip && !flooraligned);

        if (pr_billboardingmode && alignmask == CSTAT_SPRITE_ALIGNMENT_FACING)
        {
            // do surgery on the face tspr to make it look like a wall sprite
            tspr->cstat |= CSTAT_SPRITE_ALIGNMENT_WALL;
            tspr->ang = (fix16_to_int(viewangle) + 1024) & 2047;
        }

        if (flipu)
            xoff = -xoff;

        if (yflip && alignmask != CSTAT_SPRITE_ALIGNMENT_FACING)
            yoff = -yoff;
    }

    switch (tspr->cstat & CSTAT_SPRITE_ALIGNMENT)
    {
    case CSTAT_SPRITE_ALIGNMENT_FACING:
        ang = (float)((fix16_to_int(viewangle)) & 2047) * (360.f/2048.f);

        glTranslatef(spos[0], spos[1], spos[2]);
        glRotatef(-ang, 0.0f, 1.0f, 0.0f);
        glRotatef(-horizang, 1.0f, 0.0f, 0.0f);
        glTranslatef((float)(-xoff), (float)(yoff), 0.0f);
        glScalef((float)(xsize), (float)(ysize), 1.0f);
        break;
    case CSTAT_SPRITE_ALIGNMENT_WALL:
        {
            int16_t wallnum = ((unsigned)tspr->owner >= MAXSPRITES) ? -1 : ornament[tspr->owner].wall;
            wallspriteinfo_t *ws = ((unsigned)tspr->owner >= MAXSPRITES) ? nullptr : &ornament[tspr->owner];
            vec2f_t const vf = { ((float)tspr->xrepeat * (float)sintable[(tspr->ang) & 2047] * (1.0f / 65536.f)) * f,
                                 ((float)tspr->xrepeat * (float)sintable[(tspr->ang + 1536) & 2047] * (1.0f / 65536.f)) * f};

            polymost_checkornamentedsprite(tspr, &wallnum, ws);

            ang = (float)((tspr->ang + 1024) & 2047) * (360.f / 2048.f);

            if (wallnum != -1 && ((ws != nullptr && !ws->invalid) || polymost_testintersection(tspr->xyz, { (int)vf.x, (int)vf.y }, wallnum)))
            {
                fix16_t const ang16 = (gethiq16angle(wall[wallnum].x - POINT2(wallnum).x,
                                                     wall[wallnum].y - POINT2(wallnum).y) + F16(1536)) & 0x7FFFFFF;

                if (fix16_to_float(fix16_abs(getq16angledelta(fix16_from_int(tspr->ang), ang16))) <= MAXINTERSECTIONANGDIFF)
                {
                    if (ws != nullptr)
                        ws->invalid = 0;

                    ang = fix16_to_float(((ang16 + F16(1024)) & 0x7FFFFFF)) * (360.f / 2048.f);
                }
            }

            glTranslatef(spos[0], spos[1], spos[2]);
            glRotatef(-ang, 0.0f, 1.0f, 0.0f);
            glTranslatef((float)(-xoff), (float)(yoff - centeryoff), 0.0f);
            glScalef((float)(xsize), (float)(ysize), 1.0f);
            break;
        }
    case CSTAT_SPRITE_ALIGNMENT_FLOOR:
    case CSTAT_SPRITE_ALIGNMENT_SLOPE:
    {
        float const sang = atan2f(float(heinum), 4096.f) * (180.f * float(M_1_PI));
        ang = (float)((tspr->ang + 1024) & 2047) * (360.f/2048.f);

        glTranslatef(spos[0], spos[1], spos[2]);
        glRotatef(-ang, 0.0f, 1.0f, 0.0f);
        glRotatef(-sang, 1.0f, 0.0f, 0.0f);
        glTranslatef((float)(-xoff), 1.0f, (float)(yoff));
        glScalef((float)(xsize), 1.0f, (float)(ysize));

        inbuffer = horizsprite;
        break;
    }
    }

    glGetFloatv(GL_MODELVIEW_MATRIX, spritemodelview);
    glPopMatrix();

    Bmemcpy(s->plane.buffer, inbuffer, sizeof(_prvert) * 4);

    if (flipu || flipv)
    {
        i = 0;
        do
        {
            if (flipu)
                s->plane.buffer[i].u =
                (s->plane.buffer[i].u - 1.0f) * -1.0f;
            if (flipv)
                s->plane.buffer[i].v =
                (s->plane.buffer[i].v - 1.0f) * -1.0f;
        }
        while (++i < 4);
    }

    i = 0;
    do
        polymer_transformpoint(&inbuffer[i].x, &s->plane.buffer[i].x, spritemodelview);
    while (++i < 4);

    polymer_computeplane(&s->plane);

    if (pr_nullrender < 2)
    {
        if (alignmask != CSTAT_SPRITE_ALIGNMENT_FACING)
        {
            buildgl_bindBuffer(GL_ARRAY_BUFFER, s->plane.vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(_prvert), s->plane.buffer);
            buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);
        }
        else if (s->plane.vbo) // clean up the vbo if a wall/floor sprite becomes a face sprite
        {
            glDeleteBuffers(1, &s->plane.vbo);
            s->plane.vbo = 0;
        }
    }

    if (alignmask != CSTAT_SPRITE_ALIGNMENT_FACING)
    {
        int32_t curpriority = 0;

        polymer_resetplanelights(&s->plane);

        while (curpriority < pr_maxlightpriority)
        {
            i = j = 0;
            while (j < lightcount)
            {
                while (!prlights[i].flags.active)
                    i++;

                if (prlights[i].priority != curpriority)
                {
                    i++;
                    j++;
                    continue;
                }

                if (polymer_planeinlight(s->plane, prlights[i]))
                    polymer_addplanelight(&s->plane, i);
                i++;
                j++;
            }
            curpriority++;
        }
    }
}

// SKIES
static void polymer_getsky(int16_t picnum, uint8_t pal, int8_t shade)
{
    int32_t horizfrac;

    cursky = picnum;
    curskypal = pal;
    curskyshade = shade;

    getpsky(cursky, &horizfrac, NULL, NULL, NULL);

    switch (horizfrac)
    {
    case 0:
        // psky always at same level wrt screen
        curskyangmul = 0.f;
        break;
    case 65536:
        // psky horiz follows camera horiz
        curskyangmul = 1.f;
        break;
    default:
        // sky has hard-coded parallax
        curskyangmul = 1 / DEFAULT_ARTSKY_ANGDIV;
        break;
    }
}

void         polymer_drawsky(int16_t tilenum, char palnum, int8_t shade)
{
    float           pos[3];
    pthtyp*         pth;

    pos[0] = fglobalposy;
    pos[1] = fglobalposz * (-1.f/16.f);
    pos[2] = -fglobalposx;

    glPushMatrix();
    glLoadIdentity();

    glLoadMatrixf(curskymodelviewmatrix);

    glTranslatef(pos[0], pos[1], pos[2]);
    glScalef(1000.0f, 1000.0f, 1000.0f);

    drawingskybox = 1;
    pth = texcache_fetch(tilenum, 0, 0, DAMETH_NOMASK);
    drawingskybox = 0;

    if (pth && (pth->flags & PTH_SKYBOX))
        polymer_drawskybox(tilenum, palnum, shade);
    else
        polymer_drawartsky(tilenum, palnum, shade);

    glPopMatrix();
}

static void         polymer_initartsky(void)
{
    constexpr double factor = 2.0 * PI / (double)PSKYOFF_MAX;

    for (int i = 0; i < PSKYOFF_MAX; i++)
    {
        artskydata[i * 2 + 0] = -cosf(i * factor);
        artskydata[i * 2 + 1] = sinf(i * factor);
    }
}

static inline void polymer_drawartskyquad(int32_t p1, int32_t p2, GLfloat height)
{
    polymost_startBufferedDrawing(4);
    polymost_bufferVert({ artskydata[(p1 * 2) + 1], height, artskydata[p1 * 2] }, { 0.f, 0.f });
    polymost_bufferVert({ artskydata[(p1 * 2) + 1], -height, artskydata[p1 * 2] }, { 0.0f, 1.0f });
    polymost_bufferVert({ artskydata[(p2 * 2) + 1], -height, artskydata[p2 * 2] }, { 1.0f, 1.0f });
    polymost_bufferVert({ artskydata[(p2 * 2) + 1], height, artskydata[p2 * 2] }, { 1.0f, 0.0f });
    polymost_finishBufferedDrawing(GL_QUADS);
}

static void         polymer_drawartsky(int16_t tilenum, char palnum, int8_t shade)
{
    pthtyp*         pth;
    //GLuint          glpics[PSKYOFF_MAX];
    GLfloat         glcolors[PSKYOFF_MAX][3];
    int32_t         i, j;
    GLfloat         height = 2.45f / 2.0f;

    int32_t dapskybits;
    const int8_t *dapskyoff = getpsky(tilenum, NULL, &dapskybits, NULL, NULL);
    const int32_t numskytiles = 1<<dapskybits;
    const int32_t numskytilesm1 = numskytiles-1;

    i = 0;
    while (i < numskytiles)
    {
        int16_t picnum = tilenum + dapskyoff[i];
        // Prevent oob by bad user input:
        if (picnum >= MAXTILES)
            picnum = MAXTILES-1;

        tileUpdatePicnum(&picnum, 0);
        if (!waloff[picnum])
            tileLoad(picnum);
        pth = texcache_fetch(picnum, palnum, 0, DAMETH_NOMASK);
        //glpics[i] = pth ? pth->glpic : 0;

        glcolors[i][0] = glcolors[i][1] = glcolors[i][2] = getshadefactor(shade, palnum);

        if (pth)
        {
            // tinting
            polytintflags_t const tintflags = hictinting[palnum].f;
            if (!(tintflags & HICTINT_PRECOMPUTED))
            {
                if (pth->flags & PTH_HIGHTILE)
                {
                    if (pth->palnum != palnum || (pth->effects & HICTINT_IN_MEMORY) || (tintflags & HICTINT_APPLYOVERALTPAL))
                        hictinting_apply(glcolors[i], palnum);
                }
                else if (tintflags & (HICTINT_USEONART|HICTINT_ALWAYSUSEART))
                    hictinting_apply(glcolors[i], palnum);
            }

            // global tinting
            if ((pth->flags & PTH_HIGHTILE) && have_basepal_tint())
                hictinting_apply(glcolors[i], MAXPALOOKUPS-1);

            globaltinting_apply(glcolors[i]);
        }

        i++;
    }
    buildgl_setEnabled(GL_TEXTURE_2D);
    i = 0;
    j = 0;
    int32_t const increment = PSKYOFF_MAX>>max(3, dapskybits);  // In Polymer, an ART sky has 8 or 16 sides...
//    buildgl_bindSamplerObject(0, PTH_TEMP_SKY_HACK);
    while (i < PSKYOFF_MAX)
    {
        // ... but in case a multi-psky specifies less than 8, repeat cyclically:
        const int8_t tileofs = j&numskytilesm1;
        polymer_inb4rotatesprite(tilenum+dapskyoff[tileofs], palnum, shade, DAMETH_CLAMPED);
        glColor4f(glcolors[tileofs][0], glcolors[tileofs][1], glcolors[tileofs][2], 1.0f);
        //buildgl_bindTexture(GL_TEXTURE_2D, glpics[tileofs]);
        polymer_drawartskyquad(i, (i + increment) & (PSKYOFF_MAX - 1), height);
        polymer_postrotatesprite();

        i += increment;
        ++j;
    }

    //buildgl_bindSamplerObject(0, 0);
    buildgl_setDisabled(GL_TEXTURE_2D);
}

static void         polymer_drawskybox(int16_t tilenum, char palnum, int8_t shade)
{
    pthtyp*         pth;
    int32_t         i;
    GLfloat         color[3];

    if (skyboxdatavbo == 0)
    {
        glGenBuffers(1, &skyboxdatavbo);

        buildgl_bindBuffer(GL_ARRAY_BUFFER, skyboxdatavbo);
        glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat) * 5 * 6, skyboxdata, modelvbousage);

        buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);
    }

    buildgl_bindBuffer(GL_ARRAY_BUFFER, skyboxdatavbo);

    tileUpdatePicnum(&tilenum, 0);

    _prmaterial     skymaterial;
    drawingskybox = 1;
    polymer_getbuildmaterial(&skymaterial, tilenum, palnum, shade, 0, PTH_HIGHTILE|PTH_CLAMPED);
    auto skymaterialbits = polymer_bindmaterial(&skymaterial, NULL, 0);
    //buildgl_bindSamplerObject(0, PTH_HIGHTILE|PTH_CLAMPED);

    i = 0;
    while (i < 6)
    {
        drawingskybox = i + 1;
        pth = texcache_fetch(tilenum, palnum, 0, DAMETH_CLAMPED);

        color[0] = color[1] = color[2] = getshadefactor(shade, palnum);

        if (pth)
        {
            // tinting
            polytintflags_t const tintflags = hictinting[palnum].f;
            if (!(tintflags & HICTINT_PRECOMPUTED))
            {
                if (pth->flags & PTH_HIGHTILE)
                {
                    if (pth->palnum != palnum || (pth->effects & HICTINT_IN_MEMORY) || (tintflags & HICTINT_APPLYOVERALTPAL))
                        hictinting_apply(color, palnum);
                }
                else if (tintflags & (HICTINT_USEONART|HICTINT_ALWAYSUSEART))
                    hictinting_apply(color, palnum);
            }

            // global tinting
            if ((pth->flags & PTH_HIGHTILE) && have_basepal_tint())
                hictinting_apply(color, MAXPALOOKUPS-1);

            globaltinting_apply(color);
        }

        glColor4f(color[0], color[1], color[2], 1.0);
        buildgl_setEnabled(GL_TEXTURE_2D);
        buildgl_bindTexture(GL_TEXTURE_2D, pth ? rhi->texture_handle(pth->rhipic) : 0);
        glVertexPointer(3, GL_FLOAT, 5 * sizeof(GLfloat), (GLfloat*)(4 * 5 * i * sizeof(GLfloat)));
        glTexCoordPointer(2, GL_FLOAT, 5 * sizeof(GLfloat), (GLfloat*)(((4 * 5 * i) + 3) * sizeof(GLfloat)));
        glDrawArrays(GL_QUADS, 0, 4);
        buildgl_setDisabled(GL_TEXTURE_2D);

        i++;
    }
    drawingskybox = 0;

    polymer_unbindmaterial(skymaterialbits);
    buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);
}

// MDSPRITES
void polymer_drawmdsprite(tspriteptr_t tspr)
{
    md3model_t*     m;
    mdskinmap_t*    sk;
    md3surf_t       *s;
    char            targetpal, usinghighpal, foundpalskin;
    float           spos2[3], spos[3], tspos[3], lpos[3], tlpos[3], vec[3], mat[4][4];
    float           ang;
    float           scale;
    double          det;
    int32_t         surfi, i, j;
    GLubyte*        color;
    int32_t         materialbits;
    float           sradius, lradius;
    int16_t         modellights[PR_MAXPLANELIGHTS];
    char            modellightcount;
    uint8_t         curpriority;

    uint8_t lpal = (tspr->owner >= MAXSPRITES) ? tspr->pal : sprite[tspr->owner].pal;

    m = (md3model_t*)models[tile2model[Ptile2tile(tspr->picnum,lpal)].modelid];
    updateanimation((md2model_t *)m,tspr,lpal);

    if (m->indices == NULL)
        polymer_loadmodelvbos(m);

    auto const tsprflags = tspr->clipdist;

    // Hackish, but that means it's a model drawn by rotatesprite.
    if (tspriteptr[maxspritesonscreen] == tspr) {
        spos[0] = fglobalposy;
        spos[1] = fglobalposz * (-1.f/16.f);
        spos[2] = -fglobalposx;

        // The coordinates are actually floats disguised as int in this case
        vec3f_t v = *(vec3f_t*)&tspr->xyz;

        spos2[0] = v.y - fglobalposy;
        spos2[1] = (v.z - fglobalposz) * (-1.f/16.f);
        spos2[2] = fglobalposx - v.x;

        ang = fix16_to_float((fix16_from_int((tspr->ang+spriteext[tspr->owner].mdangoff-globalang) & 2047) + qglobalang) & 0x7FFFFFF) * (360.f/2048.f);
    } else {
        spos[0] = (float)tspr->y+spriteext[tspr->owner].mdposition_offset.y;
        spos[1] = -(float)(tspr->z+spriteext[tspr->owner].mdposition_offset.z) * (1.f/16.f);
        spos[2] = -(float)(tspr->x+spriteext[tspr->owner].mdposition_offset.x);

        spos2[0] = spos2[1] = spos2[2] = 0.0f;

        ang = (float)((tspr->ang+spriteext[tspr->owner].mdangoff) & 2047) * (360.f/2048.f);
    }

    ang -= 90.0f;

    if (tspr->cstat & CSTAT_SPRITE_ALIGNMENT_FLOOR)
        ang -= 90.0f;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    scale = m->scale * 0.25f;

    if (pr_overridemodelscale) {
        scale *= pr_overridemodelscale;
    } else {
        scale *= m->bscale;
    }

    if (tspriteptr[maxspritesonscreen] == tspr) {
        float playerang = fix16_to_float(qglobalang & 0x7FFFFFF) * (360.f/2048.f) - 90.0f;
        float radplayerang = fix16_to_float(qglobalang & 0x7FFFFFF) * (2.0f * fPI / 2048.0f);
        float cosminusradplayerang = cosf(-radplayerang);
        float sinminusradplayerang = sinf(-radplayerang);
        float hudzoom = 65536.f / spriteext[tspr->owner].mdpivot_offset.z;

        glTranslatef(spos[0], spos[1], spos[2]);
        glRotatef(horizang, -cosminusradplayerang, 0.0f, sinminusradplayerang);
        glRotatef(spriteext[tspr->owner].mdroll * (360.f/2048.f), sinminusradplayerang, 0.0f, cosminusradplayerang);
        glRotatef(-playerang, 0.0f, 1.0f, 0.0f);
        glScalef(hudzoom, 1.0f, 1.0f);
        glRotatef(playerang, 0.0f, 1.0f, 0.0f);
        glTranslatef(spos2[0], spos2[1], spos2[2]);
        glRotatef(-ang, 0.0f, 1.0f, 0.0f);
    } else {
        glTranslatef(spos[0], spos[1], spos[2]);
        glRotatef(-ang, 0.0f, 1.0f, 0.0f);
    }
    if (tspr->cstat & CSTAT_SPRITE_ALIGNMENT_FLOOR)
    {
        glTranslatef(0.0f, 0.0, -(float)(tilesiz[tspr->picnum].y * tspr->yrepeat) / 8.0f);
        glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    }
    else
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);

    if ((tspr->cstat & 128) && !(tspr->cstat & CSTAT_SPRITE_ALIGNMENT_FLOOR))
        glTranslatef(0.0f, 0.0, -(float)(tilesiz[tspr->picnum].y * tspr->yrepeat) / 8.0f);

    // yoffset differs from zadd in that it does not follow cstat&8 y-flipping
    glTranslatef(0.0f, 0.0, m->yoffset * 64 * scale * tspr->yrepeat);

    if (tspr->cstat & 8)
    {
        glTranslatef(0.0f, 0.0, (float)(tilesiz[tspr->picnum].y * tspr->yrepeat) / 4.0f);
        glScalef(1.0f, 1.0f, -1.0f);
    }

    if (tspr->cstat & 4)
        glScalef(1.0f, -1.0f, 1.0f);

    if (!(tspr->cstat & 4) != !(tspr->cstat & 8)) {
        // Only inverting one coordinate will reverse the winding order of
        // faces, so we need to account for that when culling.
        SWITCH_CULL_DIRECTION;
    }

    glScalef(scale * tspr->xrepeat, scale * tspr->xrepeat, scale * tspr->yrepeat);
    glTranslatef(0.0f, 0.0, m->zadd * 64);

    // scripted model rotation
    if (tspr->owner < MAXSPRITES &&
        (spriteext[tspr->owner].mdpitch || spriteext[tspr->owner].mdroll))
    {
        float       pitchang, rollang, offsets[3];

        pitchang = (float)(spriteext[tspr->owner].mdpitch) * (360.f/2048.f);
        rollang = (float)(spriteext[tspr->owner].mdroll) * (360.f/2048.f);

        offsets[0] = -spriteext[tspr->owner].mdpivot_offset.x / (scale * tspr->xrepeat);
        offsets[1] = -spriteext[tspr->owner].mdpivot_offset.y / (scale * tspr->xrepeat);
        offsets[2] = (float)(spriteext[tspr->owner].mdpivot_offset.z) * (1.f/16.f) / (scale * tspr->yrepeat);

        glTranslatef(-offsets[0], -offsets[1], -offsets[2]);

        glRotatef(pitchang, 0.0f, 1.0f, 0.0f);
        glRotatef(rollang, -1.0f, 0.0f, 0.0f);

        glTranslatef(offsets[0], offsets[1], offsets[2]);
    }

    glGetFloatv(GL_MODELVIEW_MATRIX, spritemodelview);

    glPopMatrix();
    glPushMatrix();
    glMultMatrixf(spritemodelview);

    // invert this matrix to get the polymer -> mdsprite space
    memcpy(mat, spritemodelview, sizeof(float) * 16);
    INVERT_4X4(mdspritespace, det, mat);

    // debug code for drawing the model bounding sphere
//     glDisable(GL_TEXTURE_2D);
//     glBegin(GL_LINES);
//     glColor4f(1.0, 0.0, 0.0, 1.0);
//     glVertex3f(m->head.frames[m->cframe].cen.x,
//                 m->head.frames[m->cframe].cen.y,
//                 m->head.frames[m->cframe].cen.z);
//     glVertex3f(m->head.frames[m->cframe].cen.x + m->head.frames[m->cframe].r,
//                 m->head.frames[m->cframe].cen.y,
//                 m->head.frames[m->cframe].cen.z);
//     glColor4f(0.0, 1.0, 0.0, 1.0);
//     glVertex3f(m->head.frames[m->cframe].cen.x,
//                 m->head.frames[m->cframe].cen.y,
//                 m->head.frames[m->cframe].cen.z);
//     glVertex3f(m->head.frames[m->cframe].cen.x,
//                 m->head.frames[m->cframe].cen.y + m->head.frames[m->cframe].r,
//                 m->head.frames[m->cframe].cen.z);
//     glColor4f(0.0, 0.0, 1.0, 1.0);
//     glVertex3f(m->head.frames[m->cframe].cen.x,
//                 m->head.frames[m->cframe].cen.y,
//                 m->head.frames[m->cframe].cen.z);
//     glVertex3f(m->head.frames[m->cframe].cen.x,
//                 m->head.frames[m->cframe].cen.y,
//                 m->head.frames[m->cframe].cen.z + m->head.frames[m->cframe].r);
//     glEnd();
//     glEnable(GL_TEXTURE_2D);

    polymer_getscratchmaterial(&mdspritematerial);

    color = mdspritematerial.diffusemodulation;

    color[0] = color[1] = color[2] =
        (GLubyte)(((float)(numshades-min(max((tspr->shade * shadescale)+m->shadeoff,0.f),(float)numshades)))/((float)numshades) * 0xFF);

    usinghighpal = (pr_highpalookups && polymer_havehighpalookup(curbasepal, tspr->pal) && prhighpalookups[curbasepal][tspr->pal].map);

    // tinting
    polytintflags_t const tintflags = hictinting[tspr->pal].f;
    if (!usinghighpal && !(tintflags & HICTINT_PRECOMPUTED))
    {
        if (!(m->flags&1))
            hictinting_apply_ub(color, tspr->pal);
        else globalnoeffect=1; //mdloadskin reads this
    }

    // global tinting
    if (!usinghighpal && have_basepal_tint())
        hictinting_apply_ub(color, MAXPALOOKUPS-1);

    globaltinting_apply_ub(color);

    if (tspr->cstat & 2)
    {
        if (!(tspr->cstat&512))
            color[3] = transluctable[1];
        else
            color[3] = transluctable[0];
    } else
        color[3] = 0xFF;

    {
        double f = color[3] * (1.0f - spriteext[tspr->owner].alpha);
        color[3] = (GLubyte)f;
    }

    if (searchit == 2)
    {
        color[0] = 0x03;
        color[1] = ((GLubyte *)(&tspr->owner))[0];
        color[2] = ((GLubyte *)(&tspr->owner))[1];
        color[3] = 0xFF;
    }

    if (pr_gpusmoothing)
        mdspritematerial.frameprogress = m->interpol;

    mdspritematerial.mdspritespace = GL_TRUE;

    modellightcount = 0;
    curpriority = 0;

    // light culling
    if (lightcount && (!depth || mirrors[depth-1].plane))
    {
        sradius = (m->head.frames[m->cframe].r * (1 - m->interpol)) +
                  (m->head.frames[m->nframe].r * m->interpol);

        sradius *= max(scale * tspr->xrepeat, scale * tspr->yrepeat);
        sradius /= 1000.0f;

        spos[0] = (m->head.frames[m->cframe].cen.x * (1 - m->interpol)) +
                  (m->head.frames[m->nframe].cen.x * m->interpol);
        spos[1] = (m->head.frames[m->cframe].cen.y * (1 - m->interpol)) +
                  (m->head.frames[m->nframe].cen.y * m->interpol);
        spos[2] = (m->head.frames[m->cframe].cen.z * (1 - m->interpol)) +
                  (m->head.frames[m->nframe].cen.z * m->interpol);

        polymer_transformpoint(spos, tspos, spritemodelview);
        polymer_transformpoint(tspos, spos, rootmodelviewmatrix);

        while (curpriority < pr_maxlightpriority)
        {
            i = j = 0;
            while (j < lightcount && modellightcount < PR_MAXPLANELIGHTS)
            {
                while (!prlights[i].flags.active)
                    i++;

                if (prlights[i].priority != curpriority)
                {
                    i++;
                    j++;
                    continue;
                }

                lradius = prlights[i].range / 1000.0f;

                lpos[0] = (float)prlights[i].y;
                lpos[1] = -(float)prlights[i].z * (1.f/16.f);
                lpos[2] = -(float)prlights[i].x;

                polymer_transformpoint(lpos, tlpos, rootmodelviewmatrix);

                vec[0] = tlpos[0] - spos[0];
                vec[0] *= vec[0];
                vec[1] = tlpos[1] - spos[1];
                vec[1] *= vec[1];
                vec[2] = tlpos[2] - spos[2];
                vec[2] *= vec[2];

                if ((vec[0] + vec[1] + vec[2]) <= ((sradius+lradius) * (sradius+lradius)))
                    modellights[modellightcount++] = i;

                i++;
                j++;
            }
            curpriority++;
        }
    }

    for (surfi=0;surfi<m->head.numsurfs;surfi++)
    {
        s = &m->head.surfs[surfi];
        //float *v0 = &s->geometry[m->cframe*s->numverts*15];
        //float *v1 = &s->geometry[m->nframe*s->numverts*15];

        // debug code for drawing model normals
//         glDisable(GL_TEXTURE_2D);
//         glBegin(GL_LINES);
//         glColor4f(1.0, 1.0, 1.0, 1.0);
//
//         int i = 0;
//         while (i < s->numverts)
//         {
//             glVertex3f(v0[(i * 6) + 0],
//                         v0[(i * 6) + 1],
//                         v0[(i * 6) + 2]);
//             glVertex3f(v0[(i * 6) + 0] + v0[(i * 6) + 3] * 100,
//                         v0[(i * 6) + 1] + v0[(i * 6) + 4] * 100,
//                         v0[(i * 6) + 2] + v0[(i * 6) + 5] * 100);
//             i++;
//         }
//         glEnd();
//         glEnable(GL_TEXTURE_2D);


        targetpal = tspr->pal;
        foundpalskin = 0;

        for (sk = m->skinmap; sk; sk = sk->next)
            if ((int32_t)sk->palette == tspr->pal &&
                 sk->skinnum == tile2model[Ptile2tile(tspr->picnum,lpal)].skinnum &&
                 sk->surfnum == surfi)
        {
            if (sk->specpower != 1.0)
                mdspritematerial.specmaterial[0] = sk->specpower;
            mdspritematerial.specmaterial[1] = sk->specfactor;
            foundpalskin = 1;
        }

        // If we have a global palette tint, the palskin won't do us any good
        if (curbasepal)
            foundpalskin = 0;

        if (!foundpalskin && usinghighpal) {
            // We don't have a specific skin defined for this palette
            // Use the base skin instead and plug in our highpalookup map
            targetpal = 0;
            mdspritematerial.highpalookupmap = prhighpalookups[curbasepal][tspr->pal].map;
        }

        mdspritematerial.diffusemap = rhi->texture_handle(
                mdloadskin((md2model_t *)m,tile2model[Ptile2tile(tspr->picnum,lpal)].skinnum,targetpal,surfi));
        if (!mdspritematerial.diffusemap)
            continue;

        if (!(tsprflags & TSPR_FLAGS_MDHACK))
        {
            mdspritematerial.detailmap = rhi->texture_handle(
                    mdloadskin((md2model_t *)m,tile2model[Ptile2tile(tspr->picnum,lpal)].skinnum,DETAILPAL,surfi));

            for (sk = m->skinmap; sk; sk = sk->next)
                if ((int32_t)sk->palette == DETAILPAL &&
                    sk->skinnum == tile2model[Ptile2tile(tspr->picnum,lpal)].skinnum &&
                    sk->surfnum == surfi)
                    mdspritematerial.detailscale[0] = mdspritematerial.detailscale[1] = sk->param;

            mdspritematerial.specmap = rhi->texture_handle(
                    mdloadskin((md2model_t *)m,tile2model[Ptile2tile(tspr->picnum,lpal)].skinnum,SPECULARPAL,surfi));

            mdspritematerial.normalmap = rhi->texture_handle(
                    mdloadskin((md2model_t *)m,tile2model[Ptile2tile(tspr->picnum,lpal)].skinnum,NORMALPAL,surfi));

            for (sk = m->skinmap; sk; sk = sk->next)
                if ((int32_t)sk->palette == NORMALPAL &&
                    sk->skinnum == tile2model[Ptile2tile(tspr->picnum,lpal)].skinnum &&
                    sk->surfnum == surfi) {
                    mdspritematerial.normalbias[0] = sk->specpower;
                    mdspritematerial.normalbias[1] = sk->specfactor;
                }

            mdspritematerial.glowmap = rhi->texture_handle(
                    mdloadskin((md2model_t *)m,tile2model[Ptile2tile(tspr->picnum,lpal)].skinnum,GLOWPAL,surfi));
        }

        glEnableClientState(GL_NORMAL_ARRAY);

        buildgl_bindBuffer(GL_ARRAY_BUFFER, m->texcoords[surfi]);
        glTexCoordPointer(2, GL_FLOAT, 0, 0);

        buildgl_bindBuffer(GL_ARRAY_BUFFER, m->geometry[surfi]);
        glVertexPointer(3, GL_FLOAT, sizeof(float) * 15, (GLfloat*)(m->cframe * s->numverts * sizeof(float) * 15));
        glNormalPointer(GL_FLOAT, sizeof(float) * 15, (GLfloat*)(m->cframe * s->numverts * sizeof(float) * 15) + 3);

        mdspritematerial.tbn = (GLfloat*)(m->cframe * s->numverts * sizeof(float) * 15) + 6;

        if (pr_gpusmoothing) {
            mdspritematerial.nextframedata = (GLfloat*)(m->nframe * s->numverts * sizeof(float) * 15);
        }

        buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->indices[surfi]);

        curlight = 0;
        do {
            materialbits = polymer_bindmaterial(&mdspritematerial, modellights, modellightcount);
            glDrawElements(GL_TRIANGLES, s->numtris * 3, GL_UNSIGNED_INT, 0);
            polymer_unbindmaterial(materialbits);
        } while ((++curlight < modellightcount) && (curlight < pr_maxlightpasses));

        buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);

        glDisableClientState(GL_NORMAL_ARRAY);
    }

    glPopMatrix();

    if (!(tspr->cstat & 4) != !(tspr->cstat & 8)) {
        SWITCH_CULL_DIRECTION;
    }

    globalnoeffect = 0;
}

static void         polymer_loadmodelvbos(md3model_t* m)
{
    int32_t         i;
    md3surf_t       *s;

    m->indices = (GLuint *)Xmalloc(m->head.numsurfs * sizeof(GLuint));
    m->texcoords = (GLuint *)Xmalloc(m->head.numsurfs * sizeof(GLuint));
    m->geometry = (GLuint *)Xmalloc(m->head.numsurfs * sizeof(GLuint));

    glGenBuffers(m->head.numsurfs, m->indices);
    glGenBuffers(m->head.numsurfs, m->texcoords);
    glGenBuffers(m->head.numsurfs, m->geometry);

    i = 0;
    while (i < m->head.numsurfs)
    {
        s = &m->head.surfs[i];

        buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->indices[i]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, s->numtris * sizeof(md3tri_t), s->tris, modelvbousage);

        buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        buildgl_bindBuffer(GL_ARRAY_BUFFER, m->texcoords[i]);
        glBufferData(GL_ARRAY_BUFFER, s->numverts * sizeof(md3uv_t), s->uv, modelvbousage);

        buildgl_bindBuffer(GL_ARRAY_BUFFER, m->geometry[i]);
        glBufferData(GL_ARRAY_BUFFER, s->numframes * s->numverts * sizeof(float) * (15), s->geometry, modelvbousage);

        buildgl_bindBuffer(GL_ARRAY_BUFFER, 0);
        i++;
    }
}

// MATERIALS
static void         polymer_getscratchmaterial(_prmaterial* material)
{
    // this function returns a material that won't validate any bits
    // make sure to keep it up to date with the validation logic in bindmaterial

    // PR_BIT_ANIM_INTERPOLATION
    material->frameprogress = 0.0f;
    material->nextframedata = (float*)-1;
    // PR_BIT_NORMAL_MAP
    material->normalmap = 0;
    material->normalbias[0] = material->normalbias[1] = 0.0f;
    material->tbn = NULL;
    // PR_BIT_ART_MAP
    material->artmap = 0;
    material->basepalmap = 0;
    material->lookupmap = 0;
    // PR_BIT_DIFFUSE_MAP
    material->diffusemap = 0;
    material->diffusescale[0] = material->diffusescale[1] = 1.0f;
    // PR_BIT_HIGHPALOOKUP_MAP
    material->highpalookupmap = 0;
    // PR_BIT_DIFFUSE_DETAIL_MAP
    material->detailmap = 0;
    material->detailscale[0] = material->detailscale[1] = 1.0f;
    // PR_BIT_DIFFUSE_MODULATION
    material->diffusemodulation[0] =
            material->diffusemodulation[1] =
            material->diffusemodulation[2] =
            material->diffusemodulation[3] = 0xFF;
    // PR_BIT_SPECULAR_MAP
    material->specmap = 0;
    // PR_BIT_SPECULAR_MATERIAL
    material->specmaterial[0] = 15.0f;
    material->specmaterial[1] = 1.0f;
    // PR_BIT_MIRROR_MAP
    material->mirrormap = 0;
    // PR_BIT_GLOW_MAP
    material->glowmap = 0;
    // PR_BIT_PROJECTION_MAP
    material->mdspritespace = GL_FALSE;
}

static void         polymer_setupartmap(int16_t tilenum, char pal, int32_t meth)
{
    if (!prartmaps[tilenum]) {
        char *tilebuffer = (char *) waloff[tilenum];
        char *tempbuffer = (char *) Xmalloc(tilesiz[tilenum].x * tilesiz[tilenum].y);
        int i, j, k;

        i = k = 0;
        while (i < tilesiz[tilenum].y) {
            j = 0;
            while (j < tilesiz[tilenum].x) {
                tempbuffer[k] = tilebuffer[(j * tilesiz[tilenum].y) + i];
                k++;
                j++;
            }
            i++;
        }

        glGenTextures(1, &prartmaps[tilenum]);
        buildgl_bindTexture(GL_TEXTURE_2D, prartmaps[tilenum]);
        glTexImage2D(GL_TEXTURE_2D,
            0,
            GL_RED,
            tilesiz[tilenum].x,
            tilesiz[tilenum].y,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            tempbuffer);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, meth & DAMETH_CLAMPED ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, meth & DAMETH_CLAMPED ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        buildgl_bindTexture(GL_TEXTURE_2D, 0);
        Xfree(tempbuffer);
    }

    if (!prbasepalmaps[curbasepal]) {
        glGenTextures(1, &prbasepalmaps[curbasepal]);
        buildgl_bindTexture(GL_TEXTURE_2D, prbasepalmaps[curbasepal]);
        glTexImage2D(GL_TEXTURE_2D,
            0,
            GL_RGB,
            256,
            1,
            0,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            basepaltable[curbasepal]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        buildgl_bindTexture(GL_TEXTURE_2D, 0);
    }

    if (!prlookups[pal]) {
        glGenTextures(1, &prlookups[pal]);
        buildgl_bindTexture(GL_TEXTURE_RECTANGLE_ARB, prlookups[pal]);
        glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
            0,
            GL_RED,
            256,
            numshades,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            palookup[pal]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        buildgl_bindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    }
}

static _prbucket*   polymer_getbuildmaterial(_prmaterial* material, int16_t tilenum, char pal, int8_t shade, int8_t vis, int32_t cmeth)
{
    // find corresponding bucket; XXX key that with pr_buckets later, need to be tied to restartvid
    _prbucket *bucketptr = polymer_findbucket(tilenum, pal);

    polymer_getscratchmaterial(material);

    if (!waloff[tilenum])
        tileLoad(tilenum);

    // PR_BIT_DIFFUSE_MAP
    pthtyp *pth = texcache_fetch(tilenum, pal, 0, cmeth);

    if (pth)
    {
        material->diffusemap = rhi->texture_handle(pth->rhipic);

        if (pth->hicr)
        {
            material->diffusescale[0] = pth->hicr->scale.x;
            material->diffusescale[1] = pth->hicr->scale.y;
        }
    }

    int32_t usinghighpal = 0;

    // Lazily fill in all the textures we need, move this to precaching later
    if (polymer_useartmapping() && !(globalflags & GLOBAL_NO_GL_TILESHADES) && polymer_eligible_for_artmap(tilenum, pth))
    {
        polytintflags_t const tintflags = hictinting[pal].f;

        if (tintflags & (HICTINT_USEONART|HICTINT_ALWAYSUSEART))
        {
            if (!(tintflags & HICTINT_APPLYOVERPALSWAP))
                pal = 0;
        }

        if (!prartmaps[tilenum] || !prbasepalmaps[curbasepal] || !prlookups[pal])
            polymer_setupartmap(tilenum, pal, cmeth);

        material->artmap = prartmaps[tilenum];
        material->basepalmap = prbasepalmaps[curbasepal];
        material->lookupmap = prlookups[pal];

        if (!material->basepalmap || !material->lookupmap) {
            material->artmap = 0;
        }

        material->shadeoffset = shade;
        material->visibility = (uint8_t)(vis+16);

        globaltinting_apply_ub(material->diffusemodulation);
        // all the stuff below is mutually exclusive with artmapping
        goto done;
    }

    // PR_BIT_HIGHPALOOKUP_MAP
    if (pr_highpalookups && polymer_havehighpalookup(curbasepal, pal)
        && prhighpalookups[curbasepal][pal].map && hicfindsubst(tilenum, 0)
        && (curbasepal || (hicfindsubst(tilenum, pal)->palnum != pal)))
    {
        material->highpalookupmap = prhighpalookups[curbasepal][pal].map;
        pal = 0;
        usinghighpal = 1;
    }

    if (pth)
    {
        if (pth->hicr)
        {
            // PR_BIT_SPECULAR_MATERIAL
            if (pth->hicr->specpower != 1.0f)
                material->specmaterial[0] = pth->hicr->specpower;
            material->specmaterial[1] = pth->hicr->specfactor;
        }

        // PR_BIT_DIFFUSE_MODULATION
        material->diffusemodulation[0] =
            material->diffusemodulation[1] =
            material->diffusemodulation[2] =
            (GLubyte)(getshadefactor(shade, pal) * 0xFF);

        // tinting
        polytintflags_t const tintflags = hictinting[pal].f;
        if (!(tintflags & HICTINT_PRECOMPUTED))
        {
            if (pth->flags & PTH_HIGHTILE)
            {
                if (pth->palnum != pal || (pth->effects & HICTINT_IN_MEMORY) || (tintflags & HICTINT_APPLYOVERALTPAL))
                    hictinting_apply_ub(material->diffusemodulation, pal);
            }
            else if (tintflags & (HICTINT_USEONART|HICTINT_ALWAYSUSEART))
                hictinting_apply_ub(material->diffusemodulation, pal);
        }

        // global tinting
        if ((pth->flags & PTH_HIGHTILE) && !usinghighpal && have_basepal_tint())
            hictinting_apply_ub(material->diffusemodulation, MAXPALOOKUPS-1);

        globaltinting_apply_ub(material->diffusemodulation);

        // PR_BIT_GLOW_MAP
        if (r_fullbrights && pth->flags & PTH_HASFULLBRIGHT)
            material->glowmap = rhi->texture_handle(pth->ofb->rhipic);
    }

    // PR_BIT_DIFFUSE_DETAIL_MAP
    if (hicfindsubst(tilenum, DETAILPAL, 1) && (pth = texcache_fetch(tilenum, DETAILPAL, 0, DAMETH_NOMASK)) &&
        pth->hicr && (pth->hicr->palnum == DETAILPAL))
    {
        material->detailmap = rhi->texture_handle(pth->rhipic);
        material->detailscale[0] = pth->hicr->scale.x;
        material->detailscale[1] = pth->hicr->scale.y;
    }

    // PR_BIT_GLOW_MAP
    if (hicfindsubst(tilenum, GLOWPAL, 1) && (pth = texcache_fetch(tilenum, GLOWPAL, 0, DAMETH_MASK)) &&
        pth->hicr && (pth->hicr->palnum == GLOWPAL))
        material->glowmap = rhi->texture_handle(pth->rhipic);

    // PR_BIT_SPECULAR_MAP
    if (hicfindsubst(tilenum, SPECULARPAL, 1) && (pth = texcache_fetch(tilenum, SPECULARPAL, 0, DAMETH_NOMASK)) &&
        pth->hicr && (pth->hicr->palnum == SPECULARPAL))
        material->specmap = rhi->texture_handle(pth->rhipic);

    // PR_BIT_NORMAL_MAP
    if (hicfindsubst(tilenum, NORMALPAL, 1) && (pth = texcache_fetch(tilenum, NORMALPAL, 0, DAMETH_NOMASK)) &&
        pth->hicr && (pth->hicr->palnum == NORMALPAL))
    {
        material->normalmap = rhi->texture_handle(pth->rhipic);
        material->normalbias[0] = pth->hicr->specpower;
        material->normalbias[1] = pth->hicr->specfactor;
    }

done:
    if (bucketptr->invalidmaterial != 0)
    {
        bucketptr->material = *material;
        bucketptr->invalidmaterial = 0;
    }

    return bucketptr;
}

static int32_t      polymer_bindmaterial(const _prmaterial *material, const int16_t* lights, int matlightcount)
{
    int32_t         programbits;
    int32_t         texunit;

    programbits = 0;

    // --------- bit validation

    // PR_BIT_ANIM_INTERPOLATION
    if (material->nextframedata != ((float*)-1))
        programbits |= (1 << PR_BIT_ANIM_INTERPOLATION);

    // PR_BIT_LIGHTING_PASS
    if (curlight && matlightcount)
        programbits |= (1 << PR_BIT_LIGHTING_PASS);

    // PR_BIT_NORMAL_MAP
    if (pr_normalmapping && material->normalmap)
        programbits |= (1 << PR_BIT_NORMAL_MAP);

    // PR_BIT_ART_MAP
    if (polymer_useartmapping() && material->artmap &&
        !(globalflags & GLOBAL_NO_GL_TILESHADES) &&
        (overridematerial & (1 << PR_BIT_ART_MAP))) {
        programbits |= (1 << PR_BIT_ART_MAP);
    } else
    // PR_BIT_DIFFUSE_MAP
    if (material->diffusemap) {
        programbits |= (1 << PR_BIT_DIFFUSE_MAP);
        programbits |= (1 << PR_BIT_DIFFUSE_MAP2);
    }

    // PR_BIT_HIGHPALOOKUP_MAP
    if (material->highpalookupmap)
        programbits |= (1 << PR_BIT_HIGHPALOOKUP_MAP);

    // PR_BIT_DIFFUSE_DETAIL_MAP
    if (r_detailmapping && material->detailmap)
        programbits |= (1 << PR_BIT_DIFFUSE_DETAIL_MAP);

    // PR_BIT_DIFFUSE_MODULATION
    programbits |= (1 << PR_BIT_DIFFUSE_MODULATION);

    // PR_BIT_SPECULAR_MAP
    if (pr_specularmapping && material->specmap)
        programbits |= (1 << PR_BIT_SPECULAR_MAP);

    // PR_BIT_SPECULAR_MATERIAL
    if ((material->specmaterial[0] != 15.0) || (material->specmaterial[1] != 1.0) || pr_overridespecular)
        programbits |= (1 << PR_BIT_SPECULAR_MATERIAL);

    // PR_BIT_MIRROR_MAP
    if (!curlight && material->mirrormap)
        programbits |= (1 << PR_BIT_MIRROR_MAP);

    // PR_BIT_FOG
    if (!material->artmap && !curlight && !material->mirrormap)
        programbits |= (1 << PR_BIT_FOG);

    // PR_BIT_GLOW_MAP
    if (!curlight && r_glowmapping && material->glowmap)
        programbits |= (1 << PR_BIT_GLOW_MAP);

    // PR_BIT_POINT_LIGHT
    if (matlightcount) {
        programbits |= (1 << PR_BIT_POINT_LIGHT);
        // PR_BIT_SPOT_LIGHT
        if (prlights[lights[curlight]].radius) {
            programbits |= (1 << PR_BIT_SPOT_LIGHT);
            // PR_BIT_SHADOW_MAP
            if (prlights[lights[curlight]].rtindex != -1) {
                programbits |= (1 << PR_BIT_SHADOW_MAP);
                programbits |= (1 << PR_BIT_PROJECTION_MAP);
            }
            // PR_BIT_LIGHT_MAP
            if (prlights[lights[curlight]].lightmap) {
                programbits |= (1 << PR_BIT_LIGHT_MAP);
                programbits |= (1 << PR_BIT_PROJECTION_MAP);
            }
        }
    }

    // material override
    programbits &= overridematerial;

    programbits |= (1 << PR_BIT_HEADER);
    programbits |= (1 << PR_BIT_FOOTER);

    // --------- program compiling
    auto &prprogram = *polymer_getprogram(programbits);

    buildgl_useShaderProgram(prprogram.handle);

    // --------- bit setup

    texunit = 0;
    //buildgl_bindSamplerObject(texunit, (programbits & (1 << PR_BIT_ART_MAP)) ? PTH_INDEXED : PTH_HIGHTILE);

    // PR_BIT_ANIM_INTERPOLATION
    if (programbits & (1 << PR_BIT_ANIM_INTERPOLATION))
    {
        glEnableVertexAttribArray(prprogram.attrib_nextFrameData);
        if (prprogram.attrib_nextFrameNormal != -1)
            glEnableVertexAttribArray(prprogram.attrib_nextFrameNormal);
        glVertexAttribPointer(prprogram.attrib_nextFrameData,
                               3, GL_FLOAT, GL_FALSE,
                               sizeof(float) * 15,
                               material->nextframedata);
        if (prprogram.attrib_nextFrameNormal != -1)
            glVertexAttribPointer(prprogram.attrib_nextFrameNormal,
                                   3, GL_FLOAT, GL_FALSE,
                                   sizeof(float) * 15,
                                   material->nextframedata + 3);

        glUniform1f(prprogram.uniform_frameProgress, material->frameprogress);
    }

    // PR_BIT_LIGHTING_PASS
    if (programbits & (1 << PR_BIT_LIGHTING_PASS))
    {
        glPushAttrib(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        if (prlights[lights[curlight]].publicflags.negative) {
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        }

        glUniform4f(prprogram.uniform_colorCorrection, min(1.0f, g_glColorCorrection.x), g_glColorCorrection.y, g_glColorCorrection.z, g_glColorCorrection.w);
    }
    else
        glUniform4f(prprogram.uniform_colorCorrection, g_glColorCorrection.x, g_glColorCorrection.y, g_glColorCorrection.z, g_glColorCorrection.w);

    // PR_BIT_NORMAL_MAP
    if (programbits & (1 << PR_BIT_NORMAL_MAP))
    {
        float pos[3], bias[2];

        pos[0] = fglobalposy;
        pos[1] = fglobalposz * (-1.f/16.f);
        pos[2] = -fglobalposx;

        buildgl_activeTexture(texunit + GL_TEXTURE0);
        //buildgl_bindSamplerObject(texunit, (programbits & (1 << PR_BIT_ART_MAP)) ? PTH_INDEXED : PTH_HIGHTILE);

        buildgl_bindTexture(GL_TEXTURE_2D, material->normalmap);

        if (material->mdspritespace == GL_TRUE) {
            float mdspritespacepos[3];
            polymer_transformpoint(pos, mdspritespacepos, (float *)mdspritespace);
            glUniform3fv(prprogram.uniform_eyePosition, 1, mdspritespacepos);
        } else
            glUniform3fv(prprogram.uniform_eyePosition, 1, pos);
        glUniform1i(prprogram.uniform_normalMap, texunit);
        if (pr_overrideparallax) {
            bias[0] = pr_parallaxscale;
            bias[1] = pr_parallaxbias;
            glUniform2fv(prprogram.uniform_normalBias, 1, bias);
        } else
            glUniform2fv(prprogram.uniform_normalBias, 1, material->normalbias);

        if (material->tbn) {
            glEnableVertexAttribArray(prprogram.attrib_T);
            glEnableVertexAttribArray(prprogram.attrib_B);
            glEnableVertexAttribArray(prprogram.attrib_N);

            glVertexAttribPointer(prprogram.attrib_T,
                                      3, GL_FLOAT, GL_FALSE,
                                      sizeof(float) * 15,
                                      material->tbn);
            glVertexAttribPointer(prprogram.attrib_B,
                                      3, GL_FLOAT, GL_FALSE,
                                      sizeof(float) * 15,
                                      material->tbn + 3);
            glVertexAttribPointer(prprogram.attrib_N,
                                      3, GL_FLOAT, GL_FALSE,
                                      sizeof(float) * 15,
                                      material->tbn + 6);
        }

        texunit++;
    }

    // PR_BIT_ART_MAP
    if (programbits & (1 << PR_BIT_ART_MAP))
    {
        buildgl_activeTexture(texunit + GL_TEXTURE0);
        //buildgl_bindSamplerObject(texunit, PTH_INDEXED);
        buildgl_bindTexture(GL_TEXTURE_2D, material->artmap);

        glUniform1i(prprogram.uniform_artMap, texunit);

        texunit++;

        buildgl_activeTexture(texunit + GL_TEXTURE0);
        //buildgl_bindSamplerObject(texunit, PTH_INDEXED|PTH_CLAMPED);
        buildgl_bindTexture(GL_TEXTURE_2D, material->basepalmap);

        glUniform1i(prprogram.uniform_basePalMap, texunit);

        texunit++;

        buildgl_activeTexture(texunit + GL_TEXTURE0);
        //buildgl_bindSamplerObject(texunit, PTH_INDEXED|PTH_CLAMPED);
        buildgl_bindTexture(GL_TEXTURE_RECTANGLE_ARB, material->lookupmap);

        glUniform1i(prprogram.uniform_lookupMap, texunit);

        texunit++;

        glUniform1f(prprogram.uniform_shadeOffset, (GLfloat)material->shadeoffset);
        if (r_usenewshading == 4)
        {
            // the fog in Polymer is a sphere insted of a plane, the furthest visible point should be the same as Polymost
            glUniform1f(prprogram.uniform_visibility, globalvisibility / 262144.f * material->visibility);
        }
        else
        {
            static constexpr float material_visibility_divisor = 16.f;

            // NOTE: the denominator was 1.024, but we increase it towards a bit
            // farther far clipoff distance to account for the fact that the
            // distance to the fragment is the common Euclidean one, as opposed to
            // the "ortho" distance of Build.
            static constexpr float factor_new = 1.f / ((2048.f * (1.07f / 1.024f) * (150.f / 230.f) / 35.f) * material_visibility_divisor);

            static constexpr float factor_old = 1.f / ((2048.f * (1.07f / 1.024f) / 35.f) * material_visibility_divisor);

            glUniform1f(prprogram.uniform_visibility, globalvisibility * material->visibility * r_usenewshading > 1 ? factor_new : factor_old);
        }
    }

    // PR_BIT_DIFFUSE_MAP
    if (programbits & (1 << PR_BIT_DIFFUSE_MAP))
    {
        buildgl_activeTexture(texunit + GL_TEXTURE0);
        //buildgl_bindSamplerObject(texunit, (programbits & (1 << PR_BIT_ART_MAP)) ? PTH_INDEXED : PTH_HIGHTILE);
        buildgl_bindTexture(GL_TEXTURE_2D, material->diffusemap);

        glUniform1i(prprogram.uniform_diffuseMap, texunit);
        glUniform2fv(prprogram.uniform_diffuseScale, 1, material->diffusescale);

        texunit++;
    }

    // PR_BIT_HIGHPALOOKUP_MAP
    if (programbits & (1 << PR_BIT_HIGHPALOOKUP_MAP))
    {
        buildgl_activeTexture(texunit + GL_TEXTURE0);
        //buildgl_bindSamplerObject(texunit, PTH_CLAMPED);
        buildgl_bindTexture(GL_TEXTURE_3D, material->highpalookupmap);

        glUniform1i(prprogram.uniform_highPalookupMap, texunit);

        texunit++;
    }

    // PR_BIT_DIFFUSE_DETAIL_MAP
    if (programbits & (1 << PR_BIT_DIFFUSE_DETAIL_MAP))
    {
        float scale[2];

        // scale by the diffuse map scale if we're not doing normal mapping
        if (!(programbits & (1 << PR_BIT_NORMAL_MAP)))
        {
            scale[0] = material->diffusescale[0] * material->detailscale[0];
            scale[1] = material->diffusescale[1] * material->detailscale[1];
        } else {
            scale[0] = material->detailscale[0];
            scale[1] = material->detailscale[1];
        }

        buildgl_activeTexture(texunit + GL_TEXTURE0);
        //buildgl_bindSamplerObject(texunit, PTH_HIGHTILE);
        buildgl_bindTexture(GL_TEXTURE_2D, material->detailmap);

        glUniform1i(prprogram.uniform_detailMap, texunit);
        glUniform2fv(prprogram.uniform_detailScale, 1, scale);

        texunit++;
    }

    // PR_BIT_DIFFUSE_MODULATION
    if (programbits & (1 << PR_BIT_DIFFUSE_MODULATION))
    {
            glColor4ub(material->diffusemodulation[0],
                        material->diffusemodulation[1],
                        material->diffusemodulation[2],
                        material->diffusemodulation[3]);
    }

    // PR_BIT_SPECULAR_MAP
    if (programbits & (1 << PR_BIT_SPECULAR_MAP))
    {
        buildgl_activeTexture(texunit + GL_TEXTURE0);
        //buildgl_bindSamplerObject(texunit, (programbits & (1 << PR_BIT_ART_MAP)) ? PTH_INDEXED : PTH_HIGHTILE);
        buildgl_bindTexture(GL_TEXTURE_2D, material->specmap);

        glUniform1i(prprogram.uniform_specMap, texunit);

        texunit++;
    }

    // PR_BIT_SPECULAR_MATERIAL
    if (programbits & (1 << PR_BIT_SPECULAR_MATERIAL))
    {
        float specmaterial[2];

        if (pr_overridespecular) {
            specmaterial[0] = pr_specularpower;
            specmaterial[1] = pr_specularfactor;
            glUniform2fv(prprogram.uniform_specMaterial, 1, specmaterial);
        } else
            glUniform2fv(prprogram.uniform_specMaterial, 1, material->specmaterial);
    }

    // PR_BIT_MIRROR_MAP
    if (programbits & (1 << PR_BIT_MIRROR_MAP))
    {
        buildgl_activeTexture(texunit + GL_TEXTURE0);
        //buildgl_bindSamplerObject(texunit, PTH_CLAMPED);
        buildgl_bindTexture(GL_TEXTURE_RECTANGLE_ARB, material->mirrormap);

        glUniform1i(prprogram.uniform_mirrorMap, texunit);

        texunit++;
    }
#ifdef PR_LINEAR_FOG
    if (programbits & (1 << PR_BIT_FOG))
    {
        glUniform1i(prprogram.uniform_linearFog, r_usenewshading >= 2);
    }
#endif
    // PR_BIT_GLOW_MAP
    if (programbits & (1 << PR_BIT_GLOW_MAP))
    {
        buildgl_activeTexture(texunit + GL_TEXTURE0);
        //buildgl_bindSamplerObject(texunit, PTH_HIGHTILE|PTH_CLAMPED);
        buildgl_bindTexture(GL_TEXTURE_2D, material->glowmap);

        glUniform1i(prprogram.uniform_glowMap, texunit);

        texunit++;
    }

    // PR_BIT_POINT_LIGHT
    if (programbits & (1 << PR_BIT_POINT_LIGHT))
    {
        float inpos[4], pos[4];
        float range[2];
        float color[4];

        inpos[0] = (float)prlights[lights[curlight]].y;
        inpos[1] = -(float)prlights[lights[curlight]].z * (1.f/16.f);
        inpos[2] = -(float)prlights[lights[curlight]].x;

        polymer_transformpoint(inpos, pos, curmodelviewmatrix);

        // PR_BIT_SPOT_LIGHT
        if (programbits & (1 << PR_BIT_SPOT_LIGHT))
        {
            float sinang, cosang, sinhorizang, coshorizangs;
            float indir[3], dir[3];

            cosang = (float)(sintable[(-prlights[lights[curlight]].angle+1024)&2047]) / 16383.0f;
            sinang = (float)(sintable[(-prlights[lights[curlight]].angle+512)&2047]) / 16383.0f;
            coshorizangs = (float)(sintable[(getangle(128, prlights[lights[curlight]].horiz-100)+1024)&2047]) / 16383.0f;
            sinhorizang = (float)(sintable[(getangle(128, prlights[lights[curlight]].horiz-100)+512)&2047]) / 16383.0f;

            indir[0] = inpos[0] + sinhorizang * cosang;
            indir[1] = inpos[1] - coshorizangs;
            indir[2] = inpos[2] - sinhorizang * sinang;

            polymer_transformpoint(indir, dir, curmodelviewmatrix);

            dir[0] -= pos[0];
            dir[1] -= pos[1];
            dir[2] -= pos[2];

            indir[0] = (float)(sintable[(prlights[lights[curlight]].radius+512)&2047]) / 16383.0f;
            indir[1] = (float)(sintable[(prlights[lights[curlight]].faderadius+512)&2047]) / 16383.0f;
            indir[1] = 1.f / (indir[1] - indir[0]);

            glUniform3fv(prprogram.uniform_spotDir, 1, dir);
            glUniform2fv(prprogram.uniform_spotRadius, 1, indir);

            // PR_BIT_PROJECTION_MAP
            if (programbits & (1 << PR_BIT_PROJECTION_MAP))
            {
                GLfloat matrix[16];

                glMatrixMode(GL_TEXTURE);
                glLoadMatrixf(shadowBias);
                glMultMatrixf(prlights[lights[curlight]].proj);
                glMultMatrixf(prlights[lights[curlight]].transform);
                if (material->mdspritespace == GL_TRUE)
                    glMultMatrixf(spritemodelview);
                glGetFloatv(GL_TEXTURE_MATRIX, matrix);
                glLoadIdentity();
                glMatrixMode(GL_MODELVIEW);

                glUniformMatrix4fv(prprogram.uniform_shadowProjMatrix, 1, GL_FALSE, matrix);

                // PR_BIT_SHADOW_MAP
                if (programbits & (1 << PR_BIT_SHADOW_MAP))
                {
                    buildgl_activeTexture(texunit + GL_TEXTURE0);
                    //buildgl_bindSamplerObject(texunit, PTH_DEPTH_SAMPLER);
                    buildgl_bindTexture(prrts[prlights[lights[curlight]].rtindex].target, prrts[prlights[lights[curlight]].rtindex].z);

                    glUniform1i(prprogram.uniform_shadowMap, texunit);

                    texunit++;
                }

                // PR_BIT_LIGHT_MAP
                if (programbits & (1 << PR_BIT_LIGHT_MAP))
                {
                    buildgl_activeTexture(texunit + GL_TEXTURE0);
                    //buildgl_bindSamplerObject(texunit, PTH_CLAMPED);
                    buildgl_bindTexture(GL_TEXTURE_2D, prlights[lights[curlight]].lightmap);

                    glUniform1i(prprogram.uniform_lightMap, texunit);

                    texunit++;
                }
            }
        }

        range[0] = prlights[lights[curlight]].range * (1.f/1000.f);
        range[1] = 1.f/(range[0]*range[0]);

        color[0] = prlights[lights[curlight]].color[0] * (1.f/255.f);
        color[1] = prlights[lights[curlight]].color[1] * (1.f/255.f);
        color[2] = prlights[lights[curlight]].color[2] * (1.f/255.f);

        // If this isn't a lighting-only pass, just negate the components
        if (!curlight && prlights[lights[curlight]].publicflags.negative) {
            color[0] = -color[0];
            color[1] = -color[1];
            color[2] = -color[2];
        }

        glLightfv(GL_LIGHT0, GL_AMBIENT, pos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, color);
        if (material->mdspritespace == GL_TRUE) {
            float mdspritespacepos[3];
            polymer_transformpoint(inpos, mdspritespacepos, (float *)mdspritespace);
            glLightfv(GL_LIGHT0, GL_SPECULAR, mdspritespacepos);
        } else {
            glLightfv(GL_LIGHT0, GL_SPECULAR, inpos);
        }
        glLightfv(GL_LIGHT0, GL_LINEAR_ATTENUATION, &range[1]);
    }

    buildgl_activeTexture(GL_TEXTURE0);

    return programbits;
}

static void         polymer_unbindmaterial(int32_t programbits)
{
    auto &prprogram = *polymer_getprogram(programbits);
    // repair any dirty GL state here

    // PR_BIT_ANIM_INTERPOLATION
    if (programbits & (1 << PR_BIT_ANIM_INTERPOLATION))
    {
        if (prprogram.attrib_nextFrameNormal != -1)
            glDisableVertexAttribArray(prprogram.attrib_nextFrameNormal);
        glDisableVertexAttribArray(prprogram.attrib_nextFrameData);
    }

    // PR_BIT_LIGHTING_PASS
    if (programbits & (1 << PR_BIT_LIGHTING_PASS))
    {
        glPopAttrib();
    }

    // PR_BIT_NORMAL_MAP
    if (programbits & (1 << PR_BIT_NORMAL_MAP))
    {
        glDisableVertexAttribArray(prprogram.attrib_T);
        glDisableVertexAttribArray(prprogram.attrib_B);
        glDisableVertexAttribArray(prprogram.attrib_N);
    }

    buildgl_useShaderProgram(0);
}

static _prprograminfo *polymer_compileprogram(int32_t programbits)
{
    int32_t         i, enabledbits;
    GLuint          vert, frag, program;
    const GLchar*      source[PR_BIT_COUNT * 2];
    GLchar       infobuffer[PR_INFO_LOG_BUFFER_SIZE];
    GLint           linkstatus;

    // --------- VERTEX
    vert = glCreateShader(GL_VERTEX_SHADER);

    enabledbits = i = 0;
    while (i < PR_BIT_COUNT)
    {
        if (programbits & (1 << i))
            source[enabledbits++] = prprogrambits[i].vert_def;
        i++;
    }
    i = 0;
    while (i < PR_BIT_COUNT)
    {
        if (programbits & (1 << i))
            source[enabledbits++] = prprogrambits[i].vert_prog;
        i++;
    }

    glShaderSource(vert, enabledbits, source, NULL);

    glCompileShader(vert);

    // --------- FRAGMENT
    frag = glCreateShader(GL_FRAGMENT_SHADER);

    enabledbits = i = 0;
    while (i < PR_BIT_COUNT)
    {
        if (programbits & (1 << i))
            source[enabledbits++] = prprogrambits[i].frag_def;
        i++;
    }
    i = 0;
    while (i < PR_BIT_COUNT)
    {
        if (programbits & (1 << i))
            source[enabledbits++] = prprogrambits[i].frag_prog;
        i++;
    }

    glShaderSource(frag, enabledbits, (const GLchar**)source, NULL);

    glCompileShader(frag);

    // --------- PROGRAM
    program = glCreateProgram();

    glAttachShader(program, vert);
    glAttachShader(program, frag);

    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &linkstatus);

    glGetProgramInfoLog(program, PR_INFO_LOG_BUFFER_SIZE, NULL, infobuffer);

    auto &prprogram = *(_prprograminfo *)Xcalloc(1, sizeof(_prprograminfo));

    prprogram.handle = program;

#ifdef DEBUGGINGAIDS
    if (pr_verbosity >= 1)
#else
    if (pr_verbosity >= 2)
#endif
        VLOG_F(LOG_PR, "Compiling program with bits (octal) %o", (unsigned)programbits);
    if (!linkstatus) {
        VLOG_F(LOG_PR, "Failed to compile program with bits (octal) %o!", (unsigned)programbits);
        if (pr_verbosity >= 1)
        {
            glGetProgramInfoLog(program, PR_INFO_LOG_BUFFER_SIZE, NULL, infobuffer);
            VLOG_F(LOG_PR, "Linking log:\n%s", infobuffer);

            glGetShaderInfoLog(vert, PR_INFO_LOG_BUFFER_SIZE, NULL, infobuffer);
            VLOG_F(LOG_PR, "Vertex compile log:\n%s", infobuffer);

            glGetShaderInfoLog(frag, PR_INFO_LOG_BUFFER_SIZE, NULL, infobuffer);
            VLOG_F(LOG_PR, "Fragment compile log:\n%s", infobuffer);

            glGetShaderSource(vert, PR_INFO_LOG_BUFFER_SIZE, NULL, infobuffer);
            VLOG_F(LOG_PR, "Vertex source dump:\n%s", infobuffer);

            glGetShaderSource(frag, PR_INFO_LOG_BUFFER_SIZE, NULL, infobuffer);
            VLOG_F(LOG_PR, "Fragment source dump:\n%s", infobuffer);
        }
    }

    // --------- ATTRIBUTE/UNIFORM LOCATIONS

    prprogram.uniform_colorCorrection = glGetUniformLocation(program, "u_colorCorrection");

    if (programbits & (1 << PR_BIT_ANIM_INTERPOLATION))
    {
        prprogram.attrib_nextFrameData   = glGetAttribLocation(program, "nextFrameData");
        prprogram.attrib_nextFrameNormal = glGetAttribLocation(program, "nextFrameNormal");
        prprogram.uniform_frameProgress  = glGetUniformLocation(program, "frameProgress");
    }

    if (programbits & (1 << PR_BIT_NORMAL_MAP))
    {
        prprogram.attrib_T = glGetAttribLocation(program, "T");
        prprogram.attrib_B = glGetAttribLocation(program, "B");
        prprogram.attrib_N = glGetAttribLocation(program, "N");
        prprogram.uniform_eyePosition = glGetUniformLocation(program, "eyePosition");
        prprogram.uniform_normalMap = glGetUniformLocation(program, "normalMap");
        prprogram.uniform_normalBias  = glGetUniformLocation(program, "normalBias");
    }

    if (programbits & (1 << PR_BIT_ART_MAP))
    {
        prprogram.uniform_artMap      = glGetUniformLocation(program, "artMap");
        prprogram.uniform_basePalMap = glGetUniformLocation(program, "basePalMap");
        prprogram.uniform_lookupMap = glGetUniformLocation(program, "lookupMap");
        prprogram.uniform_shadeOffset = glGetUniformLocation(program, "shadeOffset");
        prprogram.uniform_visibility  = glGetUniformLocation(program, "visibility");
    }

    if (programbits & (1 << PR_BIT_DIFFUSE_MAP))
    {
        prprogram.uniform_diffuseMap   = glGetUniformLocation(program, "diffuseMap");
        prprogram.uniform_diffuseScale = glGetUniformLocation(program, "diffuseScale");
    }

    if (programbits & (1 << PR_BIT_HIGHPALOOKUP_MAP)) prprogram.uniform_highPalookupMap = glGetUniformLocation(program, "highPalookupMap");

    if (programbits & (1 << PR_BIT_DIFFUSE_DETAIL_MAP))
    {
        prprogram.uniform_detailMap   = glGetUniformLocation(program, "detailMap");
        prprogram.uniform_detailScale = glGetUniformLocation(program, "detailScale");
    }

    if (programbits & (1 << PR_BIT_SPECULAR_MAP))      prprogram.uniform_specMap          = glGetUniformLocation(program, "specMap");
    if (programbits & (1 << PR_BIT_SPECULAR_MATERIAL)) prprogram.uniform_specMaterial     = glGetUniformLocation(program, "specMaterial");
    if (programbits & (1 << PR_BIT_MIRROR_MAP))        prprogram.uniform_mirrorMap        = glGetUniformLocation(program, "mirrorMap");
#ifdef PR_LINEAR_FOG
    if (programbits & (1 << PR_BIT_FOG))               prprogram.uniform_linearFog        = glGetUniformLocation(program, "linearFog");
#endif
    if (programbits & (1 << PR_BIT_GLOW_MAP))          prprogram.uniform_glowMap          = glGetUniformLocation(program, "glowMap");
    if (programbits & (1 << PR_BIT_PROJECTION_MAP))    prprogram.uniform_shadowProjMatrix = glGetUniformLocation(program, "shadowProjMatrix");
    if (programbits & (1 << PR_BIT_SHADOW_MAP))        prprogram.uniform_shadowMap        = glGetUniformLocation(program, "shadowMap");
    if (programbits & (1 << PR_BIT_LIGHT_MAP))         prprogram.uniform_lightMap         = glGetUniformLocation(program, "lightMap");

    if (programbits & (1 << PR_BIT_SPOT_LIGHT))
    {
        prprogram.uniform_spotDir = glGetUniformLocation(program, "spotDir");
        prprogram.uniform_spotRadius = glGetUniformLocation(program, "spotRadius");
    }

    inthash_add(&prprogramtable, programbits, (intptr_t)&prprogram, 0);
    prprogramptrs.append(&prprogram);
    return prprogramptrs.last();
}

// LIGHTS
static void         polymer_removelight(int16_t lighti)
{
    _prplanelist*   oldhead;

    while (prlights[lighti].planelist)
    {
        polymer_deleteplanelight(prlights[lighti].planelist->plane, lighti);
        oldhead = prlights[lighti].planelist;
        prlights[lighti].planelist = prlights[lighti].planelist->n;
        oldhead->n = plpool;
        plpool = oldhead;
        plpool->plane = NULL;
    }
    prlights[lighti].planecount = 0;
    prlights[lighti].planelist = NULL;
}

static void         polymer_updatelights(void)
{
    int32_t         i = 0;

    do
    {
        _prlight* light = &prlights[i];

        if (light->flags.active && light->flags.invalidate) {
            // highly suboptimal
            polymer_removelight(i);

            if (light->radius)
                polymer_processspotlight(light);

            if (!polymer_culllight(i))
                light->flags.invalidate = 0;
        }

        if (light->flags.active) {
            // get the texture handle for the lightmap
            if (light->radius && light->tilenum > 0)
            {
                int16_t     picnum = light->tilenum;
                pthtyp*     pth;

                tileUpdatePicnum(&picnum, 0);

                if (!waloff[picnum])
                    tileLoad(picnum);

                pth = NULL;
                pth = texcache_fetch(picnum, 0, 0, DAMETH_NOMASK);

                if (pth)
                    light->lightmap = rhi->texture_handle(pth->rhipic);
            }

            light->rtindex = -1;
        }
    }
    while (++i < PR_MAXLIGHTS);
}

static FORCE_INLINE void  polymer_resetplanelights(_prplane* plane)
{
    Bmemset(&plane->lights[0], -1, sizeof(plane->lights[0]) * plane->lightcount);
    plane->lightcount = 0;
}

static void         polymer_addplanelight(_prplane* plane, int16_t lighti)
{
    _prplanelist*   oldhead;
    int32_t         i = 0;

    if (plane->lightcount)
    {
        if (plane->lightcount == PR_MAXPLANELIGHTS - 1)
            return;

        do
        {
            if (plane->lights[i++] == lighti)
                goto out;
        }
        while (i < plane->lightcount);

        i = 0;
        while (i < plane->lightcount && prlights[plane->lights[i]].priority < prlights[lighti].priority)
            i++;
        Bmemmove(&plane->lights[i+1], &plane->lights[i], sizeof(int16_t) * (plane->lightcount - i));
    }

    plane->lights[i] = lighti;
    plane->lightcount++;

out:
    oldhead = prlights[lighti].planelist;
    while (oldhead != NULL)
    {
        if (oldhead->plane == plane) return;
        oldhead = oldhead->n;
    }

    oldhead = prlights[lighti].planelist;
    if (plpool == NULL)
    {
        prlights[lighti].planelist = (_prplanelist *) Xmalloc(sizeof(_prplanelist));
        prlights[lighti].planelist->n = oldhead;
    }
    else
    {
        prlights[lighti].planelist = plpool;
        plpool = plpool->n;
        prlights[lighti].planelist->n = oldhead;
    }

    prlights[lighti].planelist->plane = plane;
    prlights[lighti].planecount++;
}

static FORCE_INLINE void polymer_deleteplanelight(_prplane *const plane, int16_t const lighti)
{
    for (int i = plane->lightcount - 1; i >= 0; --i)
    {
        if (plane->lights[i] == lighti)
        {
            Bmemmove(&plane->lights[i], &plane->lights[i+1], sizeof(int16_t) * (plane->lightcount - i));
            plane->lightcount--;
            return;
        }
    }
}

static int polymer_planeinlight(_prplane const &plane, _prlight const &light)
{
    if (!plane.vertcount)
        return 0;

    if (light.radius)
        return polymer_planeinfrustum(plane, light.frustum);

    float const lightpos[3] = { (float)light.y,
                               -(float)light.z * (1.f/16.f),
                               -(float)light.x };
    int i = 0, j, k, l;

    do
    {
        j = k = l = 0;

        do
        {
            if ((&plane.buffer[j].x)[i] > (lightpos[i] + light.range)) k++;
            if ((&plane.buffer[j].x)[i] < (lightpos[i] - light.range)) l++;
        }
        while (++j < plane.vertcount);

        if ((k == plane.vertcount) || (l == plane.vertcount))
            return 0;
    }
    while (++i < 3);

    return 1;
}

static FORCE_INLINE void polymer_invalidateplanelights(_prplane const &plane)
{
    int i = plane.lightcount;

    while (i--)
    {
        if (((unsigned)plane.lights[i] < PR_MAXPLANELIGHTS) && (prlights[plane.lights[i]].flags.active))
            prlights[plane.lights[i]].flags.invalidate = 1;
    }
}

static void polymer_invalidatesectorlights(int16_t const sectnum)
{
    auto s = prsectors[sectnum];
    auto sec = (usectorptr_t)&sector[sectnum];

    if (!s)
        return;

    polymer_invalidateplanelights(s->floor);
    polymer_invalidateplanelights(s->ceil);

    int i = sec->wallnum;

    while (i--)
    {
        _prwall *w;
        if (!(w = prwalls[sec->wallptr + i])) continue;

        polymer_invalidateplanelights(w->wall);
        polymer_invalidateplanelights(w->over);
        polymer_invalidateplanelights(w->mask);
    }
}

static void polymer_processspotlight(_prlight *const light)
{
    // hack to avoid lights beams perpendicular to walls
    if ((light->horiz <= 100) && (light->horiz > 90))
        light->horiz = 90;

    if ((light->horiz > 100) && (light->horiz < 110))
        light->horiz = 110;

    float const lightpos[3] = { (float)light->y,
                               -(float)light->z * (1.f/16.f),
                               -(float)light->x };

    // calculate the spot light transformations and matrices
    float const radius   = (float)(light->radius) * (360.f/2048.f);
    float const ang      = (float)(light->angle) * (360.f/2048.f);
    float const horizang = (float)(-getangle(128, light->horiz-100)) * (360.f/2048.f);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    buildgl_setPerspective(radius * 2, 1, 0.1f, light->range * (1.f/1000.f));
    glGetFloatv(GL_PROJECTION_MATRIX, light->proj);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRotatef(horizang, 1.0f, 0.0f, 0.0f);
    glRotatef(ang, 0.0f, 1.0f, 0.0f);
    glScalef(1.0f / 1000.0f, 1.0f / 1000.0f, 1.0f / 1000.0f);
    glTranslatef(-lightpos[0], -lightpos[1], -lightpos[2]);
    glGetFloatv(GL_MODELVIEW_MATRIX, light->transform);
    glPopMatrix();

    polymer_extractfrustum(light->transform, light->proj, light->frustum);

    light->rtindex = -1;
    light->lightmap = 0;
}

static int polymer_culllight(int16_t lighti)
{
    _prlight const &light = prlights[lighti];

    int32_t front = 0;
    int32_t back  = 1;
#ifdef YAX_ENABLE
    int16_t bunchnum;
#endif

    if (!sectorsareconnected(globalcursectnum, light.sector)) return 1;

    Bmemset(drawingstate, 0, sizeof(drawingstate));
    bitmap_set(drawingstate, light.sector);
    sectorqueue[0] = light.sector;

    do
    {
        auto s   = prsectors[sectorqueue[front]];
        auto sec = (usectorptr_t)&sector[sectorqueue[front]];

        polymer_pokesector(sectorqueue[front]);

#ifdef YAX_ENABLE
        int checkror = FALSE;
#endif

        if (!(sec->floorstat & 1))
        {
            int32_t zdiff = light.z - s->floorz;
            if (zdiff < 0)
                zdiff = -zdiff;
            zdiff >>= 4;

            if (!light.radius) {
                if (zdiff < light.range) {
                    polymer_addplanelight(&s->floor, lighti);
#ifdef YAX_ENABLE
                    checkror = TRUE;
#endif
                }
            } else if (polymer_planeinlight(s->floor, light)) {
                polymer_addplanelight(&s->floor, lighti);
#ifdef YAX_ENABLE
                checkror = TRUE;
#endif
            }
        }

#ifdef YAX_ENABLE
        // queue ROR neighbors
        if (checkror &&
            (bunchnum = yax_getbunch(sectorqueue[front], YAX_FLOOR)) >= 0) {

            for (int16_t SECTORS_OF_BUNCH(bunchnum, YAX_CEILING, ns)) {

                if (ns >= 0 && !bitmap_test(drawingstate, ns) &&
                    polymer_planeinlight(prsectors[ns]->ceil, light)) {

                    sectorqueue[back++] = ns;
                    bitmap_set(drawingstate, ns);
                }
            }
        }
        checkror = FALSE;
#endif

        if (!(sec->ceilingstat & 1))
        {
            int32_t zdiff = light.z - s->ceilingz;
            if (zdiff < 0)
                zdiff = -zdiff;
            zdiff >>= 4;

            if (!light.radius) {
                if (zdiff < light.range) {
                    polymer_addplanelight(&s->ceil, lighti);
#ifdef YAX_ENABLE
                    checkror = TRUE;
#endif
                }
            } else if (polymer_planeinlight(s->ceil, light)) {
                polymer_addplanelight(&s->ceil, lighti);
#ifdef YAX_ENABLE
                checkror = TRUE;
#endif
            }
        }

#ifdef YAX_ENABLE
        // queue ROR neighbors
        if (checkror &&
            (bunchnum = yax_getbunch(sectorqueue[front], YAX_CEILING)) >= 0) {

            for (int16_t SECTORS_OF_BUNCH(bunchnum, YAX_FLOOR, ns)) {

                if (ns >= 0 && !bitmap_test(drawingstate, ns) &&
                    polymer_planeinlight(prsectors[ns]->floor, light)) {

                    sectorqueue[back++] = ns;
                    bitmap_set(drawingstate, ns);
                }
            }
        }
#endif
        int i = 0;
        do
        {
            auto w    = prwalls[sec->wallptr + i];
            auto wal  = (uwallptr_t)&wall[sec->wallptr + i];
            auto nsec = (wal->nextsector >= 0 && wal->nextsector < numsectors) ? (usectorptr_t)&sector[wal->nextsector] : nullptr;

            int j = 0;

            if (!wallvisible(light.x, light.y, sec->wallptr + i) && (wal->nextwall == -1 || !wallvisible(light.x, light.y, wal->nextwall)))
                continue;

            if (!(sec->floorstat & 1 && nsec && nsec->floorstat & 1))
            {
                if (polymer_planeinlight(w->wall, light)) {
                    polymer_addplanelight(&w->wall, lighti);
                    j++;
                }
            }

            if (!(sec->ceilingstat & 1 && nsec && nsec->ceilingstat & 1))
            {
                if (polymer_planeinlight(w->over, light)) {
                    polymer_addplanelight(&w->over, lighti);
                    j++;
                }
            }

            // assume the light hits the middle section if it hits the top and bottom
            if (wallvisible(light.x, light.y, sec->wallptr + i) && (j == 2 || polymer_planeinlight(w->mask, light)))
            {
                if ((w->mask.vertcount == 4) &&
                    (w->mask.buffer[0].y >= w->mask.buffer[3].y) &&
                    (w->mask.buffer[1].y >= w->mask.buffer[2].y))
                    continue;

                polymer_addplanelight(&w->mask, lighti);

                int ns = wall[sec->wallptr + i].nextsector;

                if (((unsigned)ns < MAXSECTORS) && !bitmap_test(drawingstate, ns))
                {
                    bitmap_set(drawingstate, ns);

                    if (getsectordist(light.xy, ns) < light.range)
                        sectorqueue[back++] = ns;
                }
            }
        }
        while (++i < sec->wallnum);

        for (int16_t SPRITES_OF_SECT(sectorqueue[front], i))
        {
            _prsprite *s = prsprites[i];

            if ((sprite[i].cstat & CSTAT_SPRITE_ALIGNMENT) == CSTAT_SPRITE_ALIGNMENT_FACING || s == NULL)
                continue;

            if (polymer_planeinlight(s->plane, light))
                polymer_addplanelight(&s->plane, lighti);
        }

        front++;
    }
    while (front != back);

    return 0;
}

static void         polymer_prepareshadows(void)
{
    // for wallvisible()
    int32_t gx = globalposx;
    int32_t gy = globalposy;
    int32_t gz = globalposz;
    // build globals used by drawmasks
    fix16_t oviewangle = viewangle;
    fix16_t oglobalang = qglobalang;

    int i=0, j=0, k=0;

    while ((k < lightcount) && (j < pr_shadowcount))
    {
        while (!prlights[i].flags.active)
            i++;

        if (prlights[i].radius && prlights[i].publicflags.emitshadow &&
            prlights[i].flags.isinview)
        {
            prlights[i].flags.isinview = 0;
            prlights[i].rtindex = j + 1;
            if (pr_verbosity >= 3) VLOG_F(LOG_PR, "Drawing shadow %i", i);

            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, prrts[prlights[i].rtindex].fbo);
            glPushAttrib(GL_VIEWPORT_BIT);
            glViewport(0, 0, prrts[prlights[i].rtindex].xdim, prrts[prlights[i].rtindex].ydim);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadMatrixf(prlights[i].proj);
            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(prlights[i].transform);

            buildgl_setEnabled(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(5, SHADOW_DEPTH_OFFSET);

            set_globalpos(prlights[i].x, prlights[i].y, prlights[i].z);

            // build globals used by rotatesprite
            viewangle = fix16_from_int(prlights[i].angle);
            set_globalang(fix16_from_int(prlights[i].angle));

            int32_t oldoverridematerial = overridematerial;
            // smooth model shadows
            overridematerial = (1 << PR_BIT_ANIM_INTERPOLATION);
            // used by alpha-testing for sprite silhouette
            overridematerial |= (1 << PR_BIT_DIFFUSE_MAP);
            overridematerial |= (1 << PR_BIT_DIFFUSE_MAP2);

            // to force sprite drawing
            mirrors[depth++].plane = NULL;
            polymer_displayrooms(prlights[i].sector);
            depth--;

            overridematerial = oldoverridematerial;

            buildgl_setDisabled(GL_POLYGON_OFFSET_FILL);

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();

            glPopAttrib();
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

            j++;
        }
        i++;
        k++;
    }

    set_globalpos(gx, gy, gz);

    viewangle = oviewangle;
    set_globalang(oglobalang);
}

// RENDER TARGETS
static void         polymer_initrendertargets(int32_t count)
{
    int32_t         i;

    static int32_t ocount;

    if (count == 0)  // uninit
    {
        if (prrts)
        {
            for (i=0; i<ocount; i++)
            {
                if (prrts[i].color)
                {
                    glDeleteTextures(1, &prrts[i].color);
                    prrts[i].color = 0;
                }
                glDeleteTextures(1, &prrts[i].z);
                prrts[i].z = 0;

                glDeleteFramebuffersEXT(1, &prrts[i].fbo);
                prrts[i].fbo = 0;
            }
            DO_FREE_AND_NULL(prrts);
        }

        ocount = 0;
        return;
    }

    ocount = count;
    //////////

    prrts = (_prrt *)Xcalloc(count, sizeof(_prrt));

    i = 0;
    while (i < count)
    {
        if (!i) {
            prrts[i].target = GL_TEXTURE_RECTANGLE_ARB;
            prrts[i].xdim = xdim;
            prrts[i].ydim = ydim;

            glGenTextures(1, &prrts[i].color);
            buildgl_bindTexture(prrts[i].target, prrts[i].color);

            glTexImage2D(prrts[i].target, 0, GL_RGB, prrts[i].xdim, prrts[i].ydim, 0, GL_RGB, GL_SHORT, NULL);
            glTexParameteri(prrts[i].target, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(prrts[i].target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(prrts[i].target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(prrts[i].target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(prrts[i].target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } else {
            prrts[i].target = GL_TEXTURE_2D;
            prrts[i].xdim = 128 << pr_shadowdetail;
            prrts[i].ydim = 128 << pr_shadowdetail;
            prrts[i].color = 0;

            if (pr_ati_fboworkaround) {
                glGenTextures(1, &prrts[i].color);
                buildgl_bindTexture(prrts[i].target, prrts[i].color);

                glTexImage2D(prrts[i].target, 0, GL_RGB, prrts[i].xdim, prrts[i].ydim, 0, GL_RGB, GL_SHORT, NULL);
                glTexParameteri(prrts[i].target, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(prrts[i].target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(prrts[i].target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(prrts[i].target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(prrts[i].target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
        }

        glGenTextures(1, &prrts[i].z);
        buildgl_bindTexture(prrts[i].target, prrts[i].z);

        glTexImage2D(prrts[i].target, 0, GL_DEPTH_COMPONENT, prrts[i].xdim, prrts[i].ydim, 0, GL_DEPTH_COMPONENT, GL_SHORT, NULL);
        glTexParameteri(prrts[i].target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(prrts[i].target, GL_TEXTURE_MIN_FILTER, pr_shadowfiltering ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(prrts[i].target, GL_TEXTURE_MAG_FILTER, pr_shadowfiltering ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(prrts[i].target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(prrts[i].target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(prrts[i].target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
        glTexParameteri(prrts[i].target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glTexParameteri(prrts[i].target, GL_DEPTH_TEXTURE_MODE, GL_ALPHA);

        glGenFramebuffersEXT(1, &prrts[i].fbo);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, prrts[i].fbo);

        if (prrts[i].color)
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                       prrts[i].target, prrts[i].color, 0);
        else {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, prrts[i].target, prrts[i].z, 0);

        if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
        {
            VLOG_F(LOG_PR, "FBO #%d initialization failed.", i);
        }

        buildgl_bindTexture(prrts[i].target, 0);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

        i++;
    }
}

#endif
