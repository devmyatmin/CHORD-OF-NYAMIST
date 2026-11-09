#pragma once

#include <cstdint>
#include <string>

namespace nyamist::discovery {

struct CommandSubmissionValidationReport {
    std::uint32_t schema_version{1U};
    std::string status{"unavailable"};
    bool vulkan_compiled{};
    std::string loader_library;
    std::string loader_api_version;
    std::string platform_scope{"process-local"};

    std::string physical_device_status{"not_attempted"};
    std::string device_name;
    std::string vendor_id_hex;
    std::string device_id_hex;
    std::string physical_device_api_version;
    bool target_gpu_match{};

    std::string logical_device_status{"not_attempted"};
    std::uint32_t queue_family_index{};
    bool queue_handle_non_null{};

    std::string command_pool_status{"not_attempted"};
    std::string command_buffer_status{"not_attempted"};
    std::string command_recording_status{"not_attempted"};
    std::string queue_submit_status{"not_attempted"};
    std::string queue_submit_result;
    bool queue_submission_succeeded{};

    std::string fence_create_status{"not_attempted"};
    std::string fence_wait_status{"not_attempted"};
    std::string fence_wait_result;
    std::string fence_status_status{"not_attempted"};
    std::string fence_status_result;
    bool fence_signaled{};
    bool device_lost{};

    std::string device_idle_status{"not_attempted"};
    std::string cleanup_status{"not_attempted"};
    bool fence_destroyed{};
    bool command_buffer_freed{};
    bool command_pool_destroyed{};
    bool logical_device_destroyed{};
    bool instance_destroyed{};

    std::string failure_reason;
};

[[nodiscard]] CommandSubmissionValidationReport
collectCommandSubmissionValidationReport();

[[nodiscard]] std::string
toJson(const CommandSubmissionValidationReport& report);

} // namespace nyamist::discovery
