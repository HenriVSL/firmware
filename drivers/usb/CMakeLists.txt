# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tommi Tauriainen

# USB device stack: control requests, descriptors and the CDC-ACM serial
# port function. Portable; runs on any UsbDeviceController.

add_library(dmm_drivers_usb STATIC src/device.cpp src/cdc_acm.cpp)
target_include_directories(dmm_drivers_usb PUBLIC include)
target_link_libraries(dmm_drivers_usb PUBLIC dmm_hal dmm_lib)
dmm_core_options(dmm_drivers_usb)
