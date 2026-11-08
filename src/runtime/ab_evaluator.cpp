#include "nyamist/runtime/ab_evaluator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace nyamist::runtime {
namespace {

bool validProfileKey(const std::string& key) {
    if (key.size()!=16U) return false;
    return std::all_of(key.begin(),key.end(),[](char c){
        return (c>='0'&&c<='9')||(c>='a'&&c<='f');
    });
}

bool sameDevice(const DeviceSessionSnapshot& left,
                const DeviceSessionSnapshot& right) {
    const auto& a=left.probe();
    const auto& b=right.probe();
    return a.device.manufacturer==b.device.manufacturer &&
           a.device.brand==b.device.brand &&
           a.device.model==b.device.model &&
           a.device.soc_model==b.device.soc_model &&
           a.device.android_api==b.device.android_api &&
           a.device.primary_abi==b.device.primary_abi &&
           a.device.logical_cpu_count==b.device.logical_cpu_count &&
           a.vulkan.vendor_id==b.vulkan.vendor_id &&
           a.vulkan.device_id==b.vulkan.device_id &&
           a.vulkan.device_name==b.vulkan.device_name &&
           a.vulkan.driver_name==b.vulkan.driver_name &&
           a.vulkan.driver_version==b.vulkan.driver_version &&
           a.vulkan.device_api_version==b.vulkan.device_api_version;
}

std::optional<std::uint64_t> p95FrameTime(const ABRunRecord& run) {
    const auto found=run.session.telemetry().fresh_readings.find(
        TelemetryMetric::frame_time_p95_microseconds);
    if (found==run.session.telemetry().fresh_readings.end() ||
        found->second.value<=0) return std::nullopt;
    return static_cast<std::uint64_t>(found->second.value);
}

std::uint64_t median(std::vector<std::uint64_t> values) {
    std::sort(values.begin(),values.end());
    const auto middle=values.size()/2U;
    if (values.size()%2U!=0U) return values[middle];
    return (values[middle-1U]+values[middle])/2U;
}

void validatePolicy(const ABEvaluationPolicy& policy) {
    if (policy.minimum_repetitions<3U || policy.minimum_repetitions>100U ||
        policy.minimum_run_duration_seconds<300U ||
        policy.minimum_run_duration_seconds>86400U ||
        policy.regression_threshold_basis_points==0U ||
        policy.regression_threshold_basis_points>5000U) {
        throw std::invalid_argument("A/B policy must require 3-100 repeats, 300-86400 seconds, and a 1-50% threshold");
    }
}

void addRunBlockers(const ABRunRecord& run,
                    const ABEvaluationPolicy& policy,
                    const std::string& group,
                    std::vector<std::string>& blockers) {
    const auto prefix=group+" run";
    if (run.workload_id.empty()) blockers.push_back(prefix+": workload identity is missing.");
    if (run.device_profile_id.empty()) blockers.push_back(prefix+": device profile identity is missing.");
    if (!validProfileKey(run.compatibility_profile_key))
        blockers.push_back(prefix+": compatibility profile key is invalid.");
    if (run.started_at_unix_ms==0U ||
        run.session.capturedAtUnixMs()<run.started_at_unix_ms) {
        blockers.push_back(prefix+": session timestamp does not follow a valid run start.");
    } else {
        const auto elapsed=run.session.capturedAtUnixMs()-run.started_at_unix_ms;
        if (elapsed/1000U<policy.minimum_run_duration_seconds)
            blockers.push_back(prefix+": captured session does not cover the required run duration.");
    }
    if (run.duration_seconds<policy.minimum_run_duration_seconds)
        blockers.push_back(prefix+": sustained duration is below the policy minimum.");
    if (!run.render_present_passed)
        blockers.push_back(prefix+": explicit render/Present success is missing.");
    if (run.frame_count==0U || run.present_calls==0U)
        blockers.push_back(prefix+": frame or Present-call counters are missing.");
    if (run.process_exit_code!=0)
        blockers.push_back(prefix+": process did not exit normally.");
    if (!run.present_failures_reported || run.present_failures!=0U)
        blockers.push_back(prefix+": Present failures were not explicitly reported as zero.");
    if (!run.device_lost_status_reported || run.device_lost_events!=0U)
        blockers.push_back(prefix+": device-loss events were not explicitly reported as zero.");
    const auto& vulkan=run.session.probe().vulkan;
    if (vulkan.enumeration_status!="success" || vulkan.device_count==0U)
        blockers.push_back(prefix+": Vulkan physical-device enumeration is not successful.");
}

bool uniformWorkloadAndDevice(
    const std::vector<ABRunRecord>& runs,
    const std::string& expected_workload,
    const std::string& expected_device,
    const std::string& expected_compatibility,
    const DeviceSessionSnapshot& reference,
    const std::string& group,
    std::vector<std::string>& blockers) {
    bool valid=true;
    for (const auto& run:runs) {
        if (run.workload_id!=expected_workload) {
            blockers.push_back(group+": workload differs between repetitions.");
            valid=false;
        }
        if (run.device_profile_id!=expected_device) {
            blockers.push_back(group+": device profile differs between repetitions.");
            valid=false;
        }
        if (run.compatibility_profile_key!=expected_compatibility) {
            blockers.push_back(group+": compatibility profile changes within the same comparison arm.");
            valid=false;
        }
        if (!sameDevice(reference,run.session)) {
            blockers.push_back(group+": hardware or Vulkan driver identity differs between repetitions.");
            valid=false;
        }
    }
    return valid;
}

} // namespace

