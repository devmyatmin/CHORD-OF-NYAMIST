#include "nyamist/runtime/command_advisor.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

nyamist::runtime::DeviceSessionSnapshot makeSession(std::int64_t cpu_load, bool include_cpu = true) {
    using namespace nyamist::runtime;
    DeviceProbeSnapshot probe;
    probe.schema_version = 1U;
    probe.evidence_source = "command-advisor-test";
    probe.device.model = "Test device";
    probe.device.logical_cpu_count = 8U;
    probe.vulkan.enumeration_status = "success";
    probe.vulkan.device_count = 1U;
    std::vector<TelemetryReading> readings;
    if (include_cpu) {
        readings.push_back({TelemetryMetric::cpu_utilization_percent, cpu_load, 10000U, "test-cpu-monitor"});
    }
    auto telemetry = TelemetryAggregator::aggregate(probe.device, readings, 10000U, 3000U);
    return DeviceSessionSnapshot(std::move(probe), std::move(telemetry), 10000U);
}

nyamist::runtime::PipelineGateDecision allowedGate() {
    nyamist::runtime::PipelineGateDecision gate;
    gate.status = nyamist::runtime::GateStatus::warning;
    gate.session_fresh = true;
    gate.vulkan_enumerated = true;
    gate.smoke_test_allowed = true;
    return gate;
}

void testHighCpuLoadSuggestsOneStepReduction() {
    using namespace nyamist::runtime;
    const auto session = makeSession(92);
    const auto proposal = RuntimeCommandAdvisor::propose(session, allowedGate(), 3U);
    check(proposal.action == "reduce", "high CPU load should suggest a reduction");
    check(proposal.proposed_value == 2U, "reduction must be one worker step");
    check(proposal.rollback_value == 3U, "rollback value must preserve the current setting");
}

void testLowCpuLoadSuggestsOneStepIncrease() {
    using namespace nyamist::runtime;
    const auto session = makeSession(40);
    const auto proposal = RuntimeCommandAdvisor::propose(session, allowedGate(), 2U);
    check(proposal.action == "increase", "low CPU load should suggest an increase");
    check(proposal.proposed_value == 3U, "increase must be one worker step");
    check(proposal.maximum_value == 4U, "eight logical CPUs imply a four-worker conservative ceiling");
}

void testHighLoadAtMinimumHolds() {
    using namespace nyamist::runtime;
    const auto session = makeSession(95);
    const auto proposal = RuntimeCommandAdvisor::propose(session, allowedGate(), 1U);
    check(proposal.action == "hold" && proposal.proposed_value == 1U,
          "worker budget must never be reduced below one");
}

void testWorkerBudgetIsCappedByCpuCount() {
    using namespace nyamist::runtime;
    DeviceProbeSnapshot probe;
    probe.schema_version = 1U;
    probe.evidence_source = "two-core-test";
    probe.device.model = "Two core test device";
    probe.device.logical_cpu_count = 2U;
    probe.vulkan.enumeration_status = "success";
    probe.vulkan.device_count = 1U;
    auto telemetry = TelemetryAggregator::aggregate(
        probe.device, {{TelemetryMetric::cpu_utilization_percent, 35, 10000U, "test"}}, 10000U, 3000U);
    const DeviceSessionSnapshot session(std::move(probe), std::move(telemetry), 10000U);
    const auto proposal = RuntimeCommandAdvisor::propose(session, allowedGate(), 3U);
    check(proposal.proposed_value == 1U && proposal.maximum_value == 1U,
          "worker budget must not exceed the CPU-derived ceiling");
}

void testMissingCpuTelemetryProducesNoRecommendation() {
    using namespace nyamist::runtime;
    const auto session = makeSession(40, false);
    const auto proposal = RuntimeCommandAdvisor::propose(session, allowedGate(), 2U);
    check(proposal.action == "insufficient_evidence",
          "missing CPU telemetry must not be replaced by a guessed value");
    check(!proposal.evidence_sufficient, "proposal must mark missing evidence");
    check(proposal.proposed_value == proposal.current_value,
          "insufficient evidence must preserve current setting");
}

void testBlockedGatePreventsProposal() {
    using namespace nyamist::runtime;
    const auto session = makeSession(30);
    PipelineGateDecision gate;
    gate.status = GateStatus::blocked;
    const auto proposal = RuntimeCommandAdvisor::propose(session, gate, 2U);
    check(proposal.action == "blocked", "blocked pipeline must prevent a policy proposal");
    check(proposal.proposed_value == 2U, "blocked proposal must not change the current budget");
}

void testAutomaticApplicationIsAlwaysDisabled() {
    using namespace nyamist::runtime;
    const auto session = makeSession(30);
    const auto proposal = RuntimeCommandAdvisor::propose(session, allowedGate(), 2U);
    check(!proposal.automatic_application_allowed,
          "advisor must never apply settings automatically");
    check(proposal.evidence_sufficient, "fresh CPU input must be cited as proposal evidence");
}

void testInvalidCurrentBudgetRejected() {
    using namespace nyamist::runtime;
    const auto session = makeSession(30);
    bool threw = false;
    try {
        (void)RuntimeCommandAdvisor::propose(session, allowedGate(), 0U);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "worker budget outside [1,4] must be rejected");
}
} // namespace

int main() {
    try {
        testHighCpuLoadSuggestsOneStepReduction();
        testLowCpuLoadSuggestsOneStepIncrease();
        testHighLoadAtMinimumHolds();
        testWorkerBudgetIsCappedByCpuCount();
        testMissingCpuTelemetryProducesNoRecommendation();
        testBlockedGatePreventsProposal();
        testAutomaticApplicationIsAlwaysDisabled();
        testInvalidCurrentBudgetRejected();
        std::cout << "All NYAMIST command advisor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Command advisor test failure: " << error.what() << '\n';
        return 1;
    }
}
