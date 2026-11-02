# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen

# Reports where an image's constant data and code end against LIMIT, the
# end of the flash the MCU reads without wait states, and warns when
# constant data reaches past it. The symbols come from the linker script.
# Usage: cmake -DNM=<nm> -DELF=<file> -DLIMIT=<hex> -P CheckFlashLayout.cmake

execute_process(COMMAND ${NM} ${ELF} OUTPUT_VARIABLE out RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "nm failed on ${ELF}")
endif()
foreach(sym __rodata_end __code_end)
    if(NOT out MATCHES "([0-9a-fA-F]+) [A-Za-z] ${sym}\n")
        message(FATAL_ERROR "${ELF} has no ${sym} symbol")
    endif()
    math(EXPR ${sym} "0x${CMAKE_MATCH_1}")
    math(EXPR ${sym}_hex "0x${CMAKE_MATCH_1}" OUTPUT_FORMAT HEXADECIMAL)
endforeach()
math(EXPR limit "${LIMIT}")

if(__rodata_end GREATER limit)
    math(EXPR over "${__rodata_end} - ${limit}")
    message(WARNING "Constant data ends at ${__rodata_end_hex}, ${over} bytes past ${LIMIT}: reads from there are slow.")
elseif(__code_end GREATER limit)
    math(EXPR over "${__code_end} - ${limit}")
    message(STATUS "Constant data ends at ${__rodata_end_hex}, code at ${__code_end_hex}: ${over} bytes of code past ${LIMIT}")
else()
    math(EXPR left "${limit} - ${__code_end}")
    message(STATUS "Constant data ends at ${__rodata_end_hex}, code at ${__code_end_hex}: ${left} bytes below ${LIMIT}")
endif()
