#pragma once

#include "nyamist/runtime/session_snapshot.hpp"
#include "nyamist/runtime/runtime_coordinator.hpp"
#include "nyamist/runtime/component.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nyamist::runtime {

// A compact, versioned summary linking a device session to the components
// evaluated for one graphics API. It stores evidence; it never activates them.
struct RuntimeCompatibilityProfile {
    std::uint32_t schema_version = 1U;
    std::uint64_t captured_at_unix_ms = 0U;
    std::string device_profile_key;
    GraphicsApi graphics_api = GraphicsApi::d3d11;
    std::string translator_id;
    std::string translator_version = "unknown";
    ValidationLevel translator_validation = ValidationLevel::unavailable;
    std::string vulkan_driver_id;
    std::string vulkan_driver_version = "unknown";
    ValidationLevel vulkan_driver_validation = ValidationLevel::unavailable;
    bool smoke_test_allowed = false;
    bool production_ready = false;
    std::vector<std::string> selected_components;
    std::vector<std::string> blockers;
    std::vector<std::string> warnings;
    std::vector<std::string> capabilities;
};

// Content-addressed, bounded persistence for NYAMIST-owned device sessions
// and compatibility summaries. Files are independent of upstream driver and
// translator caches. Invalid, oversized, truncated, or unknown-schema files
// are rejected rather than partially restored.
class RuntimeProfileStore {
public:
    explicit RuntimeProfileStore(std::filesystem::path root);

    [[nodiscard]] std::string saveSession(const DeviceSessionSnapshot& session) const;
    [[nodiscard]] std::optional<DeviceSessionSnapshot> loadSession(
        std::string_view profile_key) const;
    [[nodiscard]] std::string saveCompatibility(
        const RuntimeCompatibilityProfile& profile) const;
    [[nodiscard]] std::optional<RuntimeCompatibilityProfile> loadCompatibility(
        std::string_view profile_key) const;

    [[nodiscard]] std::vector<std::string> listSessionProfiles() const;
    [[nodiscard]] std::vector<std::string> listCompatibilityProfiles() const;

private:
    std::filesystem::path root_;
};

} // namespace nyamist::runtime
