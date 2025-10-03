#pragma once

#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>

#define VK_CHECKRESULT(x) {VkResult result = (x); if (result < 0) return result; }
#define VK_FRAMES_IN_FLIGHT 2

struct VmaImage
{
	VkImage image;
	VkImageCreateInfo info;
	VmaAllocation mem;
	VkImageLayout layout;
	VkPipelineStageFlags stage;
	VkAccessFlagBits access;

	VkResult allocate(VkImageCreateInfo* info, VmaAllocationCreateInfo* vma_info = nullptr);
	void destroy();
	void to_layout(VkCommandBuffer vk_cmd, VkImageLayout dst_layout, VkPipelineStageFlags dst_stage,
		VkAccessFlagBits dst_access);
};

struct VmaBuffer
{
	VkBuffer buffer;
	VmaAllocation mem;

	VkResult allocate(VkBufferCreateInfo* info, VmaAllocationCreateInfo* vma_info = nullptr);
	void destroy();
	void* map();
	void unmap();
};

VkResult vk_initialize_instance(uint32_t required_ext_num, const char** required_ext, PFN_vkGetInstanceProcAddr proc);
void vk_shutdown_instance();
VkResult vk_initialize_swapchain(VkSurfaceKHR surface, uint32_t width, uint32_t height, uint32_t vsync);
void vk_destroy_swapchain();
void vk_ensure_rendering();
void vk_next_page();
void vk_wait_idle();

extern VkInstance vk_instance;
extern uint32_t vk_frame_id;
extern VkCommandBuffer vk_cmd;
