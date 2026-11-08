#include "nyamist/runtime/pipeline_gate.hpp"

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

nyamist::runtime::DeviceSessionSnapshot makeSession(
    std::uint64_t captured_at,
    bool vulkan_available,
    bool with_fresh_telemetry) {
    using namespace nyamist::runtime;
    DeviceProbeSnapshot probe;
    probe.schema_version = 1U;
    probe.evidence_source = "pipeline-gate-test";
    probe.device.model = "Test device";
    probe.device.logical_cpu_count = 8U;
    if (vulkan_available) {
        probe.vulkan.enumeration_status = "success";
        probe.vulkan.device_count = 1U;
        probe.vulkan.device_name = "Test Vulkan GPU";
    } else {
        probe.vulkan.enumeration_status = "unavailable";
    }

    std::vector<TelemetryReading> readings;
    if (with_fresh_telemetry) {
        readings.push_back({
            TelemetryMetric::cpu_utilization_percent, 35, captured_at, "test-monitor"});
    }
    const auto telemetry = TelemetryAggregator::aggregate(
        probe.device, readings, captured_at, 5000U);
    return DeviceSessionSnapshot(std::move(probe), telemetry, captured_at);
}

nyamist::runtime::RuntimePlan runtimePlan(bool prerequisites, bool smoke, bool production) {
    nyamist::runtime::RuntimePlan plan;
    plan.prerequisites_present = prerequisites;
    plan.smoke_test_allowed = smoke;
    plan.production_ready = production;
    if (!prerequisites) {
        plan.blockers.emplace_back("Required translator or Vulkan driver is missing.");
    }
    if (prerequisites && !production) {
        plan.warnings.emplace_back("Sustained component validation is not yet proven.");
    }
    return plan;
}

void testFreshSessionAllowsSmokeTestButNotProduction() {
    using namespace nyamist::runtime;
    const auto session = makeSession(10000U, true, true);
    const auto decision = PipelineGate::evaluate(runtimePlan(true, true, false), session, 11000U, 5000U);
    check(decision.status == GateStatus::warning, "smoke-only validation should produce a warning state");
    check(decision.session_fresh, "current session must pass freshness check");
    check(decision.vulkan_enumerated, "enumerated Vulkan device must be recorded");
    check(decision.smoke_test_allowed, "fresh prerequisites should permit controlled smoke testing");
    check(!decision.production_ready, "smoke-only evidence must not permit production readiness");
}

void testStaleSessionBlocksSmokeTest() {
    using namespace nyamist::runtime;
    const auto session = makeSession(1000U, true, true);
    const auto decision = PipelineGate::evaluate(runtimePlan(true, true, true), session, 10000U, 1000U);
    check(decision.status == GateStatus::blocked, "stale session must block the pipeline");
    check(!decision.session_fresh, "stale session must fail freshness check");
    check(!decision.smoke_test_allowed, "stale session must not permit a smoke test");
}

void testFutureSessionBlocksSmokeTest() {
    using namespace nyamist::runtime;
    const auto session = makeSession(12000U, true, true);
    const auto decision = PipelineGate::evaluate(runtimePlan(true, true, false), session, 11000U, 5000U);
    check(!decision.session_fresh, "future session must not be treated as current");
    check(!decision.smoke_test_allowed, "future session must block a smoke test");
}

void testMissingVulkanEnumerationBlocksSmokeTest() {
    using namespace nyamist::runtime;
    const auto session = makeSession(10000U, false, true);
    const auto decision = PipelineGate::evaluate(runtimePlan(true, true, false), session, 10000U, 5000U);
    check(!decision.vulkan_enumerated, "missing physical device must be explicit");
    check(!decision.smoke_test_allowed, "missing Vulkan device must block the pipeline");
}

void testMissingRuntimePrerequisiteBlocks() {
    using namespace nyamist::runtime;
    const auto session = makeSession(10000U, true, true);
    const auto decision = PipelineGate::evaluate(runtimePlan(false, false, false), session, 10000U, 5000U);
    check(!decision.smoke_test_allowed, "missing translator/driver must block the pipeline");
    check(!decision.blockers.empty(), "missing runtime prerequisite must produce a blocker");
}

void testMissingTelemetryWarnsButDoesNotBlockSmokeTest() {
    using namespace nyamist::runtime;
    const auto session = makeSession(10000U, true, false);
    const auto decision = PipelineGate::evaluate(runtimePlan(true, true, false), session, 10000U, 5000U);
    check(decision.smoke_test_allowed,
          "missing optional telemetry must not block a controlled compatibility test");
    check(decision.status == GateStatus::warning,
          "missing telemetry must be visible as a warning");
}

void testProductionReadyRequiresRuntimeSustainedProof() {
    using namespace nyamist::runtime;
    const auto session = makeSession(10000U, true, true);
    const auto decision = PipelineGate::evaluate(runtimePlan(true, true, true), session, 10000U, 5000U);
    check(decision.smoke_test_allowed, "fresh full prerequisites must allow smoke test");
    check(decision.production_ready, "sustained runtime evidence plus fresh session may pass production gate");
}

void testGateRequiresExplicitTimeBounds() {
    using namespace nyamist::runtime;
    const auto session = makeSession(10000U, true, true);
    bool threw = false;
    try {
        (void)PipelineGate::evaluate(runtimePlan(true, true, false), session, 0U, 5000U);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "gate must not implicitly read the current clock");
}

} // namespace

int main() {
    try {
        testFreshSessionAllowsSmokeTestButNotProduction();
        testStaleSessionBlocksSmokeTest();
        testFutureSessionBlocksSmokeTest();
        testMissingVulkanEnumerationBlocksSmokeTest();
        testMissingRuntimePrerequisiteBlocks();
        testMissingTelemetryWarnsButDoesNotBlockSmokeTest();
        testProductionReadyRequiresRuntimeSustainedProof();
        testGateRequiresExplicitTimeBounds();
        std::cout << "All NYAMIST pipeline gate tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Pipeline gate test failure: " << error.what() << '\n';
        return 1;
    }
}
