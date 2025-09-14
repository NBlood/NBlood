#pragma once

#include <volk/volk.h>

extern VkInstance vk_instance;

VkResult vk_initialize_instance(uint32_t required_ext_num, const char** required_ext, PFN_vkGetInstanceProcAddr proc);
void vk_shutdown_instance();
VkResult vk_initialize_swapchain(VkSurfaceKHR surface, uint32_t width, uint32_t height, uint32_t vsync);
void vk_destroy_swapchain();
