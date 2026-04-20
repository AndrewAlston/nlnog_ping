cmake_minimum_required(VERSION 4.1.2)
find_package(Doxygen REQUIRED dot)
project(json_helper_lib)

set(CMAKE_GENERATOR Ninja)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DRTE_FORCE_INTRINSICS -g3 -O3 -Wno-reserved-identifier")
set(BUILD_SHARED_LIBS off)

include_directories(../include)

add_library(json_helper_lib json_helper.c)
target_link_options(json_helper_lib PRIVATE -static)
target_link_libraries(json_helper_lib PRIVATE generic_lib)