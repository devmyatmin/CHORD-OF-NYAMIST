#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nyamist::discovery {

// VK_QUEUE_GRAPHICS_BIT from the Vulkan core 1.0 queue flag definitions.
inline constexpr std::uint32_t kVulkanGraphicsQueueBit = 0x00000001U;

struct QueueFamilyCandidate {
    std::uint32_t index{};
    std::uint32_t queue_count{};
    std::uint32_t flags{};
};

[[nodiscard]] std::optional<std::uint32_t> selectGraphicsQueueFamily(
    const std::vector<QueueFamilyCandidate>& queue_families);

struct LogicalDeviceValidationReport {
    std::uint32_t schema_version{1U};
    std::string status{"unavailable"};
    bool vulkan_compiled{};
    std::string loader_library;
    std::string loader_api_version;
    std::string platform_scope{"process-local"};
    std::string physical_device_status{"not_attempted"};
    std::string device_name;
    std::string vendor_id_hex;
    std::string device_id_hex;
    std::string physical_device_api_version;
    bool target_gpu_match{};
    std::vector<QueueFamilyCandidate> queue_families;
    std::string queue_selection_status{"not_attempted"};
    bool has_selected_queue_family{};
    std::uint32_t selected_queue_family_index{};
    std::uint32_t selected_queue_count{};
    std::uint32_t selected_queue_flags{};
    std::string logical_device_status{"not_attempted"};
    std::string logical_device_result;
    std::string queue_retrieval_status{"not_attempted"};
    std::string queue_retrieval_result;
    bool queue_handle_non_null{};
    std::uint32_t enabled_device_extension_count{};
    std::string enabled_core_features{"none"};
    std::string cleanup_status{"not_attempted"};
    bool logical_device_destroyed{};
    std::string failure_reason;
};

// Creates one minimal logical device on the first suitable G925 graphics device,
// retrieves queue index 0, then destroys the logical device and instance.
[[nodiscard]] LogicalDeviceValidationReport collectLogicalDeviceValidationReport();
[[nodiscard]] std::string toJson(const LogicalDeviceValidationReport& report);

} // namespace nyamist::discovery
