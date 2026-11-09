#include "nyamist/discovery/logical_device_validation.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testGraphicsQueueSelection() {
    using nyamist::discovery::QueueFamilyCandidate;
    using nyamist::discovery::selectGraphicsQueueFamily;
    using nyamist::discovery::kVulkanGraphicsQueueBit;

    check(!selectGraphicsQueueFamily({}).has_value(),
          "empty queue-family inventory must not select a queue");

    const std::vector<QueueFamilyCandidate> candidates{
        {0U, 0U, kVulkanGraphicsQueueBit},
        {1U, 2U, 0x00000002U},
        {2U, 1U, kVulkanGraphicsQueueBit | 0x00000002U}
    };
    const auto selected = selectGraphicsQueueFamily(candidates);
    check(selected.has_value() && *selected == 2U,
          "selector must ignore empty/non-graphics families and choose a graphics family");
}

void testJsonContract() {
    nyamist::discovery::LogicalDeviceValidationReport report;
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
    report.queue_families = {{0U, 1U, 0x00000007U}};
    report.queue_selection_status = "success";
    report.has_selected_queue_family = true;
    report.selected_queue_family_index = 0U;
    report.selected_queue_count = 1U;
    report.selected_queue_flags = 0x00000007U;
    report.logical_device_status = "success";
    report.logical_device_result = "VK_SUCCESS";
    report.queue_retrieval_status = "success";
    report.queue_retrieval_result = "VK_SUCCESS (non-null queue handle)";
    report.queue_handle_non_null = true;
    report.cleanup_status = "success";
    report.logical_device_destroyed = true;

    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"status\": \"success\"") != std::string::npos,
          "overall validation status must be serialized");
    check(json.find("\"target_gpu_name_match\": true") != std::string::npos,
          "target GPU name match must be explicit");
    check(json.find("\"family_index\": 0") != std::string::npos,
          "selected queue-family index must be serialized");
    check(json.find("\"create_result\": \"VK_SUCCESS\"") != std::string::npos,
          "logical-device creation result must be serialized");
    check(json.find("\"queue_handle_non_null\": true") != std::string::npos,
          "queue handle validation must be explicit");
    check(json.find("\"enabled_device_extension_count\": 0") != std::string::npos,
          "R1.2 must not enable device extensions");
    check(json.find("\"enabled_core_features\": \"none\"") != std::string::npos,
          "R1.2 must not enable optional core features");
    check(json.find("\"logical_device_destroyed\": true") != std::string::npos,
          "logical-device cleanup result must be serialized");
}

void testLiveResultIsExplicitAndSafe() {
    const auto report =
        nyamist::discovery::collectLogicalDeviceValidationReport();
    check(report.status == "success" || report.status == "failed" ||
          report.status == "unavailable",
          "live validation must return an explicit bounded state");

    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"platform_scope\": \"process-local\"") != std::string::npos,
          "report must identify the process-local validation scope");
    check(json.find("\"logical_device\":") != std::string::npos &&
          json.find("\"queue_retrieval\":") != std::string::npos,
          "report must keep device creation and queue retrieval separate");

    if (report.status == "success") {
        check(report.logical_device_status == "success",
              "overall success requires logical-device creation");
        check(report.queue_retrieval_status == "success" &&
              report.queue_handle_non_null,
              "overall success requires a non-null retrieved queue");
        check(report.logical_device_destroyed &&
              report.cleanup_status == "success",
              "overall success requires logical-device destruction");
    }
}

} // namespace

int main() {
    try {
        testGraphicsQueueSelection();
        testJsonContract();
        testLiveResultIsExplicitAndSafe();
        std::cout << "All NYAMIST logical-device validation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Logical-device validation test failure: "
                  << error.what() << '\n';
        return 1;
    }
}
