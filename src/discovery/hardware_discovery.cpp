#include "nyamist/discovery/hardware_discovery.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

#if defined(__unix__) || defined(__APPLE__) || defined(__ANDROID__)
#include <sys/utsname.h>
#include <unistd.h>
#endif

#if defined(NYAMIST_HAS_VULKAN)
#include <vulkan/vulkan.h>
#endif

namespace nyamist::discovery {
namespace {

void addField(DiscoveryReport& report, std::string key, std::string value,
              std::string source, std::string confidence = "direct") {
    if (!value.empty()) {
        report.device_fields.push_back({
            std::move(key), {std::move(value), std::move(source), std::move(confidence)}
        });
    }
}

std::string utcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::string readTextFile(const char* path) {
    std::ifstream input(path);
    if (!input) return {};
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

std::string readAndroidProperty(const char* name) {
#if defined(__ANDROID__)
    char value[PROP_VALUE_MAX]{};
    const int length = __system_property_get(name, value);
    return length > 0 ? std::string(value, static_cast<std::size_t>(length)) : std::string{};
#else
    (void)name;
    return {};
#endif
}

std::string architectureName() {
#if defined(__aarch64__)
    return "arm64-v8a";
#elif defined(__arm__)
    return "armeabi-v7a";
#elif defined(__x86_64__)
    return "x86_64";
#elif defined(__i386__)
    return "x86";
#else
    return {};
#endif
}

void collectPlatformFacts(DiscoveryReport& report) {
    report.captured_at_utc = utcTimestamp();
#if defined(__ANDROID__)
    report.platform = "android";
    addField(report, "manufacturer", readAndroidProperty("ro.product.manufacturer"),
             "Android system property: ro.product.manufacturer");
    addField(report, "brand", readAndroidProperty("ro.product.brand"),
             "Android system property: ro.product.brand");
    addField(report, "model", readAndroidProperty("ro.product.model"),
             "Android system property: ro.product.model");
    addField(report, "soc_model", readAndroidProperty("ro.soc.model"),
             "Android system property: ro.soc.model");
    addField(report, "hardware", readAndroidProperty("ro.hardware"),
             "Android system property: ro.hardware");
    addField(report, "board_platform", readAndroidProperty("ro.board.platform"),
             "Android system property: ro.board.platform");
    addField(report, "android_release", readAndroidProperty("ro.build.version.release"),
             "Android system property: ro.build.version.release");
    addField(report, "android_api", readAndroidProperty("ro.build.version.sdk"),
             "Android system property: ro.build.version.sdk");
    addField(report, "primary_abi", readAndroidProperty("ro.product.cpu.abi"),
             "Android system property: ro.product.cpu.abi");
    addField(report, "abi_list", readAndroidProperty("ro.product.cpu.abilist"),
             "Android system property: ro.product.cpu.abilist");
#elif defined(__linux__)
    report.platform = "linux";
#elif defined(_WIN32)
    report.platform = "windows";
#else
    report.platform = "unknown";
#endif

    addField(report, "compiled_abi", architectureName(), "Compiler target architecture");
#if defined(__unix__) || defined(__APPLE__) || defined(__ANDROID__)
    struct utsname system_info {};
    if (uname(&system_info) == 0) {
        addField(report, "kernel_release", system_info.release, "uname(2): release");
        addField(report, "system_name", system_info.sysname, "uname(2): sysname");
        addField(report, "machine", system_info.machine, "uname(2): machine");
    }
    const long processors = sysconf(_SC_NPROCESSORS_ONLN);
    if (processors > 0) {
        addField(report, "logical_cpu_count", std::to_string(processors),
                 "sysconf(_SC_NPROCESSORS_ONLN)");
    }
#endif

    const std::string meminfo = readTextFile("/proc/meminfo");
    if (!meminfo.empty()) {
        std::istringstream lines(meminfo);
        std::string line;
        unsigned long long total_kib = 0ULL;
        unsigned long long available_kib = 0ULL;
        while (std::getline(lines, line)) {
            std::istringstream row(line);
            std::string key;
            unsigned long long value = 0ULL;
            std::string unit;
            if (!(row >> key >> value >> unit) || unit != "kB") continue;
            if (key == "MemTotal:") total_kib = value;
            else if (key == "MemAvailable:") available_kib = value;
        }
        constexpr unsigned long long kib = 1024ULL;
        if (total_kib > 0ULL &&
            total_kib <= std::numeric_limits<unsigned long long>::max() / kib &&
            available_kib > 0ULL && available_kib <= total_kib) {
            addField(report, "total_memory_bytes", std::to_string(total_kib * kib),
                     "/proc/meminfo: MemTotal");
            addField(report, "available_memory_bytes", std::to_string(available_kib * kib),
                     "/proc/meminfo: MemAvailable");
        }
    }
}

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

void writeQuoted(std::ostringstream& out, std::string_view value) {
    out << '"' << jsonEscape(value) << '"';
}

std::string hexValue(unsigned int value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::nouppercase << value;
    return out.str();
}

#if defined(NYAMIST_HAS_VULKAN)
std::string versionString(unsigned int version) {
    std::ostringstream out;
    out << VK_VERSION_MAJOR(version) << '.'
        << VK_VERSION_MINOR(version) << '.'
        << VK_VERSION_PATCH(version);
    return out.str();
}

std::string deviceTypeName(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated_gpu";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete_gpu";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual_gpu";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    default: return "other";
    }
}

std::string resultName(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    default: return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
    }
}

void queryVulkan(DiscoveryReport& report) {
    report.vulkan_compiled = true;
    std::uint32_t loader_version = VK_API_VERSION_1_0;
    const auto enumerate_instance_version =
        reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (enumerate_instance_version != nullptr) {
        const VkResult version_result = enumerate_instance_version(&loader_version);
        if (version_result != VK_SUCCESS) {
            report.vulkan_status = "failed";
            report.vulkan_failure_reason =
                "vkEnumerateInstanceVersion: " + resultName(version_result);
            return;
        }
    }
    report.vulkan_loader_api_version = versionString(loader_version);

    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "CHORD OF NYAMIST R1 Hardware Discovery";
    application_info.applicationVersion = 1U;
    application_info.pEngineName = "NYAMIST Discovery";
    application_info.engineVersion = 1U;
    application_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;

    VkInstance instance = VK_NULL_HANDLE;
    const VkResult create_result = vkCreateInstance(&create_info, nullptr, &instance);
    if (create_result != VK_SUCCESS) {
        report.vulkan_status = "failed";
        report.vulkan_failure_reason = "vkCreateInstance: " + resultName(create_result);
        return;
    }

    std::uint32_t device_count = 0U;
    VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (result != VK_SUCCESS || device_count == 0U) {
        report.vulkan_status = "failed";
        report.vulkan_failure_reason = result == VK_SUCCESS
            ? "vkEnumeratePhysicalDevices returned zero devices"
            : "vkEnumeratePhysicalDevices(count): " + resultName(result);
        vkDestroyInstance(instance, nullptr);
        return;
    }

    std::vector<VkPhysicalDevice> physical_devices(device_count);
    result = vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());
    if (result != VK_SUCCESS) {
        report.vulkan_status = "failed";
        report.vulkan_failure_reason =
            "vkEnumeratePhysicalDevices(data): " + resultName(result);
        vkDestroyInstance(instance, nullptr);
        return;
    }
    physical_devices.resize(device_count);

