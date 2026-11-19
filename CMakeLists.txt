cmake_minimum_required(VERSION 3.16)
project(pes17_native_recomp VERSION 0.0.1 LANGUAGES CXX)

# Força a biblioteca C++ a ser estática (embutida no .so) - Essencial para Android 14+
set(ANDROID_STL c++_static)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apenas o arquivo JNI para o teste de tela
add_library(pes17 SHARED
    jni_bridge.cpp
)

target_compile_options(pes17 PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Linka as bibliotecas básicas do Android (Log e Nativa)
target_link_libraries(pes17 PRIVATE
    log
    android
)
