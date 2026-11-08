#include "nyamist/runtime/translator_adapters.hpp"

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
    return first >= last ? std::string{} : std::string(first, last);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

const std::string& required(const std::map<std::string, std::string>& values,
                            const std::string& key) {
    const auto it = values.find(key);
    if (it == values.end() || it->second.empty()) {
        throw std::invalid_argument("missing required translator report field: " + key);
    }
    return it->second;
}

std::string optional(const std::map<std::string, std::string>& values,
                     const std::string& key,
                     std::string fallback = {}) {
    const auto it = values.find(key);
    return it == values.end() ? std::move(fallback) : it->second;
}

bool parseBool(const std::string& value, const std::string& key) {
    const auto normalized = lower(value);
    if (normalized == "true" || normalized == "1" || normalized == "yes") return true;
    if (normalized == "false" || normalized == "0" || normalized == "no") return false;
    throw std::invalid_argument("invalid boolean in translator report field: " + key);
}

bool optionalBool(const std::map<std::string, std::string>& values,
                  const std::string& key) {
    const auto it = values.find(key);
    return it == values.end() ? false : parseBool(it->second, key);
}

std::uint64_t parseUnsigned(const std::string& value, const std::string& key) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument("invalid non-negative integer in translator report field: " + key);
    }
    return result;
}

std::uint64_t optionalUnsigned(const std::map<std::string, std::string>& values,
                               const std::string& key) {
    const auto it = values.find(key);
    return it == values.end() ? 0U : parseUnsigned(it->second, key);
}

int optionalInt(const std::map<std::string, std::string>& values,
                const std::string& key,
                int fallback) {
    const auto it = values.find(key);
    if (it == values.end()) return fallback;
    int result = 0;
    const auto& value = it->second;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument("invalid integer in translator report field: " + key);
    }
    return result;
}

std::vector<std::string> parseList(const std::string& value) {
    std::vector<std::string> result;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const auto comma = value.find(',', begin);
        const auto end = comma == std::string::npos ? value.size() : comma;
        auto item = trim(value.substr(begin, end - begin));
        if (!item.empty()) result.push_back(std::move(item));
        if (comma == std::string::npos) break;
        begin = comma + 1U;
    }
    return result;
}

void appendListCapability(ComponentReport& report, const std::string& key,
                          const std::vector<std::string>& values) {
    if (values.empty()) return;
    std::string joined;
    for (const auto& value : values) {
        if (!joined.empty()) joined += ',';
        joined += value;
    }
    report.capabilities.emplace_back(key + "=" + joined);
}
} // namespace

TranslatorAdapterBase::TranslatorAdapterBase(std::string expected_id,
                                             std::string expected_api,
                                             TranslatorProbeSnapshot snapshot)
    : expected_id_(std::move(expected_id)),
      expected_api_(std::move(expected_api)),
      snapshot_(std::move(snapshot)) {
    if (snapshot_.translator_id != expected_id_) {
        throw std::invalid_argument("translator report identity does not match adapter");
    }
    if (snapshot_.api != expected_api_) {
        throw std::invalid_argument("translator report API does not match adapter");
    }
    if (snapshot_.translator_version.empty()) {
        throw std::invalid_argument("translator version must not be empty");
    }
}

TranslatorProbeSnapshot TranslatorAdapterBase::parseReport(std::istream& input) {
    std::map<std::string, std::string> values;
    std::string line;
    std::size_t total_bytes = 0;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        total_bytes += line.size() + 1U;
        if (total_bytes > kMaximumReportBytes || line.size() > kMaximumLineBytes) {
            throw std::invalid_argument("translator report exceeds the allowed size");
        }
        auto normalized = trim(line);
        if (normalized.empty() || normalized.front() == '#') continue;

        const auto separator = normalized.find('=');
        if (separator == std::string::npos) {
            throw std::invalid_argument(
                "expected key=value at translator report line " + std::to_string(line_number));
        }
        auto key = trim(normalized.substr(0, separator));
        auto value = trim(normalized.substr(separator + 1U));
        if (key.empty()) {
            throw std::invalid_argument(
                "empty key at translator report line " + std::to_string(line_number));
        }
        if (!values.emplace(std::move(key), std::move(value)).second) {
            throw std::invalid_argument(
                "duplicate key at translator report line " + std::to_string(line_number));
        }
    }
    if (input.bad()) throw std::runtime_error("failed while reading translator report");

    TranslatorProbeSnapshot snapshot;
    snapshot.translator_id = required(values, "translator_id");
    snapshot.api = lower(required(values, "api"));
    snapshot.translator_version = required(values, "translator_version");
    snapshot.device_created = parseBool(required(values, "device_created"), "device_created");
    snapshot.evidence_source = optional(values, "evidence_source", "external translator report");
    snapshot.max_feature_level = optional(values, "max_feature_level");
    snapshot.selected_feature_level = optional(values, "selected_feature_level");
    snapshot.supported_feature_levels = parseList(optional(values, "supported_feature_levels"));
    snapshot.shader_models = parseList(optional(values, "shader_models"));
    snapshot.render_present_passed = optionalBool(values, "render_present_passed");
    snapshot.frame_count = optionalUnsigned(values, "frame_count");
    snapshot.duration_seconds = optionalUnsigned(values, "duration_seconds");
    snapshot.present_calls = optionalUnsigned(values, "present_calls");
    snapshot.present_failures = optionalUnsigned(values, "present_failures");
    snapshot.present_failures_reported = values.contains("present_failures");
    snapshot.process_exit_code = optionalInt(values, "process_exit_code", -1);
    snapshot.device_lost_events = optionalUnsigned(values, "device_lost_events");
    snapshot.device_lost_status_reported = values.contains("device_lost_events");
    snapshot.sustained_test_passed = optionalBool(values, "sustained_test_passed");
    return snapshot;
}

