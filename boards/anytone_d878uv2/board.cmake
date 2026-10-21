# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

# Read before the layers are added: selects the MCU family and kernel port
# and provides the kernel configuration.

set(DMM_MCU gd32f30x)
set(DMM_FREERTOS_PORT GCC/ARM_CM4F)

add_library(dmm_board_config INTERFACE)
target_include_directories(dmm_board_config INTERFACE ${CMAKE_CURRENT_LIST_DIR}/config)
