#pragma once

#include "nyamist/runtime/runtime_coordinator.hpp"
#include "nyamist/runtime/session_snapshot.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nyamist::runtime {

enum class GateStatus {
    blocked,
    warning,
    ready
};

struct PipelineGateDecision {
    GateStatus status = GateStatus::blocked;
    bool session_fresh = false;
    bool vulkan_enumerated = false;
    bool smoke_test_allowed = false;
    bool production_ready = false;
    std::vector<std::string> blockers;
    std::vector<std::string> warnings;
    std::vector<std::string> evidence;
};

class PipelineGate {
public:
    // Deterministic gate: the caller provides the clock and maximum session
    // age. The gate only evaluates evidence; it never changes runtime settings.
    [[nodiscard]] static PipelineGateDecision evaluate(
        const RuntimePlan& runtime,
        const DeviceSessionSnapshot& session,
        std::uint64_t now_unix_ms,
        std::uint64_t maximum_session_age_ms);
};

[[nodiscard]] const char* gateStatusName(GateStatus status) noexcept;

} // namespace nyamist::runtime
