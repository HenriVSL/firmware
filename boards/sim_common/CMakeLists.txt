# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

# Pieces both simulators use.
add_library(dmm_sim_common STATIC sim_display.h sim_display.cpp sim_input.h sim_input.cpp sim_audio.h sim_battery.h include/dmm/board/sim_fm.h include/dmm/board/parts.h sim_storage.h sim_storage.cpp)
target_include_directories(dmm_sim_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dmm_sim_common PUBLIC dmm_services Qt6::Gui)
dmm_warnings(dmm_sim_common)
