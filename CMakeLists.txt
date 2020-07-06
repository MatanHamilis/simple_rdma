cmake_minimum_required(VERSION 3.5.0)
project (rdma_simple)
add_executable(main main.c)
target_link_libraries(main libibverbs)