cmake_minimum_required(VERSION 3.20)
project(flux-compiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── LLVM ──────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# ── Исходники ──────────────────────────────────────────
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/**/*.cpp")
add_executable(fluxc ${SOURCES})

target_include_directories(fluxc PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ── Линковка с LLVM ────────────────────────────────────
llvm_map_components_to_libnames(llvm_libs
    core support native nativecodegen
    codegen target targetparser
    irreader bitwriter bitreader
    transformutils analysis instcombine
    scalaropts passes mc mcparser
    mcdisassembler asmprinter asmparser
    selectiondag x86codegen x86asmparser
    x86info x86desc demangle
)

target_link_libraries(fluxc PRIVATE ${llvm_libs})

# ── Windows GCC ────────────────────────────────────────
if(WIN32)
    target_link_libraries(fluxc PRIVATE version ws2_32 ntdll)
endif()

# ── Статическая линковка (автономный .exe) ─────────────
target_link_options(fluxc PRIVATE -static-libgcc -static-libstdc++ -static)
