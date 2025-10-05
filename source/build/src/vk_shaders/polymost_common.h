#extension GL_EXT_nonuniform_qualifier : enable

struct PerDraw
{
    mat4 modelViewMatrix;
    mat4 modelViewProjectionMatrix;
    mat4 rotMatrix;
    mat4 textureMatrix;
    vec4 npotEmulation;
    vec3 fogColor;
    uint textureId;
    bool fogEnabled;
    float fogEnd;
    float fogScale;
    bool useColorOnly;
    bool usePalette;
    bool useExtended;
};

struct PerDrawPaletted
{
    vec4 texturePosSize;
    vec2 halfTexelSize;
    vec2 texClamp;
    vec2 palswapPos;
    float shade;
    float visFactor;
};

struct PerDrawExtended
{
    mat4 detailMatrix;
    mat4 glowMatrix;
    uint detailId;
    uint glowId;
    bool useDetailMapping;
    bool useGlowMapping;
};

layout(set = 0, binding = 0) buffer upd { PerDraw bPerDraw[]; };
layout(set = 0, binding = 1) buffer updp { PerDrawPaletted bPerDrawPaletted[]; };
layout(set = 0, binding = 2) buffer upde { PerDrawExtended bPerDrawExtended[]; };
layout(set = 1, binding = 0) uniform sampler2D uTextures[];
layout(set = 1, binding = 1) uniform ConstData 
{
    vec4 u_colorCorrection;
    vec2 u_numShades;
    vec2 u_palswapSize;
    float u_brightness;
    float u_shadeInterpolate;
    uint u_palswapId;
    uint u_paletteId;
};

layout(push_constant) uniform pushConstant
{
    uint perDrawIndex;
    uint perDrawPalettedIndex;
    uint perDrawExtendedIndex;
} uPushConstant;

const float c_zero = 0.0;
const float c_one  = 1.0;
const float c_two = 2.0;
const vec4 c_vec4_one = vec4(c_one);
const float c_wrapThreshold = 0.9;
