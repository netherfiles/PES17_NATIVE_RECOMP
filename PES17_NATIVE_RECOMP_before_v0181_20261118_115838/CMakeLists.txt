cmake_minimum_required(VERSION 3.16)

project(pes17_native_recomp VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pes17_host
    runtime/main.cpp
    runtime/core/build_info.cpp
    runtime/pe/pe_image.cpp
    runtime/x86/decoder.cpp
    runtime/analysis/basic_block_builder.cpp
    runtime/analysis/function_analyzer.cpp
    runtime/analysis/function_normalizer.cpp
    runtime/ir/ir.cpp
    runtime/ir/operand.cpp
    runtime/ir/x86_lifter.cpp
    runtime/ir/verifier.cpp
    runtime/execution/machine_state.cpp
    runtime/execution/machine_state_test.cpp
    runtime/execution/executor.cpp
    runtime/execution/function_executor.cpp
    runtime/codegen/cpp_generator.cpp
    runtime/codegen/batch_recompiler.cpp
    runtime/execution/function_executor_test.cpp
    runtime/execution/executor_test.cpp
    runtime/analysis/call_graph_analyzer.cpp
)

target_include_directories(pes17_host PRIVATE runtime)

target_compile_options(pes17_host PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)


find_package(PkgConfig REQUIRED)
pkg_check_modules(CAPSTONE REQUIRED capstone)

target_include_directories(pes17_host PRIVATE
    ${CAPSTONE_INCLUDE_DIRS}
)

target_link_directories(pes17_host PRIVATE
    ${CAPSTONE_LIBRARY_DIRS}
)

target_link_libraries(pes17_host PRIVATE
    ${CAPSTONE_LIBRARIES}
)

