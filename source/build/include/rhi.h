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

enum rhiUniformType {
    RHI_UNIFORM_TEXUNIT = 0,
    RHI_UNIFORM_FLOAT,
    RHI_UNIFORM_FLOAT2,
    RHI_UNIFORM_FLOAT4,
    RHI_UNIFORM_MATRIX,
    RHI_UNIFORM_MAX
};

enum rhiUnformUsage {
    RHI_UNIFORM_USAGE_TEXUNIT = 0,
    RHI_UNIFORM_USAGE_PER_FRAME,
    RHI_UNIFORM_USAGE_PER_DRAW_0,
    RHI_UNIFORM_USAGE_PER_DRAW_1,
    RHI_UNIFORM_USAGE_PER_DRAW_2,
};

struct rhiProgramUniformInfo {
    int type;
    const char* gl_name;
    int vk_usage;
    int vk_offset;
};

struct rhiProgramInfo {
    const char* glsl_vertex_shader;
    const char* glsl_fragment_shader;
    const uint32_t* spirv_vertex_shader;
    const uint32_t* spirv_fragment_shader;
    int uniform_count;
    rhiProgramUniformInfo* uniforms;
    int texunits;
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

struct rhiVerticesFormatInfo {
    int count;
    int stride;
    int position_offset;
    int texcoord_offset;
    int normal_offset;
    int color_offset;
};

struct rhiVertex {
    vec3_t position;
    vec3_t normal;
    vec3_t color;
    vec2_t texcoord;
};

typedef void* rhiTexture;
typedef void* rhiSampler;
typedef void* rhiProgram;
typedef void* rhiVertices;
typedef void* rhiIndices;

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

    rhiProgram current_program;

    virtual void init() = 0;
    virtual void reset() = 0;
    virtual rhiProgram create_program(rhiProgramInfo* info) = 0;
    virtual void uniform_set_texunit(rhiProgram program, int uniform, int texunit) = 0;
    virtual void uniform_set_float(rhiProgram program, int uniform, float value) = 0;
    virtual void uniform_set_float2(rhiProgram program, int uniform, float value1, float value2) = 0;
    virtual void uniform_set_float4(rhiProgram program, int uniform, float value1, float value2, float value3, float value4) = 0;
    virtual void uniform_set_matrix(rhiProgram program, int uniform, float* value) = 0;
    //virtual void set_program(rhiProgramHandle program) = 0;
    //virtual void begin_rendering() = 0;
    //virtual void end_rendering() = 0;
    //virtual void set_state() = 0;
    virtual rhiTexture texture_create(rhiTextureCreateInfo* info) = 0;
    virtual void texture_destroy(rhiTexture texture) = 0;
    virtual void texture_update(rhiTexture texture, rhiTextureUploadInfo* info) = 0;
    virtual rhiSampler sampler_create(rhiSamplerCreateInfo* info) = 0;
    virtual void sampler_destroy(rhiSampler sampler) = 0;

    virtual void texunit_bind(int texunit, rhiTexture texture, rhiSampler sampler) = 0;
    virtual void texunit_unbind(int texunit) = 0;
    virtual void texunit_setscale(int texunit, float x, float y, float sx, float sy) = 0;

    virtual rhiVertices buffer_vertices_alloc(rhiVerticesFormatInfo* info, void* data) = 0;
    virtual rhiIndices buffer_indices_alloc(int count, unsigned int* data) = 0;
    virtual void buffer_vertices_destroy(rhiVertices vertices) = 0;
    virtual void buffer_indices_destroy(rhiIndices indices) = 0;

#if 0
    virtual void matrix_set_projection(float* matrix) = 0;
    virtual void matrix_set_modelview(float* matrix) = 0;

    virtual rhiVertex* draw_begin(rhiTopology topology, int count) = 0;
    virtual void draw_end() = 0;
    virtual void draw_buffer(rhiTopology topology, rhiVertices vert, rhiIndices ind) = 0;
#endif

    // temp
    virtual void program_bind(rhiProgram program) = 0;
    virtual int texture_handle(rhiTexture texture) = 0;
};

extern rhiInterface* rhi;

extern rhiSampler tempSampler;


