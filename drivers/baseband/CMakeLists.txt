# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

add_library(dmm_drivers_baseband STATIC src/sct3258.cpp)
target_include_directories(dmm_drivers_baseband PUBLIC include)
target_link_libraries(dmm_drivers_baseband PUBLIC dmm_hal dmm_lib)
dmm_core_options(dmm_drivers_baseband)
