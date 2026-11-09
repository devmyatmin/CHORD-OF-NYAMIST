#include "nyamist/discovery/gpu_resource_transfer_validation.hpp"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(NYAMIST_HAS_VULKAN)
#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
#include <dlfcn.h>
#endif
#endif

namespace nyamist::discovery {
namespace {

std::string jsonEscape(std::string_view value) {
    std::ostringstream out;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20U) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch) << std::dec;
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    return out.str();
}

void quoted(std::ostringstream& out, const std::string& value) {
    out << '"' << jsonEscape(value) << '"';
}

std::string hexValue(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::nouppercase << value;
    return out.str();
}

bool isG925(const char* name) {
    const std::string value = name != nullptr ? name : "";
    return value.find("G925") != std::string::npos ||
           value.find("g925") != std::string::npos;
}

void setFailure(GpuResourceTransferValidationReport& report, const std::string& reason) {
    if (report.failure_reason.empty()) report.failure_reason = reason;
}

#if defined(NYAMIST_HAS_VULKAN)
std::string resultName(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
    default:
        return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
    }
}

struct MemoryTypeChoice {
    bool found{};
    std::uint32_t index{};
    VkMemoryPropertyFlags flags{};
};

MemoryTypeChoice chooseHostCoherentMemoryType(
    const VkPhysicalDeviceMemoryProperties& properties,
    std::uint32_t compatible_bits) {
    MemoryTypeChoice fallback{};
    for (std::uint32_t i = 0U; i < properties.memoryTypeCount; ++i) {
        if ((compatible_bits & (1U << i)) == 0U) continue;
        const VkMemoryPropertyFlags flags = properties.memoryTypes[i].propertyFlags;
        const VkMemoryPropertyFlags required =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if ((flags & required) != required) continue;
        const MemoryTypeChoice candidate{true, i, flags};
        if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0U) return candidate;
        if (!fallback.found) fallback = candidate;
    }
    return fallback;
}
#endif

} // namespace

