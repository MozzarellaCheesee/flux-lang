cmake_minimum_required(VERSION 3.20)
project(flux-lang LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/**/*.cpp")
add_executable(fluxc ${SOURCES})

target_include_directories(fluxc
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

