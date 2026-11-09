#include "nyamist/discovery/command_submission_validation.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testJsonContract() {
    nyamist::discovery::CommandSubmissionValidationReport report;
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
    report.queue_family_index = 0U;
    report.queue_handle_non_null = true;
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
    report.cleanup_status = "success";
    report.fence_destroyed = true;
    report.command_buffer_freed = true;
    report.command_pool_destroyed = true;
    report.logical_device_destroyed = true;
    report.instance_destroyed = true;

    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"schema_version\": 1") != std::string::npos,
          "schema version must be serialized");
    check(json.find("\"project\": \"CHORD OF NYAMIST / NYAMIST G925 R1.3\"") != std::string::npos,
          "R1.3 project identity must be serialized");
    check(json.find("\"queue_submission\":") != std::string::npos &&
          json.find("\"submitted\": true") != std::string::npos,
          "queue submission must be explicit");
    check(json.find("\"wait_result\": \"VK_SUCCESS\"") != std::string::npos,
          "fence wait result must be serialized");
    check(json.find("\"signaled\": true") != std::string::npos,
          "fence signal state must be explicit");
    check(json.find("\"command_pool_destroyed\": true") != std::string::npos,
          "command-pool cleanup must be serialized");
    check(json.find("\"logical_device_destroyed\": true") != std::string::npos,
          "logical-device cleanup must be serialized");
}

void testLiveResultIsExplicitAndBounded() {
    const auto report =
        nyamist::discovery::collectCommandSubmissionValidationReport();
    check(report.status == "success" || report.status == "failed" ||
          report.status == "unavailable",
          "live R1.3 validation must return an explicit bounded status");

    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"physical_device\":") != std::string::npos,
          "report must include physical-device identity");
    check(json.find("\"queue_submission\":") != std::string::npos,
          "report must separate queue submission");
    check(json.find("\"fence\":") != std::string::npos,
          "report must separate fence completion");
    check(json.find("\"cleanup\":") != std::string::npos,
          "report must include cleanup status");

    if (report.status == "success") {
        check(report.logical_device_status == "success",
              "overall success requires logical-device creation");
        check(report.queue_handle_non_null,
              "overall success requires a non-null queue");
        check(report.command_pool_status == "success" &&
              report.command_buffer_status == "success" &&
              report.command_recording_status == "success",
              "overall success requires command-pool and command-buffer setup");
        check(report.queue_submit_status == "success" &&
              report.queue_submission_succeeded,
              "overall success requires queue submission");
        check(report.fence_wait_status == "success" &&
              report.fence_status_status == "success" &&
              report.fence_signaled,
              "overall success requires fence completion");
        check(report.cleanup_status == "success" &&
              report.fence_destroyed && report.command_buffer_freed &&
              report.command_pool_destroyed && report.logical_device_destroyed &&
              report.instance_destroyed,
              "overall success requires complete Vulkan object cleanup");
    }
}

} // namespace

int main() {
    try {
        testJsonContract();
        testLiveResultIsExplicitAndBounded();
        std::cout << "All NYAMIST command-submission validation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Command-submission validation test failure: "
                  << error.what() << '\n';
        return 1;
    }
}
