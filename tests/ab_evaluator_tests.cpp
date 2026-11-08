#include "nyamist/runtime/ab_evaluator.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void check(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

nyamist::runtime::ABRunRecord makeRun(
    const std::string& compatibility_key,
    std::uint64_t started_at,
    std::int64_t p95,
    bool include_p95=true) {
    using namespace nyamist::runtime;
    constexpr std::uint64_t duration=300U;
    const auto captured_at=started_at+duration*1000U;
    DeviceProbeSnapshot probe;
    probe.schema_version=1U;
    probe.evidence_source="ab-evaluator-test";
    probe.device.manufacturer="Xiaomi";
    probe.device.brand="REDMI";
    probe.device.model="REDMI K80 Ultra";
    probe.device.soc_model="Dimensity 9400+";
    probe.device.android_release="16";
    probe.device.android_api=36U;
    probe.device.primary_abi="arm64-v8a";
    probe.device.logical_cpu_count=8U;
    probe.vulkan.enumeration_status="success";
    probe.vulkan.device_count=1U;
    probe.vulkan.vendor_id=0x13b5U;
    probe.vulkan.device_id=0xd8300015U;
    probe.vulkan.device_name="Immortalis-G925";
    probe.vulkan.driver_name="panvk";
    probe.vulkan.driver_version="rc5";
    probe.vulkan.device_api_version="1.3";
    std::vector<TelemetryReading> readings;
    if (include_p95) {
        readings.push_back({TelemetryMetric::frame_time_p95_microseconds,
                            p95,captured_at-100U,"frame-time-probe"});
    }
    auto telemetry=TelemetryAggregator::aggregate(
        probe.device,readings,captured_at,3000U);
    DeviceSessionSnapshot session(std::move(probe),std::move(telemetry),captured_at);
    return ABRunRecord{
        "synthetic-cube-720p",
        "redmi-k80-ultra-panvk-rc5",
        compatibility_key,
        started_at,
        duration,
        true,
        18000U,
        0,
        18000U,
        0U,
        true,
        0U,
        true,
        std::move(session)
    };
}

std::vector<nyamist::runtime::ABRunRecord> makeArm(
    const std::string& key,
    std::uint64_t first_start,
    const std::vector<std::int64_t>& p95_values,
    bool include_p95=true) {
    std::vector<nyamist::runtime::ABRunRecord> runs;
    for (std::size_t i=0U;i<p95_values.size();++i) {
        runs.push_back(makeRun(key,first_start+static_cast<std::uint64_t>(i)*400000U,
                               p95_values[i],include_p95));
    }
    return runs;
}

void testRepeatedRunsReportImprovement() {
    using namespace nyamist::runtime;
    const auto baseline=makeArm("1111111111111111",1000000U,{20000,22000,21000});
    const auto candidate=makeArm("2222222222222222",2200000U,{18000,19000,20000});
    const auto result=ABRegressionEvaluator::evaluate(baseline,candidate);
    check(result.outcome==ABOutcome::improved,"lower candidate p95 should be reported as improved");
    check(result.baseline_median_p95_microseconds==21000U,"baseline median must be deterministic");
    check(result.candidate_median_p95_microseconds==19000U,"candidate median must be deterministic");
    check(result.delta_basis_points.has_value() && *result.delta_basis_points < -500,
          "improvement must exceed the 5% threshold");
    check(!result.automatic_application_allowed,
          "A/B evidence must never automatically apply a runtime setting");
}

void testWithinThresholdReportsNoRegression() {
    using namespace nyamist::runtime;
    const auto baseline=makeArm("1111111111111111",1000000U,{20000,21000,22000});
    const auto candidate=makeArm("2222222222222222",2200000U,{20500,21000,21500});
    const auto result=ABRegressionEvaluator::evaluate(baseline,candidate);
    check(result.outcome==ABOutcome::no_regression,
          "changes inside the configured threshold must not be labeled a regression");
}

