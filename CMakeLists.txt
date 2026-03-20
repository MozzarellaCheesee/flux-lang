cmake_minimum_required(VERSION 3.20)
project(flux-compiler CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(fluxc ${SOURCES})

target_include_directories(fluxc PRIVATE include)

target_include_directories(fluxc PRIVATE ${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(fluxc PRIVATE ${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(
    llvm_libs 
    core 
    support 
    native 
    codegen 
    target 
    scalar 
    irreader 
    transformutils 
    analysis
)

target_link_libraries(fluxc PRIVATE ${llvm_libs})

target_compile_options(fluxc PRIVATE -O2)