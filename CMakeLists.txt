cmake_minimum_required(VERSION 3.16)

set(CMAKE_VERBOSE_MAKEFILE ON)

project(isax C)

set(CMAKE_C_STANDARD 11)

include_directories(include/utils)

add_subdirectory(src/index)
add_subdirectory(src/query)
add_subdirectory(src/utils)

add_executable(isax main.c)
target_link_libraries(isax utils pthread)

install (TARGETS isax DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)