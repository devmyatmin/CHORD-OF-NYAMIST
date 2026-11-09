#include "nyamist/discovery/hardware_discovery.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

#if defined(__unix__) || defined(__APPLE__) || defined(__ANDROID__)
#include <sys/utsname.h>
#include <unistd.h>
#endif

#if defined(NYAMIST_HAS_VULKAN)
#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
#include <dlfcn.h>
#endif
#endif

namespace nyamist::discovery {
namespace {

void addField(DiscoveryReport& report, std::string key, std::string value,
              std::string source, std::string confidence = "direct") {
    if (!value.empty()) {
        report.device_fields.push_back({
            std::move(key), {std::move(value), std::move(source), std::move(confidence)}
        });
    }
}

std::string utcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::string readTextFile(const char* path) {
    std::ifstream input(path);
    if (!input) return {};
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

std::string readAndroidProperty(const char* name) {
#if defined(__ANDROID__)
    char value[PROP_VALUE_MAX]{};
    const int length = __system_property_get(name, value);
    return length > 0 ? std::string(value, static_cast<std::size_t>(length)) : std::string{};
#else
    (void)name;
    return {};
#endif
}

std::string architectureName() {
#if defined(__aarch64__)
    return "arm64-v8a";
#elif defined(__arm__)
    return "armeabi-v7a";
#elif defined(__x86_64__)
    return "x86_64";
#elif defined(__i386__)
    return "x86";
#else
    return {};
#endif
}

void collectPlatformFacts(DiscoveryReport& report) {
    report.captured_at_utc = utcTimestamp();
#if defined(__ANDROID__)
    report.platform = "android";
    addField(report, "manufacturer", readAndroidProperty("ro.product.manufacturer"),
             "Android system property: ro.product.manufacturer");
    addField(report, "brand", readAndroidProperty("ro.product.brand"),
             "Android system property: ro.product.brand");
    addField(report, "model", readAndroidProperty("ro.product.model"),
             "Android system property: ro.product.model");
    addField(report, "soc_model", readAndroidProperty("ro.soc.model"),
             "Android system property: ro.soc.model");
    addField(report, "hardware", readAndroidProperty("ro.hardware"),
             "Android system property: ro.hardware");
    addField(report, "board_platform", readAndroidProperty("ro.board.platform"),
             "Android system property: ro.board.platform");
    addField(report, "android_release", readAndroidProperty("ro.build.version.release"),
             "Android system property: ro.build.version.release");
    addField(report, "android_api", readAndroidProperty("ro.build.version.sdk"),
             "Android system property: ro.build.version.sdk");
    addField(report, "primary_abi", readAndroidProperty("ro.product.cpu.abi"),
             "Android system property: ro.product.cpu.abi");
    addField(report, "abi_list", readAndroidProperty("ro.product.cpu.abilist"),
             "Android system property: ro.product.cpu.abilist");
#elif defined(__linux__)
    report.platform = "linux";
#elif defined(_WIN32)
    report.platform = "windows";
#else
    report.platform = "unknown";
#endif

    addField(report, "compiled_abi", architectureName(), "Compiler target architecture");
#if defined(__unix__) || defined(__APPLE__) || defined(__ANDROID__)
    struct utsname system_info {};
    if (uname(&system_info) == 0) {
        addField(report, "kernel_release", system_info.release, "uname(2): release");
        addField(report, "system_name", system_info.sysname, "uname(2): sysname");
        addField(report, "machine", system_info.machine, "uname(2): machine");
    }
    const long processors = sysconf(_SC_NPROCESSORS_ONLN);
    if (processors > 0) {
        addField(report, "logical_cpu_count", std::to_string(processors),
                 "sysconf(_SC_NPROCESSORS_ONLN)");
    }
#endif

    const std::string meminfo = readTextFile("/proc/meminfo");
    if (!meminfo.empty()) {
        std::istringstream lines(meminfo);
        std::string line;
        unsigned long long total_kib = 0ULL;
        unsigned long long available_kib = 0ULL;
        while (std::getline(lines, line)) {
            std::istringstream row(line);
            std::string key;
            unsigned long long value = 0ULL;
            std::string unit;
            if (!(row >> key >> value >> unit) || unit != "kB") continue;
            if (key == "MemTotal:") total_kib = value;
            else if (key == "MemAvailable:") available_kib = value;
        }
        constexpr unsigned long long kib = 1024ULL;
        if (total_kib > 0ULL &&
            total_kib <= std::numeric_limits<unsigned long long>::max() / kib &&
            available_kib > 0ULL && available_kib <= total_kib) {
            addField(report, "total_memory_bytes", std::to_string(total_kib * kib),
                     "/proc/meminfo: MemTotal");
            addField(report, "available_memory_bytes", std::to_string(available_kib * kib),
                     "/proc/meminfo: MemAvailable");
        }
    }
}

std::string jsonEscape(std::string_view value) {
    std::ostringstream out;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20U) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch) << std::dec;
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    return out.str();
}

void writeQuoted(std::ostringstream& out, std::string_view value) {
    out << '"' << jsonEscape(value) << '"';
}

std::string hexValue(unsigned int value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::nouppercase << value;
    return out.str();
}

#if defined(NYAMIST_HAS_VULKAN)
std::string versionString(unsigned int version) {
    std::ostringstream out;
    out << VK_VERSION_MAJOR(version) << '.'
        << VK_VERSION_MINOR(version) << '.'
        << VK_VERSION_PATCH(version);
    return out.str();
}

std::string deviceTypeName(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated_gpu";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete_gpu";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual_gpu";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    default: return "other";
    }
}



void appendQueueFlagNames(unsigned int flags, std::vector<std::string>& names) {
    if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0U) names.emplace_back("graphics");
    if ((flags & VK_QUEUE_COMPUTE_BIT) != 0U) names.emplace_back("compute");
    if ((flags & VK_QUEUE_TRANSFER_BIT) != 0U) names.emplace_back("transfer");
    if ((flags & VK_QUEUE_SPARSE_BINDING_BIT) != 0U) names.emplace_back("sparse_binding");
#if defined(VK_VERSION_1_1)
    if ((flags & VK_QUEUE_PROTECTED_BIT) != 0U) names.emplace_back("protected");
#endif
}

void appendMemoryPropertyNames(unsigned int flags, std::vector<std::string>& names) {
    if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0U) names.emplace_back("device_local");
    if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0U) names.emplace_back("host_visible");
    if ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0U) names.emplace_back("host_coherent");
    if ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0U) names.emplace_back("host_cached");
    if ((flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0U) names.emplace_back("lazily_allocated");
#if defined(VK_VERSION_1_1)
    if ((flags & VK_MEMORY_PROPERTY_PROTECTED_BIT) != 0U) names.emplace_back("protected");
#endif
}

