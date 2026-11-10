#pragma once
#include "nyamist/runtime/vulkan_backend.hpp"
#include "nyamist/runtime/vulkan_resource_manager.hpp"
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
    [[nodiscard]] std::vector<VulkanBufferMemoryType> memory_types() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace nyamist::runtime
