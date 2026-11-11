#include "nyamist/discovery/command_submission_validation.hpp"

#include <cstdint>
#include <iomanip>
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

#if defined(NYAMIST_HAS_VULKAN)
std::string resultName(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    default:
        return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
    }
}
#endif

void setFailure(CommandSubmissionValidationReport& report, const std::string& reason) {
    if (report.failure_reason.empty()) report.failure_reason = reason;
}

} // namespace

std::string toJson(const CommandSubmissionValidationReport& report) {
    std::ostringstream out;
    out << "{\n"
        << "  \"schema_version\": " << report.schema_version << ",\n"
        << "  \"project\": \"CHORD OF NYAMIST / NYAMIST G925 R1.3\",\n"
        << "  \"status\": ";
    quoted(out, report.status);
    out << ",\n  \"vulkan_compiled\": " << (report.vulkan_compiled ? "true" : "false")
        << ",\n  \"loader_library\": ";
    if (report.loader_library.empty()) out << "null"; else quoted(out, report.loader_library);
    out << ",\n  \"loader_api_version\": ";
    if (report.loader_api_version.empty()) out << "null"; else quoted(out, report.loader_api_version);
    out << ",\n  \"platform_scope\": ";
    quoted(out, report.platform_scope);
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
        << "\n  },\n  \"command_pool\": {\n    \"create_status\": ";
    quoted(out, report.command_pool_status);
    out << "\n  },\n  \"command_buffer\": {\n    \"allocation_status\": ";
    quoted(out, report.command_buffer_status);
    out << ",\n    \"recording_status\": ";
    quoted(out, report.command_recording_status);
    out << "\n  },\n  \"queue_submission\": {\n    \"status\": ";
    quoted(out, report.queue_submit_status);
    out << ",\n    \"result\": ";
    if (report.queue_submit_result.empty()) out << "null"; else quoted(out, report.queue_submit_result);
    out << ",\n    \"submitted\": " << (report.queue_submission_succeeded ? "true" : "false")
        << "\n  },\n  \"fence\": {\n    \"create_status\": ";
    quoted(out, report.fence_create_status);
    out << ",\n    \"wait_status\": ";
    quoted(out, report.fence_wait_status);
    out << ",\n    \"wait_result\": ";
    if (report.fence_wait_result.empty()) out << "null"; else quoted(out, report.fence_wait_result);
    out << ",\n    \"status_query\": ";
    quoted(out, report.fence_status_status);
    out << ",\n    \"status_result\": ";
    if (report.fence_status_result.empty()) out << "null"; else quoted(out, report.fence_status_result);
    out << ",\n    \"signaled\": " << (report.fence_signaled ? "true" : "false")
        << "\n  },\n  \"device_lost\": " << (report.device_lost ? "true" : "false")
        << ",\n  \"device_idle\": ";
    quoted(out, report.device_idle_status);
    out << ",\n  \"cleanup\": {\n    \"status\": ";
    quoted(out, report.cleanup_status);
    out << ",\n    \"fence_destroyed\": " << (report.fence_destroyed ? "true" : "false")
        << ",\n    \"command_buffer_freed\": " << (report.command_buffer_freed ? "true" : "false")
        << ",\n    \"command_pool_destroyed\": " << (report.command_pool_destroyed ? "true" : "false")
        << ",\n    \"logical_device_destroyed\": " << (report.logical_device_destroyed ? "true" : "false")
        << ",\n    \"instance_destroyed\": " << (report.instance_destroyed ? "true" : "false")
        << "\n  },\n  \"failure_reason\": ";
    if (report.failure_reason.empty()) out << "null"; else quoted(out, report.failure_reason);
    out << "\n}\n";
    return out.str();
}

