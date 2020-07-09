cmake_minimum_required(VERSION 3.16)

add_library(sax sax.c breakpoints.c)

# -mavx -mavx2 -mavx512 -msse4.2 are supported by Intel(R) Xeon(R) Gold 6134 CPU @ 3.20GHz in andromache
target_compile_options(sax PUBLIC -march=native -msse3 -msse4.2 -mavx -mavx2)
