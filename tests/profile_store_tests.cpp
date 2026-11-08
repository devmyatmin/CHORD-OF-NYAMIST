#include "nyamist/runtime/profile_store.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

nyamist::runtime::DeviceSessionSnapshot makeSession() {
    using namespace nyamist::runtime;
    DeviceProbeSnapshot probe;
    probe.schema_version = 1U;
    probe.captured_at = "2026-11-08T08:00:00Z";
    probe.evidence_source = "profile-store-test";
    probe.device.manufacturer = "Example";
    probe.device.brand = "Example";
    probe.device.model = "Profile Test Device";
    probe.device.soc_model = "Test SoC";
    probe.device.android_release = "16";
    probe.device.android_api = 36U;
    probe.device.primary_abi = "arm64-v8a";
    probe.device.logical_cpu_count = 8U;
    probe.device.total_memory_bytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    probe.device.available_memory_bytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    probe.device.memory_stats_reported = true;
    probe.vulkan.enumeration_status = "success";
    probe.vulkan.device_count = 1U;
    probe.vulkan.device_name = "Test Vulkan GPU";
    probe.vulkan.vendor_id = 0x13b5U;
    probe.vulkan.device_id = 0xd8300015U;
    probe.vulkan.driver_name = "panvk";
    probe.vulkan.driver_version = "rc5";
    probe.vulkan.extensions_reported = true;
    probe.vulkan.supported_extensions = {"VK_KHR_swapchain", "VK_KHR_timeline_semaphore"};
    probe.vulkan.features.emplace("geometryShader", false);
    probe.vulkan.features.emplace("timelineSemaphore", true);

    std::vector<TelemetryReading> readings = {
        {TelemetryMetric::cpu_utilization_percent, 42, 9900U, "cpu-monitor"},
        {TelemetryMetric::gpu_utilization_percent, 67, 6000U, "gpu-monitor"},
        {TelemetryMetric::frame_time_p95_microseconds, 18000, 9800U, "frame-monitor"}
    };
    auto telemetry = TelemetryAggregator::aggregate(
        probe.device, readings, 10000U, 3000U);
    return DeviceSessionSnapshot(std::move(probe), std::move(telemetry), 10000U);
}

void testSessionRoundTripAndContentAddressing(
    const std::filesystem::path& root,
    std::string& saved_key) {
    using namespace nyamist::runtime;
    RuntimeProfileStore store(root);
    const auto original = makeSession();
    saved_key = store.saveSession(original);
    check(saved_key.size() == 16U, "session profile key must be a 16-character fingerprint");
    check(store.saveSession(original) == saved_key,
          "saving identical session content must be idempotent");

    const auto restored = store.loadSession(saved_key);
    check(restored.has_value(), "saved session profile must load");
    check(restored->schemaVersion() == 1U, "session schema version must be preserved");
    check(restored->capturedAtUnixMs() == 10000U, "session capture timestamp must be preserved");
    check(restored->probe().device.model == "Profile Test Device",
          "device identity must survive a round trip");
    check(restored->probe().vulkan.device_id == 0xd8300015U,
          "Vulkan device identity must survive a round trip");
    check(restored->probe().vulkan.features.at("timelineSemaphore"),
          "Vulkan feature map must survive a round trip");
    check(restored->probe().vulkan.supported_extensions.size() == 2U,
          "Vulkan extension list must survive a round trip");
    check(restored->telemetry().fresh_readings.at(TelemetryMetric::cpu_utilization_percent).value == 42,
          "fresh telemetry must survive a round trip");
    check(restored->telemetry().stale_readings.at(TelemetryMetric::gpu_utilization_percent).value == 67,
          "stale telemetry must remain diagnostic-only after a round trip");
    check(restored->telemetry().diagnostics.size() == original.telemetry().diagnostics.size(),
          "telemetry diagnostics must survive a round trip");
    check(store.listSessionProfiles() == std::vector<std::string>{saved_key},
          "session profile listing must return only valid stored records");
}

