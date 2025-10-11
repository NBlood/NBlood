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
	RHI_FORMAT_B8G8R8A8_UNORM,
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
	float maxanisotropy;
	int maxTextureSize;
	union {
		uint32_t features;
		struct
		{
			unsigned int bgra : 1;
#if 0
			unsigned int bufferstorage : 1;
			unsigned int debugoutput : 1;
			unsigned int depthclamp : 1;
			unsigned int depthtex : 1;
			unsigned int fbos : 1;
			unsigned int glsl : 1;
			unsigned int multitex : 1;
			unsigned int occlusionqueries : 1;
			unsigned int rect : 1;
			unsigned int reset_notification : 1;
			unsigned int samplerobjects : 1;
			unsigned int shadow : 1;
			unsigned int sync : 1;
			unsigned int texcompr : 1;
			unsigned int texnpot : 1;
			unsigned int vsync : 1;
#endif
		};
	};
	//virtual rhiProgramHandle create_program(rhiProgramInfo* info) = 0;
	//virtual void set_program(rhiProgramHandle program) = 0;
	//virtual void begin_rendering() = 0;
	//virtual void end_rendering() = 0;
	//virtual void set_state() = 0;
	virtual void init() = 0;
	virtual void reset() = 0;
	virtual rhiTexture texture_create(rhiTextureCreateInfo* info) = 0;
	virtual void texture_destroy(rhiTexture texture) = 0;
	virtual void texture_update(rhiTexture texture, rhiTextureUploadInfo* info) = 0;
	virtual rhiSampler sampler_create(rhiSamplerCreateInfo* info) = 0;
	virtual void sampler_destroy(rhiSampler sampler) = 0;


	virtual void texture_bind(rhiTexture texture) = 0;
	virtual int texture_handle(rhiTexture texture) = 0;
};

extern rhiInterface* rhi;


