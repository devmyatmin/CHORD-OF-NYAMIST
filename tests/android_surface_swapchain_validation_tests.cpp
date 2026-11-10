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
    check(json.find("\"schema_version\": 5") != std::string::npos,
          "schema version must be serialized");
    check(json.find("NYAMIST G925 R1.9") != std::string::npos,
          "R1.7 identity must be serialized");
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
    report.image_view_create_status = "success";
    report.render_pass_create_status = "success";
    report.framebuffer_create_status = "success";
    report.render_pass_begin_recorded = true;
    report.render_pass_end_recorded = true;
    report.vertex_shader_module_create_status = "success";
    report.fragment_shader_module_create_status = "success";
    report.pipeline_layout_create_status = "success";
    report.graphics_pipeline_create_status = "success";
    report.graphics_pipeline_bind_recorded = true;
    report.draw_call_recorded = true;
    report.draw_vertex_count = 3U;
    report.frame_content_written = true;
    report.frame_timing_source = "std::chrono::steady_clock";
    report.acquire_cpu_us = 100U;
    report.queue_submit_cpu_us = 200U;
    report.present_call_cpu_us = 300U;
    report.queue_idle_wait_cpu_us = 400U;
    report.frame_total_cpu_us = 1000U;
    report.frame_timing_sample_count = 1U;
    report.frame_timing_valid = true;
    report.gpu_timestamp_status = "available_not_queried";
    report.queue_timestamp_valid_bits = 36U;
    report.timestamp_compute_and_graphics_supported = true;
    report.timestamp_period_ns = 1.0;
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
    report.image_view_destroy_status = "success";
    report.render_pass_destroy_status = "success";
    report.framebuffer_destroy_status = "success";
    report.graphics_pipeline_destroy_status = "success";
    report.pipeline_layout_destroy_status = "success";
    report.vertex_shader_module_destroy_status = "success";
    report.fragment_shader_module_destroy_status = "success";
    report.swapchain_destroy_status = "success";
    report.device_destroy_status = "success";
    report.surface_destroy_status = "success";
    report.instance_destroy_status = "success";
    report.native_window_release_status = "success";

    report.loader_api_version = "1.1";
    report.swapchain_maintenance_instance_extension_status = "enabled";
    report.swapchain_maintenance_device_extension_status = "supported";
    report.swapchain_maintenance_feature_status = "enabled";
    report.swapchain_maintenance_feature_supported = true;
    report.present_fence_used = true;
    report.present_completion_guaranteed = true;
    report.present_completion_mode = "VK_EXT_swapchain_maintenance1";
    report.present_fence_entry_points_status = "success";
    report.present_fence_create_status = "success";
    report.present_fence_attach_status = "success";
    report.present_fence_wait_status = "success";
    report.present_fence_reset_status = "success";
    report.present_fence_wait_cpu_us = 42U;
    report.present_fence_wait_cpu_us_samples = {40U, 42U, 45U};
    report.present_fence_destroy_status = "success";
    report.shutdown_presentation_completion_status = "present_fence_signaled";

    const auto json = nyamist::discovery::toJson(report);
    check(json.find("\"schema_version\": 5") != std::string::npos,
          "R1.9 schema version must be serialized");
    check(json.find(R"("present_fence_used": true)") != std::string::npos,
          "present fence use must be serialized");
    check(json.find(R"("present_completion_guaranteed": true)") != std::string::npos,
          "presentation completion guarantee must be serialized");
    check(json.find(R"("present_completion_mode": "VK_EXT_swapchain_maintenance1")") != std::string::npos,
          "present completion mode must be serialized");
    check(json.find(R"("present_fence_wait_cpu_us_samples": [40, 42, 45])") != std::string::npos,
          "present fence wait samples must be serialized");
    check(json.find(R"("shutdown_presentation_completion_status": "present_fence_signaled")") != std::string::npos,
          "shutdown presentation completion must be serialized");
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
    check(json.find(R"("frame_content_written": true)") != std::string::npos,
          "R1.7 must report swapchain image writes");
    check(json.find(R"("graphics_pipeline_create_status": "success")") != std::string::npos,
          "graphics pipeline creation must be serialized");
    check(json.find(R"("draw_call_recorded": true)") != std::string::npos,
          "vkCmdDraw recording must be serialized");
    check(json.find(R"("draw_vertex_count": 3)") != std::string::npos,
          "the first draw must be a three-vertex triangle");
    check(json.find(R"("gpu_timestamp_status": "available_not_queried")") != std::string::npos,
          "timestamp capability must not be mistaken for a timing measurement");
    check(json.find(R"("render_pass_begin_recorded": true)") != std::string::npos,
          "render pass begin must be serialized");
    check(json.find(R"("frame_timing_valid": true)") != std::string::npos,
          "CPU frame timing sample must be serialized");
    check(json.find(R"("frame_total_cpu_us": 1000)") != std::string::npos,
          "frame CPU duration must be serialized");
    check(json.find(R"("gpu_timestamp_status": "available_not_queried")") != std::string::npos,
          "timestamp capability must not be misrepresented as a GPU timing measurement");
    check(json.find("\"present_semaphore_destroy_status\": \"success\"") != std::string::npos,
          "present semaphore cleanup must be serialized");
    check(json.find("\"command_buffer_free_status\": \"success\"") != std::string::npos,
          "command buffer cleanup must be serialized");
    check(json.find("\"command_pool_destroy_status\": \"success\"") != std::string::npos,
          "command pool cleanup must be serialized");
    check(json.find("\"native_window_release_status\": \"success\"") != std::string::npos,
          "native-window release must be serialized");
}

