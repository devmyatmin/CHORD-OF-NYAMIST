#pragma once

#include "nyamist/runtime/device_probe_adapter.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nyamist::runtime {

enum class TelemetryMetric {
    cpu_utilization_percent,
    gpu_utilization_percent,
    memory_available_bytes,
    gpu_temperature_millicelsius,
    battery_temperature_millicelsius,
    frame_time_p95_microseconds
};

[[nodiscard]] const char* telemetryMetricName(TelemetryMetric metric) noexcept;

struct TelemetryReading {
    TelemetryMetric metric = TelemetryMetric::cpu_utilization_percent;
    std::int64_t value = 0;
    std::uint64_t observed_at_unix_ms = 0;
    std::string source;
};

struct TelemetrySnapshot {
    std::uint32_t schema_version = 1;
    std::uint64_t aggregated_at_unix_ms = 0;
    std::uint64_t freshness_window_ms = 0;
    std::map<TelemetryMetric, TelemetryReading> fresh_readings;
    std::map<TelemetryMetric, TelemetryReading> stale_readings;
    std::uint64_t rejected_readings = 0;
    std::vector<std::string> diagnostics;
};

class TelemetryAggregator {
public:
    [[nodiscard]] static TelemetrySnapshot aggregate(
        const DeviceFactsSnapshot& device,
        const std::vector<TelemetryReading>& readings,
        std::uint64_t now_unix_ms,
        std::uint64_t freshness_window_ms);
};

// Immutable after construction: consumers receive const views and no setters.
// This is a NYAMIST-owned control-plane object, not a driver shared-memory ABI.
class DeviceSessionSnapshot {
public:
    DeviceSessionSnapshot(
        DeviceProbeSnapshot probe,
        TelemetrySnapshot telemetry,
        std::uint64_t captured_at_unix_ms);

    [[nodiscard]] std::uint32_t schemaVersion() const noexcept;
    [[nodiscard]] std::uint64_t capturedAtUnixMs() const noexcept;
    [[nodiscard]] const DeviceProbeSnapshot& probe() const noexcept;
    [[nodiscard]] const TelemetrySnapshot& telemetry() const noexcept;

private:
    const std::uint32_t schema_version_;
    const std::uint64_t captured_at_unix_ms_;
    const DeviceProbeSnapshot probe_;
    const TelemetrySnapshot telemetry_;
};

} // namespace nyamist::runtime
