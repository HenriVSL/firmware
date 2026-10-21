# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

# FreeRTOS kernel (MIT), built from its sources with static allocation
# only. The board provides FreeRTOSConfig.h through dmm_board_config and
# names the kernel port in DMM_FREERTOS_PORT.

FetchContent_Declare(freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG V11.3.2
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(freertos_kernel)

set(_port ${freertos_kernel_SOURCE_DIR}/portable/${DMM_FREERTOS_PORT})
add_library(dmm_freertos_kernel STATIC
    ${freertos_kernel_SOURCE_DIR}/list.c
    ${freertos_kernel_SOURCE_DIR}/queue.c
    ${freertos_kernel_SOURCE_DIR}/tasks.c
    ${_port}/port.c)
target_include_directories(dmm_freertos_kernel SYSTEM PUBLIC ${freertos_kernel_SOURCE_DIR}/include ${_port})
target_link_libraries(dmm_freertos_kernel PUBLIC dmm_board_config)

add_library(dmm_os STATIC src/os_freertos.cpp)
target_include_directories(dmm_os PUBLIC ../include include)
target_link_libraries(dmm_os PUBLIC dmm_lib PRIVATE dmm_freertos_kernel)
dmm_core_options(dmm_os)
