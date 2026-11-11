#include "nyamist/runtime/vulkan_resource_manager.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace nyamist::runtime;

struct Context {
    int buffers_created{}, buffers_destroyed{};
    int memories_allocated{}, memories_freed{}, binds{};
    int maps{}, unmaps{}, flushes{}, invalidates{};
    bool fail_allocate{}, fail_bind{};
    bool fail_map{}, fail_flush{}, fail_invalidate{};
    std::array<unsigned char, 8192> storage{};
};

void check(bool v, const char* m) {
    if (!v) throw std::runtime_error(m);
}

bool createBuffer(void* p, std::uint64_t size, std::uint32_t usage,
                  NativeHandle* out,
                  VulkanBufferMemoryRequirements* req, std::string*) {
    auto& c = *static_cast<Context*>(p);
    check(size == 4096U && usage == 7U, "buffer request forwarded");
    ++c.buffers_created;
    *out = 101U;
    *req = {8192U, 256U, 0b110U};
    return true;
}

bool allocateMemory(void* p, std::uint64_t size, std::uint32_t type,
                    NativeHandle* out, std::string* e) {
    auto& c = *static_cast<Context*>(p);
    check(size == 8192U, "allocation size forwarded");
    check(type < 3U, "valid memory type selected");
    if (c.fail_allocate) {
        if (e) *e = "injected allocation failure";
        return false;
    }
    ++c.memories_allocated;
    *out = 202U;
    return true;
}

bool bindMemory(void* p, NativeHandle b, NativeHandle m, std::string* e) {
    auto& c = *static_cast<Context*>(p);
    check(b == 101U && m == 202U, "bind correct handles");
    if (c.fail_bind) {
        if (e) *e = "injected bind failure";
        return false;
    }
    ++c.binds;
    return true;
}

void destroyBuffer(void* p, NativeHandle h) {
    check(h == 101U, "destroy buffer handle");
    ++static_cast<Context*>(p)->buffers_destroyed;
}

void freeMemory(void* p, NativeHandle h) {
    check(h == 202U, "free memory handle");
    ++static_cast<Context*>(p)->memories_freed;
}

bool mapMemory(void* p, NativeHandle h, std::uint64_t size,
               void** out, std::string* e) {
    auto& c = *static_cast<Context*>(p);
    check(h == 202U && size == 8192U, "map correct allocation");
    ++c.maps;
    if (c.fail_map) {
        if (e) *e = "injected map failure";
        return false;
    }
    *out = c.storage.data();
    return true;
}

void unmapMemory(void* p, NativeHandle h) {
    check(h == 202U, "unmap correct allocation");
    ++static_cast<Context*>(p)->unmaps;
}

bool flushMemory(void* p, NativeHandle h, std::string* e) {
    auto& c = *static_cast<Context*>(p);
    check(h == 202U, "flush correct allocation");
    ++c.flushes;
    if (c.fail_flush) {
        if (e) *e = "injected flush failure";
        return false;
    }
    return true;
}

bool invalidateMemory(void* p, NativeHandle h, std::string* e) {
    auto& c = *static_cast<Context*>(p);
    check(h == 202U, "invalidate correct allocation");
    ++c.invalidates;
    if (c.fail_invalidate) {
        if (e) *e = "injected invalidate failure";
        return false;
    }
    return true;
}

VulkanBufferDispatch makeDispatch(Context& c) {
    return {
        createBuffer, allocateMemory, bindMemory,
        destroyBuffer, freeMemory, &c,
        mapMemory, unmapMemory, flushMemory, invalidateMemory
    };
}

std::vector<VulkanBufferMemoryType> memoryTypes() {
    return {{0b001U}, {0b011U}, {0b111U}};
}

void testCreateAndDestroy() {
    Context c;
    VulkanBufferResourceManager manager(makeDispatch(c), memoryTypes());
    VulkanBufferAllocation allocation{};
    std::string error;
    check(manager.create({4096U, 7U, 0b001U, 0b100U},
                         &allocation, &error), "create buffer allocation");
    check(allocation.buffer == 101U && allocation.memory == 202U &&
          allocation.allocation_size == 8192U &&
          allocation.memory_type_index == 2U, "allocation metadata");
    manager.destroy(&allocation);
    manager.destroy(&allocation);
    check(c.buffers_destroyed == 1 && c.memories_freed == 1,
          "destroy resources exactly once");
}

