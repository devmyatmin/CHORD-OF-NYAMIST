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
    check(json.find("\"schema_version\": 11") != std::string::npos,
          "schema version must be serialized");
    check(json.find("NYAMIST G925 R2.6") != std::string::npos,
          "R2.2 identity must be serialized");
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
    report.instance_api_version = "1.4.0";
    report.physical_device_properties2_query_status = "available";
    report.driver_properties_query_status = "success";
    report.driver_id = 9U;
    report.driver_id_name = "VK_DRIVER_ID_ARM_PROPRIETARY";
    report.driver_name = "Arm Proprietary";
    report.driver_info = "test driver build";
    report.driver_version_raw = 0x12345678U;
    report.driver_version_hex = "0x12345678";
    report.driver_conformance_version = "1.4.0.0";
    report.physical_device_uuid_hex = "00112233445566778899aabbccddeeff";
    report.vulkan_1_4_support_status = "not_exposed_by_physical_device";
    report.vulkan_1_4_features_query_status = "skipped_physical_device_api_below_1_4";
    report.vulkan_1_4_promoted_extension_inventory_status = "success";
    report.vulkan_1_4_promoted_extension_inventory_scope =
        "extension_name_advertisement_on_pre_1_4_device";
    report.vulkan_1_4_promoted_extension_coverage_status = "partial_advertisement";
    report.vulkan_1_4_promoted_extension_total_count = 16U;
    report.vulkan_1_4_promoted_extension_supported_count = 2U;
    report.vulkan_1_4_promoted_extension_not_advertised_count = 14U;
    report.vulkan_1_4_promoted_extensions_supported = {
        "VK_KHR_line_rasterization", "VK_EXT_host_image_copy"};
    report.vulkan_1_4_promoted_extensions_not_advertised = {
        "VK_KHR_dynamic_rendering_local_read", "VK_KHR_global_priority"};
    report.vulkan_1_4_promoted_feature_query_status = "success";
    report.vulkan_1_4_promoted_feature_query_scope =
        "advertised_extension_feature_structs_on_pre_1_4_device";
    report.vulkan_1_4_promoted_feature_total_count = 21U;
    report.vulkan_1_4_promoted_feature_queried_count = 2U;
    report.vulkan_1_4_promoted_feature_supported_count = 1U;
    report.vulkan_1_4_promoted_feature_not_supported_count = 1U;
    report.vulkan_1_4_promoted_feature_not_advertised_count = 1U;
    report.vulkan_1_4_promoted_feature_implicitly_supported_count = 1U;
    report.vulkan_1_4_promoted_feature_query_failed_count = 0U;
    report.vulkan_1_4_promoted_feature_not_queryable_count = 0U;
    report.vulkan_1_4_promoted_features_supported = {
        "VK_KHR_global_priority.globalPriorityQuery"};
    report.vulkan_1_4_promoted_features_not_supported = {
        "VK_KHR_line_rasterization.bresenhamLines"};
    report.vulkan_1_4_promoted_features_not_advertised = {
        "VK_EXT_host_image_copy.hostImageCopy"};
    report.vulkan_1_4_promoted_features_implicitly_supported = {
        "VK_KHR_push_descriptor.pushDescriptor (implicit via VK_KHR_push_descriptor)"};
    report.vulkan_1_4_promoted_features_not_applicable = {
        "VK_KHR_load_store_op_none (no feature bit)",
        "VK_KHR_map_memory2 (no feature bit)"};
    report.target_gpu_match = true;
    report.index_type_uint8_extension_status = "advertised";
    report.index_type_uint8_feature_support_status = "supported";
    report.index_type_uint8_feature_enable_status = "enabled";
    report.index_type_uint8_buffer_create_status = "success";
    report.index_type_uint8_memory_allocate_status = "success";
    report.index_type_uint8_memory_bind_status = "success";
    report.index_type_uint8_memory_map_status = "success";
    report.index_type_uint8_upload_status = "success";
    report.index_type_uint8_memory_unmap_status = "success";
    report.index_type_uint8_draw_entry_points_status = "success";
    report.index_type_uint8_draw_record_status = "success";
    report.index_type_uint8_draw_recorded = true;
    report.index_type_uint8_draw_index_count = 3U;
    report.index_type_uint8_probe_status = "success";
    report.index_type_uint8_buffer_destroy_status = "success";
    report.index_type_uint8_memory_free_status = "success";
    report.swapchain_extension_status = "success";
    report.queue_family_status = "success";
    report.graphics_queue_supported = true;
    report.present_queue_supported = true;
    report.surface_capabilities_status = "success";
    report.surface_format_count = 2U;
    report.present_mode_count = 1U;
    report.selected_surface_format = "B8G8R8A8_SRGB / SRGB_NONLINEAR";
    report.selected_present_mode = "FIFO";
    report.selected_image_usage = "COLOR_ATTACHMENT|TRANSFER_SRC";
    report.swapchain_transfer_src_support_status = "supported";
    report.pixel_readback_mode = "swapchain_transfer_src";
    report.pixel_readback_source = "acquired_swapchain_image";
    report.pixel_readback_buffer_create_status = "success";
    report.pixel_readback_memory_allocate_status = "success";
    report.pixel_readback_bind_status = "success";
    report.pixel_readback_memory_type = "HOST_VISIBLE|HOST_COHERENT";
    report.pixel_readback_allocation_size = 32U;
    report.pixel_readback_byte_count = 20U;
    report.pixel_readback_recorded = true;
    report.pixel_readback_copy_status = "success";
    report.pixel_readback_map_status = "success";
    report.pixel_readback_invalidate_status = "not_required_host_coherent";
    report.pixel_readback_unmap_status = "success";
    report.pixel_readback_sample_count = 5U;
    report.pixel_readback_sample_labels =
        "top_left,top_right,bottom_left,bottom_right,triangle_center";
    report.pixel_expected_clear_rgba8 = {53U, 105U, 153U, 255U};
    report.pixel_rgba8_samples = {
        53U,105U,153U,255U, 53U,105U,153U,255U,
        53U,105U,153U,255U, 53U,105U,153U,255U,
        173U,149U,158U,255U
    };
    report.pixel_background_samples_match_clear = true;
    report.pixel_triangle_center_detected = true;
    report.pixel_verification_status = "success";
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
    report.pixel_readback_buffer_destroy_status = "success";
    report.pixel_readback_memory_free_status = "success";
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
    report.present_fence_attach_count = 17U;
    report.present_fence_wait_count = 17U;
    report.present_fence_reset_count = 16U;
    report.present_fence_wait_cpu_us = 42U;
    report.present_fence_wait_cpu_us_samples = {40U, 42U, 45U};
    report.present_fence_destroy_status = "success";
    report.shutdown_presentation_completion_status = "present_fence_signaled";
    report.swapchain_recreation_status = "success";
    report.swapchain_recreation_attempt_count = 1U;
    report.swapchain_recreation_success_count = 1U;
    report.swapchain_old_handle_supplied = true;
    report.swapchain_old_destroy_status = "success";
    report.swapchain_recreated_image_query_status = "success";
    report.swapchain_recreated_image_count = 3U;
    report.swapchain_extent_before_width = 1080U;
    report.swapchain_extent_before_height = 2400U;
    report.swapchain_extent_after_width = 1080U;
    report.swapchain_extent_after_height = 2400U;
    report.swapchain_logical_device_reused = true;
    report.swapchain_recreation_probe_acquire_status = "success";
    report.swapchain_recreation_probe_submit_status = "success";
    report.swapchain_recreation_probe_present_status = "success";
    report.swapchain_recreation_probe_completion_status = "present_fence_signaled";
    report.swapchain_recreation_probe_present_count = 1U;
    report.swapchain_result_policy_status = "success";
    report.swapchain_out_of_date_recovery_status = "not_observed";

    const auto json = nyamist::discovery::toJson(report);
    check(json.find("\"schema_version\": 11") != std::string::npos,
          "R2.6 schema version must be serialized");
    check(json.find(R"("present_fence_used": true)") != std::string::npos,
          "present fence use must be serialized");
    check(json.find(R"("present_fence_attach_count": 17)") != std::string::npos,
          "all 17 present operations must carry a fence");
    check(json.find(R"("present_fence_wait_count": 17)") != std::string::npos,
          "all 17 present fences must be waited");
    check(json.find(R"("present_fence_reset_count": 16)") != std::string::npos,
          "only the first 16 present-fence reuse cycles are reset");
    check(json.find(R"("present_completion_guaranteed": true)") != std::string::npos,
          "presentation completion guarantee must be serialized");
    check(json.find(R"("present_completion_mode": "VK_EXT_swapchain_maintenance1")") != std::string::npos,
          "present completion mode must be serialized");
    check(json.find(R"("present_fence_wait_cpu_us_samples": [40, 42, 45])") != std::string::npos,
          "present fence wait samples must be serialized");
    check(json.find(R"("shutdown_presentation_completion_status": "present_fence_signaled")") != std::string::npos,
          "shutdown presentation completion must be serialized");
    check(json.find("\"target_gpu_match\": true") != std::string::npos,
          "G925 target match must be serialized");
    check(json.find(R"("index_type_uint8_feature_enable_status": "enabled")") != std::string::npos,
          "uint8 index feature enablement must be serialized");
    check(json.find(R"("index_type_uint8_draw_recorded": true)") != std::string::npos,
          "uint8 indexed draw recording must be serialized");
    check(json.find(R"("index_type_uint8_draw_index_count": 3)") != std::string::npos,
          "uint8 indexed draw count must be serialized");
    check(json.find(R"("index_type_uint8_probe_status": "success")") != std::string::npos,
          "uint8 index functional probe status must be serialized");
    check(json.find(R"("driver_id": 9)") != std::string::npos,
          "Vulkan driver ID must be serialized");
    check(json.find(R"("driver_id_name": "VK_DRIVER_ID_ARM_PROPRIETARY")") != std::string::npos,
          "Arm driver ID must be named");
    check(json.find(R"("driver_version_hex": "0x12345678")") != std::string::npos,
          "raw Vulkan driver version must be preserved");
    check(json.find(R"("physical_device_uuid": "00112233445566778899aabbccddeeff")") != std::string::npos,
          "physical-device UUID must be serialized");
    check(json.find(R"("vulkan_1_4_support_status": "not_exposed_by_physical_device")") != std::string::npos,
          "a Vulkan 1.3 device must not be misreported as Vulkan 1.4");
    check(json.find(R"("vulkan_1_4_promoted_extension_inventory_status": "success")") != std::string::npos,
          "promoted extension inventory status must be serialized");
    check(json.find(R"("vulkan_1_4_promoted_extension_total_count": 16)") != std::string::npos,
          "all 16 promoted extension names must be counted");
    check(json.find(R"("vulkan_1_4_promoted_extension_supported_count": 2)") != std::string::npos,
          "advertised extension count must be serialized");
    check(json.find(R"("vulkan_1_4_promoted_extensions_supported": ["VK_KHR_line_rasterization", "VK_EXT_host_image_copy"])") != std::string::npos,
          "advertised extension names must be serialized");
    check(json.find(R"("vulkan_1_4_promoted_extensions_not_advertised": ["VK_KHR_dynamic_rendering_local_read", "VK_KHR_global_priority"])") != std::string::npos,
          "not-advertised extension names must be serialized");
    check(json.find(R"("vulkan_1_4_promoted_feature_total_count": 21)") != std::string::npos,
          "R2.5 retained feature-bit count must be serialized");
    check(json.find(R"("vulkan_1_4_promoted_feature_query_status": "success")") != std::string::npos,
          "R2.5 retained feature query status must be serialized");
    check(json.find(R"("vulkan_1_4_promoted_features_supported": ["VK_KHR_global_priority.globalPriorityQuery"])") != std::string::npos,
          "supported feature bits must be serialized");
    check(json.find(R"("vulkan_1_4_promoted_features_not_supported": ["VK_KHR_line_rasterization.bresenhamLines"])") != std::string::npos,
          "unsupported feature bits must be serialized");
    check(json.find(R"json("vulkan_1_4_promoted_features_implicitly_supported": ["VK_KHR_push_descriptor.pushDescriptor (implicit via VK_KHR_push_descriptor)"])json") != std::string::npos,
          "pre-1.4 push descriptors must be reported as implicitly supported when the extension is advertised");
    check(json.find(R"json("vulkan_1_4_promoted_features_not_applicable": ["VK_KHR_load_store_op_none (no feature bit)", "VK_KHR_map_memory2 (no feature bit)"])json") != std::string::npos,
          "promoted extensions without feature bits must be explicit");
    check(json.find("\"selected_present_mode\": \"FIFO\"") != std::string::npos,
          "FIFO presentation mode must be serialized");
    check(json.find(R"("swapchain_recreation_status": "success")") != std::string::npos,
          "oldSwapchain recreation status must be serialized");
    check(json.find(R"("swapchain_old_handle_supplied": true)") != std::string::npos,
          "oldSwapchain handoff must be serialized");
    check(json.find(R"("swapchain_logical_device_reused": true)") != std::string::npos,
          "logical device reuse must be serialized");
    check(json.find(R"("swapchain_recreation_probe_present_status": "success")") != std::string::npos,
          "replacement swapchain presentation must be serialized");
    check(json.find(R"("swapchain_out_of_date_recovery_status": "not_observed")") != std::string::npos,
          "lack of an actual OUT_OF_DATE event must be explicit");
    check(json.find(R"("swapchain_transfer_src_support_status": "supported")") != std::string::npos,
          "swapchain transfer-source support must be serialized");
    check(json.find(R"("pixel_readback_mode": "swapchain_transfer_src")") != std::string::npos,
          "pixel readback mode must be serialized");
    check(json.find(R"("pixel_readback_byte_count": 20)") != std::string::npos,
          "pixel readback byte count must be serialized");
    check(json.find(R"("pixel_readback_sample_count": 5)") != std::string::npos,
          "five sample locations must be serialized");
    check(json.find(R"("pixel_background_samples_match_clear": true)") != std::string::npos,
          "clear-color background verification must be serialized");
    check(json.find(R"("pixel_triangle_center_detected": true)") != std::string::npos,
          "triangle center verification must be serialized");
    check(json.find(R"("pixel_verification_status": "success")") != std::string::npos,
          "pixel verification result must be serialized");

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
    report.schema_version = 7U;
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
          "R2.0 frame target must be serialized");
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

