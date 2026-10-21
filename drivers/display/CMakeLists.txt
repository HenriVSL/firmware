# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

add_library(dmm_drivers_display STATIC src/st7735.cpp)
target_include_directories(dmm_drivers_display PUBLIC include)
target_link_libraries(dmm_drivers_display PUBLIC dmm_hal dmm_lib)
dmm_core_options(dmm_drivers_display)
