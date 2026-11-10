#pragma once

#include <cstdint>
#include <string>

namespace nyamist::runtime {

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
    // Records, submits, and waits for a minimal command buffer. The platform
    // adapter owns all transient command-pool/buffer/fence objects.
    bool (*submit_empty_command_and_wait)(NativeHandle device, NativeHandle queue,
                                          std::string* error){};
};

struct VulkanBackendStatus {
    bool initialized{};
    NativeHandle instance{};
    NativeHandle physical_device{};
    NativeHandle device{};
    NativeHandle graphics_queue{};
    std::uint32_t graphics_queue_family{};
    std::uint64_t completed_smoke_submissions{};
    std::string last_submission_status{"not_attempted"};
    std::string error;
};

class VulkanBackendSession final {
public:
    explicit VulkanBackendSession(VulkanBackendDispatch dispatch) noexcept;
    ~VulkanBackendSession();

    VulkanBackendSession(const VulkanBackendSession&) = delete;
    VulkanBackendSession& operator=(const VulkanBackendSession&) = delete;
    VulkanBackendSession(VulkanBackendSession&&) = delete;
    VulkanBackendSession& operator=(VulkanBackendSession&&) = delete;

    [[nodiscard]] bool initialize();
    // Diagnostic no-op command submission; this is not a rendering operation.
    [[nodiscard]] bool submit_smoke_command();
    void shutdown() noexcept;
    [[nodiscard]] const VulkanBackendStatus& status() const noexcept;

private:
    VulkanBackendDispatch dispatch_{};
    VulkanBackendStatus status_{};
};

} // namespace nyamist::runtime