std::string toJson(const GpuResourceTransferValidationReport& report) {
    std::ostringstream out;
    out << "{\n"
        << "  \"schema_version\": " << report.schema_version << ",\n"
        << "  \"project\": \"CHORD OF NYAMIST / NYAMIST G925 R1.4\",\n"
        << "  \"status\": ";
    quoted(out, report.status);
    out << ",\n  \"vulkan_compiled\": " << (report.vulkan_compiled ? "true" : "false")
        << ",\n  \"loader_library\": ";
    if (report.loader_library.empty()) out << "null"; else quoted(out, report.loader_library);
    out << ",\n  \"loader_api_version\": ";
    if (report.loader_api_version.empty()) out << "null"; else quoted(out, report.loader_api_version);
    out << ",\n  \"physical_device\": {\n    \"query_status\": ";
    quoted(out, report.physical_device_status);
    out << ",\n    \"name\": ";
    if (report.device_name.empty()) out << "null"; else quoted(out, report.device_name);
    out << ",\n    \"vendor_id\": ";
    if (report.vendor_id_hex.empty()) out << "null"; else quoted(out, report.vendor_id_hex);
    out << ",\n    \"device_id\": ";
    if (report.device_id_hex.empty()) out << "null"; else quoted(out, report.device_id_hex);
    out << ",\n    \"api_version\": ";
    if (report.physical_device_api_version.empty()) out << "null"; else quoted(out, report.physical_device_api_version);
    out << ",\n    \"target_gpu_name_match\": " << (report.target_gpu_match ? "true" : "false")
        << "\n  },\n  \"logical_device\": {\n    \"create_status\": ";
    quoted(out, report.logical_device_status);
    out << ",\n    \"queue_family_index\": " << report.queue_family_index
        << ",\n    \"queue_handle_non_null\": " << (report.queue_handle_non_null ? "true" : "false")
        << "\n  },\n  \"buffers\": {\n    \"size_bytes\": " << report.transfer_size_bytes
        << ",\n    \"source_create_status\": ";
    quoted(out, report.source_buffer_status);
    out << ",\n    \"destination_create_status\": ";
    quoted(out, report.destination_buffer_status);
    out << "\n  },\n  \"memory\": {\n    \"source_type_status\": ";
    quoted(out, report.source_memory_type_status);
    out << ",\n    \"source_type_index\": " << report.source_memory_type_index
        << ",\n    \"source_host_visible\": " << (report.source_memory_host_visible ? "true" : "false")
        << ",\n    \"source_host_coherent\": " << (report.source_memory_host_coherent ? "true" : "false")
        << ",\n    \"source_device_local\": " << (report.source_memory_device_local ? "true" : "false")
        << ",\n    \"destination_type_status\": ";
    quoted(out, report.destination_memory_type_status);
    out << ",\n    \"destination_type_index\": " << report.destination_memory_type_index
        << ",\n    \"destination_host_visible\": " << (report.destination_memory_host_visible ? "true" : "false")
        << ",\n    \"destination_host_coherent\": " << (report.destination_memory_host_coherent ? "true" : "false")
        << ",\n    \"destination_device_local\": " << (report.destination_memory_device_local ? "true" : "false")
        << ",\n    \"source_allocate_status\": ";
    quoted(out, report.source_memory_allocate_status);
    out << ",\n    \"destination_allocate_status\": ";
    quoted(out, report.destination_memory_allocate_status);
    out << ",\n    \"source_bind_status\": ";
    quoted(out, report.source_memory_bind_status);
    out << ",\n    \"destination_bind_status\": ";
    quoted(out, report.destination_memory_bind_status);
    out << ",\n    \"source_map_status\": ";
    quoted(out, report.source_map_status);
    out << ",\n    \"source_unmap_status\": ";
    quoted(out, report.source_unmap_status);
    out << ",\n    \"destination_map_status\": ";
    quoted(out, report.destination_map_status);
    out << ",\n    \"destination_unmap_status\": ";
    quoted(out, report.destination_unmap_status);
    out << "\n  },\n  \"command\": {\n    \"pool_status\": ";
    quoted(out, report.command_pool_status);
    out << ",\n    \"buffer_status\": ";
    quoted(out, report.command_buffer_status);
    out << ",\n    \"recording_status\": ";
    quoted(out, report.command_recording_status);
    out << ",\n    \"submit_status\": ";
    quoted(out, report.queue_submit_status);
    out << ",\n    \"submit_result\": ";
    if (report.queue_submit_result.empty()) out << "null"; else quoted(out, report.queue_submit_result);
    out << ",\n    \"submitted\": " << (report.queue_submission_succeeded ? "true" : "false")
        << ",\n    \"fence_create_status\": ";
    quoted(out, report.fence_create_status);
    out << ",\n    \"fence_wait_status\": ";
    quoted(out, report.fence_wait_status);
    out << ",\n    \"fence_wait_result\": ";
    if (report.fence_wait_result.empty()) out << "null"; else quoted(out, report.fence_wait_result);
    out << ",\n    \"fence_status\": ";
    quoted(out, report.fence_status_status);
    out << ",\n    \"fence_status_result\": ";
    if (report.fence_status_result.empty()) out << "null"; else quoted(out, report.fence_status_result);
    out << ",\n    \"fence_signaled\": " << (report.fence_signaled ? "true" : "false")
        << ",\n    \"device_idle_status\": ";
    quoted(out, report.device_idle_status);
    out << "\n  },\n  \"readback\": {\n    \"status\": ";
    quoted(out, report.readback_status);
    out << ",\n    \"bytes_copied\": " << report.bytes_copied
        << ",\n    \"bytes_verified\": " << report.bytes_verified
        << ",\n    \"data_match\": " << (report.data_match ? "true" : "false")
        << ",\n    \"first_mismatch_offset\": ";
    if (report.has_mismatch_offset) out << report.first_mismatch_offset; else out << "null";
    out << "\n  },\n  \"device_lost\": " << (report.device_lost ? "true" : "false")
        << ",\n  \"cleanup\": {\n    \"status\": ";
    quoted(out, report.cleanup_status);
    out << ",\n    \"fence_destroyed\": " << (report.fence_destroyed ? "true" : "false")
        << ",\n    \"command_buffer_freed\": " << (report.command_buffer_freed ? "true" : "false")
        << ",\n    \"command_pool_destroyed\": " << (report.command_pool_destroyed ? "true" : "false")
        << ",\n    \"source_buffer_destroyed\": " << (report.source_buffer_destroyed ? "true" : "false")
        << ",\n    \"destination_buffer_destroyed\": " << (report.destination_buffer_destroyed ? "true" : "false")
        << ",\n    \"source_memory_freed\": " << (report.source_memory_freed ? "true" : "false")
        << ",\n    \"destination_memory_freed\": " << (report.destination_memory_freed ? "true" : "false")
        << ",\n    \"logical_device_destroyed\": " << (report.logical_device_destroyed ? "true" : "false")
        << ",\n    \"instance_destroyed\": " << (report.instance_destroyed ? "true" : "false")
        << "\n  },\n  \"failure_reason\": ";
    if (report.failure_reason.empty()) out << "null"; else quoted(out, report.failure_reason);
    out << "\n}\n";
    return out.str();
}

