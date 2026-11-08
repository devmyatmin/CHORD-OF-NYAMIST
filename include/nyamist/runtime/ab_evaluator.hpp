#pragma once

#include "nyamist/runtime/session_snapshot.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nyamist::runtime {

struct ABRunRecord {
    std::string workload_id;
    std::string device_profile_id;
    std::string compatibility_profile_key;
    std::uint64_t started_at_unix_ms = 0U;
    std::uint64_t duration_seconds = 0U;
    bool render_present_passed = false;
    std::uint64_t frame_count = 0U;
    int process_exit_code = -1;
    std::uint64_t present_calls = 0U;
    std::uint64_t present_failures = 0U;
    bool present_failures_reported = false;
    std::uint64_t device_lost_events = 0U;
    bool device_lost_status_reported = false;
    DeviceSessionSnapshot session;
};

struct ABEvaluationPolicy {
    std::uint32_t minimum_repetitions = 3U;
    std::uint64_t minimum_run_duration_seconds = 300U;
    // 500 basis points = 5% p95 frame-time change.
    std::uint32_t regression_threshold_basis_points = 500U;
};

enum class ABOutcome {
    insufficient_evidence,
    blocked,
    no_regression,
    improved,
    regressed
};

struct ABEvaluationDecision {
    ABOutcome outcome = ABOutcome::insufficient_evidence;
    std::uint32_t baseline_repetitions = 0U;
    std::uint32_t candidate_repetitions = 0U;
    std::uint64_t baseline_median_p95_microseconds = 0U;
    std::uint64_t candidate_median_p95_microseconds = 0U;
    std::optional<std::int64_t> delta_basis_points;
    bool automatic_application_allowed = false;
    std::vector<std::string> blockers;
    std::vector<std::string> warnings;
    std::vector<std::string> evidence;
};

// Compares repeated, sustained runs from the same device and workload.
// A result is advisory only: no runtime setting is modified or auto-applied.
class ABRegressionEvaluator {
public:
    [[nodiscard]] static ABEvaluationDecision evaluate(
        const std::vector<ABRunRecord>& baseline,
        const std::vector<ABRunRecord>& candidate,
        const ABEvaluationPolicy& policy = {});
};

[[nodiscard]] const char* abOutcomeName(ABOutcome outcome) noexcept;

} // namespace nyamist::runtime
