#include "nyamist/runtime/vulkan_graphics_pipeline_manager.hpp"

namespace nyamist::runtime {

VulkanGraphicsPipelineManager::VulkanGraphicsPipelineManager(
    VulkanGraphicsPipelineDispatch dispatch) noexcept
    : dispatch_(dispatch) {}

VulkanGraphicsPipelineManager::~VulkanGraphicsPipelineManager() = default;

bool VulkanGraphicsPipelineManager::create(
    const std::vector<std::uint32_t>& vertex_spirv,
    const std::vector<std::uint32_t>& fragment_spirv,
    NativeHandle render_pass,
    NativeHandle descriptor_set_layout,
    VulkanGraphicsPipelineAllocation* out,
    std::string* error) {

    if (!out) {
        if (error) *error = "Graphics pipeline output allocation is null";
        return false;
    }

    *out = {};

    const auto valid_spirv = [](const std::vector<std::uint32_t>& code) {
        return code.size() >= 5U && code.front() == 0x07230203U;
    };

    if (!valid_spirv(vertex_spirv) || !valid_spirv(fragment_spirv)) {
        if (error) *error = "Invalid vertex or fragment SPIR-V module";
        return false;
    }

    if (render_pass == 0U) {
        if (error) *error = "Graphics pipeline render pass is null";
        return false;
    }

    if (!dispatch_.create_shader_module ||
        !dispatch_.destroy_shader_module ||
        !dispatch_.create_pipeline_layout ||
        !dispatch_.destroy_pipeline_layout ||
        !dispatch_.create_graphics_pipeline ||
        !dispatch_.destroy_graphics_pipeline) {
        if (error) *error = "Graphics pipeline dispatch table is incomplete";
        return false;
    }

    auto fail = [&]() {
        destroy(out);
        return false;
    };

    if (!dispatch_.create_shader_module(
            dispatch_.context, vertex_spirv.data(),
            vertex_spirv.size() * sizeof(std::uint32_t),
            &out->vertex_shader_module, error) ||
        out->vertex_shader_module == 0U) {
        if (error && error->empty())
            *error = "Vertex shader module creation failed";
        return fail();
    }

    if (!dispatch_.create_shader_module(
            dispatch_.context, fragment_spirv.data(),
            fragment_spirv.size() * sizeof(std::uint32_t),
            &out->fragment_shader_module, error) ||
        out->fragment_shader_module == 0U) {
        if (error && error->empty())
            *error = "Fragment shader module creation failed";
        return fail();
    }

    if (!dispatch_.create_pipeline_layout(
            dispatch_.context, descriptor_set_layout,
            &out->pipeline_layout, error) ||
        out->pipeline_layout == 0U) {
        if (error && error->empty())
            *error = "Graphics pipeline layout creation failed";
        return fail();
    }

    if (!dispatch_.create_graphics_pipeline(
            dispatch_.context,
            out->vertex_shader_module,
            out->fragment_shader_module,
            out->pipeline_layout,
            render_pass,
            &out->pipeline, error) ||
        out->pipeline == 0U) {
        if (error && error->empty())
            *error = "Graphics pipeline creation failed";
        return fail();
    }

    return true;
}

void VulkanGraphicsPipelineManager::destroy(
    VulkanGraphicsPipelineAllocation* allocation) noexcept {
    if (!allocation) return;

    if (allocation->pipeline && dispatch_.destroy_graphics_pipeline)
        dispatch_.destroy_graphics_pipeline(
            dispatch_.context, allocation->pipeline);

    if (allocation->pipeline_layout && dispatch_.destroy_pipeline_layout)
        dispatch_.destroy_pipeline_layout(
            dispatch_.context, allocation->pipeline_layout);

    if (allocation->fragment_shader_module && dispatch_.destroy_shader_module)
        dispatch_.destroy_shader_module(
            dispatch_.context, allocation->fragment_shader_module);

    if (allocation->vertex_shader_module && dispatch_.destroy_shader_module)
        dispatch_.destroy_shader_module(
            dispatch_.context, allocation->vertex_shader_module);

    *allocation = {};
}

} // namespace nyamist::runtime