void appendFormatFeatureNames(
    unsigned int flags, std::vector<std::string>& names) {
#define NYAMIST_FORMAT_FLAG(bit, label) \
    if ((flags & static_cast<unsigned int>(bit)) != 0U) names.emplace_back(label)
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "sampled_image");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT, "storage_image");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT, "storage_image_atomic");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT, "uniform_texel_buffer");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT, "storage_texel_buffer");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT, "storage_texel_buffer_atomic");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT, "vertex_buffer");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT, "color_attachment");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT, "color_attachment_blend");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, "depth_stencil_attachment");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_BLIT_SRC_BIT, "blit_src");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_BLIT_DST_BIT, "blit_dst");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, "sampled_image_filter_linear");
#if defined(VK_VERSION_1_1)
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_TRANSFER_SRC_BIT, "transfer_src");
    NYAMIST_FORMAT_FLAG(VK_FORMAT_FEATURE_TRANSFER_DST_BIT, "transfer_dst");
#endif
#undef NYAMIST_FORMAT_FLAG
}

void addFeature(
    VulkanDeviceObservation& device, const char* name, const char* api,
    VkBool32 supported) {
    device.features.push_back({name, api, supported == VK_TRUE});
}

void collectCore10Features(
    VulkanDeviceObservation& device, const VkPhysicalDeviceFeatures& f) {
#define NYAMIST_ADD_FEATURE(field) addFeature(device, #field, "1.0", f.field)
    NYAMIST_ADD_FEATURE(robustBufferAccess);
    NYAMIST_ADD_FEATURE(fullDrawIndexUint32);
    NYAMIST_ADD_FEATURE(imageCubeArray);
    NYAMIST_ADD_FEATURE(independentBlend);
    NYAMIST_ADD_FEATURE(geometryShader);
    NYAMIST_ADD_FEATURE(tessellationShader);
    NYAMIST_ADD_FEATURE(sampleRateShading);
    NYAMIST_ADD_FEATURE(dualSrcBlend);
    NYAMIST_ADD_FEATURE(logicOp);
    NYAMIST_ADD_FEATURE(multiDrawIndirect);
    NYAMIST_ADD_FEATURE(drawIndirectFirstInstance);
    NYAMIST_ADD_FEATURE(depthClamp);
    NYAMIST_ADD_FEATURE(depthBiasClamp);
    NYAMIST_ADD_FEATURE(fillModeNonSolid);
    NYAMIST_ADD_FEATURE(depthBounds);
    NYAMIST_ADD_FEATURE(wideLines);
    NYAMIST_ADD_FEATURE(largePoints);
    NYAMIST_ADD_FEATURE(alphaToOne);
    NYAMIST_ADD_FEATURE(multiViewport);
    NYAMIST_ADD_FEATURE(samplerAnisotropy);
    NYAMIST_ADD_FEATURE(textureCompressionETC2);
    NYAMIST_ADD_FEATURE(textureCompressionASTC_LDR);
    NYAMIST_ADD_FEATURE(textureCompressionBC);
    NYAMIST_ADD_FEATURE(occlusionQueryPrecise);
    NYAMIST_ADD_FEATURE(pipelineStatisticsQuery);
    NYAMIST_ADD_FEATURE(vertexPipelineStoresAndAtomics);
    NYAMIST_ADD_FEATURE(fragmentStoresAndAtomics);
    NYAMIST_ADD_FEATURE(shaderTessellationAndGeometryPointSize);
    NYAMIST_ADD_FEATURE(shaderImageGatherExtended);
    NYAMIST_ADD_FEATURE(shaderStorageImageExtendedFormats);
    NYAMIST_ADD_FEATURE(shaderStorageImageMultisample);
    NYAMIST_ADD_FEATURE(shaderStorageImageReadWithoutFormat);
    NYAMIST_ADD_FEATURE(shaderStorageImageWriteWithoutFormat);
    NYAMIST_ADD_FEATURE(shaderUniformBufferArrayDynamicIndexing);
    NYAMIST_ADD_FEATURE(shaderSampledImageArrayDynamicIndexing);
    NYAMIST_ADD_FEATURE(shaderStorageBufferArrayDynamicIndexing);
    NYAMIST_ADD_FEATURE(shaderStorageImageArrayDynamicIndexing);
    NYAMIST_ADD_FEATURE(shaderClipDistance);
    NYAMIST_ADD_FEATURE(shaderCullDistance);
    NYAMIST_ADD_FEATURE(shaderFloat64);
    NYAMIST_ADD_FEATURE(shaderInt64);
    NYAMIST_ADD_FEATURE(shaderInt16);
    NYAMIST_ADD_FEATURE(shaderResourceResidency);
    NYAMIST_ADD_FEATURE(shaderResourceMinLod);
    NYAMIST_ADD_FEATURE(sparseBinding);
    NYAMIST_ADD_FEATURE(sparseResidencyBuffer);
    NYAMIST_ADD_FEATURE(sparseResidencyImage2D);
    NYAMIST_ADD_FEATURE(sparseResidencyImage3D);
    NYAMIST_ADD_FEATURE(sparseResidency2Samples);
    NYAMIST_ADD_FEATURE(sparseResidency4Samples);
    NYAMIST_ADD_FEATURE(sparseResidency8Samples);
    NYAMIST_ADD_FEATURE(sparseResidency16Samples);
    NYAMIST_ADD_FEATURE(sparseResidencyAliased);
    NYAMIST_ADD_FEATURE(variableMultisampleRate);
    NYAMIST_ADD_FEATURE(inheritedQueries);
#undef NYAMIST_ADD_FEATURE
}

#if defined(VK_VERSION_1_1)
void collectCore11Features(
    VulkanDeviceObservation& device, const VkPhysicalDeviceVulkan11Features& f) {
#define NYAMIST_ADD_FEATURE(field) addFeature(device, #field, "1.1", f.field)
    NYAMIST_ADD_FEATURE(storageBuffer16BitAccess);
    NYAMIST_ADD_FEATURE(uniformAndStorageBuffer16BitAccess);
    NYAMIST_ADD_FEATURE(storagePushConstant16);
    NYAMIST_ADD_FEATURE(storageInputOutput16);
    NYAMIST_ADD_FEATURE(multiview);
    NYAMIST_ADD_FEATURE(multiviewGeometryShader);
    NYAMIST_ADD_FEATURE(multiviewTessellationShader);
    NYAMIST_ADD_FEATURE(variablePointersStorageBuffer);
    NYAMIST_ADD_FEATURE(variablePointers);
    NYAMIST_ADD_FEATURE(protectedMemory);
    NYAMIST_ADD_FEATURE(samplerYcbcrConversion);
    NYAMIST_ADD_FEATURE(shaderDrawParameters);
#undef NYAMIST_ADD_FEATURE
}
#endif

