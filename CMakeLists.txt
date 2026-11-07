cmake_minimum_required(VERSION 3.20)
project(nyamist_layer VERSION 0.1.0 LANGUAGES CXX)

option(NYAMIST_BUILD_TESTS "Build NYAMIST unit tests" ON)

add_library(nyamist_core
    src/cache_store.cpp
    src/fingerprint.cpp
    src/optimization_advisor.cpp
)
target_include_directories(nyamist_core PUBLIC include)
target_compile_features(nyamist_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(nyamist_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(nyamist_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(nyamist-cli src/main.cpp)
target_link_libraries(nyamist-cli PRIVATE nyamist_core)

if(NYAMIST_BUILD_TESTS)
    include(CTest)
    add_executable(nyamist-tests tests/test_main.cpp)
    target_link_libraries(nyamist-tests PRIVATE nyamist_core)
    add_test(NAME nyamist-unit-tests COMMAND nyamist-tests)
endif()
