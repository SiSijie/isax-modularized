cmake_minimum_required(VERSION 3.16)

include_directories(../../include/utils)

file(GLOB SOURCE_FILES *.c)
add_library(utils ${SOURCE_FILES})
