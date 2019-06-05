
cmake_minimum_required(VERSION 3.1)

SET(CMAKE_BUILD_TYPE "Debug")

if(BUILD_SHARED_LIBS)
message("build shared ...")
set(_LIB_SUFFIX "shared")
else()
message("build static ...")
set(_LIB_SUFFIX "static")
endif()
#SET(CMAKE_CXX_COMPILER_ID "Clang")
#SET(CMAKE_C_COMPILER "clang")
#SET(CMAKE_CXX_COMPILER "clang++")

project(lease_cache)

include(cmake/netLink.cmake)
include(cmake/cache_system.cmake)
add_subdirectory(sample sample)
if(MSVC)
set(OUTPUT_DIR build/out)
else()
set(OUTPUT_DIR out)
endif()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})

