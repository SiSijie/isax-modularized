cmake_minimum_required(VERSION 3.16)

add_executable(testSIMD test_simd_paa2sax.c)

target_link_libraries(testSIMD PRIVATE index sax paa utils)
target_link_libraries(testSIMD PUBLIC pthread m)