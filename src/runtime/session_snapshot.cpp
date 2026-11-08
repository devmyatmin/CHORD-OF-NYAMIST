#include "nyamist/runtime/session_snapshot.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace nyamist::runtime {
namespace {

bool validReading(const DeviceFactsSnapshot& device, const TelemetryReading& reading) {
    if (reading.observed_at_unix_ms == 0U || reading.source.empty()) {
        return false;
    }

    switch (reading.metric) {
    case TelemetryMetric::cpu_utilization_percent:
    case TelemetryMetric::gpu_utilization_percent:
        return reading.value >= 0 && reading.value <= 100;
    case TelemetryMetric::memory_available_bytes:
        if (reading.value < 0) {
            return false;
        }
        return !device.memory_stats_reported ||
               static_cast<std::uint64_t>(reading.value) <= device.total_memory_bytes;
    case TelemetryMetric::gpu_temperature_millicelsius:
    case TelemetryMetric::battery_temperature_millicelsius:
        return reading.value >= -40000 && reading.value <= 200000;
    case TelemetryMetric::frame_time_p95_microseconds:
        return reading.value > 0 && reading.value <= 120000000;
    }
    return false;
}

} // namespace

const char* telemetryMetricName(TelemetryMetric metric) noexcept {
    switch (metric) {
    case TelemetryMetric::cpu_utilization_percent: return "cpu_utilization_percent";
    case TelemetryMetric::gpu_utilization_percent: return "gpu_utilization_percent";
    case TelemetryMetric::memory_available_bytes: return "memory_available_bytes";
    case TelemetryMetric::gpu_temperature_millicelsius: return "gpu_temperature_millicelsius";
    case TelemetryMetric::battery_temperature_millicelsius: return "battery_temperature_millicelsius";
    case TelemetryMetric::frame_time_p95_microseconds: return "frame_time_p95_microseconds";
    }
    return "unknown";
}

TelemetrySnapshot TelemetryAggregator::aggregate(
    const DeviceFactsSnapshot& device,
    const std::vector<TelemetryReading>& readings,
    std::uint64_t now_unix_ms,
    std::uint64_t freshness_window_ms) {
    if (now_unix_ms == 0U || freshness_window_ms == 0U) {
        throw std::invalid_argument("telemetry aggregation time and freshness window must be non-zero");
    }

    TelemetrySnapshot snapshot;
    snapshot.aggregated_at_unix_ms = now_unix_ms;
    snapshot.freshness_window_ms = freshness_window_ms;

    std::map<TelemetryMetric, std::vector<TelemetryReading>> grouped;
    for (const auto& reading : readings) {
        if (!validReading(device, reading) || reading.observed_at_unix_ms > now_unix_ms) {
            ++snapshot.rejected_readings;
            snapshot.diagnostics.emplace_back(
                std::string("Rejected invalid/future telemetry reading: ") +
                telemetryMetricName(reading.metric));
            continue;
        }
        grouped[reading.metric].push_back(reading);
    }

    for (const auto& [metric, candidates] : grouped) {
        const auto newest = std::max_element(
            candidates.begin(), candidates.end(),
            [](const TelemetryReading& left, const TelemetryReading& right) {
                return left.observed_at_unix_ms < right.observed_at_unix_ms;
            });
        const auto newest_timestamp = newest->observed_at_unix_ms;
        const auto first_latest = std::find_if(
            candidates.begin(), candidates.end(),
            [newest_timestamp](const TelemetryReading& reading) {
                return reading.observed_at_unix_ms == newest_timestamp;
            });

        const bool conflicting_latest = std::any_of(
            std::next(first_latest), candidates.end(),
            [newest_timestamp, first_latest](const TelemetryReading& reading) {
                return reading.observed_at_unix_ms == newest_timestamp &&
                       reading.value != first_latest->value;
            });
        if (conflicting_latest) {
            snapshot.diagnostics.emplace_back(
                std::string("Conflicting equally-new telemetry readings omitted: ") +
                telemetryMetricName(metric));
            continue;
        }

        auto selected = *first_latest;
        for (auto it = std::next(first_latest); it != candidates.end(); ++it) {
            if (it->observed_at_unix_ms == newest_timestamp &&
                it->value == selected.value && it->source < selected.source) {
                selected = *it;
            }
        }

        const auto age_ms = now_unix_ms - selected.observed_at_unix_ms;
        if (age_ms <= freshness_window_ms) {
            snapshot.fresh_readings.emplace(metric, std::move(selected));
        } else {
            snapshot.stale_readings.emplace(metric, std::move(selected));
            snapshot.diagnostics.emplace_back(
                std::string("Stale telemetry excluded from current policy input: ") +
                telemetryMetricName(metric));
        }
    }

    if (snapshot.fresh_readings.empty()) {
        snapshot.diagnostics.emplace_back("No fresh telemetry readings are available.");
    }
    return snapshot;
}

DeviceSessionSnapshot::DeviceSessionSnapshot(
    DeviceProbeSnapshot probe,
    TelemetrySnapshot telemetry,
    std::uint64_t captured_at_unix_ms)
    : schema_version_(1U),
      captured_at_unix_ms_(captured_at_unix_ms),
      probe_(std::move(probe)),
      telemetry_(std::move(telemetry)) {
    if (captured_at_unix_ms_ == 0U) {
        throw std::invalid_argument("device session capture timestamp must be non-zero");
    }
    if (probe_.schema_version != 1U || probe_.evidence_source.empty() ||
        probe_.device.model.empty()) {
        throw std::invalid_argument("device session requires a valid schema-v1 device probe");
    }
    if (telemetry_.schema_version != 1U ||
        telemetry_.aggregated_at_unix_ms != captured_at_unix_ms_) {
        throw std::invalid_argument("device session telemetry timestamp/schema mismatch");
    }
}

std::uint32_t DeviceSessionSnapshot::schemaVersion() const noexcept {
    return schema_version_;
}

std::uint64_t DeviceSessionSnapshot::capturedAtUnixMs() const noexcept {
    return captured_at_unix_ms_;
}

const DeviceProbeSnapshot& DeviceSessionSnapshot::probe() const noexcept {
    return probe_;
}

const TelemetrySnapshot& DeviceSessionSnapshot::telemetry() const noexcept {
    return telemetry_;
}

} // namespace nyamist
