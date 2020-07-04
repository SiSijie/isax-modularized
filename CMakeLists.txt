cmake_minimum_required(VERSION 3.16)

set(CMAKE_VERBOSE_MAKEFILE ON)

project(isax C)

set(CMAKE_C_STANDARD 11)

include_directories(include/index)
include_directories(include/paa)
include_directories(include/query)
include_directories(include/sax)
include_directories(include/utils)

add_subdirectory(src/index)
add_subdirectory(src/paa)
add_subdirectory(src/query)
add_subdirectory(src/sax)
add_subdirectory(src/utils)

add_executable(isax main.c)

target_link_libraries(isax PRIVATE query index sax paa utils)
target_link_libraries(isax PUBLIC pthread m)

#add_compile_options(-g -ggdb)
#set(CMAKE_BUILD_TYPE Debug)

install (TARGETS isax DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)