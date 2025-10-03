#include "baselayer.h"
#include "build.h"
#include "vk.h"
#include "vksurface.h"

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

bool vksurface_initialize(vec2_t res)
{
    if (buffer)
        vksurface_destroy();

    buffer_res = res;
    buffer_size = buffer_res.x * buffer_res.y;

    buffer = Xmalloc(buffer_size);

    vksurface_setPalette(curpalettefaded);

    VkImageCreateInfo info_frame = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    info_frame.imageType = VK_IMAGE_TYPE_2D;
    info_frame.format = VK_FORMAT_R8_UINT;
    info_frame.extent.width = res.x;
    info_frame.extent.height = res.y;
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
        frame_texture[i].allocate(&info_frame);
        palette_texture[i].allocate(&info_palette);
        staging_buffer[i].allocate(&info_buffer, &info_vmabuffer);

        staging_buffer_map[i] = staging_buffer[i].map();
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

    vk_ensure_rendering();

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
}
