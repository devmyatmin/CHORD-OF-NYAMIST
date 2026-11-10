#pragma once

#include <cstdint>
#include <string>

namespace nyamist::runtime {

// Opaque handles keep the runtime API independent from Vulkan headers.
// The platform loader adapter supplies these callbacks from vkGet*ProcAddr.
using NativeHandle = std::uintptr_t;

struct VulkanBackendDispatch {
    bool (*create_instance)(NativeHandle* instance, std::string* error){};
    bool (*select_physical_device)(NativeHandle instance, NativeHandle* physical,
                                   std::uint32_t* graphics_family, std::string* error){};
    bool (*create_device)(NativeHandle physical, std::uint32_t graphics_family,
                          NativeHandle* device, std::string* error){};
    bool (*get_queue)(NativeHandle device, std::uint32_t graphics_family,
                      NativeHandle* queue, std::string* error){};
    void (*destroy_device)(NativeHandle device){};
    void (*destroy_instance)(NativeHandle instance){};
};

struct VulkanBackendStatus {
    bool initialized{};
    NativeHandle instance{};
    NativeHandle physical_device{};
    NativeHandle device{};
    NativeHandle graphics_queue{};
    std::uint32_t graphics_queue_family{};
    std::string error;
};

// Owns the lifetime of one Vulkan instance/device/graphics-queue session.
// Calls are serialized by the caller; queue submission is added by the next
// backend slice. Destroying the session is always safe, including after a
// partially failed initialization.
class VulkanBackendSession final {
public:
    explicit VulkanBackendSession(VulkanBackendDispatch dispatch) noexcept;
    ~VulkanBackendSession();

    VulkanBackendSession(const VulkanBackendSession&) = delete;
    VulkanBackendSession& operator=(const VulkanBackendSession&) = delete;
    VulkanBackendSession(VulkanBackendSession&&) = delete;
    VulkanBackendSession& operator=(VulkanBackendSession&&) = delete;

    [[nodiscard]] bool initialize();
    void shutdown() noexcept;
    [[nodiscard]] const VulkanBackendStatus& status() const noexcept;

private:
    VulkanBackendDispatch dispatch_{};
    VulkanBackendStatus status_{};
};

} // namespace nyamist::runtime
