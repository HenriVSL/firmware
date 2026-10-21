# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

# dmm_os: the OS API. Radios use the FreeRTOS backend (os/freertos), the PC
# the Qt backend (os/qt). Host objects live as long as the program, so the
# Qt backend allocates them once; the FreeRTOS backend never allocates.

if(CMAKE_CROSSCOMPILING)
    add_subdirectory(freertos)
elseif(TARGET Qt6::Core)
    add_library(dmm_os STATIC qt/src/os_qt.cpp)
    target_include_directories(dmm_os PUBLIC include qt/include)
    target_link_libraries(dmm_os PUBLIC dmm_lib Qt6::Core)
    dmm_warnings(dmm_os)
endif()
