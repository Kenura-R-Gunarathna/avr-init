#include "scheduler.h"
#include "millis.h"

/* Task array order is tie-breaking priority: lower index fires first when two tasks
 * are due at the same now. It is NOT blocking — a lower-index task cannot delay a
 * higher-index one; every slot is checked on each loop tick regardless. */
void scheduler_run(struct task *tasks, uint8_t count) {
    for (;;) {
        uint32_t now = millis();
        for (uint8_t i = 0; i < count; i++) {
            if ((uint32_t)(now - tasks[i].last_ms) >= tasks[i].interval_ms) {
                uint32_t next = tasks[i].run(); // retrieve the the end tiem of the current action
                tasks[i].last_ms = now;
                if (next) tasks[i].interval_ms = next; // return the end tiem of the
            }
        }
    }
}
