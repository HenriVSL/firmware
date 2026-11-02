// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/os/os.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

static_assert(configTICK_RATE_HZ == 1000, "os::Ticks are milliseconds");
static_assert(sizeof(TickType_t) == sizeof(dmm::os::Ticks));
static_assert(configMAX_PRIORITIES > 7, "task priorities go up to 7");
static_assert(sizeof(StaticTask_t) <= sizeof(dmm::os::detail::TaskControl::storage));
static_assert(sizeof(StaticQueue_t) <= sizeof(dmm::os::detail::QueueControl::storage));
static_assert(sizeof(StaticSemaphore_t) <= sizeof(dmm::os::detail::MutexControl::storage));
static_assert(sizeof(StackType_t) == sizeof(dmm::os::detail::StackWord));

namespace dmm::os {

namespace {

FatalHandler fatalHandler = nullptr;

constexpr std::size_t MaxTasks = 16;
detail::TaskControl *startedTasks[MaxTasks]{};
std::size_t startedCount = 0;

TickType_t toTicks(std::uint32_t timeoutMs)
{
    return timeoutMs == WaitForever ? portMAX_DELAY : timeoutMs;
}

QueueHandle_t queueHandle(detail::QueueControl &q)
{
    return reinterpret_cast<QueueHandle_t>(q.storage);
}

SemaphoreHandle_t mutexHandle(detail::MutexControl &m)
{
    return reinterpret_cast<SemaphoreHandle_t>(m.storage);
}

} // namespace

Ticks nowMs()
{
    return xTaskGetTickCount();
}

void sleepMs(Ticks ms)
{
    vTaskDelay(ms);
}

// The Cortex-M cycle counter (ARMv7-M Architecture Reference Manual, the
// Data Watchpoint and Trace unit): it counts once the trace enable bit in
// the debug monitor control register and the counter's enable bit in the
// unit's control register are set.
std::uint32_t cycleCount()
{
    auto *const demcr = reinterpret_cast<volatile std::uint32_t *>(0xE000EDFCu);
    auto *const control = reinterpret_cast<volatile std::uint32_t *>(0xE0001000u);
    auto *const counter = reinterpret_cast<volatile std::uint32_t *>(0xE0001004u);
    constexpr std::uint32_t TraceEnable = 1u << 24;
    constexpr std::uint32_t CounterEnable = 1u << 0;
    if ((*control & CounterEnable) == 0) {
        *demcr = *demcr | TraceEnable;
        *control = *control | CounterEnable;
    }
    return *counter;
}

std::uint32_t cyclesPerSecond()
{
    return configCPU_CLOCK_HZ;
}

void sleepUntil(Ticks &wake, Ticks period)
{
    xTaskDelayUntil(&wake, period);
}

TaskStack taskStack(std::size_t index)
{
    if (index >= startedCount) {
        return {};
    }
    const auto handle = reinterpret_cast<TaskHandle_t>(startedTasks[index]->storage);
    return {pcTaskGetName(handle), uxTaskGetStackHighWaterMark(handle)};
}

void runScheduler()
{
    vTaskStartScheduler();
    for (;;) {
    }
}

bool stopRequested()
{
    return false;
}

void requestStop() {}

void joinAll() {}

const char *currentTaskName()
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return nullptr;
    }
    return pcTaskGetName(nullptr);
}

void setFatalHandler(FatalHandler handler)
{
    fatalHandler = handler;
}

[[noreturn]] void fatalError(const char *what, const char *task)
{
    if (fatalHandler != nullptr) {
        fatalHandler(what, task);
    }
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

namespace detail {

void startTask(TaskControl &t, const char *name, unsigned priority, void (*entry)(void *), void *arg,
    StackWord *stack, std::size_t words)
{
    xTaskCreateStatic(entry, name, static_cast<configSTACK_DEPTH_TYPE>(words), arg, priority, stack,
        reinterpret_cast<StaticTask_t *>(t.storage));
    if (startedCount < MaxTasks) {
        startedTasks[startedCount++] = &t;
    }
}

std::size_t unusedStackWords(TaskControl &t)
{
    return uxTaskGetStackHighWaterMark(reinterpret_cast<TaskHandle_t>(t.storage));
}

void createQueue(QueueControl &q, std::uint8_t *storage, std::size_t itemSize, std::size_t capacity)
{
    xQueueCreateStatic(capacity, itemSize, storage, reinterpret_cast<StaticQueue_t *>(q.storage));
}

bool queueSend(QueueControl &q, const void *item, std::uint32_t timeoutMs)
{
    return xQueueSend(queueHandle(q), item, toTicks(timeoutMs)) == pdPASS;
}

bool queueReceive(QueueControl &q, void *item, std::uint32_t timeoutMs)
{
    return xQueueReceive(queueHandle(q), item, toTicks(timeoutMs)) == pdPASS;
}

void createMutex(MutexControl &m)
{
    xSemaphoreCreateMutexStatic(reinterpret_cast<StaticSemaphore_t *>(m.storage));
}

void mutexLock(MutexControl &m)
{
    xSemaphoreTake(mutexHandle(m), portMAX_DELAY);
}

void mutexUnlock(MutexControl &m)
{
    xSemaphoreGive(mutexHandle(m));
}

} // namespace detail

} // namespace dmm::os

extern "C" {

// The CPU sleeps whenever no task is ready; the next interrupt (at the
// latest the 1 ms tick) wakes it.
void vApplicationIdleHook()
{
    asm volatile("dsb\n\twfi\n\tisb" ::: "memory");
}

void dmmKernelCheckFailed()
{
    dmm::os::fatalError("kernel check failed", dmm::os::currentTaskName());
}

// A task overran its stack.
void vApplicationStackOverflowHook(TaskHandle_t, char *name)
{
    dmm::os::fatalError("stack overflow", name);
}

} // extern "C"
