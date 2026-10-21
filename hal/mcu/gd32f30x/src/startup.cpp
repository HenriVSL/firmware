// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/startup.h"
#include "dmm/hal/cortex_m/core.h"
#include "dmm/hal/gd32f30x/irq.h"

#include <cstdint>

// Provided by the linker script.
extern "C" {
extern std::uint32_t __stack_top;
extern std::uint32_t __data_load;
extern std::uint32_t __data_start;
extern std::uint32_t __data_end;
extern std::uint32_t __bss_start;
extern std::uint32_t __bss_end;
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();
}

namespace cm = dmm::hal::cortex_m;
namespace irq = dmm::hal::gd32f30x::irq;

extern "C" {

// Records the crash and restarts (hal/cpu/cortex_m fault capture).
[[noreturn]] void dmmFaultEntry();

// Unhandled exception or interrupt.
[[noreturn, gnu::naked]] void defaultHandler()
{
    asm volatile("b dmmFaultEntry");
}

[[noreturn]] void startImage()
{
    cm::enableFpu();
    extern const void *const vectorTable[];
    cm::setVectorTable(vectorTable);
    cm::resetInterruptState(irq::Count);
    cm::usePreemptOnlyPriorities();

    const std::uint32_t *from = &__data_load;
    for (std::uint32_t *to = &__data_start; to < &__data_end;) {
        *to++ = *from++;
    }
    for (std::uint32_t *to = &__bss_start; to < &__bss_end;) {
        *to++ = 0;
    }
    for (auto *ctor = __init_array_start; ctor < __init_array_end; ++ctor) {
        (*ctor)();
    }
    dmm::hal::gd32f30x::boardMain();
}

[[noreturn, gnu::naked]] void resetHandler()
{
    asm volatile("cpsid i\n\t"
                 "ldr r0, =__stack_top\n\t"
                 "msr msp, r0\n\t"
                 "b startImage\n\t"
                 ".ltorg");
}

// Static objects are never destroyed: the firmware does not exit.
int atexit(void (*)())
{
    return 0;
}

int __cxa_atexit(void (*)(void *), void *, void *)
{
    return 0;
}

void *__dso_handle = nullptr;

#define DMM_WEAK_HANDLER(name) void name() __attribute__((weak, alias("defaultHandler"), noreturn, nothrow));
DMM_WEAK_HANDLER(nmiHandler)
DMM_WEAK_HANDLER(hardFaultHandler)
DMM_WEAK_HANDLER(memManageHandler)
DMM_WEAK_HANDLER(busFaultHandler)
DMM_WEAK_HANDLER(usageFaultHandler)
DMM_WEAK_HANDLER(SVC_Handler)
DMM_WEAK_HANDLER(debugMonHandler)
DMM_WEAK_HANDLER(PendSV_Handler)
DMM_WEAK_HANDLER(SysTick_Handler)
DMM_WEAK_HANDLER(irq0Handler)
DMM_WEAK_HANDLER(irq1Handler)
DMM_WEAK_HANDLER(irq2Handler)
DMM_WEAK_HANDLER(irq3Handler)
DMM_WEAK_HANDLER(irq4Handler)
DMM_WEAK_HANDLER(irq5Handler)
DMM_WEAK_HANDLER(irq6Handler)
DMM_WEAK_HANDLER(irq7Handler)
DMM_WEAK_HANDLER(irq8Handler)
DMM_WEAK_HANDLER(irq9Handler)
DMM_WEAK_HANDLER(irq10Handler)
DMM_WEAK_HANDLER(irq11Handler)
DMM_WEAK_HANDLER(irq12Handler)
DMM_WEAK_HANDLER(irq13Handler)
DMM_WEAK_HANDLER(irq14Handler)
DMM_WEAK_HANDLER(irq15Handler)
DMM_WEAK_HANDLER(irq16Handler)
DMM_WEAK_HANDLER(irq17Handler)
DMM_WEAK_HANDLER(irq18Handler)
DMM_WEAK_HANDLER(irq19Handler)
DMM_WEAK_HANDLER(irq20Handler)
DMM_WEAK_HANDLER(irq21Handler)
DMM_WEAK_HANDLER(irq22Handler)
DMM_WEAK_HANDLER(irq23Handler)
DMM_WEAK_HANDLER(irq24Handler)
DMM_WEAK_HANDLER(irq25Handler)
DMM_WEAK_HANDLER(irq26Handler)
DMM_WEAK_HANDLER(irq27Handler)
DMM_WEAK_HANDLER(irq28Handler)
DMM_WEAK_HANDLER(irq29Handler)
DMM_WEAK_HANDLER(irq30Handler)
DMM_WEAK_HANDLER(irq31Handler)
DMM_WEAK_HANDLER(irq32Handler)
DMM_WEAK_HANDLER(irq33Handler)
DMM_WEAK_HANDLER(irq34Handler)
DMM_WEAK_HANDLER(irq35Handler)
DMM_WEAK_HANDLER(irq36Handler)
DMM_WEAK_HANDLER(irq37Handler)
DMM_WEAK_HANDLER(irq38Handler)
DMM_WEAK_HANDLER(irq39Handler)
DMM_WEAK_HANDLER(irq40Handler)
DMM_WEAK_HANDLER(irq41Handler)
DMM_WEAK_HANDLER(irq42Handler)
DMM_WEAK_HANDLER(irq43Handler)
DMM_WEAK_HANDLER(irq44Handler)
DMM_WEAK_HANDLER(irq45Handler)
DMM_WEAK_HANDLER(irq46Handler)
DMM_WEAK_HANDLER(irq47Handler)
DMM_WEAK_HANDLER(irq48Handler)
DMM_WEAK_HANDLER(irq49Handler)
DMM_WEAK_HANDLER(irq50Handler)
DMM_WEAK_HANDLER(irq51Handler)
DMM_WEAK_HANDLER(irq52Handler)
DMM_WEAK_HANDLER(irq53Handler)
DMM_WEAK_HANDLER(irq54Handler)
DMM_WEAK_HANDLER(irq55Handler)
DMM_WEAK_HANDLER(irq56Handler)
DMM_WEAK_HANDLER(irq57Handler)
DMM_WEAK_HANDLER(irq58Handler)
DMM_WEAK_HANDLER(irq59Handler)
DMM_WEAK_HANDLER(irq60Handler)
DMM_WEAK_HANDLER(irq61Handler)
DMM_WEAK_HANDLER(irq62Handler)
DMM_WEAK_HANDLER(irq63Handler)
DMM_WEAK_HANDLER(irq64Handler)
DMM_WEAK_HANDLER(irq65Handler)
DMM_WEAK_HANDLER(irq66Handler)
DMM_WEAK_HANDLER(irq67Handler)
#undef DMM_WEAK_HANDLER

// Core exceptions, then the 68 GD32F30x interrupts. SVC, PendSV and SysTick
// keep the names the kernel's Cortex-M port is configured to define.
__attribute__((section(".vectors"), used)) extern const void *const vectorTable[] = {
    &__stack_top,
    reinterpret_cast<const void *>(resetHandler),
    reinterpret_cast<const void *>(nmiHandler),
    reinterpret_cast<const void *>(hardFaultHandler),
    reinterpret_cast<const void *>(memManageHandler),
    reinterpret_cast<const void *>(busFaultHandler),
    reinterpret_cast<const void *>(usageFaultHandler),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    reinterpret_cast<const void *>(SVC_Handler),
    reinterpret_cast<const void *>(debugMonHandler),
    nullptr,
    reinterpret_cast<const void *>(PendSV_Handler),
    reinterpret_cast<const void *>(SysTick_Handler),
    reinterpret_cast<const void *>(irq0Handler),
    reinterpret_cast<const void *>(irq1Handler),
    reinterpret_cast<const void *>(irq2Handler),
    reinterpret_cast<const void *>(irq3Handler),
    reinterpret_cast<const void *>(irq4Handler),
    reinterpret_cast<const void *>(irq5Handler),
    reinterpret_cast<const void *>(irq6Handler),
    reinterpret_cast<const void *>(irq7Handler),
    reinterpret_cast<const void *>(irq8Handler),
    reinterpret_cast<const void *>(irq9Handler),
    reinterpret_cast<const void *>(irq10Handler),
    reinterpret_cast<const void *>(irq11Handler),
    reinterpret_cast<const void *>(irq12Handler),
    reinterpret_cast<const void *>(irq13Handler),
    reinterpret_cast<const void *>(irq14Handler),
    reinterpret_cast<const void *>(irq15Handler),
    reinterpret_cast<const void *>(irq16Handler),
    reinterpret_cast<const void *>(irq17Handler),
    reinterpret_cast<const void *>(irq18Handler),
    reinterpret_cast<const void *>(irq19Handler),
    reinterpret_cast<const void *>(irq20Handler),
    reinterpret_cast<const void *>(irq21Handler),
    reinterpret_cast<const void *>(irq22Handler),
    reinterpret_cast<const void *>(irq23Handler),
    reinterpret_cast<const void *>(irq24Handler),
    reinterpret_cast<const void *>(irq25Handler),
    reinterpret_cast<const void *>(irq26Handler),
    reinterpret_cast<const void *>(irq27Handler),
    reinterpret_cast<const void *>(irq28Handler),
    reinterpret_cast<const void *>(irq29Handler),
    reinterpret_cast<const void *>(irq30Handler),
    reinterpret_cast<const void *>(irq31Handler),
    reinterpret_cast<const void *>(irq32Handler),
    reinterpret_cast<const void *>(irq33Handler),
    reinterpret_cast<const void *>(irq34Handler),
    reinterpret_cast<const void *>(irq35Handler),
    reinterpret_cast<const void *>(irq36Handler),
    reinterpret_cast<const void *>(irq37Handler),
    reinterpret_cast<const void *>(irq38Handler),
    reinterpret_cast<const void *>(irq39Handler),
    reinterpret_cast<const void *>(irq40Handler),
    reinterpret_cast<const void *>(irq41Handler),
    reinterpret_cast<const void *>(irq42Handler),
    reinterpret_cast<const void *>(irq43Handler),
    reinterpret_cast<const void *>(irq44Handler),
    reinterpret_cast<const void *>(irq45Handler),
    reinterpret_cast<const void *>(irq46Handler),
    reinterpret_cast<const void *>(irq47Handler),
    reinterpret_cast<const void *>(irq48Handler),
    reinterpret_cast<const void *>(irq49Handler),
    reinterpret_cast<const void *>(irq50Handler),
    reinterpret_cast<const void *>(irq51Handler),
    reinterpret_cast<const void *>(irq52Handler),
    reinterpret_cast<const void *>(irq53Handler),
    reinterpret_cast<const void *>(irq54Handler),
    reinterpret_cast<const void *>(irq55Handler),
    reinterpret_cast<const void *>(irq56Handler),
    reinterpret_cast<const void *>(irq57Handler),
    reinterpret_cast<const void *>(irq58Handler),
    reinterpret_cast<const void *>(irq59Handler),
    reinterpret_cast<const void *>(irq60Handler),
    reinterpret_cast<const void *>(irq61Handler),
    reinterpret_cast<const void *>(irq62Handler),
    reinterpret_cast<const void *>(irq63Handler),
    reinterpret_cast<const void *>(irq64Handler),
    reinterpret_cast<const void *>(irq65Handler),
    reinterpret_cast<const void *>(irq66Handler),
    reinterpret_cast<const void *>(irq67Handler),
};

} // extern "C"

static_assert(irq::Count == 68, "vector table lists 68 interrupts");
