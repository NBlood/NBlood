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

	void init() override
	{
		buildgl_resetStateAccounting();
		maxTextureSize = glinfo.maxTextureSize;

#if (defined _MSC_VER) || (!defined BITNESS64)
		if (maxTextureSize > 8192)
			maxTextureSize = 8192;
#endif
		bgra = glinfo.bgra;
	}

	void reset() override
	{
		buildgl_resetStateAccounting();
        buildgl_activeTexture(GL_TEXTURE0);
	}

	rhiTexture texture_create(rhiTextureCreateInfo* info) override
	{
		glTexture* tex = (glTexture*)Bcalloc(1, sizeof(glTexture));
		glGenTextures(1, &tex->texture);
		tex->info = *info;
		tex->sampler.mag_filter = GL_NEAREST;
		tex->sampler.min_filter = GL_NEAREST;
		tex->sampler.wrap_s = GL_REPEAT;
		tex->sampler.wrap_t = GL_REPEAT;
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
		glTexImage2D(GL_TEXTURE_2D, 0, format_map.internal_format, info->width,
			info->height, 0, format_map.format, format_map.type, nullptr);

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

	void texture_bind(rhiTexture texture) override
	{
		if (!texture)
			return;

		glTexture* tex = (glTexture*)texture;
		buildgl_bindTexture(GL_TEXTURE_2D, tex->texture);
	}
	virtual GLint texture_handle(rhiTexture texture) override
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
