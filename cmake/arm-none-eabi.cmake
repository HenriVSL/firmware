# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

# Toolchain file for Arm GNU Toolchain 14.2 or newer. The compiler is taken
# from ARM_GNU_TOOLCHAIN (the toolchain's root folder) when set, else PATH.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(DEFINED ENV{ARM_GNU_TOOLCHAIN})
    set(_dmm_tc_bin "$ENV{ARM_GNU_TOOLCHAIN}/bin/")
else()
    set(_dmm_tc_bin "")
endif()

set(CMAKE_C_COMPILER "${_dmm_tc_bin}arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "${_dmm_tc_bin}arm-none-eabi-g++")
set(CMAKE_ASM_COMPILER "${_dmm_tc_bin}arm-none-eabi-gcc")
set(CMAKE_OBJCOPY "${_dmm_tc_bin}arm-none-eabi-objcopy" CACHE FILEPATH "")
set(CMAKE_SIZE "${_dmm_tc_bin}arm-none-eabi-size" CACHE FILEPATH "")
set(CMAKE_NM "${_dmm_tc_bin}arm-none-eabi-nm" CACHE FILEPATH "")

set(_dmm_cpu "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS_INIT "${_dmm_cpu} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${_dmm_cpu} -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit")
set(CMAKE_ASM_FLAGS_INIT "${_dmm_cpu} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_dmm_cpu} -Wl,--gc-sections -specs=nano.specs -specs=nosys.specs")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