void testLargeRegressionIsReported() {
    using namespace nyamist::runtime;
    const auto baseline=makeArm("1111111111111111",1000000U,{20000,21000,22000});
    const auto candidate=makeArm("2222222222222222",2200000U,{23000,24000,25000});
    const auto result=ABRegressionEvaluator::evaluate(baseline,candidate);
    check(result.outcome==ABOutcome::regressed,
          "candidate p95 degradation above 5% must be reported as a regression");
}

void testTooFewRunsAreInsufficientEvidence() {
    using namespace nyamist::runtime;
    const auto baseline=makeArm("1111111111111111",1000000U,{20000,21000});
    const auto candidate=makeArm("2222222222222222",1800000U,{19000,20000,21000});
    const auto result=ABRegressionEvaluator::evaluate(baseline,candidate);
    check(result.outcome==ABOutcome::insufficient_evidence,
          "fewer than three repetitions must not produce a performance conclusion");
}

void testMissingP95IsInsufficientEvidence() {
    using namespace nyamist::runtime;
    const auto baseline=makeArm("1111111111111111",1000000U,{20000,21000,22000});
    const auto candidate=makeArm("2222222222222222",2200000U,{19000,20000,21000},false);
    const auto result=ABRegressionEvaluator::evaluate(baseline,candidate);
    check(result.outcome==ABOutcome::insufficient_evidence,
          "missing fresh p95 telemetry must prevent a comparison result");
}

void testFailedPresentEvidenceBlocksComparison() {
    using namespace nyamist::runtime;
    auto baseline=makeArm("1111111111111111",1000000U,{20000,21000,22000});
    auto candidate=makeArm("2222222222222222",2200000U,{19000,20000,21000});
    candidate.front().present_failures_reported=false;
    const auto result=ABRegressionEvaluator::evaluate(baseline,candidate);
    check(result.outcome==ABOutcome::blocked && !result.blockers.empty(),
          "missing explicit zero Present-failure evidence must block comparison");
}

void testWorkloadMismatchBlocksComparison() {
    using namespace nyamist::runtime;
    auto baseline=makeArm("1111111111111111",1000000U,{20000,21000,22000});
    auto candidate=makeArm("2222222222222222",2200000U,{19000,20000,21000});
    candidate.front().workload_id="different-scene";
    const auto result=ABRegressionEvaluator::evaluate(baseline,candidate);
    check(result.outcome==ABOutcome::blocked,
          "baseline and candidate must use the same workload identity");
}

void testSameCompatibilityProfileCannotBeCompared() {
    using namespace nyamist::runtime;
    const auto baseline=makeArm("1111111111111111",1000000U,{20000,21000,22000});
    const auto candidate=makeArm("1111111111111111",2200000U,{19000,20000,21000});
    const auto result=ABRegressionEvaluator::evaluate(baseline,candidate);
    check(result.outcome==ABOutcome::blocked,
          "baseline and candidate must identify distinct compatibility profiles");
}

void testShortDurationPolicyIsRejected() {
    using namespace nyamist::runtime;
    ABEvaluationPolicy policy;
    policy.minimum_run_duration_seconds=60U;
    bool rejected=false;
    try {
        (void)ABRegressionEvaluator::evaluate({}, {}, policy);
    } catch (const std::invalid_argument&) {
        rejected=true;
    }
    check(rejected,"A/B policy must preserve the 300-second sustained-test minimum");
}

} // namespace

int main() {
    try {
        testRepeatedRunsReportImprovement();
        testWithinThresholdReportsNoRegression();
        testLargeRegressionIsReported();
        testTooFewRunsAreInsufficientEvidence();
        testMissingP95IsInsufficientEvidence();
        testFailedPresentEvidenceBlocksComparison();
        testWorkloadMismatchBlocksComparison();
        testSameCompatibilityProfileCannotBeCompared();
        testShortDurationPolicyIsRejected();
        std::cout << "All NYAMIST A/B evaluator tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "A/B evaluator test failure: " << error.what() << '\n';
        return 1;
    }
}
