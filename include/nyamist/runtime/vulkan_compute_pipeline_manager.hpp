#pragma once

#include "nyamist/runtime/vulkan_backend.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nyamist::runtime {

struct VulkanComputePipelineAllocation {
    NativeHandle shader_module{};
    NativeHandle descriptor_set_layout{};
    NativeHandle descriptor_pool{};
    NativeHandle descriptor_set{};
    NativeHandle pipeline_layout{};
    NativeHandle pipeline{};
};

struct VulkanComputePipelineDispatch {
    bool (*create_shader_module)(void*, const std::uint32_t*, std::size_t,
                                 NativeHandle*, std::string*){};
    void (*destroy_shader_module)(void*, NativeHandle){};

    bool (*create_descriptor_set_layout)(void*, NativeHandle*, std::string*){};
    void (*destroy_descriptor_set_layout)(void*, NativeHandle){};

    bool (*create_descriptor_pool)(void*, NativeHandle*, std::string*){};
    void (*destroy_descriptor_pool)(void*, NativeHandle){};

    bool (*allocate_descriptor_set)(void*, NativeHandle, NativeHandle,
                                    NativeHandle*, std::string*){};

    bool (*create_pipeline_layout)(void*, NativeHandle, NativeHandle*,
                                   std::string*){};
    void (*destroy_pipeline_layout)(void*, NativeHandle){};

    bool (*create_compute_pipeline)(void*, NativeHandle, NativeHandle,
                                    NativeHandle*, std::string*){};
    void (*destroy_compute_pipeline)(void*, NativeHandle){};

    bool (*update_storage_buffer_descriptor)(
        void*, NativeHandle, NativeHandle, std::uint64_t, std::string*){};

    bool (*dispatch_compute)(
        void*, NativeHandle, NativeHandle, NativeHandle,
        std::uint32_t, std::uint32_t, std::uint32_t, std::string*){};

    void* context{};
};

class VulkanComputePipelineManager final {
public:
    explicit VulkanComputePipelineManager(VulkanComputePipelineDispatch dispatch) noexcept;
    ~VulkanComputePipelineManager();

    VulkanComputePipelineManager(const VulkanComputePipelineManager&) = delete;
    VulkanComputePipelineManager& operator=(const VulkanComputePipelineManager&) = delete;

    [[nodiscard]] bool create(const std::vector<std::uint32_t>& spirv,
                              VulkanComputePipelineAllocation* out,
                              std::string* error = nullptr);
    void destroy(VulkanComputePipelineAllocation* allocation) noexcept;

private:
    VulkanComputePipelineDispatch dispatch_{};
};

} // namespace nyamist::runtime
