#include "nyamist/discovery/android_surface_swapchain_validation.hpp"
#include "triangle_shaders.hpp"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <dlfcn.h>
#include <string>
#include <vector>

namespace {

using nyamist::discovery::AndroidSurfaceSwapchainValidationReport;

struct VulkanDispatch {
    void* library{};
    PFN_vkGetInstanceProcAddr get_instance_proc_addr{};

    PFN_vkEnumerateInstanceExtensionProperties enumerate_instance_extensions{};
    PFN_vkEnumerateInstanceVersion enumerate_instance_version{};
    PFN_vkCreateInstance create_instance{};

    PFN_vkDestroyInstance destroy_instance{};
    PFN_vkEnumeratePhysicalDevices enumerate_physical_devices{};
    PFN_vkGetPhysicalDeviceProperties get_physical_device_properties{};
    PFN_vkGetPhysicalDeviceFeatures2 get_physical_device_features2{};
    PFN_vkGetPhysicalDeviceQueueFamilyProperties get_queue_families{};
    PFN_vkEnumerateDeviceExtensionProperties enumerate_device_extensions{};
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR get_surface_support{};
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR get_surface_capabilities{};
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR get_surface_formats{};
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR get_present_modes{};
    PFN_vkCreateAndroidSurfaceKHR create_android_surface{};
    PFN_vkDestroySurfaceKHR destroy_surface{};
    PFN_vkCreateDevice create_device{};
    PFN_vkGetDeviceProcAddr get_device_proc_addr{};

    PFN_vkDestroyDevice destroy_device{};
    PFN_vkGetDeviceQueue get_device_queue{};
    PFN_vkCreateSwapchainKHR create_swapchain{};
    PFN_vkDestroySwapchainKHR destroy_swapchain{};
    PFN_vkCreateImageView create_image_view{};
    PFN_vkDestroyImageView destroy_image_view{};
    PFN_vkCreateRenderPass create_render_pass{};
    PFN_vkDestroyRenderPass destroy_render_pass{};
    PFN_vkCreateFramebuffer create_framebuffer{};
    PFN_vkDestroyFramebuffer destroy_framebuffer{};
    PFN_vkCreateShaderModule create_shader_module{};
    PFN_vkDestroyShaderModule destroy_shader_module{};
    PFN_vkCreatePipelineLayout create_pipeline_layout{};
    PFN_vkDestroyPipelineLayout destroy_pipeline_layout{};
    PFN_vkCreateGraphicsPipelines create_graphics_pipelines{};
    PFN_vkDestroyPipeline destroy_pipeline{};
    PFN_vkCmdBeginRenderPass cmd_begin_render_pass{};
    PFN_vkCmdEndRenderPass cmd_end_render_pass{};
    PFN_vkCmdBindPipeline cmd_bind_pipeline{};
    PFN_vkCmdDraw cmd_draw{};
    PFN_vkGetSwapchainImagesKHR get_swapchain_images{};
    PFN_vkCreateSemaphore create_semaphore{};
    PFN_vkDestroySemaphore destroy_semaphore{};
    PFN_vkCreateFence create_fence{};
    PFN_vkDestroyFence destroy_fence{};
    PFN_vkWaitForFences wait_for_fences{};
    PFN_vkResetFences reset_fences{};
    PFN_vkAcquireNextImageKHR acquire_next_image{};
    PFN_vkQueuePresentKHR queue_present{};
    PFN_vkQueueWaitIdle queue_wait_idle{};
    PFN_vkDeviceWaitIdle device_wait_idle{};
    PFN_vkCreateCommandPool create_command_pool{};
    PFN_vkDestroyCommandPool destroy_command_pool{};
    PFN_vkAllocateCommandBuffers allocate_command_buffers{};
    PFN_vkFreeCommandBuffers free_command_buffers{};
    PFN_vkBeginCommandBuffer begin_command_buffer{};
    PFN_vkEndCommandBuffer end_command_buffer{};
    PFN_vkCmdPipelineBarrier cmd_pipeline_barrier{};
    PFN_vkQueueSubmit queue_submit{};
    PFN_vkResetCommandPool reset_command_pool{};
    PFN_vkCreateQueryPool create_query_pool{};
    PFN_vkDestroyQueryPool destroy_query_pool{};
    PFN_vkCmdResetQueryPool cmd_reset_query_pool{};
    PFN_vkCmdWriteTimestamp cmd_write_timestamp{};
    PFN_vkGetQueryPoolResults get_query_pool_results{};
};

bool isUsable(VkResult result) {
    return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
}

std::string resultName(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    default: return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
    }
}

void fail(AndroidSurfaceSwapchainValidationReport& report, const std::string& reason) {
    if (report.failure_reason.empty()) report.failure_reason = reason;
    report.status = "failed";
}

bool hasInstanceExtension(const std::vector<VkExtensionProperties>& extensions,
                          const char* name) {
    return std::any_of(extensions.begin(), extensions.end(),
        [name](const VkExtensionProperties& item) {
            return std::strcmp(item.extensionName, name) == 0;
        });
}

bool hasDeviceExtension(const std::vector<VkExtensionProperties>& extensions,
                        const char* name) {
    return hasInstanceExtension(extensions, name);
}

std::string versionString(std::uint32_t version) {
    return std::to_string(VK_VERSION_MAJOR(version)) + "." +
           std::to_string(VK_VERSION_MINOR(version)) + "." +
           std::to_string(VK_VERSION_PATCH(version));
}

std::string formatName(VkFormat format) {
    switch (format) {
    case VK_FORMAT_B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
    case VK_FORMAT_R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
    case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
    case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
    default: return "VkFormat(" + std::to_string(static_cast<int>(format)) + ")";
    }
}

