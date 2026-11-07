#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nyamist {

struct GpuProfile {
    std::string gpu_name = "unknown";
    std::string gpu_architecture = "unknown";
    std::string vulkan_api_version = "unknown";
    std::string driver_version = "unknown";
    std::uint32_t logical_cpu_count = 4;
    bool supports_graphics_pipeline_library = false;
};

struct ShaderProfile {
    std::uint64_t instruction_count = 0;
    bool uses_geometry_shader = false;
    bool uses_tessellation = false;
    bool uses_bindless_resources = false;
    bool uses_uav_atomics = false;
};

struct OptimizationPlan {
    bool use_graphics_pipeline_library = false;
    std::uint32_t suggested_shader_workers = 1;
    std::vector<std::string> notes;
    std::vector<std::string> compatibility_warnings;
};

class IOptimizationAdvisor {
public:
    virtual ~IOptimizationAdvisor() = default;
    [[nodiscard]] virtual OptimizationPlan recommend(
        const GpuProfile& gpu,
        const ShaderProfile& shader) const = 0;
};

// Deterministic baseline. A future on-device AI implementation can implement
// IOptimizationAdvisor, but its proposals must still pass runtime validation.
class RuleBasedAdvisor final : public IOptimizationAdvisor {
public:
    [[nodiscard]] OptimizationPlan recommend(
        const GpuProfile& gpu,
        const ShaderProfile& shader) const override;
};

} // namespace nyamist
