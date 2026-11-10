#include "nyamist/runtime/vulkan_loader_adapter.hpp"

#include <string>
#include <utility>

#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <type_traits>
#include <vector>
#endif

namespace nyamist::runtime {
struct VulkanLoaderAdapter::Impl {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    void* library{};
    PFN_vkGetInstanceProcAddr get_instance_proc_addr{};
    PFN_vkGetDeviceProcAddr get_device_proc_addr{};
    VkInstance instance{VK_NULL_HANDLE};
    VkPhysicalDevice physical{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkQueue queue{VK_NULL_HANDLE};
    std::uint32_t queue_family{};

    template <typename T> static NativeHandle toNative(T value) {
        if constexpr (std::is_pointer_v<T>) return reinterpret_cast<NativeHandle>(value);
        else return static_cast<NativeHandle>(value);
    }
    template <typename T> T instanceProc(const char* name) const {
        return reinterpret_cast<T>(get_instance_proc_addr(instance, name));
    }
    template <typename T> T deviceProc(const char* name) const {
        return reinterpret_cast<T>(get_device_proc_addr(device, name));
    }

    static bool createInstance(void* opaque, NativeHandle* out, std::string* error) {
        auto& self=*static_cast<Impl*>(opaque);
        auto create=reinterpret_cast<PFN_vkCreateInstance>(
            self.get_instance_proc_addr(VK_NULL_HANDLE,"vkCreateInstance"));
        if(!create){if(error)*error="vkCreateInstance unavailable";return false;}
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName="NYAMIST";app.applicationVersion=1;
        app.pEngineName="NYAMIST Runtime";app.engineVersion=1;app.apiVersion=VK_API_VERSION_1_0;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.pApplicationInfo=&app;
        const VkResult result=create(&info,nullptr,&self.instance);
        if(result!=VK_SUCCESS){if(error)*error="vkCreateInstance failed: "+std::to_string(result);return false;}
        *out=toNative(self.instance);return true;
    }
    static bool selectPhysical(void* opaque, NativeHandle, NativeHandle* out,
                               std::uint32_t* family, std::string* error) {
        auto& self=*static_cast<Impl*>(opaque);
        auto enumerate=self.instanceProc<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
        auto getQueues=self.instanceProc<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        if(!enumerate||!getQueues){if(error)*error="Physical-device Vulkan entry points unavailable";return false;}
        std::uint32_t count=0;VkResult result=enumerate(self.instance,&count,nullptr);
        if(result!=VK_SUCCESS||count==0){if(error)*error="No Vulkan physical devices";return false;}
        std::vector<VkPhysicalDevice> devices(count);
        result=enumerate(self.instance,&count,devices.data());
        if(result!=VK_SUCCESS){if(error)*error="Physical-device enumeration failed: "+std::to_string(result);return false;}
        for(const auto physical:devices){
            std::uint32_t queueCount=0;getQueues(physical,&queueCount,nullptr);
            std::vector<VkQueueFamilyProperties> queues(queueCount);
            if(queueCount)getQueues(physical,&queueCount,queues.data());
            for(std::uint32_t i=0;i<queueCount;++i){
                if(queues[i].queueCount>0&&(queues[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){
                    self.physical=physical;self.queue_family=i;*out=toNative(physical);*family=i;return true;
                }
            }
        }
        if(error)*error="No graphics-capable Vulkan queue family found";
        return false;
    }
    static bool createDevice(void* opaque, NativeHandle, std::uint32_t family,
                             NativeHandle* out, std::string* error) {
        auto& self=*static_cast<Impl*>(opaque);
        auto create=self.instanceProc<PFN_vkCreateDevice>("vkCreateDevice");
        if(!create){if(error)*error="vkCreateDevice unavailable";return false;}
        // Resolve device dispatch before creating the device. If this lookup
        // fails, there is no newly-created VkDevice to leak during rollback.
        self.get_device_proc_addr=reinterpret_cast<PFN_vkGetDeviceProcAddr>(
            self.get_instance_proc_addr(self.instance,"vkGetDeviceProcAddr"));
        if(!self.get_device_proc_addr){if(error)*error="vkGetDeviceProcAddr unavailable";return false;}

        const float priority=1.0F;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex=family;queueInfo.queueCount=1;queueInfo.pQueuePriorities=&priority;
        VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        info.queueCreateInfoCount=1;info.pQueueCreateInfos=&queueInfo;
        const VkResult result=create(self.physical,&info,nullptr,&self.device);
        if(result!=VK_SUCCESS){if(error)*error="vkCreateDevice failed: "+std::to_string(result);return false;}
        // Publish the handle before loading device dispatch so session rollback
        // can destroy the device if vkGetDeviceProcAddr resolution fails.
        *out=toNative(self.device);
        return true;
    }
    static bool getQueue(void* opaque, NativeHandle, std::uint32_t family,
                         NativeHandle* out, std::string* error) {
        auto& self=*static_cast<Impl*>(opaque);
        auto get=self.deviceProc<PFN_vkGetDeviceQueue>("vkGetDeviceQueue");
        if(!get){if(error)*error="vkGetDeviceQueue unavailable";return false;}
        get(self.device,family,0,&self.queue);
        if(self.queue==VK_NULL_HANDLE){if(error)*error="vkGetDeviceQueue returned null";return false;}
        *out=toNative(self.queue);return true;
    }
    static bool submitEmpty(void* opaque, NativeHandle, NativeHandle, std::string* error) {
        auto& self=*static_cast<Impl*>(opaque);
        auto createPool=self.deviceProc<PFN_vkCreateCommandPool>("vkCreateCommandPool");
        auto destroyPool=self.deviceProc<PFN_vkDestroyCommandPool>("vkDestroyCommandPool");
        auto allocate=self.deviceProc<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers");
        auto freeBuffers=self.deviceProc<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers");
        auto begin=self.deviceProc<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer");
        auto end=self.deviceProc<PFN_vkEndCommandBuffer>("vkEndCommandBuffer");
        auto createFence=self.deviceProc<PFN_vkCreateFence>("vkCreateFence");
        auto destroyFence=self.deviceProc<PFN_vkDestroyFence>("vkDestroyFence");
        auto submit=self.deviceProc<PFN_vkQueueSubmit>("vkQueueSubmit");
        auto wait=self.deviceProc<PFN_vkWaitForFences>("vkWaitForFences");
        if(!createPool||!destroyPool||!allocate||!freeBuffers||!begin||!end||
           !createFence||!destroyFence||!submit||!wait){
            if(error)*error="Command submission Vulkan entry points unavailable";return false;
        }
        VkCommandPool pool=VK_NULL_HANDLE;VkCommandBuffer command=VK_NULL_HANDLE;VkFence fence=VK_NULL_HANDLE;
        VkResult result=VK_SUCCESS;
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex=self.queue_family;
        result=createPool(self.device,&poolInfo,nullptr,&pool);
        if(result==VK_SUCCESS){
            VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocInfo.commandPool=pool;allocInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;allocInfo.commandBufferCount=1;
            result=allocate(self.device,&allocInfo,&command);
        }
        if(result==VK_SUCCESS){
            VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            beginInfo.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result=begin(command,&beginInfo);
        }
        if(result==VK_SUCCESS)result=end(command);
        if(result==VK_SUCCESS){
            VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            result=createFence(self.device,&fenceInfo,nullptr,&fence);
        }
        if(result==VK_SUCCESS){
            VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submitInfo.commandBufferCount=1;submitInfo.pCommandBuffers=&command;
            result=submit(self.queue,1,&submitInfo,fence);
        }
        if(result==VK_SUCCESS)result=wait(self.device,1,&fence,VK_TRUE,UINT64_MAX);
        if(fence!=VK_NULL_HANDLE)destroyFence(self.device,fence,nullptr);
        if(command!=VK_NULL_HANDLE&&pool!=VK_NULL_HANDLE)freeBuffers(self.device,pool,1,&command);
        if(pool!=VK_NULL_HANDLE)destroyPool(self.device,pool,nullptr);
        if(result!=VK_SUCCESS){if(error)*error="Vulkan empty-command submission failed: "+std::to_string(result);return false;}
        return true;
    }
    static void destroyDevice(void* opaque, NativeHandle) {
        auto& self=*static_cast<Impl*>(opaque);
        auto destroy=self.deviceProc<PFN_vkDestroyDevice>("vkDestroyDevice");
        if(self.device!=VK_NULL_HANDLE&&destroy)destroy(self.device,nullptr);
        self.device=VK_NULL_HANDLE;self.queue=VK_NULL_HANDLE;self.get_device_proc_addr=nullptr;
    }
    static void destroyInstance(void* opaque, NativeHandle) {
        auto& self=*static_cast<Impl*>(opaque);
        auto destroy=self.instanceProc<PFN_vkDestroyInstance>("vkDestroyInstance");
        if(self.instance!=VK_NULL_HANDLE&&destroy)destroy(self.instance,nullptr);
        self.instance=VK_NULL_HANDLE;self.physical=VK_NULL_HANDLE;
    }
#endif
};

VulkanLoaderAdapter::VulkanLoaderAdapter():impl_(std::make_unique<Impl>()){}
VulkanLoaderAdapter::~VulkanLoaderAdapter(){close();}
bool VulkanLoaderAdapter::open(std::string* error){
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if(impl_->library)return true;
    const char* candidates[]={"libvulkan.so","libvulkan.so.1"};
    std::string details;
    for(const char* name:candidates){
        void* lib=dlopen(name,RTLD_NOW|RTLD_LOCAL);
        if(!lib){const char* why=dlerror();if(!details.empty())details+="; ";details+=name;details+=": ";details+=why?why:"dlopen failed";continue;}
        auto proc=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib,"vkGetInstanceProcAddr"));
        if(!proc){dlclose(lib);continue;}
        impl_->library=lib;impl_->get_instance_proc_addr=proc;return true;
    }
    if(error)*error="Unable to load Android/public Vulkan loader: "+details;
    return false;
#else
    if(error)*error="This build has no Android Vulkan loader adapter support";
    return false;
#endif
}
void VulkanLoaderAdapter::close() noexcept {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if(impl_&&impl_->library){dlclose(impl_->library);impl_->library=nullptr;impl_->get_instance_proc_addr=nullptr;}
#endif
}
bool VulkanLoaderAdapter::available() const noexcept {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    return impl_&&impl_->library&&impl_->get_instance_proc_addr;
#else
    return false;
#endif
}
VulkanBackendDispatch VulkanLoaderAdapter::dispatch() noexcept {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    return {Impl::createInstance,Impl::selectPhysical,Impl::createDevice,Impl::getQueue,
            Impl::destroyDevice,Impl::destroyInstance,Impl::submitEmpty,impl_.get()};
#else
    return {};
#endif
}
} // namespace nyamist::runtime
