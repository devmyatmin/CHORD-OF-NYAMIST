#include "nyamist/runtime/vulkan_graphics_pipeline_manager.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using namespace nyamist::runtime;

struct Fake {
    int calls = 0;
    int fail_at = 0;
    int destroyed = 0;
    NativeHandle next = 200;

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

static bool layout(void* p, NativeHandle, NativeHandle* out,
                   std::string* e) {
    return static_cast<Fake*>(p)->step(out, e);
}

static void destroy_layout(void* p, NativeHandle) {
    ++static_cast<Fake*>(p)->destroyed;
}

static bool graphics(void* p, NativeHandle, NativeHandle, NativeHandle,
                     NativeHandle, NativeHandle* out, std::string* e) {
    return static_cast<Fake*>(p)->step(out, e);
}

static void destroy_graphics(void* p, NativeHandle) {
    ++static_cast<Fake*>(p)->destroyed;
}

static VulkanGraphicsPipelineDispatch make_dispatch(Fake* fake) {
    VulkanGraphicsPipelineDispatch d{};
    d.create_shader_module = shader;
    d.destroy_shader_module = destroy_shader;
    d.create_pipeline_layout = layout;
    d.destroy_pipeline_layout = destroy_layout;
    d.create_graphics_pipeline = graphics;
    d.destroy_graphics_pipeline = destroy_graphics;
    d.context = fake;
    return d;
}

int main() {
    const std::vector<std::uint32_t> valid{
        0x07230203U, 0x00010000U, 0U, 1U, 0U
    };

    {
        Fake fake;
        VulkanGraphicsPipelineManager manager(make_dispatch(&fake));
        VulkanGraphicsPipelineAllocation allocation{};
        std::string error;

        assert(manager.create(valid, valid, 50U, 0U,
                              &allocation, &error));
        assert(allocation.vertex_shader_module != 0U);
        assert(allocation.fragment_shader_module != 0U);
        assert(allocation.pipeline_layout != 0U);
        assert(allocation.pipeline != 0U);

        manager.destroy(&allocation);
        assert(allocation.pipeline == 0U);
        assert(allocation.vertex_shader_module == 0U);
        assert(fake.destroyed == 4);
    }

    {
        Fake fake;
        VulkanGraphicsPipelineManager manager(make_dispatch(&fake));
        VulkanGraphicsPipelineAllocation allocation{};
        std::string error;
        const std::vector<std::uint32_t> invalid{1U, 2U, 3U};

        assert(!manager.create(invalid, valid, 50U, 0U,
                               &allocation, &error));
        assert(fake.calls == 0);
        assert(!error.empty());
    }

    {
        Fake fake;
        VulkanGraphicsPipelineManager manager(make_dispatch(&fake));
        VulkanGraphicsPipelineAllocation allocation{};
        std::string error;

        assert(!manager.create(valid, valid, 0U, 0U,
                               &allocation, &error));
        assert(fake.calls == 0);
        assert(!error.empty());
    }

    {
        VulkanGraphicsPipelineManager manager({});
        VulkanGraphicsPipelineAllocation allocation{};
        std::string error;

        assert(!manager.create(valid, valid, 50U, 0U,
                               &allocation, &error));
        assert(!error.empty());
    }

    {
        Fake fake;
        fake.fail_at = 4;
        VulkanGraphicsPipelineManager manager(make_dispatch(&fake));
        VulkanGraphicsPipelineAllocation allocation{};
        std::string error;

        assert(!manager.create(valid, valid, 50U, 0U,
                               &allocation, &error));
        assert(fake.calls == 4);
        assert(fake.destroyed == 3);
        assert(allocation.pipeline == 0U);
        assert(allocation.vertex_shader_module == 0U);
    }

    return 0;
}
