cmake_minimum_required(VERSION 3.20)
project(flux-compiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_CMAKE_DIR}")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_compile_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(${LLVM_INCLUDE_DIRS})

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(fluxc ${SOURCES})

target_include_directories(fluxc PRIVATE include)

# Компоненты LLVM, которые нужны
llvm_map_components_to_libnames(llvm_libs
    core                    # Основной IR
    support                 # Базовые утилиты  
    native                  # Target-specific codegen
    codegen                 # Code generation
    target                  # Target info
    irreader                # LLVM IR parsing
    transformutils          # Pass utilities
    analysis                # Analysis passes
)

target_link_libraries(fluxc PRIVATE ${llvm_libs})
target_compile_options(fluxc PRIVATE -O2)