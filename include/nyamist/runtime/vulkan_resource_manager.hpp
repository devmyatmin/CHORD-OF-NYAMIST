#pragma once

#include "nyamist/runtime/vulkan_backend.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nyamist::runtime {

struct VulkanBufferMemoryRequirements {
    std::uint64_t size{};
    std::uint64_t alignment{};
    std::uint32_t memory_type_bits{};
};

struct VulkanBufferMemoryType {
    std::uint32_t property_flags{};
};

struct VulkanBufferRequest {
    std::uint64_t size{};
    std::uint32_t usage_flags{};
    std::uint32_t required_memory_flags{};
    std::uint32_t preferred_memory_flags{};
};

struct VulkanBufferAllocation {
    NativeHandle buffer{};
    NativeHandle memory{};
    std::uint64_t allocation_size{};
    std::uint32_t memory_type_index{};
    std::uint32_t memory_property_flags{};
};

struct VulkanBufferDispatch {
    bool (*create_buffer)(void*, std::uint64_t, std::uint32_t,
                          NativeHandle*, VulkanBufferMemoryRequirements*,
                          std::string*){};

    bool (*allocate_memory)(void*, std::uint64_t, std::uint32_t,
                            NativeHandle*, std::string*){};

    bool (*bind_memory)(void*, NativeHandle, NativeHandle,
                        std::string*){};

    void (*destroy_buffer)(void*, NativeHandle){};
    void (*free_memory)(void*, NativeHandle){};

    void* context{};

    bool (*map_memory)(void*, NativeHandle, std::uint64_t,
                       void**, std::string*){};

    void (*unmap_memory)(void*, NativeHandle){};

    bool (*flush_memory)(void*, NativeHandle, std::string*){};

    bool (*invalidate_memory)(void*, NativeHandle, std::string*){};
};

class VulkanBufferResourceManager final {
public:
    VulkanBufferResourceManager(
        VulkanBufferDispatch dispatch,
        std::vector<VulkanBufferMemoryType> memory_types);

    ~VulkanBufferResourceManager();

    VulkanBufferResourceManager(const VulkanBufferResourceManager&) = delete;
    VulkanBufferResourceManager& operator=(const VulkanBufferResourceManager&) = delete;

    [[nodiscard]] bool create(
        const VulkanBufferRequest& request,
        VulkanBufferAllocation* out,
        std::string* error = nullptr);

    void destroy(VulkanBufferAllocation* allocation) noexcept;

    [[nodiscard]] bool write(
        const VulkanBufferAllocation& allocation,
        const void* data,
        std::uint64_t size,
        std::string* error = nullptr);

    [[nodiscard]] bool read(
        const VulkanBufferAllocation& allocation,
        void* data,
        std::uint64_t size,
        std::string* error = nullptr);

private:
    [[nodiscard]] bool choose_memory_type(
        std::uint32_t allowed_bits,
        std::uint32_t required_flags,
        std::uint32_t preferred_flags,
        std::uint32_t* index) const noexcept;

    VulkanBufferDispatch dispatch_{};
    std::vector<VulkanBufferMemoryType> memory_types_;
};

} // namespace nyamist::runtime
