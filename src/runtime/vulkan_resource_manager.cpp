#include "nyamist/runtime/vulkan_resource_manager.hpp"

#include <cstring>
#include <utility>

namespace nyamist::runtime {

namespace {
constexpr std::uint32_t MEMORY_HOST_VISIBLE = 0x2U;
constexpr std::uint32_t MEMORY_HOST_COHERENT = 0x4U;
}

VulkanBufferResourceManager::VulkanBufferResourceManager(
    VulkanBufferDispatch dispatch,
    std::vector<VulkanBufferMemoryType> memory_types)
    : dispatch_(dispatch), memory_types_(std::move(memory_types)) {}

VulkanBufferResourceManager::~VulkanBufferResourceManager() = default;

bool VulkanBufferResourceManager::choose_memory_type(
    std::uint32_t allowed_bits,
    std::uint32_t required_flags,
    std::uint32_t preferred_flags,
    std::uint32_t* index) const noexcept {

    if (!index) return false;

    std::uint32_t fallback = 0;
    bool found_fallback = false;

    for (std::uint32_t i = 0;
         i < memory_types_.size() && i < 32U; ++i) {

        if ((allowed_bits & (1U << i)) == 0U)
            continue;

        const auto flags = memory_types_[i].property_flags;

        if ((flags & required_flags) != required_flags)
            continue;

        if ((flags & preferred_flags) == preferred_flags) {
            *index = i;
            return true;
        }

        if (!found_fallback) {
            fallback = i;
            found_fallback = true;
        }
    }

    if (!found_fallback)
        return false;

    *index = fallback;
    return true;
}

bool VulkanBufferResourceManager::create(
    const VulkanBufferRequest& request,
    VulkanBufferAllocation* out,
    std::string* error) {

    if (!out) {
        if (error) *error = "Buffer allocation output is null";
        return false;
    }

    *out = {};

    if (request.size == 0U || request.usage_flags == 0U) {
        if (error) *error = "Buffer size and usage flags must be non-zero";
        return false;
    }

    if (!dispatch_.create_buffer ||
        !dispatch_.allocate_memory ||
        !dispatch_.bind_memory ||
        !dispatch_.destroy_buffer ||
        !dispatch_.free_memory) {

        if (error) *error = "Vulkan buffer dispatch table is incomplete";
        return false;
    }

    VulkanBufferMemoryRequirements requirements{};

    if (!dispatch_.create_buffer(
            dispatch_.context,
            request.size,
            request.usage_flags,
            &out->buffer,
            &requirements,
            error) || out->buffer == 0U) {

        if (error && error->empty())
            *error = "Vulkan buffer creation failed";

        *out = {};
        return false;
    }

    std::uint32_t memory_type = 0;

    if (requirements.size == 0U ||
        !choose_memory_type(
            requirements.memory_type_bits,
            request.required_memory_flags,
            request.preferred_memory_flags,
            &memory_type)) {

        if (error && error->empty())
            *error = "No compatible Vulkan memory type";

        dispatch_.destroy_buffer(dispatch_.context, out->buffer);
        *out = {};
        return false;
    }

    if (!dispatch_.allocate_memory(
            dispatch_.context,
            requirements.size,
            memory_type,
            &out->memory,
            error) || out->memory == 0U) {

        if (error && error->empty())
            *error = "Vulkan buffer memory allocation failed";

        dispatch_.destroy_buffer(dispatch_.context, out->buffer);
        *out = {};
        return false;
    }

    if (!dispatch_.bind_memory(
            dispatch_.context,
            out->buffer,
            out->memory,
            error)) {

        if (error && error->empty())
            *error = "Vulkan buffer memory binding failed";

        dispatch_.free_memory(dispatch_.context, out->memory);
        dispatch_.destroy_buffer(dispatch_.context, out->buffer);
        *out = {};
        return false;
    }

    out->allocation_size = requirements.size;
    out->memory_type_index = memory_type;
    out->memory_property_flags =
        memory_types_[memory_type].property_flags;

    return true;
}

void VulkanBufferResourceManager::destroy(
    VulkanBufferAllocation* allocation) noexcept {

    if (!allocation) return;

    if (allocation->buffer != 0U && dispatch_.destroy_buffer)
        dispatch_.destroy_buffer(dispatch_.context, allocation->buffer);

    if (allocation->memory != 0U && dispatch_.free_memory)
        dispatch_.free_memory(dispatch_.context, allocation->memory);

    *allocation = {};
}

bool VulkanBufferResourceManager::write(
    const VulkanBufferAllocation& allocation,
    const void* data,
    std::uint64_t size,
    std::string* error) {

    if (!data || allocation.memory == 0U ||
        size == 0U || size > allocation.allocation_size) {

        if (error) *error = "Invalid buffer write request";
        return false;
    }

    if ((allocation.memory_property_flags & MEMORY_HOST_VISIBLE) == 0U) {
        if (error) *error = "Buffer memory is not host visible";
        return false;
    }

    if (!dispatch_.map_memory || !dispatch_.unmap_memory) {
        if (error) *error = "Vulkan memory mapping functions unavailable";
        return false;
    }

    void* mapped = nullptr;

    if (!dispatch_.map_memory(
            dispatch_.context,
            allocation.memory,
            allocation.allocation_size,
            &mapped,
            error)) {
        return false;
    }

    if (!mapped) {
        dispatch_.unmap_memory(dispatch_.context, allocation.memory);
        if (error) *error = "Vulkan returned a null mapped pointer";
        return false;
    }

    std::memcpy(mapped, data, static_cast<std::size_t>(size));

    bool ok = true;

    if ((allocation.memory_property_flags & MEMORY_HOST_COHERENT) == 0U) {
        ok = dispatch_.flush_memory &&
             dispatch_.flush_memory(
                 dispatch_.context, allocation.memory, error);

        if (!ok && error && error->empty())
            *error = "Non-coherent memory flush unavailable";
    }

    dispatch_.unmap_memory(dispatch_.context, allocation.memory);
    return ok;
}

bool VulkanBufferResourceManager::read(
    const VulkanBufferAllocation& allocation,
    void* data,
    std::uint64_t size,
    std::string* error) {

    if (!data || allocation.memory == 0U ||
        size == 0U || size > allocation.allocation_size) {

        if (error) *error = "Invalid buffer read request";
        return false;
    }

    if ((allocation.memory_property_flags & MEMORY_HOST_VISIBLE) == 0U) {
        if (error) *error = "Buffer memory is not host visible";
        return false;
    }

    if (!dispatch_.map_memory || !dispatch_.unmap_memory) {
        if (error) *error = "Vulkan memory mapping functions unavailable";
        return false;
    }

    void* mapped = nullptr;

    if (!dispatch_.map_memory(
            dispatch_.context,
            allocation.memory,
            allocation.allocation_size,
            &mapped,
            error)) {
        return false;
    }

    if (!mapped) {
        dispatch_.unmap_memory(dispatch_.context, allocation.memory);
        if (error) *error = "Vulkan returned a null mapped pointer";
        return false;
    }

    bool ok = true;

    if ((allocation.memory_property_flags & MEMORY_HOST_COHERENT) == 0U) {
        ok = dispatch_.invalidate_memory &&
             dispatch_.invalidate_memory(
                 dispatch_.context, allocation.memory, error);

        if (!ok && error && error->empty())
            *error = "Non-coherent memory invalidate unavailable";
    }

    if (ok)
        std::memcpy(data, mapped, static_cast<std::size_t>(size));

    dispatch_.unmap_memory(dispatch_.context, allocation.memory);
    return ok;
}

} // namespace nyamist::runtime