GpuResourceTransferValidationReport collectGpuResourceTransferValidationReport() {
    GpuResourceTransferValidationReport report;
#if !defined(NYAMIST_HAS_VULKAN)
    report.status = "unavailable";
    report.failure_reason = "This build has no Vulkan headers/loader integration";
    return report;
#else
    report.vulkan_compiled = true;
#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
    struct Library {
        void* handle{};
        std::string name;
        ~Library() { if (handle != nullptr) dlclose(handle); }
    } library;
    PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;
    const char* candidates[] = {"libvulkan.so", "libvulkan.so.1"};
    for (const char* candidate : candidates) {
        void* handle = dlopen(candidate, RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) continue;
        dlerror();
        auto symbol = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            dlsym(handle, "vkGetInstanceProcAddr"));
        const char* error = dlerror();
        if (symbol == nullptr || error != nullptr) {
            dlclose(handle);
            continue;
        }
        library.handle = handle;
        library.name = candidate;
        get_instance_proc_addr = symbol;
        break;
    }
    if (get_instance_proc_addr == nullptr) {
        report.status = "failed";
        report.failure_reason = "Unable to load libvulkan.so and resolve vkGetInstanceProcAddr";
        return report;
    }
    report.loader_library = library.name;
#else
    PFN_vkGetInstanceProcAddr get_instance_proc_addr = &vkGetInstanceProcAddr;
    report.loader_library = "linked";
