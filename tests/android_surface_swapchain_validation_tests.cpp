#include "nyamist/discovery/android_surface_swapchain_validation.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testUnavailableReportIsExplicit() {
    nyamist::discovery::AndroidSurfaceSwapchainValidationReport report;
    report.status = "unavailable";
    report.native_window_status = "not_available";
    report.failure_reason = "No Android Surface was supplied";
    const auto json = nyamist::discovery::toJson(report);
    check(json.find("\"schema_version\": 1") != std::string::npos,
          "schema version must be serialized");
    check(json.find("NYAMIST G925 R1.5") != std::string::npos,
          "R1.5 identity must be serialized");
    check(json.find("\"status\": \"unavailable\"") != std::string::npos,
          "missing native surface must be reported as unavailable");
    check(json.find("\"frame_content_written\": false") != std::string::npos,
          "the report must not imply rendering");
}

void testSuccessfulSwapchainReportContract() {
    nyamist::discovery::AndroidSurfaceSwapchainValidationReport report;
    report.status = "success";
    report.loader_status = "success";
    report.instance_create_status = "success";
    report.surface_extension_status = "success";
    report.android_surface_extension_status = "success";
    report.native_window_status = "success";
    report.surface_create_status = "success";
    report.physical_device_status = "success";
    report.device_name = "Mali-G925-Immortalis MC12";
    report.vendor_id_hex = "0x13b5";
    report.device_id_hex = "0xd8300010";
    report.physical_device_api_version = "1.3.278";
    report.target_gpu_match = true;
    report.swapchain_extension_status = "success";
    report.queue_family_status = "success";
    report.graphics_queue_supported = true;
    report.present_queue_supported = true;
    report.surface_capabilities_status = "success";
    report.surface_format_count = 2U;
    report.present_mode_count = 1U;
    report.selected_surface_format = "B8G8R8A8_SRGB / SRGB_NONLINEAR";
    report.selected_present_mode = "FIFO";
    report.selected_image_usage = "COLOR_ATTACHMENT";
    report.extent_width = 1080U;
    report.extent_height = 2400U;
    report.swapchain_create_status = "success";
    report.swapchain_image_query_status = "success";
    report.swapchain_image_count = 3U;
    report.semaphore_create_status = "success";
    report.present_semaphore_create_status = "success";
    report.image_acquire_status = "success";
    report.command_pool_create_status = "success";
    report.command_buffer_allocate_status = "success";
    report.command_record_status = "success";
    report.image_layout_transition_recorded = true;
    report.queue_submit_status = "success";
    report.image_acquired = true;
    report.image_present_status = "success";
    report.image_presented = true;
    report.queue_idle_status = "success";
    report.cleanup_status = "success";
    report.semaphore_destroy_status = "success";
    report.present_semaphore_destroy_status = "success";
    report.command_buffer_free_status = "success";
    report.command_pool_destroy_status = "success";
    report.swapchain_destroy_status = "success";
    report.device_destroy_status = "success";
    report.surface_destroy_status = "success";
    report.instance_destroy_status = "success";
    report.native_window_release_status = "success";

    const auto json = nyamist::discovery::toJson(report);
    check(json.find("\"target_gpu_match\": true") != std::string::npos,
          "target GPU match must be serialized");
    check(json.find("\"selected_present_mode\": \"FIFO\"") != std::string::npos,
          "FIFO presentation mode must be serialized");
    check(json.find("\"swapchain_image_count\": 3") != std::string::npos,
          "swapchain images must be counted");
    check(json.find("\"image_acquired\": true") != std::string::npos,
          "swapchain acquisition must be serialized");
    check(json.find("\"image_layout_transition_recorded\": true") != std::string::npos,
          "present layout transition must be serialized");
    check(json.find("\"queue_submit_status\": \"success\"") != std::string::npos,
          "queue submission between acquire and present must be serialized");
    check(json.find("\"present_semaphore_create_status\": \"success\"") != std::string::npos,
          "presentation wait semaphore must be serialized");
    check(json.find("\"image_presented\": true") != std::string::npos,
          "presentation must be serialized");
    check(json.find("\"frame_content_written\": false") != std::string::npos,
          "R1.5 must not claim rendering");
    check(json.find("\"native_window_release_status\": \"success\"") != std::string::npos,
          "native-window release must be serialized");
}

void testJsonEscaping() {
    nyamist::discovery::AndroidSurfaceSwapchainValidationReport report;
    report.failure_reason = "surface \"lost\"\\closed";
    const auto json = nyamist::discovery::toJson(report);
    check(json.find(R"(surface \"lost\"\\closed)") != std::string::npos,
          "JSON string fields must be escaped");
}

} // namespace

int main() {
    try {
        testUnavailableReportIsExplicit();
        testSuccessfulSwapchainReportContract();
        testJsonEscaping();
        std::cout << "All NYAMIST Android surface/swapchain validation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Android surface/swapchain test failure: " << error.what() << '\n';
        return 1;
    }
}
