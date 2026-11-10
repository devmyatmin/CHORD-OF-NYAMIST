#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nyamist::discovery {

struct AndroidSurfaceSwapchainValidationReport {
    std::uint32_t schema_version{6U};
    std::string status{"unavailable"};
    std::string platform{"android"};
    std::string loader_library;
    std::string loader_api_version;
    std::string loader_status{"not_attempted"};
    std::string instance_create_status{"not_attempted"};
    std::string surface_extension_status{"not_attempted"};
    std::string android_surface_extension_status{"not_attempted"};
    std::string native_window_status{"not_attempted"};
    std::string surface_create_status{"not_attempted"};

    std::string physical_device_status{"not_attempted"};
    std::string device_name;
    std::string vendor_id_hex;
    std::string device_id_hex;
    std::string physical_device_api_version;
    bool target_gpu_match{};

    std::string swapchain_maintenance_instance_extension_status{"not_queried"};
    std::string swapchain_maintenance_device_extension_status{"not_queried"};
    std::string swapchain_maintenance_feature_status{"not_queried"};
    bool swapchain_maintenance_feature_supported{};
    bool present_fence_used{};
    bool present_completion_guaranteed{};
    std::string present_completion_mode{"unavailable"};
    std::string present_fence_entry_points_status{"not_attempted"};
    std::string present_fence_create_status{"not_attempted"};
    std::string present_fence_attach_status{"not_attempted"};
    std::string present_fence_wait_status{"not_attempted"};
    std::string present_fence_reset_status{"not_attempted"};
    std::uint32_t present_fence_attach_count{};
    std::uint32_t present_fence_wait_count{};
    std::uint32_t present_fence_reset_count{};
    std::string present_fence_destroy_status{"not_required"};
    std::uint64_t present_fence_wait_cpu_us{};
    std::vector<std::uint64_t> present_fence_wait_cpu_us_samples;
    std::string shutdown_presentation_completion_status{"not_attempted"};

    std::string swapchain_extension_status{"not_attempted"};
    std::string queue_family_status{"not_attempted"};
    std::uint32_t queue_family_index{};
    bool graphics_queue_supported{};
    bool present_queue_supported{};

    std::string surface_capabilities_status{"not_attempted"};
    std::uint32_t surface_format_count{};
    std::uint32_t present_mode_count{};
    std::string selected_surface_format;
    std::string selected_present_mode;
    std::string selected_image_usage;
    std::string swapchain_transfer_src_support_status{"not_queried"};
    std::string pixel_readback_mode{"not_attempted"};
    std::string pixel_readback_source{"not_attempted"};
    std::string pixel_readback_buffer_create_status{"not_attempted"};
    std::string pixel_readback_memory_allocate_status{"not_attempted"};
    std::string pixel_readback_bind_status{"not_attempted"};
    std::string pixel_readback_memory_type{"not_selected"};
    std::uint64_t pixel_readback_allocation_size{};
    std::uint64_t pixel_readback_byte_count{};
    bool pixel_readback_recorded{};
    std::string pixel_readback_copy_status{"not_attempted"};
    std::string pixel_readback_map_status{"not_attempted"};
    std::string pixel_readback_invalidate_status{"not_required"};
    std::string pixel_readback_unmap_status{"not_attempted"};
    std::uint32_t pixel_readback_sample_count{};
    std::string pixel_readback_sample_labels;
    std::vector<std::uint64_t> pixel_expected_clear_rgba8;
    std::vector<std::uint64_t> pixel_rgba8_samples;
    bool pixel_background_samples_match_clear{};
    bool pixel_triangle_center_detected{};
    std::string pixel_verification_status{"not_attempted"};
    std::uint32_t extent_width{};
    std::uint32_t extent_height{};

