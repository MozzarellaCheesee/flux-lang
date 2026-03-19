cmake_minimum_required(VERSION 3.20)
project(flux-compiler CXX)

set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(fluxc ${SOURCES})
target_include_directories(fluxc PRIVATE include)