void testMultiFrameTimingSerialization() {
    nyamist::discovery::AndroidSurfaceSwapchainValidationReport report;
    report.schema_version = 5U;
    report.frame_target_count = 16U;
    report.frame_completed_count = 17U;
    report.warmup_frame_count = 1U;
    report.frame_timing_sample_count = 4U;
    report.frame_timing_valid = true;
    report.frame_total_cpu_us_samples = {100U, 200U, 300U, 400U};
    report.gpu_timestamp_status = "success";
    report.gpu_timestamp_query_pool_create_status = "success";
    report.gpu_timestamp_query_status = "success";
    report.gpu_timestamp_sample_count = 4U;
    report.gpu_frame_time_ns_samples = {800000U, 600000U, 700000U, 900000U};
    const auto json = nyamist::discovery::toJson(report);
    check(json.find(R"("frame_target_count": 16)") != std::string::npos,
          "R1.8 frame target must be serialized");
    check(json.find(R"("frame_completed_count": 17)") != std::string::npos,
          "warm-up plus sample count must be serialized");
    check(json.find(R"("frame_total_cpu_us_samples": [100, 200, 300, 400])") != std::string::npos,
          "all CPU samples must be serialized");
    check(json.find(R"("cpu_frame_total_min_us": 100)") != std::string::npos,
          "CPU minimum must be serialized");
    check(json.find(R"("cpu_frame_total_median_us": 250)") != std::string::npos,
          "CPU median must be serialized");
    check(json.find(R"("cpu_frame_total_p95_us": 400)") != std::string::npos,
          "CPU p95 must be serialized");
    check(json.find(R"("cpu_frame_total_max_us": 400)") != std::string::npos,
          "CPU maximum must be serialized");
    check(json.find(R"("gpu_timestamp_query_status": "success")") != std::string::npos,
          "GPU query status must be serialized");
    check(json.find(R"("gpu_frame_time_ns_samples": [800000, 600000, 700000, 900000])") != std::string::npos,
          "GPU samples must be serialized");
    check(json.find(R"("gpu_frame_time_median_ns": 750000)") != std::string::npos,
          "GPU median must be serialized");
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
        testMultiFrameTimingSerialization();
        testJsonEscaping();
        std::cout << "All NYAMIST Android surface/swapchain validation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Android surface/swapchain test failure: " << error.what() << '\n';
        return 1;
    }
}
