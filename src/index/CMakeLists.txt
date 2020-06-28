cmake_minimum_required(VERSION 3.16)

include_directories(../../include/index)

file(GLOB SOURCE_FILES *.c)
add_library(index ${SOURCE_FILES} index_engine.c)
