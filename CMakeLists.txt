cmake_minimum_required(VERSION 3.20)
project(nyamist_graphics_runtime VERSION 0.2.0 LANGUAGES CXX)

option(NYAMIST_BUILD_TESTS "Build NYAMIST unit tests" ON)
option(NYAMIST_BUILD_BENCHMARKS "Build NYAMIST microbenchmarks" ON)

add_library(nyamist_core
    src/cache_store.cpp
    src/fingerprint.cpp
    src/optimization_advisor.cpp
)
target_include_directories(nyamist_core PUBLIC include)
target_compile_features(nyamist_core PUBLIC cxx_std_20)

add_library(nyamist_runtime
    src/runtime/component_registry.cpp
    src/runtime/runtime_coordinator.cpp
    src/runtime/panvk_adapter.cpp
    src/runtime/translator_adapters.cpp
)
target_include_directories(nyamist_runtime PUBLIC include)
target_compile_features(nyamist_runtime PUBLIC cxx_std_20)
target_link_libraries(nyamist_runtime PUBLIC nyamist_core)

add_executable(nyamist-cli src/main.cpp)
target_link_libraries(nyamist-cli PRIVATE nyamist_core nyamist_runtime)

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

foreach(target nyamist_core nyamist_runtime)
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

    add_executable(nyamist-runtime-tests tests/runtime_tests.cpp)
    target_link_libraries(nyamist-runtime-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-runtime-tests COMMAND nyamist-runtime-tests)

    add_executable(nyamist-panvk-tests tests/panvk_adapter_tests.cpp)
    target_link_libraries(nyamist-panvk-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-panvk-adapter-tests COMMAND nyamist-panvk-tests)

    add_executable(nyamist-translator-tests tests/translator_adapter_tests.cpp)
    target_link_libraries(nyamist-translator-tests PRIVATE nyamist_runtime)
    add_test(NAME nyamist-translator-adapter-tests COMMAND nyamist-translator-tests)
endif()