const char* abOutcomeName(ABOutcome outcome) noexcept {
    switch(outcome) {
    case ABOutcome::insufficient_evidence: return "insufficient_evidence";
    case ABOutcome::blocked: return "blocked";
    case ABOutcome::no_regression: return "no_regression";
    case ABOutcome::improved: return "improved";
    case ABOutcome::regressed: return "regressed";
    }
    return "unknown";
}

ABEvaluationDecision ABRegressionEvaluator::evaluate(
    const std::vector<ABRunRecord>& baseline,
    const std::vector<ABRunRecord>& candidate,
    const ABEvaluationPolicy& policy) {
    validatePolicy(policy);
    ABEvaluationDecision decision;
    decision.baseline_repetitions=static_cast<std::uint32_t>(baseline.size());
    decision.candidate_repetitions=static_cast<std::uint32_t>(candidate.size());

    if (baseline.size()<policy.minimum_repetitions ||
        candidate.size()<policy.minimum_repetitions) {
        decision.outcome=ABOutcome::insufficient_evidence;
        decision.warnings.emplace_back(
            "At least the configured number of independent baseline and candidate runs is required.");
        return decision;
    }

    const auto& reference=baseline.front();
    if (reference.workload_id.empty() || reference.device_profile_id.empty() ||
        !validProfileKey(reference.compatibility_profile_key)) {
        decision.blockers.emplace_back("Baseline comparison identity is incomplete.");
    }
    for (const auto& run:baseline)
        addRunBlockers(run,policy,"baseline",decision.blockers);
    for (const auto& run:candidate)
        addRunBlockers(run,policy,"candidate",decision.blockers);

    const auto workload=reference.workload_id;
    const auto device=reference.device_profile_id;
    const auto baseline_compatibility=reference.compatibility_profile_key;
    uniformWorkloadAndDevice(baseline,workload,device,baseline_compatibility,
                             reference.session,"baseline",decision.blockers);

    const auto& candidate_reference=candidate.front();
    if (candidate_reference.workload_id!=workload)
        decision.blockers.emplace_back("Baseline and candidate workloads do not match.");
    if (candidate_reference.device_profile_id!=device)
        decision.blockers.emplace_back("Baseline and candidate device profiles do not match.");
    if (candidate_reference.compatibility_profile_key==baseline_compatibility)
        decision.blockers.emplace_back("Baseline and candidate compatibility profile keys must differ.");
    uniformWorkloadAndDevice(candidate,workload,device,
                             candidate_reference.compatibility_profile_key,
                             reference.session,"candidate",decision.blockers);

    std::set<std::uint64_t> start_times;
    for (const auto& run:baseline) {
        if (!start_times.insert(run.started_at_unix_ms).second)
            decision.blockers.emplace_back("A/B run start timestamps must be unique.");
    }
    for (const auto& run:candidate) {
        if (!start_times.insert(run.started_at_unix_ms).second)
            decision.blockers.emplace_back("A/B run start timestamps must be unique across both arms.");
    }

    if (!decision.blockers.empty()) {
        decision.outcome=ABOutcome::blocked;
        return decision;
    }

    std::vector<std::uint64_t> baseline_p95;
    std::vector<std::uint64_t> candidate_p95;
    baseline_p95.reserve(baseline.size());
    candidate_p95.reserve(candidate.size());
    for (const auto& run:baseline) {
        const auto value=p95FrameTime(run);
        if (!value) {
            decision.warnings.emplace_back("A baseline run is missing fresh p95 frame-time telemetry.");
            decision.outcome=ABOutcome::insufficient_evidence;
            return decision;
        }
        baseline_p95.push_back(*value);
    }
    for (const auto& run:candidate) {
        const auto value=p95FrameTime(run);
        if (!value) {
            decision.warnings.emplace_back("A candidate run is missing fresh p95 frame-time telemetry.");
            decision.outcome=ABOutcome::insufficient_evidence;
            return decision;
        }
        candidate_p95.push_back(*value);
    }

    decision.baseline_median_p95_microseconds=median(std::move(baseline_p95));
    decision.candidate_median_p95_microseconds=median(std::move(candidate_p95));
    if (decision.baseline_median_p95_microseconds==0U) {
        decision.outcome=ABOutcome::insufficient_evidence;
        decision.warnings.emplace_back("Baseline median p95 frame time must be non-zero.");
        return decision;
    }

    const auto baseline_value=static_cast<std::int64_t>(
        decision.baseline_median_p95_microseconds);
    const auto candidate_value=static_cast<std::int64_t>(
        decision.candidate_median_p95_microseconds);
    const auto delta=(candidate_value-baseline_value)*10000LL/baseline_value;
    decision.delta_basis_points=delta;
    decision.evidence.emplace_back(
        "baseline.median_p95_us="+
        std::to_string(decision.baseline_median_p95_microseconds));
    decision.evidence.emplace_back(
        "candidate.median_p95_us="+
        std::to_string(decision.candidate_median_p95_microseconds));
    decision.evidence.emplace_back("delta_basis_points="+std::to_string(delta));
    decision.evidence.emplace_back(
        "minimum_run_seconds="+std::to_string(policy.minimum_run_duration_seconds));
    decision.evidence.emplace_back(
        "regression_threshold_basis_points="+
        std::to_string(policy.regression_threshold_basis_points));
    decision.evidence.emplace_back("metric=p95_frame_time_lower_is_better");
    decision.warnings.emplace_back(
        "A/B result is advisory for the supplied workload and device only; no settings are applied.");

    const auto threshold=static_cast<std::int64_t>(
        policy.regression_threshold_basis_points);
    if (delta>threshold) decision.outcome=ABOutcome::regressed;
    else if (delta< -threshold) decision.outcome=ABOutcome::improved;
    else decision.outcome=ABOutcome::no_regression;
    return decision;
}

} // namespace nyamist::runtime
