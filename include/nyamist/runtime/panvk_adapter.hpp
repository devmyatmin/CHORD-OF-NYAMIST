#pragma once

#include "nyamist/runtime/component.hpp"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <string>

namespace nyamist::runtime {

struct PanVkProbeSnapshot {
    std::string driver_name;
    std::string driver_info;
    std::string api_version;
    std::string gpu_id;
    std::string architecture;
    std::string model;
    std::string build_id;
    std::string sha256;
    std::string evidence_source = "external PanVK report";

    bool load_success = false;
    std::uint64_t device_count = 0;
    std::uint64_t panprobe_pass = 0;
    std::uint64_t panprobe_fail = 0;
    std::uint64_t panprobe_skip = 0;

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

class PanVkAdapter final : public IComponentProbe {
public:
    explicit PanVkAdapter(PanVkProbeSnapshot snapshot);

    [[nodiscard]] ComponentReport inspect() const override;
    [[nodiscard]] static PanVkProbeSnapshot parseReport(std::istream& input);
    [[nodiscard]] static PanVkAdapter fromReportFile(
        const std::filesystem::path& path);

private:
    PanVkProbeSnapshot snapshot_;
};

} // namespace nyamist::runtime
