#pragma once

#include "nyamist/discovery/logical_device_validation.hpp"

#include <string>

namespace nyamist::discovery {

struct QueueSubmissionValidationReport : LogicalDeviceValidationReport {
    std::string command_pool_status{"not_attempted"};
    std::string command_buffer_allocation_status{"not_attempted"};
    std::string command_buffer_record_status{"not_attempted"};
    std::string fence_creation_status{"not_attempted"};
    std::string queue_submit_status{"not_attempted"};
    std::string fence_wait_status{"not_attempted"};
    std::string command_resource_cleanup_status{"not_attempted"};
    bool command_pool_created{};
    bool command_buffer_allocated{};
    bool command_buffer_recorded{};
    bool fence_created{};
    bool queue_submit_accepted{};
    bool fence_signaled{};
    bool command_resources_destroyed{};
    std::string command_failure_reason;
};

// Records and submits one empty, one-time-use primary command buffer to the
// selected graphics queue, waits for its fence, then destroys all resources.
[[nodiscard]] QueueSubmissionValidationReport collectQueueSubmissionValidationReport();
[[nodiscard]] std::string toJson(const QueueSubmissionValidationReport& report);

} // namespace nyamist::discovery
