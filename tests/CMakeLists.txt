# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(doctest)

add_executable(tests
    main.cpp
    lib/test_containers.cpp
    lib/test_format.cpp
    lib/test_strings.cpp
    lib/test_tone_synth.cpp)
target_link_libraries(tests PRIVATE dmm_lib doctest::doctest)
target_compile_definitions(tests PRIVATE DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)
dmm_core_options(tests)
add_test(NAME tests COMMAND tests)

# The core libraries must not pull in the heap, exceptions or printf.
set(DMM_CORE_LIBS $<TARGET_FILE:dmm_lib>)
add_test(NAME core-heap-free
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} "-DLIBS=${DMM_CORE_LIBS}"
            -P ${PROJECT_SOURCE_DIR}/cmake/CheckHeapFree.cmake)
