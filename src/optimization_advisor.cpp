#include "nyamist/optimization_advisor.hpp"

#include <algorithm>

namespace nyamist {

OptimizationPlan RuleBasedAdvisor::recommend(
    const GpuProfile& gpu,
    const ShaderProfile& shader) const {
    OptimizationPlan plan;
    plan.use_graphics_pipeline_library = gpu.supports_graphics_pipeline_library;

    const std::uint32_t cores = std::max<std::uint32_t>(gpu.logical_cpu_count, 1U);
    plan.suggested_shader_workers = std::clamp(cores / 2U, 1U, 4U);

    if (gpu.supports_graphics_pipeline_library) {
        plan.notes.emplace_back(
            "Driver reports graphics-pipeline-library support; allow DXVK to use its supported pipeline path.");
    } else {
        plan.notes.emplace_back(
            "Do not force graphics-pipeline-library features; retain the backend's supported fallback.");
    }

    if (shader.uses_geometry_shader || shader.uses_tessellation) {
        plan.compatibility_warnings.emplace_back(
            "Geometry/tessellation usage requires a real Vulkan feature and game-level compatibility probe.");
    }
    if (shader.uses_bindless_resources) {
        plan.compatibility_warnings.emplace_back(
            "Bindless-resource usage requires descriptor-indexing capability checks.");
    }
    if (shader.uses_uav_atomics) {
        plan.compatibility_warnings.emplace_back(
            "UAV atomics require format, memory-model, and synchronization validation.");
    }
    if (shader.instruction_count > 100000U) {
        plan.notes.emplace_back(
            "Large shader profile: benchmark compilation scheduling; do not assume more worker threads are faster.");
    }

    plan.notes.emplace_back(
        "Recommendations are advisory only; the DXVK/Vulkan runtime remains responsible for rendering correctness.");
    return plan;
}

} // namespace nyamist
