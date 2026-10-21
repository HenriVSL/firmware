// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// Interrupt numbers (GD32F30x User Manual, interrupt vector table). The
// handler for interrupt N is extern "C" void irq<N>Handler().

namespace dmm::hal::gd32f30x::irq {

inline constexpr unsigned Count = 68;
inline constexpr unsigned PriorityBits = 4;

inline constexpr unsigned Rtc = 3;
inline constexpr unsigned Exti0 = 6;
inline constexpr unsigned Exti1 = 7;
inline constexpr unsigned Exti2 = 8;
inline constexpr unsigned Exti3 = 9;
inline constexpr unsigned Exti4 = 10;
inline constexpr unsigned Dma0Channel0 = 11; // channels 0..6 follow
inline constexpr unsigned UsbdHighPriority = 19;
inline constexpr unsigned UsbdLowPriority = 20;
inline constexpr unsigned Exti5To9 = 23;
inline constexpr unsigned Timer1 = 28;
inline constexpr unsigned Timer2 = 29;
inline constexpr unsigned Timer3 = 30;
inline constexpr unsigned Spi0 = 35;
inline constexpr unsigned Spi1 = 36;
inline constexpr unsigned Usart0 = 37;
inline constexpr unsigned Usart1 = 38;
inline constexpr unsigned Usart2 = 39;
inline constexpr unsigned Exti10To15 = 40;
inline constexpr unsigned UsbdWakeup = 42;
inline constexpr unsigned Timer4 = 50;
inline constexpr unsigned Spi2 = 51;
inline constexpr unsigned Uart3 = 52;
inline constexpr unsigned Uart4 = 53;
inline constexpr unsigned Timer5 = 54;
inline constexpr unsigned Timer6 = 55;

} // namespace dmm::hal::gd32f30x::irq
