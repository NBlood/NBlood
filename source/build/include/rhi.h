#pragma once
#include "compat.h"

enum rhiTopology {
	RHI_TOPOLOGY_FAN = 0,
	RHI_TOPOLOGY_LINES
};

enum rhiStateType {
	RHI_ALPHA_TEST = 0,
	RHI_DEPTH_TEST,

};

enum rhiTextureFormat {
	RHI_FORMAT_R8_UNORM = 0,
	RHI_FORMAT_R8_UINT,
	RHI_FORMAT_R8G8B8A8_UNORM,
	RHI_FORMAT_MAX
};

enum rhiSamplerWrap {
	RHI_SAMPLER_REPEAT = 0,
	RHI_SAMPLER_CLAMP_TO_EDGE,
	RHI_SAMPLER_WRAP_MAX
};

enum rhiSamplerFilter {
	RHI_SAMPLER_NEAREST = 0,
	RHI_SAMPLER_LINEAR,
	RHI_SAMPLER_LINEAR_MIPMAP_LINEAR,
	RHI_SAMPLER_FILTER_MAX
};

struct rhiProgramInfo {
	const char* glsl_vertex_shader;
	const char* glsl_fragment_shader;
	const uint32_t* glsl_vertex_spirv;
	const uint32_t* glsl_fragment_spirv;
};

struct rhiTextureCreateInfo {
	int width;
	int height;
	int levels;
	int format;
};

struct rhiTextureUploadInfo {
	int x;
	int y;
	int width;
	int height;
	int level;
	void* data;
};

struct rhiSamplerCreateInfo {
	int mag_filter;
	int min_filter;
	int wrap_s;
	int wrap_t;
	float max_anisotropy;
};

typedef void* rhiTexture;
typedef void* rhiSampler;
typedef void* rhiProgramHandle;

void rhi_init();
void rhi_shutdown();

struct rhiInterface
{
	//virtual rhiProgramHandle create_program(rhiProgramInfo* info) = 0;
	//virtual void set_program(rhiProgramHandle program) = 0;
	//virtual void begin_rendering() = 0;
	//virtual void end_rendering() = 0;
	//virtual void set_state() = 0;
	virtual rhiTexture texture_create(rhiTextureCreateInfo* info) = 0;
	virtual void texture_destroy(rhiTexture texture) = 0;
	virtual void texture_update(rhiTexture texture, rhiTextureUploadInfo* info) = 0;
	virtual rhiSampler sampler_create(rhiSamplerCreateInfo* info) = 0;
	virtual void sampler_destroy(rhiSampler sampler) = 0;
};

extern rhiInterface* rhi;


