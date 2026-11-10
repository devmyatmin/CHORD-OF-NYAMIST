#include "nyamist/discovery/android_surface_swapchain_validation.hpp"
#include "triangle_shaders.hpp"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <dlfcn.h>
#include <string>
#include <thread>
#include <vector>

namespace {

using nyamist::discovery::AndroidSurfaceSwapchainValidationReport;

constexpr std::array<const char*, 16> kVulkan14PromotedExtensions{
    "VK_KHR_dynamic_rendering_local_read",
    "VK_KHR_global_priority",
    "VK_KHR_index_type_uint8",
    "VK_KHR_line_rasterization",
    "VK_KHR_load_store_op_none",
    "VK_KHR_maintenance5",
    "VK_KHR_maintenance6",
    "VK_KHR_map_memory2",
    "VK_KHR_push_descriptor",
    "VK_KHR_shader_expect_assume",
    "VK_KHR_shader_float_controls2",
    "VK_KHR_shader_subgroup_rotate",
    "VK_KHR_vertex_attribute_divisor",
    "VK_EXT_host_image_copy",
    "VK_EXT_pipeline_protected_access",
    "VK_EXT_pipeline_robustness"
};

struct VulkanDispatch {
    void* library{};
    PFN_vkGetInstanceProcAddr get_instance_proc_addr{};

    PFN_vkEnumerateInstanceExtensionProperties enumerate_instance_extensions{};
    PFN_vkEnumerateInstanceVersion enumerate_instance_version{};
    PFN_vkCreateInstance create_instance{};

    PFN_vkDestroyInstance destroy_instance{};
    PFN_vkEnumeratePhysicalDevices enumerate_physical_devices{};
    PFN_vkGetPhysicalDeviceProperties get_physical_device_properties{};
    PFN_vkGetPhysicalDeviceProperties2 get_physical_device_properties2{};
    PFN_vkGetPhysicalDeviceMemoryProperties get_physical_device_memory_properties{};
    PFN_vkGetPhysicalDeviceFeatures2 get_physical_device_features2{};
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
    PFN_vkCreateImageView create_image_view{};
    PFN_vkCreateBuffer create_buffer{};
    PFN_vkDestroyBuffer destroy_buffer{};
    PFN_vkGetBufferMemoryRequirements get_buffer_memory_requirements{};
    PFN_vkAllocateMemory allocate_memory{};
    PFN_vkFreeMemory free_memory{};
    PFN_vkBindBufferMemory bind_buffer_memory{};
    PFN_vkMapMemory map_memory{};
    PFN_vkUnmapMemory unmap_memory{};
    PFN_vkInvalidateMappedMemoryRanges invalidate_mapped_memory_ranges{};
    PFN_vkDestroyImageView destroy_image_view{};
    PFN_vkCreateRenderPass create_render_pass{};
    PFN_vkDestroyRenderPass destroy_render_pass{};
    PFN_vkCreateFramebuffer create_framebuffer{};
    PFN_vkDestroyFramebuffer destroy_framebuffer{};
    PFN_vkCreateShaderModule create_shader_module{};
    PFN_vkDestroyShaderModule destroy_shader_module{};
    PFN_vkCreatePipelineLayout create_pipeline_layout{};
    PFN_vkDestroyPipelineLayout destroy_pipeline_layout{};
    PFN_vkCreateGraphicsPipelines create_graphics_pipelines{};
    PFN_vkDestroyPipeline destroy_pipeline{};
    PFN_vkCmdBeginRenderPass cmd_begin_render_pass{};
    PFN_vkCmdEndRenderPass cmd_end_render_pass{};
    PFN_vkCmdBindPipeline cmd_bind_pipeline{};
    PFN_vkCmdSetViewport cmd_set_viewport{};
    PFN_vkCmdSetScissor cmd_set_scissor{};
    PFN_vkCmdDraw cmd_draw{};
    PFN_vkCmdBindIndexBuffer cmd_bind_index_buffer{};
    PFN_vkCmdDrawIndexed cmd_draw_indexed{};
    PFN_vkCmdCopyImageToBuffer cmd_copy_image_to_buffer{};
    PFN_vkGetSwapchainImagesKHR get_swapchain_images{};
    PFN_vkCreateSemaphore create_semaphore{};
    PFN_vkDestroySemaphore destroy_semaphore{};
    PFN_vkCreateFence create_fence{};
    PFN_vkDestroyFence destroy_fence{};
    PFN_vkWaitForFences wait_for_fences{};
    PFN_vkResetFences reset_fences{};
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
    PFN_vkResetCommandPool reset_command_pool{};
    PFN_vkCreateQueryPool create_query_pool{};
    PFN_vkDestroyQueryPool destroy_query_pool{};
    PFN_vkCmdResetQueryPool cmd_reset_query_pool{};
    PFN_vkCmdWriteTimestamp cmd_write_timestamp{};
    PFN_vkGetQueryPoolResults get_query_pool_results{};
};

enum class SwapchainResultPolicy {
    continue_operation,
    recreate_after_operation,
    recreate_before_operation,
    fatal
};

SwapchainResultPolicy swapchainResultPolicy(VkResult result) {
    if (result == VK_SUCCESS) return SwapchainResultPolicy::continue_operation;
    if (result == VK_SUBOPTIMAL_KHR) return SwapchainResultPolicy::recreate_after_operation;
    if (result == VK_ERROR_OUT_OF_DATE_KHR) return SwapchainResultPolicy::recreate_before_operation;
    return SwapchainResultPolicy::fatal;
}

bool swapchainRecoveryPolicySelfTest() {
    return swapchainResultPolicy(VK_SUCCESS) == SwapchainResultPolicy::continue_operation &&
           swapchainResultPolicy(VK_SUBOPTIMAL_KHR) == SwapchainResultPolicy::recreate_after_operation &&
           swapchainResultPolicy(VK_ERROR_OUT_OF_DATE_KHR) == SwapchainResultPolicy::recreate_before_operation &&
           swapchainResultPolicy(VK_ERROR_SURFACE_LOST_KHR) == SwapchainResultPolicy::fatal;
}

bool isUsable(VkResult result) {
    return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
}

// Android may report a non-identity currentTransform after device rotation.
// This renderer uses the current, already-oriented Surface extent and does not
// pre-rotate clip-space geometry. Prefer IDENTITY so the presentation engine
// does not apply a second rotation to content rendered for the new extent.
VkSurfaceTransformFlagBitsKHR chooseSwapchainPreTransform(
        const VkSurfaceCapabilitiesKHR& capabilities) {
    if ((capabilities.supportedTransforms &
         VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0U) {
        return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    return capabilities.currentTransform;
}

void recordSurfaceTransform(
        AndroidSurfaceSwapchainValidationReport& report,
        const VkSurfaceCapabilitiesKHR& capabilities) {
    const VkSurfaceTransformFlagBitsKHR selected =
        chooseSwapchainPreTransform(capabilities);
    report.surface_current_transform =
        static_cast<std::uint32_t>(capabilities.currentTransform);
    report.surface_supported_transforms =
        static_cast<std::uint32_t>(capabilities.supportedTransforms);
    report.swapchain_pre_transform = static_cast<std::uint32_t>(selected);
    report.swapchain_pre_transform_identity_selected =
        selected == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    report.swapchain_pre_transform_policy =
        report.swapchain_pre_transform_identity_selected
            ? "identity_no_app_prerotation"
            : "current_transform_fallback_requires_mvp_prerotation";
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

bool versionAtLeast(std::uint32_t version, std::uint32_t major,
                    std::uint32_t minor) {
    return VK_VERSION_MAJOR(version) > major ||
           (VK_VERSION_MAJOR(version) == major &&
            VK_VERSION_MINOR(version) >= minor);
}

std::string hex32(std::uint32_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result = "0x00000000";
    for (std::size_t index = 0; index < 8U; ++index) {
        const std::size_t shift = (7U - index) * 4U;
        result[index + 2U] = digits[(value >> shift) & 0xfU];
    }
    return result;
}

std::string uuidHex(const std::uint8_t* bytes, std::size_t count) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(count * 2U);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(digits[(bytes[index] >> 4U) & 0xfU]);
        result.push_back(digits[bytes[index] & 0xfU]);
    }
    return result;
}

std::string driverIdName(std::uint32_t driver_id) {
    switch (driver_id) {
    case 8U: return "VK_DRIVER_ID_QUALCOMM_PROPRIETARY";
    case 9U: return "VK_DRIVER_ID_ARM_PROPRIETARY";
    case 10U: return "VK_DRIVER_ID_GOOGLE_SWIFTSHADER";
    case 20U: return "VK_DRIVER_ID_MESA_PANVK";
    default: return "VK_DRIVER_ID_UNKNOWN";
    }
}

std::string conformanceVersionString(const VkConformanceVersion& version) {
    return std::to_string(version.major) + "." +
           std::to_string(version.minor) + "." +
           std::to_string(version.subminor) + "." +
           std::to_string(version.patch);
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

std::uint8_t quantizePixelChannel(float linear_value, bool srgb) {
    const float clamped = std::clamp(linear_value, 0.0F, 1.0F);
    const float encoded = !srgb ? clamped :
        (clamped <= 0.0031308F
            ? 12.92F * clamped
            : 1.055F * std::pow(clamped, 1.0F / 2.4F) - 0.055F);
    return static_cast<std::uint8_t>(
        std::clamp(std::lround(encoded * 255.0F), 0L, 255L));
}

bool createPixelReadbackResources(
        VulkanDispatch& vk,
        AndroidSurfaceSwapchainValidationReport& report,
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkBuffer& readback_buffer,
        VkDeviceMemory& readback_memory,
        bool& host_coherent) {
    constexpr VkDeviceSize sample_bytes = 5U * 4U;
    report.pixel_readback_mode = "swapchain_transfer_src";
    report.pixel_readback_source = "acquired_swapchain_image";
    report.pixel_readback_byte_count = sample_bytes;
    report.pixel_readback_sample_labels =
        "top_left,top_right,bottom_left,bottom_right,triangle_center";
    if (vk.get_physical_device_memory_properties == nullptr) {
        report.pixel_readback_memory_allocate_status = "memory_properties_entry_point_missing";
        fail(report, "vkGetPhysicalDeviceMemoryProperties is unavailable");
        return false;
    }

    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = sample_bytes;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vk.create_buffer(device, &buffer_info, nullptr, &readback_buffer);
    report.pixel_readback_buffer_create_status =
        result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkCreateBuffer(pixel readback) failed: " + resultName(result));
        return false;
    }

    VkMemoryRequirements requirements{};
    vk.get_buffer_memory_requirements(device, readback_buffer, &requirements);
    report.pixel_readback_allocation_size = requirements.size;

    VkPhysicalDeviceMemoryProperties memory_properties{};
    vk.get_physical_device_memory_properties(physical_device, &memory_properties);
    std::uint32_t selected_memory_type = UINT32_MAX;
    std::uint32_t fallback_memory_type = UINT32_MAX;
    for (std::uint32_t index = 0U; index < memory_properties.memoryTypeCount; ++index) {
        if ((requirements.memoryTypeBits & (1U << index)) == 0U) continue;
        const VkMemoryPropertyFlags flags =
            memory_properties.memoryTypes[index].propertyFlags;
        if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0U) continue;
        if ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0U) {
            selected_memory_type = index;
            host_coherent = true;
            break;
        }
        if (fallback_memory_type == UINT32_MAX) fallback_memory_type = index;
    }
    if (selected_memory_type == UINT32_MAX && fallback_memory_type != UINT32_MAX) {
        selected_memory_type = fallback_memory_type;
        host_coherent = false;
    }
    if (selected_memory_type == UINT32_MAX) {
        report.pixel_readback_memory_allocate_status = "no_host_visible_memory_type";
        fail(report, "No HOST_VISIBLE memory type is available for pixel readback");
        return false;
    }

    report.pixel_readback_memory_type = host_coherent
        ? "HOST_VISIBLE|HOST_COHERENT" : "HOST_VISIBLE";
    VkMemoryAllocateInfo allocation_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation_info.allocationSize = requirements.size;
    allocation_info.memoryTypeIndex = selected_memory_type;
    result = vk.allocate_memory(device, &allocation_info, nullptr, &readback_memory);
    report.pixel_readback_memory_allocate_status =
        result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkAllocateMemory(pixel readback) failed: " + resultName(result));
        return false;
    }

    result = vk.bind_buffer_memory(device, readback_buffer, readback_memory, 0U);
    report.pixel_readback_bind_status =
        result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkBindBufferMemory(pixel readback) failed: " + resultName(result));
        return false;
    }
    return true;
}

bool createUint8IndexProbeResources(
        VulkanDispatch& vk,
        AndroidSurfaceSwapchainValidationReport& report,
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkBuffer& index_buffer,
        VkDeviceMemory& index_memory) {
    if (report.index_type_uint8_feature_enable_status != "enabled") {
        const std::string support = report.index_type_uint8_feature_support_status;
        report.index_type_uint8_probe_status = support;
        report.index_type_uint8_buffer_create_status = "not_required";
        report.index_type_uint8_memory_allocate_status = "not_required";
        report.index_type_uint8_memory_bind_status = "not_required";
        report.index_type_uint8_memory_map_status = "not_required";
        report.index_type_uint8_upload_status = "not_required";
        report.index_type_uint8_memory_unmap_status = "not_required";
        return true;
    }

    if (vk.get_physical_device_memory_properties == nullptr ||
        vk.create_buffer == nullptr || vk.get_buffer_memory_requirements == nullptr ||
        vk.allocate_memory == nullptr || vk.bind_buffer_memory == nullptr ||
        vk.map_memory == nullptr || vk.unmap_memory == nullptr) {
        report.index_type_uint8_probe_status = "entry_point_unavailable";
        fail(report, "R2.6 uint8 index-buffer resource entry points are unavailable");
        return false;
    }

    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = 3U;
    buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vk.create_buffer(device, &buffer_info, nullptr, &index_buffer);
    report.index_type_uint8_buffer_create_status =
        result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        report.index_type_uint8_probe_status = "buffer_create_failed";
        fail(report, "R2.6 vkCreateBuffer(uint8 index) failed: " + resultName(result));
        return false;
    }

    VkMemoryRequirements requirements{};
    vk.get_buffer_memory_requirements(device, index_buffer, &requirements);
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vk.get_physical_device_memory_properties(physical_device, &memory_properties);

    std::uint32_t coherent_memory_type = UINT32_MAX;
    for (std::uint32_t index = 0U; index < memory_properties.memoryTypeCount; ++index) {
        if ((requirements.memoryTypeBits & (1U << index)) == 0U) continue;
        const VkMemoryPropertyFlags flags =
            memory_properties.memoryTypes[index].propertyFlags;
        if ((flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            coherent_memory_type = index;
            break;
        }
    }
    if (coherent_memory_type == UINT32_MAX) {
        report.index_type_uint8_memory_allocate_status =
            "host_visible_coherent_memory_unavailable";
        report.index_type_uint8_probe_status = "memory_type_unavailable";
        fail(report, "R2.6 uint8 index probe requires HOST_VISIBLE|HOST_COHERENT memory");
        return false;
    }

    VkMemoryAllocateInfo allocation_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation_info.allocationSize = requirements.size;
    allocation_info.memoryTypeIndex = coherent_memory_type;
    result = vk.allocate_memory(device, &allocation_info, nullptr, &index_memory);
    report.index_type_uint8_memory_allocate_status =
        result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        report.index_type_uint8_probe_status = "memory_allocate_failed";
        fail(report, "R2.6 vkAllocateMemory(uint8 index) failed: " + resultName(result));
        return false;
    }

    result = vk.bind_buffer_memory(device, index_buffer, index_memory, 0U);
    report.index_type_uint8_memory_bind_status =
        result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        report.index_type_uint8_probe_status = "memory_bind_failed";
        fail(report, "R2.6 vkBindBufferMemory(uint8 index) failed: " + resultName(result));
        return false;
    }

    void* mapped = nullptr;
    result = vk.map_memory(device, index_memory, 0U, VK_WHOLE_SIZE, 0U, &mapped);
    report.index_type_uint8_memory_map_status =
        result == VK_SUCCESS && mapped != nullptr ? "success" : "failed";
    if (result != VK_SUCCESS || mapped == nullptr) {
        report.index_type_uint8_probe_status = "memory_map_failed";
        fail(report, "R2.6 vkMapMemory(uint8 index) failed: " + resultName(result));
        return false;
    }

    constexpr std::array<std::uint8_t, 3> indices{0U, 1U, 2U};
    std::memcpy(mapped, indices.data(), indices.size());
    report.index_type_uint8_upload_status = "success";
    vk.unmap_memory(device, index_memory);
    report.index_type_uint8_memory_unmap_status = "success";
    return true;
}

