cmake_minimum_required(VERSION 3.16)
project(GIF565)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)


set_source_files_properties(gifdec.c PROPERTIES LANGUAGE C)
add_executable(GIF565 main.c gifdec.c gifdec.h)