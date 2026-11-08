#include "nyamist/runtime/device_probe_adapter.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

namespace nyamist::runtime {
namespace {

constexpr std::size_t kMaximumReportBytes = 128U * 1024U;
constexpr std::size_t kMaximumLineBytes = 4096U;
constexpr std::size_t kMaximumExtensions = 4096U;
constexpr std::size_t kMaximumFeatures = 256U;

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

const std::string& required(
    const std::map<std::string, std::string>& values,
    const std::string& key) {
    const auto it = values.find(key);
    if (it == values.end() || it->second.empty()) {
        throw std::invalid_argument("missing required device probe field: " + key);
    }
    return it->second;
}

std::string optional(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    std::string fallback = {}) {
    const auto it = values.find(key);
    return it == values.end() ? std::move(fallback) : it->second;
}

std::uint64_t parseUnsigned(
    const std::string& value,
    const std::string& key,
    bool allow_hex = false) {
    if (value.empty()) {
        throw std::invalid_argument("empty unsigned integer in device probe field: " + key);
    }

    int base = 10;
    const char* begin = value.data();
    const char* end = begin + value.size();
    if (allow_hex && value.size() > 2U && value[0] == '0' &&
        (value[1] == 'x' || value[1] == 'X')) {
        base = 16;
        begin += 2;
    }

    std::uint64_t result = 0;
    const auto parsed = std::from_chars(begin, end, result, base);
    if (begin == end || parsed.ec != std::errc{} || parsed.ptr != end) {
        throw std::invalid_argument("invalid unsigned integer in device probe field: " + key);
    }
    return result;
}

std::uint32_t parseUnsigned32(
    const std::string& value,
    const std::string& key,
    bool allow_hex = false) {
    const auto parsed = parseUnsigned(value, key, allow_hex);
    if (parsed > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("integer exceeds 32-bit range in device probe field: " + key);
    }
    return static_cast<std::uint32_t>(parsed);
}

std::uint64_t optionalUnsigned(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    std::uint64_t fallback = 0) {
    const auto it = values.find(key);
    return it == values.end() ? fallback : parseUnsigned(it->second, key);
}

std::uint32_t optionalUnsigned32(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    std::uint32_t fallback = 0) {
    const auto it = values.find(key);
    return it == values.end() ? fallback : parseUnsigned32(it->second, key);
}

bool parseBool(const std::string& value, const std::string& key) {
    const auto normalized = lower(value);
    if (normalized == "true" || normalized == "1" || normalized == "yes") {
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no") {
        return false;
    }
    throw std::invalid_argument("invalid boolean in device probe field: " + key);
}

bool optionalBool(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    bool fallback = false) {
    const auto it = values.find(key);
    return it == values.end() ? fallback : parseBool(it->second, key);
}

bool validFeatureName(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '_' || ch == '.';
    });
}

bool validExtensionName(std::string_view value) {
    return value.size() > 3U && value.substr(0U, 3U) == "VK_" &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) {
               return std::isalnum(ch) != 0 || ch == '_';
           });
}

