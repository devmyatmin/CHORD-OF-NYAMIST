#include <cstdint>
#include <cstring>
#include <array>
#include "nyamist/cache_store.hpp"
#include "nyamist/discovery/hardware_discovery.hpp"
#include "nyamist/discovery/logical_device_validation.hpp"
#include "nyamist/discovery/command_submission_validation.hpp"
#include "nyamist/fingerprint.hpp"
#include "nyamist/optimization_advisor.hpp"
#include "nyamist/runtime/device_probe_adapter.hpp"
#include "nyamist/runtime/vulkan_driver_adapter.hpp"
#include "nyamist/runtime/translator_adapters.hpp"
#include "nyamist/runtime/vulkan_loader_adapter.hpp"
#include "nyamist/runtime/vulkan_compute_pipeline_manager.hpp"
#include "nyamist/runtime/vulkan_backend.hpp"
#include <vulkan/vulkan.h>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>
#include <utility>

namespace {

void printUsage() {
    std::cout
        << "NYAMIST Graphics Runtime\n"
        << "Usage:\n"
        << "  nyamist-cli advice\n"
        << "  nyamist-cli fingerprint <text>\n"
        << "  nyamist-cli demo-cache <directory>\n"
        << "  nyamist-cli discover [--output <report.json>]\n"
        << "  nyamist-cli validate-device [--output <report.json>]\n"
        << "  nyamist-cli validate-submit [--output <report.json>]\n"
        << "  nyamist-cli runtime-vulkan-smoke\n"
        << "  nyamist-cli runtime-vulkan-render-pass-smoke\n"
        << "  nyamist-cli runtime-vulkan-buffer-smoke\n"
        << "  nyamist-cli runtime-vulkan-compute-pipeline-smoke <shader.spv>\n"
        << "  nyamist-cli inspect-vulkan-driver <report.env>\n"
        << "  nyamist-cli inspect-device <report.env>\n"
        << "  nyamist-cli inspect-dxvk <report.env>\n"
        << "  nyamist-cli inspect-vkd3d <report.env>\n";
}

const char* validationName(nyamist::runtime::ValidationLevel level) {
    using nyamist::runtime::ValidationLevel;
    switch (level) {
    case ValidationLevel::unavailable: return "unavailable";
    case ValidationLevel::discovered: return "discovered";
    case ValidationLevel::smoke_tested: return "smoke_tested";
    case ValidationLevel::sustained_tested: return "sustained_tested";
    }
    return "unknown";
}

int runDiscover(int argc, char** argv) {
    std::string output_path;
    if (argc == 4 && std::string(argv[2]) == "--output") {
        output_path = argv[3];
    } else if (argc != 2) {
        printUsage();
        return 2;
    }

    const auto report = nyamist::discovery::collectHardwareDiscoveryReport();
    const std::string json = nyamist::discovery::toJson(report);
    if (output_path.empty()) {
        std::cout << json;
        return std::cout.good() ? 0 : 1;
    }

    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Unable to create discovery report: " << output_path << '\n';
        return 1;
    }
    output << json;
    if (!output.good()) {
        std::cerr << "Failed while writing discovery report: " << output_path << '\n';
        return 1;
    }
    std::cout << "NYAMIST hardware discovery report: " << output_path << '\n'
              << "Vulkan enumeration status: " << report.vulkan_status << '\n';
    if (!report.vulkan_failure_reason.empty()) {
        std::cout << "Vulkan note: " << report.vulkan_failure_reason << '\n';
    }
    return 0;
}

