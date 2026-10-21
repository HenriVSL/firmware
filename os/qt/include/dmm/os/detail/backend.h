// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// PC backend: each task is a Qt thread, queues are mutex-guarded rings.
// Priorities are recorded but not enforced.

#include <cstddef>
#include <cstdint>

namespace dmm::os::detail {

using StackWord = std::uint32_t;

// Host threads get their own stacks, so the static stack is not needed.
constexpr std::size_t stackWords(std::size_t)
{
    return 1;
}

struct TaskControl
{
    void *thread = nullptr;
};

struct QueueControl
{
    void *impl = nullptr;
    std::uint8_t *storage = nullptr;
    std::size_t itemSize = 0;
    std::size_t capacity = 0;
    std::size_t head = 0;
    std::size_t count = 0;
};

struct MutexControl
{
    void *impl = nullptr;
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
