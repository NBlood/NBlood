#pragma once

#include <volk/volk.h>

extern VkInstance vk_instance;
extern VkSurfaceKHR vk_surface;

VkResult vk_initialize(unsigned int required_ext_num, const char** required_ext, PFN_vkGetInstanceProcAddr proc);

