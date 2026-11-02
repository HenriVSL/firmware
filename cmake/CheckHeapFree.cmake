# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

# Fails when any library in LIBS (a ;-list) needs a banned symbol: the
# heap (malloc, operator new), exceptions (including the library's
# std::__throw_* helpers), abort or the printf family.
# Usage: cmake -DNM=<nm> -DLIBS=<a;b> -P CheckHeapFree.cmake ('|' may
# separate the libraries instead of ';').

set(banned "^(malloc|calloc|realloc|free|_Znw.*|_Zna.*|abort|_ZSt[0-9]*__throw_.*|__cxa_throw|__cxa_allocate_exception|__cxa_begin_catch|printf|sprintf|snprintf|vsnprintf|vprintf|puts|fprintf)$")
string(REPLACE "|" ";" LIBS "${LIBS}")
set(failed FALSE)
foreach(lib IN LISTS LIBS)
    execute_process(COMMAND ${NM} -u ${lib} OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
    # An object nm cannot read (link-time optimization's) would pass unseen.
    if(NOT rc EQUAL 0 OR err MATCHES "plugin needed")
        message(FATAL_ERROR "nm failed on ${lib}: ${err}")
    endif()
    string(REPLACE "\n" ";" lines "${out}")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "^.*[ \t]U[ \t]+" "" sym "${line}")
        string(STRIP "${sym}" sym)
        if(sym MATCHES "${banned}")
            message(SEND_ERROR "${lib} needs banned symbol ${sym}")
            set(failed TRUE)
        endif()
    endforeach()
endforeach()
if(failed)
    message(FATAL_ERROR "core libraries are not heap-free")
endif()
message(STATUS "core libraries are heap-free")