#if defined(VK_VERSION_1_2)
void collectCore12Features(
    VulkanDeviceObservation& device, const VkPhysicalDeviceVulkan12Features& f) {
#define NYAMIST_ADD_FEATURE(field) addFeature(device, #field, "1.2", f.field)
    NYAMIST_ADD_FEATURE(samplerMirrorClampToEdge);
    NYAMIST_ADD_FEATURE(drawIndirectCount);
    NYAMIST_ADD_FEATURE(storageBuffer8BitAccess);
    NYAMIST_ADD_FEATURE(uniformAndStorageBuffer8BitAccess);
    NYAMIST_ADD_FEATURE(storagePushConstant8);
    NYAMIST_ADD_FEATURE(shaderBufferInt64Atomics);
    NYAMIST_ADD_FEATURE(shaderSharedInt64Atomics);
    NYAMIST_ADD_FEATURE(shaderFloat16);
    NYAMIST_ADD_FEATURE(shaderInt8);
    NYAMIST_ADD_FEATURE(descriptorIndexing);
    NYAMIST_ADD_FEATURE(shaderInputAttachmentArrayDynamicIndexing);
    NYAMIST_ADD_FEATURE(shaderUniformTexelBufferArrayDynamicIndexing);
    NYAMIST_ADD_FEATURE(shaderStorageTexelBufferArrayDynamicIndexing);
    NYAMIST_ADD_FEATURE(shaderUniformBufferArrayNonUniformIndexing);
    NYAMIST_ADD_FEATURE(shaderSampledImageArrayNonUniformIndexing);
    NYAMIST_ADD_FEATURE(shaderStorageBufferArrayNonUniformIndexing);
    NYAMIST_ADD_FEATURE(shaderStorageImageArrayNonUniformIndexing);
    NYAMIST_ADD_FEATURE(shaderInputAttachmentArrayNonUniformIndexing);
    NYAMIST_ADD_FEATURE(shaderUniformTexelBufferArrayNonUniformIndexing);
    NYAMIST_ADD_FEATURE(shaderStorageTexelBufferArrayNonUniformIndexing);
    NYAMIST_ADD_FEATURE(descriptorBindingUniformBufferUpdateAfterBind);
    NYAMIST_ADD_FEATURE(descriptorBindingSampledImageUpdateAfterBind);
    NYAMIST_ADD_FEATURE(descriptorBindingStorageImageUpdateAfterBind);
    NYAMIST_ADD_FEATURE(descriptorBindingStorageBufferUpdateAfterBind);
    NYAMIST_ADD_FEATURE(descriptorBindingUniformTexelBufferUpdateAfterBind);
    NYAMIST_ADD_FEATURE(descriptorBindingStorageTexelBufferUpdateAfterBind);
    NYAMIST_ADD_FEATURE(descriptorBindingUpdateUnusedWhilePending);
    NYAMIST_ADD_FEATURE(descriptorBindingPartiallyBound);
    NYAMIST_ADD_FEATURE(descriptorBindingVariableDescriptorCount);
    NYAMIST_ADD_FEATURE(runtimeDescriptorArray);
    NYAMIST_ADD_FEATURE(samplerFilterMinmax);
    NYAMIST_ADD_FEATURE(scalarBlockLayout);
    NYAMIST_ADD_FEATURE(imagelessFramebuffer);
    NYAMIST_ADD_FEATURE(uniformBufferStandardLayout);
    NYAMIST_ADD_FEATURE(shaderSubgroupExtendedTypes);
    NYAMIST_ADD_FEATURE(separateDepthStencilLayouts);
    NYAMIST_ADD_FEATURE(hostQueryReset);
    NYAMIST_ADD_FEATURE(timelineSemaphore);
    NYAMIST_ADD_FEATURE(bufferDeviceAddress);
    NYAMIST_ADD_FEATURE(bufferDeviceAddressCaptureReplay);
    NYAMIST_ADD_FEATURE(bufferDeviceAddressMultiDevice);
    NYAMIST_ADD_FEATURE(vulkanMemoryModel);
    NYAMIST_ADD_FEATURE(vulkanMemoryModelDeviceScope);
    NYAMIST_ADD_FEATURE(vulkanMemoryModelAvailabilityVisibilityChains);
    NYAMIST_ADD_FEATURE(shaderOutputViewportIndex);
    NYAMIST_ADD_FEATURE(shaderOutputLayer);
    NYAMIST_ADD_FEATURE(subgroupBroadcastDynamicId);
#undef NYAMIST_ADD_FEATURE
}
#endif

#if defined(VK_VERSION_1_3)
void collectCore13Features(
    VulkanDeviceObservation& device, const VkPhysicalDeviceVulkan13Features& f) {
#define NYAMIST_ADD_FEATURE(field) addFeature(device, #field, "1.3", f.field)
    NYAMIST_ADD_FEATURE(robustImageAccess);
    NYAMIST_ADD_FEATURE(inlineUniformBlock);
    NYAMIST_ADD_FEATURE(descriptorBindingInlineUniformBlockUpdateAfterBind);
    NYAMIST_ADD_FEATURE(pipelineCreationCacheControl);
    NYAMIST_ADD_FEATURE(privateData);
    NYAMIST_ADD_FEATURE(shaderDemoteToHelperInvocation);
    NYAMIST_ADD_FEATURE(shaderTerminateInvocation);
    NYAMIST_ADD_FEATURE(subgroupSizeControl);
    NYAMIST_ADD_FEATURE(computeFullSubgroups);
    NYAMIST_ADD_FEATURE(synchronization2);
    NYAMIST_ADD_FEATURE(textureCompressionASTC_HDR);
    NYAMIST_ADD_FEATURE(shaderZeroInitializeWorkgroupMemory);
    NYAMIST_ADD_FEATURE(dynamicRendering);
    NYAMIST_ADD_FEATURE(shaderIntegerDotProduct);
    NYAMIST_ADD_FEATURE(maintenance4);
#undef NYAMIST_ADD_FEATURE
}
#endif