int runValidateDevice(int argc, char** argv) {
    std::string output_path;
    if (argc == 4 && std::string(argv[2]) == "--output") {
        output_path = argv[3];
    } else if (argc != 2) {
        printUsage();
        return 2;
    }

    const auto report =
        nyamist::discovery::collectLogicalDeviceValidationReport();
    const std::string json = nyamist::discovery::toJson(report);
    if (output_path.empty()) {
        std::cout << json;
        if (!std::cout.good()) return 1;
    } else {
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            std::cerr << "Unable to create logical-device report: "
                      << output_path << '\n';
            return 1;
        }
        output << json;
        if (!output.good()) {
            std::cerr << "Failed while writing logical-device report: "
                      << output_path << '\n';
            return 1;
        }
        std::cout << "NYAMIST R1.2 logical-device report: "
                  << output_path << '\n'
                  << "Validation status: " << report.status << '\n'
                  << "Physical device: "
                  << (report.device_name.empty() ? "not identified" : report.device_name)
                  << '\n'
                  << "Target G925 name match: "
                  << (report.target_gpu_match ? "yes" : "no") << '\n'
                  << "Logical device: " << report.logical_device_status << '\n'
                  << "Queue retrieval: " << report.queue_retrieval_status << '\n'
                  << "Cleanup: " << report.cleanup_status << '\n';
        if (!report.failure_reason.empty()) {
            std::cout << "Failure reason: " << report.failure_reason << '\n';
        }
    }
    return report.status == "success" ? 0 : 1;
}

int runValidateSubmit(int argc, char** argv) {
    std::string output_path;
    if (argc == 4 && std::string(argv[2]) == "--output") {
        output_path = argv[3];
    } else if (argc != 2) {
        printUsage();
        return 2;
    }

    const auto report =
        nyamist::discovery::collectCommandSubmissionValidationReport();
    const std::string json = nyamist::discovery::toJson(report);
    if (output_path.empty()) {
        std::cout << json;
        if (!std::cout.good()) return 1;
    } else {
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            std::cerr << "Unable to create command-submission report: "
                      << output_path << '\n';
            return 1;
        }
        output << json;
        if (!output.good()) {
            std::cerr << "Failed while writing command-submission report: "
                      << output_path << '\n';
            return 1;
        }
        std::cout << "NYAMIST R1.3 command-submission report: "
                  << output_path << '\n'
                  << "Validation status: " << report.status << '\n'
                  << "Physical device: "
                  << (report.device_name.empty() ? "not identified" : report.device_name)
                  << '\n'
                  << "Queue submission: " << report.queue_submit_status << '\n'
                  << "Fence wait: " << report.fence_wait_status << '\n'
                  << "Fence signaled: " << (report.fence_signaled ? "yes" : "no") << '\n'
                  << "Device lost: " << (report.device_lost ? "yes" : "no") << '\n'
                  << "Cleanup: " << report.cleanup_status << '\n';
        if (!report.failure_reason.empty()) {
            std::cout << "Failure reason: " << report.failure_reason << '\n';
        }
    }
    return report.status == "success" ? 0 : 1;
}

int runRuntimeVulkanSmoke() {
    nyamist::runtime::VulkanLoaderAdapter loader;
    std::string error;
    if (!loader.open(&error)) {
        std::cerr << "Vulkan loader unavailable: " << error << '\n';
        return 1;
    }

    nyamist::runtime::VulkanBackendSession backend(loader.dispatch());
    if (!backend.initialize()) {
        std::cerr << "NYAMIST Vulkan backend initialization failed: "
                  << backend.status().error << '\n';
        backend.shutdown();
        loader.close();
        return 1;
    }

    std::cout << "NYAMIST Vulkan backend initialized\n"
              << "Graphics queue family: " << backend.status().graphics_queue_family << '\n';
    if (!backend.submit_smoke_command()) {
        std::cerr << "Vulkan smoke submission failed: "
                  << backend.status().error << '\n';
        backend.shutdown();
        loader.close();
        return 1;
    }

    std::cout << "Empty command submitted and fence-wait completed\n"
              << "Completed smoke submissions: "
              << backend.status().completed_smoke_submissions << '\n'
              << "Rendering: not tested by this command\n";
    backend.shutdown();
    loader.close();
    return 0;
}

