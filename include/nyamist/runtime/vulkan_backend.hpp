#pragma once
#include <cstdint>
#include <string>

namespace nyamist::runtime {
using NativeHandle = std::uintptr_t;

// Context is owned by the platform adapter and outlives this dispatch table.
struct VulkanBackendDispatch {
    bool (*create_instance)(void* context, NativeHandle* instance, std::string* error){};
    bool (*select_physical_device)(void* context, NativeHandle instance, NativeHandle* physical,
                                   std::uint32_t* graphics_family, std::string* error){};
    bool (*create_device)(void* context, NativeHandle physical, std::uint32_t graphics_family,
                          NativeHandle* device, std::string* error){};
    bool (*get_queue)(void* context, NativeHandle device, std::uint32_t graphics_family,
                      NativeHandle* queue, std::string* error){};
    void (*destroy_device)(void* context, NativeHandle device){};
    void (*destroy_instance)(void* context, NativeHandle instance){};
    bool (*submit_empty_command_and_wait)(void* context, NativeHandle device, NativeHandle queue,
                                          std::string* error){};
    void* context{};
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
    // Diagnostic no-op submission, not a rendering operation.
    [[nodiscard]] bool submit_smoke_command();
    void shutdown() noexcept;
    [[nodiscard]] const VulkanBackendStatus& status() const noexcept;

private:
    VulkanBackendDispatch dispatch_{};
    VulkanBackendStatus status_{};
};
} // namespace nyamist::runtime
