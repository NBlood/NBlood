#include "compat.h"
#include "rhi.h"
#include "glbuild.h"
#include "vk.h"

struct rhiState {
    bool alpha_test;
    bool depth_test;
    bool depth_write;
};

rhiInterface *rhi;

#if B_BIG_ENDIAN
#define GL_FORMAT_TYPE GL_UNSIGNED_INT_8_8_8_8
#else
#define GL_FORMAT_TYPE GL_UNSIGNED_INT_8_8_8_8_REV
#endif

static struct {
    GLuint internal_format;
    GLuint format;
    GLuint type;
} gl_format_map[RHI_FORMAT_MAX] = {
    GL_RED, GL_RED, GL_UNSIGNED_BYTE, // RHI_FORMAT_R8_UNORM
    GL_RED, GL_RED_INTEGER, GL_UNSIGNED_BYTE, // RHI_FORMAT_R8_UINT
    GL_RGBA, GL_RGBA, GL_FORMAT_TYPE, // RHI_FORMAT_R8G8B8A8_UNORM
    GL_BGRA, GL_BGRA, GL_FORMAT_TYPE, // RHI_FORMAT_B8G8R8A8_UNORM
};

static GLuint gl_sampler_wrap_map[RHI_SAMPLER_WRAP_MAX] = {
    GL_REPEAT,
    GL_CLAMP_TO_EDGE,
};

static GLuint gl_sampler_filter_map[RHI_SAMPLER_FILTER_MAX] = {
    GL_NEAREST,
    GL_LINEAR,
    GL_LINEAR_MIPMAP_LINEAR
};

struct glRhi : rhiInterface
{
    struct glUniform
    {
        GLuint handle;
        int type;
        const char* gl_name;
    };

    struct glProgram
    {
        GLuint program;
        int uniform_count;
        glUniform* uniforms;
    };

    struct glTexture
    {
        GLuint texture;
        rhiTextureCreateInfo info;
        rhiSamplerCreateInfo sampler;
    };

    struct glSampler
    {
        GLuint sampler;
        rhiSamplerCreateInfo info;
    };

    struct glVerticesBuffer
    {
        GLuint buffer;
        rhiVerticesFormatInfo info;
    };

    struct glIndicesBuffer
    {
        GLuint buffer;
        int count;
    };

    void init() override
    {
        buildgl_resetStateAccounting();
        maxTextureSize = glinfo.maxTextureSize;

#if (defined _MSC_VER) || (!defined BITNESS64)
        if (maxTextureSize > 8192)
            maxTextureSize = 8192;
#endif
        bgra = glinfo.bgra;

        current_program = nullptr;
    }

    void reset() override
    {
        buildgl_resetStateAccounting();
        buildgl_activeTexture(GL_TEXTURE0);

        glEnableClientState(GL_VERTEX_ARRAY);
    }

    static GLuint compileShader(GLenum shaderType, const char* const source, int* pLength = nullptr)
    {
        GLuint shaderID = glCreateShader(shaderType);
        if (shaderID == 0)
        {
            VLOG_F(LOG_GL, "glCreateShader failed!");
            return 0;
        }

        glShaderSource(shaderID,
            1,
            &source,
            pLength);
        glCompileShader(shaderID);

        GLint compileStatus;
        glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compileStatus);
        if (!compileStatus)
        {
            GLint logLength;
            glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
            VLOG_F(LOG_GL, "Compile Status: %u", compileStatus);
            if (logLength > 0)
            {
                char* infoLog = (char*)Xmalloc(logLength);
                glGetShaderInfoLog(shaderID, logLength, &logLength, infoLog);
                VLOG_F(LOG_GL, "Log:\n%s", infoLog);
                Xfree(infoLog);
            }
        }

