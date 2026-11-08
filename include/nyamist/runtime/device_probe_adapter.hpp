#pragma once

#include "nyamist/runtime/component.hpp"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <map>
#include <string>
#include <vector>

namespace nyamist::runtime {

struct DeviceFactsSnapshot {
    std::string manufacturer;
    std::string brand;
    std::string model;
    std::string soc_model;
    std::string hardware;
    std::string board_platform;
    std::string android_release;
    std::uint32_t android_api = 0;
    std::string kernel_release;
    std::string primary_abi;
    std::string abi_list;
    std::uint32_t logical_cpu_count = 0;
    std::uint64_t total_memory_bytes = 0;
    std::uint64_t available_memory_bytes = 0;
    bool memory_stats_reported = false;
};

struct VulkanCapabilitySnapshot {
    std::string enumeration_status;
    std::string failure_reason;
    std::uint32_t device_count = 0;
    std::string device_name;
    std::uint32_t vendor_id = 0;
    std::uint32_t device_id = 0;
    std::string instance_api_version;
    std::string device_api_version;
    std::string driver_name;
    std::string driver_info;
    std::string driver_version;
    bool extensions_reported = false;
    std::vector<std::string> supported_extensions;
    std::map<std::string, bool> features;
};

// A bounded, versioned import format for observations collected by an Android
// app/launcher or a separate Vulkan probe. Parsing this report never opens a
// Vulkan device or changes the active driver.
struct DeviceProbeSnapshot {
    std::uint32_t schema_version = 1;
    std::string captured_at;
    std::string evidence_source;
    DeviceFactsSnapshot device;
    VulkanCapabilitySnapshot vulkan;
};

class DeviceProbeReportParser {
public:
    [[nodiscard]] static DeviceProbeSnapshot parse(std::istream& input);
    [[nodiscard]] static DeviceProbeSnapshot fromFile(const std::filesystem::path& path);
};

class DeviceFactsAdapter final : public IComponentProbe {
public:
    explicit DeviceFactsAdapter(DeviceProbeSnapshot snapshot);
    [[nodiscard]] ComponentReport inspect() const override;

private:
    DeviceProbeSnapshot snapshot_;
};

class VulkanCapabilityAdapter final : public IComponentProbe {
public:
    explicit VulkanCapabilityAdapter(DeviceProbeSnapshot snapshot);
    [[nodiscard]] ComponentReport inspect() const override;

private:
    DeviceProbeSnapshot snapshot_;
};

} // namespace nyamist::runtime
