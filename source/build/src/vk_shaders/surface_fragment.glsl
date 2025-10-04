#version 450

layout(location = 0) in vec2 texCoord;
layout(binding = 0) uniform usampler2D frameTex;
layout(binding = 1) uniform sampler2D paletteTex;

layout(location = 0) out vec4 outColor;

void main()
{
	ivec2 size = textureSize(frameTex, 0);
	ivec2 coord = ivec2(size * texCoord);
	uint texel = texelFetch(frameTex, coord, 0).r;
	outColor = texelFetch(paletteTex, ivec2(texel, 0), 0);
}