TranslatorProbeSnapshot TranslatorAdapterBase::fromReportFile(
    const std::filesystem::path& path, const std::string& expected_id,
    const std::string& expected_api) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("unable to open translator report: " + path.string());
    auto snapshot = parseReport(input);
    if (snapshot.translator_id != expected_id || snapshot.api != expected_api) {
        throw std::invalid_argument("translator report identity/API does not match requested adapter");
    }
    return snapshot;
}

ComponentReport TranslatorAdapterBase::inspect() const {
    ComponentReport report;
    report.descriptor.id = expected_id_;
    report.descriptor.kind = ComponentKind::translator;
    report.descriptor.version = snapshot_.translator_version;
    report.descriptor.origin = snapshot_.evidence_source;

    const bool render_present_observed =
        snapshot_.device_created && snapshot_.render_present_passed &&
        snapshot_.frame_count > 0 && snapshot_.duration_seconds > 0 &&
        snapshot_.present_calls > 0 && snapshot_.present_failures_reported &&
        snapshot_.present_failures == 0 && snapshot_.process_exit_code == 0 &&
        snapshot_.device_lost_status_reported && snapshot_.device_lost_events == 0;
    const bool sustained_evidence_valid =
        render_present_observed && snapshot_.sustained_test_passed &&
        snapshot_.duration_seconds >= 300U;

    if (sustained_evidence_valid) {
        report.validation = ValidationLevel::sustained_tested;
    } else if (render_present_observed) {
        report.validation = ValidationLevel::smoke_tested;
    } else {
        report.validation = ValidationLevel::discovered;
    }

    report.capabilities.emplace_back("translator.api=" + snapshot_.api);
    report.capabilities.emplace_back(
        "translator.device_created=" + std::string(snapshot_.device_created ? "true" : "false"));
    if (!snapshot_.max_feature_level.empty()) {
        report.capabilities.emplace_back("d3d.max_feature_level=" + snapshot_.max_feature_level);
    }
    if (!snapshot_.selected_feature_level.empty()) {
        report.capabilities.emplace_back(
            "d3d.selected_feature_level=" + snapshot_.selected_feature_level);
    }
    appendListCapability(report, "d3d.supported_feature_levels",
                         snapshot_.supported_feature_levels);
    appendListCapability(report, "d3d.shader_models", snapshot_.shader_models);

    if (snapshot_.frame_count > 0) {
        report.capabilities.emplace_back(
            "render.frames.observed=" + std::to_string(snapshot_.frame_count));
    }
    if (snapshot_.duration_seconds > 0) {
        report.capabilities.emplace_back(
            "render.duration_seconds.observed=" + std::to_string(snapshot_.duration_seconds));
    }
    if (snapshot_.present_calls > 0) {
        report.capabilities.emplace_back(
            "render.present_calls.observed=" + std::to_string(snapshot_.present_calls));
    }
    if (snapshot_.process_exit_code != -1) {
        report.capabilities.emplace_back(
            "process.exit_code=" + std::to_string(snapshot_.process_exit_code));
    }
    if (snapshot_.device_lost_status_reported) {
        report.capabilities.emplace_back(
            "device.lost_events=" + std::to_string(snapshot_.device_lost_events));
    }
    if (render_present_observed) {
        report.capabilities.emplace_back("render.present.validated=true");
    }
    if (!snapshot_.device_created) {
        report.diagnostics.emplace_back(
            "Translator was identified, but device creation was not confirmed.");
    }
    if (snapshot_.present_failures > 0) {
        report.diagnostics.emplace_back(
            "Render evidence rejected because one or more Present calls failed.");
    }
    if (snapshot_.render_present_passed && !snapshot_.present_failures_reported) {
        report.diagnostics.emplace_back(
            "Render evidence rejected because Present-failure status was not reported.");
    }
    if (snapshot_.render_present_passed && !snapshot_.device_lost_status_reported) {
        report.diagnostics.emplace_back(
            "Render evidence rejected because device-lost status was not reported.");
    }
    if (snapshot_.device_lost_events > 0) {
        report.diagnostics.emplace_back(
            "Render evidence rejected because device-lost events were reported.");
    }
    if (snapshot_.sustained_test_passed && snapshot_.duration_seconds < 300U) {
        report.diagnostics.emplace_back(
            "Sustained-test flag ignored: the recorded duration is below 300 seconds.");
    }
    if (snapshot_.render_present_passed && !render_present_observed) {
        report.diagnostics.emplace_back(
            "Render/present evidence is incomplete; require frames, successful Present calls, "
            "zero failures/device-loss events, and a normal process exit.");
    }
    return report;
}

DxvkAdapter::DxvkAdapter(TranslatorProbeSnapshot snapshot)
    : TranslatorAdapterBase("dxvk", "d3d11", std::move(snapshot)) {}

DxvkAdapter DxvkAdapter::fromReportFile(const std::filesystem::path& path) {
    return DxvkAdapter(TranslatorAdapterBase::fromReportFile(path, "dxvk", "d3d11"));
}

Vkd3dProtonAdapter::Vkd3dProtonAdapter(TranslatorProbeSnapshot snapshot)
    : TranslatorAdapterBase("vkd3d-proton", "d3d12", std::move(snapshot)) {}

Vkd3dProtonAdapter Vkd3dProtonAdapter::fromReportFile(const std::filesystem::path& path) {
    return Vkd3dProtonAdapter(
        TranslatorAdapterBase::fromReportFile(path, "vkd3d-proton", "d3d12"));
}

} // namespace nyamist::runtime
