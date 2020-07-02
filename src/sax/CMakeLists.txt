cmake_minimum_required(VERSION 3.16)

file(GLOB SOURCE_FILES *.c)
add_library(sax ${SOURCE_FILES})