    std::string swapchain_create_status{"not_attempted"};
    std::string swapchain_image_query_status{"not_attempted"};
    std::uint32_t swapchain_image_count{};
    std::string semaphore_create_status{"not_attempted"};
    std::string present_semaphore_create_status{"not_attempted"};
    std::string image_acquire_status{"not_attempted"};
    std::string command_pool_create_status{"not_attempted"};
    std::string command_buffer_allocate_status{"not_attempted"};
    std::string command_record_status{"not_attempted"};
    std::string image_view_create_status{"not_attempted"};
    std::string render_pass_create_status{"not_attempted"};
    std::string framebuffer_create_status{"not_attempted"};
    std::string vertex_shader_module_create_status{"not_attempted"};
    std::string fragment_shader_module_create_status{"not_attempted"};
    std::string pipeline_layout_create_status{"not_attempted"};
    std::string graphics_pipeline_create_status{"not_attempted"};
    bool image_layout_transition_recorded{};
    std::string queue_submit_status{"not_attempted"};
    bool image_acquired{};
    std::uint32_t acquired_image_index{};
    std::string image_present_status{"not_attempted"};
    bool image_presented{};
    std::string queue_idle_status{"not_attempted"};
    bool frame_content_written{};
    bool render_pass_begin_recorded{};
    bool render_pass_end_recorded{};
    bool graphics_pipeline_bind_recorded{};
    bool draw_call_recorded{};
    std::uint32_t draw_vertex_count{};
    std::string frame_clear_color_rgba{"0.035,0.14,0.32,1.0"};
    std::string frame_timing_source;
    std::uint64_t acquire_cpu_us{};
    std::uint64_t queue_submit_cpu_us{};
    std::uint64_t present_call_cpu_us{};
    std::uint64_t queue_idle_wait_cpu_us{};
    std::uint64_t frame_total_cpu_us{};
    std::uint32_t frame_timing_sample_count{};
    bool frame_timing_valid{};
    std::uint32_t frame_target_count{16U};
    std::uint32_t frame_completed_count{};
    std::uint32_t warmup_frame_count{1U};
    std::vector<std::uint64_t> acquire_cpu_us_samples;
    std::vector<std::uint64_t> queue_submit_cpu_us_samples;
    std::vector<std::uint64_t> present_call_cpu_us_samples;
    std::vector<std::uint64_t> queue_idle_wait_cpu_us_samples;
    std::vector<std::uint64_t> frame_total_cpu_us_samples;
    std::string gpu_timestamp_status{"not_queried"};
    std::string gpu_timestamp_query_pool_create_status{"not_attempted"};
    std::string gpu_timestamp_query_status{"not_queried"};
    std::string gpu_timestamp_query_pool_destroy_status{"not_required"};
    std::uint32_t gpu_timestamp_sample_count{};
    std::vector<std::uint64_t> gpu_frame_time_ns_samples;
    std::uint32_t queue_timestamp_valid_bits{};
    bool timestamp_compute_and_graphics_supported{};
    double timestamp_period_ns{};

    std::string cleanup_status{"not_attempted"};
    std::string pixel_readback_buffer_destroy_status{"not_required"};
    std::string pixel_readback_memory_free_status{"not_required"};
    std::string semaphore_destroy_status{"not_required"};
    std::string present_semaphore_destroy_status{"not_required"};
    std::string command_buffer_free_status{"not_required"};
    std::string command_pool_destroy_status{"not_required"};
    std::string image_view_destroy_status{"not_required"};
    std::string render_pass_destroy_status{"not_required"};
    std::string framebuffer_destroy_status{"not_required"};
    std::string graphics_pipeline_destroy_status{"not_required"};
    std::string pipeline_layout_destroy_status{"not_required"};
    std::string vertex_shader_module_destroy_status{"not_required"};
    std::string fragment_shader_module_destroy_status{"not_required"};
    std::string swapchain_destroy_status{"not_required"};
    std::string device_destroy_status{"not_required"};
    std::string surface_destroy_status{"not_required"};
    std::string instance_destroy_status{"not_required"};
    std::string native_window_release_status{"not_required"};
    std::string failure_reason;
};

[[nodiscard]] std::string
toJson(const AndroidSurfaceSwapchainValidationReport& report);

} // namespace nyamist::discovery
