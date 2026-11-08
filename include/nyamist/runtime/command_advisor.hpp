#pragma once

#include "nyamist/runtime/pipeline_gate.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nyamist::runtime {

struct CommandProposal {
    std::uint32_t schema_version = 1U;
    std::string policy_id = "shader-worker-budget";
    std::string policy_version = "1";
    std::string setting = "shader_compilation_workers";
    std::string action = "insufficient_evidence";
    bool evidence_sufficient = false;
    bool automatic_application_allowed = false;
    std::uint32_t minimum_value = 1U;
    std::uint32_t maximum_value = 4U;
    std::uint32_t current_value = 1U;
    std::uint32_t proposed_value = 1U;
    std::uint32_t rollback_value = 1U;
    std::string rationale;
    std::string expected_benefit;
    std::string risk;
    std::vector<std::string> evidence;
};

class RuntimeCommandAdvisor {
public:
    // Produces a bounded proposal only. It never applies process, driver,
    // translator, or Android system configuration.
    [[nodiscard]] static CommandProposal propose(
        const DeviceSessionSnapshot& session,
        const PipelineGateDecision& gate,
        std::uint32_t current_worker_budget);
};

} // namespace nyamist::runtime
