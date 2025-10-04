#include "baselayer.h"
#include "build.h"
#include "vk.h"
#include "vksurface.h"

static uint32_t vertex_spirv[] =
#include "vk_shaders/surface_vertex_spirv.h"
;

static uint32_t fragment_spirv[] =
#include "vk_shaders/surface_fragment_spirv.h"
;

static int buffer_size;
static void* buffer;
static vec2_t buffer_res;
static uint8_t cur_palette[1024];
static VmaImage frame_texture[VK_FRAMES_IN_FLIGHT];
static VmaImage palette_texture[VK_FRAMES_IN_FLIGHT];
static VmaBuffer staging_buffer[VK_FRAMES_IN_FLIGHT];
static void* staging_buffer_map[VK_FRAMES_IN_FLIGHT];
static uint64_t staging_buffer_size;
static uint64_t staging_frame_offset;
static uint64_t staging_pal_offset;

static VkShaderModule shader_vertex;
static VkShaderModule shader_fragment;
static VkDescriptorSetLayout set_layout;
static VkPipelineLayout pipeline_layout;
static VkPipeline pipeline;
static VkDescriptorSet descriptor_set[VK_FRAMES_IN_FLIGHT];
static VkDescriptorPool descriptor_pool;
static VkSampler sampler;

VkResult vksurface_initialize_vulkan()
{
    VkImageCreateInfo info_frame = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    info_frame.imageType = VK_IMAGE_TYPE_2D;
    info_frame.format = VK_FORMAT_R8_UINT;
    info_frame.extent.width = buffer_res.x;
    info_frame.extent.height = buffer_res.y;
    info_frame.extent.depth = 1;
    info_frame.mipLevels = 1;
    info_frame.arrayLayers = 1;
    info_frame.samples = VK_SAMPLE_COUNT_1_BIT;
    info_frame.tiling = VK_IMAGE_TILING_OPTIMAL;
    info_frame.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info_frame.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info_frame.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageCreateInfo info_palette = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    info_palette.imageType = VK_IMAGE_TYPE_2D;
    info_palette.format = VK_FORMAT_R8G8B8A8_UNORM;
    info_palette.extent.width = 256;
    info_palette.extent.height = 1;
    info_palette.extent.depth = 1;
    info_palette.mipLevels = 1;
    info_palette.arrayLayers = 1;
    info_palette.samples = VK_SAMPLE_COUNT_1_BIT;
    info_palette.tiling = VK_IMAGE_TILING_OPTIMAL;
    info_palette.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info_palette.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info_palette.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    uint64_t offset = 0;
    
    staging_frame_offset = 0;
    staging_pal_offset = staging_frame_offset + buffer_size;
    staging_buffer_size = staging_pal_offset + 1024;

    VkBufferCreateInfo info_buffer = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    info_buffer.size = staging_buffer_size;
    info_buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo info_vmabuffer = {};
    info_vmabuffer.usage = VMA_MEMORY_USAGE_AUTO;
    info_vmabuffer.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    for (uint32_t i = 0; i < VK_FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECKRESULT(frame_texture[i].allocate(&info_frame));
        VK_CHECKRESULT(palette_texture[i].allocate(&info_palette));
        VK_CHECKRESULT(staging_buffer[i].allocate(&info_buffer, &info_vmabuffer));

        staging_buffer_map[i] = staging_buffer[i].map();
    }

    VkShaderModuleCreateInfo info_vert = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info_vert.codeSize = sizeof(vertex_spirv);
    info_vert.pCode = vertex_spirv;
    VkShaderModuleCreateInfo info_frag = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info_frag.codeSize = sizeof(fragment_spirv);
    info_frag.pCode = fragment_spirv;

    VK_CHECKRESULT(vkCreateShaderModule(vk_device, &info_vert, nullptr, &shader_vertex));
    VK_CHECKRESULT(vkCreateShaderModule(vk_device, &info_frag, nullptr, &shader_fragment));

    VkDescriptorSetLayoutBinding binding[2] = {};
    binding[0].binding = 0;
    binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding[0].descriptorCount = 1;
    binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding[1].binding = 1;
    binding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding[1].descriptorCount = 1;
    binding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info_set_layout = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info_set_layout.bindingCount = 2;
    info_set_layout.pBindings = binding;

    VK_CHECKRESULT(vkCreateDescriptorSetLayout(vk_device, &info_set_layout, nullptr, &set_layout));

    VkPipelineLayoutCreateInfo info_pipeline_layout = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info_pipeline_layout.setLayoutCount = 1;
    info_pipeline_layout.pSetLayouts = &set_layout;

    VK_CHECKRESULT(vkCreatePipelineLayout(vk_device, &info_pipeline_layout, nullptr, &pipeline_layout));

    VkPipelineShaderStageCreateInfo info_stages[2] = {};
    info_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    info_stages[0].module = shader_vertex;
    info_stages[0].pName = "main";
    info_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    info_stages[1].module = shader_fragment;
    info_stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo info_vertex_input = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo info_input_assembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    info_input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    VkPipelineMultisampleStateCreateInfo info_ms_state = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    info_ms_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineRasterizationStateCreateInfo info_rasterization = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    info_rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    info_rasterization.cullMode = VK_CULL_MODE_NONE;
    info_rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
    info_rasterization.lineWidth = 1.f;
    VkViewport viewport = {};
    viewport.width = buffer_res.x;
    viewport.height = buffer_res.y;
    VkRect2D scissor = {};
    scissor.extent.width = buffer_res.x;
    scissor.extent.height = buffer_res.y;
    VkPipelineViewportStateCreateInfo info_viewport = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    info_viewport.viewportCount = 1;
    info_viewport.pViewports = &viewport;
    info_viewport.scissorCount = 1;
    info_viewport.pScissors = &scissor;
    VkPipelineDepthStencilStateCreateInfo info_depth = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    VkPipelineColorBlendAttachmentState attachment = {};
    attachment.blendEnable = 0;
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo info_blend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    info_blend.attachmentCount = 1;
    info_blend.pAttachments = &attachment;

    VkGraphicsPipelineCreateInfo info_pipeline = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    info_pipeline.stageCount = 2;
    info_pipeline.pStages = info_stages;
    info_pipeline.pVertexInputState = &info_vertex_input;
    info_pipeline.pInputAssemblyState = &info_input_assembly;
    info_pipeline.pMultisampleState = &info_ms_state;
    info_pipeline.pRasterizationState = &info_rasterization;
    info_pipeline.pViewportState = &info_viewport;
    info_pipeline.pDepthStencilState = &info_depth;
    info_pipeline.pColorBlendState = &info_blend;
    info_pipeline.layout = pipeline_layout;
    info_pipeline.renderPass = vk_renderpass;
    info_pipeline.subpass = 0;

    VK_CHECKRESULT(vkCreateGraphicsPipelines(vk_device, nullptr, 1, &info_pipeline, nullptr, &pipeline));

    VkDescriptorPoolSize pool_size = {};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 2 * VK_FRAMES_IN_FLIGHT;
    VkDescriptorPoolCreateInfo info_decriptor_pool = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    info_decriptor_pool.maxSets = VK_FRAMES_IN_FLIGHT;
    info_decriptor_pool.poolSizeCount = 1;
    info_decriptor_pool.pPoolSizes = &pool_size;

    VK_CHECKRESULT(vkCreateDescriptorPool(vk_device, &info_decriptor_pool, nullptr, &descriptor_pool));

    VkDescriptorSetLayout layouts[VK_FRAMES_IN_FLIGHT] = {};
    for (uint32_t i = 0; i < VK_FRAMES_IN_FLIGHT; i++)
    {
        layouts[i] = set_layout;
    }
    VkDescriptorSetAllocateInfo info_alloc = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    info_alloc.descriptorPool = descriptor_pool;
    info_alloc.descriptorSetCount = 2;
    info_alloc.pSetLayouts = layouts;

    VK_CHECKRESULT(vkAllocateDescriptorSets(vk_device, &info_alloc, descriptor_set));

    VkSamplerCreateInfo info_sampler = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    info_sampler.magFilter = VK_FILTER_NEAREST;
    info_sampler.minFilter = VK_FILTER_NEAREST;
    info_sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info_sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info_sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECKRESULT(vkCreateSampler(vk_device, &info_sampler, nullptr, &sampler));

    VkWriteDescriptorSet descriptor_write[VK_FRAMES_IN_FLIGHT * 2] = {};
    VkDescriptorImageInfo descriptor_image[VK_FRAMES_IN_FLIGHT * 2] = {};
    for (uint32_t i = 0; i < VK_FRAMES_IN_FLIGHT; i++)
    {
        descriptor_write[i * 2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write[i * 2].dstSet = descriptor_set[i];
        descriptor_write[i * 2].dstBinding = 0;
        descriptor_write[i * 2].descriptorCount = 1;
        descriptor_write[i * 2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write[i * 2].pImageInfo = &descriptor_image[i * 2];
        descriptor_image[i * 2].sampler = sampler;
        descriptor_image[i * 2].imageView = frame_texture[i].image_view;
        descriptor_image[i * 2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        descriptor_write[i * 2 + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write[i * 2 + 1].dstSet = descriptor_set[i];
        descriptor_write[i * 2 + 1].dstBinding = 1;
        descriptor_write[i * 2 + 1].descriptorCount = 1;
        descriptor_write[i * 2 + 1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write[i * 2 + 1].pImageInfo = &descriptor_image[i * 2 + 1];
        descriptor_image[i * 2 + 1].sampler = sampler;
        descriptor_image[i * 2 + 1].imageView = palette_texture[i].image_view;
        descriptor_image[i * 2 + 1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    vkUpdateDescriptorSets(vk_device, VK_FRAMES_IN_FLIGHT * 2, descriptor_write, 0, NULL);

    return VK_SUCCESS;
}

bool vksurface_initialize(vec2_t res)
{
    if (buffer)
        vksurface_destroy();

    buffer_res = res;
    buffer_size = buffer_res.x * buffer_res.y;

    buffer = Xmalloc(buffer_size);

    vksurface_setPalette(curpalettefaded);

    VkResult result = vksurface_initialize_vulkan();

    if (result != VK_SUCCESS)
    {
        vksurface_destroy();
        return false;
    }

    return true;
}

void vksurface_destroy()
{
    if (!buffer)
        return;

    DO_FREE_AND_NULL(buffer);

    vk_wait_idle();

    for (uint32_t i = 0; i < VK_FRAMES_IN_FLIGHT; i++)
    {
        frame_texture[i].destroy();
        palette_texture[i].destroy();
        staging_buffer[i].unmap();
        staging_buffer[i].destroy();
    }
}

void* vksurface_getBuffer()
{
    return buffer;
}

void vksurface_setPalette(void* pal)
{
    if (!buffer || !pal)
        return;

    Bmemcpy(cur_palette, pal, 1024);
}

void vksurface_blitBuffer()
{
    if (!buffer)
        return;

    vk_acquire_frame();

    uint8_t* dest = (uint8_t*)staging_buffer_map[vk_frame_id];
    Bmemcpy(dest + staging_frame_offset, buffer, buffer_size);
    Bmemcpy(dest + staging_pal_offset, cur_palette, 1024);

    VkBufferImageCopy frame_copy = {};
    frame_copy.bufferOffset = staging_frame_offset;
    frame_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    frame_copy.imageSubresource.mipLevel = 0;
    frame_copy.imageSubresource.baseArrayLayer = 0;
    frame_copy.imageSubresource.layerCount = 1;
    frame_copy.imageExtent.width = buffer_res.x;
    frame_copy.imageExtent.height = buffer_res.y;
    frame_copy.imageExtent.depth = 1;
    VkBufferImageCopy pal_copy = {};
    pal_copy.bufferOffset = staging_pal_offset;
    pal_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    pal_copy.imageSubresource.mipLevel = 0;
    pal_copy.imageSubresource.baseArrayLayer = 0;
    pal_copy.imageSubresource.layerCount = 1;
    pal_copy.imageExtent.width = 256;
    pal_copy.imageExtent.height = 1;
    pal_copy.imageExtent.depth = 1;

    frame_texture[vk_frame_id].to_layout(vk_cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);
    palette_texture[vk_frame_id].to_layout(vk_cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);

    vkCmdCopyBufferToImage(vk_cmd, staging_buffer[vk_frame_id].buffer,
        frame_texture[vk_frame_id].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &frame_copy);
    vkCmdCopyBufferToImage(vk_cmd, staging_buffer[vk_frame_id].buffer,
        palette_texture[vk_frame_id].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &pal_copy);

    frame_texture[vk_frame_id].to_layout(vk_cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);
    palette_texture[vk_frame_id].to_layout(vk_cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);

    vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layout, 0, 1, &descriptor_set[vk_frame_id], 0, nullptr);

    vk_begin_renderpass();

    vkCmdDraw(vk_cmd, 3, 1, 0, 0);

    vk_end_renderpass();
}

