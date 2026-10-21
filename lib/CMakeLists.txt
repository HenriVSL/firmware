# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

add_library(dmm_lib STATIC src/sha256.cpp)
target_include_directories(dmm_lib PUBLIC include)
target_compile_features(dmm_lib PUBLIC cxx_std_20)
dmm_core_options(dmm_lib)