void collectFeatureInventory(
    VulkanDispatch& dispatch, VkPhysicalDevice physical_device,
    const VkPhysicalDeviceProperties& properties, VulkanDeviceObservation& device) {
    const std::uint32_t api_version = properties.apiVersion;
    if (api_version < VK_API_VERSION_1_1) {
        VkPhysicalDeviceFeatures features{};
        dispatch.get_physical_device_features(physical_device, &features);
        collectCore10Features(device, features);
        device.feature_query_api_max = "1.0";
        device.feature_query_status = "success";
        return;
    }

#if defined(VK_VERSION_1_1)
    if (dispatch.get_physical_device_features2 == nullptr) {
        device.feature_query_status = "failed";
        device.feature_query_api_max = "1.0";
        device.feature_query_failure_reason =
            "vkGetPhysicalDeviceFeatures2 is unavailable for a Vulkan 1.1+ physical device";
        return;
    }

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    VkPhysicalDeviceVulkan11Features features11{};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features2.pNext = &features11;

#if defined(VK_VERSION_1_2)
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    if (api_version >= VK_API_VERSION_1_2) features11.pNext = &features12;
#endif
#if defined(VK_VERSION_1_3)
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
#if defined(VK_VERSION_1_2)
    if (api_version >= VK_API_VERSION_1_3) features12.pNext = &features13;
#else
    (void)features13;
#endif
#endif

    dispatch.get_physical_device_features2(physical_device, &features2);
    collectCore10Features(device, features2.features);
    collectCore11Features(device, features11);
    device.feature_query_api_max = "1.1";
#if defined(VK_VERSION_1_2)
    if (api_version >= VK_API_VERSION_1_2) {
        collectCore12Features(device, features12);
        device.feature_query_api_max = "1.2";
    }
#endif
#if defined(VK_VERSION_1_3) && defined(VK_VERSION_1_2)
    if (api_version >= VK_API_VERSION_1_3) {
        collectCore13Features(device, features13);
        device.feature_query_api_max = "1.3";
    }
#endif
    device.feature_query_status = "success";
#else
    device.feature_query_status = "partial";
    device.feature_query_api_max = "1.0";
    device.feature_query_failure_reason =
        "Build Vulkan headers do not expose Vulkan 1.1 feature structures";
    VkPhysicalDeviceFeatures features{};
    dispatch.get_physical_device_features(physical_device, &features);
    collectCore10Features(device, features);
#endif
}

void addLimit(
    VulkanDeviceObservation& device, const char* name, unsigned long long value) {
    device.limits.push_back({name, value});
}

void collectLimitInventory(
    const VkPhysicalDeviceLimits& l, VulkanDeviceObservation& device) {
#define NYAMIST_ADD_LIMIT(field) addLimit(device, #field, static_cast<unsigned long long>(l.field))
    NYAMIST_ADD_LIMIT(maxImageDimension1D);
    NYAMIST_ADD_LIMIT(maxImageDimension2D);
    NYAMIST_ADD_LIMIT(maxImageDimension3D);
    NYAMIST_ADD_LIMIT(maxImageDimensionCube);
    NYAMIST_ADD_LIMIT(maxImageArrayLayers);
    NYAMIST_ADD_LIMIT(maxTexelBufferElements);
    NYAMIST_ADD_LIMIT(maxUniformBufferRange);
    NYAMIST_ADD_LIMIT(maxStorageBufferRange);
    NYAMIST_ADD_LIMIT(maxPushConstantsSize);
    NYAMIST_ADD_LIMIT(maxMemoryAllocationCount);
    NYAMIST_ADD_LIMIT(maxSamplerAllocationCount);
    NYAMIST_ADD_LIMIT(maxBoundDescriptorSets);
    NYAMIST_ADD_LIMIT(maxPerStageDescriptorSamplers);
    NYAMIST_ADD_LIMIT(maxPerStageDescriptorUniformBuffers);
    NYAMIST_ADD_LIMIT(maxPerStageDescriptorStorageBuffers);
    NYAMIST_ADD_LIMIT(maxPerStageDescriptorSampledImages);
    NYAMIST_ADD_LIMIT(maxPerStageDescriptorStorageImages);
    NYAMIST_ADD_LIMIT(maxPerStageDescriptorInputAttachments);
    NYAMIST_ADD_LIMIT(maxPerStageResources);
    NYAMIST_ADD_LIMIT(maxDescriptorSetSamplers);
    NYAMIST_ADD_LIMIT(maxDescriptorSetUniformBuffers);
    NYAMIST_ADD_LIMIT(maxDescriptorSetUniformBuffersDynamic);
    NYAMIST_ADD_LIMIT(maxDescriptorSetStorageBuffers);
    NYAMIST_ADD_LIMIT(maxDescriptorSetStorageBuffersDynamic);
    NYAMIST_ADD_LIMIT(maxDescriptorSetSampledImages);
    NYAMIST_ADD_LIMIT(maxDescriptorSetStorageImages);
    NYAMIST_ADD_LIMIT(maxVertexInputAttributes);
    NYAMIST_ADD_LIMIT(maxVertexInputBindings);
    NYAMIST_ADD_LIMIT(maxVertexInputAttributeOffset);
    NYAMIST_ADD_LIMIT(maxVertexInputBindingStride);
    NYAMIST_ADD_LIMIT(maxVertexOutputComponents);
    NYAMIST_ADD_LIMIT(maxFragmentInputComponents);
    NYAMIST_ADD_LIMIT(maxFragmentOutputAttachments);
    NYAMIST_ADD_LIMIT(maxFragmentDualSrcAttachments);
    NYAMIST_ADD_LIMIT(maxFragmentCombinedOutputResources);
    NYAMIST_ADD_LIMIT(maxComputeSharedMemorySize);
    NYAMIST_ADD_LIMIT(maxComputeWorkGroupInvocations);
    NYAMIST_ADD_LIMIT(maxDrawIndexedIndexValue);
    NYAMIST_ADD_LIMIT(maxDrawIndirectCount);
    NYAMIST_ADD_LIMIT(maxFramebufferWidth);
    NYAMIST_ADD_LIMIT(maxFramebufferHeight);
    NYAMIST_ADD_LIMIT(maxFramebufferLayers);
    NYAMIST_ADD_LIMIT(framebufferColorSampleCounts);
    NYAMIST_ADD_LIMIT(framebufferDepthSampleCounts);
    NYAMIST_ADD_LIMIT(framebufferStencilSampleCounts);
    NYAMIST_ADD_LIMIT(framebufferNoAttachmentsSampleCounts);
    NYAMIST_ADD_LIMIT(sampledImageColorSampleCounts);
    NYAMIST_ADD_LIMIT(sampledImageIntegerSampleCounts);
    NYAMIST_ADD_LIMIT(sampledImageDepthSampleCounts);
    NYAMIST_ADD_LIMIT(sampledImageStencilSampleCounts);
    NYAMIST_ADD_LIMIT(storageImageSampleCounts);
    NYAMIST_ADD_LIMIT(minUniformBufferOffsetAlignment);
    NYAMIST_ADD_LIMIT(minStorageBufferOffsetAlignment);
    NYAMIST_ADD_LIMIT(optimalBufferCopyOffsetAlignment);
    NYAMIST_ADD_LIMIT(optimalBufferCopyRowPitchAlignment);
    NYAMIST_ADD_LIMIT(timestampComputeAndGraphics);
#undef NYAMIST_ADD_LIMIT
    for (unsigned int index = 0U; index < 3U; ++index) {
        const std::string suffix = "[" + std::to_string(index) + "]";
        addLimit(device, ("maxComputeWorkGroupCount" + suffix).c_str(),
                 l.maxComputeWorkGroupCount[index]);
        addLimit(device, ("maxComputeWorkGroupSize" + suffix).c_str(),
                 l.maxComputeWorkGroupSize[index]);
        addLimit(device, ("maxViewportDimensions" + suffix).c_str(),
                 l.maxViewportDimensions[index]);
    }
    device.floating_limits.push_back({
        "timestampPeriod_ns", static_cast<double>(l.timestampPeriod)
    });
    device.limits_query_status = "success";
}

