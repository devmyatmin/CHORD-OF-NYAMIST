#include "nyamist/runtime/vulkan_compute_pipeline_manager.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using namespace nyamist::runtime;

struct Fake {
    int calls = 0;
    int fail_at = 0;
    int destroyed = 0;
    NativeHandle next = 100;

    bool step(NativeHandle* out, std::string* error) {
        ++calls;
        if (calls == fail_at) {
            if (error) *error = "injected failure";
            return false;
        }
        *out = ++next;
        return true;
    }
};

static bool shader(void* p, const std::uint32_t*, std::size_t,
                   NativeHandle* out, std::string* e) {
    return static_cast<Fake*>(p)->step(out, e);
}
static void destroy_shader(void* p, NativeHandle) {
    ++static_cast<Fake*>(p)->destroyed;
}
static bool set_layout(void* p, NativeHandle* out, std::string* e) {
    return static_cast<Fake*>(p)->step(out, e);
}
static void destroy_set_layout(void* p, NativeHandle) {
    ++static_cast<Fake*>(p)->destroyed;
}
static bool pool(void* p, NativeHandle* out, std::string* e) {
    return static_cast<Fake*>(p)->step(out, e);
}
static void destroy_pool(void* p, NativeHandle) {
    ++static_cast<Fake*>(p)->destroyed;
}
static bool descriptor_set(void* p, NativeHandle, NativeHandle,
                           NativeHandle* out, std::string* e) {
    return static_cast<Fake*>(p)->step(out, e);
}
static bool pipeline_layout(void* p, NativeHandle, NativeHandle* out,
                            std::string* e) {
    return static_cast<Fake*>(p)->step(out, e);
}
static void destroy_pipeline_layout(void* p, NativeHandle) {
    ++static_cast<Fake*>(p)->destroyed;
}
static bool compute_pipeline(void* p, NativeHandle, NativeHandle,
                             NativeHandle* out, std::string* e) {
    return static_cast<Fake*>(p)->step(out, e);
}
static void destroy_compute_pipeline(void* p, NativeHandle) {
    ++static_cast<Fake*>(p)->destroyed;
}

static VulkanComputePipelineDispatch make_dispatch(Fake* fake) {
    VulkanComputePipelineDispatch d{};
    d.create_shader_module = shader;
    d.destroy_shader_module = destroy_shader;
    d.create_descriptor_set_layout = set_layout;
    d.destroy_descriptor_set_layout = destroy_set_layout;
    d.create_descriptor_pool = pool;
    d.destroy_descriptor_pool = destroy_pool;
    d.allocate_descriptor_set = descriptor_set;
    d.create_pipeline_layout = pipeline_layout;
    d.destroy_pipeline_layout = destroy_pipeline_layout;
    d.create_compute_pipeline = compute_pipeline;
    d.destroy_compute_pipeline = destroy_compute_pipeline;
    d.context = fake;
    return d;
}

int main() {
    const std::vector<std::uint32_t> valid_spirv{
        0x07230203U, 0x00010000U, 0U, 1U, 0U
    };

    {
        Fake fake;
        VulkanComputePipelineManager manager(make_dispatch(&fake));
        VulkanComputePipelineAllocation allocation{};
        std::string error;
        assert(manager.create(valid_spirv, &allocation, &error));
        assert(allocation.shader_module != 0);
        assert(allocation.pipeline != 0);
        manager.destroy(&allocation);
        assert(allocation.pipeline == 0);
        assert(fake.destroyed == 5);
    }

    {
        Fake fake;
        VulkanComputePipelineManager manager(make_dispatch(&fake));
        VulkanComputePipelineAllocation allocation{};
        std::string error;
        const std::vector<std::uint32_t> invalid{1U, 2U, 3U};
        assert(!manager.create(invalid, &allocation, &error));
        assert(fake.calls == 0);
        assert(!error.empty());
    }

    {
        VulkanComputePipelineManager manager({});
        VulkanComputePipelineAllocation allocation{};
        std::string error;
        assert(!manager.create(valid_spirv, &allocation, &error));
        assert(!error.empty());
    }

    {
        Fake fake;
        fake.fail_at = 6;
        VulkanComputePipelineManager manager(make_dispatch(&fake));
        VulkanComputePipelineAllocation allocation{};
        std::string error;
        assert(!manager.create(valid_spirv, &allocation, &error));
        assert(fake.calls == 6);
        assert(fake.destroyed == 4);
        assert(allocation.pipeline == 0);
        assert(allocation.shader_module == 0);
    }

    return 0;
}