    for (const VkPhysicalDevice physical_device : physical_devices) {
        VulkanDeviceObservation device;
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        device.name = properties.deviceName;
        device.device_type = deviceTypeName(properties.deviceType);
        device.vendor_id_hex = hexValue(properties.vendorID);
        device.device_id_hex = hexValue(properties.deviceID);
        device.api_version = versionString(properties.apiVersion);
        device.driver_version = properties.driverVersion;

#if defined(VK_KHR_driver_properties)
        const auto get_properties2 =
            reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
                vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2"));
        if (get_properties2 != nullptr) {
            VkPhysicalDeviceDriverProperties driver_properties{};
            driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            VkPhysicalDeviceProperties2 properties2{};
            properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties2.pNext = &driver_properties;
            get_properties2(physical_device, &properties2);
            device.driver_name = driver_properties.driverName;
            device.driver_info = driver_properties.driverInfo;
        }
#endif

        std::uint32_t queue_count = 0U;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queue_count);
        if (queue_count > 0U) {
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, queues.data());
            queues.resize(queue_count);
        }
        for (std::uint32_t index = 0U; index < queue_count; ++index) {
            device.queue_families.push_back({
                index, queues[index].queueCount,
                static_cast<unsigned int>(queues[index].queueFlags)
            });
        }

        std::uint32_t extension_count = 0U;
        result = vkEnumerateDeviceExtensionProperties(
            physical_device, nullptr, &extension_count, nullptr);
        if (result == VK_SUCCESS && extension_count > 0U) {
            std::vector<VkExtensionProperties> extensions(extension_count);
            result = vkEnumerateDeviceExtensionProperties(
                physical_device, nullptr, &extension_count, extensions.data());
            if (result == VK_SUCCESS) {
                extensions.resize(extension_count);
                for (const auto& extension : extensions) {
                    device.extensions.emplace_back(extension.extensionName);
                }
                std::sort(device.extensions.begin(), device.extensions.end());
            }
        }

        VkPhysicalDeviceMemoryProperties memory{};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory);
        for (std::uint32_t index = 0U; index < memory.memoryHeapCount; ++index) {
            const auto& heap = memory.memoryHeaps[index];
            device.memory_heaps.push_back({
                index, static_cast<unsigned long long>(heap.size),
                (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0U
            });
        }

        struct FormatEntry { VkFormat format; const char* name; };
        constexpr FormatEntry formats[] = {
            {VK_FORMAT_R8G8B8A8_UNORM, "R8G8B8A8_UNORM"},
            {VK_FORMAT_B8G8R8A8_UNORM, "B8G8R8A8_UNORM"},
            {VK_FORMAT_R16G16B16A16_SFLOAT, "R16G16B16A16_SFLOAT"},
            {VK_FORMAT_D32_SFLOAT, "D32_SFLOAT"}
        };
        for (const auto& format : formats) {
            VkFormatProperties format_properties{};
            vkGetPhysicalDeviceFormatProperties(
                physical_device, format.format, &format_properties);
            device.formats.push_back({
                format.name,
                static_cast<unsigned int>(format_properties.linearTilingFeatures),
                static_cast<unsigned int>(format_properties.optimalTilingFeatures),
                static_cast<unsigned int>(format_properties.bufferFeatures)
            });
        }
        report.vulkan_devices.push_back(std::move(device));
    }

    report.vulkan_status = report.vulkan_devices.empty() ? "failed" : "success";
    if (report.vulkan_devices.empty()) {
        report.vulkan_failure_reason = "Vulkan returned no physical-device records";
    }
    vkDestroyInstance(instance, nullptr);
}
#else
void queryVulkan(DiscoveryReport& report) {
    report.vulkan_compiled = false;
    report.vulkan_status = "unavailable";
    report.vulkan_failure_reason =
        "This build has no Vulkan development headers/loader integration; no GPU capability was inferred";
}
#endif

} // namespace

