#pragma once
#include <stdint.h>

struct task {
    uint32_t (*run)(void);
    uint32_t   interval_ms;
    uint32_t   last_ms;
};

#define TASK_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

void scheduler_run(struct task *tasks, uint8_t count);
