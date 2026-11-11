#pragma once

#include "nyamist/runtime/vulkan_backend.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nyamist::runtime {

struct VulkanGraphicsPipelineAllocation {
    NativeHandle vertex_shader_module{};
    NativeHandle fragment_shader_module{};
    NativeHandle pipeline_layout{};
    NativeHandle pipeline{};
};

struct VulkanGraphicsPipelineDispatch {
    bool (*create_shader_module)(
        void*, const std::uint32_t*, std::size_t,
        NativeHandle*, std::string*){};
    void (*destroy_shader_module)(void*, NativeHandle){};

    bool (*create_pipeline_layout)(
        void*, NativeHandle, NativeHandle*, std::string*){};
    void (*destroy_pipeline_layout)(void*, NativeHandle){};

    bool (*create_graphics_pipeline)(
        void*, NativeHandle, NativeHandle, NativeHandle,
        NativeHandle, NativeHandle*, std::string*){};
    void (*destroy_graphics_pipeline)(void*, NativeHandle){};

    void* context{};
};

class VulkanGraphicsPipelineManager final {
public:
    explicit VulkanGraphicsPipelineManager(
        VulkanGraphicsPipelineDispatch dispatch) noexcept;

    ~VulkanGraphicsPipelineManager();

    VulkanGraphicsPipelineManager(const VulkanGraphicsPipelineManager&) = delete;
    VulkanGraphicsPipelineManager& operator=(
        const VulkanGraphicsPipelineManager&) = delete;

    [[nodiscard]] bool create(
        const std::vector<std::uint32_t>& vertex_spirv,
        const std::vector<std::uint32_t>& fragment_spirv,
        NativeHandle render_pass,
        NativeHandle descriptor_set_layout,
        VulkanGraphicsPipelineAllocation* out,
        std::string* error = nullptr);

    void destroy(VulkanGraphicsPipelineAllocation* allocation) noexcept;

private:
    VulkanGraphicsPipelineDispatch dispatch_{};
};

} // namespace nyamist::runtime
