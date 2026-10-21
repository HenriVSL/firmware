# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

# Portable HAL interfaces (built everywhere, so drivers can be tested on
# the PC), CPU core support shared by every Cortex-M part, then the MCU
# family the board names in DMM_MCU.

add_library(dmm_hal INTERFACE)
target_include_directories(dmm_hal INTERFACE common/include)
target_compile_features(dmm_hal INTERFACE cxx_std_20)

if(NOT CMAKE_CROSSCOMPILING)
    return()
endif()

add_library(dmm_hal_cortex_m STATIC cpu/cortex_m/src/fault.cpp)
target_include_directories(dmm_hal_cortex_m PUBLIC cpu/cortex_m/include)
dmm_core_options(dmm_hal_cortex_m)

if(DMM_MCU STREQUAL "gd32f30x")
    add_subdirectory(mcu/gd32f30x)
elseif(DMM_MCU)
    message(FATAL_ERROR "Unknown DMM_MCU '${DMM_MCU}'")
endif()
