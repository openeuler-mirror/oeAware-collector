cmake_minimum_required(VERSION 3.15)
project(pmu_plugin)

set(CMAKE_CXX_STANDARD 11)

option(WITH_DEBUG "debug mode" OFF)

if (WITH_DEBUG)
    message("-- Note:pmu debug mode")
    add_compile_options(-g)
endif()
add_compile_options(-O2 -fPIC -Wall -Wextra)

# libkperf
message("-- libkperf library path: ${LIB_KPERF_LIBPATH}")
message("-- libkperf include path: ${LIB_KPERF_INCPATH}")


set(pmu_src
    plugin/plugin_counting.c
    plugin/plugin_sampling.c
    plugin/plugin_spe.c
    plugin/plugin_uncore.c
    plugin/pmu_uncore.c
    plugin/plugin_netif_rx.c
    plugin/plugin_comm.c
    plugin/plugin_napi_gro_receive_entry.c
    plugin/plugin_skb_copy_datagram_iovec.c
    plugin/plugin.c
)

add_library(pmu SHARED ${pmu_src})

include_directories(pmu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin
    ${LIB_KPERF_INCPATH}
)

target_link_directories(pmu PUBLIC
    ${LIB_KPERF_LIBPATH}
)

target_link_libraries(pmu kperf boundscheck)