#endif

    auto create_instance = reinterpret_cast<PFN_vkCreateInstance>(
        get_instance_proc_addr(VK_NULL_HANDLE, "vkCreateInstance"));
    auto enumerate_instance_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        get_instance_proc_addr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (create_instance == nullptr) {
        report.status = "failed";
        setFailure(report, "vkCreateInstance entry point is unavailable");
        return report;
    }

    std::uint32_t loader_version = VK_API_VERSION_1_0;
    if (enumerate_instance_version != nullptr) {
        const VkResult version_result = enumerate_instance_version(&loader_version);
        if (version_result != VK_SUCCESS) {
            report.status = "failed";
            setFailure(report, "vkEnumerateInstanceVersion returned " + resultName(version_result));
            return report;
        }
    }
    report.loader_api_version =
        std::to_string(VK_VERSION_MAJOR(loader_version)) + "." +
        std::to_string(VK_VERSION_MINOR(loader_version)) + "." +
        std::to_string(VK_VERSION_PATCH(loader_version));

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "CHORD OF NYAMIST R1.4 GPU Resource Transfer";
    app.applicationVersion = 1U;
    app.pEngineName = "NYAMIST G925 Validation";
    app.engineVersion = 1U;
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = create_instance(&instance_info, nullptr, &instance);
    if (result != VK_SUCCESS || instance == VK_NULL_HANDLE) {
        report.status = "failed";
        setFailure(report, "vkCreateInstance returned " + resultName(result));
        return report;
    }

    auto destroy_instance = reinterpret_cast<PFN_vkDestroyInstance>(
        get_instance_proc_addr(instance, "vkDestroyInstance"));
    auto enumerate_physical_devices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        get_instance_proc_addr(instance, "vkEnumeratePhysicalDevices"));
    auto get_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        get_instance_proc_addr(instance, "vkGetPhysicalDeviceProperties"));
    auto get_queue_families = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        get_instance_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    auto get_memory_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        get_instance_proc_addr(instance, "vkGetPhysicalDeviceMemoryProperties"));
    auto create_device = reinterpret_cast<PFN_vkCreateDevice>(
        get_instance_proc_addr(instance, "vkCreateDevice"));
    auto get_device_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        get_instance_proc_addr(instance, "vkGetDeviceProcAddr"));

    if (destroy_instance == nullptr || enumerate_physical_devices == nullptr ||
        get_properties == nullptr || get_queue_families == nullptr ||
        get_memory_properties == nullptr || create_device == nullptr ||
        get_device_proc_addr == nullptr) {
        report.status = "failed";
        setFailure(report, "A required Vulkan instance entry point is unavailable");
        if (destroy_instance != nullptr) {
            destroy_instance(instance, nullptr);
            report.instance_destroyed = true;
        }
        report.cleanup_status = report.instance_destroyed ? "success" : "failed";
        return report;
    }

    std::uint32_t physical_count = 0U;
    result = enumerate_physical_devices(instance, &physical_count, nullptr);
    if (result != VK_SUCCESS || physical_count == 0U) {
        report.status = "failed";
        report.physical_device_status = "failed";
        setFailure(report, result == VK_SUCCESS
            ? "Vulkan loader enumerated zero physical devices"
            : "vkEnumeratePhysicalDevices returned " + resultName(result));
        destroy_instance(instance, nullptr);
        report.instance_destroyed = true;
        report.cleanup_status = "success";
        return report;
    }
    std::vector<VkPhysicalDevice> devices(physical_count);
    result = enumerate_physical_devices(instance, &physical_count, devices.data());
    if (result != VK_SUCCESS) {
        report.status = "failed";
        report.physical_device_status = "failed";
        setFailure(report, "vkEnumeratePhysicalDevices(data) returned " + resultName(result));
        destroy_instance(instance, nullptr);
        report.instance_destroyed = true;
        report.cleanup_status = "success";
        return report;
    }
    devices.resize(physical_count);

    VkPhysicalDevice selected = VK_NULL_HANDLE;
    std::uint32_t selected_family = 0U;
    bool selected_target = false;
    for (const VkPhysicalDevice physical : devices) {
        VkPhysicalDeviceProperties properties{};
        get_properties(physical, &properties);
        std::uint32_t family_count = 0U;
        get_queue_families(physical, &family_count, nullptr);
        std::vector<VkQueueFamilyProperties> families(family_count);
        if (family_count > 0U) {
            get_queue_families(physical, &family_count, families.data());
            families.resize(family_count);
        }
        std::uint32_t graphics_family = UINT32_MAX;
        for (std::uint32_t i = 0U; i < families.size(); ++i) {
            if (families[i].queueCount > 0U &&
                (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
                graphics_family = i;
                break;
            }
        }
        if (graphics_family == UINT32_MAX) continue;
        const bool target = isG925(properties.deviceName);
        if (selected == VK_NULL_HANDLE || (target && !selected_target)) {
            selected = physical;
            selected_family = graphics_family;
            selected_target = target;
            report.device_name = properties.deviceName;
            report.vendor_id_hex = hexValue(properties.vendorID);
            report.device_id_hex = hexValue(properties.deviceID);
            report.physical_device_api_version =
                std::to_string(VK_VERSION_MAJOR(properties.apiVersion)) + "." +
                std::to_string(VK_VERSION_MINOR(properties.apiVersion)) + "." +
                std::to_string(VK_VERSION_PATCH(properties.apiVersion));
            report.target_gpu_match = target;
        }
        if (target) break;
    }

    if (selected == VK_NULL_HANDLE) {
        report.status = "failed";
        report.physical_device_status = "failed";
        setFailure(report, "No physical device exposes a non-empty graphics queue family");
        destroy_instance(instance, nullptr);
        report.instance_destroyed = true;
        report.cleanup_status = "success";
        return report;
    }
    report.physical_device_status = "success";
    report.queue_family_index = selected_family;

    constexpr float priority = 1.0F;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = selected_family;
    queue_info.queueCount = 1U;
    queue_info.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1U;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 0U;
    device_info.pEnabledFeatures = nullptr;

    VkDevice device = VK_NULL_HANDLE;
    result = create_device(selected, &device_info, nullptr, &device);
    if (result != VK_SUCCESS || device == VK_NULL_HANDLE) {
        report.status = "failed";
        report.logical_device_status = "failed";
        if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
        setFailure(report, "vkCreateDevice returned " + resultName(result));
        destroy_instance(instance, nullptr);
        report.instance_destroyed = true;
        report.cleanup_status = "success";
        return report;
    }
    report.logical_device_status = "success";

    auto get_device_queue = reinterpret_cast<PFN_vkGetDeviceQueue>(
        get_device_proc_addr(device, "vkGetDeviceQueue"));
    auto create_command_pool = reinterpret_cast<PFN_vkCreateCommandPool>(
        get_device_proc_addr(device, "vkCreateCommandPool"));
    auto destroy_command_pool = reinterpret_cast<PFN_vkDestroyCommandPool>(
        get_device_proc_addr(device, "vkDestroyCommandPool"));
    auto allocate_command_buffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
        get_device_proc_addr(device, "vkAllocateCommandBuffers"));
    auto free_command_buffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(
        get_device_proc_addr(device, "vkFreeCommandBuffers"));
    auto begin_command_buffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(
        get_device_proc_addr(device, "vkBeginCommandBuffer"));
    auto end_command_buffer = reinterpret_cast<PFN_vkEndCommandBuffer>(
        get_device_proc_addr(device, "vkEndCommandBuffer"));
    auto create_buffer = reinterpret_cast<PFN_vkCreateBuffer>(
        get_device_proc_addr(device, "vkCreateBuffer"));
    auto destroy_buffer = reinterpret_cast<PFN_vkDestroyBuffer>(
        get_device_proc_addr(device, "vkDestroyBuffer"));
    auto get_buffer_memory_requirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
        get_device_proc_addr(device, "vkGetBufferMemoryRequirements"));
    auto allocate_memory = reinterpret_cast<PFN_vkAllocateMemory>(
        get_device_proc_addr(device, "vkAllocateMemory"));
    auto free_memory = reinterpret_cast<PFN_vkFreeMemory>(
        get_device_proc_addr(device, "vkFreeMemory"));
    auto bind_buffer_memory = reinterpret_cast<PFN_vkBindBufferMemory>(
        get_device_proc_addr(device, "vkBindBufferMemory"));
    auto map_memory = reinterpret_cast<PFN_vkMapMemory>(
        get_device_proc_addr(device, "vkMapMemory"));
    auto unmap_memory = reinterpret_cast<PFN_vkUnmapMemory>(
        get_device_proc_addr(device, "vkUnmapMemory"));
    auto cmd_copy_buffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(
        get_device_proc_addr(device, "vkCmdCopyBuffer"));
    auto queue_submit = reinterpret_cast<PFN_vkQueueSubmit>(
        get_device_proc_addr(device, "vkQueueSubmit"));
    auto create_fence = reinterpret_cast<PFN_vkCreateFence>(
        get_device_proc_addr(device, "vkCreateFence"));
    auto destroy_fence = reinterpret_cast<PFN_vkDestroyFence>(
        get_device_proc_addr(device, "vkDestroyFence"));
    auto wait_for_fences = reinterpret_cast<PFN_vkWaitForFences>(
        get_device_proc_addr(device, "vkWaitForFences"));
    auto get_fence_status = reinterpret_cast<PFN_vkGetFenceStatus>(
        get_device_proc_addr(device, "vkGetFenceStatus"));
    auto device_wait_idle = reinterpret_cast<PFN_vkDeviceWaitIdle>(
        get_device_proc_addr(device, "vkDeviceWaitIdle"));
    auto destroy_device = reinterpret_cast<PFN_vkDestroyDevice>(
        get_device_proc_addr(device, "vkDestroyDevice"));
    if (destroy_device == nullptr) {
        destroy_device = reinterpret_cast<PFN_vkDestroyDevice>(
            get_instance_proc_addr(instance, "vkDestroyDevice"));
    }

    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkBuffer source_buffer = VK_NULL_HANDLE;
    VkBuffer destination_buffer = VK_NULL_HANDLE;
    VkDeviceMemory source_memory = VK_NULL_HANDLE;
    VkDeviceMemory destination_memory = VK_NULL_HANDLE;

    const bool dispatch_ready =
        get_device_queue != nullptr && create_command_pool != nullptr &&
        destroy_command_pool != nullptr && allocate_command_buffers != nullptr &&
        free_command_buffers != nullptr && begin_command_buffer != nullptr &&
        end_command_buffer != nullptr && create_buffer != nullptr &&
        destroy_buffer != nullptr && get_buffer_memory_requirements != nullptr &&
        allocate_memory != nullptr && free_memory != nullptr &&
        bind_buffer_memory != nullptr && map_memory != nullptr &&
        unmap_memory != nullptr && cmd_copy_buffer != nullptr &&
        queue_submit != nullptr && create_fence != nullptr &&
        destroy_fence != nullptr && wait_for_fences != nullptr &&
        get_fence_status != nullptr && device_wait_idle != nullptr &&
        destroy_device != nullptr;

    if (!dispatch_ready) {
        report.status = "failed";
        setFailure(report, "One or more required Vulkan device entry points are unavailable");
    } else {
        get_device_queue(device, selected_family, 0U, &queue);
        report.queue_handle_non_null = queue != VK_NULL_HANDLE;
        bool operations_ok = report.queue_handle_non_null;
        if (!operations_ok) setFailure(report, "vkGetDeviceQueue returned VK_NULL_HANDLE");

        VkPhysicalDeviceMemoryProperties memory_properties{};
        get_memory_properties(selected, &memory_properties);

        if (operations_ok) {
            VkBufferCreateInfo source_info{};
            source_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            source_info.size = static_cast<VkDeviceSize>(report.transfer_size_bytes);
            source_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            source_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            result = create_buffer(device, &source_info, nullptr, &source_buffer);
            report.source_buffer_status =
                (result == VK_SUCCESS && source_buffer != VK_NULL_HANDLE) ? "success" : "failed";
            if (report.source_buffer_status != "success") {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "Source vkCreateBuffer returned " + resultName(result));
            }
        }

        if (operations_ok) {
            VkBufferCreateInfo destination_info{};
            destination_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            destination_info.size = static_cast<VkDeviceSize>(report.transfer_size_bytes);
            destination_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            destination_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            result = create_buffer(device, &destination_info, nullptr, &destination_buffer);
            report.destination_buffer_status =
                (result == VK_SUCCESS && destination_buffer != VK_NULL_HANDLE) ? "success" : "failed";
            if (report.destination_buffer_status != "success") {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "Destination vkCreateBuffer returned " + resultName(result));
            }
        }

        VkMemoryRequirements source_requirements{};
        VkMemoryRequirements destination_requirements{};
        MemoryTypeChoice source_choice{};
        MemoryTypeChoice destination_choice{};
        if (operations_ok) {
            get_buffer_memory_requirements(device, source_buffer, &source_requirements);
            source_choice = chooseHostCoherentMemoryType(memory_properties, source_requirements.memoryTypeBits);
            report.source_memory_type_status = source_choice.found ? "success" : "unsupported";
            if (!source_choice.found) {
                operations_ok = false;
                setFailure(report, "No HOST_VISIBLE | HOST_COHERENT memory type is compatible with the source buffer");
            } else {
                report.source_memory_type_index = source_choice.index;
                report.source_memory_host_visible =
                    (source_choice.flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0U;
                report.source_memory_host_coherent =
                    (source_choice.flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0U;
                report.source_memory_device_local =
                    (source_choice.flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0U;
            }
        }
        if (operations_ok) {
            get_buffer_memory_requirements(device, destination_buffer, &destination_requirements);
            destination_choice = chooseHostCoherentMemoryType(memory_properties, destination_requirements.memoryTypeBits);
            report.destination_memory_type_status = destination_choice.found ? "success" : "unsupported";
            if (!destination_choice.found) {
                operations_ok = false;
                setFailure(report, "No HOST_VISIBLE | HOST_COHERENT memory type is compatible with the destination buffer");
            } else {
                report.destination_memory_type_index = destination_choice.index;
                report.destination_memory_host_visible =
                    (destination_choice.flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0U;
                report.destination_memory_host_coherent =
                    (destination_choice.flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0U;
                report.destination_memory_device_local =
                    (destination_choice.flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0U;
            }
        }

        if (operations_ok) {
            VkMemoryAllocateInfo allocate_info{};
            allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocate_info.allocationSize = source_requirements.size;
            allocate_info.memoryTypeIndex = source_choice.index;
            result = allocate_memory(device, &allocate_info, nullptr, &source_memory);
            report.source_memory_allocate_status =
                (result == VK_SUCCESS && source_memory != VK_NULL_HANDLE) ? "success" : "failed";
            if (report.source_memory_allocate_status != "success") {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "Source vkAllocateMemory returned " + resultName(result));
            }
        }
        if (operations_ok) {
            VkMemoryAllocateInfo allocate_info{};
            allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocate_info.allocationSize = destination_requirements.size;
            allocate_info.memoryTypeIndex = destination_choice.index;
            result = allocate_memory(device, &allocate_info, nullptr, &destination_memory);
            report.destination_memory_allocate_status =
                (result == VK_SUCCESS && destination_memory != VK_NULL_HANDLE) ? "success" : "failed";
            if (report.destination_memory_allocate_status != "success") {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "Destination vkAllocateMemory returned " + resultName(result));
            }
        }

        if (operations_ok) {
            result = bind_buffer_memory(device, source_buffer, source_memory, 0U);
            report.source_memory_bind_status = result == VK_SUCCESS ? "success" : "failed";
            if (result != VK_SUCCESS) {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "Source vkBindBufferMemory returned " + resultName(result));
            }
        }
        if (operations_ok) {
            result = bind_buffer_memory(device, destination_buffer, destination_memory, 0U);
            report.destination_memory_bind_status = result == VK_SUCCESS ? "success" : "failed";
            if (result != VK_SUCCESS) {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "Destination vkBindBufferMemory returned " + resultName(result));
            }
        }

        std::vector<std::uint8_t> expected(static_cast<std::size_t>(report.transfer_size_bytes));
        for (std::size_t i = 0U; i < expected.size(); ++i) {
            expected[i] = static_cast<std::uint8_t>((i * 37U + 11U) & 0xffU);
        }

        if (operations_ok) {
            void* mapped = nullptr;
            result = map_memory(device, source_memory, 0U,
                static_cast<VkDeviceSize>(report.transfer_size_bytes), 0U, &mapped);
            report.source_map_status =
                (result == VK_SUCCESS && mapped != nullptr) ? "success" : "failed";
            if (report.source_map_status != "success") {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "Source vkMapMemory returned " + resultName(result));
            } else {
                std::memcpy(mapped, expected.data(), expected.size());
                unmap_memory(device, source_memory);
                report.source_unmap_status = "success";
            }
        }

        if (operations_ok) {
            VkCommandPoolCreateInfo pool_info{};
            pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool_info.queueFamilyIndex = selected_family;
            result = create_command_pool(device, &pool_info, nullptr, &pool);
            report.command_pool_status =
                (result == VK_SUCCESS && pool != VK_NULL_HANDLE) ? "success" : "failed";
            if (report.command_pool_status != "success") {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "vkCreateCommandPool returned " + resultName(result));
            }
        }
        if (operations_ok) {
            VkCommandBufferAllocateInfo allocate_info{};
            allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate_info.commandPool = pool;
            allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate_info.commandBufferCount = 1U;
            result = allocate_command_buffers(device, &allocate_info, &command_buffer);
            report.command_buffer_status =
                (result == VK_SUCCESS && command_buffer != VK_NULL_HANDLE) ? "success" : "failed";
            if (report.command_buffer_status != "success") {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "vkAllocateCommandBuffers returned " + resultName(result));
            }
        }
        if (operations_ok) {
            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = begin_command_buffer(command_buffer, &begin_info);
            if (result == VK_SUCCESS) {
                VkBufferCopy copy{};
                copy.srcOffset = 0U;
                copy.dstOffset = 0U;
                copy.size = static_cast<VkDeviceSize>(report.transfer_size_bytes);
                cmd_copy_buffer(command_buffer, source_buffer, destination_buffer, 1U, &copy);
                result = end_command_buffer(command_buffer);
            }
            report.command_recording_status = result == VK_SUCCESS ? "success" : "failed";
            if (result != VK_SUCCESS) {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "Command-buffer begin/copy/end returned " + resultName(result));
            }
        }
        if (operations_ok) {
            VkFenceCreateInfo fence_info{};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            result = create_fence(device, &fence_info, nullptr, &fence);
            report.fence_create_status =
                (result == VK_SUCCESS && fence != VK_NULL_HANDLE) ? "success" : "failed";
            if (report.fence_create_status != "success") {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "vkCreateFence returned " + resultName(result));
            }
        }
        if (operations_ok) {
            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1U;
            submit_info.pCommandBuffers = &command_buffer;
            result = queue_submit(queue, 1U, &submit_info, fence);
            report.queue_submit_result = resultName(result);
            report.queue_submit_status = result == VK_SUCCESS ? "success" : "failed";
            report.queue_submission_succeeded = result == VK_SUCCESS;
            if (result != VK_SUCCESS) {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "vkQueueSubmit returned " + resultName(result));
            }
        }
        if (operations_ok) {
            constexpr std::uint64_t timeout_ns = 10000000000ULL;
            result = wait_for_fences(device, 1U, &fence, VK_TRUE, timeout_ns);
            report.fence_wait_result = resultName(result);
            report.fence_wait_status = result == VK_SUCCESS ? "success" : "failed";
            if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
            if (result != VK_SUCCESS) {
                operations_ok = false;
                setFailure(report, "vkWaitForFences returned " + resultName(result));
            } else {
                result = get_fence_status(device, fence);
                report.fence_status_result = resultName(result);
                report.fence_status_status = result == VK_SUCCESS ? "success" : "failed";
                report.fence_signaled = result == VK_SUCCESS;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                if (result != VK_SUCCESS) {
                    operations_ok = false;
                    setFailure(report, "vkGetFenceStatus returned " + resultName(result));
                }
            }
        }
        if (operations_ok) {
            void* mapped = nullptr;
            result = map_memory(device, destination_memory, 0U,
                static_cast<VkDeviceSize>(report.transfer_size_bytes), 0U, &mapped);
            report.destination_map_status =
                (result == VK_SUCCESS && mapped != nullptr) ? "success" : "failed";
            if (report.destination_map_status != "success") {
                operations_ok = false;
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "Destination vkMapMemory returned " + resultName(result));
            } else {
                const auto* actual = static_cast<const std::uint8_t*>(mapped);
                report.data_match = true;
                for (std::size_t i = 0U; i < expected.size(); ++i) {
                    if (actual[i] != expected[i]) {
                        report.data_match = false;
                        report.has_mismatch_offset = true;
                        report.first_mismatch_offset = static_cast<std::uint64_t>(i);
                        break;
                    }
                }
                report.bytes_copied = report.transfer_size_bytes;
                report.bytes_verified = report.data_match ? report.transfer_size_bytes :
                    report.first_mismatch_offset;
                report.readback_status = report.data_match ? "success" : "mismatch";
                unmap_memory(device, destination_memory);
                report.destination_unmap_status = "success";
                if (!report.data_match) {
                    operations_ok = false;
                    setFailure(report, "Readback bytes did not match the deterministic source pattern");
                }
            }
        }
    }

    if (report.queue_submission_succeeded && !report.fence_signaled) {
        const VkResult idle_result = device_wait_idle(device);
        report.device_idle_status = idle_result == VK_SUCCESS ? "success" : "failed";
        if (idle_result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
        if (idle_result != VK_SUCCESS) setFailure(report, "vkDeviceWaitIdle returned " + resultName(idle_result));
    } else {
        report.device_idle_status = "not_required";
    }

    if (fence != VK_NULL_HANDLE) {
        destroy_fence(device, fence, nullptr);
        report.fence_destroyed = true;
    }
    if (command_buffer != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        free_command_buffers(device, pool, 1U, &command_buffer);
        report.command_buffer_freed = true;
    }
    if (pool != VK_NULL_HANDLE) {
        destroy_command_pool(device, pool, nullptr);
        report.command_pool_destroyed = true;
    }
    if (source_buffer != VK_NULL_HANDLE) {
        destroy_buffer(device, source_buffer, nullptr);
        report.source_buffer_destroyed = true;
    }
    if (destination_buffer != VK_NULL_HANDLE) {
        destroy_buffer(device, destination_buffer, nullptr);
        report.destination_buffer_destroyed = true;
    }
    if (source_memory != VK_NULL_HANDLE) {
        free_memory(device, source_memory, nullptr);
        report.source_memory_freed = true;
    }
    if (destination_memory != VK_NULL_HANDLE) {
        free_memory(device, destination_memory, nullptr);
        report.destination_memory_freed = true;
    }
    if (destroy_device != nullptr) {
        destroy_device(device, nullptr);
        report.logical_device_destroyed = true;
    }
    if (destroy_instance != nullptr) {
        destroy_instance(instance, nullptr);
        report.instance_destroyed = true;
    }

    const bool cleanup_ok = report.logical_device_destroyed && report.instance_destroyed &&
        (fence == VK_NULL_HANDLE || report.fence_destroyed) &&
        (command_buffer == VK_NULL_HANDLE || report.command_buffer_freed) &&
        (pool == VK_NULL_HANDLE || report.command_pool_destroyed) &&
        (source_buffer == VK_NULL_HANDLE || report.source_buffer_destroyed) &&
        (destination_buffer == VK_NULL_HANDLE || report.destination_buffer_destroyed) &&
        (source_memory == VK_NULL_HANDLE || report.source_memory_freed) &&
        (destination_memory == VK_NULL_HANDLE || report.destination_memory_freed);
    report.cleanup_status = cleanup_ok ? "success" : "failed";
    if (!cleanup_ok) setFailure(report, "One or more Vulkan objects could not be confirmed destroyed");

    const bool operations_complete = report.logical_device_status == "success" &&
        report.queue_handle_non_null &&
        report.source_buffer_status == "success" &&
        report.destination_buffer_status == "success" &&
        report.source_memory_type_status == "success" &&
        report.destination_memory_type_status == "success" &&
        report.source_memory_allocate_status == "success" &&
        report.destination_memory_allocate_status == "success" &&
        report.source_memory_bind_status == "success" &&
        report.destination_memory_bind_status == "success" &&
        report.source_map_status == "success" &&
        report.source_unmap_status == "success" &&
        report.command_pool_status == "success" &&
        report.command_buffer_status == "success" &&
        report.command_recording_status == "success" &&
        report.queue_submit_status == "success" &&
        report.fence_create_status == "success" &&
        report.fence_wait_status == "success" &&
        report.fence_status_status == "success" &&
        report.fence_signaled &&
        report.destination_map_status == "success" &&
        report.destination_unmap_status == "success" &&
        report.readback_status == "success" &&
        report.bytes_copied == report.transfer_size_bytes &&
        report.bytes_verified == report.transfer_size_bytes &&
        report.data_match && !report.device_lost;
    report.status = operations_complete && cleanup_ok ? "success" : "failed";
    return report;
#endif
}

} // namespace nyamist::discovery
