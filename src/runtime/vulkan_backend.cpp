#include "nyamist/runtime/vulkan_backend.hpp"

namespace nyamist::runtime {

VulkanBackendSession::VulkanBackendSession(VulkanBackendDispatch dispatch) noexcept
    : dispatch_(dispatch) {}

VulkanBackendSession::~VulkanBackendSession() {
    shutdown();
}

bool VulkanBackendSession::initialize() {
    if (status_.initialized) return true;
    shutdown();
    status_.error.clear();

    if (dispatch_.create_instance == nullptr ||
        dispatch_.select_physical_device == nullptr ||
        dispatch_.create_device == nullptr ||
        dispatch_.get_queue == nullptr ||
        dispatch_.destroy_device == nullptr ||
        dispatch_.destroy_instance == nullptr) {
        status_.error = "Vulkan backend dispatch table is incomplete";
        return false;
    }

    if (!dispatch_.create_instance(&status_.instance, &status_.error) ||
        status_.instance == 0U) {
        if (status_.error.empty()) status_.error = "Vulkan instance creation failed";
        shutdown();
        return false;
    }

    if (!dispatch_.select_physical_device(status_.instance,
            &status_.physical_device, &status_.graphics_queue_family,
            &status_.error) || status_.physical_device == 0U) {
        if (status_.error.empty()) status_.error = "No suitable Vulkan physical device";
        shutdown();
        return false;
    }

    if (!dispatch_.create_device(status_.physical_device,
            status_.graphics_queue_family, &status_.device,
            &status_.error) || status_.device == 0U) {
        if (status_.error.empty()) status_.error = "Vulkan logical device creation failed";
        shutdown();
        return false;
    }

    if (!dispatch_.get_queue(status_.device, status_.graphics_queue_family,
            &status_.graphics_queue, &status_.error) ||
        status_.graphics_queue == 0U) {
        if (status_.error.empty()) status_.error = "Vulkan graphics queue retrieval failed";
        shutdown();
        return false;
    }

    status_.initialized = true;
    return true;
}

void VulkanBackendSession::shutdown() noexcept {
    if (status_.device != 0U && dispatch_.destroy_device != nullptr) {
        dispatch_.destroy_device(status_.device);
    }
    status_.device = 0U;
    status_.graphics_queue = 0U;
    status_.physical_device = 0U;
    if (status_.instance != 0U && dispatch_.destroy_instance != nullptr) {
        dispatch_.destroy_instance(status_.instance);
    }
    status_.instance = 0U;
    status_.graphics_queue_family = 0U;
    status_.initialized = false;
}

const VulkanBackendStatus& VulkanBackendSession::status() const noexcept {
    return status_;
}

} // namespace nyamist::runtime