DiscoveryReport collectHardwareDiscoveryReport() {
    DiscoveryReport report;
    collectPlatformFacts(report);
    queryVulkan(report);
    return report;
}

std::string toJson(const DiscoveryReport& report) {
    std::ostringstream out;
    out << "{\n  \"schema_version\": " << report.schema_version
        << ",\n  \"project\": \"CHORD OF NYAMIST / NYAMIST G925 R1\",\n  \"captured_at_utc\": ";
    writeQuoted(out, report.captured_at_utc);
    out << ",\n  \"platform\": ";
    writeQuoted(out, report.platform);
    out << ",\n  \"device\": {\n";
    for (std::size_t index = 0U; index < report.device_fields.size(); ++index) {
        const auto& [key, value] = report.device_fields[index];
        out << "    ";
        writeQuoted(out, key);
        out << ": {\"value\": ";
        writeQuoted(out, value.value);
        out << ", \"source\": ";
        writeQuoted(out, value.source);
        out << ", \"confidence\": ";
        writeQuoted(out, value.confidence);
        out << "}" << (index + 1U == report.device_fields.size() ? "\n" : ",\n");
    }
    out << "  },\n  \"vulkan\": {\n"
        << "    \"compiled_with_vulkan\": " << (report.vulkan_compiled ? "true" : "false")
        << ",\n    \"enumeration_status\": ";
    writeQuoted(out, report.vulkan_status);
    out << ",\n    \"loader_api_version\": ";
    if (report.vulkan_loader_api_version.empty()) out << "null";
    else writeQuoted(out, report.vulkan_loader_api_version);
    out << ",\n    \"failure_reason\": ";
    if (report.vulkan_failure_reason.empty()) out << "null";
    else writeQuoted(out, report.vulkan_failure_reason);
    out << ",\n    \"physical_devices\": [";
    if (!report.vulkan_devices.empty()) out << '\n';

    for (std::size_t device_index = 0U; device_index < report.vulkan_devices.size(); ++device_index) {
        const auto& device = report.vulkan_devices[device_index];
        out << "      {\n        \"name\": ";
        writeQuoted(out, device.name);
        out << ",\n        \"device_type\": ";
        writeQuoted(out, device.device_type);
        out << ",\n        \"vendor_id\": ";
        writeQuoted(out, device.vendor_id_hex);
        out << ",\n        \"device_id\": ";
        writeQuoted(out, device.device_id_hex);
        out << ",\n        \"api_version\": ";
        writeQuoted(out, device.api_version);
        out << ",\n        \"driver_version_raw\": " << device.driver_version
            << ",\n        \"driver_name\": ";
        if (device.driver_name.empty()) out << "null"; else writeQuoted(out, device.driver_name);
        out << ",\n        \"driver_info\": ";
        if (device.driver_info.empty()) out << "null"; else writeQuoted(out, device.driver_info);
        out << ",\n        \"queue_families\": [";
        for (std::size_t index = 0U; index < device.queue_families.size(); ++index) {
            const auto& queue = device.queue_families[index];
            if (index > 0U) out << ", ";
            out << "{\"index\": " << queue.index << ", \"queue_count\": " << queue.queue_count
                << ", \"flags_raw\": " << queue.flags << "}";
        }
        out << "],\n        \"extensions\": [";
        for (std::size_t index = 0U; index < device.extensions.size(); ++index) {
            if (index > 0U) out << ", ";
            writeQuoted(out, device.extensions[index]);
        }
        out << "],\n        \"memory_heaps\": [";
        for (std::size_t index = 0U; index < device.memory_heaps.size(); ++index) {
            const auto& heap = device.memory_heaps[index];
            if (index > 0U) out << ", ";
            out << "{\"index\": " << heap.index << ", \"size_bytes\": " << heap.size_bytes
                << ", \"device_local\": " << (heap.device_local ? "true" : "false") << "}";
        }
        out << "],\n        \"formats\": [";
        for (std::size_t index = 0U; index < device.formats.size(); ++index) {
            const auto& format = device.formats[index];
            if (index > 0U) out << ", ";
            out << "{\"name\": ";
            writeQuoted(out, format.name);
            out << ", \"linear_tiling_features_raw\": " << format.linear_tiling_features
                << ", \"optimal_tiling_features_raw\": " << format.optimal_tiling_features
                << ", \"buffer_features_raw\": " << format.buffer_features << "}";
        }
        out << "]\n      }"
            << (device_index + 1U == report.vulkan_devices.size() ? "\n" : ",\n");
    }
    if (!report.vulkan_devices.empty()) out << "    ";
    out << "]\n  }\n}\n";
    return out.str();
}

} // namespace nyamist::discovery
