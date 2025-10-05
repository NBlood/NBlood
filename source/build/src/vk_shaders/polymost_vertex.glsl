#version 450

#include "polymost_common.h"

layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec2 in_texCoord0;
layout(location = 2) in vec2 in_texCoord3;
layout(location = 3) in vec2 in_texCoord4;
layout(location = 4) in vec4 in_color;

layout(location = 0) out vec4 v_color;
layout(location = 1) out float v_distance;
layout(location = 2) out vec2 v_texCoord0;
layout(location = 3) out vec2 v_texCoord3;
layout(location = 4) out vec2 v_texCoord4;
layout(location = 5) out float v_fogCoord;

void main()
{
    PerDraw data = bPerDraw[uPushConstant.perDrawIndex];
    vec4 vertex = data.rotMatrix * vec4(in_vertex, 0);
    vec4 eyeCoordPosition = data.modelViewMatrix * vertex;
    gl_Position = data.modelViewProjectionMatrix * vertex;

    eyeCoordPosition.xyz /= eyeCoordPosition.w;

    v_texCoord0 = (data.textureMatrix * vec4(in_texCoord0, 0, 0)).xy;
    v_texCoord0 = mix(v_texCoord0.xy, v_texCoord0.yx, float(data.usePalette));

    if (data.useExtended)
    {
        PerDrawExtended data_e = bPerDrawExtended[uPushConstant.perDrawExtendedIndex];
        v_texCoord3 = (data_e.detailMatrix * vec4(in_texCoord3, 0, 0)).xy;
        v_texCoord4 = (data_e.glowMatrix * vec4(in_texCoord4, 0, 0)).xy;
    }

    v_fogCoord = abs(eyeCoordPosition.z);

    v_color = in_color;
    v_distance = in_vertex.z;
}
