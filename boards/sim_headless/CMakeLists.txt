# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

add_executable(sim main.cpp)
target_link_libraries(sim PRIVATE dmm_app dmm_sim_common)
dmm_warnings(sim)

add_test(NAME sim-boots COMMAND sim --seconds 2 --leds)
set_tests_properties(sim-boots PROPERTIES
    PASS_REGULAR_EXPRESSION "DeltaMM [0-9.]+[^ ]* on sim_headless.*LED red on.*LED red off")

add_test(NAME sim-screen-shot COMMAND sim --seconds 4 --command "screen shot" --command-at 3)
set_tests_properties(sim-screen-shot PROPERTIES
    PASS_REGULAR_EXPRESSION "scr: start 160 128.*scr: f 0 .*scr: e [0-9]+.*scr: stop [0-9]+")

add_test(NAME sim-screen-text COMMAND sim --seconds 4 --command "screen text" --command-at 3)
set_tests_properties(sim-screen-text PROPERTIES
    PASS_REGULAR_EXPRESSION "scr: text.*scr: t [0-9]+ [0-9]+ [0-9]+ [0-9a-f]+ [0-9a-f]+ [^ ].*scr: text end")

find_program(DMM_PYTHON3 python3)
if(DMM_PYTHON3)
    add_test(NAME sim-radio-test COMMAND ${DMM_PYTHON3} ${PROJECT_SOURCE_DIR}/tools/radio_test/radio_test.py run test-tools
        --sim $<TARGET_FILE:sim> --no-human --out ${CMAKE_CURRENT_BINARY_DIR}/radio-test)
endif()
