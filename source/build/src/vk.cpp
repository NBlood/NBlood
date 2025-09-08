#include <vector>
#define VOLK_IMPLEMENTATION
#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>

#include "compat.h"
#include "log.h"
#include "vk.h"

VkInstance vk_instance;
static uint32_t vk_layers_count;
static VkLayerProperties* vk_layers;
static uint32_t vk_extensions_count;
static VkExtensionProperties* vk_extensions;
static bool volk_init;
static bool vk_validation = true;
static VkPhysicalDevice vk_physical_device;
static uint32_t vk_queue_family;
static VkDevice vk_device;
static VkQueue vk_queue;
VkSurfaceKHR vk_surface;

#define VK_CHECKRESULT(x) {VkResult result = (x); if (result < 0) return result; }

bool vk_check_layer(const char* layer)
{
	for (uint32_t i = 0; i < vk_layers_count; i++)
	{
		if (!Bstrcmp(vk_layers[i].layerName, layer))
		{
			return true;
		}
	}
	return false;
}

bool vk_check_extension(const char* extension)
{
	for (uint32_t i = 0; i < vk_extensions_count; i++)
	{
		if (!Bstrcmp(vk_extensions[i].extensionName, extension))
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

VkResult vk_initialize(unsigned int required_ext_num, const char** required_ext, PFN_vkGetInstanceProcAddr proc)
{
	if (!proc)
		return VK_ERROR_UNKNOWN;

	if (!volk_init)
	{
		volkInitializeCustom(proc);
		volk_init = true;
	}

	vkEnumerateInstanceLayerProperties(&vk_layers_count, nullptr);
	vk_layers = (VkLayerProperties*)Bmalloc(sizeof(VkLayerProperties) * vk_layers_count);
	vkEnumerateInstanceLayerProperties(&vk_layers_count, vk_layers);

	vkEnumerateInstanceExtensionProperties(nullptr, &vk_extensions_count, nullptr);
	vk_extensions = (VkExtensionProperties*)Bmalloc(sizeof(VkExtensionProperties) * vk_extensions_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &vk_extensions_count, vk_extensions);

	{
		VkApplicationInfo app_info{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		VkInstanceCreateInfo info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };

		std::vector<const char*> layers;
		std::vector<const char*> extensions;

		if (vk_validation && vk_check_layer("VK_LAYER_KHRONOS_validation"))
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
			uint32_t queue_prop_cnt;
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
			return VK_ERROR_UNKNOWN;
	}

	{
		VkDeviceQueueCreateInfo queue_info{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		float queue_priority = 1.f;
		queue_info.queueFamilyIndex = vk_queue_family;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &queue_priority;

		VkDeviceCreateInfo info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		info.queueCreateInfoCount = 1;
		info.pQueueCreateInfos = &queue_info;

		VK_CHECKRESULT(vkCreateDevice(vk_physical_device, &info, nullptr, &vk_device));
		
		vkGetDeviceQueue(vk_device, vk_queue_family, 0, &vk_queue);
	}

	return VK_SUCCESS;
}

void vk_shutdown()
{
	if (!volk_init)
		return;

	vkDestroyDevice(vk_device, nullptr);

	vkDestroyInstance(vk_instance, nullptr);

	if (vk_layers_count)
	{
		vk_layers_count = 0;
		Bfree(vk_layers);
	}

	if (vk_extensions_count)
	{
		vk_extensions_count = 0;
		Bfree(vk_extensions);
	}

	vkDestroyInstance(vk_instance, NULL);
}
