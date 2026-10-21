// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// Radio backend: FreeRTOS with static allocation. Kernel objects are held
// as raw storage here so that kernel headers stay out of every user of
// os.h; os_freertos.cpp checks that each fits.

#include <cstddef>
#include <cstdint>

namespace dmm::os::detail {

using StackWord = std::uint32_t;

constexpr std::size_t stackWords(std::size_t words)
{
    return words;
}

struct TaskControl
{
    alignas(8) std::uint8_t storage[96];
};

struct QueueControl
{
    alignas(8) std::uint8_t storage[80];
};

struct MutexControl
{
    alignas(8) std::uint8_t storage[80];
};

void startTask(TaskControl &t, const char *name, unsigned priority, void (*entry)(void *), void *arg,
    StackWord *stack, std::size_t words);
std::size_t unusedStackWords(TaskControl &t);
void createQueue(QueueControl &q, std::uint8_t *storage, std::size_t itemSize, std::size_t capacity);
bool queueSend(QueueControl &q, const void *item, std::uint32_t timeoutMs);
bool queueReceive(QueueControl &q, void *item, std::uint32_t timeoutMs);
void createMutex(MutexControl &m);
void mutexLock(MutexControl &m);
void mutexUnlock(MutexControl &m);

} // namespace dmm::os::detail
