#include "nyamist/discovery/queue_submission_validation.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
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

void writeQuoted(std::ostringstream& out, const std::string& value) {
    out << '"' << jsonEscape(value) << '"';
}

std::string hexValue(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::nouppercase << value;
    return out.str();
}

bool isG925Name(const std::string& name) {
    return name.find("G925") != std::string::npos ||
           name.find("g925") != std::string::npos;
}

} // namespace

QueueSubmissionValidationReport collectQueueSubmissionValidationReport() {
    QueueSubmissionValidationReport report;
#if !defined(NYAMIST_HAS_VULKAN)
    report.status = "unavailable";
    report.failure_reason =
        "This build has no Vulkan headers/loader integration; no device state was inferred";
    return report;
#else
    report.vulkan_compiled = true;

#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
    struct VulkanLibraryHandle {
        void* handle{};
        std::string name;
        ~VulkanLibraryHandle() {
            if (handle != nullptr) dlclose(handle);
        }
    } library;

    PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;
    const char* candidates[] = {"libvulkan.so", "libvulkan.so.1"};
    std::string load_errors;
    for (const char* candidate : candidates) {
        void* handle = dlopen(candidate, RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            const char* error = dlerror();
            if (!load_errors.empty()) load_errors += "; ";
            load_errors += candidate;
            load_errors += ": ";
            load_errors += error != nullptr ? error : "dlopen failed";
            continue;
        }
        dlerror();
        auto symbol = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            dlsym(handle, "vkGetInstanceProcAddr"));
        const char* symbol_error = dlerror();
        if (symbol == nullptr || symbol_error != nullptr) {
            if (!load_errors.empty()) load_errors += "; ";
            load_errors += candidate;
            load_errors += ": ";
            load_errors += symbol_error != nullptr
                ? symbol_error : "vkGetInstanceProcAddr is null";
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
        report.failure_reason = "Unable to load the public Vulkan loader: " + load_errors;
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
        report.failure_reason = "vkCreateInstance is unavailable from the Vulkan loader";
        return report;
    }

    std::uint32_t loader_version = VK_API_VERSION_1_0;
#if defined(VK_VERSION_1_1)
    if (enumerate_instance_version != nullptr) {
        const VkResult version_result = enumerate_instance_version(&loader_version);
        if (version_result != VK_SUCCESS) {
            report.status = "failed";
            report.failure_reason = "vkEnumerateInstanceVersion returned VkResult(" +
                std::to_string(static_cast<int>(version_result)) + ")";
            return report;
        }
    }
#else
    (void)enumerate_instance_version;
#endif
    report.loader_api_version = std::to_string(VK_VERSION_MAJOR(loader_version)) + "." +
        std::to_string(VK_VERSION_MINOR(loader_version)) + "." +
        std::to_string(VK_VERSION_PATCH(loader_version));

    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "CHORD OF NYAMIST R1.2 Device Validation";
    application_info.applicationVersion = 1U;
    application_info.pEngineName = "NYAMIST R1.2 Runtime Probe";
    application_info.engineVersion = 1U;
    application_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &application_info;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = create_instance(&instance_info, nullptr, &instance);
    if (result != VK_SUCCESS || instance == VK_NULL_HANDLE) {
        report.status = "failed";
        report.failure_reason = "vkCreateInstance failed with VkResult(" +
            std::to_string(static_cast<int>(result)) + ")";
        return report;
    }

    auto destroy_instance = reinterpret_cast<PFN_vkDestroyInstance>(
        get_instance_proc_addr(instance, "vkDestroyInstance"));
    auto enumerate_physical_devices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        get_instance_proc_addr(instance, "vkEnumeratePhysicalDevices"));
    auto get_physical_device_properties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
            get_instance_proc_addr(instance, "vkGetPhysicalDeviceProperties"));
    auto get_queue_family_properties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
            get_instance_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    auto create_device = reinterpret_cast<PFN_vkCreateDevice>(
        get_instance_proc_addr(instance, "vkCreateDevice"));
    auto get_device_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        get_instance_proc_addr(instance, "vkGetDeviceProcAddr"));

    const auto destroy_instance_if_available = [&]() {
        if (destroy_instance != nullptr) {
            destroy_instance(instance, nullptr);
        }
    };

    if (destroy_instance == nullptr || enumerate_physical_devices == nullptr ||
        get_physical_device_properties == nullptr || get_queue_family_properties == nullptr ||
        create_device == nullptr || get_device_proc_addr == nullptr) {
        report.status = "failed";
        report.failure_reason =
            "A required Vulkan instance/device entry point is unavailable";
        destroy_instance_if_available();
        return report;
    }

    std::uint32_t physical_device_count = 0U;
    result = enumerate_physical_devices(instance, &physical_device_count, nullptr);
    if (result != VK_SUCCESS || physical_device_count == 0U) {
        report.status = "failed";
        report.physical_device_status = "failed";
        report.failure_reason = result == VK_SUCCESS
            ? "vkEnumeratePhysicalDevices returned zero devices"
            : "vkEnumeratePhysicalDevices returned VkResult(" +
                std::to_string(static_cast<int>(result)) + ")";
        destroy_instance_if_available();
        return report;
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    result = enumerate_physical_devices(
        instance, &physical_device_count, physical_devices.data());
    if (result != VK_SUCCESS) {
        report.status = "failed";
        report.physical_device_status = "failed";
        report.failure_reason = "vkEnumeratePhysicalDevices(data) returned VkResult(" +
            std::to_string(static_cast<int>(result)) + ")";
        destroy_instance_if_available();
        return report;
    }
    physical_devices.resize(physical_device_count);

    VkPhysicalDevice selected_physical_device = VK_NULL_HANDLE;
    bool selected_target_device = false;
    bool selected_device_has_graphics = false;
    std::vector<QueueFamilyCandidate> selected_families;

    for (const VkPhysicalDevice physical_device : physical_devices) {
        VkPhysicalDeviceProperties properties{};
        get_physical_device_properties(physical_device, &properties);

        std::uint32_t queue_count = 0U;
        get_queue_family_properties(physical_device, &queue_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_properties(queue_count);
        if (queue_count > 0U) {
            get_queue_family_properties(
                physical_device, &queue_count, queue_properties.data());
            queue_properties.resize(queue_count);
        }

        std::vector<QueueFamilyCandidate> families;
        families.reserve(queue_properties.size());
        for (std::uint32_t index = 0U; index < queue_properties.size(); ++index) {
            families.push_back({
                index,
                queue_properties[index].queueCount,
                static_cast<std::uint32_t>(queue_properties[index].queueFlags)
            });
        }

        const bool target_name = isG925Name(properties.deviceName);
        const bool has_graphics = selectGraphicsQueueFamily(families).has_value();
        const bool should_select = target_name
            ? (!selected_target_device || (has_graphics && !selected_device_has_graphics))
            : (!selected_target_device &&
               (selected_physical_device == VK_NULL_HANDLE ||
                (has_graphics && !selected_device_has_graphics)));
        if (should_select) {
            selected_physical_device = physical_device;
            selected_target_device = target_name;
            selected_device_has_graphics = has_graphics;
            selected_families = std::move(families);
            report.device_name = properties.deviceName;
            report.vendor_id_hex = hexValue(properties.vendorID);
            report.device_id_hex = hexValue(properties.deviceID);
            report.physical_device_api_version =
                std::to_string(VK_VERSION_MAJOR(properties.apiVersion)) + "." +
                std::to_string(VK_VERSION_MINOR(properties.apiVersion)) + "." +
                std::to_string(VK_VERSION_PATCH(properties.apiVersion));
            report.target_gpu_match = target_name;
        }
        if (target_name && has_graphics) break;
    }

    if (selected_physical_device == VK_NULL_HANDLE) {
        report.status = "failed";
        report.physical_device_status = "failed";
        report.failure_reason = "No physical device was available for logical-device validation";
        destroy_instance_if_available();
        return report;
    }
    report.physical_device_status = "success";
    report.queue_families = selected_families;

    const auto selected_family = selectGraphicsQueueFamily(selected_families);
    if (!selected_family.has_value()) {
        report.status = "failed";
        report.queue_selection_status = "failed";
        report.failure_reason =
            "The selected physical device exposes no non-empty graphics-capable queue family";
        destroy_instance_if_available();
        return report;
    }

    const auto family = std::find_if(
        selected_families.begin(), selected_families.end(),
        [&selected_family](const QueueFamilyCandidate& candidate) {
            return candidate.index == *selected_family;
        });
    if (family == selected_families.end()) {
        report.status = "failed";
        report.queue_selection_status = "failed";
        report.failure_reason = "Selected queue-family index was not present in the inventory";
        destroy_instance_if_available();
        return report;
    }

    report.queue_selection_status = "success";
    report.has_selected_queue_family = true;
    report.selected_queue_family_index = family->index;
    report.selected_queue_count = family->queue_count;
    report.selected_queue_flags = family->flags;

    constexpr float queue_priority = 1.0F;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = family->index;
    queue_info.queueCount = 1U;
    queue_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1U;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 0U;
    device_info.ppEnabledExtensionNames = nullptr;
    device_info.pEnabledFeatures = nullptr;

    VkDevice device = VK_NULL_HANDLE;
    result = create_device(selected_physical_device, &device_info, nullptr, &device);
    if (result != VK_SUCCESS || device == VK_NULL_HANDLE) {
        report.status = "failed";
        report.logical_device_status = "failed";
        report.logical_device_result = "VkResult(" +
            std::to_string(static_cast<int>(result)) + ")";
        report.failure_reason = "vkCreateDevice failed with " + report.logical_device_result;
        destroy_instance_if_available();
        return report;
    }
    report.logical_device_status = "success";
    report.logical_device_result = "VK_SUCCESS";

    auto get_queue = reinterpret_cast<PFN_vkGetDeviceQueue>(
        get_device_proc_addr(device, "vkGetDeviceQueue"));
    auto destroy_device = reinterpret_cast<PFN_vkDestroyDevice>(
        get_device_proc_addr(device, "vkDestroyDevice"));
    if (destroy_device == nullptr) {
        destroy_device = reinterpret_cast<PFN_vkDestroyDevice>(
            get_instance_proc_addr(instance, "vkDestroyDevice"));
    }

    VkQueue queue = VK_NULL_HANDLE;
    if (get_queue == nullptr) {
        report.queue_retrieval_status = "failed";
        report.queue_retrieval_result = "vkGetDeviceQueue entry point unavailable";
    } else {
        get_queue(device, family->index, 0U, &queue);
        report.queue_handle_non_null = queue != VK_NULL_HANDLE;
        report.queue_retrieval_status = report.queue_handle_non_null ? "success" : "failed";
        report.queue_retrieval_result = report.queue_handle_non_null
            ? "VK_SUCCESS (non-null queue handle)"
            : "vkGetDeviceQueue returned VK_NULL_HANDLE";
    }

    auto create_command_pool = reinterpret_cast<PFN_vkCreateCommandPool>(
        get_device_proc_addr(device, "vkCreateCommandPool"));
    auto destroy_command_pool = reinterpret_cast<PFN_vkDestroyCommandPool>(
        get_device_proc_addr(device, "vkDestroyCommandPool"));
    auto allocate_command_buffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
        get_device_proc_addr(device, "vkAllocateCommandBuffers"));
    auto begin_command_buffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(
        get_device_proc_addr(device, "vkBeginCommandBuffer"));
    auto end_command_buffer = reinterpret_cast<PFN_vkEndCommandBuffer>(
        get_device_proc_addr(device, "vkEndCommandBuffer"));
    auto create_fence = reinterpret_cast<PFN_vkCreateFence>(
        get_device_proc_addr(device, "vkCreateFence"));
    auto destroy_fence = reinterpret_cast<PFN_vkDestroyFence>(
        get_device_proc_addr(device, "vkDestroyFence"));
    auto queue_submit = reinterpret_cast<PFN_vkQueueSubmit>(
        get_device_proc_addr(device, "vkQueueSubmit"));
    auto wait_for_fences = reinterpret_cast<PFN_vkWaitForFences>(
        get_device_proc_addr(device, "vkWaitForFences"));
    auto queue_wait_idle = reinterpret_cast<PFN_vkQueueWaitIdle>(
        get_device_proc_addr(device, "vkQueueWaitIdle"));

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    const bool command_entrypoints_available =
        create_command_pool != nullptr && destroy_command_pool != nullptr &&
        allocate_command_buffers != nullptr && begin_command_buffer != nullptr &&
        end_command_buffer != nullptr && create_fence != nullptr &&
        destroy_fence != nullptr && queue_submit != nullptr &&
        wait_for_fences != nullptr && queue_wait_idle != nullptr;

    if (!report.queue_handle_non_null) {
        report.command_pool_status = "not_attempted";
        report.command_failure_reason =
            "A valid graphics queue is required before command submission";
    } else if (!command_entrypoints_available) {
        report.command_pool_status = "failed";
        report.command_failure_reason =
            "One or more Vulkan command-submission entry points are unavailable";
    } else {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool_info.queueFamilyIndex = family->index;

        result = create_command_pool(device, &pool_info, nullptr, &command_pool);
        report.command_pool_created =
            result == VK_SUCCESS && command_pool != VK_NULL_HANDLE;
        report.command_pool_status =
            report.command_pool_created ? "success" : "failed";
        if (!report.command_pool_created) {
            report.command_failure_reason = "vkCreateCommandPool returned VkResult(" +
                std::to_string(static_cast<int>(result)) + ")";
        }
    }

    if (report.command_pool_created) {
        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1U;

        result = allocate_command_buffers(device, &allocate_info, &command_buffer);
        report.command_buffer_allocated =
            result == VK_SUCCESS && command_buffer != VK_NULL_HANDLE;
        report.command_buffer_allocation_status =
            report.command_buffer_allocated ? "success" : "failed";
        if (!report.command_buffer_allocated && report.command_failure_reason.empty()) {
            report.command_failure_reason =
                "vkAllocateCommandBuffers returned VkResult(" +
                std::to_string(static_cast<int>(result)) + ")";
        }
    }

    if (report.command_buffer_allocated) {
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        result = begin_command_buffer(command_buffer, &begin_info);
        if (result == VK_SUCCESS) {
            result = end_command_buffer(command_buffer);
        }
        report.command_buffer_recorded = result == VK_SUCCESS;
        report.command_buffer_record_status =
            report.command_buffer_recorded ? "success" : "failed";
        if (!report.command_buffer_recorded && report.command_failure_reason.empty()) {
            report.command_failure_reason =
                "vkBeginCommandBuffer/vkEndCommandBuffer returned VkResult(" +
                std::to_string(static_cast<int>(result)) + ")";
        }
    }

    if (report.command_buffer_recorded) {
        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        result = create_fence(device, &fence_info, nullptr, &fence);
        report.fence_created = result == VK_SUCCESS && fence != VK_NULL_HANDLE;
        report.fence_creation_status =
            report.fence_created ? "success" : "failed";
        if (!report.fence_created && report.command_failure_reason.empty()) {
            report.command_failure_reason = "vkCreateFence returned VkResult(" +
                std::to_string(static_cast<int>(result)) + ")";
        }
    }

    if (report.fence_created) {
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1U;
        submit_info.pCommandBuffers = &command_buffer;

        result = queue_submit(queue, 1U, &submit_info, fence);
        report.queue_submit_accepted = result == VK_SUCCESS;
        report.queue_submit_status =
            report.queue_submit_accepted ? "success" : "failed";
        if (!report.queue_submit_accepted && report.command_failure_reason.empty()) {
            report.command_failure_reason = "vkQueueSubmit returned VkResult(" +
                std::to_string(static_cast<int>(result)) + ")";
        }
    }

    bool queue_is_idle = !report.queue_submit_accepted;
    if (report.queue_submit_accepted) {
        constexpr std::uint64_t fence_timeout_ns = 5'000'000'000ULL;
        result = wait_for_fences(device, 1U, &fence, VK_TRUE, fence_timeout_ns);
        report.fence_signaled = result == VK_SUCCESS;
        report.fence_wait_status =
            report.fence_signaled ? "success" : "failed";
        queue_is_idle = report.fence_signaled;
        if (!report.fence_signaled) {
            if (report.command_failure_reason.empty()) {
                report.command_failure_reason = "vkWaitForFences returned VkResult(" +
                    std::to_string(static_cast<int>(result)) + ")";
            }
            const VkResult idle_result = queue_wait_idle(queue);
            queue_is_idle = idle_result == VK_SUCCESS ||
                            idle_result == VK_ERROR_DEVICE_LOST;
            if (!queue_is_idle && report.command_failure_reason.empty()) {
                report.command_failure_reason =
                    "Queue remained non-idle after failed fence wait";
            }
        }
    }

    bool fence_cleanup_ok = true;
    if (fence != VK_NULL_HANDLE) {
        if (destroy_fence != nullptr) {
            destroy_fence(device, fence, nullptr);
        } else {
            fence_cleanup_ok = false;
        }
    }

    bool pool_cleanup_ok = true;
    if (command_pool != VK_NULL_HANDLE) {
        if (queue_is_idle && destroy_command_pool != nullptr) {
            destroy_command_pool(device, command_pool, nullptr);
        } else {
            pool_cleanup_ok = false;
        }
    }

    report.command_resources_destroyed =
        fence_cleanup_ok && pool_cleanup_ok;
    report.command_resource_cleanup_status =
        report.command_resources_destroyed ? "success" : "failed";
    if (!report.command_resources_destroyed && report.command_failure_reason.empty()) {
        report.command_failure_reason =
            "Command-pool/fence cleanup could not be confirmed";
    }

    if (destroy_device != nullptr) {
        destroy_device(device, nullptr);
        report.cleanup_status = "success";
        report.logical_device_destroyed = true;
    } else {
        report.cleanup_status = "failed";
        report.failure_reason =
            "vkDestroyDevice entry point unavailable; device cleanup could not be confirmed";
    }
    destroy_instance_if_available();

    report.status = report.logical_device_status == "success" &&
        report.queue_retrieval_status == "success" && report.logical_device_destroyed &&
        report.command_pool_status == "success" &&
        report.command_buffer_allocation_status == "success" &&
        report.command_buffer_record_status == "success" &&
        report.fence_creation_status == "success" &&
        report.queue_submit_status == "success" &&
        report.fence_wait_status == "success" &&
        report.command_resource_cleanup_status == "success"
            ? "success" : "failed";
    if (report.status == "failed" && report.failure_reason.empty()) {
        report.failure_reason = !report.command_failure_reason.empty()
            ? report.command_failure_reason
            : (report.queue_retrieval_result.empty()
                ? "R1.3 logical-device/queue/submit validation did not complete"
                : report.queue_retrieval_result);
    }
    return report;
#endif
}