int runRuntimeVulkanBufferSmoke() {
    nyamist::runtime::VulkanLoaderAdapter loader;
    std::string error;
    if (!loader.open(&error)) {
        std::cerr << "Vulkan loader unavailable: " << error << '\n';
        return 1;
    }

    nyamist::runtime::VulkanBackendSession backend(loader.dispatch());
    if (!backend.initialize()) {
        std::cerr << "NYAMIST Vulkan backend initialization failed: "
                  << backend.status().error << '\n';
        backend.shutdown();
        loader.close();
        return 1;
    }

    auto memory_types = loader.memory_types();
    if (memory_types.empty()) {
        std::cerr << "No Vulkan physical-device memory types were reported\n";
        backend.shutdown();
        loader.close();
        return 1;
    }

    nyamist::runtime::VulkanBufferResourceManager resources(
        loader.buffer_dispatch(), std::move(memory_types));
    nyamist::runtime::VulkanBufferAllocation allocation{};
    // Vulkan core flag values: TRANSFER_SRC=0x1, TRANSFER_DST=0x2,
    // DEVICE_LOCAL=0x1. The adapter forwards these to the Vulkan API.
    const nyamist::runtime::VulkanBufferRequest request{
        4096U, 0x00000001U | 0x00000002U, 0U, 0x00000001U};
    if (!resources.create(request, &allocation, &error)) {
        std::cerr << "Native Vulkan buffer allocation failed: " << error << '\n';
        backend.shutdown();
        loader.close();
        return 1;
    }

    std::cout << "Native Vulkan buffer created and memory bound\n"
              << "Buffer size requested: " << request.size << " bytes\n"
              << "Memory allocation: " << allocation.allocation_size << " bytes\n"
              << "Memory type index: " << allocation.memory_type_index << '\n';
    const std::array<std::uint8_t, 16> write_data{{
        0x4EU, 0x59U, 0x41U, 0x4DU, 0x49U, 0x53U, 0x54U, 0x21U,
        0x10U, 0x20U, 0x30U, 0x40U, 0x50U, 0x60U, 0x70U, 0x80U
    }};
    std::array<std::uint8_t, 16> read_data{};

    if (!resources.write(allocation, write_data.data(),
                         write_data.size(), &error)) {
        std::cerr << "Native Vulkan buffer write failed: " << error << '\n';
        resources.destroy(&allocation);
        backend.shutdown();
        loader.close();
        return 1;
    }

    if (!resources.read(allocation, read_data.data(),
                        read_data.size(), &error)) {
        std::cerr << "Native Vulkan buffer read failed: " << error << '\n';
        resources.destroy(&allocation);
        backend.shutdown();
        loader.close();
        return 1;
    }

    if (std::memcmp(write_data.data(), read_data.data(),
                    write_data.size()) != 0) {
        std::cerr << "Vulkan buffer read-back data mismatch\n";
        resources.destroy(&allocation);
        backend.shutdown();
        loader.close();
        return 1;
    }

    std::cout << "Native Vulkan buffer write/read verification: PASS\n";
    resources.destroy(&allocation);
    std::cout << "Buffer and device memory released successfully\n";
    backend.shutdown();
    loader.close();
    return 0;
}