std::vector<std::string> parseCommaList(
    const std::string& value,
    const std::string& key,
    bool extensions) {
    std::vector<std::string> result;
    std::set<std::string> seen;
    if (value.empty()) {
        return result;
    }

    std::size_t start = 0;
    while (start <= value.size()) {
        const auto separator = value.find(',', start);
        const auto end = separator == std::string::npos ? value.size() : separator;
        const auto item = trim(value.substr(start, end - start));
        if (item.empty()) {
            throw std::invalid_argument("empty list item in device probe field: " + key);
        }
        if ((extensions && !validExtensionName(item)) ||
            (!extensions && item.size() > 128U)) {
            throw std::invalid_argument("invalid list item in device probe field: " + key);
        }
        if (!seen.insert(item).second) {
            throw std::invalid_argument("duplicate list item in device probe field: " + key);
        }
        result.push_back(item);
        if (result.size() > (extensions ? kMaximumExtensions : 32U)) {
            throw std::invalid_argument("too many list items in device probe field: " + key);
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1U;
    }
    return result;
}

bool isKnownKey(const std::string& key) {
    static const std::set<std::string> known = {
        "schema_version", "captured_at", "evidence_source",
        "device.manufacturer", "device.brand", "device.model", "device.soc_model",
        "device.hardware", "device.board_platform", "device.android_release",
        "device.android_api", "device.kernel_release", "device.primary_abi",
        "device.abi_list", "device.logical_cpu_count", "device.total_memory_bytes",
        "device.available_memory_bytes", "vulkan.enumeration_status",
        "vulkan.failure_reason", "vulkan.device_count", "vulkan.device_name",
        "vulkan.vendor_id", "vulkan.device_id", "vulkan.instance_api_version",
        "vulkan.device_api_version", "vulkan.driver_name", "vulkan.driver_info",
        "vulkan.driver_version", "vulkan.extensions_reported",
        "vulkan.supported_extensions"
    };
    if (known.contains(key)) {
        return true;
    }
    constexpr std::string_view prefix = "vulkan.feature.";
    return key.compare(0U, prefix.size(), prefix) == 0 &&
           validFeatureName(std::string_view(key).substr(prefix.size()));
}

void appendIfPresent(
    std::vector<std::string>& output,
    const std::string& key,
    const std::string& value) {
    if (!value.empty()) {
        output.push_back(key + "=" + value);
    }
}

} // namespace

