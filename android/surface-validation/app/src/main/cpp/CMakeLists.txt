cmake_minimum_required(VERSION 3.22.1)
project(nyamist_surface_validation LANGUAGES CXX)

get_filename_component(NYAMIST_REPO_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../.." ABSOLUTE)

add_library(nyamist_surface_validation SHARED
    native_surface_swapchain.cpp
    "${NYAMIST_REPO_ROOT}/src/discovery/android_surface_swapchain_validation.cpp"
)

target_include_directories(nyamist-surface-validation PRIVATE
    "${NYAMIST_REPO_ROOT}/include"
)

target_compile_features(nyamist-surface-validation PRIVATE cxx_std_20)
find_library(android-lib android REQUIRED)
find_library(log-lib log REQUIRED)

target_link_libraries(nyamist-surface-validation PRIVATE
    ${CMAKE_DL_LIBS}
    ${android-lib}
    ${log-lib}
)
