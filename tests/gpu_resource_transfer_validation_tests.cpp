#include "nyamist/discovery/gpu_resource_transfer_validation.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testJsonContract() {
    nyamist::discovery::GpuResourceTransferValidationReport report;
    report.status = "success";
    report.vulkan_compiled = true;
    report.loader_library = "libvulkan.so";
    report.loader_api_version = "1.4.0";
    report.physical_device_status = "success";
    report.device_name = "Mali-G925-Immortalis MC12";
    report.vendor_id_hex = "0x13b5";
    report.device_id_hex = "0xd8300010";
    report.physical_device_api_version = "1.3.278";
    report.target_gpu_match = true;
    report.logical_device_status = "success";
    report.queue_handle_non_null = true;
    report.source_buffer_status = "success";
    report.destination_buffer_status = "success";
    report.source_memory_type_status = "success";
    report.destination_memory_type_status = "success";
    report.source_memory_allocate_status = "success";
    report.destination_memory_allocate_status = "success";
    report.source_memory_bind_status = "success";
    report.destination_memory_bind_status = "success";
    report.source_map_status = "success";
    report.source_unmap_status = "success";
    report.command_pool_status = "success";
    report.command_buffer_status = "success";
    report.command_recording_status = "success";
    report.queue_submit_status = "success";
    report.queue_submit_result = "VK_SUCCESS";
    report.queue_submission_succeeded = true;
    report.fence_create_status = "success";
    report.fence_wait_status = "success";
    report.fence_wait_result = "VK_SUCCESS";
    report.fence_status_status = "success";
    report.fence_status_result = "VK_SUCCESS";
    report.fence_signaled = true;
    report.destination_map_status = "success";
    report.destination_unmap_status = "success";
    report.bytes_copied = report.transfer_size_bytes;
    report.bytes_verified = report.transfer_size_bytes;
    report.readback_status = "success";
    report.data_match = true;
    report.cleanup_status = "success";
    report.fence_destroyed = true;
    report.command_buffer_freed = true;
    report.command_pool_destroyed = true;
    report.source_buffer_destroyed = true;
    report.destination_buffer_destroyed = true;
    report.source_memory_freed = true;
    report.destination_memory_freed = true;
    report.logical_device_destroyed = true;
    report.instance_destroyed = true;

    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"schema_version\": 1") != std::string::npos,
          "schema version must be serialized");
    check(json.find("\"project\": \"CHORD OF NYAMIST / NYAMIST G925 R1.4\"") != std::string::npos,
          "R1.4 project identity must be serialized");
    check(json.find("\"source_create_status\": \"success\"") != std::string::npos,
          "source buffer creation status must be serialized");
    check(json.find("\"submit_result\": \"VK_SUCCESS\"") != std::string::npos,
          "queue submit result must be serialized");
    check(json.find("\"bytes_verified\": 4096") != std::string::npos,
          "verified byte count must be serialized");
    check(json.find("\"data_match\": true") != std::string::npos,
          "readback comparison must be serialized");
    check(json.find("\"source_memory_freed\": true") != std::string::npos,
          "source memory cleanup must be serialized");
}

void testLiveResultIsExplicitAndBounded() {
    const auto report =
        nyamist::discovery::collectGpuResourceTransferValidationReport();
    check(report.status == "success" || report.status == "failed" ||
          report.status == "unavailable",
          "live R1.4 validation must return an explicit bounded status");

    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"physical_device\":") != std::string::npos,
          "report must include physical-device identity");
    check(json.find("\"buffers\":") != std::string::npos,
          "report must include buffer creation");
    check(json.find("\"memory\":") != std::string::npos,
          "report must include memory allocation and binding");
    check(json.find("\"command\":") != std::string::npos,
          "report must include command submission and fence state");
    check(json.find("\"readback\":") != std::string::npos,
          "report must include readback verification");
    check(json.find("\"cleanup\":") != std::string::npos,
          "report must include cleanup state");

    if (report.status == "success") {
        check(report.source_buffer_status == "success" &&
              report.destination_buffer_status == "success",
              "overall success requires both buffers");
        check(report.source_memory_allocate_status == "success" &&
              report.destination_memory_allocate_status == "success",
              "overall success requires both memory allocations");
        check(report.source_memory_bind_status == "success" &&
              report.destination_memory_bind_status == "success",
              "overall success requires both memory bindings");
        check(report.queue_submission_succeeded && report.fence_signaled,
              "overall success requires completed transfer submission");
        check(report.bytes_copied == report.transfer_size_bytes &&
              report.bytes_verified == report.transfer_size_bytes && report.data_match,
              "overall success requires exact readback equality");
        check(report.cleanup_status == "success" &&
              report.source_buffer_destroyed && report.destination_buffer_destroyed &&
              report.source_memory_freed && report.destination_memory_freed &&
              report.logical_device_destroyed && report.instance_destroyed,
              "overall success requires complete resource cleanup");
    }
}

} // namespace

int main() {
    try {
        testJsonContract();
        testLiveResultIsExplicitAndBounded();
        std::cout << "All NYAMIST GPU resource-transfer validation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "GPU resource-transfer validation test failure: "
                  << error.what() << '\n';
        return 1;
    }
}