void testAllocationRollback() {
    Context c;
    c.fail_allocate = true;
    VulkanBufferResourceManager manager(makeDispatch(c), memoryTypes());
    VulkanBufferAllocation allocation{};
    std::string error;
    check(!manager.create({4096U, 7U, 0b001U, 0b100U},
                          &allocation, &error), "allocation failure returned");
    check(allocation.buffer == 0U && allocation.memory == 0U &&
          c.buffers_destroyed == 1,
          "buffer rolled back after memory allocation failure");
}

void testBindRollback() {
    Context c;
    c.fail_bind = true;
    VulkanBufferResourceManager manager(makeDispatch(c), memoryTypes());
    VulkanBufferAllocation allocation{};
    std::string error;
    check(!manager.create({4096U, 7U, 0b001U, 0b100U},
                          &allocation, &error), "bind failure returned");
    check(allocation.buffer == 0U && allocation.memory == 0U &&
          c.buffers_destroyed == 1 && c.memories_freed == 1,
          "memory and buffer rolled back after bind failure");
}

void testNoCompatibleMemory() {
    Context c;
    VulkanBufferResourceManager manager(
        makeDispatch(c), {{0b001U}, {0b011U}});
    VulkanBufferAllocation allocation{};
    std::string error;
    check(!manager.create({4096U, 7U, 0b100U, 0U},
                          &allocation, &error), "reject incompatible memory");
    check(c.buffers_destroyed == 1 && allocation.buffer == 0U,
          "buffer destroyed on memory type mismatch");
}

void testNonCoherentWriteRead() {
    Context c;
    VulkanBufferResourceManager manager(makeDispatch(c), memoryTypes());
    VulkanBufferAllocation allocation{};
    std::string error;

    check(manager.create({4096U, 7U, 0b010U, 0U},
                         &allocation, &error), "create host-visible allocation");
    check(allocation.memory_type_index == 1U,
          "select non-coherent host-visible memory");

    const std::array<unsigned char, 4> input{{10U, 20U, 30U, 40U}};
    std::array<unsigned char, 4> output{};

    check(manager.write(allocation, input.data(), input.size(), &error),
          "write non-coherent memory");
    check(c.storage[0] == 10U && c.storage[3] == 40U,
          "write copied bytes into mapped memory");
    check(manager.read(allocation, output.data(), output.size(), &error),
          "read non-coherent memory");
    check(output == input, "read returned written bytes");
    check(c.maps == 2 && c.unmaps == 2,
          "write and read both unmap memory");
    check(c.flushes == 1 && c.invalidates == 1,
          "non-coherent write flushes and read invalidates");

    manager.destroy(&allocation);
}

void testCoherentMemorySkipsCacheOperations() {
    Context c;
    VulkanBufferResourceManager manager(makeDispatch(c), memoryTypes());
    VulkanBufferAllocation allocation{};
    std::string error;

    check(manager.create({4096U, 7U, 0b010U, 0b100U},
                         &allocation, &error), "create coherent allocation");

    const unsigned char input[] = {1U, 2U, 3U};
    unsigned char output[] = {};

    check(manager.write(allocation, input, sizeof(input), &error),
          "write coherent memory");
    unsigned char readback[3]{};
    check(manager.read(allocation, readback, sizeof(readback), &error),
          "read coherent memory");
    check(std::memcmp(input, readback, sizeof(input)) == 0,
          "coherent memory data round trip");
    check(c.flushes == 0 && c.invalidates == 0,
          "coherent memory skips flush and invalidate");
    check(c.maps == 2 && c.unmaps == 2,
          "coherent mappings are unmapped");

    manager.destroy(&allocation);
}

void testFlushFailureStillUnmaps() {
    Context c;
    c.fail_flush = true;
    VulkanBufferResourceManager manager(makeDispatch(c), memoryTypes());
    VulkanBufferAllocation allocation{};
    std::string error;

    check(manager.create({4096U, 7U, 0b010U, 0U},
                         &allocation, &error), "create non-coherent allocation");
    const unsigned char input[] = {9U, 8U};

    check(!manager.write(allocation, input, sizeof(input), &error),
          "flush failure returned");
    check(c.maps == 1 && c.flushes == 1 && c.unmaps == 1,
          "failed flush still unmaps memory");

    manager.destroy(&allocation);
}
}

int main() {
    try {
        testCreateAndDestroy();
        testAllocationRollback();
        testBindRollback();
        testNoCompatibleMemory();
        testNonCoherentWriteRead();
        testCoherentMemorySkipsCacheOperations();
        testFlushFailureStillUnmaps();
        std::cout << "All NYAMIST Vulkan resource manager tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Vulkan resource manager test failure: "
                  << e.what() << '\n';
        return 1;
    }
}
