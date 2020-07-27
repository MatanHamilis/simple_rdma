cmake_minimum_required(VERSION 3.5.0)
project (rdma_simple C)
add_executable(main main.c verbs_wrappers.c logging.c cm.c)
find_library(   IBVERBS 
                NAMES ibverbs 
)
target_link_libraries(main ${IBVERBS})