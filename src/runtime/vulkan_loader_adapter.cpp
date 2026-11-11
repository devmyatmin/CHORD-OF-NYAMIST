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
    template <typename T> static T fromNative(NativeHandle value) {
        if constexpr (std::is_pointer_v<T>) return reinterpret_cast<T>(value);
        else return static_cast<T>(value);
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
                if(queues[i].queueCount>0&&((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT))){
                    self.physical=physical;self.queue_family=i;*out=toNative(physical);*family=i;return true;
                }
            }
        }
        if(error)*error="No queue family supporting both graphics and compute was found";
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

    static bool createColorImage(
        void* opaque, std::uint32_t width, std::uint32_t height,
        std::uint32_t format, std::uint32_t usage,
        NativeHandle* out, VulkanBufferMemoryRequirements* requirements,
        std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto create = self.deviceProc<PFN_vkCreateImage>("vkCreateImage");
        auto getReq = self.deviceProc<PFN_vkGetImageMemoryRequirements>(
            "vkGetImageMemoryRequirements");
        if (!create || !getReq || !out || !requirements || !width || !height) {
            if (error) *error = "Vulkan image creation entry points unavailable or invalid arguments";
            return false;
        }
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = static_cast<VkFormat>(format);
        info.extent = {width, height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = static_cast<VkImageUsageFlags>(usage);
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImage image = VK_NULL_HANDLE;
        VkResult result = create(self.device, &info, nullptr, &image);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkCreateImage failed: " + std::to_string(result);
            return false;
        }
        VkMemoryRequirements req{};
        getReq(self.device, image, &req);
        *out = toNative(image);
        requirements->size = req.size;
        requirements->alignment = req.alignment;
        requirements->memory_type_bits = req.memoryTypeBits;
        return true;
    }

    static bool bindImageMemory(
        void* opaque, NativeHandle image, NativeHandle memory,
        std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto bind = self.deviceProc<PFN_vkBindImageMemory>("vkBindImageMemory");
        if (!bind || !image || !memory) {
            if (error) *error = "vkBindImageMemory unavailable or invalid handles";
            return false;
        }
        VkResult result = bind(self.device, fromNative<VkImage>(image),
                               fromNative<VkDeviceMemory>(memory), 0);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkBindImageMemory failed: " + std::to_string(result);
            return false;
        }
        return true;
    }

    static void destroyImage(void* opaque, NativeHandle image) {
        auto& self = *static_cast<Impl*>(opaque);
        auto destroy = self.deviceProc<PFN_vkDestroyImage>("vkDestroyImage");
        if (destroy && self.device && image)
            destroy(self.device, fromNative<VkImage>(image), nullptr);
    }

    static bool createImageView(
        void* opaque, NativeHandle image, std::uint32_t format,
        NativeHandle* out, std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto create = self.deviceProc<PFN_vkCreateImageView>("vkCreateImageView");
        if (!create || !image || !out) {
            if (error) *error = "vkCreateImageView unavailable or invalid arguments";
            return false;
        }
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.image = fromNative<VkImage>(image);
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = static_cast<VkFormat>(format);
        info.components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY};
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        VkImageView view = VK_NULL_HANDLE;
        VkResult result = create(self.device, &info, nullptr, &view);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkCreateImageView failed: " + std::to_string(result);
            return false;
        }
        *out = toNative(view);
        return true;
    }

    static void destroyImageView(void* opaque, NativeHandle view) {
        auto& self = *static_cast<Impl*>(opaque);
        auto destroy = self.deviceProc<PFN_vkDestroyImageView>("vkDestroyImageView");
        if (destroy && self.device && view)
            destroy(self.device, fromNative<VkImageView>(view), nullptr);
    }

    static bool createFramebuffer(
        void* opaque, NativeHandle renderPass, NativeHandle imageView,
        std::uint32_t width, std::uint32_t height,
        NativeHandle* out, std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto create = self.deviceProc<PFN_vkCreateFramebuffer>("vkCreateFramebuffer");
        if (!create || !renderPass || !imageView || !out || !width || !height) {
            if (error) *error = "vkCreateFramebuffer unavailable or invalid arguments";
            return false;
        }
        VkImageView attachment = fromNative<VkImageView>(imageView);
        VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = fromNative<VkRenderPass>(renderPass);
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.width = width;
        info.height = height;
        info.layers = 1;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkResult result = create(self.device, &info, nullptr, &framebuffer);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkCreateFramebuffer failed: " + std::to_string(result);
            return false;
        }
        *out = toNative(framebuffer);
        return true;
    }

    static void destroyFramebuffer(void* opaque, NativeHandle framebuffer) {
        auto& self = *static_cast<Impl*>(opaque);
        auto destroy = self.deviceProc<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer");
        if (destroy && self.device && framebuffer)
            destroy(self.device, fromNative<VkFramebuffer>(framebuffer), nullptr);
    }

    static bool createBuffer(void* opaque, std::uint64_t size, std::uint32_t usage,
                             NativeHandle* out, VulkanBufferMemoryRequirements* requirements,
                             std::string* error) {
        auto& self=*static_cast<Impl*>(opaque);
        auto create=self.deviceProc<PFN_vkCreateBuffer>("vkCreateBuffer");
        auto getRequirements=self.deviceProc<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements");
        if(!create||!getRequirements||!out||!requirements){
            if(error)*error="Vulkan buffer creation entry points unavailable";
            return false;
        }
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=static_cast<VkDeviceSize>(size);
        info.usage=static_cast<VkBufferUsageFlags>(usage);
        info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        VkBuffer buffer=VK_NULL_HANDLE;
        const VkResult result=create(self.device,&info,nullptr,&buffer);
        if(result!=VK_SUCCESS){
            if(error)*error="vkCreateBuffer failed: "+std::to_string(result);
            return false;
        }
        VkMemoryRequirements nativeRequirements{};
        getRequirements(self.device,buffer,&nativeRequirements);
        *out=toNative(buffer);
        requirements->size=nativeRequirements.size;
        requirements->alignment=nativeRequirements.alignment;
        requirements->memory_type_bits=nativeRequirements.memoryTypeBits;
        return true;
    }
    static bool allocateBufferMemory(void* opaque, std::uint64_t size, std::uint32_t type,
                                     NativeHandle* out, std::string* error) {
        auto& self=*static_cast<Impl*>(opaque);
        auto allocate=self.deviceProc<PFN_vkAllocateMemory>("vkAllocateMemory");
        if(!allocate||!out){if(error)*error="vkAllocateMemory unavailable";return false;}
        VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        info.allocationSize=static_cast<VkDeviceSize>(size);
        info.memoryTypeIndex=type;
        VkDeviceMemory memory=VK_NULL_HANDLE;
        const VkResult result=allocate(self.device,&info,nullptr,&memory);
        if(result!=VK_SUCCESS){
            if(error)*error="vkAllocateMemory failed: "+std::to_string(result);
            return false;
        }
        *out=toNative(memory);
        return true;
    }
    static bool bindBufferMemory(void* opaque, NativeHandle buffer, NativeHandle memory,
                                 std::string* error) {
        auto& self=*static_cast<Impl*>(opaque);
        auto bind=self.deviceProc<PFN_vkBindBufferMemory>("vkBindBufferMemory");
        if(!bind){if(error)*error="vkBindBufferMemory unavailable";return false;}
        const VkResult result=bind(self.device,fromNative<VkBuffer>(buffer),
                                   fromNative<VkDeviceMemory>(memory),0);
        if(result!=VK_SUCCESS){
            if(error)*error="vkBindBufferMemory failed: "+std::to_string(result);
            return false;
        }
        return true;
    }
    static void destroyBuffer(void* opaque, NativeHandle buffer) {
        auto& self=*static_cast<Impl*>(opaque);
        auto destroy=self.deviceProc<PFN_vkDestroyBuffer>("vkDestroyBuffer");
        if(destroy&&self.device!=VK_NULL_HANDLE&&buffer!=0U)
            destroy(self.device,fromNative<VkBuffer>(buffer),nullptr);
    }
    static void freeBufferMemory(void* opaque, NativeHandle memory) {
        auto& self=*static_cast<Impl*>(opaque);
        auto release=self.deviceProc<PFN_vkFreeMemory>("vkFreeMemory");
        if(release&&self.device!=VK_NULL_HANDLE&&memory!=0U)
            release(self.device,fromNative<VkDeviceMemory>(memory),nullptr);
    }

    static bool mapBufferMemory(void* opaque, NativeHandle memory,
                                std::uint64_t size, void** out,
                                std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto map = self.deviceProc<PFN_vkMapMemory>("vkMapMemory");

        if (!map || !out || memory == 0U || size == 0U) {
            if (error) *error = "Vulkan memory mapping unavailable or invalid";
            return false;
        }

        *out = nullptr;
        const VkResult result = map(
            self.device, fromNative<VkDeviceMemory>(memory),
            0, static_cast<VkDeviceSize>(size), 0, out);

        if (result != VK_SUCCESS) {
            if (error) *error = "vkMapMemory failed: " + std::to_string(result);
            return false;
        }
        return *out != nullptr;
    }

    static void unmapBufferMemory(void* opaque, NativeHandle memory) {
        auto& self = *static_cast<Impl*>(opaque);
        auto unmap = self.deviceProc<PFN_vkUnmapMemory>("vkUnmapMemory");

        if (unmap && self.device != VK_NULL_HANDLE && memory != 0U)
            unmap(self.device, fromNative<VkDeviceMemory>(memory));
    }

    static bool flushBufferMemory(void* opaque, NativeHandle memory,
                                  std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto flush = self.deviceProc<PFN_vkFlushMappedMemoryRanges>(
            "vkFlushMappedMemoryRanges");

        if (!flush || memory == 0U) {
            if (error) *error = "Vulkan memory flush unavailable";
            return false;
        }

        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory = fromNative<VkDeviceMemory>(memory);
        range.offset = 0;
        range.size = VK_WHOLE_SIZE;

        const VkResult result = flush(self.device, 1, &range);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkFlushMappedMemoryRanges failed: " +
                                std::to_string(result);
            return false;
        }
        return true;
    }

    static bool invalidateBufferMemory(void* opaque, NativeHandle memory,
                                       std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto invalidate =
            self.deviceProc<PFN_vkInvalidateMappedMemoryRanges>(
                "vkInvalidateMappedMemoryRanges");

        if (!invalidate || memory == 0U) {
            if (error) *error = "Vulkan memory invalidate unavailable";
            return false;
        }

        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory = fromNative<VkDeviceMemory>(memory);
        range.offset = 0;
        range.size = VK_WHOLE_SIZE;

        const VkResult result = invalidate(self.device, 1, &range);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkInvalidateMappedMemoryRanges failed: " +
                                std::to_string(result);
            return false;
        }
        return true;
    }

    std::vector<VulkanBufferMemoryType> queryMemoryTypes() const {
        std::vector<VulkanBufferMemoryType> result;
        if(!physical||!instance||!get_instance_proc_addr)return result;
        auto get=instanceProc<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties");
        if(!get)return result;
        VkPhysicalDeviceMemoryProperties properties{};
        get(physical,&properties);
        result.reserve(properties.memoryTypeCount);
        for(std::uint32_t i=0;i<properties.memoryTypeCount;++i)
            result.push_back({properties.memoryTypes[i].propertyFlags});
        return result;
    }


    static bool createShaderModule(void* opaque, const std::uint32_t* code,
                                   std::size_t bytes, NativeHandle* out,
                                   std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto create = self.deviceProc<PFN_vkCreateShaderModule>("vkCreateShaderModule");
        if (!create || !code || !out || bytes == 0U || bytes % 4U != 0U) {
            if (error) *error = "Invalid SPIR-V data or vkCreateShaderModule unavailable";
            return false;
        }
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = bytes;
        info.pCode = code;
        VkShaderModule module = VK_NULL_HANDLE;
        const VkResult result = create(self.device, &info, nullptr, &module);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkCreateShaderModule failed: " + std::to_string(result);
            return false;
        }
        *out = toNative(module);
        return true;
    }

    static void destroyShaderModule(void* opaque, NativeHandle handle) {
        auto& self = *static_cast<Impl*>(opaque);
        auto destroy = self.deviceProc<PFN_vkDestroyShaderModule>("vkDestroyShaderModule");
        if (destroy && handle) destroy(self.device, fromNative<VkShaderModule>(handle), nullptr);
    }

    static bool createDescriptorSetLayout(void* opaque, NativeHandle* out,
                                          std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto create = self.deviceProc<PFN_vkCreateDescriptorSetLayout>(
            "vkCreateDescriptorSetLayout");
        if (!create || !out) {
            if (error) *error = "vkCreateDescriptorSetLayout unavailable";
            return false;
        }
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo info{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = 1;
        info.pBindings = &binding;

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        const VkResult result = create(self.device, &info, nullptr, &layout);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkCreateDescriptorSetLayout failed: " +
                                std::to_string(result);
            return false;
        }
        *out = toNative(layout);
        return true;
    }

    static void destroyDescriptorSetLayout(void* opaque, NativeHandle handle) {
        auto& self = *static_cast<Impl*>(opaque);
        auto destroy = self.deviceProc<PFN_vkDestroyDescriptorSetLayout>(
            "vkDestroyDescriptorSetLayout");
        if (destroy && handle)
            destroy(self.device, fromNative<VkDescriptorSetLayout>(handle), nullptr);
    }

    static bool createDescriptorPool(void* opaque, NativeHandle* out,
                                     std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto create = self.deviceProc<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool");
        if (!create || !out) {
            if (error) *error = "vkCreateDescriptorPool unavailable";
            return false;
        }
        VkDescriptorPoolSize size{};
        size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        size.descriptorCount = 1;

        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        info.maxSets = 1;
        info.poolSizeCount = 1;
        info.pPoolSizes = &size;

        VkDescriptorPool pool = VK_NULL_HANDLE;
        const VkResult result = create(self.device, &info, nullptr, &pool);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkCreateDescriptorPool failed: " +
                                std::to_string(result);
            return false;
        }
        *out = toNative(pool);
        return true;
    }

    static void destroyDescriptorPool(void* opaque, NativeHandle handle) {
        auto& self = *static_cast<Impl*>(opaque);
        auto destroy = self.deviceProc<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool");
        if (destroy && handle)
            destroy(self.device, fromNative<VkDescriptorPool>(handle), nullptr);
    }

    static bool allocateDescriptorSet(void* opaque, NativeHandle poolHandle,
                                      NativeHandle layoutHandle, NativeHandle* out,
                                      std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto allocate = self.deviceProc<PFN_vkAllocateDescriptorSets>(
            "vkAllocateDescriptorSets");
        if (!allocate || !out) {
            if (error) *error = "vkAllocateDescriptorSets unavailable";
            return false;
        }
        VkDescriptorSetLayout layout =
            fromNative<VkDescriptorSetLayout>(layoutHandle);
        VkDescriptorSetAllocateInfo info{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        info.descriptorPool = fromNative<VkDescriptorPool>(poolHandle);
        info.descriptorSetCount = 1;
        info.pSetLayouts = &layout;

        VkDescriptorSet set = VK_NULL_HANDLE;
        const VkResult result = allocate(self.device, &info, &set);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkAllocateDescriptorSets failed: " +
                                std::to_string(result);
            return false;
        }
        *out = toNative(set);
        return true;
    }

    static bool createPipelineLayout(void* opaque, NativeHandle setLayoutHandle,
                                     NativeHandle* out, std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto create = self.deviceProc<PFN_vkCreatePipelineLayout>(
            "vkCreatePipelineLayout");
        if (!create || !out) {
            if (error) *error = "vkCreatePipelineLayout unavailable";
            return false;
        }
        VkDescriptorSetLayout setLayout =
            fromNative<VkDescriptorSetLayout>(setLayoutHandle);
        VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        info.setLayoutCount = setLayoutHandle ? 1U : 0U;
        info.pSetLayouts = setLayoutHandle ? &setLayout : nullptr;

        VkPipelineLayout layout = VK_NULL_HANDLE;
        const VkResult result = create(self.device, &info, nullptr, &layout);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkCreatePipelineLayout failed: " +
                                std::to_string(result);
            return false;
        }
        *out = toNative(layout);
        return true;
    }

    static void destroyPipelineLayout(void* opaque, NativeHandle handle) {
        auto& self = *static_cast<Impl*>(opaque);
        auto destroy = self.deviceProc<PFN_vkDestroyPipelineLayout>(
            "vkDestroyPipelineLayout");
        if (destroy && handle)
            destroy(self.device, fromNative<VkPipelineLayout>(handle), nullptr);
    }

    static bool createComputePipeline(void* opaque, NativeHandle shaderHandle,
                                      NativeHandle layoutHandle, NativeHandle* out,
                                      std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto create = self.deviceProc<PFN_vkCreateComputePipelines>(
            "vkCreateComputePipelines");
        if (!create || !out) {
            if (error) *error = "vkCreateComputePipelines unavailable";
            return false;
        }
        VkPipelineShaderStageCreateInfo stage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = fromNative<VkShaderModule>(shaderHandle);
        stage.pName = "main";

        VkComputePipelineCreateInfo info{
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        info.stage = stage;
        info.layout = fromNative<VkPipelineLayout>(layoutHandle);

        VkPipeline pipeline = VK_NULL_HANDLE;
        const VkResult result = create(self.device, VK_NULL_HANDLE, 1,
                                       &info, nullptr, &pipeline);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkCreateComputePipelines failed: " +
                                std::to_string(result);
            return false;
        }
        *out = toNative(pipeline);
        return true;
    }

    static void destroyComputePipeline(void* opaque, NativeHandle handle) {
        auto& self = *static_cast<Impl*>(opaque);
        auto destroy = self.deviceProc<PFN_vkDestroyPipeline>("vkDestroyPipeline");
        if (destroy && handle)
            destroy(self.device, fromNative<VkPipeline>(handle), nullptr);
    }




    static bool createRenderPass(
        void* opaque, std::uint32_t format,
        NativeHandle* out, std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto create = self.deviceProc<PFN_vkCreateRenderPass>(
            "vkCreateRenderPass");

        if (!create || !out || self.device == VK_NULL_HANDLE) {
            if (error) *error = "vkCreateRenderPass unavailable or invalid output";
            return false;
        }

        VkAttachmentDescription attachment{};
        attachment.format = static_cast<VkFormat>(format);
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorReference{};
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorReference;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info{
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        const VkResult result =
            create(self.device, &info, nullptr, &renderPass);

        if (result != VK_SUCCESS) {
            if (error) {
                *error = "vkCreateRenderPass failed: " +
                         std::to_string(result);
            }
            return false;
        }

        *out = toNative(renderPass);
        return true;
    }

    static void destroyRenderPass(void* opaque, NativeHandle handle) {
        auto& self = *static_cast<Impl*>(opaque);
        auto destroy = self.deviceProc<PFN_vkDestroyRenderPass>(
            "vkDestroyRenderPass");

        if (destroy && self.device != VK_NULL_HANDLE && handle) {
            destroy(
                self.device,
                fromNative<VkRenderPass>(handle),
                nullptr);
        }
    }

    static bool createGraphicsPipeline(
        void* opaque, NativeHandle vertexHandle, NativeHandle fragmentHandle,
        NativeHandle layoutHandle, NativeHandle renderPassHandle,
        NativeHandle* out, std::string* error) {
        auto& self = *static_cast<Impl*>(opaque);
        auto create = self.deviceProc<PFN_vkCreateGraphicsPipelines>(
            "vkCreateGraphicsPipelines");

        if (!create || !out || !vertexHandle || !fragmentHandle ||
            !layoutHandle || !renderPassHandle) {
            if (error) *error = "Invalid graphics pipeline handles or vkCreateGraphicsPipelines unavailable";
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2]{
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}
        };
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = fromNative<VkShaderModule>(vertexHandle);
        stages[0].pName = "main";
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fromNative<VkShaderModule>(fragmentHandle);
        stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertexInput{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

        VkPipelineInputAssemblyStateCreateInfo assembly{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0F;

        VkPipelineMultisampleStateCreateInfo multisample{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blend{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &attachment;

        const VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamic{
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;

        VkGraphicsPipelineCreateInfo info{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = fromNative<VkPipelineLayout>(layoutHandle);
        info.renderPass = fromNative<VkRenderPass>(renderPassHandle);
        info.subpass = 0;

        VkPipeline pipeline = VK_NULL_HANDLE;
        const VkResult result = create(
            self.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
        if (result != VK_SUCCESS) {
            if (error) *error = "vkCreateGraphicsPipelines failed: " +
                                std::to_string(result);
            return false;
        }

        *out = toNative(pipeline);
        return true;
    }

    static void destroyGraphicsPipeline(void* opaque, NativeHandle handle) {
        destroyComputePipeline(opaque, handle);
    }

    static bool updateStorageBufferDescriptor(
        void* opaque, NativeHandle setHandle, NativeHandle bufferHandle,
        std::uint64_t range, std::string* error) {

        auto& self = *static_cast<Impl*>(opaque);
        auto update = self.deviceProc<PFN_vkUpdateDescriptorSets>(
            "vkUpdateDescriptorSets");

        if (!update || !setHandle || !bufferHandle || range == 0U) {
            if (error) *error = "Invalid storage-buffer descriptor update request";
            return false;
        }

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = fromNative<VkBuffer>(bufferHandle);
        bufferInfo.offset = 0;
        bufferInfo.range = static_cast<VkDeviceSize>(range);

        VkWriteDescriptorSet write{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = fromNative<VkDescriptorSet>(setHandle);
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &bufferInfo;

        update(self.device, 1, &write, 0, nullptr);
        return true;
    }

    static bool dispatchCompute(
        void* opaque, NativeHandle pipelineHandle,
        NativeHandle layoutHandle, NativeHandle setHandle,
        std::uint32_t groupsX, std::uint32_t groupsY,
        std::uint32_t groupsZ, std::string* error) {

        auto& self = *static_cast<Impl*>(opaque);

        auto createPool = self.deviceProc<PFN_vkCreateCommandPool>(
            "vkCreateCommandPool");
        auto destroyPool = self.deviceProc<PFN_vkDestroyCommandPool>(
            "vkDestroyCommandPool");
        auto allocate = self.deviceProc<PFN_vkAllocateCommandBuffers>(
            "vkAllocateCommandBuffers");
        auto freeBuffers = self.deviceProc<PFN_vkFreeCommandBuffers>(
            "vkFreeCommandBuffers");
        auto begin = self.deviceProc<PFN_vkBeginCommandBuffer>(
            "vkBeginCommandBuffer");
        auto end = self.deviceProc<PFN_vkEndCommandBuffer>(
            "vkEndCommandBuffer");
        auto bindPipeline = self.deviceProc<PFN_vkCmdBindPipeline>(
            "vkCmdBindPipeline");
        auto bindSets = self.deviceProc<PFN_vkCmdBindDescriptorSets>(
            "vkCmdBindDescriptorSets");
        auto dispatch = self.deviceProc<PFN_vkCmdDispatch>(
            "vkCmdDispatch");
        auto createFence = self.deviceProc<PFN_vkCreateFence>(
            "vkCreateFence");
        auto destroyFence = self.deviceProc<PFN_vkDestroyFence>(
            "vkDestroyFence");
        auto submit = self.deviceProc<PFN_vkQueueSubmit>(
            "vkQueueSubmit");
        auto wait = self.deviceProc<PFN_vkWaitForFences>(
            "vkWaitForFences");
        auto waitIdle = self.deviceProc<PFN_vkDeviceWaitIdle>(
            "vkDeviceWaitIdle");

        if (!createPool || !destroyPool || !allocate || !freeBuffers ||
            !begin || !end || !bindPipeline || !bindSets || !dispatch ||
            !createFence || !destroyFence || !submit || !wait ||
            !pipelineHandle || !layoutHandle || !setHandle ||
            groupsX == 0U || groupsY == 0U || groupsZ == 0U) {
            if (error) *error = "Compute dispatch entry points or arguments are invalid";
            return false;
        }

        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer command = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkResult result = VK_SUCCESS;
        bool submitted = false;

        VkCommandPoolCreateInfo poolInfo{
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = self.queue_family;

        result = createPool(self.device, &poolInfo, nullptr, &pool);

        if (result == VK_SUCCESS) {
            VkCommandBufferAllocateInfo allocInfo{
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocInfo.commandPool = pool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            result = allocate(self.device, &allocInfo, &command);
        }

        if (result == VK_SUCCESS) {
            VkCommandBufferBeginInfo beginInfo{
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = begin(command, &beginInfo);
        }

        if (result == VK_SUCCESS) {
            bindPipeline(
                command, VK_PIPELINE_BIND_POINT_COMPUTE,
                fromNative<VkPipeline>(pipelineHandle));

            VkDescriptorSet set =
                fromNative<VkDescriptorSet>(setHandle);

            bindSets(
                command, VK_PIPELINE_BIND_POINT_COMPUTE,
                fromNative<VkPipelineLayout>(layoutHandle),
                0, 1, &set, 0, nullptr);

            dispatch(command, groupsX, groupsY, groupsZ);
            result = end(command);
        }

        if (result == VK_SUCCESS) {
            VkFenceCreateInfo fenceInfo{
                VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            result = createFence(self.device, &fenceInfo, nullptr, &fence);
        }

        if (result == VK_SUCCESS) {
            VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &command;
            result = submit(self.queue, 1, &submitInfo, fence);
            submitted = result == VK_SUCCESS;
        }

        if (result == VK_SUCCESS) {
            result = wait(self.device, 1, &fence, VK_TRUE, UINT64_MAX);
        }

        if (result != VK_SUCCESS && submitted && waitIdle) {
            waitIdle(self.device);
        }

        if (fence != VK_NULL_HANDLE)
            destroyFence(self.device, fence, nullptr);

        if (command != VK_NULL_HANDLE && pool != VK_NULL_HANDLE)
            freeBuffers(self.device, pool, 1, &command);

        if (pool != VK_NULL_HANDLE)
            destroyPool(self.device, pool, nullptr);

        if (result != VK_SUCCESS) {
            if (error)
                *error = "Vulkan compute dispatch/submission failed: " +
                         std::to_string(result);
            return false;
        }

        return true;
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

VulkanBufferDispatch VulkanLoaderAdapter::buffer_dispatch() noexcept {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    return {
        Impl::createBuffer,
        Impl::allocateBufferMemory,
        Impl::bindBufferMemory,
        Impl::destroyBuffer,
        Impl::freeBufferMemory,
        impl_.get(),
        Impl::mapBufferMemory,
        Impl::unmapBufferMemory,
        Impl::flushBufferMemory,
        Impl::invalidateBufferMemory
    };
#else
    return {};
#endif
}
std::vector<VulkanBufferMemoryType> VulkanLoaderAdapter::memory_types() const {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    return impl_ ? impl_->queryMemoryTypes() : std::vector<VulkanBufferMemoryType>{};
#else
    return {};
#endif
}
VulkanComputePipelineDispatch VulkanLoaderAdapter::compute_dispatch() noexcept {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    return {
        Impl::createShaderModule,
        Impl::destroyShaderModule,
        Impl::createDescriptorSetLayout,
        Impl::destroyDescriptorSetLayout,
        Impl::createDescriptorPool,
        Impl::destroyDescriptorPool,
        Impl::allocateDescriptorSet,
        Impl::createPipelineLayout,
        Impl::destroyPipelineLayout,
        Impl::createComputePipeline,
        Impl::destroyComputePipeline,
        Impl::updateStorageBufferDescriptor,
        Impl::dispatchCompute,
        impl_.get()
    };
#else
    return {};
#endif
}

bool VulkanLoaderAdapter::create_color_render_pass(
    std::uint32_t format, NativeHandle* out, std::string* error) {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if (!impl_ || !impl_->device) {
        if (error) *error = "Vulkan device is not initialized";
        return false;
    }
    return Impl::createRenderPass(impl_.get(), format, out, error);
#else
    (void)format;
    (void)out;
    if (error) *error = "Vulkan support is not compiled in";
    return false;
#endif
}

void VulkanLoaderAdapter::destroy_render_pass(
    NativeHandle render_pass) noexcept {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if (impl_ && impl_->device && render_pass) {
        Impl::destroyRenderPass(impl_.get(), render_pass);
    }
#else
    (void)render_pass;
#endif
}


bool VulkanLoaderAdapter::create_color_image(
    std::uint32_t width, std::uint32_t height, std::uint32_t format,
    std::uint32_t usage, NativeHandle* image,
    VulkanBufferMemoryRequirements* requirements, std::string* error) {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if (!impl_ || !impl_->device) {
        if (error) *error = "Vulkan device is not initialized";
        return false;
    }
    return Impl::createColorImage(impl_.get(), width, height, format, usage,
                                  image, requirements, error);
#else
    (void)width; (void)height; (void)format; (void)usage;
    (void)image; (void)requirements;
    if (error) *error = "Vulkan support is not compiled in";
    return false;
#endif
}

bool VulkanLoaderAdapter::bind_image_memory(
    NativeHandle image, NativeHandle memory, std::string* error) {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if (!impl_ || !impl_->device) {
        if (error) *error = "Vulkan device is not initialized";
        return false;
    }
    return Impl::bindImageMemory(impl_.get(), image, memory, error);
#else
    (void)image; (void)memory;
    if (error) *error = "Vulkan support is not compiled in";
    return false;
#endif
}

void VulkanLoaderAdapter::destroy_image(NativeHandle image) noexcept {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if (impl_ && impl_->device && image) Impl::destroyImage(impl_.get(), image);
#else
    (void)image;
#endif
}

bool VulkanLoaderAdapter::create_image_view(
    NativeHandle image, std::uint32_t format, NativeHandle* view,
    std::string* error) {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if (!impl_ || !impl_->device) {
        if (error) *error = "Vulkan device is not initialized";
        return false;
    }
    return Impl::createImageView(impl_.get(), image, format, view, error);
#else
    (void)image; (void)format; (void)view;
    if (error) *error = "Vulkan support is not compiled in";
    return false;
#endif
}

void VulkanLoaderAdapter::destroy_image_view(NativeHandle view) noexcept {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if (impl_ && impl_->device && view) Impl::destroyImageView(impl_.get(), view);
#else
    (void)view;
#endif
}

bool VulkanLoaderAdapter::create_framebuffer(
    NativeHandle renderPass, NativeHandle imageView, std::uint32_t width,
    std::uint32_t height, NativeHandle* framebuffer, std::string* error) {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if (!impl_ || !impl_->device) {
        if (error) *error = "Vulkan device is not initialized";
        return false;
    }
    return Impl::createFramebuffer(impl_.get(), renderPass, imageView,
                                   width, height, framebuffer, error);
#else
    (void)renderPass; (void)imageView; (void)width; (void)height; (void)framebuffer;
    if (error) *error = "Vulkan support is not compiled in";
    return false;
#endif
}

void VulkanLoaderAdapter::destroy_framebuffer(NativeHandle framebuffer) noexcept {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    if (impl_ && impl_->device && framebuffer)
        Impl::destroyFramebuffer(impl_.get(), framebuffer);
#else
    (void)framebuffer;
#endif
}

VulkanGraphicsPipelineDispatch VulkanLoaderAdapter::graphics_dispatch() noexcept {
#if defined(NYAMIST_RUNTIME_HAS_VULKAN)
    return {
        Impl::createShaderModule,
        Impl::destroyShaderModule,
        Impl::createPipelineLayout,
        Impl::destroyPipelineLayout,
        Impl::createGraphicsPipeline,
        Impl::destroyGraphicsPipeline,
        impl_.get()
    };
#else
    return {};
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
