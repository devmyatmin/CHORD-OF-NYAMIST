#include "nyamist/discovery/android_surface_swapchain_validation.hpp"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <vector>

namespace {

using nyamist::discovery::AndroidSurfaceSwapchainValidationReport;

struct VulkanDispatch {
    void* library{};
    PFN_vkGetInstanceProcAddr get_instance_proc_addr{};

    PFN_vkEnumerateInstanceExtensionProperties enumerate_instance_extensions{};
    PFN_vkCreateInstance create_instance{};

    PFN_vkDestroyInstance destroy_instance{};
    PFN_vkEnumeratePhysicalDevices enumerate_physical_devices{};
    PFN_vkGetPhysicalDeviceProperties get_physical_device_properties{};
    PFN_vkGetPhysicalDeviceQueueFamilyProperties get_queue_families{};
    PFN_vkEnumerateDeviceExtensionProperties enumerate_device_extensions{};
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR get_surface_support{};
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR get_surface_capabilities{};
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR get_surface_formats{};
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR get_present_modes{};
    PFN_vkCreateAndroidSurfaceKHR create_android_surface{};
    PFN_vkDestroySurfaceKHR destroy_surface{};
    PFN_vkCreateDevice create_device{};
    PFN_vkGetDeviceProcAddr get_device_proc_addr{};

    PFN_vkDestroyDevice destroy_device{};
    PFN_vkGetDeviceQueue get_device_queue{};
    PFN_vkCreateSwapchainKHR create_swapchain{};
    PFN_vkDestroySwapchainKHR destroy_swapchain{};
    PFN_vkGetSwapchainImagesKHR get_swapchain_images{};
    PFN_vkCreateSemaphore create_semaphore{};
    PFN_vkDestroySemaphore destroy_semaphore{};
    PFN_vkAcquireNextImageKHR acquire_next_image{};
    PFN_vkQueuePresentKHR queue_present{};
    PFN_vkQueueWaitIdle queue_wait_idle{};
    PFN_vkDeviceWaitIdle device_wait_idle{};
    PFN_vkCreateCommandPool create_command_pool{};
    PFN_vkDestroyCommandPool destroy_command_pool{};
    PFN_vkAllocateCommandBuffers allocate_command_buffers{};
    PFN_vkFreeCommandBuffers free_command_buffers{};
    PFN_vkBeginCommandBuffer begin_command_buffer{};
    PFN_vkEndCommandBuffer end_command_buffer{};
    PFN_vkCmdPipelineBarrier cmd_pipeline_barrier{};
    PFN_vkQueueSubmit queue_submit{};
};

bool isUsable(VkResult result) {
    return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
}

std::string resultName(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    default: return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
    }
}

void fail(AndroidSurfaceSwapchainValidationReport& report, const std::string& reason) {
    if (report.failure_reason.empty()) report.failure_reason = reason;
    report.status = "failed";
}

bool hasInstanceExtension(const std::vector<VkExtensionProperties>& extensions,
                          const char* name) {
    return std::any_of(extensions.begin(), extensions.end(),
        [name](const VkExtensionProperties& item) {
            return std::strcmp(item.extensionName, name) == 0;
        });
}

bool hasDeviceExtension(const std::vector<VkExtensionProperties>& extensions,
                        const char* name) {
    return hasInstanceExtension(extensions, name);
}

std::string versionString(std::uint32_t version) {
    return std::to_string(VK_VERSION_MAJOR(version)) + "." +
           std::to_string(VK_VERSION_MINOR(version)) + "." +
           std::to_string(VK_VERSION_PATCH(version));
}

std::string formatName(VkFormat format) {
    switch (format) {
    case VK_FORMAT_B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
    case VK_FORMAT_R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
    case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
    case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
    default: return "VkFormat(" + std::to_string(static_cast<int>(format)) + ")";
    }
}

