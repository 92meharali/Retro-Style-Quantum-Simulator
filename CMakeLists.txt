cmake_minimum_required(VERSION 3.20)
project(QuantumSimulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Vendored dependencies (see third_party/)
add_library(nlohmann_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CATCH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/Catch2-3.5.2 EXCLUDE_FROM_ALL)

add_subdirectory(quantum-core)

option(QSIM_BUILD_TESTS "Build unit tests" ON)
if(QSIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

option(QSIM_BUILD_UI "Build desktop UI" ON)
if(QSIM_BUILD_UI)
    if(NOT WIN32)
        find_package(OpenGL REQUIRED)
    endif()

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/glfw-3.4 EXCLUDE_FROM_ALL)

    add_subdirectory(ui)
endif()
