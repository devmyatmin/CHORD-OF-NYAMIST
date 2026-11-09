#pragma once

#include <cstdint>
#include <string>

namespace nyamist::discovery {

struct AndroidSurfaceSwapchainValidationReport {
    std::uint32_t schema_version{1U};
    std::string status{"unavailable"};
    std::string platform{"android"};
    std::string loader_library;
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
    bool image_layout_transition_recorded{};
    std::string queue_submit_status{"not_attempted"};
    bool image_acquired{};
    std::uint32_t acquired_image_index{};
    std::string image_present_status{"not_attempted"};
    bool image_presented{};
    std::string queue_idle_status{"not_attempted"};
    bool frame_content_written{};

    std::string cleanup_status{"not_attempted"};
    std::string semaphore_destroy_status{"not_required"};
    std::string present_semaphore_destroy_status{"not_required"};
    std::string command_buffer_free_status{"not_required"};
    std::string command_pool_destroy_status{"not_required"};
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