std::string colorSpaceName(VkColorSpaceKHR color_space) {
    if (color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return "SRGB_NONLINEAR";
    return "VkColorSpaceKHR(" +
           std::to_string(static_cast<int>(color_space)) + ")";
}

AndroidSurfaceSwapchainValidationReport runValidation(
        JNIEnv* env, jobject java_surface, jint requested_width, jint requested_height) {
    AndroidSurfaceSwapchainValidationReport report;
    VulkanDispatch vk;
    ANativeWindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
    VkSemaphore present_semaphore = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    if (java_surface == nullptr || requested_width <= 0 || requested_height <= 0) {
        report.native_window_status = "invalid_input";
        fail(report, "A valid Android Surface and positive dimensions are required");
    } else {
        window = ANativeWindow_fromSurface(env, java_surface);
        report.native_window_status = window != nullptr ? "success" : "failed";
        if (window == nullptr) fail(report, "ANativeWindow_fromSurface returned null");
    }

    if (report.failure_reason.empty()) {
        vk.library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        if (vk.library == nullptr) {
            report.loader_status = "failed";
            fail(report, "Could not load the Android public Vulkan loader libvulkan.so");
        } else {
            report.loader_library = "libvulkan.so";
            vk.get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                dlsym(vk.library, "vkGetInstanceProcAddr"));
            report.loader_status = vk.get_instance_proc_addr != nullptr ? "success" : "failed";
            if (vk.get_instance_proc_addr == nullptr) {
                fail(report, "vkGetInstanceProcAddr was not exported by libvulkan.so");
            }
        }
    }

    if (report.failure_reason.empty()) {
#define LOAD_GLOBAL(member, symbol) \
        vk.member = reinterpret_cast<decltype(vk.member)>( \
            vk.get_instance_proc_addr(VK_NULL_HANDLE, symbol))
        LOAD_GLOBAL(enumerate_instance_extensions, "vkEnumerateInstanceExtensionProperties");
        LOAD_GLOBAL(create_instance, "vkCreateInstance");
#undef LOAD_GLOBAL
        if (vk.enumerate_instance_extensions == nullptr || vk.create_instance == nullptr) {
            fail(report, "Required Vulkan global entry points are unavailable");
        }
    }

    if (report.failure_reason.empty()) {
        std::uint32_t extension_count = 0U;
        VkResult result = vk.enumerate_instance_extensions(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions;
        if (result == VK_SUCCESS && extension_count > 0U) {
            extensions.resize(extension_count);
            result = vk.enumerate_instance_extensions(nullptr, &extension_count, extensions.data());
        }
        if (result != VK_SUCCESS) {
            fail(report, "Instance extension enumeration failed: " + resultName(result));
        } else {
            report.surface_extension_status =
                hasInstanceExtension(extensions, VK_KHR_SURFACE_EXTENSION_NAME) ? "success" : "missing";
            report.android_surface_extension_status =
                hasInstanceExtension(extensions, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) ? "success" : "missing";
            if (report.surface_extension_status != "success" ||
                report.android_surface_extension_status != "success") {
                fail(report, "Required VK_KHR_surface or VK_KHR_android_surface extension is missing");
            }
        }
    }

    if (report.failure_reason.empty()) {
        const char* instance_extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
        };
        VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app_info.pApplicationName = "NYAMIST G925 R1.5";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 5, 0);
        app_info.pEngineName = "CHORD OF NYAMIST";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        create_info.pApplicationInfo = &app_info;
        create_info.enabledExtensionCount = 2U;
        create_info.ppEnabledExtensionNames = instance_extensions;

        const VkResult result = vk.create_instance(&create_info, nullptr, &instance);
        report.instance_create_status = isUsable(result) ? "success" : "failed";
        if (!isUsable(result)) fail(report, "vkCreateInstance failed: " + resultName(result));
    }

    if (instance != VK_NULL_HANDLE) {
#define LOAD_INSTANCE(member, symbol) \
        vk.member = reinterpret_cast<decltype(vk.member)>(vk.get_instance_proc_addr(instance, symbol))
        LOAD_INSTANCE(destroy_instance, "vkDestroyInstance");
        LOAD_INSTANCE(enumerate_physical_devices, "vkEnumeratePhysicalDevices");
        LOAD_INSTANCE(get_physical_device_properties, "vkGetPhysicalDeviceProperties");
        LOAD_INSTANCE(get_queue_families, "vkGetPhysicalDeviceQueueFamilyProperties");
        LOAD_INSTANCE(enumerate_device_extensions, "vkEnumerateDeviceExtensionProperties");
        LOAD_INSTANCE(get_surface_support, "vkGetPhysicalDeviceSurfaceSupportKHR");
        LOAD_INSTANCE(get_surface_capabilities, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        LOAD_INSTANCE(get_surface_formats, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        LOAD_INSTANCE(get_present_modes, "vkGetPhysicalDeviceSurfacePresentModesKHR");
        LOAD_INSTANCE(create_android_surface, "vkCreateAndroidSurfaceKHR");
        LOAD_INSTANCE(destroy_surface, "vkDestroySurfaceKHR");
        LOAD_INSTANCE(create_device, "vkCreateDevice");
        LOAD_INSTANCE(get_device_proc_addr, "vkGetDeviceProcAddr");
#undef LOAD_INSTANCE
    }

    if (report.failure_reason.empty() && instance != VK_NULL_HANDLE) {
        if (vk.create_android_surface == nullptr || vk.destroy_surface == nullptr ||
            vk.enumerate_physical_devices == nullptr || vk.get_queue_families == nullptr ||
            vk.get_surface_support == nullptr || vk.get_surface_capabilities == nullptr ||
            vk.get_surface_formats == nullptr || vk.get_present_modes == nullptr ||
            vk.enumerate_device_extensions == nullptr || vk.create_device == nullptr ||
            vk.get_device_proc_addr == nullptr) {
            fail(report, "One or more required Vulkan instance entry points are unavailable");
        } else {
            VkAndroidSurfaceCreateInfoKHR surface_info{
                VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
            surface_info.window = window;
            const VkResult result = vk.create_android_surface(instance, &surface_info,
                                                               nullptr, &surface);
            report.surface_create_status = isUsable(result) ? "success" : "failed";
            if (!isUsable(result)) fail(report, "vkCreateAndroidSurfaceKHR failed: " + resultName(result));
        }
    }

    std::uint32_t selected_queue_family = 0U;
    VkPhysicalDevice selected_physical_device = VK_NULL_HANDLE;
    if (report.failure_reason.empty() && surface != VK_NULL_HANDLE) {
        std::uint32_t physical_count = 0U;
        VkResult result = vk.enumerate_physical_devices(instance, &physical_count, nullptr);
        std::vector<VkPhysicalDevice> physical_devices;
        if (result == VK_SUCCESS && physical_count > 0U) {
            physical_devices.resize(physical_count);
            result = vk.enumerate_physical_devices(instance, &physical_count, physical_devices.data());
        }
        if (result != VK_SUCCESS || physical_devices.empty()) {
            fail(report, "No Vulkan physical device was enumerated: " + resultName(result));
        } else {
            for (const VkPhysicalDevice physical : physical_devices) {
                std::uint32_t device_extension_count = 0U;
                VkResult ext_result = vk.enumerate_device_extensions(
                    physical, nullptr, &device_extension_count, nullptr);
                std::vector<VkExtensionProperties> device_extensions;
                if (ext_result == VK_SUCCESS && device_extension_count > 0U) {
                    device_extensions.resize(device_extension_count);
                    ext_result = vk.enumerate_device_extensions(
                        physical, nullptr, &device_extension_count, device_extensions.data());
                }
                if (ext_result != VK_SUCCESS ||
                    !hasDeviceExtension(device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                    continue;
                }

                std::uint32_t family_count = 0U;
                vk.get_queue_families(physical, &family_count, nullptr);
                std::vector<VkQueueFamilyProperties> families(family_count);
                if (family_count == 0U) continue;
                vk.get_queue_families(physical, &family_count, families.data());

                for (std::uint32_t index = 0U; index < family_count; ++index) {
                    if (families[index].queueCount == 0U ||
                        (families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0U) continue;
                    VkBool32 present_supported = VK_FALSE;
                    const VkResult support_result = vk.get_surface_support(
                        physical, index, surface, &present_supported);
                    if (support_result == VK_SUCCESS && present_supported == VK_TRUE) {
                        selected_physical_device = physical;
                        selected_queue_family = index;
                        break;
                    }
                }
                if (selected_physical_device != VK_NULL_HANDLE) break;
            }

            if (selected_physical_device == VK_NULL_HANDLE) {
                report.swapchain_extension_status = "missing_or_unsupported";
                report.queue_family_status = "missing_graphics_present_queue";
                fail(report, "No physical device exposes VK_KHR_swapchain and a graphics+present queue for this Android Surface");
            } else {
                report.physical_device_status = "success";
                report.swapchain_extension_status = "success";
                report.queue_family_status = "success";
                report.queue_family_index = selected_queue_family;
                report.graphics_queue_supported = true;
                report.present_queue_supported = true;
                VkPhysicalDeviceProperties properties{};
                vk.get_physical_device_properties(selected_physical_device, &properties);
                report.device_name = properties.deviceName;
                report.vendor_id_hex = "0x" + [] (std::uint32_t value) {
                    constexpr char digits[] = "0123456789abcdef";
                    std::string out;
                    do { out.insert(out.begin(), digits[value & 0xfU]); value >>= 4U; } while (value);
                    return out;
                }(properties.vendorID);
                report.device_id_hex = "0x" + [] (std::uint32_t value) {
                    constexpr char digits[] = "0123456789abcdef";
                    std::string out;
                    do { out.insert(out.begin(), digits[value & 0xfU]); value >>= 4U; } while (value);
                    return out;
                }(properties.deviceID);
                report.physical_device_api_version = versionString(properties.apiVersion);
                const std::string gpu_name = report.device_name;
                report.target_gpu_match = gpu_name.find("G925") != std::string::npos ||
                                          gpu_name.find("g925") != std::string::npos;
                if (!report.target_gpu_match) {
                    fail(report, "The selected physical device is not identified as Immortalis-G925");
                }
            }
        }
    }

    if (report.failure_reason.empty() && selected_physical_device != VK_NULL_HANDLE) {
        const float priority = 1.0F;
        VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.queueFamilyIndex = selected_queue_family;
        queue_info.queueCount = 1U;
        queue_info.pQueuePriorities = &priority;
        const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device_info.queueCreateInfoCount = 1U;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = 1U;
        device_info.ppEnabledExtensionNames = device_extensions;

        const VkResult result = vk.create_device(selected_physical_device, &device_info,
                                                 nullptr, &device);
        if (!isUsable(result)) {
            fail(report, "vkCreateDevice for VK_KHR_swapchain failed: " + resultName(result));
        } else {
#define LOAD_DEVICE(member, symbol) \
            vk.member = reinterpret_cast<decltype(vk.member)>(vk.get_device_proc_addr(device, symbol))
            LOAD_DEVICE(destroy_device, "vkDestroyDevice");
            LOAD_DEVICE(get_device_queue, "vkGetDeviceQueue");
            LOAD_DEVICE(create_swapchain, "vkCreateSwapchainKHR");
            LOAD_DEVICE(destroy_swapchain, "vkDestroySwapchainKHR");
            LOAD_DEVICE(get_swapchain_images, "vkGetSwapchainImagesKHR");
            LOAD_DEVICE(create_semaphore, "vkCreateSemaphore");
            LOAD_DEVICE(destroy_semaphore, "vkDestroySemaphore");
            LOAD_DEVICE(acquire_next_image, "vkAcquireNextImageKHR");
            LOAD_DEVICE(queue_present, "vkQueuePresentKHR");
            LOAD_DEVICE(queue_wait_idle, "vkQueueWaitIdle");
            LOAD_DEVICE(device_wait_idle, "vkDeviceWaitIdle");
            LOAD_DEVICE(create_command_pool, "vkCreateCommandPool");
            LOAD_DEVICE(destroy_command_pool, "vkDestroyCommandPool");
            LOAD_DEVICE(allocate_command_buffers, "vkAllocateCommandBuffers");
            LOAD_DEVICE(free_command_buffers, "vkFreeCommandBuffers");
            LOAD_DEVICE(begin_command_buffer, "vkBeginCommandBuffer");
            LOAD_DEVICE(end_command_buffer, "vkEndCommandBuffer");
            LOAD_DEVICE(cmd_pipeline_barrier, "vkCmdPipelineBarrier");
            LOAD_DEVICE(queue_submit, "vkQueueSubmit");
#undef LOAD_DEVICE
            if (vk.get_device_queue == nullptr || vk.create_swapchain == nullptr ||
                vk.destroy_swapchain == nullptr || vk.get_swapchain_images == nullptr ||
                vk.create_semaphore == nullptr || vk.destroy_semaphore == nullptr ||
                vk.acquire_next_image == nullptr || vk.queue_present == nullptr ||
                vk.queue_wait_idle == nullptr || vk.create_command_pool == nullptr ||
                vk.destroy_command_pool == nullptr || vk.allocate_command_buffers == nullptr ||
                vk.free_command_buffers == nullptr || vk.begin_command_buffer == nullptr ||
                vk.end_command_buffer == nullptr || vk.cmd_pipeline_barrier == nullptr ||
                vk.queue_submit == nullptr || vk.device_wait_idle == nullptr) {
                fail(report, "One or more required Vulkan device entry points are unavailable");
            } else {
                vk.get_device_queue(device, selected_queue_family, 0U, &queue);
                if (queue == VK_NULL_HANDLE) fail(report, "vkGetDeviceQueue returned a null queue");
            }
        }
    }

    if (report.failure_reason.empty() && device != VK_NULL_HANDLE && queue != VK_NULL_HANDLE) {
        VkSurfaceCapabilitiesKHR capabilities{};
        VkResult result = vk.get_surface_capabilities(
            selected_physical_device, surface, &capabilities);
        if (result != VK_SUCCESS) {
            fail(report, "Surface capabilities query failed: " + resultName(result));
        } else {
            report.surface_capabilities_status = "success";
            std::uint32_t format_count = 0U;
            result = vk.get_surface_formats(selected_physical_device, surface, &format_count, nullptr);
            std::vector<VkSurfaceFormatKHR> formats;
            if (result == VK_SUCCESS && format_count > 0U) {
                formats.resize(format_count);
                result = vk.get_surface_formats(
                    selected_physical_device, surface, &format_count, formats.data());
            }
            if (result != VK_SUCCESS || formats.empty()) {
                fail(report, "Surface format enumeration failed or returned no formats");
            } else {
                report.surface_format_count = format_count;
                std::uint32_t mode_count = 0U;
                result = vk.get_present_modes(selected_physical_device, surface, &mode_count, nullptr);
                std::vector<VkPresentModeKHR> modes;
                if (result == VK_SUCCESS && mode_count > 0U) {
                    modes.resize(mode_count);
                    result = vk.get_present_modes(
                        selected_physical_device, surface, &mode_count, modes.data());
                }
                if (result != VK_SUCCESS || modes.empty()) {
                    fail(report, "Surface present-mode enumeration failed or returned no modes");
                } else {
                    report.present_mode_count = mode_count;
                    const bool fifo_supported = std::find(
                        modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_KHR) != modes.end();
                    if (!fifo_supported) {
                        fail(report, "The required VK_PRESENT_MODE_FIFO_KHR mode is unavailable");
                    } else {
                        VkSurfaceFormatKHR selected_format = formats.front();
                        if (selected_format.format == VK_FORMAT_UNDEFINED) {
                            selected_format.format = VK_FORMAT_B8G8R8A8_UNORM;
                        } else {
                            const auto preferred = std::find_if(formats.begin(), formats.end(),
                                [](const VkSurfaceFormatKHR& item) {
                                    return (item.format == VK_FORMAT_B8G8R8A8_SRGB ||
                                            item.format == VK_FORMAT_R8G8B8A8_SRGB) &&
                                           item.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                                });
                            if (preferred != formats.end()) selected_format = *preferred;
                        }
                        report.selected_surface_format =
                            formatName(selected_format.format) + " / " + colorSpaceName(selected_format.colorSpace);
                        report.selected_present_mode = "FIFO";

                        VkImageUsageFlags image_usage = 0U;
                        if ((capabilities.supportedUsageFlags &
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0U) {
                            image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                            report.selected_image_usage = "COLOR_ATTACHMENT";
                        } else if ((capabilities.supportedUsageFlags &
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0U) {
                            image_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                            report.selected_image_usage = "TRANSFER_DST";
                        } else {
                            fail(report, "Surface supports neither COLOR_ATTACHMENT nor TRANSFER_DST usage");
                        }

                        if (report.failure_reason.empty()) {
                            VkExtent2D extent{};
                            if (capabilities.currentExtent.width != UINT32_MAX) {
                                extent = capabilities.currentExtent;
                            } else {
                                extent.width = std::clamp(
                                    static_cast<std::uint32_t>(requested_width),
                                    capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
                                extent.height = std::clamp(
                                    static_cast<std::uint32_t>(requested_height),
                                    capabilities.minImageExtent.height,
                                    capabilities.maxImageExtent.height);
                            }
                            report.extent_width = extent.width;
                            report.extent_height = extent.height;
                            std::uint32_t image_count = capabilities.minImageCount + 1U;
                            if (capabilities.maxImageCount > 0U &&
                                image_count > capabilities.maxImageCount) {
                                image_count = capabilities.maxImageCount;
                            }

                            VkCompositeAlphaFlagBitsKHR composite_alpha =
                                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
                            const VkCompositeAlphaFlagBitsKHR alpha_modes[] = {
                                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                                VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
                            };
                            for (const auto candidate : alpha_modes) {
                                if ((capabilities.supportedCompositeAlpha & candidate) != 0U) {
                                    composite_alpha = candidate;
                                    break;
                                }
                            }

                            VkSwapchainCreateInfoKHR swapchain_info{
                                VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
                            swapchain_info.surface = surface;
                            swapchain_info.minImageCount = image_count;
                            swapchain_info.imageFormat = selected_format.format;
                            swapchain_info.imageColorSpace = selected_format.colorSpace;
                            swapchain_info.imageExtent = extent;
                            swapchain_info.imageArrayLayers = 1U;
                            swapchain_info.imageUsage = image_usage;
                            swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                            swapchain_info.preTransform = capabilities.currentTransform;
                            swapchain_info.compositeAlpha = composite_alpha;
                            swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
                            swapchain_info.clipped = VK_TRUE;
                            result = vk.create_swapchain(device, &swapchain_info, nullptr, &swapchain);
                            report.swapchain_create_status = isUsable(result) ? "success" : "failed";
                            if (!isUsable(result)) {
                                fail(report, "vkCreateSwapchainKHR failed: " + resultName(result));
                            } else {
                                std::uint32_t swapchain_image_count = 0U;
                                result = vk.get_swapchain_images(
                                    device, swapchain, &swapchain_image_count, nullptr);
                                std::vector<VkImage> images;
                                if (result == VK_SUCCESS && swapchain_image_count > 0U) {
                                    images.resize(swapchain_image_count);
                                    result = vk.get_swapchain_images(
                                        device, swapchain, &swapchain_image_count, images.data());
                                }
                                if (result != VK_SUCCESS || images.empty()) {
                                    report.swapchain_image_query_status = "failed";
                                    fail(report, "Swapchain image enumeration failed: " + resultName(result));
                                } else {
                                    report.swapchain_image_query_status = "success";
                                    report.swapchain_image_count = swapchain_image_count;
                                    VkSemaphoreCreateInfo semaphore_info{
                                        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                                    result = vk.create_semaphore(
                                        device, &semaphore_info, nullptr, &acquire_semaphore);
                                    report.semaphore_create_status =
                                        isUsable(result) ? "success" : "failed";
                                    if (!isUsable(result)) {
                                        fail(report, "vkCreateSemaphore failed: " + resultName(result));
                                    } else {
                                        std::uint32_t image_index = 0U;
                                        result = vk.acquire_next_image(
                                            device, swapchain, 1000000000ULL,
                                            acquire_semaphore, VK_NULL_HANDLE, &image_index);
                                        report.image_acquire_status =
                                            isUsable(result) ? "success" : "failed";
                                        if (!isUsable(result)) {
                                            fail(report, "vkAcquireNextImageKHR failed: " + resultName(result));
                                        } else if (image_index >= images.size()) {
                                            report.image_acquire_status = "failed";
                                            fail(report, "vkAcquireNextImageKHR returned an image index outside the enumerated swapchain image range");
                                        } else {
                                            report.image_acquired = true;
                                            report.acquired_image_index = image_index;

                                            result = vk.create_semaphore(
                                                device, &semaphore_info, nullptr, &present_semaphore);
                                            report.present_semaphore_create_status =
                                                result == VK_SUCCESS ? "success" : "failed";
                                            if (result != VK_SUCCESS) {
                                                fail(report, "Present semaphore creation failed: " + resultName(result));
                                            } else {
                                                VkCommandPoolCreateInfo pool_info{
                                                    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
                                                pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
                                                pool_info.queueFamilyIndex = selected_queue_family;
                                                result = vk.create_command_pool(
                                                    device, &pool_info, nullptr, &command_pool);
                                                report.command_pool_create_status =
                                                    result == VK_SUCCESS ? "success" : "failed";
                                                if (result != VK_SUCCESS) {
                                                    fail(report, "vkCreateCommandPool failed: " + resultName(result));
                                                } else {
                                                    VkCommandBufferAllocateInfo allocate_info{
                                                        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                                                    allocate_info.commandPool = command_pool;
                                                    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                                                    allocate_info.commandBufferCount = 1U;
                                                    result = vk.allocate_command_buffers(
                                                        device, &allocate_info, &command_buffer);
                                                    report.command_buffer_allocate_status =
                                                        result == VK_SUCCESS ? "success" : "failed";
                                                    if (result != VK_SUCCESS) {
                                                        fail(report, "vkAllocateCommandBuffers failed: " + resultName(result));
                                                    } else {
                                                        VkCommandBufferBeginInfo begin_info{
                                                            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                                                        begin_info.flags =
                                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                                                        result = vk.begin_command_buffer(
                                                            command_buffer, &begin_info);
                                                        if (result != VK_SUCCESS) {
                                                            report.command_record_status = "failed";
                                                            fail(report, "vkBeginCommandBuffer failed: " + resultName(result));
                                                        } else {
                                                            VkImageMemoryBarrier present_barrier{
                                                                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                                                            present_barrier.srcAccessMask = 0U;
                                                            present_barrier.dstAccessMask = 0U;
                                                            present_barrier.oldLayout =
                                                                VK_IMAGE_LAYOUT_UNDEFINED;
                                                            present_barrier.newLayout =
                                                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                                                            present_barrier.srcQueueFamilyIndex =
                                                                VK_QUEUE_FAMILY_IGNORED;
                                                            present_barrier.dstQueueFamilyIndex =
                                                                VK_QUEUE_FAMILY_IGNORED;
                                                            present_barrier.image = images[image_index];
                                                            present_barrier.subresourceRange.aspectMask =
                                                                VK_IMAGE_ASPECT_COLOR_BIT;
                                                            present_barrier.subresourceRange.baseMipLevel = 0U;
                                                            present_barrier.subresourceRange.levelCount = 1U;
                                                            present_barrier.subresourceRange.baseArrayLayer = 0U;
                                                            present_barrier.subresourceRange.layerCount = 1U;
                                                            // The acquire semaphore is waited at TOP_OF_PIPE in
                                                            // the following submission. This barrier transitions
                                                            // the acquired image to the layout required by present;
                                                            // no pixel or shader writes are performed.
                                                            vk.cmd_pipeline_barrier(
                                                                command_buffer,
                                                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                                0U, 0U, nullptr, 0U, nullptr,
                                                                1U, &present_barrier);
                                                            report.image_layout_transition_recorded = true;
                                                            result = vk.end_command_buffer(command_buffer);
                                                            report.command_record_status =
                                                                result == VK_SUCCESS ? "success" : "failed";
                                                            if (result != VK_SUCCESS) {
                                                                fail(report, "vkEndCommandBuffer failed: " + resultName(result));
                                                            } else {
                                                                // Block the command stream until acquisition is
                                                                // complete; presentation waits on the semaphore
                                                                // signaled after this layout transition.
                                                                const VkPipelineStageFlags wait_stage =
                                                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                                                                VkSubmitInfo submit_info{
                                                                    VK_STRUCTURE_TYPE_SUBMIT_INFO};
                                                                submit_info.waitSemaphoreCount = 1U;
                                                                submit_info.pWaitSemaphores = &acquire_semaphore;
                                                                submit_info.pWaitDstStageMask = &wait_stage;
                                                                submit_info.commandBufferCount = 1U;
                                                                submit_info.pCommandBuffers = &command_buffer;
                                                                submit_info.signalSemaphoreCount = 1U;
                                                                submit_info.pSignalSemaphores = &present_semaphore;
                                                                result = vk.queue_submit(
                                                                    queue, 1U, &submit_info, VK_NULL_HANDLE);
                                                                report.queue_submit_status =
                                                                    result == VK_SUCCESS ? "success" : "failed";
                                                                if (result != VK_SUCCESS) {
                                                                    fail(report, "vkQueueSubmit for present-layout transition failed: " + resultName(result));
                                                                } else {
                                                                    VkPresentInfoKHR present_info{
                                                                        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
                                                                    present_info.waitSemaphoreCount = 1U;
                                                                    present_info.pWaitSemaphores = &present_semaphore;
                                                                    present_info.swapchainCount = 1U;
                                                                    present_info.pSwapchains = &swapchain;
                                                                    present_info.pImageIndices = &image_index;
                                                                    result = vk.queue_present(queue, &present_info);
                                                                    report.image_present_status =
                                                                        isUsable(result) ? "success" : "failed";
                                                                    report.image_presented = isUsable(result);
                                                                    if (!isUsable(result)) {
                                                                        fail(report, "vkQueuePresentKHR failed: " + resultName(result));
                                                                    } else {
                                                                        result = vk.queue_wait_idle(queue);
                                                                        report.queue_idle_status =
                                                                            result == VK_SUCCESS ? "success" : "failed";
                                                                        if (result != VK_SUCCESS) {
                                                                            fail(report, "vkQueueWaitIdle failed: " + resultName(result));
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    bool cleanup_ok = true;
    if (device != VK_NULL_HANDLE && vk.device_wait_idle != nullptr) {
        const VkResult idle_result = vk.device_wait_idle(device);
        if (idle_result != VK_SUCCESS) cleanup_ok = false;
    }
    if (command_buffer != VK_NULL_HANDLE && command_pool != VK_NULL_HANDLE) {
        if (vk.free_command_buffers != nullptr) {
            vk.free_command_buffers(device, command_pool, 1U, &command_buffer);
            report.command_buffer_free_status = "success";
        } else {
            report.command_buffer_free_status = "failed";
            cleanup_ok = false;
        }
    }
    if (command_pool != VK_NULL_HANDLE) {
        if (vk.destroy_command_pool != nullptr) {
            vk.destroy_command_pool(device, command_pool, nullptr);
            report.command_pool_destroy_status = "success";
        } else {
            report.command_pool_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (acquire_semaphore != VK_NULL_HANDLE) {
        if (vk.destroy_semaphore != nullptr) {
            vk.destroy_semaphore(device, acquire_semaphore, nullptr);
            report.semaphore_destroy_status = "success";
        } else {
            report.semaphore_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (present_semaphore != VK_NULL_HANDLE) {
        if (vk.destroy_semaphore != nullptr) {
            vk.destroy_semaphore(device, present_semaphore, nullptr);
            report.present_semaphore_destroy_status = "success";
        } else {
            report.present_semaphore_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (swapchain != VK_NULL_HANDLE) {
        if (vk.destroy_swapchain != nullptr) {
            vk.destroy_swapchain(device, swapchain, nullptr);
            report.swapchain_destroy_status = "success";
        } else {
            report.swapchain_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (device != VK_NULL_HANDLE) {
        if (vk.destroy_device != nullptr) {
            vk.destroy_device(device, nullptr);
            report.device_destroy_status = "success";
        } else {
            report.device_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (surface != VK_NULL_HANDLE) {
        if (vk.destroy_surface != nullptr) {
            vk.destroy_surface(instance, surface, nullptr);
            report.surface_destroy_status = "success";
        } else {
            report.surface_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (instance != VK_NULL_HANDLE) {
        if (vk.destroy_instance != nullptr) {
            vk.destroy_instance(instance, nullptr);
            report.instance_destroy_status = "success";
        } else {
            report.instance_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (window != nullptr) {
        ANativeWindow_release(window);
        report.native_window_release_status = "success";
    }

    report.cleanup_status = cleanup_ok ? "success" : "failed";
    if (!cleanup_ok) fail(report, "One or more Vulkan objects could not be confirmed destroyed");
    if (vk.library != nullptr) dlclose(vk.library);

    const bool complete = report.loader_status == "success" &&
        report.instance_create_status == "success" &&
        report.surface_extension_status == "success" &&
        report.android_surface_extension_status == "success" &&
        report.native_window_status == "success" &&
        report.surface_create_status == "success" &&
        report.physical_device_status == "success" && report.target_gpu_match &&
        report.swapchain_extension_status == "success" &&
        report.queue_family_status == "success" &&
        report.graphics_queue_supported && report.present_queue_supported &&
        report.surface_capabilities_status == "success" &&
        report.surface_format_count > 0U && report.present_mode_count > 0U &&
        report.swapchain_create_status == "success" &&
        report.swapchain_image_query_status == "success" &&
        report.swapchain_image_count > 0U &&
        report.semaphore_create_status == "success" &&
        report.present_semaphore_create_status == "success" &&
        report.image_acquire_status == "success" && report.image_acquired &&
        report.command_pool_create_status == "success" &&
        report.command_buffer_allocate_status == "success" &&
        report.command_record_status == "success" &&
        report.image_layout_transition_recorded &&
        report.queue_submit_status == "success" &&
        report.image_present_status == "success" && report.image_presented &&
        report.queue_idle_status == "success" && report.cleanup_status == "success" &&
        report.semaphore_destroy_status == "success" &&
        report.present_semaphore_destroy_status == "success" &&
        report.command_buffer_free_status == "success" &&
        report.command_pool_destroy_status == "success";
    if (complete && report.failure_reason.empty()) report.status = "success";
    else if (report.status != "failed") report.status = "failed";
    return report;
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_dev_nyamist_g925surface_MainActivity_validateSurfaceNative(
        JNIEnv* env, jclass, jobject surface, jint width, jint height) {
    const auto report = runValidation(env, surface, width, height);
    const std::string json = nyamist::discovery::toJson(report);
    return env->NewStringUTF(json.c_str());
}
