#include "nyamist/runtime/vulkan_resource_manager.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace nyamist::runtime;
struct Context {
    int buffers_created{}, buffers_destroyed{}, memories_allocated{}, memories_freed{}, binds{};
    bool fail_allocate{}, fail_bind{};
};
void check(bool v, const char* m) { if (!v) throw std::runtime_error(m); }
bool createBuffer(void* p, std::uint64_t size, std::uint32_t usage, NativeHandle* out,
                  VulkanBufferMemoryRequirements* req, std::string*) {
    auto& c=*static_cast<Context*>(p); check(size==4096U&&usage==7U,"buffer request forwarded");
    ++c.buffers_created; *out=101U; *req={8192U,256U,0b110U}; return true;
}
bool allocateMemory(void* p, std::uint64_t size, std::uint32_t type, NativeHandle* out, std::string* e) {
    auto& c=*static_cast<Context*>(p); check(size==8192U&&type==2U,"requirements and preferred memory type selected");
    if(c.fail_allocate){if(e)*e="injected allocation failure";return false;}
    ++c.memories_allocated;*out=202U;return true;
}
bool bindMemory(void* p, NativeHandle b, NativeHandle m, std::string* e) {
    auto& c=*static_cast<Context*>(p);check(b==101U&&m==202U,"bind correct handles");
    if(c.fail_bind){if(e)*e="injected bind failure";return false;}++c.binds;return true;
}
void destroyBuffer(void* p, NativeHandle h){check(h==101U,"destroy buffer handle");++static_cast<Context*>(p)->buffers_destroyed;}
void freeMemory(void* p, NativeHandle h){check(h==202U,"free memory handle");++static_cast<Context*>(p)->memories_freed;}
VulkanBufferDispatch makeDispatch(Context& c){return {createBuffer,allocateMemory,bindMemory,destroyBuffer,freeMemory,&c};}

void testCreateAndDestroy() {
    Context c;
    VulkanBufferResourceManager manager(makeDispatch(c),{{0b001U},{0b011U},{0b111U}});
    VulkanBufferAllocation allocation{};std::string error;
    check(manager.create({4096U,7U,0b001U,0b100U},&allocation,&error),"create buffer allocation");
    check(allocation.buffer==101U&&allocation.memory==202U&&allocation.allocation_size==8192U&&
          allocation.memory_type_index==2U,"allocation metadata");
    manager.destroy(&allocation);manager.destroy(&allocation);
    check(c.buffers_destroyed==1&&c.memories_freed==1,"destroy resources exactly once");
}
void testAllocationRollback() {
    Context c;c.fail_allocate=true;
    VulkanBufferResourceManager manager(makeDispatch(c),{{0b001U},{0b011U},{0b111U}});
    VulkanBufferAllocation allocation{};std::string error;
    check(!manager.create({4096U,7U,0b001U,0b100U},&allocation,&error),"allocation failure returned");
    check(allocation.buffer==0U&&allocation.memory==0U&&c.buffers_destroyed==1,
          "buffer rolled back after memory allocation failure");
}
void testBindRollback() {
    Context c;c.fail_bind=true;
    VulkanBufferResourceManager manager(makeDispatch(c),{{0b001U},{0b011U},{0b111U}});
    VulkanBufferAllocation allocation{};std::string error;
    check(!manager.create({4096U,7U,0b001U,0b100U},&allocation,&error),"bind failure returned");
    check(allocation.buffer==0U&&allocation.memory==0U&&c.buffers_destroyed==1&&c.memories_freed==1,
          "memory and buffer rolled back after bind failure");
}
void testNoCompatibleMemory() {
    Context c;
    VulkanBufferResourceManager manager(makeDispatch(c),{{0b001U},{0b011U}});
    VulkanBufferAllocation allocation{};std::string error;
    check(!manager.create({4096U,7U,0b100U,0U},&allocation,&error),"reject incompatible memory");
    check(c.buffers_destroyed==1&&allocation.buffer==0U,"buffer destroyed on memory type mismatch");
}
}
int main(){try{testCreateAndDestroy();testAllocationRollback();testBindRollback();testNoCompatibleMemory();
std::cout<<"All NYAMIST Vulkan resource manager tests passed.\n";return 0;}
catch(const std::exception& e){std::cerr<<"Vulkan resource manager test failure: "<<e.what()<<'\n';return 1;}}
