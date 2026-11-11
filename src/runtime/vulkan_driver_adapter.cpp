#include "nyamist/runtime/vulkan_driver_adapter.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace nyamist::runtime {
namespace {

constexpr std::size_t kMaximumReportBytes = 64U * 1024U;
constexpr std::size_t kMaximumLineBytes = 4096U;

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
        throw std::invalid_argument("missing required Vulkan driver report field: " + key);
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

bool parseBool(const std::string& value, const std::string& key) {
    const auto normalized = lower(value);
    if (normalized == "true" || normalized == "1" || normalized == "yes") {
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no") {
        return false;
    }
    throw std::invalid_argument("invalid boolean in Vulkan driver report field: " + key);
}

bool optionalBool(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    bool fallback = false) {
    const auto it = values.find(key);
    return it == values.end() ? fallback : parseBool(it->second, key);
}

std::uint64_t parseUnsigned(const std::string& value, const std::string& key) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument("invalid non-negative integer in Vulkan driver report field: " + key);
    }
    return result;
}

std::uint64_t optionalUnsigned(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    std::uint64_t fallback = 0) {
    const auto it = values.find(key);
    return it == values.end() ? fallback : parseUnsigned(it->second, key);
}

int optionalInt(const std::map<std::string, std::string>& values,
                const std::string& key,
                int fallback) {
    const auto it = values.find(key);
    if (it == values.end()) {
        return fallback;
    }

    int result = 0;
    const auto& value = it->second;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument("invalid integer in Vulkan driver report field: " + key);
    }
    return result;
}

bool isHexDigest(const std::string& value) {
    return value.size() == 64U &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) {
               return std::isxdigit(ch) != 0;
           });
}

} // namespace

VulkanDriverAdapter::VulkanDriverAdapter(VulkanDriverProbeSnapshot snapshot)
    : snapshot_(std::move(snapshot)) {
    if (trim(snapshot_.driver_name).empty()) {
        throw std::invalid_argument("Vulkan driver name must not be empty");
    }
    if (!snapshot_.sha256.empty() && !isHexDigest(snapshot_.sha256)) {
        throw std::invalid_argument("Vulkan driver report SHA-256 must contain 64 hexadecimal characters");
    }
}

VulkanDriverProbeSnapshot VulkanDriverAdapter::parseReport(std::istream& input) {
    std::map<std::string, std::string> values;
    std::string line;
    std::size_t total_bytes = 0;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        total_bytes += line.size() + 1U;
        if (total_bytes > kMaximumReportBytes || line.size() > kMaximumLineBytes) {
            throw std::invalid_argument("Vulkan driver report exceeds the allowed size");
        }

        auto normalized = trim(line);
        if (normalized.empty() || normalized.front() == '#') {
            continue;
        }

        const auto separator = normalized.find('=');
        if (separator == std::string::npos) {
            throw std::invalid_argument(
                "expected key=value at Vulkan driver report line " + std::to_string(line_number));
        }

        auto key = trim(normalized.substr(0, separator));
        auto value = trim(normalized.substr(separator + 1U));
        if (key.empty()) {
            throw std::invalid_argument(
                "empty key at Vulkan driver report line " + std::to_string(line_number));
        }
        if (!values.emplace(std::move(key), std::move(value)).second) {
            throw std::invalid_argument(
                "duplicate key at Vulkan driver report line " + std::to_string(line_number));
        }
    }

    if (input.bad()) {
        throw std::runtime_error("failed while reading Vulkan driver report");
    }

    VulkanDriverProbeSnapshot snapshot;
    snapshot.driver_name = required(values, "driver_name");
    snapshot.driver_info = required(values, "driver_info");
    snapshot.api_version = required(values, "api_version");
    snapshot.gpu_id = required(values, "gpu_id");
    snapshot.load_success = parseBool(required(values, "load_success"), "load_success");
    snapshot.device_count = parseUnsigned(required(values, "device_count"), "device_count");
    snapshot.probe_pass = parseUnsigned(required(values, "probe_pass"), "probe_pass");
    snapshot.probe_fail = parseUnsigned(required(values, "probe_fail"), "probe_fail");
    snapshot.probe_skip = parseUnsigned(required(values, "probe_skip"), "probe_skip");

    snapshot.architecture = optional(values, "architecture");
    snapshot.model = optional(values, "model");
    snapshot.build_id = optional(values, "build_id");
    snapshot.sha256 = optional(values, "sha256");
    snapshot.evidence_source = optional(values, "evidence_source", "external Android Vulkan driver report");

    snapshot.gralloc_status_reported = values.contains("gralloc_hwmodule");
    snapshot.gralloc_hwmodule = optionalBool(values, "gralloc_hwmodule");
    snapshot.conformance_status_reported = values.contains("conformant");
    snapshot.conformant = optionalBool(values, "conformant");

    snapshot.render_present_passed = optionalBool(values, "render_present_passed");
    snapshot.render_present_frames = optionalUnsigned(values, "render_present_frames");
    snapshot.test_duration_seconds = optionalUnsigned(values, "test_duration_seconds");
    snapshot.process_exit_code = optionalInt(values, "process_exit_code", -1);
    snapshot.device_lost_events = optionalUnsigned(values, "device_lost_events");
    snapshot.device_lost_status_reported = values.contains("device_lost_events");
    snapshot.sustained_test_passed = optionalBool(values, "sustained_test_passed");

    return snapshot;
}

