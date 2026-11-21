add_executable(pes17_host
    runtime/main.cpp
    # ... (deixe todos os outros arquivos que já estavam aqui) ...
    runtime/win32/win32_stubs.cpp
    runtime/win32/win32_file_stubs.cpp
)

target_include_directories(pes17_host PRIVATE runtime runtime/win32)
