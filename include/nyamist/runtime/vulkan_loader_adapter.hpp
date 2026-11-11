#pragma once
#include "nyamist/runtime/vulkan_backend.hpp"
#include "nyamist/runtime/vulkan_resource_manager.hpp"
#include "nyamist/runtime/vulkan_compute_pipeline_manager.hpp"
#include "nyamist/runtime/vulkan_graphics_pipeline_manager.hpp"
#include <memory>
#include <string>
#include <vector>

namespace nyamist::runtime {

// Owns the public Vulkan loader handle. Keep this object alive until after the
// VulkanBackendSession has been shut down.
class VulkanLoaderAdapter final {
public:
    VulkanLoaderAdapter();
    ~VulkanLoaderAdapter();
    VulkanLoaderAdapter(const VulkanLoaderAdapter&) = delete;
    VulkanLoaderAdapter& operator=(const VulkanLoaderAdapter&) = delete;
    VulkanLoaderAdapter(VulkanLoaderAdapter&&) = delete;
    VulkanLoaderAdapter& operator=(VulkanLoaderAdapter&&) = delete;

    [[nodiscard]] bool open(std::string* error = nullptr);
    void close() noexcept;
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] VulkanBackendDispatch dispatch() noexcept;
    [[nodiscard]] VulkanBufferDispatch buffer_dispatch() noexcept;
    [[nodiscard]] VulkanComputePipelineDispatch compute_dispatch() noexcept;
    [[nodiscard]] VulkanGraphicsPipelineDispatch graphics_dispatch() noexcept;
    [[nodiscard]] bool create_color_render_pass(
        std::uint32_t format, NativeHandle* out, std::string* error = nullptr);
    void destroy_render_pass(NativeHandle render_pass) noexcept;

    [[nodiscard]] bool create_color_image(
        std::uint32_t width, std::uint32_t height,
        std::uint32_t format, std::uint32_t usage,
        NativeHandle* image, VulkanBufferMemoryRequirements* requirements,
        std::string* error = nullptr);
    [[nodiscard]] bool bind_image_memory(
        NativeHandle image, NativeHandle memory, std::string* error = nullptr);
    void destroy_image(NativeHandle image) noexcept;
    [[nodiscard]] bool create_image_view(
        NativeHandle image, std::uint32_t format,
        NativeHandle* view, std::string* error = nullptr);
    void destroy_image_view(NativeHandle view) noexcept;
    [[nodiscard]] bool create_framebuffer(
        NativeHandle render_pass, NativeHandle image_view,
        std::uint32_t width, std::uint32_t height,
        NativeHandle* framebuffer, std::string* error = nullptr);
    void destroy_framebuffer(NativeHandle framebuffer) noexcept;
    [[nodiscard]] std::vector<VulkanBufferMemoryType> memory_types() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace nyamist::runtime
