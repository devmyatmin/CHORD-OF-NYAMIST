cmake_minimum_required(VERSION 3.20)
project(nyamist_graphics_runtime VERSION 0.2.0 LANGUAGES CXX)

option(NYAMIST_BUILD_TESTS "Build NYAMIST unit tests" ON)
option(NYAMIST_BUILD_BENCHMARKS "Build NYAMIST developer microbenchmarks" OFF)
option(NYAMIST_ENABLE_VULKAN_DISCOVERY "Compile live Vulkan physical-device discovery when the SDK/loader is available" ON)
option(NYAMIST_FORCE_VULKAN_DYNAMIC_LOADING "Resolve Vulkan commands through dlopen/vkGetInstanceProcAddr (also used to test the Android path on desktop CI)" OFF)

add_library(nyamist_core
    src/cache_store.cpp
    src/fingerprint.cpp
    src/optimization_advisor.cpp
)
target_include_directories(nyamist_core PUBLIC include)
target_compile_features(nyamist_core PUBLIC cxx_std_20)

add_library(nyamist_discovery
    src/discovery/hardware_discovery.cpp
    src/discovery/logical_device_validation.cpp
    src/discovery/command_submission_validation.cpp
)
target_include_directories(nyamist_discovery PUBLIC include)
target_compile_features(nyamist_discovery PUBLIC cxx_std_20)