std::string colorSpaceName(VkColorSpaceKHR color_space) {
    if (color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return "SRGB_NONLINEAR";
    return "VkColorSpaceKHR(" +
           std::to_string(static_cast<int>(color_space)) + ")";
}


bool renderAndPresentFrame(
        VulkanDispatch& vk,
        AndroidSurfaceSwapchainValidationReport& report,
        VkDevice device,
        VkQueue queue,
        std::uint32_t queue_family,
        VkSwapchainKHR swapchain,
        VkImage image,
        VkFormat format,
        VkExtent2D extent,
        std::uint32_t image_index,
        VkSemaphore acquire_semaphore,
        VkSemaphore present_semaphore,
        VkCommandPool& command_pool,
        VkCommandBuffer& command_buffer,
        VkImageView& image_view,
        VkRenderPass& render_pass,
        VkFramebuffer& framebuffer,
        VkShaderModule& vertex_shader,
        VkShaderModule& fragment_shader,
        VkPipelineLayout& pipeline_layout,
        VkPipeline& graphics_pipeline,
        VkQueryPool timestamp_query_pool,
        std::uint32_t timestamp_query_base,
        VkFence present_fence,
        bool present_fence_enabled,
        bool collect_frame_sample,
        std::chrono::steady_clock::time_point frame_start) {
    VkImageViewCreateInfo image_view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = format;
    image_view_info.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
    };
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0U;
    image_view_info.subresourceRange.levelCount = 1U;
    image_view_info.subresourceRange.baseArrayLayer = 0U;
    image_view_info.subresourceRange.layerCount = 1U;

    VkResult result = vk.create_image_view(device, &image_view_info, nullptr, &image_view);
    report.image_view_create_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkCreateImageView failed: " + resultName(result));
        return false;
    }

    if (render_pass == VK_NULL_HANDLE) {
        VkAttachmentDescription color_attachment{};
        color_attachment.format = format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_reference{};
        color_reference.attachment = 0U;
        color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1U;
        subpass.pColorAttachments = &color_reference;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0U;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0U;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        render_pass_info.attachmentCount = 1U;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1U;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1U;
        render_pass_info.pDependencies = &dependency;
        result = vk.create_render_pass(device, &render_pass_info, nullptr, &render_pass);
        report.render_pass_create_status = result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateRenderPass failed: " + resultName(result));
            return false;
        }

    }

    VkFramebufferCreateInfo framebuffer_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer_info.renderPass = render_pass;
    framebuffer_info.attachmentCount = 1U;
    framebuffer_info.pAttachments = &image_view;
    framebuffer_info.width = extent.width;
    framebuffer_info.height = extent.height;
    framebuffer_info.layers = 1U;
    result = vk.create_framebuffer(device, &framebuffer_info, nullptr, &framebuffer);
    report.framebuffer_create_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkCreateFramebuffer failed: " + resultName(result));
        return false;
    }

    if (graphics_pipeline == VK_NULL_HANDLE) {
        VkShaderModuleCreateInfo vertex_shader_info{
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        vertex_shader_info.codeSize = sizeof(nyamist::shaders::triangle_vertex_spirv);
        vertex_shader_info.pCode = nyamist::shaders::triangle_vertex_spirv;
        result = vk.create_shader_module(device, &vertex_shader_info, nullptr, &vertex_shader);
        report.vertex_shader_module_create_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateShaderModule(vertex) failed: " + resultName(result));
            return false;
        }

        VkShaderModuleCreateInfo fragment_shader_info{
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        fragment_shader_info.codeSize = sizeof(nyamist::shaders::triangle_fragment_spirv);
        fragment_shader_info.pCode = nyamist::shaders::triangle_fragment_spirv;
        result = vk.create_shader_module(device, &fragment_shader_info, nullptr, &fragment_shader);
        report.fragment_shader_module_create_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateShaderModule(fragment) failed: " + resultName(result));
            return false;
        }

        VkPipelineLayoutCreateInfo pipeline_layout_info{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        result = vk.create_pipeline_layout(device, &pipeline_layout_info, nullptr,
                                           &pipeline_layout);
        report.pipeline_layout_create_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreatePipelineLayout failed: " + resultName(result));
            return false;
        }

        VkPipelineShaderStageCreateInfo shader_stages[2]{};
        shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stages[0].module = vertex_shader;
        shader_stages[0].pName = "main";
        shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stages[1].module = fragment_shader;
        shader_stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertex_input{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo input_assembly{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0F;
        viewport.y = 0.0F;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        VkPipelineViewportStateCreateInfo viewport_state{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport_state.viewportCount = 1U;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1U;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterization{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterization.depthClampEnable = VK_FALSE;
        rasterization.rasterizerDiscardEnable = VK_FALSE;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.depthBiasEnable = VK_FALSE;
        rasterization.lineWidth = 1.0F;

        VkPipelineMultisampleStateCreateInfo multisampling{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.sampleShadingEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.blendEnable = VK_FALSE;
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo color_blending{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1U;
        color_blending.pAttachments = &color_blend_attachment;

        VkGraphicsPipelineCreateInfo pipeline_info{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipeline_info.stageCount = 2U;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterization;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.layout = pipeline_layout;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0U;
        pipeline_info.basePipelineIndex = -1;
        result = vk.create_graphics_pipelines(device, VK_NULL_HANDLE, 1U,
                                              &pipeline_info, nullptr,
                                              &graphics_pipeline);
        report.graphics_pipeline_create_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateGraphicsPipelines failed: " + resultName(result));
            return false;
        }

    }

    if (command_pool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool_info.queueFamilyIndex = queue_family;
        result = vk.create_command_pool(device, &pool_info, nullptr, &command_pool);
        report.command_pool_create_status = result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateCommandPool failed: " + resultName(result));
            return false;
        }

        VkCommandBufferAllocateInfo allocate_info{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.commandPool = command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1U;
        result = vk.allocate_command_buffers(device, &allocate_info, &command_buffer);
        report.command_buffer_allocate_status = result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkAllocateCommandBuffers failed: " + resultName(result));
            return false;
        }

    } else {
        result = vk.reset_command_pool(device, command_pool, 0U);
        if (result != VK_SUCCESS) {
            fail(report, "vkResetCommandPool failed: " + resultName(result));
            return false;
        }
    }

    VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vk.begin_command_buffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        report.command_record_status = "failed";
        fail(report, "vkBeginCommandBuffer failed: " + resultName(result));
        return false;
    }

    if (timestamp_query_pool != VK_NULL_HANDLE && collect_frame_sample) {
        vk.cmd_reset_query_pool(command_buffer, timestamp_query_pool, timestamp_query_base, 2U);
        vk.cmd_write_timestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               timestamp_query_pool, timestamp_query_base);
    }

    VkClearValue clear_value{};
    clear_value.color.float32[0] = 0.035F;
    clear_value.color.float32[1] = 0.14F;
    clear_value.color.float32[2] = 0.32F;
    clear_value.color.float32[3] = 1.0F;

    VkRenderPassBeginInfo render_begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render_begin.renderPass = render_pass;
    render_begin.framebuffer = framebuffer;
    render_begin.renderArea.offset = {0, 0};
    render_begin.renderArea.extent = extent;
    render_begin.clearValueCount = 1U;
    render_begin.pClearValues = &clear_value;

    // The reusable render pass clears the acquired swapchain image, then executes
    // the R1.9 graphics pipeline and three-vertex draw before presentation.
    vk.cmd_begin_render_pass(command_buffer, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
    report.render_pass_begin_recorded = true;
    vk.cmd_bind_pipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                         graphics_pipeline);
    report.graphics_pipeline_bind_recorded = true;
    vk.cmd_draw(command_buffer, 3U, 1U, 0U, 0U);
    report.draw_call_recorded = true;
    report.draw_vertex_count = 3U;
    vk.cmd_end_render_pass(command_buffer);
    report.render_pass_end_recorded = true;
    report.image_layout_transition_recorded = true;
    if (timestamp_query_pool != VK_NULL_HANDLE && collect_frame_sample) {
        vk.cmd_write_timestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                               timestamp_query_pool, timestamp_query_base + 1U);
    }

    result = vk.end_command_buffer(command_buffer);
    report.command_record_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkEndCommandBuffer failed: " + resultName(result));
        return false;
    }

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.waitSemaphoreCount = 1U;
    submit_info.pWaitSemaphores = &acquire_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1U;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1U;
    submit_info.pSignalSemaphores = &present_semaphore;
    const auto submit_start = std::chrono::steady_clock::now();
    result = vk.queue_submit(queue, 1U, &submit_info, VK_NULL_HANDLE);
    const auto submit_end = std::chrono::steady_clock::now();
    report.queue_submit_cpu_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            submit_end - submit_start).count());
    report.queue_submit_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkQueueSubmit for render-pass clear failed: " + resultName(result));
        return false;
    }

    VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1U;
    present_info.pWaitSemaphores = &present_semaphore;
    present_info.swapchainCount = 1U;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &image_index;
    VkSwapchainPresentFenceInfoEXT present_fence_info{
        VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT};
    present_fence_info.swapchainCount = 1U;
    present_fence_info.pFences = &present_fence;
    if (present_fence_enabled) {
        present_info.pNext = &present_fence_info;
        report.present_fence_attach_status = "success";
    } else {
        report.present_fence_attach_status = "not_supported";
    }
    const auto present_start = std::chrono::steady_clock::now();
    result = vk.queue_present(queue, &present_info);
    const auto present_end = std::chrono::steady_clock::now();
    report.present_call_cpu_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            present_end - present_start).count());
    report.image_present_status = isUsable(result) ? "success" : "failed";
    report.image_presented = isUsable(result);
    if (!isUsable(result)) {
        fail(report, "vkQueuePresentKHR failed: " + resultName(result));
        return false;
    }
    if (present_fence_enabled) {
        ++report.present_fence_attach_count;
    }

    if (present_fence_enabled) {
        const auto fence_wait_start = std::chrono::steady_clock::now();
        result = vk.wait_for_fences(device, 1U, &present_fence, VK_TRUE,
                                     std::numeric_limits<std::uint64_t>::max());
        const auto fence_wait_end = std::chrono::steady_clock::now();
        report.present_fence_wait_cpu_us = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                fence_wait_end - fence_wait_start).count());
        report.present_fence_wait_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkWaitForFences(present) failed: " + resultName(result));
            return false;
        }
        ++report.present_fence_wait_count;
        report.present_fence_used = true;
        report.present_completion_guaranteed = true;
        report.present_completion_mode = "VK_EXT_swapchain_maintenance1";
        if (collect_frame_sample) {
            report.present_fence_wait_cpu_us_samples.push_back(
                report.present_fence_wait_cpu_us);
        }
    }

    const auto idle_start = std::chrono::steady_clock::now();
    result = vk.queue_wait_idle(queue);
    const auto idle_end = std::chrono::steady_clock::now();
    report.queue_idle_wait_cpu_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            idle_end - idle_start).count());
    report.queue_idle_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "vkQueueWaitIdle failed after rendering/presentation: " + resultName(result));
        return false;
    }

    report.frame_content_written = true;
    report.frame_timing_source = "std::chrono::steady_clock";
    report.frame_total_cpu_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            idle_end - frame_start).count());
    if (collect_frame_sample) {
        report.acquire_cpu_us_samples.push_back(report.acquire_cpu_us);
        report.queue_submit_cpu_us_samples.push_back(report.queue_submit_cpu_us);
        report.present_call_cpu_us_samples.push_back(report.present_call_cpu_us);
        report.queue_idle_wait_cpu_us_samples.push_back(report.queue_idle_wait_cpu_us);
        report.frame_total_cpu_us_samples.push_back(report.frame_total_cpu_us);
        report.frame_timing_sample_count =
            static_cast<std::uint32_t>(report.frame_total_cpu_us_samples.size());
        if (timestamp_query_pool != VK_NULL_HANDLE) {
            std::uint64_t timestamps[2]{};
            result = vk.get_query_pool_results(
                device, timestamp_query_pool, timestamp_query_base, 2U,
                sizeof(timestamps), timestamps, sizeof(std::uint64_t),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            if (result != VK_SUCCESS) {
                report.gpu_timestamp_query_status = "failed";
                fail(report, "vkGetQueryPoolResults failed: " + resultName(result));
                return false;
            }
            std::uint64_t ticks = timestamps[1] - timestamps[0];
            if (report.queue_timestamp_valid_bits < 64U) {
                const std::uint64_t mask =
                    (std::uint64_t{1} << report.queue_timestamp_valid_bits) - 1U;
                ticks &= mask;
            }
            const auto elapsed_ns = static_cast<std::uint64_t>(std::llround(
                static_cast<long double>(ticks) *
                static_cast<long double>(report.timestamp_period_ns)));
            report.gpu_frame_time_ns_samples.push_back(elapsed_ns);
            report.gpu_timestamp_sample_count =
                static_cast<std::uint32_t>(report.gpu_frame_time_ns_samples.size());
        }
    }
    report.frame_timing_valid =
        report.frame_timing_sample_count == report.frame_target_count;
    return true;
}