DeviceProbeSnapshot DeviceProbeReportParser::parse(std::istream& input) {
    std::map<std::string, std::string> values;
    std::string line;
    std::size_t total_bytes = 0;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        total_bytes += line.size() + 1U;
        if (total_bytes > kMaximumReportBytes || line.size() > kMaximumLineBytes) {
            throw std::invalid_argument("device probe report exceeds the allowed size");
        }

        auto normalized = trim(line);
        if (normalized.empty() || normalized.front() == '#') {
            continue;
        }
        const auto separator = normalized.find('=');
        if (separator == std::string::npos) {
            throw std::invalid_argument(
                "expected key=value at device probe report line " + std::to_string(line_number));
        }

        auto key = trim(normalized.substr(0U, separator));
        auto value = trim(normalized.substr(separator + 1U));
        if (key.empty() || !isKnownKey(key)) {
            throw std::invalid_argument(
                "unknown or empty key at device probe report line " + std::to_string(line_number));
        }
        if (!values.emplace(std::move(key), std::move(value)).second) {
            throw std::invalid_argument(
                "duplicate key at device probe report line " + std::to_string(line_number));
        }
    }
    if (input.bad()) {
        throw std::runtime_error("failed while reading device probe report");
    }

    DeviceProbeSnapshot snapshot;
    snapshot.schema_version = parseUnsigned32(required(values, "schema_version"), "schema_version");
    if (snapshot.schema_version != 1U) {
        throw std::invalid_argument("unsupported device probe schema_version");
    }
    snapshot.evidence_source = required(values, "evidence_source");
    snapshot.captured_at = optional(values, "captured_at");

    auto& device = snapshot.device;
    device.manufacturer = optional(values, "device.manufacturer");
    device.brand = optional(values, "device.brand");
    device.model = required(values, "device.model");
    device.soc_model = optional(values, "device.soc_model");
    device.hardware = optional(values, "device.hardware");
    device.board_platform = optional(values, "device.board_platform");
    device.android_release = optional(values, "device.android_release");
    device.android_api = parseUnsigned32(required(values, "device.android_api"), "device.android_api");
    device.kernel_release = required(values, "device.kernel_release");
    device.primary_abi = required(values, "device.primary_abi");
    device.abi_list = optional(values, "device.abi_list");
    device.logical_cpu_count = optionalUnsigned32(values, "device.logical_cpu_count");
    device.total_memory_bytes = optionalUnsigned(values, "device.total_memory_bytes");
    device.available_memory_bytes = optionalUnsigned(values, "device.available_memory_bytes");
    device.memory_stats_reported = values.contains("device.total_memory_bytes") ||
                                   values.contains("device.available_memory_bytes");
    if (device.logical_cpu_count == 0U) {
        throw std::invalid_argument("device.logical_cpu_count must be greater than zero");
    }
    const bool total_memory_reported = values.contains("device.total_memory_bytes");
    const bool available_memory_reported = values.contains("device.available_memory_bytes");
    if (total_memory_reported != available_memory_reported ||
        (device.memory_stats_reported &&
         (device.total_memory_bytes == 0U || device.available_memory_bytes > device.total_memory_bytes))) {
        throw std::invalid_argument("device memory counters are incomplete or inconsistent");
    }

    auto& vulkan = snapshot.vulkan;
    vulkan.enumeration_status = lower(required(values, "vulkan.enumeration_status"));
    if (vulkan.enumeration_status != "success" &&
        vulkan.enumeration_status != "unavailable" &&
        vulkan.enumeration_status != "failed") {
        throw std::invalid_argument("invalid vulkan.enumeration_status");
    }
    vulkan.failure_reason = optional(values, "vulkan.failure_reason");
    if (vulkan.enumeration_status == "success") {
        vulkan.device_count = parseUnsigned32(required(values, "vulkan.device_count"), "vulkan.device_count");
        if (vulkan.device_count == 0U) {
            throw std::invalid_argument("successful Vulkan enumeration requires at least one device");
        }
        vulkan.device_name = required(values, "vulkan.device_name");
        vulkan.vendor_id = parseUnsigned32(required(values, "vulkan.vendor_id"), "vulkan.vendor_id", true);
        vulkan.device_id = parseUnsigned32(required(values, "vulkan.device_id"), "vulkan.device_id", true);
        vulkan.device_api_version = required(values, "vulkan.device_api_version");
        vulkan.driver_name = required(values, "vulkan.driver_name");
        vulkan.driver_info = required(values, "vulkan.driver_info");
        vulkan.instance_api_version = optional(values, "vulkan.instance_api_version");
        vulkan.driver_version = optional(values, "vulkan.driver_version");
        vulkan.extensions_reported = optionalBool(values, "vulkan.extensions_reported");
        if (vulkan.extensions_reported) {
            vulkan.supported_extensions = parseCommaList(
                required(values, "vulkan.supported_extensions"), "vulkan.supported_extensions", true);
        } else if (values.contains("vulkan.supported_extensions")) {
            throw std::invalid_argument(
                "vulkan.supported_extensions must not be supplied when extensions_reported is false");
        }
    } else {
        vulkan.device_count = optionalUnsigned32(values, "vulkan.device_count");
        if (vulkan.device_count != 0U) {
            throw std::invalid_argument("non-successful Vulkan enumeration must report zero devices");
        }
    }

    for (const auto& [key, value] : values) {
        constexpr std::string_view prefix = "vulkan.feature.";
        if (key.compare(0U, prefix.size(), prefix) == 0) {
            if (vulkan.enumeration_status != "success") {
                throw std::invalid_argument("Vulkan feature fields require successful enumeration");
            }
            if (vulkan.features.size() >= kMaximumFeatures) {
                throw std::invalid_argument("too many Vulkan feature fields");
            }
            const auto name = key.substr(prefix.size());
            vulkan.features.emplace(name, parseBool(value, key));
        }
    }

    return snapshot;
}

DeviceProbeSnapshot DeviceProbeReportParser::fromFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("unable to open device probe report: " + path.string());
    }
    return parse(input);
}

DeviceFactsAdapter::DeviceFactsAdapter(DeviceProbeSnapshot snapshot)
    : snapshot_(std::move(snapshot)) {}

