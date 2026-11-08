#include "nyamist/runtime/session_snapshot.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

nyamist::runtime::DeviceFactsSnapshot deviceFacts() {
    nyamist::runtime::DeviceFactsSnapshot device;
    device.model = "Test device";
    device.logical_cpu_count = 8U;
    device.total_memory_bytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    device.available_memory_bytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    device.memory_stats_reported = true;
    return device;
}

nyamist::runtime::TelemetryReading reading(
    nyamist::runtime::TelemetryMetric metric,
    std::int64_t value,
    std::uint64_t timestamp,
    std::string source = "test-probe") {
    return {metric, value, timestamp, std::move(source)};
}

void testFreshestReadingWins() {
    using namespace nyamist::runtime;
    const auto result = TelemetryAggregator::aggregate(
        deviceFacts(),
        {reading(TelemetryMetric::cpu_utilization_percent, 30, 9500),
         reading(TelemetryMetric::cpu_utilization_percent, 65, 9900)},
        10000, 1000);
    const auto it = result.fresh_readings.find(TelemetryMetric::cpu_utilization_percent);
    check(it != result.fresh_readings.end(), "fresh CPU reading must be retained");
    check(it->second.value == 65, "newest CPU reading must win");
}

void testStaleReadingIsExcludedFromFreshPolicyInput() {
    using namespace nyamist::runtime;
    const auto result = TelemetryAggregator::aggregate(
        deviceFacts(),
        {reading(TelemetryMetric::gpu_temperature_millicelsius, 55000, 1000)},
        10000, 500);
    check(result.fresh_readings.empty(), "stale readings must not appear in fresh telemetry");
    check(result.stale_readings.size() == 1U, "stale readings must remain inspectable");
    check(!result.diagnostics.empty(), "stale exclusion must be diagnosed");
}

void testFutureAndOutOfRangeSamplesAreRejected() {
    using namespace nyamist::runtime;
    const auto result = TelemetryAggregator::aggregate(
        deviceFacts(),
        {reading(TelemetryMetric::cpu_utilization_percent, 101, 9900),
         reading(TelemetryMetric::gpu_utilization_percent, 20, 10001),
         reading(TelemetryMetric::battery_temperature_millicelsius, 2000000, 9900)},
        10000, 1000);
    check(result.rejected_readings == 3U, "invalid/future readings must be rejected");
    check(result.fresh_readings.empty(), "invalid readings must not reach policy consumers");
}

void testAvailableMemoryCannotExceedDeviceCapacity() {
    using namespace nyamist::runtime;
    const auto result = TelemetryAggregator::aggregate(
        deviceFacts(),
        {reading(TelemetryMetric::memory_available_bytes, 9LL * 1024LL * 1024LL * 1024LL, 9900)},
        10000, 1000);
    check(result.rejected_readings == 1U,
          "available-memory telemetry must not exceed reported physical memory");
}

void testEqualTimestampConflictIsOmitted() {
    using namespace nyamist::runtime;
    const auto result = TelemetryAggregator::aggregate(
        deviceFacts(),
        {reading(TelemetryMetric::gpu_utilization_percent, 20, 9900, "source-a"),
         reading(TelemetryMetric::gpu_utilization_percent, 40, 9900, "source-b")},
        10000, 1000);
    check(result.fresh_readings.empty(),
          "conflicting readings at the same newest timestamp must not be selected");
    check(!result.diagnostics.empty(), "conflict must be diagnosed");
}

void testEqualTimestampIdenticalValueHasDeterministicSource() {
    using namespace nyamist::runtime;
    const auto left = TelemetryAggregator::aggregate(
        deviceFacts(),
        {reading(TelemetryMetric::cpu_utilization_percent, 40, 9900, "z-source"),
         reading(TelemetryMetric::cpu_utilization_percent, 40, 9900, "a-source")},
        10000, 1000);
    const auto right = TelemetryAggregator::aggregate(
        deviceFacts(),
        {reading(TelemetryMetric::cpu_utilization_percent, 40, 9900, "a-source"),
         reading(TelemetryMetric::cpu_utilization_percent, 40, 9900, "z-source")},
        10000, 1000);
    check(left.fresh_readings.at(TelemetryMetric::cpu_utilization_percent).source == "a-source" &&
          right.fresh_readings.at(TelemetryMetric::cpu_utilization_percent).source == "a-source",
          "same-value ties must select a deterministic source independent of input order");
}

void testSessionSnapshotIsVersionedAndReadOnly() {
    using namespace nyamist::runtime;
    DeviceProbeSnapshot probe;
    probe.schema_version = 1U;
    probe.evidence_source = "unit-test";
    probe.device = deviceFacts();
    probe.vulkan.enumeration_status = "unavailable";
    const auto telemetry = TelemetryAggregator::aggregate(
        probe.device,
        {reading(TelemetryMetric::cpu_utilization_percent, 42, 9900)},
        10000, 1000);
    const DeviceSessionSnapshot session(probe, telemetry, 10000);

    check(session.schemaVersion() == 1U, "session schema version must be explicit");
    check(session.capturedAtUnixMs() == 10000U, "session capture timestamp must be stable");
    check(session.probe().device.model == "Test device", "device facts must be included");
    check(session.telemetry().fresh_readings.size() == 1U,
          "fresh telemetry must be included in the session");
}

void testSessionRejectsMismatchedTelemetryTimestamp() {
    using namespace nyamist::runtime;
    DeviceProbeSnapshot probe;
    probe.schema_version = 1U;
    probe.evidence_source = "unit-test";
    probe.device = deviceFacts();
    auto telemetry = TelemetryAggregator::aggregate(probe.device, {}, 9000, 1000);
    bool threw = false;
    try {
        (void)DeviceSessionSnapshot(probe, telemetry, 10000);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "snapshot must reject mismatched capture timestamps");
}

void testAggregationRequiresExplicitClockAndFreshness() {
    using namespace nyamist::runtime;
    bool threw = false;
    try {
        (void)TelemetryAggregator::aggregate(deviceFacts(), {}, 0, 1000);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "zero aggregation time must be rejected");
}

} // namespace

int main() {
    try {
        testFreshestReadingWins();
        testStaleReadingIsExcludedFromFreshPolicyInput();
        testFutureAndOutOfRangeSamplesAreRejected();
        testAvailableMemoryCannotExceedDeviceCapacity();
        testEqualTimestampConflictIsOmitted();
        testEqualTimestampIdenticalValueHasDeterministicSource();
        testSessionSnapshotIsVersionedAndReadOnly();
        testSessionRejectsMismatchedTelemetryTimestamp();
        testAggregationRequiresExplicitClockAndFreshness();
        std::cout << "All NYAMIST session snapshot tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Session snapshot test failure: " << error.what() << '\n';
        return 1;
    }
}