std::string resultName(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    default: return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
    }
}

#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
struct VulkanLibraryHandle {
    void* handle{};
    std::string name;

    ~VulkanLibraryHandle() {
        if (handle != nullptr) {
            dlclose(handle);
        }
    }
};
#endif

struct VulkanDispatch {
    PFN_vkGetInstanceProcAddr get_instance_proc_addr{};
    PFN_vkEnumerateInstanceVersion enumerate_instance_version{};
    PFN_vkEnumerateInstanceExtensionProperties enumerate_instance_extensions{};
    PFN_vkCreateInstance create_instance{};
    PFN_vkDestroyInstance destroy_instance{};
    PFN_vkEnumeratePhysicalDevices enumerate_physical_devices{};
    PFN_vkGetPhysicalDeviceProperties get_physical_device_properties{};
    PFN_vkGetPhysicalDeviceProperties2 get_physical_device_properties2{};
    PFN_vkGetPhysicalDeviceFeatures get_physical_device_features{};
#if defined(VK_VERSION_1_1)
    PFN_vkGetPhysicalDeviceFeatures2 get_physical_device_features2{};
#endif
    PFN_vkGetPhysicalDeviceQueueFamilyProperties get_queue_family_properties{};
    PFN_vkEnumerateDeviceExtensionProperties enumerate_device_extensions{};
    PFN_vkGetPhysicalDeviceMemoryProperties get_memory_properties{};
    PFN_vkGetPhysicalDeviceFormatProperties get_format_properties{};
};

bool initializeVulkanDispatch(
    VulkanDispatch& dispatch,
    DiscoveryReport& report
#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
    , VulkanLibraryHandle& library
#endif
) {
#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
    constexpr const char* candidates[] = {
        "libvulkan.so",
        "libvulkan.so.1"
    };
    std::string load_errors;

    for (const char* candidate : candidates) {
        dlerror();
        void* handle = dlopen(candidate, RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            const char* error = dlerror();
            if (!load_errors.empty()) load_errors += "; ";
            load_errors += candidate;
            load_errors += ": ";
            load_errors += error != nullptr ? error : "dlopen failed without diagnostic";
            continue;
        }

        dlerror();
        auto get_instance_proc_addr =
            reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                dlsym(handle, "vkGetInstanceProcAddr"));
        const char* symbol_error = dlerror();

        if (get_instance_proc_addr == nullptr || symbol_error != nullptr) {
            if (!load_errors.empty()) load_errors += "; ";
            load_errors += candidate;
            load_errors += ": ";
            load_errors += symbol_error != nullptr
                ? symbol_error : "vkGetInstanceProcAddr symbol is null";
            dlclose(handle);
            continue;
        }

        library.handle = handle;
        library.name = candidate;
        dispatch.get_instance_proc_addr = get_instance_proc_addr;
        break;
    }

    if (library.handle == nullptr) {
        report.vulkan_failure_reason =
            "Unable to load Vulkan loader at runtime: " +
            (load_errors.empty() ? std::string("no candidate library loaded") : load_errors);
        return false;
    }

    report.vulkan_loader_library = library.name;
    dispatch.enumerate_instance_version =
        reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            dispatch.get_instance_proc_addr(
                VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    dispatch.enumerate_instance_extensions =
        reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
            dispatch.get_instance_proc_addr(
                VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
    dispatch.create_instance =
        reinterpret_cast<PFN_vkCreateInstance>(
            dispatch.get_instance_proc_addr(VK_NULL_HANDLE, "vkCreateInstance"));
#else
    dispatch.get_instance_proc_addr = &vkGetInstanceProcAddr;
    dispatch.enumerate_instance_version = &vkEnumerateInstanceVersion;
    dispatch.enumerate_instance_extensions = &vkEnumerateInstanceExtensionProperties;
    dispatch.create_instance = &vkCreateInstance;
    report.vulkan_loader_library = "linked";
#endif

    if (dispatch.get_instance_proc_addr == nullptr) {
        report.vulkan_failure_reason = "vkGetInstanceProcAddr is unavailable";
        return false;
    }
    if (dispatch.enumerate_instance_extensions == nullptr) {
        report.vulkan_failure_reason =
            "vkEnumerateInstanceExtensionProperties is unavailable";
        return false;
    }
    if (dispatch.create_instance == nullptr) {
        report.vulkan_failure_reason = "vkCreateInstance is unavailable";
        return false;
    }
    return true;
}

bool loadInstanceDispatch(VulkanDispatch& dispatch, VkInstance instance,
                          std::string& missing_function) {
    const auto resolve = [&dispatch, instance](const char* name) {
        return dispatch.get_instance_proc_addr(instance, name);
    };

    dispatch.destroy_instance =
        reinterpret_cast<PFN_vkDestroyInstance>(resolve("vkDestroyInstance"));
    dispatch.enumerate_physical_devices =
        reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
            resolve("vkEnumeratePhysicalDevices"));
    dispatch.get_physical_device_properties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
            resolve("vkGetPhysicalDeviceProperties"));
    dispatch.get_physical_device_properties2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            resolve("vkGetPhysicalDeviceProperties2"));
    dispatch.get_physical_device_features =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(
            resolve("vkGetPhysicalDeviceFeatures"));
#if defined(VK_VERSION_1_1)
    dispatch.get_physical_device_features2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            resolve("vkGetPhysicalDeviceFeatures2"));