int runRuntimeVulkanComputePipelineSmoke(const std::string& shader_path) {
    std::ifstream input(shader_path, std::ios::binary | std::ios::ate);
    if (!input) {
        std::cerr << "Unable to open SPIR-V shader: " << shader_path << '\n';
        return 1;
    }

    const auto end = input.tellg();
    if (end < 20 || static_cast<std::uint64_t>(end) % 4U != 0U) {
        std::cerr << "SPIR-V file has an invalid size\n";
        return 1;
    }

    std::vector<std::uint32_t> spirv(
        static_cast<std::size_t>(end) / sizeof(std::uint32_t));

    input.seekg(0);
    input.read(
        reinterpret_cast<char*>(spirv.data()),
        static_cast<std::streamsize>(end));

    if (!input) {
        std::cerr << "Failed to read SPIR-V shader\n";
        return 1;
    }

    nyamist::runtime::VulkanLoaderAdapter loader;
    std::string error;

    if (!loader.open(&error)) {
        std::cerr << "Vulkan loader unavailable: " << error << '\n';
        return 1;
    }

    nyamist::runtime::VulkanBackendSession backend(loader.dispatch());

    if (!backend.initialize()) {
        std::cerr << "Vulkan backend initialization failed: "
                  << backend.status().error << '\n';
        backend.shutdown();
        loader.close();
        return 1;
    }

    nyamist::runtime::VulkanComputePipelineManager manager(
        loader.compute_dispatch());

    nyamist::runtime::VulkanComputePipelineAllocation pipeline{};

    if (!manager.create(spirv, &pipeline, &error)) {
        std::cerr << "Compute pipeline creation failed: " << error << '\n';
        manager.destroy(&pipeline);
        backend.shutdown();
        loader.close();
        return 1;
    }

    auto memory_types = loader.memory_types();

    if (memory_types.empty()) {
        std::cerr << "No Vulkan memory types available\n";
        manager.destroy(&pipeline);
        backend.shutdown();
        loader.close();
        return 1;
    }

    nyamist::runtime::VulkanBufferResourceManager resources(
        loader.buffer_dispatch(), std::move(memory_types));

    nyamist::runtime::VulkanBufferAllocation buffer{};

    // VK_BUFFER_USAGE_STORAGE_BUFFER_BIT = 0x20.
    // HOST_VISIBLE = 0x2; HOST_COHERENT = 0x4.
    const nyamist::runtime::VulkanBufferRequest request{
        sizeof(std::uint32_t), 0x20U, 0x2U, 0x4U
    };

    if (!resources.create(request, &buffer, &error)) {
        std::cerr << "Compute storage buffer creation failed: "
                  << error << '\n';
        manager.destroy(&pipeline);
        backend.shutdown();
        loader.close();
        return 1;
    }

    const std::uint32_t initial_value = 41U;
    std::uint32_t result_value = 0U;

    if (!resources.write(
            buffer, &initial_value, sizeof(initial_value), &error)) {
        std::cerr << "Initial storage buffer write failed: "
                  << error << '\n';
        resources.destroy(&buffer);
        manager.destroy(&pipeline);
        backend.shutdown();
        loader.close();
        return 1;
    }

    auto compute = loader.compute_dispatch();

    if (!compute.update_storage_buffer_descriptor ||
        !compute.dispatch_compute) {
        std::cerr << "Compute execution dispatch table is incomplete\n";
        resources.destroy(&buffer);
        manager.destroy(&pipeline);
        backend.shutdown();
        loader.close();
        return 1;
    }

    if (!compute.update_storage_buffer_descriptor(
            compute.context,
            pipeline.descriptor_set,
            buffer.buffer,
            sizeof(std::uint32_t),
            &error)) {
        std::cerr << "Storage buffer descriptor update failed: "
                  << error << '\n';
        resources.destroy(&buffer);
        manager.destroy(&pipeline);
        backend.shutdown();
        loader.close();
        return 1;
    }

    std::cout << "Compute storage buffer: created and initialized\n"
              << "Initial value: " << initial_value << '\n'
              << "Storage-buffer descriptor: updated\n";

    if (!compute.dispatch_compute(
            compute.context,
            pipeline.pipeline,
            pipeline.pipeline_layout,
            pipeline.descriptor_set,
            1U, 1U, 1U,
            &error)) {
        std::cerr << "GPU compute dispatch failed: " << error << '\n';
        resources.destroy(&buffer);
        manager.destroy(&pipeline);
        backend.shutdown();
        loader.close();
        return 1;
    }

    std::cout << "Native Vulkan compute dispatch: submitted\n"
              << "GPU completion fence: signaled\n";

    if (!resources.read(
            buffer, &result_value, sizeof(result_value), &error)) {
        std::cerr << "GPU result readback failed: " << error << '\n';
        resources.destroy(&buffer);
        manager.destroy(&pipeline);
        backend.shutdown();
        loader.close();
        return 1;
    }

    std::cout << "GPU readback value: " << result_value << '\n';

    const bool passed = result_value == initial_value + 1U;

    resources.destroy(&buffer);
    manager.destroy(&pipeline);
    backend.shutdown();
    loader.close();

    if (!passed) {
        std::cerr << "GPU COMPUTE VERIFICATION: FAIL\n";
        return 1;
    }

    std::cout << "GPU COMPUTE VERIFICATION: PASS\n"
              << "Expected result: 42\n"
              << "Storage buffer and pipeline resources released\n";

    return 0;
}


