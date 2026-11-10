cmake_minimum_required(VERSION 3.22.1)
project(nyamist_surface_validation LANGUAGES CXX)

get_filename_component(NYAMIST_REPO_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../.." ABSOLUTE)

find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(NYAMIST_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/../shaders")
set(NYAMIST_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(NYAMIST_SHADER_HEADER "${NYAMIST_SHADER_OUTPUT_DIR}/triangle_shaders.hpp")
set(NYAMIST_VERTEX_SPIRV "${NYAMIST_SHADER_OUTPUT_DIR}/triangle.vert.spv")
set(NYAMIST_FRAGMENT_SPIRV "${NYAMIST_SHADER_OUTPUT_DIR}/triangle.frag.spv")

add_custom_command(
    OUTPUT "${NYAMIST_SHADER_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${NYAMIST_SHADER_OUTPUT_DIR}"
    COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.0
            "${NYAMIST_SHADER_DIR}/triangle.vert" -o "${NYAMIST_VERTEX_SPIRV}"
    COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.0
            "${NYAMIST_SHADER_DIR}/triangle.frag" -o "${NYAMIST_FRAGMENT_SPIRV}"
    COMMAND ${Python3_EXECUTABLE}
            "${NYAMIST_REPO_ROOT}/tools/embed_spirv.py"
            --vertex "${NYAMIST_VERTEX_SPIRV}"
            --fragment "${NYAMIST_FRAGMENT_SPIRV}"
            --output "${NYAMIST_SHADER_HEADER}"
    DEPENDS
        "${NYAMIST_SHADER_DIR}/triangle.vert"
        "${NYAMIST_SHADER_DIR}/triangle.frag"
        "${NYAMIST_REPO_ROOT}/tools/embed_spirv.py"
    VERBATIM
)
add_custom_target(nyamist_triangle_shaders DEPENDS "${NYAMIST_SHADER_HEADER}")

add_library(nyamist_surface_validation SHARED
    native_surface_swapchain.cpp
    "${NYAMIST_REPO_ROOT}/src/discovery/android_surface_swapchain_validation.cpp"
)

# Prefer a pinned Khronos Vulkan-Headers revision for Vulkan 1.4
# declarations that may be newer than the Android NDK's bundled headers.
set(NYAMIST_VULKAN_HEADERS_ROOT "$ENV{NYAMIST_VULKAN_HEADERS_ROOT}")
if(NYAMIST_VULKAN_HEADERS_ROOT AND
   EXISTS "${NYAMIST_VULKAN_HEADERS_ROOT}/include/vulkan/vulkan.h")
    target_include_directories(nyamist_surface_validation BEFORE PRIVATE
        "${NYAMIST_VULKAN_HEADERS_ROOT}/include")
    message(STATUS "NYAMIST Vulkan headers: ${NYAMIST_VULKAN_HEADERS_ROOT}")
else()
    message(FATAL_ERROR
        "R2.3 requires official Vulkan-Headers v1.4.368; set NYAMIST_VULKAN_HEADERS_ROOT")
endif()

target_include_directories(nyamist_surface_validation PRIVATE
    "${NYAMIST_REPO_ROOT}/include"
    "${NYAMIST_SHADER_OUTPUT_DIR}"
)
add_dependencies(nyamist_surface_validation nyamist_triangle_shaders)

target_compile_features(nyamist_surface_validation PRIVATE cxx_std_20)
find_library(android-lib android REQUIRED)
find_library(log-lib log REQUIRED)

target_link_libraries(nyamist_surface_validation PRIVATE
    ${CMAKE_DL_LIBS}
    ${android-lib}
    ${log-lib}
)
