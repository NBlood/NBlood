#include <vector>
#define VOLK_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>

#include "compat.h"
#include "log.h"
#include "vk.h"

static VmaAllocator vk_memmgr;

struct VmaImage
{
	VkImage image;
	VmaAllocation mem;

	VkResult allocate(VkImageCreateInfo* info, VmaAllocationCreateInfo* vma_info = nullptr)
	{
		VmaAllocationCreateInfo null_info;
		if (!vma_info)
		{
			null_info = {};
			null_info.usage = VMA_MEMORY_USAGE_AUTO;
			vma_info = &null_info;
		}
		return vmaCreateImage(vk_memmgr, info, vma_info, &image, &mem, nullptr);;
	}

	void destroy()
	{
		vmaDestroyImage(vk_memmgr, image, mem);
		image = nullptr;
		mem = nullptr;
	}
};

VkInstance vk_instance;
static bool volk_init;
static bool vk_validation = true;
static VkPhysicalDevice vk_physical_device;
static uint32_t vk_queue_family;
static VkDevice vk_device;
static VkQueue vk_queue;
static VkSurfaceKHR vk_surface;
static VkSurfaceCapabilitiesKHR vk_surf_caps;
static VkSwapchainKHR vk_swapchain;
static VkImage* vk_color_image;
static VkImageView* vk_color_image_view;
static VmaImage vk_depth_image;
static VkImageView vk_depth_image_view;
static uint32_t vk_swapchain_image_cnt;
static VkFormat vk_swapchain_format;

#define VK_CHECKRESULT(x) {VkResult result = (x); if (result < 0) return result; }

bool vk_check_layer(uint32_t layers_count, VkLayerProperties* layers, const char* layer)
{
	for (uint32_t i = 0; i < layers_count; i++)
	{
		if (!Bstrcmp(layers[i].layerName, layer))
		{
			return true;
		}
	}
	return false;
}

bool vk_check_extension(uint32_t extensions_count, VkExtensionProperties* extensions, const char* extension)
{
	for (uint32_t i = 0; i < extensions_count; i++)
	{
		if (!Bstrcmp(extensions[i].extensionName, extension))
		{
			return true;
		}
	}
	return false;
}

static VkBool32 vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data)
{
	if (callback_data && callback_data->pMessage)
		LOG_F(ERROR, "Vulkan: %s\n", callback_data->pMessage);

	return 1;
}

