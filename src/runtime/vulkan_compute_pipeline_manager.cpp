#include "nyamist/runtime/vulkan_compute_pipeline_manager.hpp"

namespace nyamist::runtime {

VulkanComputePipelineManager::VulkanComputePipelineManager(
    VulkanComputePipelineDispatch dispatch) noexcept
    : dispatch_(dispatch) {}

VulkanComputePipelineManager::~VulkanComputePipelineManager() = default;

bool VulkanComputePipelineManager::create(
    const std::vector<std::uint32_t>& spirv,
    VulkanComputePipelineAllocation* out,
    std::string* error) {

    if (!out) {
        if (error) *error = "Compute pipeline output allocation is null";
        return false;
    }

    *out = {};

    if (spirv.size() < 5U || spirv.front() != 0x07230203U) {
        if (error) *error = "Invalid SPIR-V header or module is too small";
        return false;
    }

    if (!dispatch_.create_shader_module ||
        !dispatch_.destroy_shader_module ||
        !dispatch_.create_descriptor_set_layout ||
        !dispatch_.destroy_descriptor_set_layout ||
        !dispatch_.create_descriptor_pool ||
        !dispatch_.destroy_descriptor_pool ||
        !dispatch_.allocate_descriptor_set ||
        !dispatch_.create_pipeline_layout ||
        !dispatch_.destroy_pipeline_layout ||
        !dispatch_.create_compute_pipeline ||
        !dispatch_.destroy_compute_pipeline) {
        if (error) *error = "Compute pipeline dispatch table is incomplete";
        return false;
    }

    auto fail = [&]() {
        destroy(out);
        return false;
    };

    if (!dispatch_.create_shader_module(
            dispatch_.context, spirv.data(),
            spirv.size() * sizeof(std::uint32_t),
            &out->shader_module, error) || out->shader_module == 0U) {
        if (error && error->empty()) *error = "Shader module creation failed";
        return fail();
    }

    if (!dispatch_.create_descriptor_set_layout(
            dispatch_.context, &out->descriptor_set_layout, error) ||
        out->descriptor_set_layout == 0U) {
        if (error && error->empty()) *error = "Descriptor set layout creation failed";
        return fail();
    }

    if (!dispatch_.create_descriptor_pool(
            dispatch_.context, &out->descriptor_pool, error) ||
        out->descriptor_pool == 0U) {
        if (error && error->empty()) *error = "Descriptor pool creation failed";
        return fail();
    }

    if (!dispatch_.allocate_descriptor_set(
            dispatch_.context, out->descriptor_pool,
            out->descriptor_set_layout, &out->descriptor_set, error) ||
        out->descriptor_set == 0U) {
        if (error && error->empty()) *error = "Descriptor set allocation failed";
        return fail();
    }

    if (!dispatch_.create_pipeline_layout(
            dispatch_.context, out->descriptor_set_layout,
            &out->pipeline_layout, error) ||
        out->pipeline_layout == 0U) {
        if (error && error->empty()) *error = "Pipeline layout creation failed";
        return fail();
    }

    if (!dispatch_.create_compute_pipeline(
            dispatch_.context, out->shader_module,
            out->pipeline_layout, &out->pipeline, error) ||
        out->pipeline == 0U) {
        if (error && error->empty()) *error = "Compute pipeline creation failed";
        return fail();
    }

    return true;
}

void VulkanComputePipelineManager::destroy(
    VulkanComputePipelineAllocation* allocation) noexcept {
    if (!allocation) return;

    if (allocation->pipeline && dispatch_.destroy_compute_pipeline)
        dispatch_.destroy_compute_pipeline(dispatch_.context, allocation->pipeline);

    if (allocation->pipeline_layout && dispatch_.destroy_pipeline_layout)
        dispatch_.destroy_pipeline_layout(dispatch_.context, allocation->pipeline_layout);

    if (allocation->descriptor_pool && dispatch_.destroy_descriptor_pool)
        dispatch_.destroy_descriptor_pool(dispatch_.context, allocation->descriptor_pool);

    if (allocation->descriptor_set_layout && dispatch_.destroy_descriptor_set_layout)
        dispatch_.destroy_descriptor_set_layout(
            dispatch_.context, allocation->descriptor_set_layout);

    if (allocation->shader_module && dispatch_.destroy_shader_module)
        dispatch_.destroy_shader_module(dispatch_.context, allocation->shader_module);

    *allocation = {};
}

} // namespace nyamist::runtime