#endif
    dispatch.get_queue_family_properties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
            resolve("vkGetPhysicalDeviceQueueFamilyProperties"));
    dispatch.enumerate_device_extensions =
        reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
            resolve("vkEnumerateDeviceExtensionProperties"));
    dispatch.get_memory_properties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
            resolve("vkGetPhysicalDeviceMemoryProperties"));
    dispatch.get_format_properties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
            resolve("vkGetPhysicalDeviceFormatProperties"));

    const std::pair<const char*, bool> required[] = {
        {"vkDestroyInstance", dispatch.destroy_instance != nullptr},
        {"vkEnumeratePhysicalDevices", dispatch.enumerate_physical_devices != nullptr},
        {"vkGetPhysicalDeviceProperties",
         dispatch.get_physical_device_properties != nullptr},
        {"vkGetPhysicalDeviceFeatures",
         dispatch.get_physical_device_features != nullptr},
        {"vkGetPhysicalDeviceQueueFamilyProperties",
         dispatch.get_queue_family_properties != nullptr},
        {"vkEnumerateDeviceExtensionProperties",
         dispatch.enumerate_device_extensions != nullptr},
        {"vkGetPhysicalDeviceMemoryProperties",
         dispatch.get_memory_properties != nullptr},
        {"vkGetPhysicalDeviceFormatProperties",
         dispatch.get_format_properties != nullptr}
    };

    for (const auto& [name, available] : required) {
        if (!available) {
            if (!missing_function.empty()) missing_function += ", ";
            missing_function += name;
        }
    }
    return missing_function.empty();
}

void queryVulkan(DiscoveryReport& report) {
    report.vulkan_compiled = true;
    VulkanDispatch dispatch;
#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
    VulkanLibraryHandle library;
#endif
    if (!initializeVulkanDispatch(
            dispatch, report
#if defined(NYAMIST_VULKAN_DYNAMIC_LOADING)
            , library
#endif
            )) {
        report.vulkan_status = "failed";
        return;
    }

    std::uint32_t loader_version = VK_API_VERSION_1_0;
    if (dispatch.enumerate_instance_version != nullptr) {
        const VkResult version_result =
            dispatch.enumerate_instance_version(&loader_version);
        if (version_result != VK_SUCCESS) {
            report.vulkan_status = "failed";
            report.vulkan_failure_reason =
                "vkEnumerateInstanceVersion: " + resultName(version_result);
            return;
        }
    }
    report.vulkan_loader_api_version = versionString(loader_version);

    std::uint32_t instance_extension_count = 0U;
    VkResult result = dispatch.enumerate_instance_extensions(
        nullptr, &instance_extension_count, nullptr);
    if (result != VK_SUCCESS) {
        report.vulkan_status = "failed";
        report.vulkan_failure_reason =
            "vkEnumerateInstanceExtensionProperties(count): " + resultName(result);
        return;
    }

    if (instance_extension_count > 0U) {
        std::vector<VkExtensionProperties> extensions(instance_extension_count);
        result = dispatch.enumerate_instance_extensions(
            nullptr, &instance_extension_count, extensions.data());
        if (result != VK_SUCCESS) {
            report.vulkan_status = "failed";
            report.vulkan_failure_reason =
                "vkEnumerateInstanceExtensionProperties(data): " + resultName(result);
            return;
        }
        extensions.resize(instance_extension_count);
        for (const auto& extension : extensions) {
            report.vulkan_instance_extensions.emplace_back(extension.extensionName);
        }
        std::sort(report.vulkan_instance_extensions.begin(),
                  report.vulkan_instance_extensions.end());
    }

    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "CHORD OF NYAMIST R1 Hardware Discovery";
    application_info.applicationVersion = 1U;
    application_info.pEngineName = "NYAMIST Discovery";
    application_info.engineVersion = 1U;
    // R1 currently uses core functionality through Vulkan 1.3 at most.
    application_info.apiVersion = std::min(loader_version, VK_API_VERSION_1_3);

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;

    VkInstance instance = VK_NULL_HANDLE;
    const VkResult create_result =
        dispatch.create_instance(&create_info, nullptr, &instance);
    if (create_result != VK_SUCCESS) {
        report.vulkan_status = "failed";
        report.vulkan_failure_reason =
            "vkCreateInstance: " + resultName(create_result);
        return;
    }

    std::string missing_function;
    if (!loadInstanceDispatch(dispatch, instance, missing_function)) {
        report.vulkan_status = "failed";
        report.vulkan_failure_reason =
            "Vulkan instance created, but required entry points are unavailable: " +
            missing_function;
        if (dispatch.destroy_instance != nullptr) {
            dispatch.destroy_instance(instance, nullptr);
        }
        return;
    }

    std::uint32_t device_count = 0U;
    result = dispatch.enumerate_physical_devices(instance, &device_count, nullptr);
    if (result != VK_SUCCESS || device_count == 0U) {
        report.vulkan_status = "failed";
        report.vulkan_failure_reason = result == VK_SUCCESS
            ? "vkEnumeratePhysicalDevices returned zero devices"
            : "vkEnumeratePhysicalDevices(count): " + resultName(result);
        dispatch.destroy_instance(instance, nullptr);
        return;
    }

    std::vector<VkPhysicalDevice> physical_devices(device_count);
    result = dispatch.enumerate_physical_devices(
        instance, &device_count, physical_devices.data());
    if (result != VK_SUCCESS) {
        report.vulkan_status = "failed";
        report.vulkan_failure_reason =
            "vkEnumeratePhysicalDevices(data): " + resultName(result);
        dispatch.destroy_instance(instance, nullptr);
        return;
    }
    physical_devices.resize(device_count);

    for (const VkPhysicalDevice physical_device : physical_devices) {
        VulkanDeviceObservation device;
        VkPhysicalDeviceProperties properties{};
        dispatch.get_physical_device_properties(physical_device, &properties);
        device.name = properties.deviceName;
        device.device_type = deviceTypeName(properties.deviceType);
        device.vendor_id_hex = hexValue(properties.vendorID);
        device.device_id_hex = hexValue(properties.deviceID);
        device.api_version = versionString(properties.apiVersion);
        device.driver_version = properties.driverVersion;

        std::uint32_t extension_count = 0U;
        result = dispatch.enumerate_device_extensions(
            physical_device, nullptr, &extension_count, nullptr);
        if (result != VK_SUCCESS) {
            device.extension_query_status = "failed";
            device.extension_query_failure_reason =
                "vkEnumerateDeviceExtensionProperties(count): " + resultName(result);
        } else if (extension_count > 0U) {
            std::vector<VkExtensionProperties> extensions(extension_count);
            result = dispatch.enumerate_device_extensions(
                physical_device, nullptr, &extension_count, extensions.data());
            if (result != VK_SUCCESS) {
                device.extension_query_status = "failed";
                device.extension_query_failure_reason =
                    "vkEnumerateDeviceExtensionProperties(data): " + resultName(result);
            } else {
                extensions.resize(extension_count);
                for (const auto& extension : extensions) {
                    device.extensions.emplace_back(extension.extensionName);
                }
                std::sort(device.extensions.begin(), device.extensions.end());
                device.extension_query_status = "success";
            }
        } else {
            device.extension_query_status = "success";
        }

        collectFeatureInventory(dispatch, physical_device, properties, device);
        collectLimitInventory(properties.limits, device);

#if defined(VK_KHR_driver_properties)
        const bool supports_driver_properties =
            properties.apiVersion >= VK_API_VERSION_1_2 ||
            std::find(device.extensions.begin(), device.extensions.end(),
                      "VK_KHR_driver_properties") != device.extensions.end();
        if (dispatch.get_physical_device_properties2 != nullptr &&
            supports_driver_properties) {
            VkPhysicalDeviceDriverProperties driver_properties{};
            driver_properties.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            VkPhysicalDeviceProperties2 properties2{};
            properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties2.pNext = &driver_properties;
            dispatch.get_physical_device_properties2(physical_device, &properties2);
            device.driver_name = driver_properties.driverName;
            device.driver_info = driver_properties.driverInfo;
        }
#endif

        std::uint32_t queue_count = 0U;
        dispatch.get_queue_family_properties(
            physical_device, &queue_count, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queue_count);
        if (queue_count > 0U) {
            dispatch.get_queue_family_properties(
                physical_device, &queue_count, queues.data());
            queues.resize(queue_count);
        }
        for (std::uint32_t index = 0U; index < queue_count; ++index) {
            QueueFamilyObservation observation;
            observation.index = index;
            observation.queue_count = queues[index].queueCount;
            observation.flags = static_cast<unsigned int>(queues[index].queueFlags);
            appendQueueFlagNames(observation.flags, observation.flag_names);
            device.queue_families.push_back(std::move(observation));
        }

        VkPhysicalDeviceMemoryProperties memory{};
        dispatch.get_memory_properties(physical_device, &memory);
        for (std::uint32_t index = 0U; index < memory.memoryHeapCount; ++index) {
            const auto& heap = memory.memoryHeaps[index];
            device.memory_heaps.push_back({
                index, static_cast<unsigned long long>(heap.size),
                (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0U
            });
        }
        for (std::uint32_t index = 0U; index < memory.memoryTypeCount; ++index) {
            const auto& memory_type = memory.memoryTypes[index];
            MemoryTypeObservation observation;
            observation.index = index;
            observation.heap_index = memory_type.heapIndex;
            observation.property_flags =
                static_cast<unsigned int>(memory_type.propertyFlags);
            appendMemoryPropertyNames(
                observation.property_flags, observation.property_flag_names);
            device.memory_types.push_back(std::move(observation));
        }

        struct FormatEntry { VkFormat format; const char* name; };
        constexpr FormatEntry formats[] = {
            {VK_FORMAT_R8G8B8A8_UNORM, "R8G8B8A8_UNORM"},
            {VK_FORMAT_B8G8R8A8_UNORM, "B8G8R8A8_UNORM"},
            {VK_FORMAT_R16G16B16A16_SFLOAT, "R16G16B16A16_SFLOAT"},
            {VK_FORMAT_D32_SFLOAT, "D32_SFLOAT"}
        };
        for (const auto& format : formats) {
            VkFormatProperties format_properties{};
            dispatch.get_format_properties(
                physical_device, format.format, &format_properties);
            FormatObservation observation;
            observation.name = format.name;
            observation.linear_tiling_features =
                static_cast<unsigned int>(format_properties.linearTilingFeatures);
            observation.optimal_tiling_features =
                static_cast<unsigned int>(format_properties.optimalTilingFeatures);
            observation.buffer_features =
                static_cast<unsigned int>(format_properties.bufferFeatures);
            appendFormatFeatureNames(
                observation.linear_tiling_features,
                observation.linear_tiling_feature_names);
            appendFormatFeatureNames(
                observation.optimal_tiling_features,
                observation.optimal_tiling_feature_names);
            appendFormatFeatureNames(
                observation.buffer_features,
                observation.buffer_feature_names);
            device.formats.push_back(std::move(observation));
        }
        report.vulkan_devices.push_back(std::move(device));
    }

    report.vulkan_status = report.vulkan_devices.empty() ? "failed" : "success";
    if (report.vulkan_devices.empty()) {
        report.vulkan_failure_reason =
            "Vulkan returned no physical-device records";
    }
    dispatch.destroy_instance(instance, nullptr);
}
#else
void queryVulkan(DiscoveryReport& report) {
    report.vulkan_compiled = false;
    report.vulkan_status = "unavailable";
    report.vulkan_failure_reason =
        "This build has no Vulkan development headers/loader integration; no GPU capability was inferred";
}
#endif

} // namespace