int runRuntimeVulkanRenderPassSmoke() {
    nyamist::runtime::VulkanLoaderAdapter loader;
    std::string error;

    if (!loader.open(&error)) {
        std::cerr << "Vulkan loader unavailable: " << error << '\n';
        return 1;
    }

    nyamist::runtime::VulkanBackendSession backend(loader.dispatch());
    if (!backend.initialize()) {
        std::cerr << "Vulkan backend initialization failed: "
                  << backend.status().error << '\n';
        backend.shutdown();
        loader.close();
        return 1;
    }

    nyamist::runtime::NativeHandle render_pass = 0;
    const auto format =
        static_cast<std::uint32_t>(VK_FORMAT_R8G8B8A8_UNORM);

    if (!loader.create_color_render_pass(format, &render_pass, &error)) {
        std::cerr << "Render Pass creation failed: " << error << '\n';
        backend.shutdown();
        loader.close();
        return 1;
    }

    if (render_pass == 0) {
        std::cerr << "Render Pass creation returned a null handle\n";
        backend.shutdown();
        loader.close();
        return 1;
    }

    std::cout << "Native Vulkan color Render Pass: created\n";

    loader.destroy_render_pass(render_pass);
    std::cout << "Native Vulkan color Render Pass: destroyed\n";

    backend.shutdown();
    loader.close();

    std::cout << "RENDER PASS CREATE/DESTROY: PASS\n";
    return 0;
}

int runAdvice() {
    nyamist::GpuProfile gpu;
    gpu.gpu_name = "REDMI K80 Ultra (target profile; runtime probe pending)";
    gpu.gpu_architecture = "Arm Immortalis-G925";
    gpu.vulkan_api_version = "probe-required";
    gpu.driver_version = "probe-required";
    gpu.logical_cpu_count = 8;

    nyamist::ShaderProfile shader;
    const nyamist::RuleBasedAdvisor advisor;
    const auto plan = advisor.recommend(gpu, shader);

    std::cout << "GPU: " << gpu.gpu_name << '\n'
              << "Architecture: " << gpu.gpu_architecture << '\n'
              << "Graphics Pipeline Library: "
              << (plan.use_graphics_pipeline_library ? "reported by profile" : "not confirmed") << '\n'
              << "Suggested shader workers: " << plan.suggested_shader_workers << '\n';
    for (const auto& note : plan.notes) {
        std::cout << "[note] " << note << '\n';
    }
    for (const auto& warning : plan.compatibility_warnings) {
        std::cout << "[warning] " << warning << '\n';
    }
    return 0;
}

int runDemoCache(const std::string& directory) {
    nyamist::CacheStore cache(directory);
    nyamist::CacheIdentity identity;
    identity.source_fingerprint = nyamist::fingerprint("demo-d3d11-shader-bytecode");
    identity.gpu_architecture = "Immortalis-G925";
    identity.vulkan_api_version = "runtime-probe-required";
    identity.driver_version = "runtime-probe-required";

    const std::string demo = "validated-demo-optimization-metadata";
    const auto bytes = std::as_bytes(std::span(demo.data(), demo.size()));
    cache.saveValidated(identity, bytes, true);

    const auto restored = cache.load(identity);
    if (!restored) {
        std::cerr << "Cache round-trip failed.\n";
        return 1;
    }
    std::cout << "Validated cache entry stored and loaded. Key: "
              << cache.keyFor(identity) << '\n';
    std::cout << "This is a cache-core demo, not a translated game shader.\n";
    return 0;
}

void printMonitorReport(const nyamist::runtime::ComponentReport& report) {
    std::cout << "Component: " << report.descriptor.id << '\n'
              << "Origin: " << report.descriptor.origin << '\n'
              << "Validation: " << validationName(report.validation) << '\n';
    for (const auto& capability : report.capabilities) {
        std::cout << "[capability] " << capability << '\n';
    }
    for (const auto& diagnostic : report.diagnostics) {
        std::cout << "[diagnostic] " << diagnostic << '\n';
    }
}