bool verifyPixelReadback(
        VulkanDispatch& vk,
        AndroidSurfaceSwapchainValidationReport& report,
        VkDevice device,
        VkDeviceMemory readback_memory,
        VkFormat format,
        VkExtent2D extent,
        bool host_coherent) {
    if (extent.width < 8U || extent.height < 8U) {
        report.pixel_verification_status = "invalid_extent";
        fail(report, "The swapchain extent is too small for independent pixel sample points");
        return false;
    }

    void* mapped = nullptr;
    VkResult result = vk.map_memory(
        device, readback_memory, 0U, VK_WHOLE_SIZE, 0U, &mapped);
    report.pixel_readback_map_status =
        result == VK_SUCCESS && mapped != nullptr ? "success" : "failed";
    if (result != VK_SUCCESS || mapped == nullptr) {
        report.pixel_verification_status = "map_failed";
        fail(report, "vkMapMemory(pixel readback) failed: " + resultName(result));
        return false;
    }

    bool readable = true;
    if (host_coherent) {
        report.pixel_readback_invalidate_status = "not_required_host_coherent";
    } else {
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory = readback_memory;
        range.offset = 0U;
        range.size = VK_WHOLE_SIZE;
        result = vk.invalidate_mapped_memory_ranges(device, 1U, &range);
        report.pixel_readback_invalidate_status =
            result == VK_SUCCESS ? "success" : "failed";
        readable = result == VK_SUCCESS;
        if (!readable) {
            report.pixel_verification_status = "invalidate_failed";
            fail(report, "vkInvalidateMappedMemoryRanges(pixel readback) failed: " +
                         resultName(result));
        }
    }

    if (readable) {
        const bool srgb = format == VK_FORMAT_R8G8B8A8_SRGB ||
                          format == VK_FORMAT_B8G8R8A8_SRGB;
        const bool bgra = format == VK_FORMAT_B8G8R8A8_SRGB ||
                          format == VK_FORMAT_B8G8R8A8_UNORM;
        const std::uint64_t expected[4] = {
            quantizePixelChannel(0.035F, srgb),
            quantizePixelChannel(0.14F, srgb),
            quantizePixelChannel(0.32F, srgb),
            255U
        };
        for (const std::uint64_t channel : expected) {
            report.pixel_expected_clear_rgba8.push_back(channel);
        }

        const auto* bytes = static_cast<const std::uint8_t*>(mapped);
        bool backgrounds_match = true;
        for (std::uint32_t sample = 0U; sample < 5U; ++sample) {
            const std::uint8_t* raw = bytes + (sample * 4U);
            const std::uint64_t rgba[4] = {
                bgra ? raw[2] : raw[0],
                raw[1],
                bgra ? raw[0] : raw[2],
                raw[3]
            };
            for (const std::uint64_t channel : rgba) {
                report.pixel_rgba8_samples.push_back(channel);
            }
            if (sample < 4U) {
                for (std::size_t channel = 0U; channel < 4U; ++channel) {
                    const auto delta = rgba[channel] > expected[channel]
                        ? rgba[channel] - expected[channel]
                        : expected[channel] - rgba[channel];
                    const std::uint64_t tolerance = channel == 3U ? 1U : 4U;
                    if (delta > tolerance) backgrounds_match = false;
                }
            } else {
                bool differs_from_clear = false;
                for (std::size_t channel = 0U; channel < 3U; ++channel) {
                    const auto delta = rgba[channel] > expected[channel]
                        ? rgba[channel] - expected[channel]
                        : expected[channel] - rgba[channel];
                    if (delta >= 16U) differs_from_clear = true;
                }
                report.pixel_triangle_center_detected =
                    differs_from_clear && rgba[3] >= 250U;
            }
        }
        report.pixel_readback_sample_count = 5U;
        report.pixel_background_samples_match_clear = backgrounds_match;
        report.pixel_readback_copy_status = "success";
        report.pixel_verification_status =
            backgrounds_match && report.pixel_triangle_center_detected
                ? "success" : "failed";
        if (report.pixel_verification_status != "success") {
            fail(report, "Pixel samples did not match the clear-color background and rendered triangle expectations");
        }
    }

    vk.unmap_memory(device, readback_memory);
    report.pixel_readback_unmap_status = "success";
    return readable && report.pixel_verification_status == "success";
}



bool renderAndPresentFrame(
        VulkanDispatch& vk,
        AndroidSurfaceSwapchainValidationReport& report,
        VkDevice device,
        VkQueue queue,
        std::uint32_t queue_family,
        VkSwapchainKHR swapchain,
        VkImage image,
        VkFormat format,
        VkExtent2D extent,
        std::uint32_t image_index,
        VkSemaphore acquire_semaphore,
        VkSemaphore present_semaphore,
        VkCommandPool& command_pool,
        VkCommandBuffer& command_buffer,
        VkImageView& image_view,
        VkRenderPass& render_pass,
        VkFramebuffer& framebuffer,
        VkShaderModule& vertex_shader,
        VkShaderModule& fragment_shader,
        VkPipelineLayout& pipeline_layout,
        VkPipeline& graphics_pipeline,
        VkQueryPool timestamp_query_pool,
        std::uint32_t timestamp_query_base,
        VkFence present_fence,
        bool present_fence_enabled,
        bool pixel_readback_enabled,
        bool perform_pixel_readback,
        VkBuffer pixel_readback_buffer,
        VkDeviceMemory pixel_readback_memory,
        bool pixel_readback_memory_coherent,
        VkBuffer uint8_index_buffer,
        bool uint8_index_probe_enabled,
        bool collect_frame_sample,
        std::chrono::steady_clock::time_point frame_start) {
    VkImageViewCreateInfo image_view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = format;
    image_view_info.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
    };
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0U;
    image_view_info.subresourceRange.levelCount = 1U;
    image_view_info.subresourceRange.baseArrayLayer = 0U;
    image_view_info.subresourceRange.layerCount = 1U;

    VkResult result = vk.create_image_view(device, &image_view_info, nullptr, &image_view);
    report.image_view_create_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkCreateImageView failed: " + resultName(result));
        return false;
    }

    if (render_pass == VK_NULL_HANDLE) {
        VkAttachmentDescription color_attachment{};
        color_attachment.format = format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = pixel_readback_enabled
            ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_reference{};
        color_reference.attachment = 0U;
        color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1U;
        subpass.pColorAttachments = &color_reference;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0U;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0U;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        render_pass_info.attachmentCount = 1U;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1U;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1U;
        render_pass_info.pDependencies = &dependency;
        result = vk.create_render_pass(device, &render_pass_info, nullptr, &render_pass);
        report.render_pass_create_status = result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateRenderPass failed: " + resultName(result));
            return false;
        }

    }

    VkFramebufferCreateInfo framebuffer_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer_info.renderPass = render_pass;
    framebuffer_info.attachmentCount = 1U;
    framebuffer_info.pAttachments = &image_view;
    framebuffer_info.width = extent.width;
    framebuffer_info.height = extent.height;
    framebuffer_info.layers = 1U;
    result = vk.create_framebuffer(device, &framebuffer_info, nullptr, &framebuffer);
    report.framebuffer_create_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkCreateFramebuffer failed: " + resultName(result));
        return false;
    }

    // Viewport/scissor are computed from the current swapchain extent for every
    // frame. They must not be frozen into a reusable graphics-pipeline object:
    // Surface rotations/resizes can leave that object alive across WSI sessions.
    const std::uint32_t render_viewport_extent =
        std::min(extent.width, extent.height);
    const std::uint32_t render_viewport_x =
        (extent.width - render_viewport_extent) / 2U;
    const std::uint32_t render_viewport_y =
        (extent.height - render_viewport_extent) / 2U;
    VkViewport render_viewport{};
    render_viewport.x = static_cast<float>(render_viewport_x);
    render_viewport.y = static_cast<float>(render_viewport_y);
    render_viewport.width = static_cast<float>(render_viewport_extent);
    render_viewport.height = static_cast<float>(render_viewport_extent);
    render_viewport.minDepth = 0.0F;
    render_viewport.maxDepth = 1.0F;
    VkRect2D render_scissor{};
    render_scissor.offset = {0, 0};
    render_scissor.extent = extent;

    if (graphics_pipeline == VK_NULL_HANDLE) {
        VkShaderModuleCreateInfo vertex_shader_info{
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        vertex_shader_info.codeSize = sizeof(nyamist::shaders::triangle_vertex_spirv);
        vertex_shader_info.pCode = nyamist::shaders::triangle_vertex_spirv;
        result = vk.create_shader_module(device, &vertex_shader_info, nullptr, &vertex_shader);
        report.vertex_shader_module_create_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateShaderModule(vertex) failed: " + resultName(result));
            return false;
        }

        VkShaderModuleCreateInfo fragment_shader_info{
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        fragment_shader_info.codeSize = sizeof(nyamist::shaders::triangle_fragment_spirv);
        fragment_shader_info.pCode = nyamist::shaders::triangle_fragment_spirv;
        result = vk.create_shader_module(device, &fragment_shader_info, nullptr, &fragment_shader);
        report.fragment_shader_module_create_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateShaderModule(fragment) failed: " + resultName(result));
            return false;
        }

        VkPipelineLayoutCreateInfo pipeline_layout_info{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        result = vk.create_pipeline_layout(device, &pipeline_layout_info, nullptr,
                                           &pipeline_layout);
        report.pipeline_layout_create_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreatePipelineLayout failed: " + resultName(result));
            return false;
        }

        VkPipelineShaderStageCreateInfo shader_stages[2]{};
        shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stages[0].module = vertex_shader;
        shader_stages[0].pName = "main";
        shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stages[1].module = fragment_shader;
        shader_stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertex_input{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo input_assembly{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // Viewport and scissor are dynamic; current extent-derived values are
        // supplied with vkCmdSetViewport/vkCmdSetScissor for every frame.
        VkPipelineViewportStateCreateInfo viewport_state{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport_state.viewportCount = 1U;
        viewport_state.pViewports = nullptr;
        viewport_state.scissorCount = 1U;
        viewport_state.pScissors = nullptr;
        const VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamic_state_info{
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic_state_info.dynamicStateCount = 2U;
        dynamic_state_info.pDynamicStates = dynamic_states;

        VkPipelineRasterizationStateCreateInfo rasterization{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterization.depthClampEnable = VK_FALSE;
        rasterization.rasterizerDiscardEnable = VK_FALSE;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.depthBiasEnable = VK_FALSE;
        rasterization.lineWidth = 1.0F;

        VkPipelineMultisampleStateCreateInfo multisampling{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.sampleShadingEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.blendEnable = VK_FALSE;
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo color_blending{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1U;
        color_blending.pAttachments = &color_blend_attachment;

        VkGraphicsPipelineCreateInfo pipeline_info{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipeline_info.stageCount = 2U;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pDynamicState = &dynamic_state_info;
        pipeline_info.pRasterizationState = &rasterization;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.layout = pipeline_layout;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0U;
        pipeline_info.basePipelineIndex = -1;
        result = vk.create_graphics_pipelines(device, VK_NULL_HANDLE, 1U,
                                              &pipeline_info, nullptr,
                                              &graphics_pipeline);
        report.graphics_pipeline_create_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateGraphicsPipelines failed: " + resultName(result));
            return false;
        }

    }

    if (command_pool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool_info.queueFamilyIndex = queue_family;
        result = vk.create_command_pool(device, &pool_info, nullptr, &command_pool);
        report.command_pool_create_status = result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateCommandPool failed: " + resultName(result));
            return false;
        }

        VkCommandBufferAllocateInfo allocate_info{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.commandPool = command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1U;
        result = vk.allocate_command_buffers(device, &allocate_info, &command_buffer);
        report.command_buffer_allocate_status = result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkAllocateCommandBuffers failed: " + resultName(result));
            return false;
        }

    } else {
        result = vk.reset_command_pool(device, command_pool, 0U);
        if (result != VK_SUCCESS) {
            fail(report, "vkResetCommandPool failed: " + resultName(result));
            return false;
        }
    }

    VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vk.begin_command_buffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        report.command_record_status = "failed";
        fail(report, "vkBeginCommandBuffer failed: " + resultName(result));
        return false;
    }

    if (timestamp_query_pool != VK_NULL_HANDLE && collect_frame_sample) {
        vk.cmd_reset_query_pool(command_buffer, timestamp_query_pool, timestamp_query_base, 2U);
        vk.cmd_write_timestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               timestamp_query_pool, timestamp_query_base);
    }

    VkClearValue clear_value{};
    clear_value.color.float32[0] = 0.035F;
    clear_value.color.float32[1] = 0.14F;
    clear_value.color.float32[2] = 0.32F;
    clear_value.color.float32[3] = 1.0F;

    VkRenderPassBeginInfo render_begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render_begin.renderPass = render_pass;
    render_begin.framebuffer = framebuffer;
    render_begin.renderArea.offset = {0, 0};
    render_begin.renderArea.extent = extent;
    render_begin.clearValueCount = 1U;
    render_begin.pClearValues = &clear_value;

    // The reusable render pass clears the acquired swapchain image, then executes
    // the R2.0 graphics pipeline and three-vertex draw before presentation.
    vk.cmd_begin_render_pass(command_buffer, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
    report.render_pass_begin_recorded = true;
    vk.cmd_bind_pipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                         graphics_pipeline);
    vk.cmd_set_viewport(command_buffer, 0U, 1U, &render_viewport);
    vk.cmd_set_scissor(command_buffer, 0U, 1U, &render_scissor);
    report.render_viewport_dynamic_state_enabled = true;
    report.render_viewport_x = render_viewport_x;
    report.render_viewport_y = render_viewport_y;
    report.render_viewport_width = render_viewport.width;
    report.render_viewport_height = render_viewport.height;
    ++report.render_viewport_update_count;
    report.render_scissor_width = render_scissor.extent.width;
    report.render_scissor_height = render_scissor.extent.height;
    ++report.render_scissor_update_count;
    report.graphics_pipeline_bind_recorded = true;
    if (uint8_index_probe_enabled && perform_pixel_readback) {
        // Make the first/readback frame an indexed-only draw. Pixel readback
        // therefore proves the uint8 index path rendered the triangle; a
        // preceding non-indexed draw cannot mask a failed indexed draw.
        if (uint8_index_buffer == VK_NULL_HANDLE ||
            vk.cmd_bind_index_buffer == nullptr || vk.cmd_draw_indexed == nullptr) {
            report.index_type_uint8_draw_entry_points_status = "unavailable";
            report.index_type_uint8_draw_record_status = "failed";
            report.index_type_uint8_probe_status = "draw_entry_point_unavailable";
            fail(report, "R2.6 uint8 indexed draw entry points or index buffer are unavailable");
            return false;
        }
        report.index_type_uint8_draw_entry_points_status = "success";
        vk.cmd_bind_index_buffer(command_buffer, uint8_index_buffer, 0U,
                                 VK_INDEX_TYPE_UINT8_KHR);
        vk.cmd_draw_indexed(command_buffer, 3U, 1U, 0, 0U, 0U);
        report.index_type_uint8_draw_record_status = "success";
        report.index_type_uint8_draw_recorded = true;
        report.index_type_uint8_draw_index_count = 3U;
    } else {
        vk.cmd_draw(command_buffer, 3U, 1U, 0U, 0U);
    }
    report.draw_call_recorded = true;
    report.draw_vertex_count = 3U;
    vk.cmd_end_render_pass(command_buffer);
    report.render_pass_end_recorded = true;
    report.image_layout_transition_recorded = true;
    if (timestamp_query_pool != VK_NULL_HANDLE && collect_frame_sample) {
        // Timestamp scope ends with the render pass; the diagnostic image copy is excluded.
        vk.cmd_write_timestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                               timestamp_query_pool, timestamp_query_base + 1U);
    }

    if (pixel_readback_enabled) {
        if (perform_pixel_readback) {
            VkImageMemoryBarrier image_to_transfer{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            image_to_transfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            image_to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            image_to_transfer.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            image_to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            image_to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_to_transfer.image = image;
            image_to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            image_to_transfer.subresourceRange.baseMipLevel = 0U;
            image_to_transfer.subresourceRange.levelCount = 1U;
            image_to_transfer.subresourceRange.baseArrayLayer = 0U;
            image_to_transfer.subresourceRange.layerCount = 1U;
            vk.cmd_pipeline_barrier(command_buffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0U,
                0U, nullptr, 0U, nullptr, 1U, &image_to_transfer);

            const std::uint32_t sample_x[5] = {
                2U, extent.width - 3U, 2U, extent.width - 3U, extent.width / 2U
            };
            const std::uint32_t sample_y[5] = {
                2U, 2U, extent.height - 3U, extent.height - 3U, extent.height / 2U
            };
            VkBufferImageCopy regions[5]{};
            for (std::uint32_t sample = 0U; sample < 5U; ++sample) {
                regions[sample].bufferOffset = static_cast<VkDeviceSize>(sample) * 4U;
                regions[sample].bufferRowLength = 0U;
                regions[sample].bufferImageHeight = 0U;
                regions[sample].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                regions[sample].imageSubresource.mipLevel = 0U;
                regions[sample].imageSubresource.baseArrayLayer = 0U;
                regions[sample].imageSubresource.layerCount = 1U;
                regions[sample].imageOffset = {
                    static_cast<std::int32_t>(sample_x[sample]),
                    static_cast<std::int32_t>(sample_y[sample]), 0
                };
                regions[sample].imageExtent = {1U, 1U, 1U};
            }
            vk.cmd_copy_image_to_buffer(command_buffer, image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pixel_readback_buffer,
                5U, regions);
            report.pixel_readback_recorded = true;
            report.pixel_readback_copy_status = "recorded";

            VkBufferMemoryBarrier buffer_to_host{
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            buffer_to_host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            buffer_to_host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            buffer_to_host.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            buffer_to_host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            buffer_to_host.buffer = pixel_readback_buffer;
            buffer_to_host.offset = 0U;
            buffer_to_host.size = 5U * 4U;

            VkImageMemoryBarrier image_to_present{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            image_to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            image_to_present.dstAccessMask = 0U;
            image_to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            image_to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            image_to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_to_present.image = image;
            image_to_present.subresourceRange = image_to_transfer.subresourceRange;
            vk.cmd_pipeline_barrier(command_buffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0U, 0U, nullptr, 1U, &buffer_to_host, 1U, &image_to_present);
        } else {
            VkImageMemoryBarrier image_to_present{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            image_to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            image_to_present.dstAccessMask = 0U;
            image_to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            image_to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            image_to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_to_present.image = image;
            image_to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            image_to_present.subresourceRange.baseMipLevel = 0U;
            image_to_present.subresourceRange.levelCount = 1U;
            image_to_present.subresourceRange.baseArrayLayer = 0U;
            image_to_present.subresourceRange.layerCount = 1U;
            vk.cmd_pipeline_barrier(command_buffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0U,
                0U, nullptr, 0U, nullptr, 1U, &image_to_present);
        }
    }

    result = vk.end_command_buffer(command_buffer);
    report.command_record_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkEndCommandBuffer failed: " + resultName(result));
        return false;
    }

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.waitSemaphoreCount = 1U;
    submit_info.pWaitSemaphores = &acquire_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1U;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1U;
    submit_info.pSignalSemaphores = &present_semaphore;
    const auto submit_start = std::chrono::steady_clock::now();
    result = vk.queue_submit(queue, 1U, &submit_info, VK_NULL_HANDLE);
    const auto submit_end = std::chrono::steady_clock::now();
    report.queue_submit_cpu_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            submit_end - submit_start).count());
    report.queue_submit_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkQueueSubmit for render-pass clear failed: " + resultName(result));
        return false;
    }

    VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1U;
    present_info.pWaitSemaphores = &present_semaphore;
    present_info.swapchainCount = 1U;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &image_index;
    VkSwapchainPresentFenceInfoEXT present_fence_info{
        VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT};
    present_fence_info.swapchainCount = 1U;
    present_fence_info.pFences = &present_fence;
    if (present_fence_enabled) {
        present_info.pNext = &present_fence_info;
        report.present_fence_attach_status = "success";
    } else {
        report.present_fence_attach_status = "not_supported";
    }
    const auto present_start = std::chrono::steady_clock::now();
    result = vk.queue_present(queue, &present_info);
    const auto present_end = std::chrono::steady_clock::now();
    report.present_call_cpu_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            present_end - present_start).count());
    report.image_present_status = isUsable(result) ? "success" : "failed";
    report.image_presented = isUsable(result);
    if (result == VK_SUBOPTIMAL_KHR) {
        ++report.swapchain_present_suboptimal_count;
    } else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        ++report.swapchain_present_out_of_date_count;
        report.swapchain_out_of_date_recovery_status =
            "primary_present_out_of_date_requires_session_retry";
    }
    if (!isUsable(result)) {
        fail(report, "vkQueuePresentKHR failed: " + resultName(result));
        return false;
    }
    if (present_fence_enabled) {
        ++report.present_fence_attach_count;
    }

    if (present_fence_enabled) {
        const auto fence_wait_start = std::chrono::steady_clock::now();
        result = vk.wait_for_fences(device, 1U, &present_fence, VK_TRUE,
                                     std::numeric_limits<std::uint64_t>::max());
        const auto fence_wait_end = std::chrono::steady_clock::now();
        report.present_fence_wait_cpu_us = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                fence_wait_end - fence_wait_start).count());
        report.present_fence_wait_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkWaitForFences(present) failed: " + resultName(result));
            return false;
        }
        ++report.present_fence_wait_count;
        report.present_fence_used = true;
        report.present_completion_guaranteed = true;
        report.present_completion_mode = "VK_EXT_swapchain_maintenance1";
        if (collect_frame_sample) {
            report.present_fence_wait_cpu_us_samples.push_back(
                report.present_fence_wait_cpu_us);
        }
    }

    const auto idle_start = std::chrono::steady_clock::now();
    result = vk.queue_wait_idle(queue);
    const auto idle_end = std::chrono::steady_clock::now();
    report.queue_idle_wait_cpu_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            idle_end - idle_start).count());
    report.queue_idle_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkQueueWaitIdle failed after rendering/presentation: " + resultName(result));
        return false;
    }

    if (perform_pixel_readback) {
        if (!verifyPixelReadback(vk, report, device, pixel_readback_memory,
                                 format, extent, pixel_readback_memory_coherent)) {
            if (uint8_index_probe_enabled) {
                report.index_type_uint8_probe_status = "pixel_readback_failed";
            }
            return false;
        }
        if (uint8_index_probe_enabled) {
            report.index_type_uint8_probe_status = "success";
        }
    }

    report.frame_content_written = true;
    report.frame_timing_source = "std::chrono::steady_clock";
    report.frame_total_cpu_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            idle_end - frame_start).count());
    if (collect_frame_sample) {
        report.acquire_cpu_us_samples.push_back(report.acquire_cpu_us);
        report.queue_submit_cpu_us_samples.push_back(report.queue_submit_cpu_us);
        report.present_call_cpu_us_samples.push_back(report.present_call_cpu_us);
        report.queue_idle_wait_cpu_us_samples.push_back(report.queue_idle_wait_cpu_us);
        report.frame_total_cpu_us_samples.push_back(report.frame_total_cpu_us);
        report.frame_timing_sample_count =
            static_cast<std::uint32_t>(report.frame_total_cpu_us_samples.size());
        if (timestamp_query_pool != VK_NULL_HANDLE) {
            std::uint64_t timestamps[2]{};
            result = vk.get_query_pool_results(
                device, timestamp_query_pool, timestamp_query_base, 2U,
                sizeof(timestamps), timestamps, sizeof(std::uint64_t),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            if (result != VK_SUCCESS) {
                report.gpu_timestamp_query_status = "failed";
                fail(report, "vkGetQueryPoolResults failed: " + resultName(result));
                return false;
            }
            std::uint64_t ticks = timestamps[1] - timestamps[0];
            if (report.queue_timestamp_valid_bits < 64U) {
                const std::uint64_t mask =
                    (std::uint64_t{1} << report.queue_timestamp_valid_bits) - 1U;
                ticks &= mask;
            }
            const auto elapsed_ns = static_cast<std::uint64_t>(std::llround(
                static_cast<long double>(ticks) *
                static_cast<long double>(report.timestamp_period_ns)));
            report.gpu_frame_time_ns_samples.push_back(elapsed_ns);
            report.gpu_timestamp_sample_count =
                static_cast<std::uint32_t>(report.gpu_frame_time_ns_samples.size());
        }
    }
    report.frame_timing_valid =
        report.frame_timing_sample_count == report.frame_target_count;
    return true;
}