CommandSubmissionValidationReport collectCommandSubmissionValidationReport() {
    CommandSubmissionValidationReport report;
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
    app.pApplicationName = "CHORD OF NYAMIST R1.3 Command Submission";
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
    auto create_device = reinterpret_cast<PFN_vkCreateDevice>(
        get_instance_proc_addr(instance, "vkCreateDevice"));
    auto get_device_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        get_instance_proc_addr(instance, "vkGetDeviceProcAddr"));

    if (destroy_instance == nullptr || enumerate_physical_devices == nullptr ||
        get_properties == nullptr || get_queue_families == nullptr ||
        create_device == nullptr || get_device_proc_addr == nullptr) {
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
    const bool dispatch_ready = get_device_queue != nullptr &&
        create_command_pool != nullptr && destroy_command_pool != nullptr &&
        allocate_command_buffers != nullptr && free_command_buffers != nullptr &&
        begin_command_buffer != nullptr && end_command_buffer != nullptr &&
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
        if (!report.queue_handle_non_null) {
            report.status = "failed";
            setFailure(report, "vkGetDeviceQueue returned VK_NULL_HANDLE");
        } else {
            VkCommandPoolCreateInfo pool_info{};
            pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool_info.queueFamilyIndex = selected_family;
            result = create_command_pool(device, &pool_info, nullptr, &pool);
            report.command_pool_status =
                (result == VK_SUCCESS && pool != VK_NULL_HANDLE) ? "success" : "failed";
            if (report.command_pool_status != "success") {
                report.status = "failed";
                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                setFailure(report, "vkCreateCommandPool returned " + resultName(result));
            } else {
                VkCommandBufferAllocateInfo allocate_info{};
                allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocate_info.commandPool = pool;
                allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocate_info.commandBufferCount = 1U;
                result = allocate_command_buffers(device, &allocate_info, &command_buffer);
                report.command_buffer_status =
                    (result == VK_SUCCESS && command_buffer != VK_NULL_HANDLE) ? "success" : "failed";
                if (report.command_buffer_status != "success") {
                    report.status = "failed";
                    if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                    setFailure(report, "vkAllocateCommandBuffers returned " + resultName(result));
                } else {
                    VkCommandBufferBeginInfo begin_info{};
                    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                    result = begin_command_buffer(command_buffer, &begin_info);
                    if (result == VK_SUCCESS) result = end_command_buffer(command_buffer);
                    report.command_recording_status = result == VK_SUCCESS ? "success" : "failed";
                    if (result != VK_SUCCESS) {
                        report.status = "failed";
                        if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                        setFailure(report, "Command-buffer begin/end returned " + resultName(result));
                    } else {
                        VkFenceCreateInfo fence_info{};
                        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                        result = create_fence(device, &fence_info, nullptr, &fence);
                        report.fence_create_status =
                            (result == VK_SUCCESS && fence != VK_NULL_HANDLE) ? "success" : "failed";
                        if (report.fence_create_status != "success") {
                            report.status = "failed";
                            if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                            setFailure(report, "vkCreateFence returned " + resultName(result));
                        } else {
                            VkSubmitInfo submit_info{};
                            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                            submit_info.commandBufferCount = 1U;
                            submit_info.pCommandBuffers = &command_buffer;
                            result = queue_submit(queue, 1U, &submit_info, fence);
                            report.queue_submit_result = resultName(result);
                            report.queue_submit_status = result == VK_SUCCESS ? "success" : "failed";
                            report.queue_submission_succeeded = result == VK_SUCCESS;
                            if (result != VK_SUCCESS) {
                                report.status = "failed";
                                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                                setFailure(report, "vkQueueSubmit returned " + resultName(result));
                            } else {
                                constexpr std::uint64_t timeout_ns = 10000000000ULL;
                                result = wait_for_fences(device, 1U, &fence, VK_TRUE, timeout_ns);
                                report.fence_wait_result = resultName(result);
                                report.fence_wait_status = result == VK_SUCCESS ? "success" : "failed";
                                if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                                if (result != VK_SUCCESS) {
                                    report.status = "failed";
                                    setFailure(report, "vkWaitForFences returned " + resultName(result));
                                } else {
                                    result = get_fence_status(device, fence);
                                    report.fence_status_result = resultName(result);
                                    report.fence_status_status = result == VK_SUCCESS ? "success" : "failed";
                                    report.fence_signaled = result == VK_SUCCESS;
                                    if (result == VK_ERROR_DEVICE_LOST) report.device_lost = true;
                                    if (result != VK_SUCCESS) {
                                        report.status = "failed";
                                        setFailure(report, "vkGetFenceStatus returned " + resultName(result));
                                    }
                                }
                            }
                        }
                    }
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
        (pool == VK_NULL_HANDLE || report.command_pool_destroyed) &&
        (command_buffer == VK_NULL_HANDLE || report.command_buffer_freed);
    report.cleanup_status = cleanup_ok ? "success" : "failed";
    if (!cleanup_ok) setFailure(report, "One or more Vulkan objects could not be confirmed destroyed");

    const bool operations_ok = report.logical_device_status == "success" &&
        report.queue_handle_non_null && report.command_pool_status == "success" &&
        report.command_buffer_status == "success" &&
        report.command_recording_status == "success" &&
        report.queue_submit_status == "success" &&
        report.fence_create_status == "success" &&
        report.fence_wait_status == "success" &&
        report.fence_status_status == "success" && report.fence_signaled;
    report.status = operations_ok && cleanup_ok ? "success" : "failed";
    return report;
#endif
}

} // namespace nyamist::discovery
