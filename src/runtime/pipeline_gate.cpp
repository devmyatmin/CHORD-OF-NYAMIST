#include "nyamist/runtime/pipeline_gate.hpp"
#include <stdexcept>
#include <string>
namespace nyamist::runtime {
const char* gateStatusName(GateStatus status) noexcept {
 switch(status) { case GateStatus::blocked: return "blocked"; case GateStatus::warning: return "warning"; case GateStatus::ready: return "ready"; }
 return "unknown";
}
PipelineGateDecision PipelineGate::evaluate(const RuntimePlan& runtime, const DeviceSessionSnapshot& session, std::uint64_t now_unix_ms, std::uint64_t maximum_session_age_ms) {
 if(now_unix_ms==0U || maximum_session_age_ms==0U) throw std::invalid_argument("pipeline gate clock and maximum session age must be non-zero");
 PipelineGateDecision decision;
 const auto captured_at=session.capturedAtUnixMs();
 if(captured_at>now_unix_ms) decision.blockers.emplace_back("Device session timestamp is in the future.");
 else if(now_unix_ms-captured_at>maximum_session_age_ms) decision.blockers.emplace_back("Device session is stale and must be refreshed.");
 else { decision.session_fresh=true; decision.evidence.emplace_back("device_session=fresh"); }
 const auto& vulkan=session.probe().vulkan;
 decision.vulkan_enumerated=vulkan.enumeration_status=="success" && vulkan.device_count>0U;
 if(decision.vulkan_enumerated) decision.evidence.emplace_back("vulkan.physical_device_count="+std::to_string(vulkan.device_count));
 else decision.blockers.emplace_back("Vulkan physical-device enumeration is unavailable; no graphics smoke test is allowed.");
 if(!runtime.prerequisites_present) {
  if(runtime.blockers.empty()) decision.blockers.emplace_back("Runtime prerequisites are not present.");
  else decision.blockers.insert(decision.blockers.end(),runtime.blockers.begin(),runtime.blockers.end());
 } else if(!runtime.smoke_test_allowed) decision.blockers.emplace_back("Runtime coordinator did not authorize a controlled smoke test.");
 else decision.evidence.emplace_back("runtime_prerequisites=present");
 decision.warnings.insert(decision.warnings.end(),runtime.warnings.begin(),runtime.warnings.end());
 const auto& telemetry=session.telemetry();
 if(telemetry.fresh_readings.empty()) decision.warnings.emplace_back("No fresh telemetry is available; performance policy must remain conservative.");
 if(!telemetry.stale_readings.empty()) decision.warnings.emplace_back("Stale telemetry is available for diagnostics only and is excluded from policy input.");
 if(telemetry.rejected_readings>0U) decision.warnings.emplace_back("One or more telemetry readings were rejected during aggregation.");
 decision.smoke_test_allowed=decision.blockers.empty() && decision.session_fresh && decision.vulkan_enumerated && runtime.prerequisites_present && runtime.smoke_test_allowed;
 decision.production_ready=decision.smoke_test_allowed && runtime.production_ready;
 if(decision.smoke_test_allowed && !decision.production_ready) decision.warnings.emplace_back("Controlled smoke testing is allowed; sustained production validation is not proven.");
 if(!decision.blockers.empty()) decision.status=GateStatus::blocked;
 else if(!decision.warnings.empty()) decision.status=GateStatus::warning;
 else decision.status=GateStatus::ready;
 return decision;
}
} // namespace nyamist::runtime
