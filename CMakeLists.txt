cmake_minimum_required(VERSION 3.22)

project(ntfs_tamper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /level='requireAdministrator' /uiAccess='false'" )

add_executable(ntfs_tamper)
aux_source_directory(src sources)
target_sources(ntfs_tamper PUBLIC ${sources})
message("${sources}")