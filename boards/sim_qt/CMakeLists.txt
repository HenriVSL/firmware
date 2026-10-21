# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

qt_add_executable(simqt main.cpp sim_bridge.h sim_bridge.cpp sim_screen.h sim_screen.cpp)
qt_add_qml_module(simqt
    URI DeltaMMSim
    VERSION 1.0
    QML_FILES Main.qml)
target_link_libraries(simqt PRIVATE dmm_app dmm_sim_common Qt6::Quick)
dmm_warnings(simqt)

add_test(NAME simqt-starts COMMAND simqt --seconds 2)
set_tests_properties(simqt-starts PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
