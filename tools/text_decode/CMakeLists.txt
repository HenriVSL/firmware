# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen

add_executable(text-decode main.cpp ../afsk_decode/audio_file.h ../afsk_decode/audio_file.cpp)
target_link_libraries(text-decode PRIVATE dmm_services)
dmm_warnings(text-decode)