VkResult vk_initialize_instance(uint32_t required_ext_num, const char** required_ext, PFN_vkGetInstanceProcAddr proc)
{
	if (vk_instance && vk_device)
	{
		vkQueueWaitIdle(vk_queue);
		return VK_SUCCESS;
	}

	if (!proc)
		return VK_ERROR_UNKNOWN;

	if (!volk_init)
	{
		volkInitializeCustom(proc);
		volk_init = true;
	}

	uint32_t sup_layers_count = 0;
	vkEnumerateInstanceLayerProperties(&sup_layers_count, nullptr);
	VkLayerProperties* sup_layers = (VkLayerProperties*)Bmalloc(sizeof(VkLayerProperties) * sup_layers_count);
	vkEnumerateInstanceLayerProperties(&sup_layers_count, sup_layers);

	uint32_t sup_extensions_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &sup_extensions_count, nullptr);
	VkExtensionProperties* sup_extensions = (VkExtensionProperties*)Bmalloc(sizeof(VkExtensionProperties) * sup_extensions_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &sup_extensions_count, sup_extensions);

	for (uint32_t i = 0; i < required_ext_num; i++)
	{
		if (!vk_check_extension(sup_extensions_count, sup_extensions, required_ext[i]))
		{
			Bfree(sup_layers);
			Bfree(sup_extensions);
			vk_shutdown_instance();
			return VK_ERROR_UNKNOWN;
		}
	}

	{
		VkApplicationInfo app_info{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		VkInstanceCreateInfo info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };

		std::vector<const char*> layers;
		std::vector<const char*> extensions;

		if (vk_validation && vk_check_layer(sup_layers_count, sup_layers, "VK_LAYER_KHRONOS_validation"))
		{
			layers.push_back("VK_LAYER_KHRONOS_validation");
		}
		else
			vk_validation = false;

		for (uint32_t i = 0; i < required_ext_num; i++)
		{
			extensions.push_back(required_ext[i]);
		}
		VkDebugUtilsMessengerCreateInfoEXT info_debug{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		if (vk_validation)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			info_debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT;
			info_debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_FLAG_BITS_MAX_ENUM_EXT;
			info_debug.pfnUserCallback = vk_debug_callback;
		}

		app_info.apiVersion = VK_VERSION_1_0;

		info.pApplicationInfo = &app_info;
		info.enabledLayerCount = layers.size();
		info.ppEnabledLayerNames = layers.data();
		info.enabledExtensionCount = extensions.size();
		info.ppEnabledExtensionNames = extensions.data();

		VK_CHECKRESULT(vkCreateInstance(&info, nullptr, &vk_instance));

		volkLoadInstance(vk_instance);
	}

	Bfree(sup_layers);
	Bfree(sup_extensions);

	{
		uint32_t dev_count;
		vkEnumeratePhysicalDevices(vk_instance, &dev_count, nullptr);
		VkPhysicalDevice* dev = (VkPhysicalDevice*)Bmalloc(sizeof(VkPhysicalDevice) * dev_count);
		vkEnumeratePhysicalDevices(vk_instance, &dev_count, dev);

		vk_physical_device = VK_NULL_HANDLE;
		vk_queue_family = 0;
		uint32_t best_score = 0;

		for (uint32_t i = 0; i < dev_count; i++)
		{
			uint32_t queue_prop_cnt = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(dev[i], &queue_prop_cnt, nullptr);
			VkQueueFamilyProperties* queue_prop = (VkQueueFamilyProperties*)Bmalloc(sizeof(VkQueueFamilyProperties) * queue_prop_cnt);
			vkGetPhysicalDeviceQueueFamilyProperties(dev[i], &queue_prop_cnt, queue_prop);

			uint32_t queue_family = UINT32_MAX;

			for (uint32_t j = 0; j < queue_prop_cnt; j++)
			{
				if ((queue_prop[j].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
					== (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
					queue_family = j;
			}

			Bfree(queue_prop);

			if (queue_family == UINT32_MAX)
				continue;

			uint32_t dev_ext_count = 0;
			vkEnumerateDeviceExtensionProperties(dev[i], nullptr, &dev_ext_count, nullptr);
			VkExtensionProperties* dev_extensions = (VkExtensionProperties*)Bmalloc(sizeof(VkExtensionProperties) * dev_ext_count);
			vkEnumerateDeviceExtensionProperties(dev[i], nullptr, &dev_ext_count, dev_extensions);

			bool ext_support = vk_check_extension(dev_ext_count, dev_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			
			Bfree(dev_extensions);
			if (!ext_support)
				continue;

			VkPhysicalDeviceProperties props{};
			vkGetPhysicalDeviceProperties(dev[i], &props);
			uint32_t score = 0;

			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				score += 0x80000000;
			}
			else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
			{
				score += 0x40000000;
			}

			VkPhysicalDeviceMemoryProperties mem_props{};
			vkGetPhysicalDeviceMemoryProperties(dev[i], &mem_props);
			for (uint32_t j = 0; j < mem_props.memoryHeapCount; j++)
			{
				if (mem_props.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
				{
					score += mem_props.memoryHeaps[j].size >> 10;
				}
			}
			if (score > best_score)
			{
				best_score = score;
				vk_physical_device = dev[i];
				vk_queue_family = queue_family;
			}
		}
		Bfree(dev);

		if (!vk_physical_device)
		{
			vk_shutdown_instance();
			return VK_ERROR_UNKNOWN;
		}
	}

	{
		std::vector<const char*> extensions;

		extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		VkDeviceQueueCreateInfo queue_info{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		float queue_priority = 1.f;
		queue_info.queueFamilyIndex = vk_queue_family;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &queue_priority;

		VkDeviceCreateInfo info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		info.queueCreateInfoCount = 1;
		info.pQueueCreateInfos = &queue_info;

		info.enabledExtensionCount = extensions.size();
		info.ppEnabledExtensionNames = extensions.data();

		VK_CHECKRESULT(vkCreateDevice(vk_physical_device, &info, nullptr, &vk_device));
		
		vkGetDeviceQueue(vk_device, vk_queue_family, 0, &vk_queue);
	}

	{
		VmaVulkanFunctions vulkanFunctions = {};
		vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo info = {};
		info.vulkanApiVersion = VK_API_VERSION_1_0;
		info.physicalDevice = vk_physical_device;
		info.device = vk_device;
		info.instance = vk_instance;
		info.pVulkanFunctions = &vulkanFunctions;
		VK_CHECKRESULT(vmaCreateAllocator(&info, &vk_memmgr));
	}

	return VK_SUCCESS;
}

void vk_shutdown_instance()
{
	if (vk_device)
	{
		vkDeviceWaitIdle(vk_device);

		vk_destroy_swapchain();

		if (vk_memmgr)
			vmaDestroyAllocator(vk_memmgr);

		vkDestroyDevice(vk_device, nullptr);
	}

	vk_device = nullptr;

	if (vk_instance)
	{
		vkDestroyInstance(vk_instance, nullptr);

		vk_instance = nullptr;
	}
}

VkResult vk_initialize_swapchain(VkSurfaceKHR surface, uint32_t width, uint32_t height, uint32_t vsync)
{
	if (!vk_device)
		return VK_ERROR_UNKNOWN;

	if (vk_swapchain)
	{
		vk_destroy_swapchain();
	}

	vk_surface = surface;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, surface, &vk_surf_caps);

	uint32_t surface_formats_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, surface, &surface_formats_count, nullptr);
	VkSurfaceFormatKHR* surface_formats = (VkSurfaceFormatKHR*)Bmalloc(sizeof(VkSurfaceFormatKHR) * surface_formats_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, surface, &surface_formats_count, surface_formats);

	VkFormat format = VK_FORMAT_UNDEFINED;

	for (uint32_t i = 0; i < surface_formats_count; i++)
	{
		switch (surface_formats[i].format)
		{
			case VK_FORMAT_B8G8R8A8_UNORM:
			case VK_FORMAT_R8G8B8A8_UNORM:
				format = surface_formats[i].format;
				break;
		}
		if (format != VK_FORMAT_UNDEFINED)
			break;
	}
	Bfree(surface_formats);
	if (format == VK_FORMAT_UNDEFINED)
	{
		return VK_ERROR_UNKNOWN;
	}

	VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };

	uint32_t imageCnt = 3;
	if (imageCnt < vk_surf_caps.minImageCount)
		imageCnt = vk_surf_caps.minImageCount;
	if (imageCnt > vk_surf_caps.maxImageCount)
		imageCnt = vk_surf_caps.maxImageCount;

	info.surface = surface;
	info.minImageCount = imageCnt;
	info.imageFormat = format;
	info.imageExtent.height = height;
	info.imageExtent.width = width;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = vk_surf_caps.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	switch (vsync)
	{
		case -1: // adaptive:
			info.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			break;
		case 0: // disabled:
		default:
			info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			break;
		case 1:
		case 2:
			info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
	}
	info.clipped = VK_TRUE;

	VK_CHECKRESULT(vkCreateSwapchainKHR(vk_device, &info, nullptr, &vk_swapchain));

	vk_swapchain_format = format;

	vk_swapchain_image_cnt = 0;
	vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &vk_swapchain_image_cnt, nullptr);
	vk_color_image = (VkImage*)Bmalloc(sizeof(VkImage) * vk_swapchain_image_cnt);
	vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &vk_swapchain_image_cnt, vk_color_image);

	vk_color_image_view = (VkImageView*)Bmalloc(sizeof(VkImageView) * vk_swapchain_image_cnt);

	for (uint32_t i = 0; i < vk_swapchain_image_cnt; i++)
	{
		VkImageViewCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		info.image = vk_color_image[i];
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.format = format;
		info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;

		VK_CHECKRESULT(vkCreateImageView(vk_device, &info, nullptr, &vk_color_image_view[i]));
	}


	{
		VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = VK_FORMAT_D24_UNORM_S8_UINT;
		info.extent.height = height;
		info.extent.width = width;
		info.extent.depth = 1;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECKRESULT(vk_depth_image.allocate(&info));

		VkImageViewCreateInfo info_view = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		info_view.image = vk_depth_image.image;
		info_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info_view.format = format;
		info_view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		info_view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		info_view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		info_view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		info_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		info_view.subresourceRange.baseMipLevel = 0;
		info_view.subresourceRange.levelCount = 1;
		info_view.subresourceRange.baseArrayLayer = 0;
		info_view.subresourceRange.layerCount = 1;

		VK_CHECKRESULT(vkCreateImageView(vk_device, &info_view, nullptr, &vk_depth_image_view));
	}

	return VK_SUCCESS;
}

void vk_destroy_swapchain()
{
	if (!vk_swapchain)
		return;
	vkQueueWaitIdle(vk_queue);

	vkDestroyImageView(vk_device, vk_depth_image_view, nullptr);
	vk_depth_image.destroy();

	for (uint32_t i = 0; i < vk_swapchain_image_cnt; i++)
		vkDestroyImageView(vk_device, vk_color_image_view[i], nullptr);

	vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
	vk_swapchain = nullptr;
}