ComponentReport DeviceFactsAdapter::inspect() const {
    ComponentReport report;
    report.descriptor.id = "device-facts";
    report.descriptor.kind = ComponentKind::monitor;
    report.descriptor.version = "schema-" + std::to_string(snapshot_.schema_version);
    report.descriptor.origin = snapshot_.evidence_source;
    report.validation = ValidationLevel::discovered;

    appendIfPresent(report.capabilities, "device.manufacturer", snapshot_.device.manufacturer);
    appendIfPresent(report.capabilities, "device.brand", snapshot_.device.brand);
    appendIfPresent(report.capabilities, "device.model", snapshot_.device.model);
    appendIfPresent(report.capabilities, "device.soc_model", snapshot_.device.soc_model);
    appendIfPresent(report.capabilities, "device.hardware", snapshot_.device.hardware);
    appendIfPresent(report.capabilities, "device.board_platform", snapshot_.device.board_platform);
    appendIfPresent(report.capabilities, "android.release", snapshot_.device.android_release);
    report.capabilities.emplace_back("android.api=" + std::to_string(snapshot_.device.android_api));
    report.capabilities.emplace_back("kernel.release=" + snapshot_.device.kernel_release);
    report.capabilities.emplace_back("device.primary_abi=" + snapshot_.device.primary_abi);
    appendIfPresent(report.capabilities, "device.abi_list", snapshot_.device.abi_list);
    report.capabilities.emplace_back(
        "device.logical_cpu_count=" + std::to_string(snapshot_.device.logical_cpu_count));
    if (snapshot_.device.memory_stats_reported) {
        report.capabilities.emplace_back(
            "memory.total_bytes=" + std::to_string(snapshot_.device.total_memory_bytes));
        report.capabilities.emplace_back(
            "memory.available_bytes=" + std::to_string(snapshot_.device.available_memory_bytes));
    }
    appendIfPresent(report.capabilities, "capture.timestamp", snapshot_.captured_at);
    report.diagnostics.emplace_back(
        "Device facts are imported observations; unavailable fields are not inferred from the SoC or model name.");
    return report;
}

VulkanCapabilityAdapter::VulkanCapabilityAdapter(DeviceProbeSnapshot snapshot)
    : snapshot_(std::move(snapshot)) {}

ComponentReport VulkanCapabilityAdapter::inspect() const {
    ComponentReport report;
    report.descriptor.id = "vulkan-capabilities";
    report.descriptor.kind = ComponentKind::monitor;
    report.descriptor.version = "schema-" + std::to_string(snapshot_.schema_version);
    report.descriptor.origin = snapshot_.evidence_source;

    if (snapshot_.vulkan.enumeration_status != "success") {
        report.validation = ValidationLevel::unavailable;
        report.diagnostics.emplace_back(
            "Vulkan physical-device enumeration was not successful: " +
            snapshot_.vulkan.enumeration_status);
        appendIfPresent(report.diagnostics, "reason", snapshot_.vulkan.failure_reason);
        return report;
    }

    report.validation = ValidationLevel::discovered;
    appendIfPresent(report.capabilities, "vulkan.device_name", snapshot_.vulkan.device_name);
    report.capabilities.emplace_back(
        "vulkan.device_count=" + std::to_string(snapshot_.vulkan.device_count));
    report.capabilities.emplace_back(
        "vulkan.vendor_id=" + std::to_string(snapshot_.vulkan.vendor_id));
    report.capabilities.emplace_back(
        "vulkan.device_id=" + std::to_string(snapshot_.vulkan.device_id));
    appendIfPresent(report.capabilities, "vulkan.instance_api_version", snapshot_.vulkan.instance_api_version);
    appendIfPresent(report.capabilities, "vulkan.device_api_version", snapshot_.vulkan.device_api_version);
    appendIfPresent(report.capabilities, "vulkan.driver_name", snapshot_.vulkan.driver_name);
    appendIfPresent(report.capabilities, "vulkan.driver_info", snapshot_.vulkan.driver_info);
    appendIfPresent(report.capabilities, "vulkan.driver_version", snapshot_.vulkan.driver_version);

    if (snapshot_.vulkan.extensions_reported) {
        report.capabilities.emplace_back(
            "vulkan.supported_extension_count=" +
            std::to_string(snapshot_.vulkan.supported_extensions.size()));
        for (const auto& extension : snapshot_.vulkan.supported_extensions) {
            report.capabilities.emplace_back("vulkan.extension=" + extension);
        }
    } else {
        report.diagnostics.emplace_back(
            "The Vulkan report does not contain a complete supported-extension list.");
    }
    for (const auto& [feature, supported] : snapshot_.vulkan.features) {
        report.capabilities.emplace_back(
            "vulkan.feature." + feature + "=" + (supported ? "true" : "false"));
    }
    report.diagnostics.emplace_back(
        "Physical-device enumeration and feature queries do not prove queue submission, render/present, or game compatibility.");
    return report;
}

} // namespace nyamist::runtime