DiscoveryReport collectHardwareDiscoveryReport() {
    DiscoveryReport report;
    collectPlatformFacts(report);
    queryVulkan(report);
    return report;
}

std::string toJson(const DiscoveryReport& report) {
    std::ostringstream out;
    out << "{\n  \"schema_version\": " << report.schema_version
        << ",\n  \"project\": \"CHORD OF NYAMIST / NYAMIST G925 R1\",\n  \"captured_at_utc\": ";
    writeQuoted(out, report.captured_at_utc);
    out << ",\n  \"platform\": ";
    writeQuoted(out, report.platform);
    out << ",\n  \"device\": {\n";
    for (std::size_t index = 0U; index < report.device_fields.size(); ++index) {
        const auto& [key, value] = report.device_fields[index];
        out << "    ";
        writeQuoted(out, key);
        out << ": {\"value\": ";
        writeQuoted(out, value.value);
        out << ", \"source\": ";
        writeQuoted(out, value.source);
        out << ", \"confidence\": ";
        writeQuoted(out, value.confidence);
        out << "}" << (index + 1U == report.device_fields.size() ? "\n" : ",\n");
    }
    out << "  },\n  \"vulkan\": {\n"
        << "    \"compiled_with_vulkan\": " << (report.vulkan_compiled ? "true" : "false")
        << ",\n    \"enumeration_status\": ";
    writeQuoted(out, report.vulkan_status);
    out << ",\n    \"loader_api_version\": ";
    if (report.vulkan_loader_api_version.empty()) out << "null";
    else writeQuoted(out, report.vulkan_loader_api_version);
    out << ",\n    \"loader_library\": ";
    if (report.vulkan_loader_library.empty()) out << "null";
    else writeQuoted(out, report.vulkan_loader_library);
    out << ",\n    \"instance_extensions\": [";
    for (std::size_t index = 0U; index < report.vulkan_instance_extensions.size(); ++index) {
        if (index > 0U) out << ", ";
        writeQuoted(out, report.vulkan_instance_extensions[index]);
    }
    out << "]";
    out << ",\n    \"failure_reason\": ";
    if (report.vulkan_failure_reason.empty()) out << "null";
    else writeQuoted(out, report.vulkan_failure_reason);
    out << ",\n    \"physical_devices\": [";
    if (!report.vulkan_devices.empty()) out << '\n';

    for (std::size_t device_index = 0U; device_index < report.vulkan_devices.size(); ++device_index) {
        const auto& device = report.vulkan_devices[device_index];
        out << "      {\n        \"name\": ";
        writeQuoted(out, device.name);
        out << ",\n        \"device_type\": ";
        writeQuoted(out, device.device_type);
        out << ",\n        \"vendor_id\": ";
        writeQuoted(out, device.vendor_id_hex);
        out << ",\n        \"device_id\": ";
        writeQuoted(out, device.device_id_hex);
        out << ",\n        \"api_version\": ";
        writeQuoted(out, device.api_version);
        out << ",\n        \"driver_version_raw\": " << device.driver_version
            << ",\n        \"driver_name\": ";
        if (device.driver_name.empty()) out << "null"; else writeQuoted(out, device.driver_name);
        out << ",\n        \"driver_info\": ";
        if (device.driver_info.empty()) out << "null"; else writeQuoted(out, device.driver_info);
        out << ",\n        \"extension_query_status\": ";
        writeQuoted(out, device.extension_query_status);
        out << ",\n        \"extension_query_failure_reason\": ";
        if (device.extension_query_failure_reason.empty()) out << "null";
        else writeQuoted(out, device.extension_query_failure_reason);
        out << ",\n        \"feature_query_status\": ";
        writeQuoted(out, device.feature_query_status);
        out << ",\n        \"feature_query_api_max\": ";
        if (device.feature_query_api_max.empty()) out << "null";
        else writeQuoted(out, device.feature_query_api_max);
        out << ",\n        \"feature_query_failure_reason\": ";
        if (device.feature_query_failure_reason.empty()) out << "null";
        else writeQuoted(out, device.feature_query_failure_reason);
        out << ",\n        \"features\": [";
        for (std::size_t index = 0U; index < device.features.size(); ++index) {
            const auto& feature = device.features[index];
            if (index > 0U) out << ", ";
            out << "{\"name\": ";
            writeQuoted(out, feature.name);
            out << ", \"api_version\": ";
            writeQuoted(out, feature.api_version);
            out << ", \"supported\": "
                << (feature.supported ? "true" : "false") << "}";
        }
        out << "],\n        \"limits_query_status\": ";
        writeQuoted(out, device.limits_query_status);
        out << ",\n        \"limits\": [";
        for (std::size_t index = 0U; index < device.limits.size(); ++index) {
            const auto& limit = device.limits[index];
            if (index > 0U) out << ", ";
            out << "{\"name\": ";
            writeQuoted(out, limit.name);
            out << ", \"value\": " << limit.value << "}";
        }
        out << "],\n        \"floating_limits\": [";
        for (std::size_t index = 0U; index < device.floating_limits.size(); ++index) {
            const auto& limit = device.floating_limits[index];
            if (index > 0U) out << ", ";
            out << "{\"name\": ";
            writeQuoted(out, limit.name);
            out << ", \"value\": " << std::setprecision(9) << limit.value
                << std::setprecision(6) << "}";
        }
        out << "],\n        \"queue_families\": [";
        for (std::size_t index = 0U; index < device.queue_families.size(); ++index) {
            const auto& queue = device.queue_families[index];
            if (index > 0U) out << ", ";
            out << "{\"index\": " << queue.index << ", \"queue_count\": " << queue.queue_count
                << ", \"flags_raw\": " << queue.flags << ", \"flag_names\": [";
            for (std::size_t flag = 0U; flag < queue.flag_names.size(); ++flag) {
                if (flag > 0U) out << ", ";
                writeQuoted(out, queue.flag_names[flag]);
            }
            out << "]}";
        }
        out << "],\n        \"extensions\": [";
        for (std::size_t index = 0U; index < device.extensions.size(); ++index) {
            if (index > 0U) out << ", ";
            writeQuoted(out, device.extensions[index]);
        }
        out << "],\n        \"memory_heaps\": [";
        for (std::size_t index = 0U; index < device.memory_heaps.size(); ++index) {
            const auto& heap = device.memory_heaps[index];
            if (index > 0U) out << ", ";
            out << "{\"index\": " << heap.index << ", \"size_bytes\": " << heap.size_bytes
                << ", \"device_local\": " << (heap.device_local ? "true" : "false") << "}";
        }
        out << "],\n        \"memory_types\": [";
        for (std::size_t index = 0U; index < device.memory_types.size(); ++index) {
            const auto& memory_type = device.memory_types[index];
            if (index > 0U) out << ", ";
            out << "{\"index\": " << memory_type.index
                << ", \"heap_index\": " << memory_type.heap_index
                << ", \"property_flags_raw\": " << memory_type.property_flags
                << ", \"property_flag_names\": [";
            for (std::size_t flag = 0U; flag < memory_type.property_flag_names.size(); ++flag) {
                if (flag > 0U) out << ", ";
                writeQuoted(out, memory_type.property_flag_names[flag]);
            }
            out << "]}";
        }
        out << "],\n        \"formats\": [";
        for (std::size_t index = 0U; index < device.formats.size(); ++index) {
            const auto& format = device.formats[index];
            if (index > 0U) out << ", ";
            out << "{\"name\": ";
            writeQuoted(out, format.name);
            out << ", \"linear_tiling_features_raw\": " << format.linear_tiling_features
                << ", \"optimal_tiling_features_raw\": " << format.optimal_tiling_features
                << ", \"buffer_features_raw\": " << format.buffer_features
                << ", \"linear_tiling_feature_names\": [";
            for (std::size_t flag = 0U; flag < format.linear_tiling_feature_names.size(); ++flag) {
                if (flag > 0U) out << ", ";
                writeQuoted(out, format.linear_tiling_feature_names[flag]);
            }
            out << "], \"optimal_tiling_feature_names\": [";
            for (std::size_t flag = 0U; flag < format.optimal_tiling_feature_names.size(); ++flag) {
                if (flag > 0U) out << ", ";
                writeQuoted(out, format.optimal_tiling_feature_names[flag]);
            }
            out << "], \"buffer_feature_names\": [";
            for (std::size_t flag = 0U; flag < format.buffer_feature_names.size(); ++flag) {
                if (flag > 0U) out << ", ";
                writeQuoted(out, format.buffer_feature_names[flag]);
            }
            out << "]}";
        }
        out << "]\n      }"
            << (device_index + 1U == report.vulkan_devices.size() ? "\n" : ",\n");
    }
    if (!report.vulkan_devices.empty()) out << "    ";
    out << "]\n  }\n}\n";
    return out.str();
}

} // namespace nyamist::discovery
