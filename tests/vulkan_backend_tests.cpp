#include "nyamist/runtime/vulkan_backend.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using nyamist::runtime::NativeHandle;
using nyamist::runtime::VulkanBackendDispatch;
using nyamist::runtime::VulkanBackendSession;

int destroyed_devices = 0;
int destroyed_instances = 0;
int submitted_commands = 0;
bool fail_device = false;
bool fail_queue = false;
bool fail_submit = false;

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
bool createInstance(NativeHandle* out, std::string*) { *out = 11U; return true; }
bool selectDevice(NativeHandle instance, NativeHandle* out, std::uint32_t* family,
                  std::string*) {
    check(instance == 11U, "physical-device selection must receive the owned instance");
    *out = 22U; *family = 3U; return true;
}
bool createDevice(NativeHandle physical, std::uint32_t family, NativeHandle* out,
                  std::string* error) {
    check(physical == 22U && family == 3U, "device creation must use selected GPU/queue family");
    if (fail_device) { *error = "injected device failure"; return false; }
    *out = 33U; return true;
}
bool getQueue(NativeHandle device, std::uint32_t family, NativeHandle* out,
              std::string* error) {
    check(device == 33U && family == 3U, "queue retrieval must use the created device");
    if (fail_queue) { *error = "injected queue failure"; return false; }
    *out = 44U; return true;
}
bool submitEmpty(NativeHandle device, NativeHandle queue, std::string* error) {
    check(device == 33U && queue == 44U, "submission must use the owned device and queue");
    if (fail_submit) { *error = "injected submit failure"; return false; }
    ++submitted_commands;
    return true;
}
void destroyDevice(NativeHandle handle) { check(handle == 33U, "destroy correct device"); ++destroyed_devices; }
void destroyInstance(NativeHandle handle) { check(handle == 11U, "destroy correct instance"); ++destroyed_instances; }

VulkanBackendDispatch dispatch() {
    return {createInstance, selectDevice, createDevice, getQueue,
            destroyDevice, destroyInstance, submitEmpty};
}

void testSuccessfulLifetimeAndSubmission() {
    destroyed_devices = destroyed_instances = submitted_commands = 0;
    {
        VulkanBackendSession backend(dispatch());
        check(!backend.submit_smoke_command(), "submission before initialize must be rejected");
        check(backend.initialize(), "backend must initialize");
        check(backend.status().initialized && backend.status().graphics_queue == 44U,
              "initialized session must expose a valid graphics queue");
        check(backend.initialize(), "initialize must be idempotent");
        check(backend.submit_smoke_command(), "smoke command must complete");
        check(backend.submit_smoke_command(), "second smoke command must complete");
        check(backend.status().completed_smoke_submissions == 2U &&
              backend.status().last_submission_status == "success",
              "completed submissions must be counted");
        backend.shutdown();
        backend.shutdown();
        check(!backend.status().initialized, "shutdown must clear initialized state");
    }
    check(destroyed_devices == 1 && destroyed_instances == 1,
          "device and instance must each be destroyed exactly once");
}
void testSubmissionFailureIsReported() {
    submitted_commands = 0;
    fail_submit = true;
    VulkanBackendSession backend(dispatch());
    check(backend.initialize(), "backend must initialize before injected submit failure");
    check(!backend.submit_smoke_command(), "injected submit failure must be reported");
    check(backend.status().last_submission_status == "failed" &&
          backend.status().completed_smoke_submissions == 0U &&
          backend.status().error == "injected submit failure",
          "failed submission must not increment completion count");
    fail_submit = false;
}
void testRollbackAfterDeviceFailure() {
    destroyed_devices = destroyed_instances = 0;
    fail_device = true;
    VulkanBackendSession backend(dispatch());
    check(!backend.initialize(), "injected device failure must fail initialization");
    check(backend.status().instance == 0U && !backend.status().initialized,
          "failed initialization must clear owned handles");
    check(destroyed_instances == 1 && destroyed_devices == 0,
          "rollback must destroy instance when device creation fails");
    fail_device = false;
}
void testRollbackAfterQueueFailure() {
    destroyed_devices = destroyed_instances = 0;
    fail_queue = true;
    VulkanBackendSession backend(dispatch());
    check(!backend.initialize(), "injected queue failure must fail initialization");
    check(destroyed_devices == 1 && destroyed_instances == 1,
          "rollback must destroy device before instance");
    fail_queue = false;
}
}
int main() {
    try {
        testSuccessfulLifetimeAndSubmission();
        testSubmissionFailureIsReported();
        testRollbackAfterDeviceFailure();
        testRollbackAfterQueueFailure();
        std::cout << "All NYAMIST Vulkan backend lifecycle/submission tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Vulkan backend test failure: " << error.what() << '\n';
        return 1;
    }
}
