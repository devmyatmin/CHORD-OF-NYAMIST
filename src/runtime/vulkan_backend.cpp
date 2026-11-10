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
        dispatch_.destroy_instance == nullptr ||
        dispatch_.submit_empty_command_and_wait == nullptr) {
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

bool VulkanBackendSession::submit_smoke_command() {
    if (!status_.initialized || status_.device == 0U || status_.graphics_queue == 0U) {
        status_.error = "Cannot submit a command before backend initialization";
        status_.last_submission_status = "rejected_not_initialized";
        return false;
    }

    status_.error.clear();
    if (!dispatch_.submit_empty_command_and_wait(
            status_.device, status_.graphics_queue, &status_.error)) {
        if (status_.error.empty()) status_.error = "Vulkan smoke command submission failed";
        status_.last_submission_status = "failed";
        return false;
    }

    ++status_.completed_smoke_submissions;
    status_.last_submission_status = "success";
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
