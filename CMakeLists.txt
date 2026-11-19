cmake_minimum_required(VERSION 3.16)
project(pes17_native_recomp VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# APENAS O JNI BRIDGE POR ENQUANTO
add_library(pes17 SHARED
    jni_bridge.cpp
)

target_compile_options(pes17 PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(pes17 PRIVATE log android)
