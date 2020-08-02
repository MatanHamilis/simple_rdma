cmake_minimum_required(VERSION 3.5.0)
project (rdma_simple C)
add_executable(main main.c latency_measure.c verbs_wrappers.c logging.c cm.c memutils.c cache_exhauster.c)
find_library(   IBVERBS 
                NAMES ibverbs 
)
target_include_directories(main 
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(main ${IBVERBS})