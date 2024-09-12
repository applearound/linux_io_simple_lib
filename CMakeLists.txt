cmake_minimum_required(VERSION 3.5)

project(linux_io_sample_lib)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_FLAGS -Wall)

add_executable(uring_cat src/io_uring_cat.c)
add_executable(socket_classic src/socket_classic.c)
add_executable(socket_epoll src/socket_epoll.c)

target_link_libraries(uring_cat uring)
