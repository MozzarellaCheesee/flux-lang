cmake_minimum_required(VERSION 3.20)
project(flux-compiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── LLVM ──────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_CMAKE_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# ── Исходники компилятора ─────────────────────────────
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/**/*.cpp"
)
add_executable(fluxc ${SOURCES})

# ── Линковка с LLVM компонентами ──────────────────────
llvm_map_components_to_libnames(llvm_libs
    core
    support
    native
    nativecodegen
    codegen
    target
    targetparser
    irreader
    bitwriter
    bitreader
    transformutils
    analysis
    instcombine
    scalaropts
    passes
    mc
    mcparser
    mcdisassembler
    asmprinter
    asmparser
    selectiondag
    x86codegen
    x86asmparser
    x86info
    x86desc
    demangle
)

target_link_libraries(fluxc PRIVATE ${llvm_libs})

target_include_directories(fluxc PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# ── Windows: статическая линковка рантайма ────────────
if(WIN32)
    target_link_options(fluxc PRIVATE /MT)
    target_link_libraries(fluxc PRIVATE
        Version.lib
        Ws2_32.lib
        Ntdll.lib
    )
endif()