bool runFrameSequence(
        VulkanDispatch& vk,
        AndroidSurfaceSwapchainValidationReport& report,
        VkDevice device,
        VkQueue queue,
        std::uint32_t queue_family,
        VkSwapchainKHR swapchain,
        const std::vector<VkImage>& images,
        VkFormat format,
        VkExtent2D extent,
        VkSemaphore& acquire_semaphore,
        std::vector<VkSemaphore>& present_semaphores,
        VkFence& present_fence,
        bool present_fence_enabled,
        VkQueryPool& timestamp_query_pool,
        VkCommandPool& command_pool,
        VkCommandBuffer& command_buffer,
        VkImageView& image_view,
        VkRenderPass& render_pass,
        VkFramebuffer& framebuffer,
        VkShaderModule& vertex_shader,
        VkShaderModule& fragment_shader,
        VkPipelineLayout& pipeline_layout,
        VkPipeline& graphics_pipeline) {
    if (images.empty()) {
        fail(report, "The swapchain returned no images for the frame sequence");
        return false;
    }

    VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkResult result = vk.create_semaphore(
        device, &semaphore_info, nullptr, &acquire_semaphore);
    report.semaphore_create_status = result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        fail(report, "Acquire semaphore creation failed: " + resultName(result));
        return false;
    }
    // A present-wait semaphore is associated with the acquired swapchain image.
    // Reacquiring that same image is the Vulkan-guaranteed point at which its
    // previous presentation wait has completed and the semaphore can be reused.
    present_semaphores.assign(images.size(), VK_NULL_HANDLE);
    for (std::size_t image = 0U; image < images.size(); ++image) {
        result = vk.create_semaphore(
            device, &semaphore_info, nullptr, &present_semaphores[image]);
        if (result != VK_SUCCESS) {
            report.present_semaphore_create_status = "failed";
            fail(report, "Present semaphore creation failed for swapchain image " +
                         std::to_string(image) + ": " + resultName(result));
            return false;
        }
    }
    report.present_semaphore_create_status = "success";

    if (present_fence_enabled) {
        VkFenceCreateInfo present_fence_create_info{
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        result = vk.create_fence(
            device, &present_fence_create_info, nullptr, &present_fence);
        report.present_fence_create_status =
            result == VK_SUCCESS ? "success" : "failed";
        if (result != VK_SUCCESS) {
            fail(report, "vkCreateFence(present) failed: " + resultName(result));
            return false;
        }
        report.present_fence_entry_points_status = "success";
        report.present_completion_mode = "VK_EXT_swapchain_maintenance1";
    } else {
        report.present_fence_create_status = "not_supported";
        report.present_fence_entry_points_status = "not_supported";
        report.present_fence_attach_status = "not_supported";
        report.present_fence_wait_status = "not_supported";
        report.present_fence_reset_status = "not_required";
        report.present_completion_mode = "per_image_reacquire_fallback";
        report.present_completion_guaranteed = false;
    }

    const bool timestamp_supported =
        report.queue_timestamp_valid_bits > 0U &&
        report.timestamp_compute_and_graphics_supported &&
        vk.create_query_pool != nullptr && vk.destroy_query_pool != nullptr &&
        vk.cmd_reset_query_pool != nullptr && vk.cmd_write_timestamp != nullptr &&
        vk.get_query_pool_results != nullptr;
    if (!timestamp_supported) {
        report.gpu_timestamp_status = "unsupported";
        report.gpu_timestamp_query_pool_create_status = "not_supported";
        report.gpu_timestamp_query_status = "not_supported";
        fail(report, "R1.9 requires timestamp queries, but the selected queue or Vulkan entry points do not support them");
        return false;
    }

    VkQueryPoolCreateInfo query_info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    query_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    query_info.queryCount = report.frame_target_count * 2U;
    result = vk.create_query_pool(device, &query_info, nullptr, &timestamp_query_pool);
    report.gpu_timestamp_query_pool_create_status =
        result == VK_SUCCESS ? "success" : "failed";
    if (result != VK_SUCCESS) {
        report.gpu_timestamp_status = "query_pool_creation_failed";
        fail(report, "vkCreateQueryPool(timestamp) failed: " + resultName(result));
        return false;
    }
    report.gpu_timestamp_query_status = "pending";

    const std::uint32_t total_frames =
        report.frame_target_count + report.warmup_frame_count;
    for (std::uint32_t frame_index = 0U; frame_index < total_frames; ++frame_index) {
        const auto frame_start = std::chrono::steady_clock::now();
        std::uint32_t image_index = 0U;
        const auto acquire_start = std::chrono::steady_clock::now();
        result = vk.acquire_next_image(
            device, swapchain, 1000000000ULL,
            acquire_semaphore, VK_NULL_HANDLE, &image_index);
        const auto acquire_end = std::chrono::steady_clock::now();
        report.acquire_cpu_us = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                acquire_end - acquire_start).count());
        report.image_acquire_status = isUsable(result) ? "success" : "failed";
        if (!isUsable(result)) {
            fail(report, "vkAcquireNextImageKHR failed during frame " +
                         std::to_string(frame_index) + ": " + resultName(result));
            return false;
        }
        if (image_index >= images.size()) {
            report.image_acquire_status = "failed";
            fail(report, "vkAcquireNextImageKHR returned an out-of-range image index");
            return false;
        }
        report.image_acquired = true;
        report.acquired_image_index = image_index;

        const bool collect_sample = frame_index >= report.warmup_frame_count;
        const std::uint32_t sample_index =
            collect_sample ? frame_index - report.warmup_frame_count : 0U;
        const bool frame_ok = renderAndPresentFrame(
            vk, report, device, queue, queue_family, swapchain,
            images[image_index], format, extent, image_index,
            acquire_semaphore, present_semaphores[image_index],
            command_pool, command_buffer, image_view, render_pass, framebuffer,
            vertex_shader, fragment_shader, pipeline_layout, graphics_pipeline,
            collect_sample ? timestamp_query_pool : VK_NULL_HANDLE,
            sample_index * 2U, present_fence, present_fence_enabled,
            collect_sample, frame_start);
        if (!frame_ok) return false;
        ++report.frame_completed_count;

        if (present_fence_enabled && frame_index + 1U < total_frames) {
            result = vk.reset_fences(device, 1U, &present_fence);
            report.present_fence_reset_status =
                result == VK_SUCCESS ? "success" : "failed";
            if (result != VK_SUCCESS) {
                fail(report, "vkResetFences(present) failed: " + resultName(result));
                return false;
            }
            ++report.present_fence_reset_count;
        } else if (present_fence_enabled) {
            report.present_fence_reset_status = "not_required_after_final_present";
        }

        // A framebuffer is tied to the particular swapchain image acquired above.
        if (framebuffer != VK_NULL_HANDLE) {
            vk.destroy_framebuffer(device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
            report.framebuffer_destroy_status = "success";
        }
        if (image_view != VK_NULL_HANDLE) {
            vk.destroy_image_view(device, image_view, nullptr);
            image_view = VK_NULL_HANDLE;
            report.image_view_destroy_status = "success";
        }
    }

    report.frame_timing_valid =
        report.frame_timing_sample_count == report.frame_target_count;
    if (!report.frame_timing_valid) {
        fail(report, "The multi-frame CPU timing sample count did not reach the target");
        return false;
    }
    if (report.gpu_timestamp_sample_count != report.frame_target_count) {
        report.gpu_timestamp_query_status = "incomplete";
        fail(report, "The GPU timestamp query sample count did not reach the target");
        return false;
    }
    report.gpu_timestamp_query_status = "success";
    report.gpu_timestamp_status = "success";
    if (present_fence_enabled && report.present_fence_wait_status == "success") {
        report.shutdown_presentation_completion_status = "present_fence_signaled";
        report.present_completion_guaranteed = true;
    } else {
        report.shutdown_presentation_completion_status =
            "not_guaranteed_without_present_fence";
    }
    return true;
}

