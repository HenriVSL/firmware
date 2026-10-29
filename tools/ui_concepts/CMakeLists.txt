# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen

add_executable(ui-concepts main.cpp)
target_link_libraries(ui-concepts PRIVATE dmm_services Qt6::Gui)
dmm_warnings(ui-concepts)

add_executable(menu-concepts menus.cpp)
target_link_libraries(menu-concepts PRIVATE dmm_services Qt6::Gui)
dmm_warnings(menu-concepts)
