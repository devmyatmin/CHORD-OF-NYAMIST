#include "nyamist/discovery/android_surface_swapchain_validation.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace nyamist::discovery {
namespace {

std::string escapeJson(std::string_view value) {
    std::ostringstream out;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20U) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch) << std::dec;
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    return out.str();
}

void stringField(std::ostringstream& out, const char* name,
                 const std::string& value, bool comma = true) {
    out << "  \"" << name << "\": \"" << escapeJson(value) << "\"";
    if (comma) out << ',';
    out << '\n';
}

void boolField(std::ostringstream& out, const char* name, bool value,
               bool comma = true) {
    out << "  \"" << name << "\": " << (value ? "true" : "false");
    if (comma) out << ',';
    out << '\n';
}

void numberField(std::ostringstream& out, const char* name, std::uint32_t value,
                 bool comma = true) {
    out << "  \"" << name << "\": " << value;
    if (comma) out << ',';
    out << '\n';
}


void number64Field(std::ostringstream& out, const char* name, std::uint64_t value,
                   bool comma = true) {
    out << "  \"" << name << "\": " << value;
    if (comma) out << ',';
    out << '\n';
}

void doubleField(std::ostringstream& out, const char* name, double value,
                 bool comma = true) {
    out << "  \"" << name << "\": " << std::setprecision(10) << value;
    if (comma) out << ',';
    out << '\n';
}

void array64Field(std::ostringstream& out, const char* name,
                  const std::vector<std::uint64_t>& values) {
    out << "  \"" << name << "\": [";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0U) out << ", ";
        out << values[index];
    }
    out << "],\n";
}

void summary64Fields(std::ostringstream& out, const char* prefix,
                     const std::vector<std::uint64_t>& values,
                     const char* unit_suffix) {
    if (values.empty()) return;
    std::vector<std::uint64_t> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t middle = sorted.size() / 2U;
    const std::uint64_t median = (sorted.size() % 2U) != 0U
        ? sorted[middle]
        : static_cast<std::uint64_t>(
              (static_cast<long double>(sorted[middle - 1U]) +
               static_cast<long double>(sorted[middle])) / 2.0L);
    const std::size_t p95_index =
        std::min(sorted.size() - 1U,
                 static_cast<std::size_t>(std::ceil(0.95 * sorted.size())) - 1U);
    const std::string base(prefix);
    number64Field(out, (base + "_min" + unit_suffix).c_str(), sorted.front());
    number64Field(out, (base + "_median" + unit_suffix).c_str(), median);
    number64Field(out, (base + "_p95" + unit_suffix).c_str(), sorted[p95_index]);
    number64Field(out, (base + "_max" + unit_suffix).c_str(), sorted.back());
}

} // namespace

