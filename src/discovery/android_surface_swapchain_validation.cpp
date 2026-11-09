#include "nyamist/discovery/android_surface_swapchain_validation.hpp"

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

} // namespace

std::string toJson(const AndroidSurfaceSwapchainValidationReport& report) {
    std::ostringstream out;
    out << "{\n";
    numberField(out, "schema_version", report.schema_version);
    stringField(out, "project", "CHORD OF NYAMIST / NYAMIST G925 R1.5");
    stringField(out, "status", report.status);
    stringField(out, "platform", report.platform);
    stringField(out, "loader_library", report.loader_library);
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
    numberField(out, "extent_width", report.extent_width);
    numberField(out, "extent_height", report.extent_height);
    stringField(out, "swapchain_create_status", report.swapchain_create_status);
    stringField(out, "swapchain_image_query_status", report.swapchain_image_query_status);
    numberField(out, "swapchain_image_count", report.swapchain_image_count);
    stringField(out, "semaphore_create_status", report.semaphore_create_status);
    stringField(out, "image_acquire_status", report.image_acquire_status);
    boolField(out, "image_acquired", report.image_acquired);
    numberField(out, "acquired_image_index", report.acquired_image_index);
    stringField(out, "image_present_status", report.image_present_status);
    boolField(out, "image_presented", report.image_presented);
    stringField(out, "queue_idle_status", report.queue_idle_status);
    boolField(out, "frame_content_written", report.frame_content_written);
    stringField(out, "cleanup_status", report.cleanup_status);
    stringField(out, "semaphore_destroy_status", report.semaphore_destroy_status);
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