        return shaderID;
    }

    rhiProgram create_program(rhiProgramInfo* info) override
    {
        glProgram* program = (glProgram*)Bcalloc(1, sizeof(glProgram));
        program->program = glCreateProgram();
        GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, info->glsl_vertex_shader);
        GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, info->glsl_fragment_shader);
        if (!program->program || !vertex_shader || !fragment_shader)
            goto fail;

        glAttachShader(program->program, vertex_shader);
        glAttachShader(program->program, fragment_shader);

        glLinkProgram(program->program);

        glDetachShader(program->program, vertex_shader);
        glDetachShader(program->program, fragment_shader);

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

        
        program->uniform_count = info->uniform_count;
        program->uniforms = (glUniform*)Bcalloc(info->uniform_count, sizeof(glUniform));
        for (int i = 0; i < info->uniform_count; i++)
        {
            auto& uinfo = info->uniforms[i];
            auto& u = program->uniforms[i];
            u.handle = glGetUniformLocation(program->program, uinfo.gl_name);
            u.type = uinfo.type;
            u.gl_name = uinfo.gl_name;
        }

        return (rhiProgram)program;
    fail:
        if (program->program)
            glDeleteProgram(program->program);
        if (vertex_shader)
            glDeleteShader(vertex_shader);
        if (fragment_shader)
            glDeleteShader(fragment_shader);
        Bfree(program);
        return nullptr;
    }

    void uniform_set_texunit(rhiProgram program, int uniform, int texunit) override
    {
        glProgram* prog = (glProgram*)program;

        if (!prog
            || (uint32_t)uniform >= (uint32_t)prog->uniform_count
            || prog->uniforms[uniform].type != RHI_UNIFORM_TEXUNIT)
            return;

        if (gl.currentShaderProgramID != prog->program)
            buildgl_useShaderProgram(prog->program);
        glUniform1i(prog->uniforms[uniform].handle, texunit);
    }

    void uniform_set_float(rhiProgram program, int uniform, float value) override
    {
        glProgram* prog = (glProgram*)program;

        if (!prog
            || (uint32_t)uniform >= (uint32_t)prog->uniform_count
            || prog->uniforms[uniform].type != RHI_UNIFORM_FLOAT)
            return;

        if (gl.currentShaderProgramID != prog->program)
            buildgl_useShaderProgram(prog->program);
        glUniform1f(prog->uniforms[uniform].handle, value);
    }

    void uniform_set_float2(rhiProgram program, int uniform, float value1, float value2) override
    {
        glProgram* prog = (glProgram*)program;

        if (!prog
            || (uint32_t)uniform >= (uint32_t)prog->uniform_count
            || prog->uniforms[uniform].type != RHI_UNIFORM_FLOAT2)
            return;

        if (gl.currentShaderProgramID != prog->program)
            buildgl_useShaderProgram(prog->program);
        glUniform2f(prog->uniforms[uniform].handle, value1, value2);
    }

    void uniform_set_float4(rhiProgram program, int uniform, float value1, float value2, float value3, float value4) override
    {
        glProgram* prog = (glProgram*)program;

        if (!prog
            || (uint32_t)uniform >= (uint32_t)prog->uniform_count
            || prog->uniforms[uniform].type != RHI_UNIFORM_FLOAT4)
            return;

        if (gl.currentShaderProgramID != prog->program)
            buildgl_useShaderProgram(prog->program);
        glUniform4f(prog->uniforms[uniform].handle, value1, value2, value3, value4);
    }

    void uniform_set_matrix(rhiProgram program, int uniform, float* value) override
    {
        glProgram* prog = (glProgram*)program;

        if (!prog
            || (uint32_t)uniform >= (uint32_t)prog->uniform_count
            || prog->uniforms[uniform].type != RHI_UNIFORM_MATRIX)
            return;

        if (gl.currentShaderProgramID != prog->program)
            buildgl_useShaderProgram(prog->program);
        glUniformMatrix4fv(prog->uniforms[uniform].handle, 1, GL_FALSE, value);
    }

    rhiTexture texture_create(rhiTextureCreateInfo* info) override
    {
        glTexture* tex = (glTexture*)Bcalloc(1, sizeof(glTexture));
        glGenTextures(1, &tex->texture);
        tex->info = *info;
        tex->sampler.mag_filter = RHI_SAMPLER_NEAREST;
        tex->sampler.min_filter = RHI_SAMPLER_NEAREST;
        tex->sampler.wrap_s = RHI_SAMPLER_REPEAT;
        tex->sampler.wrap_t = RHI_SAMPLER_REPEAT;
        tex->sampler.max_anisotropy = 1.f;
        if (!tex->texture)
        {
            Bfree(tex);
            return nullptr;
        }
        auto& format_map = gl_format_map[info->format];
        buildgl_bindTexture(GL_TEXTURE_2D, tex->texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, info->levels - 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
        int width = info->width;
        int height = info->height;
        for (int level = 0; level < info->levels; level++)
        {
            glTexImage2D(GL_TEXTURE_2D, level, format_map.internal_format, width,
                height, 0, format_map.format, format_map.type, nullptr);

            width = max(width >> 1, 1);
            height = max(height >> 1, 1);
        }

        return rhiTexture(tex);
    }

    void texture_update(rhiTexture texture, rhiTextureUploadInfo* info) override
    {
        if (!texture)
            return;

        glTexture* tex = (glTexture*)texture;
        buildgl_bindTexture(GL_TEXTURE_2D, tex->texture);
        auto& format_map = gl_format_map[tex->info.format];
        glTexSubImage2D(GL_TEXTURE_2D, info->level, info->x, info->y, info->width, info->height,
            format_map.format, format_map.type, info->data);
    }

    void texture_destroy(rhiTexture texture) override
    {
        if (!texture)
            return;

        glTexture* tex = (glTexture*)texture;
        glDeleteTextures(1, &tex->texture);
        Bfree(tex);
    }

    rhiSampler sampler_create(rhiSamplerCreateInfo* info) override
    {
        glSampler* sampler = (glSampler*)Bcalloc(1, sizeof(glSampler));
        sampler->info = *info;
        if (glinfo.samplerobjects)
        {
            glGenSamplers(1, &sampler->sampler);
            if (sampler->sampler)
            {
                glSamplerParameteri(sampler->sampler, GL_TEXTURE_MAG_FILTER, gl_sampler_filter_map[info->mag_filter]);
                glSamplerParameteri(sampler->sampler, GL_TEXTURE_MIN_FILTER, gl_sampler_filter_map[info->min_filter]);
                glSamplerParameteri(sampler->sampler, GL_TEXTURE_WRAP_S, gl_sampler_wrap_map[info->wrap_s]);
                glSamplerParameteri(sampler->sampler, GL_TEXTURE_WRAP_T, gl_sampler_wrap_map[info->wrap_t]);
                glSamplerParameterf(sampler->sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, info->max_anisotropy);
            }
        }

        return rhiSampler(sampler);
    }

    void sampler_destroy(rhiSampler sampler) override
    {
        if (!sampler)
            return;

        glSampler* sampl = (glSampler*)sampler;
        if (sampl->sampler)
            glDeleteSamplers(1, &sampl->sampler);
        Bfree(sampl);
    }

    void texunit_bind(int texunit, rhiTexture texture, rhiSampler sampler) override
    {
        if (!texture)
            return;

        GLint gltex = GL_TEXTURE0 + texunit;

        glTexture* tex = (glTexture*)texture;
        buildgl_activeTexture(gltex);
        buildgl_bindTexture(GL_TEXTURE_2D, tex->texture);

        glClientActiveTexture(gltex);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        if (sampler)
        {
            glSampler* samp = (glSampler*)sampler;
            if (glinfo.samplerobjects && r_usesamplerobjects)
            {
                glBindSampler(texunit, samp->sampler);
            }
            else
            {
                if (tex->sampler.mag_filter != samp->info.mag_filter)
                {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_sampler_filter_map[samp->info.mag_filter]);
                    tex->sampler.mag_filter = samp->info.mag_filter;
                }
                if (tex->sampler.min_filter != samp->info.min_filter)
                {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_sampler_filter_map[samp->info.min_filter]);
                    tex->sampler.min_filter = samp->info.min_filter;
                }
                if (tex->sampler.wrap_s != samp->info.wrap_s)
                {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_sampler_wrap_map[samp->info.wrap_s]);
                    tex->sampler.wrap_s = samp->info.wrap_s;
                }
                if (tex->sampler.wrap_t != samp->info.wrap_t)
                {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_sampler_wrap_map[samp->info.wrap_t]);
                    tex->sampler.wrap_t = samp->info.wrap_t;
                }
                if (tex->sampler.max_anisotropy != samp->info.max_anisotropy)
                {
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, samp->info.max_anisotropy);
                    tex->sampler.max_anisotropy = samp->info.max_anisotropy;
                }
            }
        }
        if (gltex != GL_TEXTURE0)
            buildgl_activeTexture(GL_TEXTURE0);
    }

    void texunit_unbind(int texunit) override
    {
        GLint gltex = GL_TEXTURE0 + texunit;
        buildgl_activeTexture(gltex);
        buildgl_bindTexture(GL_TEXTURE_2D, 0);

        glClientActiveTexture(gltex);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        if (gltex != GL_TEXTURE0)
            buildgl_activeTexture(GL_TEXTURE0);
    }

    void texunit_setscale(int texunit, float x, float y, float sx, float sy)
    {
        buildgl_activeTexture(GL_TEXTURE0 + texunit);
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glTranslatef(x, y, 1.0f);
        glScalef(sx, sy, 1.0f);
        glMatrixMode(GL_MODELVIEW);
    }

    rhiVertices buffer_vertices_alloc(rhiVerticesFormatInfo* info, void* data) override
    {
        glVerticesBuffer* vert = (glVerticesBuffer*)Bcalloc(1, sizeof(glVerticesBuffer));
        glGenBuffers(1, &vert->buffer);
        buildgl_bindBuffer(GL_ARRAY_BUFFER, vert->buffer);
        glBufferData(GL_ARRAY_BUFFER, info->count * info->stride, data, GL_STATIC_DRAW);
        vert->info = *info;
        return rhiVertices(vert);
    }

    rhiIndices buffer_indices_alloc(int count, unsigned int* data) override
    {
        glIndicesBuffer* indices = (glIndicesBuffer*)Bcalloc(1, sizeof(glIndicesBuffer));
        glGenBuffers(1, &indices->buffer);
        buildgl_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices->buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(GLuint), data, GL_STATIC_DRAW);
        indices->count = count;
        return rhiIndices(indices);
    }

    void buffer_vertices_destroy(rhiVertices vertices) override
    {
        if (!vertices)
            return;

        glVerticesBuffer* vert = (glVerticesBuffer*)vertices;
        glDeleteBuffers(1, &vert->buffer);
    }

    void buffer_indices_destroy(rhiIndices indices) override
    {
        if (!indices)
            return;

        glVerticesBuffer* indice = (glVerticesBuffer*)indices;
        glDeleteBuffers(1, &indice->buffer);
    }

    /////

    void program_bind(rhiProgram program) override
    {
        if (!program)
            return;

        glProgram* prog = (glProgram*)program;

        buildgl_useShaderProgram(prog->program);
    }

    GLint texture_handle(rhiTexture texture) override
    {
        if (!texture)
            return 0;

        glTexture* tex = (glTexture*)texture;
        return tex->texture;
    }
};

#if 0
struct vkRhi : rhiInterface
{
    rhiTexture texture_create() override
    {
        GLuint texture;
        glGenTextures(1, &texture);
        return rhiTexture(texture);
    }

    void texture_destroy(rhiTexture texture) override
    {
        GLuint tex = GLuint(texture);
        glDeleteTextures(1, &tex);
    }
};
#endif

static rhiState state;

void rhi_init()
{
    rhi_shutdown();
    if (usevulkan)
    {
        //rhi = new vkRhi();
    }
    else
    {
        rhi = new glRhi();
    }
    rhi->init();
}

void rhi_shutdown()
{
    delete rhi;
    rhi = nullptr;
}