std::string toJson(const AndroidSurfaceSwapchainValidationReport& report) {
    std::ostringstream out;
    out << "{\n";
    numberField(out, "schema_version", report.schema_version);
    stringField(out, "project", "CHORD OF NYAMIST / NYAMIST G925 R2.0");
    stringField(out, "status", report.status);
    stringField(out, "platform", report.platform);
    stringField(out, "loader_library", report.loader_library);
    stringField(out, "loader_api_version", report.loader_api_version);
    stringField(out, "loader_status", report.loader_status);
    stringField(out, "instance_create_status", report.instance_create_status);
    stringField(out, "surface_extension_status", report.surface_extension_status);
    stringField(out, "android_surface_extension_status", report.android_surface_extension_status);
    stringField(out, "native_window_status", report.native_window_status);
    stringField(out, "surface_create_status", report.surface_create_status);
    stringField(out, "physical_device_status", report.physical_device_status);
    stringField(out, "device_name", report.device_name);
    stringField(out, "vendor_id", report.vendor_id_hex);
    stringField(out, "device_id", report.device_id_hex);
    stringField(out, "physical_device_api_version", report.physical_device_api_version);
    boolField(out, "target_gpu_match", report.target_gpu_match);
    stringField(out, "swapchain_maintenance_instance_extension_status", report.swapchain_maintenance_instance_extension_status);
    stringField(out, "swapchain_maintenance_device_extension_status", report.swapchain_maintenance_device_extension_status);
    stringField(out, "swapchain_maintenance_feature_status", report.swapchain_maintenance_feature_status);
    boolField(out, "swapchain_maintenance_feature_supported", report.swapchain_maintenance_feature_supported);
    boolField(out, "present_fence_used", report.present_fence_used);
    boolField(out, "present_completion_guaranteed", report.present_completion_guaranteed);
    stringField(out, "present_completion_mode", report.present_completion_mode);
    stringField(out, "present_fence_entry_points_status", report.present_fence_entry_points_status);
    stringField(out, "present_fence_create_status", report.present_fence_create_status);
    stringField(out, "present_fence_attach_status", report.present_fence_attach_status);
    stringField(out, "present_fence_wait_status", report.present_fence_wait_status);
    stringField(out, "present_fence_reset_status", report.present_fence_reset_status);
    numberField(out, "present_fence_attach_count", report.present_fence_attach_count);
    numberField(out, "present_fence_wait_count", report.present_fence_wait_count);
    numberField(out, "present_fence_reset_count", report.present_fence_reset_count);
    number64Field(out, "present_fence_wait_cpu_us", report.present_fence_wait_cpu_us);
    array64Field(out, "present_fence_wait_cpu_us_samples", report.present_fence_wait_cpu_us_samples);
    stringField(out, "present_fence_destroy_status", report.present_fence_destroy_status);
    stringField(out, "shutdown_presentation_completion_status", report.shutdown_presentation_completion_status);
    stringField(out, "swapchain_extension_status", report.swapchain_extension_status);
    stringField(out, "queue_family_status", report.queue_family_status);
    numberField(out, "queue_family_index", report.queue_family_index);
    boolField(out, "graphics_queue_supported", report.graphics_queue_supported);
    boolField(out, "present_queue_supported", report.present_queue_supported);
    stringField(out, "surface_capabilities_status", report.surface_capabilities_status);
    numberField(out, "surface_format_count", report.surface_format_count);
    numberField(out, "present_mode_count", report.present_mode_count);
    stringField(out, "selected_surface_format", report.selected_surface_format);
    stringField(out, "selected_present_mode", report.selected_present_mode);
    stringField(out, "selected_image_usage", report.selected_image_usage);
    stringField(out, "swapchain_transfer_src_support_status", report.swapchain_transfer_src_support_status);
    stringField(out, "pixel_readback_mode", report.pixel_readback_mode);
    stringField(out, "pixel_readback_source", report.pixel_readback_source);
    stringField(out, "pixel_readback_buffer_create_status", report.pixel_readback_buffer_create_status);
    stringField(out, "pixel_readback_memory_allocate_status", report.pixel_readback_memory_allocate_status);
    stringField(out, "pixel_readback_bind_status", report.pixel_readback_bind_status);
    stringField(out, "pixel_readback_memory_type", report.pixel_readback_memory_type);
    number64Field(out, "pixel_readback_allocation_size", report.pixel_readback_allocation_size);
    number64Field(out, "pixel_readback_byte_count", report.pixel_readback_byte_count);
    boolField(out, "pixel_readback_recorded", report.pixel_readback_recorded);
    stringField(out, "pixel_readback_copy_status", report.pixel_readback_copy_status);
    stringField(out, "pixel_readback_map_status", report.pixel_readback_map_status);
    stringField(out, "pixel_readback_invalidate_status", report.pixel_readback_invalidate_status);
    stringField(out, "pixel_readback_unmap_status", report.pixel_readback_unmap_status);
    numberField(out, "pixel_readback_sample_count", report.pixel_readback_sample_count);
    stringField(out, "pixel_readback_sample_labels", report.pixel_readback_sample_labels);
    array64Field(out, "pixel_expected_clear_rgba8", report.pixel_expected_clear_rgba8);
    array64Field(out, "pixel_rgba8_samples", report.pixel_rgba8_samples);
    boolField(out, "pixel_background_samples_match_clear", report.pixel_background_samples_match_clear);
    boolField(out, "pixel_triangle_center_detected", report.pixel_triangle_center_detected);
    stringField(out, "pixel_verification_status", report.pixel_verification_status);
    numberField(out, "extent_width", report.extent_width);
    numberField(out, "extent_height", report.extent_height);
    stringField(out, "swapchain_create_status", report.swapchain_create_status);
    stringField(out, "swapchain_image_query_status", report.swapchain_image_query_status);
    numberField(out, "swapchain_image_count", report.swapchain_image_count);
    stringField(out, "semaphore_create_status", report.semaphore_create_status);
    stringField(out, "present_semaphore_create_status", report.present_semaphore_create_status);
    stringField(out, "image_acquire_status", report.image_acquire_status);
    stringField(out, "command_pool_create_status", report.command_pool_create_status);
    stringField(out, "command_buffer_allocate_status", report.command_buffer_allocate_status);
    stringField(out, "command_record_status", report.command_record_status);
    boolField(out, "image_layout_transition_recorded", report.image_layout_transition_recorded);
    stringField(out, "queue_submit_status", report.queue_submit_status);
    boolField(out, "image_acquired", report.image_acquired);
    numberField(out, "acquired_image_index", report.acquired_image_index);
    stringField(out, "image_present_status", report.image_present_status);
    boolField(out, "image_presented", report.image_presented);
    stringField(out, "queue_idle_status", report.queue_idle_status);
    boolField(out, "frame_content_written", report.frame_content_written);
    stringField(out, "image_view_create_status", report.image_view_create_status);
    stringField(out, "render_pass_create_status", report.render_pass_create_status);
    stringField(out, "framebuffer_create_status", report.framebuffer_create_status);
    boolField(out, "render_pass_begin_recorded", report.render_pass_begin_recorded);
    boolField(out, "render_pass_end_recorded", report.render_pass_end_recorded);
    stringField(out, "vertex_shader_module_create_status", report.vertex_shader_module_create_status);
    stringField(out, "fragment_shader_module_create_status", report.fragment_shader_module_create_status);
    stringField(out, "pipeline_layout_create_status", report.pipeline_layout_create_status);
    stringField(out, "graphics_pipeline_create_status", report.graphics_pipeline_create_status);
    boolField(out, "graphics_pipeline_bind_recorded", report.graphics_pipeline_bind_recorded);
    boolField(out, "draw_call_recorded", report.draw_call_recorded);
    numberField(out, "draw_vertex_count", report.draw_vertex_count);
    stringField(out, "frame_clear_color_rgba", report.frame_clear_color_rgba);
    stringField(out, "frame_timing_source", report.frame_timing_source);
    number64Field(out, "acquire_cpu_us", report.acquire_cpu_us);
    number64Field(out, "queue_submit_cpu_us", report.queue_submit_cpu_us);
    number64Field(out, "present_call_cpu_us", report.present_call_cpu_us);
    number64Field(out, "queue_idle_wait_cpu_us", report.queue_idle_wait_cpu_us);
    number64Field(out, "frame_total_cpu_us", report.frame_total_cpu_us);
    numberField(out, "frame_timing_sample_count", report.frame_timing_sample_count);
    boolField(out, "frame_timing_valid", report.frame_timing_valid);
    numberField(out, "frame_target_count", report.frame_target_count);
    numberField(out, "frame_completed_count", report.frame_completed_count);
    numberField(out, "warmup_frame_count", report.warmup_frame_count);
    array64Field(out, "acquire_cpu_us_samples", report.acquire_cpu_us_samples);
    array64Field(out, "queue_submit_cpu_us_samples", report.queue_submit_cpu_us_samples);
    array64Field(out, "present_call_cpu_us_samples", report.present_call_cpu_us_samples);
    array64Field(out, "queue_idle_wait_cpu_us_samples", report.queue_idle_wait_cpu_us_samples);
    array64Field(out, "frame_total_cpu_us_samples", report.frame_total_cpu_us_samples);
    summary64Fields(out, "cpu_frame_total", report.frame_total_cpu_us_samples, "_us");
    stringField(out, "gpu_timestamp_status", report.gpu_timestamp_status);
    stringField(out, "gpu_timestamp_query_pool_create_status",
                report.gpu_timestamp_query_pool_create_status);
    stringField(out, "gpu_timestamp_query_status", report.gpu_timestamp_query_status);
    stringField(out, "gpu_timestamp_query_pool_destroy_status",
                report.gpu_timestamp_query_pool_destroy_status);
    numberField(out, "gpu_timestamp_sample_count", report.gpu_timestamp_sample_count);
    array64Field(out, "gpu_frame_time_ns_samples", report.gpu_frame_time_ns_samples);
    summary64Fields(out, "gpu_frame_time", report.gpu_frame_time_ns_samples, "_ns");
    numberField(out, "queue_timestamp_valid_bits", report.queue_timestamp_valid_bits);
    boolField(out, "timestamp_compute_and_graphics_supported",
              report.timestamp_compute_and_graphics_supported);
    doubleField(out, "timestamp_period_ns", report.timestamp_period_ns);
    stringField(out, "cleanup_status", report.cleanup_status);
    stringField(out, "pixel_readback_buffer_destroy_status", report.pixel_readback_buffer_destroy_status);
    stringField(out, "pixel_readback_memory_free_status", report.pixel_readback_memory_free_status);
    stringField(out, "semaphore_destroy_status", report.semaphore_destroy_status);
    stringField(out, "present_semaphore_destroy_status", report.present_semaphore_destroy_status);
    stringField(out, "command_buffer_free_status", report.command_buffer_free_status);
    stringField(out, "command_pool_destroy_status", report.command_pool_destroy_status);
    stringField(out, "image_view_destroy_status", report.image_view_destroy_status);
    stringField(out, "render_pass_destroy_status", report.render_pass_destroy_status);
    stringField(out, "framebuffer_destroy_status", report.framebuffer_destroy_status);
    stringField(out, "graphics_pipeline_destroy_status", report.graphics_pipeline_destroy_status);
    stringField(out, "pipeline_layout_destroy_status", report.pipeline_layout_destroy_status);
    stringField(out, "vertex_shader_module_destroy_status", report.vertex_shader_module_destroy_status);
    stringField(out, "fragment_shader_module_destroy_status", report.fragment_shader_module_destroy_status);
    stringField(out, "swapchain_destroy_status", report.swapchain_destroy_status);
    stringField(out, "device_destroy_status", report.device_destroy_status);
    stringField(out, "surface_destroy_status", report.surface_destroy_status);
    stringField(out, "instance_destroy_status", report.instance_destroy_status);
    stringField(out, "native_window_release_status", report.native_window_release_status);
    stringField(out, "failure_reason", report.failure_reason, false);
    out << "}\n";
    return out.str();
}

} // namespace nyamist::discovery