bool runFrameSequence(
        VulkanDispatch& vk,
        AndroidSurfaceSwapchainValidationReport& report,
        VkDevice device,
        VkQueue queue,
        std::uint32_t queue_family,
        VkSwapchainKHR swapchain,
        const std::vector<VkImage>& images,
        VkFormat format,
        VkExtent2D extent,
        VkSemaphore& acquire_semaphore,
        std::vector<VkSemaphore>& present_semaphores,
        VkFence& present_fence,
        bool present_fence_enabled,
        VkQueryPool& timestamp_query_pool,
        VkBuffer pixel_readback_buffer,
        VkDeviceMemory pixel_readback_memory,
        bool pixel_readback_memory_coherent,
        VkBuffer uint8_index_buffer,
        bool uint8_index_probe_enabled,
        bool pixel_readback_enabled,
        VkCommandPool& command_pool,
        VkCommandBuffer& command_buffer,
        VkImageView& image_view,
        VkRenderPass& render_pass,
        VkFramebuffer& framebuffer,
        VkShaderModule& vertex_shader,
        VkShaderModule& fragment_shader,
        VkPipelineLayout& pipeline_layout,
        VkPipeline& graphics_pipeline) {
    if (images.empty()) {
        fail(report, "The swapchain returned no images for the frame sequence");
        return false;
    }

    VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkResult result = vk.create_semaphore(
        device, &semaphore_info, nullptr, &acquire_semaphore);
    report.semaphore_create_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "Acquire semaphore creation failed: " + resultName(result));
        return false;
    }
    // A present-wait semaphore is associated with the acquired swapchain image.
    // Reacquiring that same image is the Vulkan-guaranteed point at which its
    // previous presentation wait has completed and the semaphore can be reused.
    present_semaphores.assign(images.size(), VK_NULL_HANDLE);
    for (std::size_t image = 0U; image < images.size(); ++image) {
        result = vk.create_semaphore(
            device, &semaphore_info, nullptr, &present_semaphores[image]);
        if (result != VK_SUCCESS) {
            report.present_semaphore_create_status = "failed";
            fail(report, "Present semaphore creation failed for swapchain image " +
                         std::to_string(image) + ": " + resultName(result));
            return false;
        }
    }
    report.present_semaphore_create_status = "success";

    if (present_fence_enabled) {
        VkFenceCreateInfo present_fence_create_info{
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        result = vk.create_fence(
            device, &present_fence_create_info, nullptr, &present_fence);
        report.present_fence_create_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateFence(present) failed: " + resultName(result));
            return false;
        }
        report.present_fence_entry_points_status = "success";
        report.present_completion_mode = "VK_EXT_swapchain_maintenance1";
    } else {
        report.present_fence_create_status = "not_supported";
        report.present_fence_entry_points_status = "not_supported";
        report.present_fence_attach_status = "not_supported";
        report.present_fence_wait_status = "not_supported";
        report.present_fence_reset_status = "not_required";
        report.present_completion_mode = "per_image_reacquire_fallback";
        report.present_completion_guaranteed = false;
    }

    const bool timestamp_supported =
        report.queue_timestamp_valid_bits > 0U &&
        report.timestamp_compute_and_graphics_supported &&
        vk.create_query_pool != nullptr && vk.destroy_query_pool != nullptr &&
        vk.cmd_reset_query_pool != nullptr && vk.cmd_write_timestamp != nullptr &&
        vk.get_query_pool_results != nullptr;
    if (!timestamp_supported) {
        report.gpu_timestamp_status = "unsupported";
        report.gpu_timestamp_query_pool_create_status = "not_supported";
        report.gpu_timestamp_query_status = "not_supported";
        fail(report, "R2.0 requires timestamp queries, but the selected queue or Vulkan entry points do not support them");
        return false;
    }

    VkQueryPoolCreateInfo query_info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    query_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    query_info.queryCount = report.frame_target_count * 2U;
    result = vk.create_query_pool(device, &query_info, nullptr, &timestamp_query_pool);
    report.gpu_timestamp_query_pool_create_status =
        result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        report.gpu_timestamp_status = "query_pool_creation_failed";
        fail(report, "vkCreateQueryPool(timestamp) failed: " + resultName(result));
        return false;
    }
    report.gpu_timestamp_query_status = "pending";

    const std::uint32_t total_frames =
        report.frame_target_count + report.warmup_frame_count;
    for (std::uint32_t frame_index = 0U; frame_index < total_frames; ++frame_index) {
        const auto frame_start = std::chrono::steady_clock::now();
        std::uint32_t image_index = 0U;
        const auto acquire_start = std::chrono::steady_clock::now();
        result = vk.acquire_next_image(
            device, swapchain, 1000000000ULL,
            acquire_semaphore, VK_NULL_HANDLE, &image_index);
        const auto acquire_end = std::chrono::steady_clock::now();
        report.acquire_cpu_us = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                acquire_end - acquire_start).count());
        report.image_acquire_status = isUsable(result) ? "success" : "failed";
        if (result == VK_SUBOPTIMAL_KHR) {
            ++report.swapchain_acquire_suboptimal_count;
        } else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            ++report.swapchain_acquire_out_of_date_count;
            report.swapchain_out_of_date_recovery_status =
                "primary_acquire_out_of_date_requires_session_retry";
        }
        if (!isUsable(result)) {
            fail(report, "vkAcquireNextImageKHR failed during frame " +
                         std::to_string(frame_index) + ": " + resultName(result));
            return false;
        }
        if (image_index >= images.size()) {
            report.image_acquire_status = "failed";
            fail(report, "vkAcquireNextImageKHR returned an out-of-range image index");
            return false;
        }
        report.image_acquired = true;
        report.acquired_image_index = image_index;

        const bool collect_sample = frame_index >= report.warmup_frame_count;
        const std::uint32_t sample_index =
            collect_sample ? frame_index - report.warmup_frame_count : 0U;
        const bool frame_ok = renderAndPresentFrame(
            vk, report, device, queue, queue_family, swapchain,
            images[image_index], format, extent, image_index,
            acquire_semaphore, present_semaphores[image_index],
            command_pool, command_buffer, image_view, render_pass, framebuffer,
            vertex_shader, fragment_shader, pipeline_layout, graphics_pipeline,
            collect_sample ? timestamp_query_pool : VK_NULL_HANDLE,
            sample_index * 2U, present_fence, present_fence_enabled,
            pixel_readback_enabled, frame_index == 0U,
            pixel_readback_buffer, pixel_readback_memory,
            pixel_readback_memory_coherent, uint8_index_buffer,
            uint8_index_probe_enabled, collect_sample, frame_start);
        if (!frame_ok) return false;
        ++report.frame_completed_count;

        if (present_fence_enabled && frame_index + 1U < total_frames) {
            result = vk.reset_fences(device, 1U, &present_fence);
            report.present_fence_reset_status =
                result == VK_SUCCESS ? "success" : "failed";
            if (result != VK_SUCCESS) {
                fail(report, "vkResetFences(present) failed: " + resultName(result));
                return false;
            }
            ++report.present_fence_reset_count;
        } else if (present_fence_enabled) {
            report.present_fence_reset_status = "not_required_after_final_present";
        }

        // A framebuffer is tied to the particular swapchain image acquired above.
        if (framebuffer != VK_NULL_HANDLE) {
            vk.destroy_framebuffer(device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
            report.framebuffer_destroy_status = "success";
        }
        if (image_view != VK_NULL_HANDLE) {
            vk.destroy_image_view(device, image_view, nullptr);
            image_view = VK_NULL_HANDLE;
            report.image_view_destroy_status = "success";
        }
    }

    report.frame_timing_valid =
        report.frame_timing_sample_count == report.frame_target_count;
    if (!report.frame_timing_valid) {
        fail(report, "The multi-frame CPU timing sample count did not reach the target");
        return false;
    }
    if (report.gpu_timestamp_sample_count != report.frame_target_count) {
        report.gpu_timestamp_query_status = "incomplete";
        fail(report, "The GPU timestamp query sample count did not reach the target");
        return false;
    }
    report.gpu_timestamp_query_status = "success";
    report.gpu_timestamp_status = "success";
    if (present_fence_enabled && report.present_fence_wait_status == "success") {
        report.shutdown_presentation_completion_status = "present_fence_signaled";
        report.present_completion_guaranteed = true;
    } else {
        report.shutdown_presentation_completion_status =
            "not_guaranteed_without_present_fence";
    }
    return true;
}

