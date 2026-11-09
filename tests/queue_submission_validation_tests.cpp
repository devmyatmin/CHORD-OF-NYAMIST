#include "nyamist/discovery/queue_submission_validation.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testDefaultReportIsBounded() {
    const nyamist::discovery::QueueSubmissionValidationReport report;
    check(report.status == "unavailable",
          "default report must begin in an explicit unavailable state");
    check(report.command_pool_status == "not_attempted",
          "command pool must not claim execution before the probe");
    check(report.queue_submit_status == "not_attempted",
          "queue submit must not claim execution before the probe");
    check(!report.fence_signaled,
          "fence must not be reported signaled before the probe");
}

void testJsonContract() {
    nyamist::discovery::QueueSubmissionValidationReport report;
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
    report.queue_families = {{0U, 2U, 23U}};
    report.queue_selection_status = "success";
    report.has_selected_queue_family = true;
    report.selected_queue_family_index = 0U;
    report.selected_queue_count = 2U;
    report.selected_queue_flags = 23U;
    report.logical_device_status = "success";
    report.logical_device_result = "VK_SUCCESS";
    report.queue_retrieval_status = "success";
    report.queue_retrieval_result = "VK_SUCCESS (non-null queue handle)";
    report.queue_handle_non_null = true;
    report.cleanup_status = "success";
    report.logical_device_destroyed = true;
    report.command_pool_status = "success";
    report.command_buffer_allocation_status = "success";
    report.command_buffer_record_status = "success";
    report.fence_creation_status = "success";
    report.queue_submit_status = "success";
    report.fence_wait_status = "success";
    report.command_resource_cleanup_status = "success";
    report.command_pool_created = true;
    report.command_buffer_allocated = true;
    report.command_buffer_recorded = true;
    report.fence_created = true;
    report.queue_submit_accepted = true;
    report.fence_signaled = true;
    report.command_resources_destroyed = true;

    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"project\": \"CHORD OF NYAMIST / NYAMIST G925 R1.3\"")
              != std::string::npos,
          "R1.3 report identity must be serialized");
    check(json.find("\"logical_device_validation\": {") != std::string::npos,
          "R1.2 logical-device evidence must be nested in the R1.3 report");
    check(json.find("\"submit_status\": \"success\"") != std::string::npos,
          "queue-submit status must be serialized");
    check(json.find("\"fence_signaled\": true") != std::string::npos,
          "fence completion must be explicit");
    check(json.find("\"fence_timeout_ns\": 5000000000") != std::string::npos,
          "bounded fence timeout must be serialized");
    check(json.find("\"resources_destroyed\": true") != std::string::npos,
          "command-resource cleanup must be explicit");
}

void testLiveResultIsExplicitAndSafe() {
    const auto report =
        nyamist::discovery::collectQueueSubmissionValidationReport();
    check(report.status == "success" || report.status == "failed" ||
          report.status == "unavailable",
          "live R1.3 validation must return an explicit bounded state");

    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"platform_scope\": \"process-local\"") != std::string::npos,
          "R1.3 report must retain process-local validation scope");
    check(json.find("\"queue_submission\":") != std::string::npos,
          "report must include queue submission fields");
    check(json.find("\"command_resource_cleanup\":") != std::string::npos,
          "report must include command-resource cleanup fields");

    if (report.status == "success") {
        check(report.logical_device_status == "success" &&
              report.queue_retrieval_status == "success" &&
              report.logical_device_destroyed,
              "R1.3 success requires the complete R1.2 device/queue lifecycle");
        check(report.command_pool_created && report.command_buffer_allocated &&
              report.command_buffer_recorded,
              "R1.3 success requires a recorded command buffer");
        check(report.queue_submit_accepted && report.fence_signaled,
              "R1.3 success requires queue submission and fence completion");
        check(report.command_resources_destroyed,
              "R1.3 success requires command resources to be destroyed");
    }
}

} // namespace

int main() {
    try {
        testDefaultReportIsBounded();
        testJsonContract();
        testLiveResultIsExplicitAndSafe();
        std::cout << "All NYAMIST queue-submission validation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Queue-submission validation test failure: "
                  << error.what() << '\\n';
        return 1;
    }
}