void testCompatibilityProfileRoundTrip(
    const std::filesystem::path& root,
    const std::string& session_key) {
    using namespace nyamist::runtime;
    RuntimeProfileStore store(root);
    RuntimeCompatibilityProfile profile;
    profile.captured_at_unix_ms = 11000U;
    profile.device_profile_key = session_key;
    profile.graphics_api = GraphicsApi::d3d11;
    profile.translator_id = "dxvk";
    profile.translator_version = "3.1.1";
    profile.translator_validation = ValidationLevel::smoke_tested;
    profile.vulkan_driver_id = "panvk";
    profile.vulkan_driver_version = "rc5";
    profile.vulkan_driver_validation = ValidationLevel::smoke_tested;
    profile.smoke_test_allowed = true;
    profile.selected_components = {"dxvk", "panvk"};
    profile.warnings = {"sustained validation is still pending"};
    profile.capabilities = {"D3D11_FL11_1", "Vulkan_1.3"};

    const auto key = store.saveCompatibility(profile);
    const auto restored = store.loadCompatibility(key);
    check(restored.has_value(), "saved compatibility profile must load");
    check(restored->device_profile_key == session_key,
          "compatibility profile must retain its linked device session key");
    check(restored->graphics_api == GraphicsApi::d3d11,
          "graphics API must survive a compatibility profile round trip");
    check(restored->translator_validation == ValidationLevel::smoke_tested,
          "translator validation level must survive a round trip");
    check(restored->smoke_test_allowed && !restored->production_ready,
          "smoke-test and production readiness must remain separate");
    check(restored->warnings == profile.warnings &&
              restored->capabilities == profile.capabilities,
          "compatibility evidence must survive a round trip");
    check(store.listCompatibilityProfiles() == std::vector<std::string>{key},
          "compatibility profile listing must return valid records");
}

void testCorruptAndInvalidKeysAreRejected(
    const std::filesystem::path& root,
    const std::string& session_key) {
    using namespace nyamist::runtime;
    RuntimeProfileStore store(root);
    const auto path = root / (session_key + ".nys");
    {
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        check(static_cast<bool>(file), "session profile must be available for corruption test");
        file.seekg(-1, std::ios::end);
        char value = 0;
        file.read(&value, 1);
        check(static_cast<bool>(file), "profile tail byte must be readable");
        value = static_cast<char>(value ^ 0x01);
        file.seekp(-1, std::ios::end);
        file.write(&value, 1);
    }
    check(!store.loadSession(session_key).has_value(),
          "checksum mismatch must reject a corrupt session profile");
    check(store.listSessionProfiles().empty(),
          "profile listing must omit corrupt records");
    check(!store.loadSession("../" + session_key).has_value(),
          "path-like profile keys must be rejected before file access");
}

void testInconsistentCompatibilityStateIsRejected(
    const std::filesystem::path& root,
    const std::string& session_key) {
    using namespace nyamist::runtime;
    RuntimeProfileStore store(root);
    RuntimeCompatibilityProfile invalid;
    invalid.captured_at_unix_ms = 12000U;
    invalid.device_profile_key = session_key;
    invalid.translator_id = "dxvk";
    invalid.vulkan_driver_id = "panvk";
    invalid.smoke_test_allowed = true;
    invalid.blockers = {"Vulkan device enumeration is missing"};
    bool rejected = false;
    try {
        (void)store.saveCompatibility(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "a smoke-test-allowed profile must not contain blockers");
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("nyamist-profile-store-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::string session_key;
        testSessionRoundTripAndContentAddressing(root, session_key);
        testCompatibilityProfileRoundTrip(root, session_key);
        testInconsistentCompatibilityStateIsRejected(root, session_key);
        testCorruptAndInvalidKeysAreRejected(root, session_key);
        std::filesystem::remove_all(root);
        std::cout << "All NYAMIST profile store tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
        std::cerr << "Profile store test failure: " << error.what() << '\n';
        return 1;
    }
}