void pauseForLiveResizeWindow(
        AndroidSurfaceSwapchainValidationReport& report,
        std::uint32_t pause_ms,
        bool before_present) {
    if (pause_ms == 0U) return;
    if (before_present) {
        ++report.live_resize_window_pre_present_pause_count;
    } else {
        ++report.live_resize_window_pre_acquire_pause_count;
    }
    const auto pause_start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(pause_ms));
    const auto pause_end = std::chrono::steady_clock::now();
    report.live_resize_window_pause_total_ms += static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(pause_end - pause_start).count());
}

bool recreateSwapchainAndProbe(
        VulkanDispatch& vk,
        AndroidSurfaceSwapchainValidationReport& report,
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface,
        VkDevice device,
        VkQueue queue,
        VkFormat format,
        VkColorSpaceKHR color_space,
        VkImageUsageFlags image_usage,
        VkPresentModeKHR present_mode,
        VkCompositeAlphaFlagBitsKHR preferred_composite_alpha,
        std::uint32_t requested_width,
        std::uint32_t requested_height,
        VkSwapchainKHR& swapchain,
        std::vector<VkImage>& images,
        VkExtent2D& extent,
        VkPipeline& graphics_pipeline,
        bool present_fence_enabled,
        std::uint32_t live_resize_pause_ms) {
    report.swapchain_recreation_status = "pending";
    report.swapchain_recreation_mode = "old_swapchain_handoff";
    report.swapchain_old_handle_supplied = swapchain != VK_NULL_HANDLE;
    report.swapchain_extent_before_width = extent.width;
    report.swapchain_extent_before_height = extent.height;
    report.swapchain_logical_device_reused = true;

    if (!swapchainRecoveryPolicySelfTest()) {
        report.swapchain_result_policy_status = "failed";
        fail(report, "Swapchain result recovery policy self-test failed");
        return false;
    }
    report.swapchain_result_policy_status = "success";
    bool live_resize_pre_acquire_pause_used = false;
    bool live_resize_pre_present_pause_used = false;

    for (std::uint32_t attempt = 0U; attempt < 3U; ++attempt) {
        ++report.swapchain_recreation_attempt_count;

        VkSurfaceCapabilitiesKHR capabilities{};
        VkResult result = vk.get_surface_capabilities(
            physical_device, surface, &capabilities);
        if (result != VK_SUCCESS) {
            fail(report, "Surface capabilities refresh before swapchain recreation failed: " +
                         resultName(result));
            report.swapchain_recreation_status = "capabilities_query_failed";
            return false;
        }
        recordSurfaceTransform(report, capabilities);
        if ((capabilities.supportedUsageFlags & image_usage) != image_usage) {
            fail(report, "Surface no longer supports the required swapchain image usage");
            report.swapchain_recreation_status = "usage_not_supported";
            return false;
        }

        std::uint32_t format_count = 0U;
        result = vk.get_surface_formats(physical_device, surface, &format_count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats;
        if (result == VK_SUCCESS && format_count > 0U) {
            formats.resize(format_count);
            result = vk.get_surface_formats(
                physical_device, surface, &format_count, formats.data());
        }
        if (result != VK_SUCCESS || formats.empty()) {
            fail(report, "Surface format refresh for swapchain recreation failed: " +
                         resultName(result));
            report.swapchain_recreation_status = "format_query_failed";
            return false;
        }
        const bool format_still_supported = std::any_of(
            formats.begin(), formats.end(), [format, color_space](const VkSurfaceFormatKHR& item) {
                return item.format == format && item.colorSpace == color_space;
            });
        if (!format_still_supported &&
            !(formats.size() == 1U && formats.front().format == VK_FORMAT_UNDEFINED)) {
            fail(report, "The selected surface format changed; this recovery path requires a new render-format session");
            report.swapchain_recreation_status = "format_changed_requires_session_rebuild";
            return false;
        }

        std::uint32_t mode_count = 0U;
        result = vk.get_present_modes(physical_device, surface, &mode_count, nullptr);
        std::vector<VkPresentModeKHR> modes;
        if (result == VK_SUCCESS && mode_count > 0U) {
            modes.resize(mode_count);
            result = vk.get_present_modes(
                physical_device, surface, &mode_count, modes.data());
        }
        if (result != VK_SUCCESS ||
            std::find(modes.begin(), modes.end(), present_mode) == modes.end()) {
            fail(report, "The selected present mode is no longer supported by the Surface");
            report.swapchain_recreation_status = "present_mode_changed";
            return false;
        }

        VkExtent2D next_extent{};
        if (capabilities.currentExtent.width != UINT32_MAX) {
            next_extent = capabilities.currentExtent;
        } else {
            next_extent.width = std::clamp(
                requested_width, capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width);
            next_extent.height = std::clamp(
                requested_height, capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height);
        }
        if (next_extent.width == 0U || next_extent.height == 0U) {
            fail(report, "The refreshed Surface extent is zero; swapchain recreation is deferred");
            report.swapchain_recreation_status = "zero_extent_deferred";
            return false;
        }

        std::uint32_t image_count = capabilities.minImageCount + 1U;
        if (capabilities.maxImageCount > 0U && image_count > capabilities.maxImageCount) {
            image_count = capabilities.maxImageCount;
        }

        VkCompositeAlphaFlagBitsKHR composite_alpha = preferred_composite_alpha;
        if ((capabilities.supportedCompositeAlpha & composite_alpha) == 0U) {
            const VkCompositeAlphaFlagBitsKHR candidates[] = {
                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
            };
            bool found = false;
            for (const auto candidate : candidates) {
                if ((capabilities.supportedCompositeAlpha & candidate) != 0U) {
                    composite_alpha = candidate;
                    found = true;
                    break;
                }
            }
            if (!found) {
                fail(report, "No supported composite-alpha mode remains for swapchain recreation");
                report.swapchain_recreation_status = "composite_alpha_unavailable";
                return false;
            }
        }

        VkSwapchainCreateInfoKHR create_info{
            VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        create_info.surface = surface;
        create_info.minImageCount = image_count;
        create_info.imageFormat = format;
        create_info.imageColorSpace = color_space;
        create_info.imageExtent = next_extent;
        create_info.imageArrayLayers = 1U;
        create_info.imageUsage = image_usage;
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.preTransform = chooseSwapchainPreTransform(capabilities);
        create_info.compositeAlpha = composite_alpha;
        create_info.presentMode = present_mode;
        create_info.clipped = VK_TRUE;
        create_info.oldSwapchain = swapchain;

        VkSwapchainKHR replacement = VK_NULL_HANDLE;
        result = vk.create_swapchain(device, &create_info, nullptr, &replacement);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            ++report.swapchain_create_out_of_date_count;
            ++report.swapchain_out_of_date_recovery_attempt_count;
            report.swapchain_out_of_date_recovery_status = "retrying_after_create_out_of_date";
            if (attempt + 1U < 3U) continue;
            fail(report, "vkCreateSwapchainKHR remained OUT_OF_DATE after recovery retries");
            report.swapchain_recreation_status = "out_of_date_retry_exhausted";
            return false;
        }
        if (result != VK_SUCCESS || replacement == VK_NULL_HANDLE) {
            fail(report, "Replacement vkCreateSwapchainKHR failed: " + resultName(result));
            report.swapchain_recreation_status = "create_failed";
            return false;
        }
        ++report.swapchain_recreation_success_count;

        std::uint32_t replacement_image_count = 0U;
        result = vk.get_swapchain_images(device, replacement, &replacement_image_count, nullptr);
        std::vector<VkImage> replacement_images;
        if (result == VK_SUCCESS && replacement_image_count > 0U) {
            replacement_images.resize(replacement_image_count);
            result = vk.get_swapchain_images(
                device, replacement, &replacement_image_count, replacement_images.data());
        }
        if (result != VK_SUCCESS || replacement_images.empty()) {
            vk.device_wait_idle(device);
            vk.destroy_swapchain(device, replacement, nullptr);
            fail(report, "Replacement swapchain image enumeration failed: " + resultName(result));
            report.swapchain_recreated_image_query_status = "failed";
            report.swapchain_recreation_status = "image_query_failed";
            return false;
        }

        VkSwapchainKHR retired = swapchain;
        swapchain = replacement;
        images = std::move(replacement_images);
        if (retired != VK_NULL_HANDLE) {
            result = vk.device_wait_idle(device);
            if (result != VK_SUCCESS) {
                fail(report, "vkDeviceWaitIdle failed before retiring the old swapchain: " +
                             resultName(result));
                report.swapchain_recreation_status = "device_wait_failed";
                return false;
            }
            vk.destroy_swapchain(device, retired, nullptr);
            report.swapchain_old_destroy_status = "success";
        }

        report.swapchain_recreated_image_query_status = "success";
        report.swapchain_recreated_image_count =
            static_cast<std::uint32_t>(images.size());
        report.swapchain_image_count = static_cast<std::uint32_t>(images.size());
        report.swapchain_extent_after_width = next_extent.width;
        report.swapchain_extent_after_height = next_extent.height;
        report.swapchain_extent_changed =
            report.swapchain_extent_before_width != next_extent.width ||
            report.swapchain_extent_before_height != next_extent.height;
        if (report.swapchain_extent_changed && graphics_pipeline != VK_NULL_HANDLE) {
            result = vk.device_wait_idle(device);
            if (result != VK_SUCCESS) {
                fail(report, "vkDeviceWaitIdle failed before rebuilding extent-dependent graphics pipeline");
                report.swapchain_recreation_status = "pipeline_rebuild_wait_failed";
                return false;
            }
            vk.destroy_pipeline(device, graphics_pipeline, nullptr);
            graphics_pipeline = VK_NULL_HANDLE;
            report.graphics_pipeline_destroy_status = "success";
        }
        report.extent_width = next_extent.width;
        report.extent_height = next_extent.height;
        extent = next_extent;

        VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
        VkSemaphore render_semaphore = VK_NULL_HANDLE;
        VkFence completion_fence = VK_NULL_HANDLE;
        VkCommandPool probe_pool = VK_NULL_HANDLE;
        VkCommandBuffer probe_command = VK_NULL_HANDLE;
        bool probe_ok = false;
        bool retry_out_of_date = false;
        bool retry_after_suboptimal = false;

        VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        result = vk.create_semaphore(device, &semaphore_info, nullptr, &acquire_semaphore);
        if (result == VK_SUCCESS) {
            result = vk.create_semaphore(device, &semaphore_info, nullptr, &render_semaphore);
        }
        if (result != VK_SUCCESS) {
            fail(report, "Swapchain recreation probe semaphore creation failed: " +
                         resultName(result));
            report.swapchain_recreation_probe_acquire_status = "semaphore_create_failed";
        } else {
            VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool_info.queueFamilyIndex = report.queue_family_index;
            result = vk.create_command_pool(device, &pool_info, nullptr, &probe_pool);
            if (result == VK_SUCCESS) {
                VkCommandBufferAllocateInfo allocate_info{
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocate_info.commandPool = probe_pool;
                allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocate_info.commandBufferCount = 1U;
                result = vk.allocate_command_buffers(device, &allocate_info, &probe_command);
            }
            if (result != VK_SUCCESS) {
                fail(report, "Swapchain recreation probe command allocation failed: " +
                             resultName(result));
                report.swapchain_recreation_probe_submit_status = "command_setup_failed";
            } else {
                if (present_fence_enabled) {
                    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                    result = vk.create_fence(device, &fence_info, nullptr, &completion_fence);
                }
                if (result != VK_SUCCESS) {
                    fail(report, "Swapchain recreation probe fence creation failed: " +
                                 resultName(result));
                    report.swapchain_recreation_probe_completion_status = "fence_create_failed";
                } else {
                    if (live_resize_pause_ms > 0U && !live_resize_pre_acquire_pause_used) {
                        live_resize_pre_acquire_pause_used = true;
                        pauseForLiveResizeWindow(report, live_resize_pause_ms, false);
                    }
                    std::uint32_t image_index = 0U;
                    result = vk.acquire_next_image(
                        device, swapchain, 1000000000ULL,
                        acquire_semaphore, VK_NULL_HANDLE, &image_index);
                    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                        ++report.swapchain_acquire_out_of_date_count;
                        ++report.swapchain_out_of_date_recovery_attempt_count;
                        report.swapchain_out_of_date_recovery_status = "retrying_after_acquire_out_of_date";
                        retry_out_of_date = true;
                        report.swapchain_recreation_probe_acquire_status =
                            "out_of_date_retry_requested";
                    } else if (!isUsable(result) || image_index >= images.size()) {
                        fail(report, "Replacement swapchain acquire probe failed: " +
                                     resultName(result));
                        report.swapchain_recreation_probe_acquire_status = "failed";
                    } else {
                        if (result == VK_SUBOPTIMAL_KHR) {
                            ++report.swapchain_acquire_suboptimal_count;
                            retry_after_suboptimal = true;
                        }
                        report.swapchain_recreation_probe_acquire_status = "success";
                        VkCommandBufferBeginInfo begin_info{
                            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                        result = vk.begin_command_buffer(probe_command, &begin_info);
                        if (result == VK_SUCCESS) {
                            VkImageMemoryBarrier to_present{
                                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                            to_present.srcAccessMask = 0U;
                            to_present.dstAccessMask = 0U;
                            to_present.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                            to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                            to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            to_present.image = images[image_index];
                            to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                            to_present.subresourceRange.baseMipLevel = 0U;
                            to_present.subresourceRange.levelCount = 1U;
                            to_present.subresourceRange.baseArrayLayer = 0U;
                            to_present.subresourceRange.layerCount = 1U;
                            vk.cmd_pipeline_barrier(
                                probe_command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0U,
                                0U, nullptr, 0U, nullptr, 1U, &to_present);
                            result = vk.end_command_buffer(probe_command);
                        }
                        report.swapchain_recreation_probe_submit_status =
                            result == VK_SUCCESS ? "recorded" : "failed";
                        if (result == VK_SUCCESS) {
                            const VkPipelineStageFlags wait_stage =
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                            VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
                            submit_info.waitSemaphoreCount = 1U;
                            submit_info.pWaitSemaphores = &acquire_semaphore;
                            submit_info.pWaitDstStageMask = &wait_stage;
                            submit_info.commandBufferCount = 1U;
                            submit_info.pCommandBuffers = &probe_command;
                            submit_info.signalSemaphoreCount = 1U;
                            submit_info.pSignalSemaphores = &render_semaphore;
                            result = vk.queue_submit(queue, 1U, &submit_info, VK_NULL_HANDLE);
                        }
                        if (result == VK_SUCCESS) {
                            report.swapchain_recreation_probe_submit_status = "success";
                            if (live_resize_pause_ms > 0U && !live_resize_pre_present_pause_used) {
                                live_resize_pre_present_pause_used = true;
                                pauseForLiveResizeWindow(report, live_resize_pause_ms, true);
                            }
                            VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
                            present_info.waitSemaphoreCount = 1U;
                            present_info.pWaitSemaphores = &render_semaphore;
                            present_info.swapchainCount = 1U;
                            present_info.pSwapchains = &swapchain;
                            present_info.pImageIndices = &image_index;
                            VkSwapchainPresentFenceInfoEXT fence_info{
                                VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT};
                            fence_info.swapchainCount = 1U;
                            fence_info.pFences = &completion_fence;
                            if (present_fence_enabled) present_info.pNext = &fence_info;
                            result = vk.queue_present(queue, &present_info);
                            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                                ++report.swapchain_present_out_of_date_count;
                                ++report.swapchain_out_of_date_recovery_attempt_count;
                                report.swapchain_out_of_date_recovery_status = "retrying_after_present_out_of_date";
                                retry_out_of_date = true;
                                report.swapchain_recreation_probe_present_status =
                                    "out_of_date_retry_requested";
                                vk.device_wait_idle(device);
                            } else if (!isUsable(result)) {
                                fail(report, "Replacement swapchain present probe failed: " +
                                             resultName(result));
                                report.swapchain_recreation_probe_present_status = "failed";
                                vk.device_wait_idle(device);
                            } else {
                                if (result == VK_SUBOPTIMAL_KHR) {
                                    ++report.swapchain_present_suboptimal_count;
                                    retry_after_suboptimal = true;
                                }
                                report.swapchain_recreation_probe_present_status = "success";
                                if (present_fence_enabled) {
                                    result = vk.wait_for_fences(
                                        device, 1U, &completion_fence, VK_TRUE,
                                        std::numeric_limits<std::uint64_t>::max());
                                    if (result == VK_SUCCESS) {
                                        report.swapchain_recreation_probe_completion_status =
                                            "present_fence_signaled";
                                    } else {
                                        fail(report, "Replacement swapchain present-fence wait failed: " +
                                                     resultName(result));
                                        report.swapchain_recreation_probe_completion_status = "failed";
                                    }
                                } else {
                                    result = vk.queue_wait_idle(queue);
                                    if (result == VK_SUCCESS) {
                                        report.swapchain_recreation_probe_completion_status =
                                            "queue_idle_fallback";
                                    } else {
                                        fail(report, "Queue idle failed after replacement swapchain probe: " +
                                                     resultName(result));
                                        report.swapchain_recreation_probe_completion_status = "failed";
                                    }
                                }
                                if (result == VK_SUCCESS) {
                                    result = vk.queue_wait_idle(queue);
                                    probe_ok = result == VK_SUCCESS;
                                    if (!probe_ok) {
                                        fail(report, "Queue idle failed after replacement swapchain presentation");
                                    }
                                }
                            }
                        } else {
                            fail(report, "Replacement swapchain probe queue submit failed: " +
                                         resultName(result));
                            report.swapchain_recreation_probe_submit_status = "failed";
                        }
                    }
                }
            }
        }

        if (probe_pool != VK_NULL_HANDLE) {
            vk.destroy_command_pool(device, probe_pool, nullptr);
        }
        if (acquire_semaphore != VK_NULL_HANDLE) {
            vk.destroy_semaphore(device, acquire_semaphore, nullptr);
        }
        if (render_semaphore != VK_NULL_HANDLE) {
            vk.destroy_semaphore(device, render_semaphore, nullptr);
        }
        if (completion_fence != VK_NULL_HANDLE) {
            vk.destroy_fence(device, completion_fence, nullptr);
        }

        if (probe_ok) {
            ++report.swapchain_recreation_probe_present_count;
            report.swapchain_recreation_probe_rendered = false;
            if (retry_after_suboptimal && attempt + 1U < 3U) {
                continue;
            }
            if (retry_after_suboptimal) {
                fail(report, "Replacement swapchain remained SUBOPTIMAL after recreation retries");
                report.swapchain_recreation_status = "suboptimal_retry_exhausted";
                return false;
            }
            if (report.swapchain_out_of_date_recovery_attempt_count > 0U) {
                ++report.swapchain_out_of_date_recovery_success_count;
            }
            report.swapchain_recreation_status = "success";
            report.swapchain_out_of_date_recovery_status =
                report.swapchain_out_of_date_recovery_attempt_count > 0U
                    ? "recovered" : "not_observed";
            return true;
        }
        if ((retry_out_of_date || retry_after_suboptimal) && attempt + 1U < 3U) {
            continue;
        }
        if (retry_out_of_date) {
            fail(report, "Replacement swapchain remained OUT_OF_DATE after recovery retries");
            report.swapchain_recreation_status = "out_of_date_retry_exhausted";
            return false;
        }
        if (retry_after_suboptimal) {
            fail(report, "Replacement swapchain remained SUBOPTIMAL after recovery retries");
            report.swapchain_recreation_status = "suboptimal_retry_exhausted";
            return false;
        }
        report.swapchain_recreation_status = "probe_failed";
        return false;
    }

    report.swapchain_recreation_status = "out_of_date_retry_exhausted";
    return false;
}

AndroidSurfaceSwapchainValidationReport runValidation(
        JNIEnv* env, jobject java_surface, jint requested_width, jint requested_height,
        jint live_resize_window_mode) {
    AndroidSurfaceSwapchainValidationReport report;
    report.live_resize_window_mode_enabled = live_resize_window_mode != 0;
    report.live_resize_window_pause_ms_per_stage =
        report.live_resize_window_mode_enabled ? 2000U : 0U;
    VulkanDispatch vk;
    ANativeWindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;
    VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
    std::vector<VkSemaphore> present_semaphores;
    VkFence present_fence = VK_NULL_HANDLE;
    VkQueryPool timestamp_query_pool = VK_NULL_HANDLE;
    VkBuffer pixel_readback_buffer = VK_NULL_HANDLE;
    VkDeviceMemory pixel_readback_memory = VK_NULL_HANDLE;
    VkBuffer uint8_index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory uint8_index_memory = VK_NULL_HANDLE;
    bool index_type_uint8_feature_enabled = false;
    bool pixel_readback_memory_coherent = false;
    bool maintenance_instance_extensions_enabled = false;
    bool maintenance_device_extension_supported = false;
    bool maintenance_feature_supported = false;
    std::uint32_t loader_api_version = VK_API_VERSION_1_0;
    std::uint32_t instance_api_version = VK_API_VERSION_1_0;
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
        LOAD_GLOBAL(enumerate_instance_version, "vkEnumerateInstanceVersion");
        LOAD_GLOBAL(create_instance, "vkCreateInstance");
#undef LOAD_GLOBAL
        if (vk.enumerate_instance_extensions == nullptr || vk.create_instance == nullptr) {
            fail(report, "Required Vulkan global entry points are unavailable");
        } else if (vk.enumerate_instance_version != nullptr) {
            std::uint32_t version = VK_API_VERSION_1_0;
            const VkResult version_result = vk.enumerate_instance_version(&version);
            if (version_result == VK_SUCCESS) loader_api_version = version;
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
            const bool surface_maintenance_available =
                hasInstanceExtension(extensions, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            const bool surface_capabilities2_available =
                hasInstanceExtension(extensions, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            const bool properties2_available =
                hasInstanceExtension(extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            const bool core_features2_available =
                VK_VERSION_MAJOR(loader_api_version) > 1U ||
                VK_VERSION_MINOR(loader_api_version) >= 1U;
            const bool features2_available =
                core_features2_available || properties2_available;
            maintenance_instance_extensions_enabled =
                surface_maintenance_available && surface_capabilities2_available &&
                features2_available;
            report.swapchain_maintenance_instance_extension_status =
                maintenance_instance_extensions_enabled
                    ? "available_for_enablement"
                    : "unsupported_or_dependency_missing";
            if (report.surface_extension_status != "success" ||
                report.android_surface_extension_status != "success") {
                fail(report, "Required VK_KHR_surface or VK_KHR_android_surface extension is missing");
            }
        }
    }

    if (report.failure_reason.empty()) {
        std::vector<const char*> instance_extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
        };
        const bool properties2_available =
            maintenance_instance_extensions_enabled &&
            VK_VERSION_MAJOR(loader_api_version) == 1U &&
            VK_VERSION_MINOR(loader_api_version) < 1U;
        if (maintenance_instance_extensions_enabled) {
            instance_extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            instance_extensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            if (properties2_available) {
                instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            }
        }
        VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app_info.pApplicationName = "NYAMIST G925 R2.3";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 9, 0);
        app_info.pEngineName = "CHORD OF NYAMIST";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        // Request the highest core version exposed by the public Android
        // loader, capped at 1.4. The physical-device version is queried
        // separately; a 1.4 loader cannot upgrade a 1.3 GPU driver.
        if (versionAtLeast(loader_api_version, 1U, 4U)) {
            instance_api_version = VK_MAKE_VERSION(1, 4, 0);
        } else if (versionAtLeast(loader_api_version, 1U, 3U)) {
            instance_api_version = VK_MAKE_VERSION(1, 3, 0);
        } else if (versionAtLeast(loader_api_version, 1U, 2U)) {
            instance_api_version = VK_MAKE_VERSION(1, 2, 0);
        } else if (versionAtLeast(loader_api_version, 1U, 1U)) {
            instance_api_version = VK_API_VERSION_1_1;
        } else {
            instance_api_version = VK_API_VERSION_1_0;
        }
        app_info.apiVersion = instance_api_version;
        report.loader_api_version = versionString(loader_api_version);
        report.instance_api_version = versionString(instance_api_version);

        VkInstanceCreateInfo create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        create_info.pApplicationInfo = &app_info;
        create_info.enabledExtensionCount =
            static_cast<std::uint32_t>(instance_extensions.size());
        create_info.ppEnabledExtensionNames = instance_extensions.data();

        const VkResult result = vk.create_instance(&create_info, nullptr, &instance);
        report.instance_create_status = isUsable(result) ? "success" : "failed";
        if (!isUsable(result)) {
            fail(report, "vkCreateInstance failed: " + resultName(result));
        } else if (maintenance_instance_extensions_enabled) {
            report.swapchain_maintenance_instance_extension_status = "enabled";
        }
    }

    if (instance != VK_NULL_HANDLE) {
#define LOAD_INSTANCE(member, symbol) \
        vk.member = reinterpret_cast<decltype(vk.member)>(vk.get_instance_proc_addr(instance, symbol))
        LOAD_INSTANCE(destroy_instance, "vkDestroyInstance");
        LOAD_INSTANCE(enumerate_physical_devices, "vkEnumeratePhysicalDevices");
        LOAD_INSTANCE(get_physical_device_properties, "vkGetPhysicalDeviceProperties");
        LOAD_INSTANCE(get_physical_device_properties2, "vkGetPhysicalDeviceProperties2");
        if (vk.get_physical_device_properties2 == nullptr) {
            vk.get_physical_device_properties2 =
                reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
                    vk.get_instance_proc_addr(instance, "vkGetPhysicalDeviceProperties2KHR"));
        }
        LOAD_INSTANCE(get_physical_device_memory_properties, "vkGetPhysicalDeviceMemoryProperties");
        LOAD_INSTANCE(get_physical_device_features2, "vkGetPhysicalDeviceFeatures2");
        if (vk.get_physical_device_features2 == nullptr) {
            vk.get_physical_device_features2 =
                reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
                    vk.get_instance_proc_addr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
        }
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
    std::uint32_t selected_queue_timestamp_valid_bits = 0U;
    VkPhysicalDevice selected_physical_device = VK_NULL_HANDLE;
    std::vector<VkExtensionProperties> selected_physical_device_extensions;
    bool selected_physical_device_extensions_enumerated = false;
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
                        selected_queue_timestamp_valid_bits = families[index].timestampValidBits;
                        selected_physical_device_extensions = device_extensions;
                        selected_physical_device_extensions_enumerated = true;
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

                // R2.4 inventories promoted Vulkan 1.4 extension names.
                // On pre-1.4 devices this records advertisement only, not
                // feature or limit compliance with Vulkan 1.4 core.
                report.vulkan_1_4_promoted_extension_total_count =
                    static_cast<std::uint32_t>(kVulkan14PromotedExtensions.size());
                if (versionAtLeast(properties.apiVersion, 1U, 4U)) {
                    report.vulkan_1_4_promoted_extension_inventory_status =
                        "not_applicable_core_api_1_4_or_higher";
                    report.vulkan_1_4_promoted_extension_inventory_scope =
                        "promoted_functionality_is_in_core_api";
                    report.vulkan_1_4_promoted_extension_coverage_status = "core_api";
                } else if (selected_physical_device_extensions_enumerated) {
                    report.vulkan_1_4_promoted_extension_inventory_status = "success";
                    report.vulkan_1_4_promoted_extension_inventory_scope =
                        "extension_name_advertisement_on_pre_1_4_device";
                    for (const char* extension_name : kVulkan14PromotedExtensions) {
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               extension_name)) {
                            report.vulkan_1_4_promoted_extensions_supported.emplace_back(
                                extension_name);
                        } else {
                            report.vulkan_1_4_promoted_extensions_not_advertised.emplace_back(
                                extension_name);
                        }
                    }
                    report.vulkan_1_4_promoted_extension_supported_count =
                        static_cast<std::uint32_t>(
                            report.vulkan_1_4_promoted_extensions_supported.size());
                    report.vulkan_1_4_promoted_extension_not_advertised_count =
                        static_cast<std::uint32_t>(
                            report.vulkan_1_4_promoted_extensions_not_advertised.size());
                    if (report.vulkan_1_4_promoted_extension_supported_count == 0U) {
                        report.vulkan_1_4_promoted_extension_coverage_status = "none_advertised";
                    } else if (report.vulkan_1_4_promoted_extension_not_advertised_count == 0U) {
                        report.vulkan_1_4_promoted_extension_coverage_status = "all_advertised";
                    } else {
                        report.vulkan_1_4_promoted_extension_coverage_status =
                            "partial_advertisement";
                    }
                } else {
                    report.vulkan_1_4_promoted_extension_inventory_status = "enumeration_failed";
                    report.vulkan_1_4_promoted_extension_inventory_scope =
                        "extension_name_advertisement_on_pre_1_4_device";
                    report.vulkan_1_4_promoted_extension_coverage_status = "unknown";
                }

                // Query driver identity and the physical-device UUID through
                // the public Vulkan properties2 API, not private driver APIs.
                report.physical_device_properties2_query_status =
                    vk.get_physical_device_properties2 != nullptr
                        ? "available" : "entry_point_unavailable";
                if (vk.get_physical_device_properties2 != nullptr) {
                    VkPhysicalDeviceIDProperties id_properties{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
                    VkPhysicalDeviceProperties2 properties2{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
                    if (versionAtLeast(properties.apiVersion, 1U, 2U)) {
                        VkPhysicalDeviceDriverProperties driver_properties{
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
                        driver_properties.pNext = &id_properties;
                        properties2.pNext = &driver_properties;
                        vk.get_physical_device_properties2(
                            selected_physical_device, &properties2);
                        report.driver_properties_query_status = "success";
                        report.driver_id = static_cast<std::uint32_t>(
                            driver_properties.driverID);
                        report.driver_id_name = driverIdName(report.driver_id);
                        report.driver_name = driver_properties.driverName;
                        report.driver_info = driver_properties.driverInfo;
                        report.driver_version_raw = properties.driverVersion;
                        report.driver_version_hex = hex32(properties.driverVersion);
                        report.driver_conformance_version =
                            conformanceVersionString(driver_properties.conformanceVersion);
                        report.physical_device_uuid_hex =
                            uuidHex(id_properties.deviceUUID, VK_UUID_SIZE);
                    } else if (versionAtLeast(properties.apiVersion, 1U, 1U)) {
                        properties2.pNext = &id_properties;
                        vk.get_physical_device_properties2(
                            selected_physical_device, &properties2);
                        report.driver_properties_query_status =
                            "not_core_before_vulkan_1_2";
                        report.physical_device_uuid_hex =
                            uuidHex(id_properties.deviceUUID, VK_UUID_SIZE);
                    } else {
                        report.driver_properties_query_status =
                            "not_supported_by_physical_device_api";
                    }
                } else {
                    report.driver_properties_query_status = "unavailable";
                }

                report.vulkan_1_4_device_api_supported =
                    versionAtLeast(properties.apiVersion, 1U, 4U);
                if (!report.vulkan_1_4_device_api_supported) {
                    report.vulkan_1_4_support_status =
                        "not_exposed_by_physical_device";
                    report.vulkan_1_4_features_query_status =
                        "skipped_physical_device_api_below_1_4";
                    report.vulkan_1_4_required_features_status = "not_exposed";
                } else if (!versionAtLeast(instance_api_version, 1U, 4U)) {
                    report.vulkan_1_4_support_status = "device_api_1_4_instance_api_too_low";
                    report.vulkan_1_4_features_query_status =
                        "skipped_instance_api_below_1_4";
                } else if (vk.get_physical_device_features2 == nullptr) {
                    report.vulkan_1_4_support_status = "device_api_1_4_features2_unavailable";
                    report.vulkan_1_4_features_query_status = "entry_point_unavailable";
                } else {
                    VkPhysicalDeviceVulkan14Features features14{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
                    VkPhysicalDeviceVulkan11Features features11{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
                    VkPhysicalDeviceFeatures2 features2{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
                    features2.pNext = &features11;
                    features11.pNext = &features14;
                    vk.get_physical_device_features2(selected_physical_device, &features2);
                    report.vulkan_1_4_features_query_status = "success";
                    report.vulkan_1_4_bresenham_lines_supported =
                        features14.bresenhamLines == VK_TRUE;
                    report.vulkan_1_4_shader_subgroup_rotate_clustered_supported =
                        features14.shaderSubgroupRotateClustered == VK_TRUE;
                    report.vulkan_1_4_push_descriptor_supported =
                        features14.pushDescriptor == VK_TRUE;
                    report.vulkan_1_4_protected_memory_supported =
                        features11.protectedMemory == VK_TRUE;
                    report.vulkan_1_4_pipeline_protected_access_required =
                        report.vulkan_1_4_protected_memory_supported;
                    report.vulkan_1_4_pipeline_protected_access_supported =
                        features14.pipelineProtectedAccess == VK_TRUE;
                    report.vulkan_1_4_required_features_supported =
                        report.vulkan_1_4_bresenham_lines_supported &&
                        report.vulkan_1_4_shader_subgroup_rotate_clustered_supported &&
                        report.vulkan_1_4_push_descriptor_supported &&
                        (!report.vulkan_1_4_pipeline_protected_access_required ||
                         report.vulkan_1_4_pipeline_protected_access_supported);
                    report.vulkan_1_4_required_features_status =
                        report.vulkan_1_4_required_features_supported
                            ? "required_feature_sample_pass"
                            : "required_feature_sample_failed";
                    report.vulkan_1_4_support_status =
                        report.vulkan_1_4_required_features_supported
                            ? "api_1_4_exposed_feature_sample_pass"
                            : "api_1_4_exposed_feature_sample_failed";
                }

                // R2.5 queries individual promoted feature structures only
                // when their extension is advertised on this selected device.
                // A pre-1.4 physical device must never receive the Vulkan 1.4
                // aggregate feature structure in its Features2 pNext chain.
                report.vulkan_1_4_promoted_feature_total_count = 21U;
                report.vulkan_1_4_promoted_features_not_applicable = {
                    "VK_KHR_load_store_op_none (no feature bit)",
                    "VK_KHR_map_memory2 (no feature bit)"
                };

                auto record_promoted_feature = [&](const char* name, VkBool32 supported) {
                    ++report.vulkan_1_4_promoted_feature_queried_count;
                    if (supported == VK_TRUE) {
                        report.vulkan_1_4_promoted_features_supported.emplace_back(name);
                        ++report.vulkan_1_4_promoted_feature_supported_count;
                    } else {
                        report.vulkan_1_4_promoted_features_not_supported.emplace_back(name);
                        ++report.vulkan_1_4_promoted_feature_not_supported_count;
                    }
                };
                auto record_not_advertised = [&](std::initializer_list<const char*> names) {
                    for (const char* name : names) {
                        report.vulkan_1_4_promoted_features_not_advertised.emplace_back(name);
                        ++report.vulkan_1_4_promoted_feature_not_advertised_count;
                    }
                };
                auto record_query_failed = [&](std::initializer_list<const char*> names) {
                    for (const char* name : names) {
                        report.vulkan_1_4_promoted_features_query_failed.emplace_back(name);
                        ++report.vulkan_1_4_promoted_feature_query_failed_count;
                    }
                };
                auto record_not_queryable = [&](std::initializer_list<const char*> names) {
                    for (const char* name : names) {
                        report.vulkan_1_4_promoted_features_not_queryable.emplace_back(name);
                        ++report.vulkan_1_4_promoted_feature_not_queryable_count;
                    }
                };

                if (versionAtLeast(properties.apiVersion, 1U, 4U)) {
                    report.vulkan_1_4_promoted_feature_query_scope =
                        "vulkan_1_4_core_features2_on_api_1_4_device";
                    if (!versionAtLeast(instance_api_version, 1U, 4U)) {
                        record_not_queryable({
                            "VK_KHR_global_priority.globalPriorityQuery",
                            "VK_KHR_shader_subgroup_rotate.shaderSubgroupRotate",
                            "VK_KHR_shader_subgroup_rotate.shaderSubgroupRotateClustered",
                            "VK_KHR_shader_float_controls2.shaderFloatControls2",
                            "VK_KHR_shader_expect_assume.shaderExpectAssume",
                            "VK_KHR_line_rasterization.rectangularLines",
                            "VK_KHR_line_rasterization.bresenhamLines",
                            "VK_KHR_line_rasterization.smoothLines",
                            "VK_KHR_line_rasterization.stippledRectangularLines",
                            "VK_KHR_line_rasterization.stippledBresenhamLines",
                            "VK_KHR_line_rasterization.stippledSmoothLines",
                            "VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateDivisor",
                            "VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateZeroDivisor",
                            "VK_KHR_index_type_uint8.indexTypeUint8",
                            "VK_KHR_dynamic_rendering_local_read.dynamicRenderingLocalRead",
                            "VK_KHR_maintenance5.maintenance5",
                            "VK_KHR_maintenance6.maintenance6",
                            "VK_EXT_pipeline_protected_access.pipelineProtectedAccess",
                            "VK_EXT_pipeline_robustness.pipelineRobustness",
                            "VK_EXT_host_image_copy.hostImageCopy",
                            "VK_KHR_push_descriptor.pushDescriptor"
                        });
                        report.vulkan_1_4_promoted_feature_query_status =
                            "skipped_instance_api_below_1_4";
                    } else if (vk.get_physical_device_features2 == nullptr) {
                        record_query_failed({
                            "VK_KHR_global_priority.globalPriorityQuery",
                            "VK_KHR_shader_subgroup_rotate.shaderSubgroupRotate",
                            "VK_KHR_shader_subgroup_rotate.shaderSubgroupRotateClustered",
                            "VK_KHR_shader_float_controls2.shaderFloatControls2",
                            "VK_KHR_shader_expect_assume.shaderExpectAssume",
                            "VK_KHR_line_rasterization.rectangularLines",
                            "VK_KHR_line_rasterization.bresenhamLines",
                            "VK_KHR_line_rasterization.smoothLines",
                            "VK_KHR_line_rasterization.stippledRectangularLines",
                            "VK_KHR_line_rasterization.stippledBresenhamLines",
                            "VK_KHR_line_rasterization.stippledSmoothLines",
                            "VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateDivisor",
                            "VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateZeroDivisor",
                            "VK_KHR_index_type_uint8.indexTypeUint8",
                            "VK_KHR_dynamic_rendering_local_read.dynamicRenderingLocalRead",
                            "VK_KHR_maintenance5.maintenance5",
                            "VK_KHR_maintenance6.maintenance6",
                            "VK_EXT_pipeline_protected_access.pipelineProtectedAccess",
                            "VK_EXT_pipeline_robustness.pipelineRobustness",
                            "VK_EXT_host_image_copy.hostImageCopy",
                            "VK_KHR_push_descriptor.pushDescriptor"
                        });
                        report.vulkan_1_4_promoted_feature_query_status = "entry_point_unavailable";
                    } else {
                        VkPhysicalDeviceVulkan14Features features14{
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
                        VkPhysicalDeviceFeatures2 features2{
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
                        features2.pNext = &features14;
                        vk.get_physical_device_features2(selected_physical_device, &features2);
                        report.vulkan_1_4_promoted_feature_query_status = "success";
                        record_promoted_feature("VK_KHR_global_priority.globalPriorityQuery",
                                                features14.globalPriorityQuery);
                        record_promoted_feature("VK_KHR_shader_subgroup_rotate.shaderSubgroupRotate",
                                                features14.shaderSubgroupRotate);
                        record_promoted_feature("VK_KHR_shader_subgroup_rotate.shaderSubgroupRotateClustered",
                                                features14.shaderSubgroupRotateClustered);
                        record_promoted_feature("VK_KHR_shader_float_controls2.shaderFloatControls2",
                                                features14.shaderFloatControls2);
                        record_promoted_feature("VK_KHR_shader_expect_assume.shaderExpectAssume",
                                                features14.shaderExpectAssume);
                        record_promoted_feature("VK_KHR_line_rasterization.rectangularLines",
                                                features14.rectangularLines);
                        record_promoted_feature("VK_KHR_line_rasterization.bresenhamLines",
                                                features14.bresenhamLines);
                        record_promoted_feature("VK_KHR_line_rasterization.smoothLines",
                                                features14.smoothLines);
                        record_promoted_feature("VK_KHR_line_rasterization.stippledRectangularLines",
                                                features14.stippledRectangularLines);
                        record_promoted_feature("VK_KHR_line_rasterization.stippledBresenhamLines",
                                                features14.stippledBresenhamLines);
                        record_promoted_feature("VK_KHR_line_rasterization.stippledSmoothLines",
                                                features14.stippledSmoothLines);
                        record_promoted_feature("VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateDivisor",
                                                features14.vertexAttributeInstanceRateDivisor);
                        record_promoted_feature("VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateZeroDivisor",
                                                features14.vertexAttributeInstanceRateZeroDivisor);
                        record_promoted_feature("VK_KHR_index_type_uint8.indexTypeUint8",
                                                features14.indexTypeUint8);
                        record_promoted_feature("VK_KHR_dynamic_rendering_local_read.dynamicRenderingLocalRead",
                                                features14.dynamicRenderingLocalRead);
                        record_promoted_feature("VK_KHR_maintenance5.maintenance5",
                                                features14.maintenance5);
                        record_promoted_feature("VK_KHR_maintenance6.maintenance6",
                                                features14.maintenance6);
                        record_promoted_feature("VK_EXT_pipeline_protected_access.pipelineProtectedAccess",
                                                features14.pipelineProtectedAccess);
                        record_promoted_feature("VK_EXT_pipeline_robustness.pipelineRobustness",
                                                features14.pipelineRobustness);
                        record_promoted_feature("VK_EXT_host_image_copy.hostImageCopy",
                                                features14.hostImageCopy);
                        record_promoted_feature("VK_KHR_push_descriptor.pushDescriptor",
                                                features14.pushDescriptor);
                    }
                } else {
                    report.vulkan_1_4_promoted_feature_query_scope =
                        "advertised_extension_feature_structs_on_pre_1_4_device";
                    // VK_KHR_push_descriptor did not define an extension
                    // feature bit before Vulkan 1.4. Its advertised extension
                    // implicitly guarantees push-descriptor support; Vulkan 1.4
                    // adds an explicit feature bit for the same functionality.
                    if (hasDeviceExtension(selected_physical_device_extensions,
                                           "VK_KHR_push_descriptor")) {
                        report.vulkan_1_4_promoted_features_implicitly_supported.emplace_back(
                            "VK_KHR_push_descriptor.pushDescriptor (implicit via VK_KHR_push_descriptor)");
                        ++report.vulkan_1_4_promoted_feature_implicitly_supported_count;
                    } else {
                        record_not_advertised({"VK_KHR_push_descriptor.pushDescriptor"});
                    }

                    VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR dynamic_local_read{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR};
                    VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR global_priority{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_KHR};
                    VkPhysicalDeviceIndexTypeUint8FeaturesKHR index_type_uint8{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_KHR};
                    VkPhysicalDeviceLineRasterizationFeaturesKHR line_rasterization{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_KHR};
                    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR};
                    VkPhysicalDeviceMaintenance6FeaturesKHR maintenance6{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR};
                    VkPhysicalDeviceShaderExpectAssumeFeaturesKHR shader_expect_assume{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EXPECT_ASSUME_FEATURES_KHR};
                    VkPhysicalDeviceShaderFloatControls2FeaturesKHR shader_float_controls2{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT_CONTROLS_2_FEATURES_KHR};
                    VkPhysicalDeviceShaderSubgroupRotateFeaturesKHR shader_subgroup_rotate{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_ROTATE_FEATURES_KHR};
                    VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR vertex_attribute_divisor{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_KHR};
                    VkPhysicalDeviceHostImageCopyFeaturesEXT host_image_copy{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT};
                    VkPhysicalDevicePipelineProtectedAccessFeaturesEXT pipeline_protected_access{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROTECTED_ACCESS_FEATURES_EXT};
                    VkPhysicalDevicePipelineRobustnessFeaturesEXT pipeline_robustness{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES_EXT};

                    VkPhysicalDeviceFeatures2 promoted_features2{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
                    void** feature_tail = &promoted_features2.pNext;
                    bool any_promoted_feature_struct_queued = false;
                    auto append_if_advertised = [&](const char* extension_name,
                                                    auto& feature_struct,
                                                    std::initializer_list<const char*> feature_names) {
                        if (!hasDeviceExtension(selected_physical_device_extensions,
                                                extension_name)) {
                            record_not_advertised(feature_names);
                            return;
                        }
                        if (vk.get_physical_device_features2 == nullptr) {
                            record_query_failed(feature_names);
                            return;
                        }
                        feature_struct.pNext = nullptr;
                        *feature_tail = &feature_struct;
                        feature_tail = &feature_struct.pNext;
                        any_promoted_feature_struct_queued = true;
                    };

                    append_if_advertised("VK_KHR_dynamic_rendering_local_read",
                        dynamic_local_read, {
                            "VK_KHR_dynamic_rendering_local_read.dynamicRenderingLocalRead"});
                    append_if_advertised("VK_KHR_global_priority",
                        global_priority, {
                            "VK_KHR_global_priority.globalPriorityQuery"});
                    append_if_advertised("VK_KHR_index_type_uint8",
                        index_type_uint8, {
                            "VK_KHR_index_type_uint8.indexTypeUint8"});
                    append_if_advertised("VK_KHR_line_rasterization",
                        line_rasterization, {
                            "VK_KHR_line_rasterization.rectangularLines",
                            "VK_KHR_line_rasterization.bresenhamLines",
                            "VK_KHR_line_rasterization.smoothLines",
                            "VK_KHR_line_rasterization.stippledRectangularLines",
                            "VK_KHR_line_rasterization.stippledBresenhamLines",
                            "VK_KHR_line_rasterization.stippledSmoothLines"});
                    append_if_advertised("VK_KHR_maintenance5",
                        maintenance5, {"VK_KHR_maintenance5.maintenance5"});
                    append_if_advertised("VK_KHR_maintenance6",
                        maintenance6, {"VK_KHR_maintenance6.maintenance6"});
                    append_if_advertised("VK_KHR_shader_expect_assume",
                        shader_expect_assume, {"VK_KHR_shader_expect_assume.shaderExpectAssume"});
                    append_if_advertised("VK_KHR_shader_float_controls2",
                        shader_float_controls2, {"VK_KHR_shader_float_controls2.shaderFloatControls2"});
                    append_if_advertised("VK_KHR_shader_subgroup_rotate",
                        shader_subgroup_rotate, {
                            "VK_KHR_shader_subgroup_rotate.shaderSubgroupRotate",
                            "VK_KHR_shader_subgroup_rotate.shaderSubgroupRotateClustered"});
                    append_if_advertised("VK_KHR_vertex_attribute_divisor",
                        vertex_attribute_divisor, {
                            "VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateDivisor",
                            "VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateZeroDivisor"});
                    append_if_advertised("VK_EXT_host_image_copy",
                        host_image_copy, {"VK_EXT_host_image_copy.hostImageCopy"});
                    append_if_advertised("VK_EXT_pipeline_protected_access",
                        pipeline_protected_access, {
                            "VK_EXT_pipeline_protected_access.pipelineProtectedAccess"});
                    append_if_advertised("VK_EXT_pipeline_robustness",
                        pipeline_robustness, {"VK_EXT_pipeline_robustness.pipelineRobustness"});

                    if (any_promoted_feature_struct_queued) {
                        *feature_tail = nullptr;
                        vk.get_physical_device_features2(selected_physical_device,
                                                         &promoted_features2);
                        report.vulkan_1_4_promoted_feature_query_status = "success";
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_KHR_dynamic_rendering_local_read"))
                            record_promoted_feature(
                                "VK_KHR_dynamic_rendering_local_read.dynamicRenderingLocalRead",
                                dynamic_local_read.dynamicRenderingLocalRead);
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_KHR_global_priority"))
                            record_promoted_feature("VK_KHR_global_priority.globalPriorityQuery",
                                                    global_priority.globalPriorityQuery);
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_KHR_index_type_uint8"))
                            record_promoted_feature("VK_KHR_index_type_uint8.indexTypeUint8",
                                                    index_type_uint8.indexTypeUint8);
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_KHR_line_rasterization")) {
                            record_promoted_feature("VK_KHR_line_rasterization.rectangularLines",
                                                    line_rasterization.rectangularLines);
                            record_promoted_feature("VK_KHR_line_rasterization.bresenhamLines",
                                                    line_rasterization.bresenhamLines);
                            record_promoted_feature("VK_KHR_line_rasterization.smoothLines",
                                                    line_rasterization.smoothLines);
                            record_promoted_feature("VK_KHR_line_rasterization.stippledRectangularLines",
                                                    line_rasterization.stippledRectangularLines);
                            record_promoted_feature("VK_KHR_line_rasterization.stippledBresenhamLines",
                                                    line_rasterization.stippledBresenhamLines);
                            record_promoted_feature("VK_KHR_line_rasterization.stippledSmoothLines",
                                                    line_rasterization.stippledSmoothLines);
                        }
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_KHR_maintenance5"))
                            record_promoted_feature("VK_KHR_maintenance5.maintenance5",
                                                    maintenance5.maintenance5);
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_KHR_maintenance6"))
                            record_promoted_feature("VK_KHR_maintenance6.maintenance6",
                                                    maintenance6.maintenance6);
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_KHR_shader_expect_assume"))
                            record_promoted_feature("VK_KHR_shader_expect_assume.shaderExpectAssume",
                                                    shader_expect_assume.shaderExpectAssume);
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_KHR_shader_float_controls2"))
                            record_promoted_feature("VK_KHR_shader_float_controls2.shaderFloatControls2",
                                                    shader_float_controls2.shaderFloatControls2);
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_KHR_shader_subgroup_rotate")) {
                            record_promoted_feature("VK_KHR_shader_subgroup_rotate.shaderSubgroupRotate",
                                                    shader_subgroup_rotate.shaderSubgroupRotate);
                            record_promoted_feature(
                                "VK_KHR_shader_subgroup_rotate.shaderSubgroupRotateClustered",
                                shader_subgroup_rotate.shaderSubgroupRotateClustered);
                        }
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_KHR_vertex_attribute_divisor")) {
                            record_promoted_feature(
                                "VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateDivisor",
                                vertex_attribute_divisor.vertexAttributeInstanceRateDivisor);
                            record_promoted_feature(
                                "VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateZeroDivisor",
                                vertex_attribute_divisor.vertexAttributeInstanceRateZeroDivisor);
                        }
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_EXT_host_image_copy"))
                            record_promoted_feature("VK_EXT_host_image_copy.hostImageCopy",
                                                    host_image_copy.hostImageCopy);
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_EXT_pipeline_protected_access"))
                            record_promoted_feature(
                                "VK_EXT_pipeline_protected_access.pipelineProtectedAccess",
                                pipeline_protected_access.pipelineProtectedAccess);
                        if (hasDeviceExtension(selected_physical_device_extensions,
                                               "VK_EXT_pipeline_robustness"))
                            record_promoted_feature("VK_EXT_pipeline_robustness.pipelineRobustness",
                                                    pipeline_robustness.pipelineRobustness);
                    } else {
                        report.vulkan_1_4_promoted_feature_query_status =
                            report.vulkan_1_4_promoted_feature_query_failed_count > 0U
                                ? "entry_point_unavailable"
                                : "no_advertised_queryable_feature_structs";
                    }
                }

                report.queue_timestamp_valid_bits = selected_queue_timestamp_valid_bits;
                report.timestamp_compute_and_graphics_supported =
                    properties.limits.timestampComputeAndGraphics == VK_TRUE;
                report.timestamp_period_ns = properties.limits.timestampPeriod;
                report.gpu_timestamp_status =
                    (selected_queue_timestamp_valid_bits > 0U &&
                     report.timestamp_compute_and_graphics_supported)
                        ? "available_not_queried" : "unsupported";
                const std::string gpu_name = report.device_name;
                report.target_gpu_match = gpu_name.find("G925") != std::string::npos ||
                                          gpu_name.find("g925") != std::string::npos;
                if (!report.target_gpu_match) {
                    fail(report, "The selected physical device is not identified as Immortalis-G925");
                }

                std::uint32_t maintenance_extension_count = 0U;
                VkResult maintenance_result = vk.enumerate_device_extensions(
                    selected_physical_device, nullptr, &maintenance_extension_count, nullptr);
                std::vector<VkExtensionProperties> selected_device_extensions;
                if (maintenance_result == VK_SUCCESS && maintenance_extension_count > 0U) {
                    selected_device_extensions.resize(maintenance_extension_count);
                    maintenance_result = vk.enumerate_device_extensions(
                        selected_physical_device, nullptr, &maintenance_extension_count,
                        selected_device_extensions.data());
                }
                maintenance_device_extension_supported =
                    maintenance_result == VK_SUCCESS &&
                    hasDeviceExtension(selected_device_extensions,
                                       VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
                report.swapchain_maintenance_device_extension_status =
                    maintenance_device_extension_supported ? "supported" :
                    (maintenance_result == VK_SUCCESS ? "not_supported" : "query_failed");

                if (maintenance_device_extension_supported &&
                    maintenance_instance_extensions_enabled &&
                    vk.get_physical_device_features2 != nullptr) {
                    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance_features{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
                    VkPhysicalDeviceFeatures2 device_features{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
                    device_features.pNext = &maintenance_features;
                    vk.get_physical_device_features2(selected_physical_device, &device_features);
                    maintenance_feature_supported =
                        maintenance_features.swapchainMaintenance1 == VK_TRUE;
                    report.swapchain_maintenance_feature_supported =
                        maintenance_feature_supported;
                    report.swapchain_maintenance_feature_status =
                        maintenance_feature_supported ? "supported" : "unsupported";
                } else {
                    report.swapchain_maintenance_feature_status =
                        "not_queryable_or_extension_missing";
                }
            }
        }
    }

    const std::string uint8_index_feature_name =
        "VK_KHR_index_type_uint8.indexTypeUint8";
    const auto contains_feature = [&](const std::vector<std::string>& values) {
        return std::find(values.begin(), values.end(), uint8_index_feature_name) !=
               values.end();
    };
    const bool uint8_index_extension_advertised =
        selected_physical_device_extensions_enumerated &&
        hasDeviceExtension(selected_physical_device_extensions,
                           VK_KHR_INDEX_TYPE_UINT8_EXTENSION_NAME);
    const bool uint8_index_core_path = report.vulkan_1_4_device_api_supported;
    report.index_type_uint8_extension_status =
        uint8_index_core_path ? "core_vulkan_1_4" :
        (uint8_index_extension_advertised ? "advertised" : "not_advertised");

    // Preserve R2.5's per-feature classification. A feature omitted because
    // its query failed or was not queryable must never be mislabeled unsupported.
    if (contains_feature(report.vulkan_1_4_promoted_features_supported)) {
        report.index_type_uint8_feature_support_status = "supported";
        index_type_uint8_feature_enabled = true;
    } else if (contains_feature(report.vulkan_1_4_promoted_features_not_supported)) {
        report.index_type_uint8_feature_support_status = "not_supported";
    } else if (contains_feature(report.vulkan_1_4_promoted_features_not_advertised)) {
        report.index_type_uint8_extension_status = "not_advertised";
        report.index_type_uint8_feature_support_status = "not_advertised";
    } else if (contains_feature(report.vulkan_1_4_promoted_features_query_failed)) {
        report.index_type_uint8_feature_support_status = "query_failed";
    } else if (contains_feature(report.vulkan_1_4_promoted_features_not_queryable)) {
        report.index_type_uint8_feature_support_status = "not_queryable";
    } else if (report.vulkan_1_4_promoted_feature_query_status != "success") {
        report.index_type_uint8_feature_support_status = "query_failed";
    } else {
        report.index_type_uint8_feature_support_status = "unclassified";
    }

    if (report.failure_reason.empty() && selected_physical_device != VK_NULL_HANDLE) {
        const float priority = 1.0F;
        VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.queueFamilyIndex = selected_queue_family;
        queue_info.queueCount = 1U;
        queue_info.pQueuePriorities = &priority;
        std::vector<const char*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance_features_enable{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
        VkPhysicalDeviceIndexTypeUint8FeaturesKHR uint8_index_features_enable{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_KHR};
        if (maintenance_feature_supported) {
            device_extensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
            maintenance_features_enable.swapchainMaintenance1 = VK_TRUE;
            report.swapchain_maintenance_feature_status = "enabled";
        }
        if (index_type_uint8_feature_enabled) {
            if (!report.vulkan_1_4_device_api_supported) {
                device_extensions.push_back(VK_KHR_INDEX_TYPE_UINT8_EXTENSION_NAME);
            }
            uint8_index_features_enable.indexTypeUint8 = VK_TRUE;
            report.index_type_uint8_feature_enable_status = "requested";
        } else {
            report.index_type_uint8_feature_enable_status =
                "not_enabled_" + report.index_type_uint8_feature_support_status;
        }
        VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        if (maintenance_feature_supported && index_type_uint8_feature_enabled) {
            maintenance_features_enable.pNext = &uint8_index_features_enable;
            device_info.pNext = &maintenance_features_enable;
        } else if (maintenance_feature_supported) {
            device_info.pNext = &maintenance_features_enable;
        } else if (index_type_uint8_feature_enabled) {
            device_info.pNext = &uint8_index_features_enable;
        }
        device_info.queueCreateInfoCount = 1U;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount =
            static_cast<std::uint32_t>(device_extensions.size());
        device_info.ppEnabledExtensionNames = device_extensions.data();

        const VkResult result = vk.create_device(selected_physical_device, &device_info,
                                                 nullptr, &device);
        if (!isUsable(result)) {
            if (index_type_uint8_feature_enabled) {
                report.index_type_uint8_feature_enable_status = "device_create_failed";
            }
            fail(report, "vkCreateDevice for VK_KHR_swapchain failed: " + resultName(result));
        } else {
            if (index_type_uint8_feature_enabled) {
                report.index_type_uint8_feature_enable_status = "enabled";
            }
#define LOAD_DEVICE(member, symbol) \
            vk.member = reinterpret_cast<decltype(vk.member)>(vk.get_device_proc_addr(device, symbol))
            LOAD_DEVICE(destroy_device, "vkDestroyDevice");
            LOAD_DEVICE(get_device_queue, "vkGetDeviceQueue");
            LOAD_DEVICE(create_swapchain, "vkCreateSwapchainKHR");
            LOAD_DEVICE(destroy_swapchain, "vkDestroySwapchainKHR");
            LOAD_DEVICE(create_image_view, "vkCreateImageView");
            LOAD_DEVICE(create_buffer, "vkCreateBuffer");
            LOAD_DEVICE(destroy_buffer, "vkDestroyBuffer");
            LOAD_DEVICE(get_buffer_memory_requirements, "vkGetBufferMemoryRequirements");
            LOAD_DEVICE(allocate_memory, "vkAllocateMemory");
            LOAD_DEVICE(free_memory, "vkFreeMemory");
            LOAD_DEVICE(bind_buffer_memory, "vkBindBufferMemory");
            LOAD_DEVICE(map_memory, "vkMapMemory");
            LOAD_DEVICE(unmap_memory, "vkUnmapMemory");
            LOAD_DEVICE(invalidate_mapped_memory_ranges, "vkInvalidateMappedMemoryRanges");
            LOAD_DEVICE(destroy_image_view, "vkDestroyImageView");
            LOAD_DEVICE(create_render_pass, "vkCreateRenderPass");
            LOAD_DEVICE(destroy_render_pass, "vkDestroyRenderPass");
            LOAD_DEVICE(create_framebuffer, "vkCreateFramebuffer");
            LOAD_DEVICE(destroy_framebuffer, "vkDestroyFramebuffer");
            LOAD_DEVICE(create_shader_module, "vkCreateShaderModule");
            LOAD_DEVICE(destroy_shader_module, "vkDestroyShaderModule");
            LOAD_DEVICE(create_pipeline_layout, "vkCreatePipelineLayout");
            LOAD_DEVICE(destroy_pipeline_layout, "vkDestroyPipelineLayout");
            LOAD_DEVICE(create_graphics_pipelines, "vkCreateGraphicsPipelines");
            LOAD_DEVICE(destroy_pipeline, "vkDestroyPipeline");
            LOAD_DEVICE(cmd_begin_render_pass, "vkCmdBeginRenderPass");
            LOAD_DEVICE(cmd_end_render_pass, "vkCmdEndRenderPass");
            LOAD_DEVICE(cmd_bind_pipeline, "vkCmdBindPipeline");
            LOAD_DEVICE(cmd_set_viewport, "vkCmdSetViewport");
            LOAD_DEVICE(cmd_set_scissor, "vkCmdSetScissor");
            LOAD_DEVICE(cmd_draw, "vkCmdDraw");
            LOAD_DEVICE(cmd_bind_index_buffer, "vkCmdBindIndexBuffer");
            LOAD_DEVICE(cmd_draw_indexed, "vkCmdDrawIndexed");
            LOAD_DEVICE(cmd_copy_image_to_buffer, "vkCmdCopyImageToBuffer");
            LOAD_DEVICE(get_swapchain_images, "vkGetSwapchainImagesKHR");
            LOAD_DEVICE(create_semaphore, "vkCreateSemaphore");
            LOAD_DEVICE(destroy_semaphore, "vkDestroySemaphore");
            LOAD_DEVICE(create_fence, "vkCreateFence");
            LOAD_DEVICE(destroy_fence, "vkDestroyFence");
            LOAD_DEVICE(wait_for_fences, "vkWaitForFences");
            LOAD_DEVICE(reset_fences, "vkResetFences");
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
            LOAD_DEVICE(reset_command_pool, "vkResetCommandPool");
            LOAD_DEVICE(create_query_pool, "vkCreateQueryPool");
            LOAD_DEVICE(destroy_query_pool, "vkDestroyQueryPool");
            LOAD_DEVICE(cmd_reset_query_pool, "vkCmdResetQueryPool");
            LOAD_DEVICE(cmd_write_timestamp, "vkCmdWriteTimestamp");
            LOAD_DEVICE(get_query_pool_results, "vkGetQueryPoolResults");
#undef LOAD_DEVICE
            if (vk.get_device_queue == nullptr || vk.create_swapchain == nullptr ||
                vk.destroy_swapchain == nullptr || vk.get_swapchain_images == nullptr ||
                vk.create_image_view == nullptr || vk.destroy_image_view == nullptr ||
                vk.create_buffer == nullptr || vk.destroy_buffer == nullptr ||
                vk.get_buffer_memory_requirements == nullptr ||
                vk.allocate_memory == nullptr || vk.free_memory == nullptr ||
                vk.bind_buffer_memory == nullptr || vk.map_memory == nullptr ||
                vk.unmap_memory == nullptr ||
                vk.invalidate_mapped_memory_ranges == nullptr ||
                vk.create_render_pass == nullptr || vk.destroy_render_pass == nullptr ||
                vk.create_framebuffer == nullptr || vk.destroy_framebuffer == nullptr ||
                vk.create_shader_module == nullptr || vk.destroy_shader_module == nullptr ||
                vk.create_pipeline_layout == nullptr || vk.destroy_pipeline_layout == nullptr ||
                vk.create_graphics_pipelines == nullptr || vk.destroy_pipeline == nullptr ||
                vk.cmd_begin_render_pass == nullptr || vk.cmd_end_render_pass == nullptr ||
                vk.cmd_bind_pipeline == nullptr ||
                vk.cmd_set_viewport == nullptr || vk.cmd_set_scissor == nullptr ||
                vk.cmd_draw == nullptr ||
                (index_type_uint8_feature_enabled &&
                 (vk.cmd_bind_index_buffer == nullptr || vk.cmd_draw_indexed == nullptr)) ||
                vk.cmd_copy_image_to_buffer == nullptr ||
                vk.create_semaphore == nullptr || vk.destroy_semaphore == nullptr ||
                vk.create_fence == nullptr || vk.destroy_fence == nullptr ||
                vk.wait_for_fences == nullptr || vk.reset_fences == nullptr ||
                vk.acquire_next_image == nullptr || vk.queue_present == nullptr ||
                vk.queue_wait_idle == nullptr || vk.create_command_pool == nullptr ||
                vk.destroy_command_pool == nullptr || vk.allocate_command_buffers == nullptr ||
                vk.free_command_buffers == nullptr || vk.begin_command_buffer == nullptr ||
                vk.end_command_buffer == nullptr || vk.cmd_pipeline_barrier == nullptr ||
                vk.queue_submit == nullptr || vk.device_wait_idle == nullptr ||
                vk.reset_command_pool == nullptr) {
                fail(report, "One or more required Vulkan device entry points are unavailable");
            } else {
                vk.get_device_queue(device, selected_queue_family, 0U, &queue);
                if (queue == VK_NULL_HANDLE) fail(report, "vkGetDeviceQueue returned a null queue");
                else if (maintenance_feature_supported) {
                    report.present_fence_entry_points_status = "success";
                }
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
            recordSurfaceTransform(report, capabilities);
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

                        VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                        if ((capabilities.supportedUsageFlags &
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0U) {
                            fail(report, "R2.0 requires swapchain COLOR_ATTACHMENT usage");
                        } else if ((capabilities.supportedUsageFlags &
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0U) {
                            report.swapchain_transfer_src_support_status = "not_supported";
                            report.pixel_readback_mode = "unsupported_surface_transfer_src";
                            report.pixel_verification_status = "unsupported";
                            fail(report, "The Android Surface does not support VK_IMAGE_USAGE_TRANSFER_SRC_BIT for swapchain pixel verification");
                        } else {
                            image_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                            report.selected_image_usage = "COLOR_ATTACHMENT|TRANSFER_SRC";
                            report.swapchain_transfer_src_support_status = "supported";
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
                            swapchain_info.preTransform = chooseSwapchainPreTransform(capabilities);
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
                                    report.swapchain_image_count =
                                        static_cast<std::uint32_t>(images.size());
                                    if (report.selected_image_usage != "COLOR_ATTACHMENT|TRANSFER_SRC") {
                                        fail(report, "Selected swapchain usage does not include COLOR_ATTACHMENT|TRANSFER_SRC");
                                    } else if (!recreateSwapchainAndProbe(
                                        vk, report, selected_physical_device, surface, device,
                                        queue, selected_format.format, selected_format.colorSpace,
                                        image_usage, VK_PRESENT_MODE_FIFO_KHR, composite_alpha,
                                        static_cast<std::uint32_t>(requested_width),
                                        static_cast<std::uint32_t>(requested_height),
                                        swapchain, images, extent, graphics_pipeline,
                                        maintenance_feature_supported,
                                        report.live_resize_window_pause_ms_per_stage)) {
                                        if (report.failure_reason.empty()) {
                                            fail(report, "R2.2 oldSwapchain recreation/probe failed");
                                        }
                                    } else if (!createUint8IndexProbeResources(
                                        vk, report, selected_physical_device, device,
                                        uint8_index_buffer, uint8_index_memory)) {
                                        if (report.failure_reason.empty()) {
                                            fail(report, "R2.6 uint8 index probe resource initialization failed");
                                        }
                                    } else if (!createPixelReadbackResources(
                                        vk, report, selected_physical_device, device,
                                        pixel_readback_buffer, pixel_readback_memory,
                                        pixel_readback_memory_coherent)) {
                                        if (report.failure_reason.empty()) {
                                            fail(report, "Pixel readback buffer initialization failed");
                                        }
                                    } else if (!runFrameSequence(
                                        vk, report, device, queue, selected_queue_family,
                                        swapchain, images, selected_format.format, extent,
                                        acquire_semaphore, present_semaphores,
                                        present_fence, maintenance_feature_supported,
                                        timestamp_query_pool,
                                        pixel_readback_buffer, pixel_readback_memory,
                                        pixel_readback_memory_coherent,
                                        uint8_index_buffer, index_type_uint8_feature_enabled, true,
                                        command_pool, command_buffer, image_view, render_pass,
                                        framebuffer, vertex_shader, fragment_shader,
                                        pipeline_layout, graphics_pipeline)) {
                                        if (report.failure_reason.empty()) {
                                            fail(report, "R1.8 multi-frame timestamp validation failed");
                                        }
                                    } else if ((report.swapchain_acquire_suboptimal_count > 0U ||
                                                report.swapchain_present_suboptimal_count > 0U) &&
                                               !recreateSwapchainAndProbe(
                                        vk, report, selected_physical_device, surface, device,
                                        queue, selected_format.format, selected_format.colorSpace,
                                        image_usage, VK_PRESENT_MODE_FIFO_KHR, composite_alpha,
                                        static_cast<std::uint32_t>(requested_width),
                                        static_cast<std::uint32_t>(requested_height),
                                        swapchain, images, extent, graphics_pipeline,
                                        maintenance_feature_supported, 0U)) {
                                        if (report.failure_reason.empty()) {
                                            fail(report, "R2.2 SUBOPTIMAL swapchain handoff did not recover");
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
    if (maintenance_feature_supported && report.present_fence_wait_status == "success") {
        report.shutdown_presentation_completion_status = "present_fence_signaled";
        report.present_completion_guaranteed = true;
    } else if (!maintenance_feature_supported &&
               report.present_completion_mode == "per_image_reacquire_fallback") {
        report.shutdown_presentation_completion_status =
            "not_guaranteed_without_present_fence";
    }
    if (present_fence != VK_NULL_HANDLE) {
        if (vk.destroy_fence != nullptr) {
            vk.destroy_fence(device, present_fence, nullptr);
            report.present_fence_destroy_status = "success";
        } else {
            report.present_fence_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (uint8_index_buffer != VK_NULL_HANDLE) {
        if (vk.destroy_buffer != nullptr) {
            vk.destroy_buffer(device, uint8_index_buffer, nullptr);
            report.index_type_uint8_buffer_destroy_status = "success";
        } else {
            report.index_type_uint8_buffer_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (uint8_index_memory != VK_NULL_HANDLE) {
        if (vk.free_memory != nullptr) {
            vk.free_memory(device, uint8_index_memory, nullptr);
            report.index_type_uint8_memory_free_status = "success";
        } else {
            report.index_type_uint8_memory_free_status = "failed";
            cleanup_ok = false;
        }
    }
    if (pixel_readback_buffer != VK_NULL_HANDLE) {
        if (vk.destroy_buffer != nullptr) {
            vk.destroy_buffer(device, pixel_readback_buffer, nullptr);
            report.pixel_readback_buffer_destroy_status = "success";
        } else {
            report.pixel_readback_buffer_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (pixel_readback_memory != VK_NULL_HANDLE) {
        if (vk.free_memory != nullptr) {
            vk.free_memory(device, pixel_readback_memory, nullptr);
            report.pixel_readback_memory_free_status = "success";
        } else {
            report.pixel_readback_memory_free_status = "failed";
            cleanup_ok = false;
        }
    }
    if (timestamp_query_pool != VK_NULL_HANDLE) {
        if (vk.destroy_query_pool != nullptr) {
            vk.destroy_query_pool(device, timestamp_query_pool, nullptr);
            report.gpu_timestamp_query_pool_destroy_status = "success";
        } else {
            report.gpu_timestamp_query_pool_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (graphics_pipeline != VK_NULL_HANDLE) {
        if (vk.destroy_pipeline != nullptr) {
            vk.destroy_pipeline(device, graphics_pipeline, nullptr);
            report.graphics_pipeline_destroy_status = "success";
        } else {
            report.graphics_pipeline_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (pipeline_layout != VK_NULL_HANDLE) {
        if (vk.destroy_pipeline_layout != nullptr) {
            vk.destroy_pipeline_layout(device, pipeline_layout, nullptr);
            report.pipeline_layout_destroy_status = "success";
        } else {
            report.pipeline_layout_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (fragment_shader != VK_NULL_HANDLE) {
        if (vk.destroy_shader_module != nullptr) {
            vk.destroy_shader_module(device, fragment_shader, nullptr);
            report.fragment_shader_module_destroy_status = "success";
        } else {
            report.fragment_shader_module_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (vertex_shader != VK_NULL_HANDLE) {
        if (vk.destroy_shader_module != nullptr) {
            vk.destroy_shader_module(device, vertex_shader, nullptr);
            report.vertex_shader_module_destroy_status = "success";
        } else {
            report.vertex_shader_module_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (framebuffer != VK_NULL_HANDLE) {
        if (vk.destroy_framebuffer != nullptr) {
            vk.destroy_framebuffer(device, framebuffer, nullptr);
            report.framebuffer_destroy_status = "success";
        } else {
            report.framebuffer_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (render_pass != VK_NULL_HANDLE) {
        if (vk.destroy_render_pass != nullptr) {
            vk.destroy_render_pass(device, render_pass, nullptr);
            report.render_pass_destroy_status = "success";
        } else {
            report.render_pass_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (image_view != VK_NULL_HANDLE) {
        if (vk.destroy_image_view != nullptr) {
            vk.destroy_image_view(device, image_view, nullptr);
            report.image_view_destroy_status = "success";
        } else {
            report.image_view_destroy_status = "failed";
            cleanup_ok = false;
        }
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
    bool present_semaphores_destroyed = true;
    for (VkSemaphore& semaphore : present_semaphores) {
        if (semaphore == VK_NULL_HANDLE) continue;
        if (vk.destroy_semaphore != nullptr) {
            vk.destroy_semaphore(device, semaphore, nullptr);
            semaphore = VK_NULL_HANDLE;
        } else {
            present_semaphores_destroyed = false;
            cleanup_ok = false;
        }
    }
    if (present_semaphores_destroyed && !present_semaphores.empty()) {
        report.present_semaphore_destroy_status = "success";
    } else if (!present_semaphores_destroyed) {
        report.present_semaphore_destroy_status = "failed";
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

    const bool presentation_completion_valid =
        !maintenance_feature_supported ||
        (report.present_fence_used && report.present_completion_guaranteed &&
         report.present_fence_entry_points_status == "success" &&
         report.present_fence_create_status == "success" &&
         report.present_fence_attach_status == "success" &&
         report.present_fence_wait_status == "success" &&
         report.present_fence_destroy_status == "success" &&
         report.present_fence_attach_count == report.frame_completed_count &&
         report.present_fence_wait_count == report.frame_completed_count &&
         report.present_fence_reset_count + 1U == report.frame_completed_count &&
         report.shutdown_presentation_completion_status == "present_fence_signaled");
    const bool uint8_index_probe_valid =
        index_type_uint8_feature_enabled
            ? (report.index_type_uint8_feature_enable_status == "enabled" &&
               report.index_type_uint8_buffer_create_status == "success" &&
               report.index_type_uint8_memory_allocate_status == "success" &&
               report.index_type_uint8_memory_bind_status == "success" &&
               report.index_type_uint8_memory_map_status == "success" &&
               report.index_type_uint8_upload_status == "success" &&
               report.index_type_uint8_memory_unmap_status == "success" &&
               report.index_type_uint8_draw_entry_points_status == "success" &&
               report.index_type_uint8_draw_record_status == "success" &&
               report.index_type_uint8_draw_recorded &&
               report.index_type_uint8_draw_index_count == 3U &&
               report.index_type_uint8_probe_status == "success" &&
               report.index_type_uint8_buffer_destroy_status == "success" &&
               report.index_type_uint8_memory_free_status == "success")
            : (report.index_type_uint8_probe_status == "not_supported" ||
               report.index_type_uint8_probe_status == "not_advertised");
    const bool complete = presentation_completion_valid && uint8_index_probe_valid &&
        report.loader_status == "success" &&
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
        report.command_pool_destroy_status == "success" &&
        report.image_view_create_status == "success" &&
        report.render_pass_create_status == "success" &&
        report.framebuffer_create_status == "success" &&
        report.vertex_shader_module_create_status == "success" &&
        report.fragment_shader_module_create_status == "success" &&
        report.pipeline_layout_create_status == "success" &&
        report.graphics_pipeline_create_status == "success" &&
        report.render_pass_begin_recorded && report.render_pass_end_recorded &&
        report.graphics_pipeline_bind_recorded && report.draw_call_recorded &&
        report.draw_vertex_count == 3U &&
        report.graphics_pipeline_destroy_status == "success" &&
        report.pipeline_layout_destroy_status == "success" &&
        report.vertex_shader_module_destroy_status == "success" &&
        report.fragment_shader_module_destroy_status == "success" &&
        report.frame_content_written && report.frame_timing_valid &&
        report.frame_target_count == 16U &&
        report.frame_completed_count == report.frame_target_count + report.warmup_frame_count &&
        report.frame_timing_sample_count == report.frame_target_count &&
        report.gpu_timestamp_status == "success" &&
        report.gpu_timestamp_query_pool_create_status == "success" &&
        report.gpu_timestamp_query_status == "success" &&
        report.gpu_timestamp_sample_count == report.frame_target_count &&
        report.gpu_timestamp_query_pool_destroy_status == "success" &&
        report.swapchain_transfer_src_support_status == "supported" &&
        report.pixel_readback_buffer_create_status == "success" &&
        report.pixel_readback_memory_allocate_status == "success" &&
        report.pixel_readback_bind_status == "success" &&
        report.pixel_readback_recorded &&
        report.pixel_readback_copy_status == "success" &&
        report.pixel_readback_map_status == "success" &&
        (report.pixel_readback_invalidate_status == "success" ||
         report.pixel_readback_invalidate_status == "not_required_host_coherent") &&
        report.pixel_readback_unmap_status == "success" &&
        report.pixel_readback_sample_count == 5U &&
        report.pixel_background_samples_match_clear &&
        report.pixel_triangle_center_detected &&
        report.pixel_verification_status == "success" &&
        report.pixel_readback_buffer_destroy_status == "success" &&
        report.pixel_readback_memory_free_status == "success" &&
        report.image_view_destroy_status == "success" &&
        report.render_pass_destroy_status == "success" &&
        report.framebuffer_destroy_status == "success" &&
        report.swapchain_recreation_status == "success" &&
        report.swapchain_recreation_mode == "old_swapchain_handoff" &&
        report.swapchain_recreation_attempt_count >= 1U &&
        report.swapchain_recreation_success_count >= 1U &&
        report.swapchain_old_handle_supplied &&
        report.swapchain_old_destroy_status == "success" &&
        report.swapchain_recreated_image_query_status == "success" &&
        report.swapchain_recreated_image_count > 0U &&
        report.swapchain_logical_device_reused &&
        report.swapchain_recreation_probe_acquire_status == "success" &&
        report.swapchain_recreation_probe_submit_status == "success" &&
        report.swapchain_recreation_probe_present_status == "success" &&
        (report.swapchain_recreation_probe_completion_status == "present_fence_signaled" ||
         report.swapchain_recreation_probe_completion_status == "queue_idle_fallback") &&
        report.swapchain_recreation_probe_present_count >= 1U &&
        report.swapchain_result_policy_status == "success" &&
        (report.swapchain_out_of_date_recovery_status == "not_observed" ||
         report.swapchain_out_of_date_recovery_status == "recovered");
    if (complete && report.failure_reason.empty()) report.status = "success";
    else if (report.status != "failed") report.status = "failed";
    return report;
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_dev_nyamist_g925surface_MainActivity_validateSurfaceNative(
        JNIEnv* env, jclass, jobject surface, jint width, jint height,
        jint live_resize_window_mode) {
    const auto report = runValidation(env, surface, width, height, live_resize_window_mode);
    const std::string json = nyamist::discovery::toJson(report);
    return env->NewStringUTF(json.c_str());
}