VulkanDriverAdapter VulkanDriverAdapter::fromReportFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("unable to open Vulkan driver report: " + path.string());
    }
    return VulkanDriverAdapter(parseReport(input));
}

ComponentReport VulkanDriverAdapter::inspect() const {
    ComponentReport report;
    report.descriptor.id = "vulkan-driver";
    report.descriptor.kind = ComponentKind::vulkan_driver;
    report.descriptor.version = snapshot_.driver_info;
    report.descriptor.origin = snapshot_.evidence_source;

    const bool device_available = snapshot_.load_success && snapshot_.device_count > 0;
    const bool render_present_observed =
        snapshot_.render_present_passed &&
        snapshot_.render_present_frames > 0 &&
        snapshot_.test_duration_seconds > 0 &&
        snapshot_.process_exit_code == 0;
    const bool render_evidence_valid =
        render_present_observed && snapshot_.device_lost_status_reported &&
        snapshot_.device_lost_events == 0;
    const bool sustained_evidence_valid =
        render_evidence_valid &&
        snapshot_.sustained_test_passed &&
        snapshot_.test_duration_seconds >= 300U;

    if (!device_available) {
        report.validation = ValidationLevel::unavailable;
        report.diagnostics.emplace_back(
            "Vulkan driver load/device enumeration was not confirmed by the supplied report.");
    } else if (sustained_evidence_valid) {
        report.validation = ValidationLevel::sustained_tested;
    } else if (render_evidence_valid) {
        report.validation = ValidationLevel::smoke_tested;
    } else {
        report.validation = ValidationLevel::discovered;
    }

    report.capabilities.emplace_back("driver.name=" + snapshot_.driver_name);
    report.capabilities.emplace_back("vulkan.api=" + snapshot_.api_version);
    report.capabilities.emplace_back("gpu.id=" + snapshot_.gpu_id);
    report.capabilities.emplace_back("device.count=" + std::to_string(snapshot_.device_count));
    report.capabilities.emplace_back(
        "probe.pass=" + std::to_string(snapshot_.probe_pass));
    report.capabilities.emplace_back(
        "probe.fail=" + std::to_string(snapshot_.probe_fail));
    report.capabilities.emplace_back(
        "probe.skip=" + std::to_string(snapshot_.probe_skip));

    if (!snapshot_.architecture.empty()) {
        report.capabilities.emplace_back("gpu.architecture=" + snapshot_.architecture);
    }
    if (!snapshot_.model.empty()) {
        report.capabilities.emplace_back("gpu.model=" + snapshot_.model);
    }
    if (!snapshot_.build_id.empty()) {
        report.capabilities.emplace_back("driver.build_id=" + snapshot_.build_id);
    }
    if (!snapshot_.sha256.empty()) {
        report.capabilities.emplace_back("driver.sha256=" + lower(snapshot_.sha256));
    }
    if (snapshot_.gralloc_status_reported) {
        report.capabilities.emplace_back(
            std::string("android.gralloc_hwmodule=") +
            (snapshot_.gralloc_hwmodule ? "true" : "false"));
    }
    if (snapshot_.conformance_status_reported) {
        report.capabilities.emplace_back(
            std::string("vulkan.conformant=") +
            (snapshot_.conformant ? "true" : "false"));
    }
    if (render_evidence_valid) {
        report.capabilities.emplace_back(
            "render.present.frames=" + std::to_string(snapshot_.render_present_frames));
        report.capabilities.emplace_back(
            "render.present.duration_seconds=" +
            std::to_string(snapshot_.test_duration_seconds));
    }

    if (snapshot_.probe_fail > 0) {
        report.diagnostics.emplace_back("Capability probe reported one or more failing tests.");
    }
    if (!snapshot_.conformant && snapshot_.conformance_status_reported) {
        report.diagnostics.emplace_back(
            "Driver report marks this implementation non-conformant/testing-only.");
    }
    if (snapshot_.gralloc_status_reported && !snapshot_.gralloc_hwmodule) {
        report.diagnostics.emplace_back(
            "No gralloc hwmodule was reported; validate Android surface presentation separately.");
    }
    if (device_available && !render_evidence_valid) {
        report.diagnostics.emplace_back(
            "ICD load and device enumeration do not by themselves prove render/present success.");
    }
    if (snapshot_.sustained_test_passed && snapshot_.test_duration_seconds < 300U) {
        report.diagnostics.emplace_back(
            "Sustained-test flag ignored: the recorded duration is below 300 seconds.");
    }
    if (render_present_observed && !snapshot_.device_lost_status_reported) {
        report.diagnostics.emplace_back(
            "Render evidence rejected because device-lost status was not reported.");
    }
    if (render_present_observed && snapshot_.device_lost_events > 0) {
        report.diagnostics.emplace_back(
            "Render evidence rejected because device-lost events were reported.");
    }

    return report;
}

} // namespace nyamist::runtime
