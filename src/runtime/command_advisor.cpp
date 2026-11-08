#include "nyamist/runtime/command_advisor.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
namespace nyamist::runtime {
CommandProposal RuntimeCommandAdvisor::propose(const DeviceSessionSnapshot& session, const PipelineGateDecision& gate, std::uint32_t current_worker_budget) {
 if(current_worker_budget<1U || current_worker_budget>4U) throw std::invalid_argument("current shader worker budget must be within [1, 4]");
 CommandProposal proposal;
 proposal.current_value=current_worker_budget;
 proposal.proposed_value=current_worker_budget;
 proposal.rollback_value=current_worker_budget;
 if(gate.status==GateStatus::blocked || !gate.smoke_test_allowed) {
  proposal.action="blocked";
  proposal.rationale="Pipeline Gate has not authorized a controlled smoke test.";
  proposal.risk="No policy proposal is emitted while runtime prerequisites or session evidence are blocked.";
  return proposal;
 }
 const auto cpu=session.telemetry().fresh_readings.find(TelemetryMetric::cpu_utilization_percent);
 if(cpu==session.telemetry().fresh_readings.end()) {
  proposal.action="insufficient_evidence";
  proposal.rationale="Fresh CPU utilization telemetry is required for a worker-budget proposal.";
  proposal.risk="Do not infer CPU headroom from device model, core count, or stale readings.";
  return proposal;
 }
 const auto logical_cpus=session.probe().device.logical_cpu_count;
 if(logical_cpus==0U) {
  proposal.action="insufficient_evidence";
  proposal.rationale="Logical CPU count is unavailable; the worker ceiling cannot be derived.";
  proposal.risk="No worker-count adjustment is proposed without a verified CPU count.";
  return proposal;
 }
 const auto cpu_ceiling=std::clamp(logical_cpus/2U,1U,4U);
 proposal.minimum_value=1U;
 proposal.maximum_value=cpu_ceiling;
 proposal.evidence_sufficient=true;
 proposal.evidence.emplace_back("cpu.utilization_percent="+std::to_string(cpu->second.value));
 proposal.evidence.emplace_back("device.logical_cpu_count="+std::to_string(logical_cpus));
 proposal.evidence.emplace_back("telemetry.source="+cpu->second.source);
 proposal.evidence.emplace_back("telemetry.observed_at_unix_ms="+std::to_string(cpu->second.observed_at_unix_ms));
 if(current_worker_budget>cpu_ceiling) {
  proposal.proposed_value=cpu_ceiling;
  proposal.action="reduce";
  proposal.rationale="Current worker budget exceeds the conservative CPU-derived ceiling.";
  proposal.expected_benefit="Reduce compile-thread contention on a device with limited logical CPUs.";
  proposal.risk="Lower concurrency may increase shader preparation latency; compare repeatable sessions.";
 } else if(cpu->second.value>=85 && current_worker_budget>1U) {
  proposal.proposed_value=current_worker_budget-1U;
  proposal.action="reduce";
  proposal.rationale="Fresh CPU utilization is at or above the 85% high-load guard.";
  proposal.expected_benefit="Reduce additional shader-worker contention while CPU load is high.";
  proposal.risk="Lower concurrency may increase shader preparation latency; compare repeatable sessions.";
 } else if(cpu->second.value<=55 && current_worker_budget<cpu_ceiling) {
  proposal.proposed_value=current_worker_budget+1U;
  proposal.action="increase";
  proposal.rationale="Fresh CPU utilization is at or below the 55% low-load guard.";
  proposal.expected_benefit="Test one additional shader worker without jumping directly to the maximum.";
  proposal.risk="Higher concurrency can increase CPU contention, power draw, and thermal load.";
 } else {
  proposal.action="hold";
  proposal.rationale="CPU telemetry is inside the conservative hold band or the worker ceiling is reached.";
  proposal.expected_benefit="Preserve the current worker budget and avoid an unsupported adjustment.";
  proposal.risk="Re-evaluate after collecting fresh telemetry from a repeatable workload.";
 }
 proposal.proposed_value=std::clamp(proposal.proposed_value,proposal.minimum_value,proposal.maximum_value);
 proposal.rollback_value=current_worker_budget;
 proposal.automatic_application_allowed=false;
 return proposal;
}
} // namespace nyamist::runtime