std::string toJson(const QueueSubmissionValidationReport& report) {
    const auto& base = static_cast<const LogicalDeviceValidationReport&>(report);
    std::string base_json = toJson(base);
    if (!base_json.empty() && base_json.back() == '\n') base_json.pop_back();

    std::ostringstream out;
    out << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"project\": \"CHORD OF NYAMIST / NYAMIST G925 R1.3\",\n"
        << "  \"status\": ";
    writeQuoted(out, report.status);
    out << ",\n  \"logical_device_validation\": " << base_json
        << ",\n  \"command_pool\": {\n"
        << "    \"status\": ";
    writeQuoted(out, report.command_pool_status);
    out << ",\n    \"created\": "
        << (report.command_pool_created ? "true" : "false")
        << "\n  },\n  \"command_buffer\": {\n"
        << "    \"allocation_status\": ";
    writeQuoted(out, report.command_buffer_allocation_status);
    out << ",\n    \"allocated\": "
        << (report.command_buffer_allocated ? "true" : "false")
        << ",\n    \"record_status\": ";
    writeQuoted(out, report.command_buffer_record_status);
    out << ",\n    \"recorded\": "
        << (report.command_buffer_recorded ? "true" : "false")
        << "\n  },\n  \"queue_submission\": {\n"
        << "    \"fence_creation_status\": ";
    writeQuoted(out, report.fence_creation_status);
    out << ",\n    \"fence_created\": "
        << (report.fence_created ? "true" : "false")
        << ",\n    \"submit_status\": ";
    writeQuoted(out, report.queue_submit_status);
    out << ",\n    \"submit_accepted\": "
        << (report.queue_submit_accepted ? "true" : "false")
        << ",\n    \"fence_wait_status\": ";
    writeQuoted(out, report.fence_wait_status);
    out << ",\n    \"fence_signaled\": "
        << (report.fence_signaled ? "true" : "false")
        << ",\n    \"fence_timeout_ns\": 5000000000"
        << "\n  },\n  \"command_resource_cleanup\": {\n"
        << "    \"status\": ";
    writeQuoted(out, report.command_resource_cleanup_status);
    out << ",\n    \"resources_destroyed\": "
        << (report.command_resources_destroyed ? "true" : "false")
        << "\n  },\n  \"command_failure_reason\": ";
    if (report.command_failure_reason.empty()) out << "null";
    else writeQuoted(out, report.command_failure_reason);
    out << "\n}\n";
    return out.str();
}

} // namespace nyamist::discovery
