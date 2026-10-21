# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

# The SCT3258 DMR baseband chip needs a 2 KB boot program made for that
# chip, loaded into it at every power-on. It is cut out of the user's own
# factory firmware file at configure time and compiled into the build
# folder only.
#
# dmm_baseband_boot_blob(<target> <firmware file>) adds a library <target>
# defining dmm::baseband::bootBlob(). Without a known firmware file the
# span is empty and DMR features report that the build has no boot program.

# Known factory firmware files: SHA-256 of the whole file, offset of the boot
# program in it.
set(_dmm_blob_files
    "0973126fc33fda1d734dbc064852648710de52f44c2bc7df07bc9fd4272cfef8:0xDBAD2"  # D878UVII V3.06
    "97d1d8428f3865f7db2612ee144a65a9d42db562c563363e5d69423d1e37b536:0xDBB42"  # D878UVII V3.08N
    "8f840cfcfc1bb9acab65429ced211481eeb80079725ec46cf225afce071afeeb:0xD9CC6") # D878UVII V4.00
# SHA-256 of the boot program's lowercase hex text (CMake reads it as hex).
set(_dmm_blob_hex_sha256 "3189bb6f78fd306641d98c4de2fe0da549d8323a88419ff6e9b65eb414077727")
set(_dmm_blob_bytes 2048)

function(dmm_baseband_boot_blob target firmware)
    set(out_dir ${CMAKE_CURRENT_BINARY_DIR}/generated_blob)
    set(source ${out_dir}/baseband_boot_blob.cpp)
    set(hex "")
    if(firmware)
        if(NOT EXISTS "${firmware}")
            message(FATAL_ERROR "DMM_STOCK_FIRMWARE: ${firmware} does not exist")
        endif()
        file(SHA256 "${firmware}" file_hash)
        set(offset "")
        foreach(entry IN LISTS _dmm_blob_files)
            string(REPLACE ":" ";" parts "${entry}")
            list(GET parts 0 known_hash)
            if(file_hash STREQUAL known_hash)
                list(GET parts 1 offset)
            endif()
        endforeach()
        if(offset STREQUAL "")
            message(FATAL_ERROR "DMM_STOCK_FIRMWARE: ${firmware} is not a factory firmware file this build knows "
                                "(supported: D878UVII V3.06, V3.08N, V4.00)")
        endif()
        math(EXPR offset_dec "${offset}")
        file(READ "${firmware}" hex OFFSET ${offset_dec} LIMIT ${_dmm_blob_bytes} HEX)
        string(SHA256 hex_hash "${hex}")
        if(NOT hex_hash STREQUAL _dmm_blob_hex_sha256)
            message(FATAL_ERROR "DMM_STOCK_FIRMWARE: the boot program in ${firmware} does not match")
        endif()
        message(STATUS "SCT3258 boot program taken from ${firmware}")
    else()
        message(STATUS "No DMM_STOCK_FIRMWARE: building without the SCT3258 boot program")
    endif()

    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(REGEX REPLACE "((0x..,){16})" "\\1\n    " bytes "${bytes}")
    set(content "// Generated at configure time from the user's own factory firmware file; never committed.\n\n#include \"dmm/baseband_boot_blob.h\"\n\nnamespace dmm::baseband {\n\nnamespace {\n")
    if(hex STREQUAL "")
        string(APPEND content "\n} // namespace\n\nstd::span<const std::uint8_t> bootBlob()\n{\n    return {};\n}\n")
    else()
        string(APPEND content "\nconstexpr std::uint8_t Blob[] = {\n    ${bytes}};\n\n} // namespace\n\nstd::span<const std::uint8_t> bootBlob()\n{\n    return Blob;\n}\n")
    endif()
    string(APPEND content "\n} // namespace dmm::baseband\n")
    file(MAKE_DIRECTORY ${out_dir}/dmm)
    file(WRITE ${out_dir}/dmm/baseband_boot_blob.h "// Generated; see cmake/BasebandBootBlob.cmake.\n\n#pragma once\n\n#include <cstdint>\n#include <span>\n\nnamespace dmm::baseband {\n\n/// The SCT3258's boot program, or empty if this build has none.\nstd::span<const std::uint8_t> bootBlob();\n\n} // namespace dmm::baseband\n")
    file(WRITE ${source} "${content}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${firmware}")

    add_library(${target} STATIC ${source})
    target_include_directories(${target} PUBLIC ${out_dir})
    target_compile_features(${target} PUBLIC cxx_std_20)
endfunction()
