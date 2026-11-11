#include "nyamist/runtime/vulkan_backend.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using nyamist::runtime::NativeHandle;
using nyamist::runtime::VulkanBackendDispatch;
using nyamist::runtime::VulkanBackendSession;
struct TestContext { int destroyed_devices{}; int destroyed_instances{}; int submitted{}; bool fail_device{}; bool fail_queue{}; bool fail_submit{}; };
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool createInstance(void*, NativeHandle* out, std::string*) { *out=11U; return true; }
bool selectDevice(void*, NativeHandle instance, NativeHandle* out, std::uint32_t* family, std::string*) {
    check(instance==11U,"selection receives owned instance"); *out=22U; *family=3U; return true;
}
bool createDevice(void* ctx, NativeHandle physical, std::uint32_t family, NativeHandle* out, std::string* error) {
    auto& c=*static_cast<TestContext*>(ctx); check(physical==22U&&family==3U,"device uses selected GPU/family");
    if(c.fail_device){*error="injected device failure";return false;} *out=33U;return true;
}
bool getQueue(void* ctx, NativeHandle device, std::uint32_t family, NativeHandle* out, std::string* error) {
    auto& c=*static_cast<TestContext*>(ctx); check(device==33U&&family==3U,"queue uses created device");
    if(c.fail_queue){*error="injected queue failure";return false;} *out=44U;return true;
}
bool submitEmpty(void* ctx, NativeHandle device, NativeHandle queue, std::string* error) {
    auto& c=*static_cast<TestContext*>(ctx);check(device==33U&&queue==44U,"submission uses owned handles");
    if(c.fail_submit){*error="injected submit failure";return false;}++c.submitted;return true;
}
void destroyDevice(void* ctx, NativeHandle h){check(h==33U,"destroy correct device");++static_cast<TestContext*>(ctx)->destroyed_devices;}
void destroyInstance(void* ctx, NativeHandle h){check(h==11U,"destroy correct instance");++static_cast<TestContext*>(ctx)->destroyed_instances;}
VulkanBackendDispatch dispatch(TestContext& c){return {createInstance,selectDevice,createDevice,getQueue,destroyDevice,destroyInstance,submitEmpty,&c};}

void testLifecycleAndSubmission() {
    TestContext c; {
        VulkanBackendSession b(dispatch(c));
        check(!b.submit_smoke_command(),"reject submission before initialization");
        check(b.initialize(),"initialize backend");
        check(b.status().initialized&&b.status().graphics_queue==44U,"expose valid graphics queue");
        check(b.initialize(),"initialize idempotent");
        check(b.submit_smoke_command()&&b.submit_smoke_command(),"complete smoke submissions");
        check(b.status().completed_smoke_submissions==2U&&b.status().last_submission_status=="success","count completed submissions");
        b.shutdown();b.shutdown();check(!b.status().initialized,"shutdown clears state");
    }
    check(c.destroyed_devices==1&&c.destroyed_instances==1,"destroy each owned object once");
}
void testSubmitFailure() {
    TestContext c;c.fail_submit=true;VulkanBackendSession b(dispatch(c));
    check(b.initialize(),"initialize before failed submit");
    check(!b.submit_smoke_command(),"report injected submission failure");
    check(b.status().last_submission_status=="failed"&&b.status().completed_smoke_submissions==0U&&
          b.status().error=="injected submit failure","failed submit is not counted");
}
void testDeviceRollback() {
    TestContext c;c.fail_device=true;VulkanBackendSession b(dispatch(c));
    check(!b.initialize(),"device failure aborts initialization");
    check(c.destroyed_instances==1&&c.destroyed_devices==0,"rollback destroys instance");
}
void testQueueRollback() {
    TestContext c;c.fail_queue=true;VulkanBackendSession b(dispatch(c));
    check(!b.initialize(),"queue failure aborts initialization");
    check(c.destroyed_devices==1&&c.destroyed_instances==1,"rollback destroys device before instance");
}
}
int main() {
    try {
        testLifecycleAndSubmission();testSubmitFailure();testDeviceRollback();testQueueRollback();
        std::cout<<"All NYAMIST Vulkan backend lifecycle/submission tests passed.\n";return 0;
    } catch(const std::exception& e) {
        std::cerr<<"Vulkan backend test failure: "<<e.what()<<'\n';return 1;
    }
}
