cmake_minimum_required(VERSION 3.20)
project(flux-compiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── LLVM ──────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_CMAKE_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# ── Исходники компилятора ─────────────────────────────
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/**/*.cpp"
)
add_executable(fluxc ${SOURCES})

# ── Линковка с LLVM компонентами ──────────────────────
llvm_map_components_to_libnames(llvm_libs
core                    # Основной IR
support                 # Базовые утилиты  
native                  # Target-specific codegen
codegen                 # Code generation
target                  # Target info
irreader                # LLVM IR parsing
transformutils          # Pass utilities
analysis                # Analysis passes
mc
mcparser
nativecodegen
passes
x86codegen
x86asmparser
x86info
)

target_link_libraries(fluxc PRIVATE ${LLVM_LIBS})

target_include_directories(fluxc PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
# ── Windows: линковка статически ─────────────────────
if(WIN32)
    target_link_options(fluxc PRIVATE /MT)
endif()