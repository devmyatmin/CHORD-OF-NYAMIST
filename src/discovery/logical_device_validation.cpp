#include "nyamist/discovery/logical_device_validation.hpp"

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

std::optional<std::uint32_t> selectGraphicsQueueFamily(
    const std::vector<QueueFamilyCandidate>& queue_families) {
    for (const auto& family : queue_families) {
        if (family.queue_count > 0U &&
            (family.flags & kVulkanGraphicsQueueBit) != 0U) {
            return family.index;
        }
    }
    return std::nullopt;
}

std::string toJson(const LogicalDeviceValidationReport& report) {
    std::ostringstream out;
    out << "{\n"
        << "  \"schema_version\": " << report.schema_version << ",\n"
        << "  \"project\": \"CHORD OF NYAMIST / NYAMIST G925 R1.2\",\n"
        << "  \"status\": ";
    writeQuoted(out, report.status);
    out << ",\n  \"vulkan_compiled\": "
        << (report.vulkan_compiled ? "true" : "false")
        << ",\n  \"loader_library\": ";
    if (report.loader_library.empty()) out << "null";
    else writeQuoted(out, report.loader_library);
    out << ",\n  \"loader_api_version\": ";
    if (report.loader_api_version.empty()) out << "null";
    else writeQuoted(out, report.loader_api_version);
    out << ",\n  \"platform_scope\": ";
    writeQuoted(out, report.platform_scope);
    out << ",\n  \"physical_device\": {\n"
        << "    \"query_status\": ";
    writeQuoted(out, report.physical_device_status);
    out << ",\n    \"name\": ";
    if (report.device_name.empty()) out << "null";
    else writeQuoted(out, report.device_name);
    out << ",\n    \"vendor_id\": ";
    if (report.vendor_id_hex.empty()) out << "null";
    else writeQuoted(out, report.vendor_id_hex);
    out << ",\n    \"device_id\": ";
    if (report.device_id_hex.empty()) out << "null";
    else writeQuoted(out, report.device_id_hex);
    out << ",\n    \"api_version\": ";
    if (report.physical_device_api_version.empty()) out << "null";
    else writeQuoted(out, report.physical_device_api_version);
    out << ",\n    \"target_gpu_name_match\": "
        << (report.target_gpu_match ? "true" : "false") << "\n  },\n"
        << "  \"queue_families\": [";
    if (!report.queue_families.empty()) out << '\n';
    for (std::size_t i = 0U; i < report.queue_families.size(); ++i) {
        const auto& family = report.queue_families[i];
        out << "    {\"index\": " << family.index
            << ", \"queue_count\": " << family.queue_count
            << ", \"flags_raw\": " << family.flags << "}"
            << (i + 1U == report.queue_families.size() ? "\n" : ",\n");
    }
    out << "  ],\n  \"queue_selection\": {\n"
        << "    \"status\": ";
    writeQuoted(out, report.queue_selection_status);
    out << ",\n    \"selected\": "
        << (report.has_selected_queue_family ? "true" : "false")
        << ",\n    \"family_index\": ";
    if (report.has_selected_queue_family) out << report.selected_queue_family_index;
    else out << "null";
    out << ",\n    \"queue_count\": ";
    if (report.has_selected_queue_family) out << report.selected_queue_count;
    else out << "null";
    out << ",\n    \"flags_raw\": ";
    if (report.has_selected_queue_family) out << report.selected_queue_flags;
    else out << "null";
    out << "\n  },\n  \"logical_device\": {\n    \"create_status\": ";
    writeQuoted(out, report.logical_device_status);
    out << ",\n    \"create_result\": ";
    if (report.logical_device_result.empty()) out << "null";
    else writeQuoted(out, report.logical_device_result);
    out << ",\n    \"enabled_device_extension_count\": "
        << report.enabled_device_extension_count
        << ",\n    \"enabled_core_features\": ";
    writeQuoted(out, report.enabled_core_features);
    out << "\n  },\n  \"queue_retrieval\": {\n    \"status\": ";
    writeQuoted(out, report.queue_retrieval_status);
    out << ",\n    \"result\": ";
    if (report.queue_retrieval_result.empty()) out << "null";
    else writeQuoted(out, report.queue_retrieval_result);
    out << ",\n    \"queue_handle_non_null\": "
        << (report.queue_handle_non_null ? "true" : "false")
        << "\n  },\n  \"cleanup\": {\n    \"status\": ";
    writeQuoted(out, report.cleanup_status);
    out << ",\n    \"logical_device_destroyed\": "
        << (report.logical_device_destroyed ? "true" : "false")
        << "\n  },\n  \"failure_reason\": ";
    if (report.failure_reason.empty()) out << "null";
    else writeQuoted(out, report.failure_reason);
    out << "\n}\n";
    return out.str();
}

LogicalDeviceValidationReport collectLogicalDeviceValidationReport() {
    LogicalDeviceValidationReport report;
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
    std::vector<QueueFamilyCandidate> selected_families;
    bool first_device_seen = false;

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
        const bool should_select = !first_device_seen ||
            (target_name && !selected_target_device) ||
            (has_graphics && selectGraphicsQueueFamily(selected_families) == std::nullopt);
        if (should_select) {
            first_device_seen = true;
            selected_physical_device = physical_device;
            selected_target_device = target_name;
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

    if (get_queue == nullptr) {
        report.queue_retrieval_status = "failed";
        report.queue_retrieval_result = "vkGetDeviceQueue entry point unavailable";
    } else {
        VkQueue queue = VK_NULL_HANDLE;
        get_queue(device, family->index, 0U, &queue);
        report.queue_handle_non_null = queue != VK_NULL_HANDLE;
        report.queue_retrieval_status = report.queue_handle_non_null ? "success" : "failed";
        report.queue_retrieval_result = report.queue_handle_non_null
            ? "VK_SUCCESS (non-null queue handle)"
            : "vkGetDeviceQueue returned VK_NULL_HANDLE";
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
        report.queue_retrieval_status == "success" &&
        report.logical_device_destroyed ? "success" : "failed";
    if (report.status == "failed" && report.failure_reason.empty()) {
        report.failure_reason = report.queue_retrieval_result.empty()
            ? "Logical-device or queue validation did not complete"
            : report.queue_retrieval_result;
    }
    return report;
#endif
}

} // namespace nyamist::discovery