AndroidSurfaceSwapchainValidationReport runValidation(
        JNIEnv* env, jobject java_surface, jint requested_width, jint requested_height) {
    AndroidSurfaceSwapchainValidationReport report;
    VulkanDispatch vk;
    ANativeWindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;
    VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
    std::vector<VkSemaphore> present_semaphores;
    VkFence present_fence = VK_NULL_HANDLE;
    VkQueryPool timestamp_query_pool = VK_NULL_HANDLE;
    bool maintenance_instance_extensions_enabled = false;
    bool maintenance_device_extension_supported = false;
    bool maintenance_feature_supported = false;
    std::uint32_t loader_api_version = VK_API_VERSION_1_0;
    std::uint32_t instance_api_version = VK_API_VERSION_1_0;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    if (java_surface == nullptr || requested_width <= 0 || requested_height <= 0) {
        report.native_window_status = "invalid_input";
        fail(report, "A valid Android Surface and positive dimensions are required");
    } else {
        window = ANativeWindow_fromSurface(env, java_surface);
        report.native_window_status = window != nullptr ? "success" : "failed";
        if (window == nullptr) fail(report, "ANativeWindow_fromSurface returned null");
    }

    if (report.failure_reason.empty()) {
        vk.library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        if (vk.library == nullptr) {
            report.loader_status = "failed";
            fail(report, "Could not load the Android public Vulkan loader libvulkan.so");
        } else {
            report.loader_library = "libvulkan.so";
            vk.get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                dlsym(vk.library, "vkGetInstanceProcAddr"));
            report.loader_status = vk.get_instance_proc_addr != nullptr ? "success" : "failed";
            if (vk.get_instance_proc_addr == nullptr) {
                fail(report, "vkGetInstanceProcAddr was not exported by libvulkan.so");
            }
        }
    }

    if (report.failure_reason.empty()) {
#define LOAD_GLOBAL(member, symbol) \
        vk.member = reinterpret_cast<decltype(vk.member)>( \
            vk.get_instance_proc_addr(VK_NULL_HANDLE, symbol))
        LOAD_GLOBAL(enumerate_instance_extensions, "vkEnumerateInstanceExtensionProperties");
        LOAD_GLOBAL(enumerate_instance_version, "vkEnumerateInstanceVersion");
        LOAD_GLOBAL(create_instance, "vkCreateInstance");
#undef LOAD_GLOBAL
        if (vk.enumerate_instance_extensions == nullptr || vk.create_instance == nullptr) {
            fail(report, "Required Vulkan global entry points are unavailable");
        } else if (vk.enumerate_instance_version != nullptr) {
            std::uint32_t version = VK_API_VERSION_1_0;
            const VkResult version_result = vk.enumerate_instance_version(&version);
            if (version_result == VK_SUCCESS) loader_api_version = version;
        }
    }

    if (report.failure_reason.empty()) {
        std::uint32_t extension_count = 0U;
        VkResult result = vk.enumerate_instance_extensions(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions;
        if (result == VK_SUCCESS && extension_count > 0U) {
            extensions.resize(extension_count);
            result = vk.enumerate_instance_extensions(nullptr, &extension_count, extensions.data());
        }
        if (result != VK_SUCCESS) {
            fail(report, "Instance extension enumeration failed: " + resultName(result));
        } else {
            report.surface_extension_status =
                hasInstanceExtension(extensions, VK_KHR_SURFACE_EXTENSION_NAME) ? "success" : "missing";
            report.android_surface_extension_status =
                hasInstanceExtension(extensions, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) ? "success" : "missing";
            const bool surface_maintenance_available =
                hasInstanceExtension(extensions, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            const bool surface_capabilities2_available =
                hasInstanceExtension(extensions, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            const bool properties2_available =
                hasInstanceExtension(extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            const bool core_features2_available =
                VK_VERSION_MAJOR(loader_api_version) > 1U ||
                VK_VERSION_MINOR(loader_api_version) >= 1U;
            const bool features2_available =
                core_features2_available || properties2_available;
            maintenance_instance_extensions_enabled =
                surface_maintenance_available && surface_capabilities2_available &&
                features2_available;
            report.swapchain_maintenance_instance_extension_status =
                maintenance_instance_extensions_enabled
                    ? "available_for_enablement"
                    : "unsupported_or_dependency_missing";
            if (report.surface_extension_status != "success" ||
                report.android_surface_extension_status != "success") {
                fail(report, "Required VK_KHR_surface or VK_KHR_android_surface extension is missing");
            }
        }
    }

    if (report.failure_reason.empty()) {
        std::vector<const char*> instance_extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
        };
        const bool properties2_available =
            maintenance_instance_extensions_enabled &&
            VK_VERSION_MAJOR(loader_api_version) == 1U &&
            VK_VERSION_MINOR(loader_api_version) < 1U;
        if (maintenance_instance_extensions_enabled) {
            instance_extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            instance_extensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            if (properties2_available) {
                instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            }
        }
        VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app_info.pApplicationName = "NYAMIST G925 R1.9";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 9, 0);
        app_info.pEngineName = "CHORD OF NYAMIST";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        const bool core_features2_available =
            VK_VERSION_MAJOR(loader_api_version) > 1U ||
            VK_VERSION_MINOR(loader_api_version) >= 1U;
        instance_api_version = core_features2_available
            ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;
        app_info.apiVersion = instance_api_version;
        report.loader_api_version = versionString(loader_api_version);

        VkInstanceCreateInfo create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        create_info.pApplicationInfo = &app_info;
        create_info.enabledExtensionCount =
            static_cast<std::uint32_t>(instance_extensions.size());
        create_info.ppEnabledExtensionNames = instance_extensions.data();

        const VkResult result = vk.create_instance(&create_info, nullptr, &instance);
        report.instance_create_status = isUsable(result) ? "success" : "failed";
        if (!isUsable(result)) {
            fail(report, "vkCreateInstance failed: " + resultName(result));
        } else if (maintenance_instance_extensions_enabled) {
            report.swapchain_maintenance_instance_extension_status = "enabled";
        }
    }

    if (instance != VK_NULL_HANDLE) {
#define LOAD_INSTANCE(member, symbol) \
        vk.member = reinterpret_cast<decltype(vk.member)>(vk.get_instance_proc_addr(instance, symbol))
        LOAD_INSTANCE(destroy_instance, "vkDestroyInstance");
        LOAD_INSTANCE(enumerate_physical_devices, "vkEnumeratePhysicalDevices");
        LOAD_INSTANCE(get_physical_device_properties, "vkGetPhysicalDeviceProperties");
        LOAD_INSTANCE(get_physical_device_features2, "vkGetPhysicalDeviceFeatures2");
        if (vk.get_physical_device_features2 == nullptr) {
            vk.get_physical_device_features2 =
                reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
                    vk.get_instance_proc_addr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
        }
        LOAD_INSTANCE(get_queue_families, "vkGetPhysicalDeviceQueueFamilyProperties");
        LOAD_INSTANCE(enumerate_device_extensions, "vkEnumerateDeviceExtensionProperties");
        LOAD_INSTANCE(get_surface_support, "vkGetPhysicalDeviceSurfaceSupportKHR");
        LOAD_INSTANCE(get_surface_capabilities, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        LOAD_INSTANCE(get_surface_formats, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        LOAD_INSTANCE(get_present_modes, "vkGetPhysicalDeviceSurfacePresentModesKHR");
        LOAD_INSTANCE(create_android_surface, "vkCreateAndroidSurfaceKHR");
        LOAD_INSTANCE(destroy_surface, "vkDestroySurfaceKHR");
        LOAD_INSTANCE(create_device, "vkCreateDevice");
        LOAD_INSTANCE(get_device_proc_addr, "vkGetDeviceProcAddr");
#undef LOAD_INSTANCE
    }

    if (report.failure_reason.empty() && instance != VK_NULL_HANDLE) {
        if (vk.create_android_surface == nullptr || vk.destroy_surface == nullptr ||
            vk.enumerate_physical_devices == nullptr || vk.get_queue_families == nullptr ||
            vk.get_surface_support == nullptr || vk.get_surface_capabilities == nullptr ||
            vk.get_surface_formats == nullptr || vk.get_present_modes == nullptr ||
            vk.enumerate_device_extensions == nullptr || vk.create_device == nullptr ||
            vk.get_device_proc_addr == nullptr) {
            fail(report, "One or more required Vulkan instance entry points are unavailable");
        } else {
            VkAndroidSurfaceCreateInfoKHR surface_info{
                VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
            surface_info.window = window;
            const VkResult result = vk.create_android_surface(instance, &surface_info,
                                                               nullptr, &surface);
            report.surface_create_status = isUsable(result) ? "success" : "failed";
            if (!isUsable(result)) fail(report, "vkCreateAndroidSurfaceKHR failed: " + resultName(result));
        }
    }

    std::uint32_t selected_queue_family = 0U;
    std::uint32_t selected_queue_timestamp_valid_bits = 0U;
    VkPhysicalDevice selected_physical_device = VK_NULL_HANDLE;
    if (report.failure_reason.empty() && surface != VK_NULL_HANDLE) {
        std::uint32_t physical_count = 0U;
        VkResult result = vk.enumerate_physical_devices(instance, &physical_count, nullptr);
        std::vector<VkPhysicalDevice> physical_devices;
        if (result == VK_SUCCESS && physical_count > 0U) {
            physical_devices.resize(physical_count);
            result = vk.enumerate_physical_devices(instance, &physical_count, physical_devices.data());
        }
        if (result != VK_SUCCESS || physical_devices.empty()) {
            fail(report, "No Vulkan physical device was enumerated: " + resultName(result));
        } else {
            for (const VkPhysicalDevice physical : physical_devices) {
                std::uint32_t device_extension_count = 0U;
                VkResult ext_result = vk.enumerate_device_extensions(
                    physical, nullptr, &device_extension_count, nullptr);
                std::vector<VkExtensionProperties> device_extensions;
                if (ext_result == VK_SUCCESS && device_extension_count > 0U) {
                    device_extensions.resize(device_extension_count);
                    ext_result = vk.enumerate_device_extensions(
                        physical, nullptr, &device_extension_count, device_extensions.data());
                }
                if (ext_result != VK_SUCCESS ||
                    !hasDeviceExtension(device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                    continue;
                }

                std::uint32_t family_count = 0U;
                vk.get_queue_families(physical, &family_count, nullptr);
                std::vector<VkQueueFamilyProperties> families(family_count);
                if (family_count == 0U) continue;
                vk.get_queue_families(physical, &family_count, families.data());

                for (std::uint32_t index = 0U; index < family_count; ++index) {
                    if (families[index].queueCount == 0U ||
                        (families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0U) continue;
                    VkBool32 present_supported = VK_FALSE;
                    const VkResult support_result = vk.get_surface_support(
                        physical, index, surface, &present_supported);
                    if (support_result == VK_SUCCESS && present_supported == VK_TRUE) {
                        selected_physical_device = physical;
                        selected_queue_family = index;
                        selected_queue_timestamp_valid_bits = families[index].timestampValidBits;
                        break;
                    }
                }
                if (selected_physical_device != VK_NULL_HANDLE) break;
            }

            if (selected_physical_device == VK_NULL_HANDLE) {
                report.swapchain_extension_status = "missing_or_unsupported";
                report.queue_family_status = "missing_graphics_present_queue";
                fail(report, "No physical device exposes VK_KHR_swapchain and a graphics+present queue for this Android Surface");
            } else {
                report.physical_device_status = "success";
                report.swapchain_extension_status = "success";
                report.queue_family_status = "success";
                report.queue_family_index = selected_queue_family;
                report.graphics_queue_supported = true;
                report.present_queue_supported = true;
                VkPhysicalDeviceProperties properties{};
                vk.get_physical_device_properties(selected_physical_device, &properties);
                report.device_name = properties.deviceName;
                report.vendor_id_hex = "0x" + [] (std::uint32_t value) {
                    constexpr char digits[] = "0123456789abcdef";
                    std::string out;
                    do { out.insert(out.begin(), digits[value & 0xfU]); value >>= 4U; } while (value);
                    return out;
                }(properties.vendorID);
                report.device_id_hex = "0x" + [] (std::uint32_t value) {
                    constexpr char digits[] = "0123456789abcdef";
                    std::string out;
                    do { out.insert(out.begin(), digits[value & 0xfU]); value >>= 4U; } while (value);
                    return out;
                }(properties.deviceID);
                report.physical_device_api_version = versionString(properties.apiVersion);
                report.queue_timestamp_valid_bits = selected_queue_timestamp_valid_bits;
                report.timestamp_compute_and_graphics_supported =
                    properties.limits.timestampComputeAndGraphics == VK_TRUE;
                report.timestamp_period_ns = properties.limits.timestampPeriod;
                report.gpu_timestamp_status =
                    (selected_queue_timestamp_valid_bits > 0U &&
                     report.timestamp_compute_and_graphics_supported)
                        ? "available_not_queried" : "unsupported";
                const std::string gpu_name = report.device_name;
                report.target_gpu_match = gpu_name.find("G925") != std::string::npos ||
                                          gpu_name.find("g925") != std::string::npos;
                if (!report.target_gpu_match) {
                    fail(report, "The selected physical device is not identified as Immortalis-G925");
                }

                std::uint32_t maintenance_extension_count = 0U;
                VkResult maintenance_result = vk.enumerate_device_extensions(
                    selected_physical_device, nullptr, &maintenance_extension_count, nullptr);
                std::vector<VkExtensionProperties> selected_device_extensions;
                if (maintenance_result == VK_SUCCESS && maintenance_extension_count > 0U) {
                    selected_device_extensions.resize(maintenance_extension_count);
                    maintenance_result = vk.enumerate_device_extensions(
                        selected_physical_device, nullptr, &maintenance_extension_count,
                        selected_device_extensions.data());
                }
                maintenance_device_extension_supported =
                    maintenance_result == VK_SUCCESS &&
                    hasDeviceExtension(selected_device_extensions,
                                       VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
                report.swapchain_maintenance_device_extension_status =
                    maintenance_device_extension_supported ? "supported" :
                    (maintenance_result == VK_SUCCESS ? "not_supported" : "query_failed");

                if (maintenance_device_extension_supported &&
                    maintenance_instance_extensions_enabled &&
                    vk.get_physical_device_features2 != nullptr) {
                    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance_features{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
                    VkPhysicalDeviceFeatures2 device_features{
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
                    device_features.pNext = &maintenance_features;
                    vk.get_physical_device_features2(selected_physical_device, &device_features);
                    maintenance_feature_supported =
                        maintenance_features.swapchainMaintenance1 == VK_TRUE;
                    report.swapchain_maintenance_feature_supported =
                        maintenance_feature_supported;
                    report.swapchain_maintenance_feature_status =
                        maintenance_feature_supported ? "supported" : "unsupported";
                } else {
                    report.swapchain_maintenance_feature_status =
                        "not_queryable_or_extension_missing";
                }
            }
        }
    }

    if (report.failure_reason.empty() && selected_physical_device != VK_NULL_HANDLE) {
        const float priority = 1.0F;
        VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.queueFamilyIndex = selected_queue_family;
        queue_info.queueCount = 1U;
        queue_info.pQueuePriorities = &priority;
        std::vector<const char*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance_features_enable{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
        if (maintenance_feature_supported) {
            device_extensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
            maintenance_features_enable.swapchainMaintenance1 = VK_TRUE;
            report.swapchain_maintenance_feature_status = "enabled";
        }
        VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device_info.pNext = maintenance_feature_supported ? &maintenance_features_enable : nullptr;
        device_info.queueCreateInfoCount = 1U;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount =
            static_cast<std::uint32_t>(device_extensions.size());
        device_info.ppEnabledExtensionNames = device_extensions.data();

        const VkResult result = vk.create_device(selected_physical_device, &device_info,
                                                 nullptr, &device);
        if (!isUsable(result)) {
            fail(report, "vkCreateDevice for VK_KHR_swapchain failed: " + resultName(result));
        } else {
#define LOAD_DEVICE(member, symbol) \
            vk.member = reinterpret_cast<decltype(vk.member)>(vk.get_device_proc_addr(device, symbol))
            LOAD_DEVICE(destroy_device, "vkDestroyDevice");
            LOAD_DEVICE(get_device_queue, "vkGetDeviceQueue");
            LOAD_DEVICE(create_swapchain, "vkCreateSwapchainKHR");
            LOAD_DEVICE(destroy_swapchain, "vkDestroySwapchainKHR");
            LOAD_DEVICE(create_image_view, "vkCreateImageView");
            LOAD_DEVICE(destroy_image_view, "vkDestroyImageView");
            LOAD_DEVICE(create_render_pass, "vkCreateRenderPass");
            LOAD_DEVICE(destroy_render_pass, "vkDestroyRenderPass");
            LOAD_DEVICE(create_framebuffer, "vkCreateFramebuffer");
            LOAD_DEVICE(destroy_framebuffer, "vkDestroyFramebuffer");
            LOAD_DEVICE(create_shader_module, "vkCreateShaderModule");
            LOAD_DEVICE(destroy_shader_module, "vkDestroyShaderModule");
            LOAD_DEVICE(create_pipeline_layout, "vkCreatePipelineLayout");
            LOAD_DEVICE(destroy_pipeline_layout, "vkDestroyPipelineLayout");
            LOAD_DEVICE(create_graphics_pipelines, "vkCreateGraphicsPipelines");
            LOAD_DEVICE(destroy_pipeline, "vkDestroyPipeline");
            LOAD_DEVICE(cmd_begin_render_pass, "vkCmdBeginRenderPass");
            LOAD_DEVICE(cmd_end_render_pass, "vkCmdEndRenderPass");
            LOAD_DEVICE(cmd_bind_pipeline, "vkCmdBindPipeline");
            LOAD_DEVICE(cmd_draw, "vkCmdDraw");
            LOAD_DEVICE(get_swapchain_images, "vkGetSwapchainImagesKHR");
            LOAD_DEVICE(create_semaphore, "vkCreateSemaphore");
            LOAD_DEVICE(destroy_semaphore, "vkDestroySemaphore");
            LOAD_DEVICE(create_fence, "vkCreateFence");
            LOAD_DEVICE(destroy_fence, "vkDestroyFence");
            LOAD_DEVICE(wait_for_fences, "vkWaitForFences");
            LOAD_DEVICE(reset_fences, "vkResetFences");
            LOAD_DEVICE(acquire_next_image, "vkAcquireNextImageKHR");
            LOAD_DEVICE(queue_present, "vkQueuePresentKHR");
            LOAD_DEVICE(queue_wait_idle, "vkQueueWaitIdle");
            LOAD_DEVICE(device_wait_idle, "vkDeviceWaitIdle");
            LOAD_DEVICE(create_command_pool, "vkCreateCommandPool");
            LOAD_DEVICE(destroy_command_pool, "vkDestroyCommandPool");
            LOAD_DEVICE(allocate_command_buffers, "vkAllocateCommandBuffers");
            LOAD_DEVICE(free_command_buffers, "vkFreeCommandBuffers");
            LOAD_DEVICE(begin_command_buffer, "vkBeginCommandBuffer");
            LOAD_DEVICE(end_command_buffer, "vkEndCommandBuffer");
            LOAD_DEVICE(cmd_pipeline_barrier, "vkCmdPipelineBarrier");
            LOAD_DEVICE(queue_submit, "vkQueueSubmit");
            LOAD_DEVICE(reset_command_pool, "vkResetCommandPool");
            LOAD_DEVICE(create_query_pool, "vkCreateQueryPool");
            LOAD_DEVICE(destroy_query_pool, "vkDestroyQueryPool");
            LOAD_DEVICE(cmd_reset_query_pool, "vkCmdResetQueryPool");
            LOAD_DEVICE(cmd_write_timestamp, "vkCmdWriteTimestamp");
            LOAD_DEVICE(get_query_pool_results, "vkGetQueryPoolResults");
#undef LOAD_DEVICE
            if (vk.get_device_queue == nullptr || vk.create_swapchain == nullptr ||
                vk.destroy_swapchain == nullptr || vk.get_swapchain_images == nullptr ||
                vk.create_image_view == nullptr || vk.destroy_image_view == nullptr ||
                vk.create_render_pass == nullptr || vk.destroy_render_pass == nullptr ||
                vk.create_framebuffer == nullptr || vk.destroy_framebuffer == nullptr ||
                vk.create_shader_module == nullptr || vk.destroy_shader_module == nullptr ||
                vk.create_pipeline_layout == nullptr || vk.destroy_pipeline_layout == nullptr ||
                vk.create_graphics_pipelines == nullptr || vk.destroy_pipeline == nullptr ||
                vk.cmd_begin_render_pass == nullptr || vk.cmd_end_render_pass == nullptr ||
                vk.cmd_bind_pipeline == nullptr || vk.cmd_draw == nullptr ||
                vk.create_semaphore == nullptr || vk.destroy_semaphore == nullptr ||
                vk.create_fence == nullptr || vk.destroy_fence == nullptr ||
                vk.wait_for_fences == nullptr || vk.reset_fences == nullptr ||
                vk.acquire_next_image == nullptr || vk.queue_present == nullptr ||
                vk.queue_wait_idle == nullptr || vk.create_command_pool == nullptr ||
                vk.destroy_command_pool == nullptr || vk.allocate_command_buffers == nullptr ||
                vk.free_command_buffers == nullptr || vk.begin_command_buffer == nullptr ||
                vk.end_command_buffer == nullptr || vk.cmd_pipeline_barrier == nullptr ||
                vk.queue_submit == nullptr || vk.device_wait_idle == nullptr ||
                vk.reset_command_pool == nullptr) {
                fail(report, "One or more required Vulkan device entry points are unavailable");
            } else {
                vk.get_device_queue(device, selected_queue_family, 0U, &queue);
                if (queue == VK_NULL_HANDLE) fail(report, "vkGetDeviceQueue returned a null queue");
                else if (maintenance_feature_supported) {
                    report.present_fence_entry_points_status = "success";
                }
            }
        }
    }

    if (report.failure_reason.empty() && device != VK_NULL_HANDLE && queue != VK_NULL_HANDLE) {
        VkSurfaceCapabilitiesKHR capabilities{};
        VkResult result = vk.get_surface_capabilities(
            selected_physical_device, surface, &capabilities);
        if (result != VK_SUCCESS) {
            fail(report, "Surface capabilities query failed: " + resultName(result));
        } else {
            report.surface_capabilities_status = "success";
            std::uint32_t format_count = 0U;
            result = vk.get_surface_formats(selected_physical_device, surface, &format_count, nullptr);
            std::vector<VkSurfaceFormatKHR> formats;
            if (result == VK_SUCCESS && format_count > 0U) {
                formats.resize(format_count);
                result = vk.get_surface_formats(
                    selected_physical_device, surface, &format_count, formats.data());
            }
            if (result != VK_SUCCESS || formats.empty()) {
                fail(report, "Surface format enumeration failed or returned no formats");
            } else {
                report.surface_format_count = format_count;
                std::uint32_t mode_count = 0U;
                result = vk.get_present_modes(selected_physical_device, surface, &mode_count, nullptr);
                std::vector<VkPresentModeKHR> modes;
                if (result == VK_SUCCESS && mode_count > 0U) {
                    modes.resize(mode_count);
                    result = vk.get_present_modes(
                        selected_physical_device, surface, &mode_count, modes.data());
                }
                if (result != VK_SUCCESS || modes.empty()) {
                    fail(report, "Surface present-mode enumeration failed or returned no modes");
                } else {
                    report.present_mode_count = mode_count;
                    const bool fifo_supported = std::find(
                        modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_KHR) != modes.end();
                    if (!fifo_supported) {
                        fail(report, "The required VK_PRESENT_MODE_FIFO_KHR mode is unavailable");
                    } else {
                        VkSurfaceFormatKHR selected_format = formats.front();
                        if (selected_format.format == VK_FORMAT_UNDEFINED) {
                            selected_format.format = VK_FORMAT_B8G8R8A8_UNORM;
                        } else {
                            const auto preferred = std::find_if(formats.begin(), formats.end(),
                                [](const VkSurfaceFormatKHR& item) {
                                    return (item.format == VK_FORMAT_B8G8R8A8_SRGB ||
                                            item.format == VK_FORMAT_R8G8B8A8_SRGB) &&
                                           item.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                                });
                            if (preferred != formats.end()) selected_format = *preferred;
                        }
                        report.selected_surface_format =
                            formatName(selected_format.format) + " / " + colorSpaceName(selected_format.colorSpace);
                        report.selected_present_mode = "FIFO";

                        VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                        if ((capabilities.supportedUsageFlags &
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0U) {
                            report.selected_image_usage = "COLOR_ATTACHMENT";
                        } else {
                            fail(report, "R1.9 requires swapchain COLOR_ATTACHMENT usage for render-pass clear");
                        }

                        if (report.failure_reason.empty()) {
                            VkExtent2D extent{};
                            if (capabilities.currentExtent.width != UINT32_MAX) {
                                extent = capabilities.currentExtent;
                            } else {
                                extent.width = std::clamp(
                                    static_cast<std::uint32_t>(requested_width),
                                    capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
                                extent.height = std::clamp(
                                    static_cast<std::uint32_t>(requested_height),
                                    capabilities.minImageExtent.height,
                                    capabilities.maxImageExtent.height);
                            }
                            report.extent_width = extent.width;
                            report.extent_height = extent.height;
                            std::uint32_t image_count = capabilities.minImageCount + 1U;
                            if (capabilities.maxImageCount > 0U &&
                                image_count > capabilities.maxImageCount) {
                                image_count = capabilities.maxImageCount;
                            }

                            VkCompositeAlphaFlagBitsKHR composite_alpha =
                                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
                            const VkCompositeAlphaFlagBitsKHR alpha_modes[] = {
                                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                                VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
                            };
                            for (const auto candidate : alpha_modes) {
                                if ((capabilities.supportedCompositeAlpha & candidate) != 0U) {
                                    composite_alpha = candidate;
                                    break;
                                }
                            }

                            VkSwapchainCreateInfoKHR swapchain_info{
                                VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
                            swapchain_info.surface = surface;
                            swapchain_info.minImageCount = image_count;
                            swapchain_info.imageFormat = selected_format.format;
                            swapchain_info.imageColorSpace = selected_format.colorSpace;
                            swapchain_info.imageExtent = extent;
                            swapchain_info.imageArrayLayers = 1U;
                            swapchain_info.imageUsage = image_usage;
                            swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                            swapchain_info.preTransform = capabilities.currentTransform;
                            swapchain_info.compositeAlpha = composite_alpha;
                            swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
                            swapchain_info.clipped = VK_TRUE;
                            result = vk.create_swapchain(device, &swapchain_info, nullptr, &swapchain);
                            report.swapchain_create_status = isUsable(result) ? "success" : "failed";
                            if (!isUsable(result)) {
                                fail(report, "vkCreateSwapchainKHR failed: " + resultName(result));
                            } else {
                                std::uint32_t swapchain_image_count = 0U;
                                result = vk.get_swapchain_images(
                                    device, swapchain, &swapchain_image_count, nullptr);
                                std::vector<VkImage> images;
                                if (result == VK_SUCCESS && swapchain_image_count > 0U) {
                                    images.resize(swapchain_image_count);
                                    result = vk.get_swapchain_images(
                                        device, swapchain, &swapchain_image_count, images.data());
                                }
                                if (result != VK_SUCCESS || images.empty()) {
                                    report.swapchain_image_query_status = "failed";
                                    fail(report, "Swapchain image enumeration failed: " + resultName(result));
                                } else {
                                    report.swapchain_image_query_status = "success";
                                    report.swapchain_image_count =
                                        static_cast<std::uint32_t>(images.size());
                                    if (report.selected_image_usage != "COLOR_ATTACHMENT") {
                                        fail(report, "Selected swapchain usage is not COLOR_ATTACHMENT");
                                    } else if (!runFrameSequence(
                                        vk, report, device, queue, selected_queue_family,
                                        swapchain, images, selected_format.format, extent,
                                        acquire_semaphore, present_semaphores,
                                        present_fence, maintenance_feature_supported,
                                        timestamp_query_pool,
                                        command_pool, command_buffer, image_view, render_pass,
                                        framebuffer, vertex_shader, fragment_shader,
                                        pipeline_layout, graphics_pipeline)) {
                                        if (report.failure_reason.empty()) {
                                            fail(report, "R1.8 multi-frame timestamp validation failed");
                                        }
                                    }
                                }
                                }
                            }
                        }
                    }
                }
            }
        }

    bool cleanup_ok = true;
    if (device != VK_NULL_HANDLE && vk.device_wait_idle != nullptr) {
        const VkResult idle_result = vk.device_wait_idle(device);
        if (idle_result != VK_SUCCESS) cleanup_ok = false;
    }
    if (maintenance_feature_supported && report.present_fence_wait_status == "success") {
        report.shutdown_presentation_completion_status = "present_fence_signaled";
        report.present_completion_guaranteed = true;
    } else if (!maintenance_feature_supported &&
               report.present_completion_mode == "per_image_reacquire_fallback") {
        report.shutdown_presentation_completion_status =
            "not_guaranteed_without_present_fence";
    }
    if (present_fence != VK_NULL_HANDLE) {
        if (vk.destroy_fence != nullptr) {
            vk.destroy_fence(device, present_fence, nullptr);
            report.present_fence_destroy_status = "success";
        } else {
            report.present_fence_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (timestamp_query_pool != VK_NULL_HANDLE) {
        if (vk.destroy_query_pool != nullptr) {
            vk.destroy_query_pool(device, timestamp_query_pool, nullptr);
            report.gpu_timestamp_query_pool_destroy_status = "success";
        } else {
            report.gpu_timestamp_query_pool_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (graphics_pipeline != VK_NULL_HANDLE) {
        if (vk.destroy_pipeline != nullptr) {
            vk.destroy_pipeline(device, graphics_pipeline, nullptr);
            report.graphics_pipeline_destroy_status = "success";
        } else {
            report.graphics_pipeline_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (pipeline_layout != VK_NULL_HANDLE) {
        if (vk.destroy_pipeline_layout != nullptr) {
            vk.destroy_pipeline_layout(device, pipeline_layout, nullptr);
            report.pipeline_layout_destroy_status = "success";
        } else {
            report.pipeline_layout_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (fragment_shader != VK_NULL_HANDLE) {
        if (vk.destroy_shader_module != nullptr) {
            vk.destroy_shader_module(device, fragment_shader, nullptr);
            report.fragment_shader_module_destroy_status = "success";
        } else {
            report.fragment_shader_module_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (vertex_shader != VK_NULL_HANDLE) {
        if (vk.destroy_shader_module != nullptr) {
            vk.destroy_shader_module(device, vertex_shader, nullptr);
            report.vertex_shader_module_destroy_status = "success";
        } else {
            report.vertex_shader_module_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (framebuffer != VK_NULL_HANDLE) {
        if (vk.destroy_framebuffer != nullptr) {
            vk.destroy_framebuffer(device, framebuffer, nullptr);
            report.framebuffer_destroy_status = "success";
        } else {
            report.framebuffer_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (render_pass != VK_NULL_HANDLE) {
        if (vk.destroy_render_pass != nullptr) {
            vk.destroy_render_pass(device, render_pass, nullptr);
            report.render_pass_destroy_status = "success";
        } else {
            report.render_pass_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (image_view != VK_NULL_HANDLE) {
        if (vk.destroy_image_view != nullptr) {
            vk.destroy_image_view(device, image_view, nullptr);
            report.image_view_destroy_status = "success";
        } else {
            report.image_view_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (command_buffer != VK_NULL_HANDLE && command_pool != VK_NULL_HANDLE) {
        if (vk.free_command_buffers != nullptr) {
            vk.free_command_buffers(device, command_pool, 1U, &command_buffer);
            report.command_buffer_free_status = "success";
        } else {
            report.command_buffer_free_status = "failed";
            cleanup_ok = false;
        }
    }
    if (command_pool != VK_NULL_HANDLE) {
        if (vk.destroy_command_pool != nullptr) {
            vk.destroy_command_pool(device, command_pool, nullptr);
            report.command_pool_destroy_status = "success";
        } else {
            report.command_pool_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (acquire_semaphore != VK_NULL_HANDLE) {
        if (vk.destroy_semaphore != nullptr) {
            vk.destroy_semaphore(device, acquire_semaphore, nullptr);
            report.semaphore_destroy_status = "success";
        } else {
            report.semaphore_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    bool present_semaphores_destroyed = true;
    for (VkSemaphore& semaphore : present_semaphores) {
        if (semaphore == VK_NULL_HANDLE) continue;
        if (vk.destroy_semaphore != nullptr) {
            vk.destroy_semaphore(device, semaphore, nullptr);
            semaphore = VK_NULL_HANDLE;
        } else {
            present_semaphores_destroyed = false;
            cleanup_ok = false;
        }
    }
    if (present_semaphores_destroyed && !present_semaphores.empty()) {
        report.present_semaphore_destroy_status = "success";
    } else if (!present_semaphores_destroyed) {
        report.present_semaphore_destroy_status = "failed";
    }
    if (swapchain != VK_NULL_HANDLE) {
        if (vk.destroy_swapchain != nullptr) {
            vk.destroy_swapchain(device, swapchain, nullptr);
            report.swapchain_destroy_status = "success";
        } else {
            report.swapchain_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (device != VK_NULL_HANDLE) {
        if (vk.destroy_device != nullptr) {
            vk.destroy_device(device, nullptr);
            report.device_destroy_status = "success";
        } else {
            report.device_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (surface != VK_NULL_HANDLE) {
        if (vk.destroy_surface != nullptr) {
            vk.destroy_surface(instance, surface, nullptr);
            report.surface_destroy_status = "success";
        } else {
            report.surface_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (instance != VK_NULL_HANDLE) {
        if (vk.destroy_instance != nullptr) {
            vk.destroy_instance(instance, nullptr);
            report.instance_destroy_status = "success";
        } else {
            report.instance_destroy_status = "failed";
            cleanup_ok = false;
        }
    }
    if (window != nullptr) {
        ANativeWindow_release(window);
        report.native_window_release_status = "success";
    }

    report.cleanup_status = cleanup_ok ? "success" : "failed";
    if (!cleanup_ok) fail(report, "One or more Vulkan objects could not be confirmed destroyed");
    if (vk.library != nullptr) dlclose(vk.library);

    const bool presentation_completion_valid =
        !maintenance_feature_supported ||
        (report.present_fence_used && report.present_completion_guaranteed &&
         report.present_fence_entry_points_status == "success" &&
         report.present_fence_create_status == "success" &&
         report.present_fence_attach_status == "success" &&
         report.present_fence_wait_status == "success" &&
         report.present_fence_destroy_status == "success" &&
         report.present_fence_attach_count == report.frame_completed_count &&
         report.present_fence_wait_count == report.frame_completed_count &&
         report.present_fence_reset_count + 1U == report.frame_completed_count &&
         report.shutdown_presentation_completion_status == "present_fence_signaled");
    const bool complete = presentation_completion_valid &&
        report.loader_status == "success" &&
        report.instance_create_status == "success" &&
        report.surface_extension_status == "success" &&
        report.android_surface_extension_status == "success" &&
        report.native_window_status == "success" &&
        report.surface_create_status == "success" &&
        report.physical_device_status == "success" && report.target_gpu_match &&
        report.swapchain_extension_status == "success" &&
        report.queue_family_status == "success" &&
        report.graphics_queue_supported && report.present_queue_supported &&
        report.surface_capabilities_status == "success" &&
        report.surface_format_count > 0U && report.present_mode_count > 0U &&
        report.swapchain_create_status == "success" &&
        report.swapchain_image_query_status == "success" &&
        report.swapchain_image_count > 0U &&
        report.semaphore_create_status == "success" &&
        report.present_semaphore_create_status == "success" &&
        report.image_acquire_status == "success" && report.image_acquired &&
        report.command_pool_create_status == "success" &&
        report.command_buffer_allocate_status == "success" &&
        report.command_record_status == "success" &&
        report.image_layout_transition_recorded &&
        report.queue_submit_status == "success" &&
        report.image_present_status == "success" && report.image_presented &&
        report.queue_idle_status == "success" && report.cleanup_status == "success" &&
        report.semaphore_destroy_status == "success" &&
        report.present_semaphore_destroy_status == "success" &&
        report.command_buffer_free_status == "success" &&
        report.command_pool_destroy_status == "success" &&
        report.image_view_create_status == "success" &&
        report.render_pass_create_status == "success" &&
        report.framebuffer_create_status == "success" &&
        report.vertex_shader_module_create_status == "success" &&
        report.fragment_shader_module_create_status == "success" &&
        report.pipeline_layout_create_status == "success" &&
        report.graphics_pipeline_create_status == "success" &&
        report.render_pass_begin_recorded && report.render_pass_end_recorded &&
        report.graphics_pipeline_bind_recorded && report.draw_call_recorded &&
        report.draw_vertex_count == 3U &&
        report.graphics_pipeline_destroy_status == "success" &&
        report.pipeline_layout_destroy_status == "success" &&
        report.vertex_shader_module_destroy_status == "success" &&
        report.fragment_shader_module_destroy_status == "success" &&
        report.frame_content_written && report.frame_timing_valid &&
        report.frame_target_count == 16U &&
        report.frame_completed_count == report.frame_target_count + report.warmup_frame_count &&
        report.frame_timing_sample_count == report.frame_target_count &&
        report.gpu_timestamp_status == "success" &&
        report.gpu_timestamp_query_pool_create_status == "success" &&
        report.gpu_timestamp_query_status == "success" &&
        report.gpu_timestamp_sample_count == report.frame_target_count &&
        report.gpu_timestamp_query_pool_destroy_status == "success" &&
        report.image_view_destroy_status == "success" &&
        report.render_pass_destroy_status == "success" &&
        report.framebuffer_destroy_status == "success";
    if (complete && report.failure_reason.empty()) report.status = "success";
    else if (report.status != "failed") report.status = "failed";
    return report;
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_dev_nyamist_g925surface_MainActivity_validateSurfaceNative(
        JNIEnv* env, jclass, jobject surface, jint width, jint height) {
    const auto report = runValidation(env, surface, width, height);
    const std::string json = nyamist::discovery::toJson(report);
    return env->NewStringUTF(json.c_str());
}