if(NYAMIST_ENABLE_VULKAN_DISCOVERY)
    set(NYAMIST_ANDROID_TARGET FALSE)
    if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android" OR
       CMAKE_CXX_COMPILER_TARGET MATCHES "android")
        set(NYAMIST_ANDROID_TARGET TRUE)
    else()
        include(CheckCXXSourceCompiles)
        check_cxx_source_compiles("
            #if !defined(__ANDROID__)
            #error Compiler target is not Android/Bionic
            #endif
            int main() { return 0; }
        " NYAMIST_COMPILER_TARGETS_ANDROID)
        if(NYAMIST_COMPILER_TARGETS_ANDROID)
            set(NYAMIST_ANDROID_TARGET TRUE)
        endif()
    endif()

    if(NYAMIST_ANDROID_TARGET OR NYAMIST_FORCE_VULKAN_DYNAMIC_LOADING)
        find_path(NYAMIST_VULKAN_INCLUDE_DIR vulkan/vulkan.h)
        if(NYAMIST_VULKAN_INCLUDE_DIR)
            target_include_directories(nyamist_discovery PRIVATE ${NYAMIST_VULKAN_INCLUDE_DIR})
            target_link_libraries(nyamist_discovery PRIVATE ${CMAKE_DL_LIBS})
            target_compile_definitions(nyamist_discovery PRIVATE
                NYAMIST_HAS_VULKAN=1 NYAMIST_VULKAN_DYNAMIC_LOADING=1)
            message(STATUS "NYAMIST Vulkan: Android public loader via dlopen/vkGetInstanceProcAddr")
        else()
            message(WARNING "Vulkan headers not found; live Vulkan discovery/adapter disabled")
        endif()
    else()
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            target_link_libraries(nyamist_discovery PRIVATE Vulkan::Vulkan)
            target_compile_definitions(nyamist_discovery PRIVATE NYAMIST_HAS_VULKAN=1)
        else()
            message(WARNING "Vulkan SDK not found; discovery will report Vulkan unavailable")
        endif()
    endif()
endif()

add_library(nyamist_runtime
    src/runtime/component_registry.cpp
    src/runtime/runtime_coordinator.cpp
    src/runtime/panvk_adapter.cpp
    src/runtime/device_probe_adapter.cpp
    src/runtime/session_snapshot.cpp
    src/runtime/pipeline_gate.cpp
    src/runtime/command_advisor.cpp
    src/runtime/profile_store.cpp
    src/runtime/ab_evaluator.cpp
    src/runtime/translator_adapters.cpp
    src/runtime/vulkan_backend.cpp
    src/runtime/vulkan_resource_manager.cpp
    src/runtime/vulkan_loader_adapter.cpp
)
target_include_directories(nyamist_runtime PUBLIC include)
target_compile_features(nyamist_runtime PUBLIC cxx_std_20)
target_link_libraries(nyamist_runtime PUBLIC nyamist_core)

if((NYAMIST_ANDROID_TARGET OR NYAMIST_FORCE_VULKAN_DYNAMIC_LOADING) AND NYAMIST_VULKAN_INCLUDE_DIR)
    target_include_directories(nyamist_runtime PRIVATE ${NYAMIST_VULKAN_INCLUDE_DIR})
    target_link_libraries(nyamist_runtime PRIVATE ${CMAKE_DL_LIBS})
    target_compile_definitions(nyamist_runtime PRIVATE NYAMIST_RUNTIME_HAS_VULKAN=1)
endif()

add_executable(nyamist-cli src/main.cpp)
target_link_libraries(nyamist-cli PRIVATE nyamist_core nyamist_runtime nyamist_discovery)

if(NYAMIST_BUILD_BENCHMARKS)
    add_executable(nyamist-cache-bench tools/cache_benchmark.cpp)
    target_link_libraries(nyamist-cache-bench PRIVATE nyamist_core)
    target_compile_features(nyamist-cache-bench PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(nyamist-cache-bench PRIVATE /W4 /permissive-)
    else()
        target_compile_options(nyamist-cache-bench PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endif()

foreach(target nyamist_core nyamist_discovery nyamist_runtime)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endforeach()

if(NYAMIST_BUILD_TESTS)
    include(CTest)
    add_executable(nyamist-tests tests/test_main.cpp)
    target_link_libraries(nyamist-tests PRIVATE nyamist_core)
    add_test(NAME nyamist-unit-tests COMMAND nyamist-tests)
    add_executable(nyamist-discovery-tests tests/hardware_discovery_tests.cpp)
    target_link_libraries(nyamist-discovery-tests PRIVATE nyamist_discovery)
    add_test(NAME nyamist-hardware-discovery-tests COMMAND nyamist-discovery-tests)
    add_executable(nyamist-logical-device-validation-tests tests/logical_device_validation_tests.cpp)
    target_link_libraries(nyamist-logical-device-validation-tests PRIVATE nyamist_discovery)
    add_test(NAME nyamist-logical-device-validation-tests COMMAND nyamist-logical-device-validation-tests)
    add_executable(nyamist-command-submission-validation-tests tests/command_submission_validation_tests.cpp)
    target_link_libraries(nyamist-command-submission-validation-tests PRIVATE nyamist_discovery)
    add_test(NAME nyamist-command-submission-validation-tests COMMAND nyamist-command-submission-validation-tests)
    add_executable(nyamist-runtime-tests tests/runtime_tests.cpp)
    target_link_libraries(nyamist-runtime-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-runtime-tests COMMAND nyamist-runtime-tests)
    add_executable(nyamist-vulkan-backend-tests tests/vulkan_backend_tests.cpp)
    target_link_libraries(nyamist-vulkan-backend-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-vulkan-backend-tests COMMAND nyamist-vulkan-backend-tests)
    add_executable(nyamist-vulkan-resource-manager-tests tests/vulkan_resource_manager_tests.cpp)
    target_link_libraries(nyamist-vulkan-resource-manager-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-vulkan-resource-manager-tests COMMAND nyamist-vulkan-resource-manager-tests)
    add_executable(nyamist-panvk-tests tests/panvk_adapter_tests.cpp)
    target_link_libraries(nyamist-panvk-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-panvk-adapter-tests COMMAND nyamist-panvk-tests)
    add_executable(nyamist-translator-tests tests/translator_adapter_tests.cpp)
    target_link_libraries(nyamist-translator-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-translator-adapter-tests COMMAND nyamist-translator-tests)
    add_executable(nyamist-device-probe-tests tests/device_probe_adapter_tests.cpp)
    target_link_libraries(nyamist-device-probe-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-device-probe-adapter-tests COMMAND nyamist-device-probe-tests)
    add_executable(nyamist-session-snapshot-tests tests/session_snapshot_tests.cpp)
    target_link_libraries(nyamist-session-snapshot-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-session-snapshot-tests COMMAND nyamist-session-snapshot-tests)
    add_executable(nyamist-pipeline-gate-tests tests/pipeline_gate_tests.cpp)
    target_link_libraries(nyamist-pipeline-gate-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-pipeline-gate-tests COMMAND nyamist-pipeline-gate-tests)
    add_executable(nyamist-command-advisor-tests tests/command_advisor_tests.cpp)
    target_link_libraries(nyamist-command-advisor-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-command-advisor-tests COMMAND nyamist-command-advisor-tests)
    add_executable(nyamist-profile-store-tests tests/profile_store_tests.cpp)
    target_link_libraries(nyamist-profile-store-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-profile-store-tests COMMAND nyamist-profile-store-tests)
    add_executable(nyamist-ab-evaluator-tests tests/ab_evaluator_tests.cpp)
    target_link_libraries(nyamist-ab-evaluator-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-ab-evaluator-tests COMMAND nyamist-ab-evaluator-tests)
endif()
