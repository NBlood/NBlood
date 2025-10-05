#version 450

#include "polymost_common.h"

layout(location = 0) in vec4 v_color;
layout(location = 1) in float v_distance;
layout(location = 2) in vec2 v_texCoord0;
layout(location = 3) in vec2 v_texCoord3;
layout(location = 4) in vec2 v_texCoord4;
layout(location = 5) in float v_fogCoord;

layout(location = 0) out vec4 outColor;

const float c_basepalScale = 255.0/256.0;
const float c_basepalOffset = 0.5/256.0;

const vec2 c_vec2_zero_one = vec2(c_zero, c_one);
const vec4 c_vec4_luma_709 = vec4(0.2126, 0.7152, 0.0722, 0.0);

void main()
{
    PerDraw data = bPerDraw[uPushConstant.perDrawIndex];
    vec2 coord = mix(v_texCoord0.xy,v_texCoord0.yx,float(data.usePalette));
    float modCoordYnpotEmulationFactor = mod(coord.y,data.npotEmulation.y);
    coord.xy = vec2(floor(modCoordYnpotEmulationFactor)*data.npotEmulation.x+coord.x, floor(coord.y*data.npotEmulation.w)+modCoordYnpotEmulationFactor);
    vec2 newCoord = mix(v_texCoord0.xy,mix(coord.xy,coord.yx,float(data.usePalette)),data.npotEmulation.z);
    
    vec4 color;
    if (data.useColorOnly)
    {
        color = c_vec4_one;
    }
    else if (data.usePalette)
    {
        PerDrawPaletted data_p = bPerDrawPaletted[uPushConstant.perDrawPalettedIndex];
        vec2 texCoord = mix(fract(newCoord.xy), clamp(newCoord.xy, c_zero, c_one), data_p.texClamp);
        texCoord = clamp(data_p.texturePosSize.zw*texCoord, data_p.halfTexelSize, data_p.texturePosSize.zw-data_p.halfTexelSize);
        color = textureGrad(uTextures[data.textureId], data_p.texturePosSize.xy+texCoord, dFdx(v_texCoord0), dFdy(v_texCoord0));
        float shade = clamp((data_p.shade+clamp(data_p.visFactor*v_distance-0.5*u_shadeInterpolate,c_zero,u_numShades.x)), c_zero, u_numShades.x-c_one);
        float shadeFrac = mod(shade, c_one);
        float colorIndex = texture(uTextures[u_palswapId], vec2(color.r, floor(shade)*u_numShades.y)*u_palswapSize+data_p.palswapPos).r * c_basepalScale + c_basepalOffset;
        float colorIndexNext = texture(uTextures[u_palswapId], u_palswapSize*vec2(color.r, (floor(shade)+c_one)*u_numShades.y)+data_p.palswapPos).r * c_basepalScale + c_basepalOffset;
        vec4 palettedColor = texture(uTextures[u_paletteId], vec2(colorIndex, c_zero));
        vec4 palettedColorNext = texture(uTextures[u_paletteId], vec2(colorIndexNext, c_zero));
        palettedColor.rgb = mix(palettedColor.rgb, palettedColorNext.rgb, shadeFrac*u_shadeInterpolate);
        float fullbright = palettedColor.a;
        palettedColor.a = c_one-floor(color.r);
        color = palettedColor;

        color.rgb = mix(v_color.rgb*color.rgb, color.rgb, fullbright);

        if (data.fogEnabled)
        {
            float fogFactor = clamp((data.fogEnd-v_fogCoord)*data.fogScale, fullbright, c_one);
            color.rgb = mix(data.fogColor, color.rgb, fogFactor);
        }
    }
    else
    {
        PerDrawExtended data_e;
        color = texture(uTextures[data.textureId], newCoord);
        if (data.useExtended)
        {
            data_e = bPerDrawExtended[uPushConstant.perDrawExtendedIndex];
            if (data_e.useDetailMapping)
            {
                vec4 detailColor = texture(uTextures[data_e.detailId], v_texCoord3.xy);
                detailColor = mix(c_vec4_one, 2.0*detailColor, detailColor.a);
                color.rgb *= detailColor.rgb;
            }
        }

        if (data.fogEnabled)
        {
            float fogFactor = clamp((data.fogEnd-v_fogCoord)*data.fogScale, c_zero, c_one);
            color.rgb = mix(data.fogColor, color.rgb, fogFactor);
        }
        
        if (data.useExtended)
        {
            if (data_e.useGlowMapping)
            {
                coord = v_texCoord4;
                modCoordYnpotEmulationFactor = mod(coord.y,data.npotEmulation.y);
                coord.xy = vec2(floor(modCoordYnpotEmulationFactor)*data.npotEmulation.x+coord.x, floor(coord.y*data.npotEmulation.w)+modCoordYnpotEmulationFactor);
                newCoord = mix(v_texCoord4.xy,coord.xy,data.npotEmulation.z);
                vec4 glowColor = texture(uTextures[data_e.glowId], newCoord);
                color.rgb = mix(color.rgb, glowColor.rgb, glowColor.a);
            }
        }
    }

    color.a *= v_color.a;

    color.rgb = pow(color.rgb, vec3(u_brightness));

    vec4 v_cc = vec4(u_colorCorrection.x - c_one, 0.5 * -(u_colorCorrection.y-c_one), -(u_colorCorrection.z-c_one), 1.0);
    outColor = mat4(c_vec2_zero_one.yxxx, c_vec2_zero_one.xyxx, c_vec2_zero_one.xxyx, v_cc.xxxw)
             * mat4(u_colorCorrection.ywww, u_colorCorrection.wyww, u_colorCorrection.wwyw, v_cc.yyyw)
             * mat4((c_vec4_luma_709.xxxw * v_cc.z) + u_colorCorrection.zwww,
                    (c_vec4_luma_709.yyyw * v_cc.z) + u_colorCorrection.wzww,
                    (c_vec4_luma_709.zzzw * v_cc.z) + u_colorCorrection.wwzw,
                    c_vec2_zero_one.xxxy)
             * color;

}