int runInspectDevice(const std::string& report_path) {
    try {
        const auto snapshot = nyamist::runtime::DeviceProbeReportParser::fromFile(report_path);
        const auto device = nyamist::runtime::DeviceFactsAdapter(snapshot).inspect();
        const auto vulkan = nyamist::runtime::VulkanCapabilityAdapter(snapshot).inspect();
        printMonitorReport(device);
        std::cout << '\n';
        printMonitorReport(vulkan);
        return device.validation == nyamist::runtime::ValidationLevel::unavailable ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Device probe inspection failed: " << error.what() << '\n';
        return 1;
    }
}

int runInspectVulkanDriver(const std::string& report_path) {
    try {
        const auto adapter =
            nyamist::runtime::VulkanDriverAdapter::fromReportFile(report_path);
        const auto report = adapter.inspect();

        std::cout << "Component: " << report.descriptor.id << '\n'
                  << "Driver: " << report.descriptor.version << '\n'
                  << "Origin: " << report.descriptor.origin << '\n'
                  << "Validation: " << validationName(report.validation) << '\n';

        for (const auto& capability : report.capabilities) {
            std::cout << "[capability] " << capability << '\n';
        }
        for (const auto& diagnostic : report.diagnostics) {
            std::cout << "[diagnostic] " << diagnostic << '\n';
        }
        return report.validation == nyamist::runtime::ValidationLevel::unavailable ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Vulkan driver report inspection failed: " << error.what() << '\n';
        return 1;
    }
}

template <typename Adapter>
int runInspectTranslator(const std::string& report_path, const char* label) {
    try {
        const auto adapter = Adapter::fromReportFile(report_path);
        const auto report = adapter.inspect();

        std::cout << "Component: " << report.descriptor.id << '\n'
                  << "Version: " << report.descriptor.version << '\n'
                  << "Origin: " << report.descriptor.origin << '\n'
                  << "Validation: " << validationName(report.validation) << '\n';

        for (const auto& capability : report.capabilities) {
            std::cout << "[capability] " << capability << '\n';
        }
        for (const auto& diagnostic : report.diagnostics) {
            std::cout << "[diagnostic] " << diagnostic << '\n';
        }
        return report.validation == nyamist::runtime::ValidationLevel::unavailable ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << label << " report inspection failed: " << error.what() << '\n';
        return 1;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 2;
    }

    const std::string command = argv[1];
    if (command == "advice") {
        return runAdvice();
    }
    if (command == "fingerprint" && argc >= 3) {
        std::cout << nyamist::fingerprint(argv[2]) << '\n';
        return 0;
    }
    if (command == "demo-cache" && argc >= 3) {
        return runDemoCache(argv[2]);
    }
    if (command == "discover") {
        return runDiscover(argc, argv);
    }
    if (command == "validate-device") {
        return runValidateDevice(argc, argv);
    }
    if (command == "runtime-vulkan-render-pass-smoke" && argc == 2) {
        return runRuntimeVulkanRenderPassSmoke();
    }
    if (command == "runtime-vulkan-smoke" && argc == 2) {
        return runRuntimeVulkanSmoke();
    }
    if (command == "runtime-vulkan-compute-pipeline-smoke" && argc == 3) {
        return runRuntimeVulkanComputePipelineSmoke(argv[2]);
    }
    if (command == "runtime-vulkan-buffer-smoke" && argc == 2) {
        return runRuntimeVulkanBufferSmoke();
    }
    if (command == "validate-submit") {
        return runValidateSubmit(argc, argv);
    }
    if (command == "inspect-vulkan-driver" && argc == 3) {
        return runInspectVulkanDriver(argv[2]);
    }
    if (command == "inspect-device" && argc == 3) {
        return runInspectDevice(argv[2]);
    }
    if (command == "inspect-dxvk" && argc == 3) {
        return runInspectTranslator<nyamist::runtime::DxvkAdapter>(argv[2], "DXVK");
    }
    if (command == "inspect-vkd3d" && argc == 3) {
        return runInspectTranslator<nyamist::runtime::Vkd3dProtonAdapter>(argv[2], "VKD3D-Proton");
    }

    printUsage();
    return 2;
}
