#pragma once

#include "nyamist/runtime/component.hpp"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <string>

namespace nyamist::runtime {

struct VulkanDriverProbeSnapshot {
    std::string driver_name;
    std::string driver_info;
    std::string api_version;
    std::string gpu_id;
    std::string architecture;
    std::string model;
    std::string build_id;
    std::string sha256;
    std::string evidence_source = "external Vulkan driver report";

    bool load_success = false;
    std::uint64_t device_count = 0;
    std::uint64_t probe_pass = 0;
    std::uint64_t probe_fail = 0;
    std::uint64_t probe_skip = 0;

    bool gralloc_status_reported = false;
    bool gralloc_hwmodule = false;
    bool conformance_status_reported = false;
    bool conformant = false;

    bool render_present_passed = false;
    std::uint64_t render_present_frames = 0;
    std::uint64_t test_duration_seconds = 0;
    int process_exit_code = -1;
    std::uint64_t device_lost_events = 0;
    bool device_lost_status_reported = false;
    bool sustained_test_passed = false;
};

class VulkanDriverAdapter final : public IComponentProbe {
public:
    explicit VulkanDriverAdapter(VulkanDriverProbeSnapshot snapshot);

    [[nodiscard]] ComponentReport inspect() const override;
    [[nodiscard]] static VulkanDriverProbeSnapshot parseReport(std::istream& input);
    [[nodiscard]] static VulkanDriverAdapter fromReportFile(
        const std::filesystem::path& path);

private:
    VulkanDriverProbeSnapshot snapshot_;
};

} // namespace nyamist::runtime