void testFallbackDoesNotClaimPresentationCompletion() {
    nyamist::discovery::AndroidSurfaceSwapchainValidationReport report;
    report.schema_version = 7U;
    report.present_fence_used = false;
    report.present_completion_guaranteed = false;
    report.present_completion_mode = "per_image_reacquire_fallback";
    report.present_fence_entry_points_status = "not_supported";
    report.present_fence_create_status = "not_supported";
    report.present_fence_attach_status = "not_supported";
    report.present_fence_wait_status = "not_supported";
    report.present_fence_reset_status = "not_required";
    report.shutdown_presentation_completion_status =
        "not_guaranteed_without_present_fence";

    const auto json = nyamist::discovery::toJson(report);
    check(json.find(R"("present_fence_used": false)") != std::string::npos,
          "fallback must not claim a present fence was used");
    check(json.find(R"("present_completion_guaranteed": false)") != std::string::npos,
          "fallback must not claim formal completion guarantee");
    check(json.find(R"("present_completion_mode": "per_image_reacquire_fallback")") != std::string::npos,
          "fallback mode must be explicit");
    check(json.find(R"("shutdown_presentation_completion_status": "not_guaranteed_without_present_fence")") != std::string::npos,
          "fallback shutdown limitation must be explicit");
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
        testFallbackDoesNotClaimPresentationCompletion();
        testJsonEscaping();
        std::cout << "All NYAMIST Android surface/swapchain validation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Android surface/swapchain test failure: " << error.what() << '\n';
        return 1;
    }
}
