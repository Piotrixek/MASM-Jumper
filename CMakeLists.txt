cmake_minimum_required(VERSION 3.20)
project(MasmJumperWin32 CXX ASM_MASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(MasmJumperWin32 WIN32 main.cpp jumper_logic.asm)

if(MSVC)
    target_compile_options(MasmJumperWin32 PRIVATE /std:c++latest /EHsc /W4)
endif()