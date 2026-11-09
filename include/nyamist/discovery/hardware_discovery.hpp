#pragma once

#include <string>
#include <utility>
#include <vector>

namespace nyamist::discovery {

struct Observation {
    std::string value;
    std::string source;
    std::string confidence;
};

struct QueueFamilyObservation {
    unsigned int index{};
    unsigned int queue_count{};
    unsigned int flags{};
};

struct MemoryHeapObservation {
    unsigned int index{};
    unsigned long long size_bytes{};
    bool device_local{};
};

struct FormatObservation {
    std::string name;
    unsigned int linear_tiling_features{};
    unsigned int optimal_tiling_features{};
    unsigned int buffer_features{};
};

struct VulkanDeviceObservation {
    std::string name;
    std::string device_type;
    std::string vendor_id_hex;
    std::string device_id_hex;
    std::string api_version;
    unsigned int driver_version{};
    std::string driver_name;
    std::string driver_info;
    std::vector<QueueFamilyObservation> queue_families;
    std::vector<std::string> extensions;
    std::vector<MemoryHeapObservation> memory_heaps;
    std::vector<FormatObservation> formats;
};

struct DiscoveryReport {
    unsigned int schema_version{1};
    std::string captured_at_utc;
    std::string platform;
    std::vector<std::pair<std::string, Observation>> device_fields;
    bool vulkan_compiled{};
    std::string vulkan_status{"unavailable"};
    std::string vulkan_loader_api_version;
    std::string vulkan_failure_reason;
    std::vector<VulkanDeviceObservation> vulkan_devices;
};

// Bounded, read-only platform discovery. It does not open GPU device nodes.
[[nodiscard]] DiscoveryReport collectHardwareDiscoveryReport();
[[nodiscard]] std::string toJson(const DiscoveryReport& report);

} // namespace nyamist::discovery
