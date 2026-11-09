#pragma once

#include <cstdint>
#include <string>

namespace nyamist::discovery {

struct GpuResourceTransferValidationReport {
    std::uint32_t schema_version{1U};
    std::string status{"unavailable"};
    bool vulkan_compiled{};
    std::string loader_library;
    std::string loader_api_version;

    std::string physical_device_status{"not_attempted"};
    std::string device_name;
    std::string vendor_id_hex;
    std::string device_id_hex;
    std::string physical_device_api_version;
    bool target_gpu_match{};

    std::string logical_device_status{"not_attempted"};
    std::uint32_t queue_family_index{};
    bool queue_handle_non_null{};

    std::uint64_t transfer_size_bytes{4096U};
    std::string source_buffer_status{"not_attempted"};
    std::string destination_buffer_status{"not_attempted"};
    std::string source_memory_type_status{"not_attempted"};
    std::string destination_memory_type_status{"not_attempted"};
    std::uint32_t source_memory_type_index{};
    std::uint32_t destination_memory_type_index{};
    bool source_memory_host_visible{};
    bool source_memory_host_coherent{};
    bool source_memory_device_local{};
    bool destination_memory_host_visible{};
    bool destination_memory_host_coherent{};
    bool destination_memory_device_local{};
    std::string source_memory_allocate_status{"not_attempted"};
    std::string destination_memory_allocate_status{"not_attempted"};
    std::string source_memory_bind_status{"not_attempted"};
    std::string destination_memory_bind_status{"not_attempted"};
    std::string source_map_status{"not_attempted"};
    std::string source_unmap_status{"not_attempted"};
    std::string destination_map_status{"not_attempted"};
    std::string destination_unmap_status{"not_attempted"};

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
    std::string device_idle_status{"not_attempted"};

    std::string readback_status{"not_attempted"};
    std::uint64_t bytes_copied{};
    std::uint64_t bytes_verified{};
    bool data_match{};
    bool has_mismatch_offset{};
    std::uint64_t first_mismatch_offset{};

    bool device_lost{};
    std::string cleanup_status{"not_attempted"};
    bool fence_destroyed{};
    bool command_buffer_freed{};
    bool command_pool_destroyed{};
    bool source_buffer_destroyed{};
    bool destination_buffer_destroyed{};
    bool source_memory_freed{};
    bool destination_memory_freed{};
    bool logical_device_destroyed{};
    bool instance_destroyed{};
    std::string failure_reason;
};

[[nodiscard]] GpuResourceTransferValidationReport
collectGpuResourceTransferValidationReport();

[[nodiscard]] std::string
toJson(const GpuResourceTransferValidationReport& report);

} // namespace nyamist::discovery
