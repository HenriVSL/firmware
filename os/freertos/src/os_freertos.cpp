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

void sleepUntil(Ticks &wake, Ticks period)
{
    xTaskDelayUntil(&wake, period);
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
