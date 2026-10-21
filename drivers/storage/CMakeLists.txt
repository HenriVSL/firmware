# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

add_library(dmm_drivers_storage STATIC src/spi_nand.cpp)
target_include_directories(dmm_drivers_storage PUBLIC include)
target_link_libraries(dmm_drivers_storage PUBLIC dmm_hal dmm_lib)
dmm_core_options(dmm_drivers_storage)
