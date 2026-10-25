/**
 * @file Zadania_18_10.c
 * @brief Automatically generated test scenario (n = 36 tasks, 10% superlinear)
 *
 * Project: RTT-Task Analyser
 * Author: Marcel Gruszecki (UAM)
 * Generated: 2026-09-23 14:00
 * Description: 18 single-core (mono) tasks and 18 dual-core (dual)
 *       tasks, i.e. 36 tasks in total (50% / 50%).
 *       Share of superlinear tasks: 10%
 *       (2 superlinear / 16 sublinear).
 *
 *       Single-core times follow the imbalanced (cascade) model: the first
 *       4 tasks are dominant tasks, each longer than half of the budget
 *       remaining at its cascade step. This forces idle time in McNaughton's
 *       rule, so the lower bound LB is not attainable and differences
 *       between the scheduling algorithms become visible.
 *
 *       Sum of p_single    : 90000 us
 *       C_max, no parallel.: 67732.0 us
 *       LB                 : 44960.5 us
 *
 * FORMAT_CZASU_DUAL: czas
 *       (marker read by generowanie_tabeli.py - do not change)
 *       The third argument of execute_dual_task() is the execution TIME (us)
 *       of the dual-core task - wall-clock time on each core, not the total
 *       work of both cores. The `p_dual (us)` column of the output table has
 *       the same unit and meaning.
 *
 *       Mono tasks are NOT pinned to a core - they are created as standalone
 *       FreeRTOS tasks with tskNO_AFFINITY, so the SMP scheduler decides on
 *       which core they actually run. Dual tasks are synchronised across
 *       both cores (the worker is pinned to core 1).
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "task_tracer.h"
#include "esp_cpu.h"
#include "esp_timer.h"
#include <stdint.h>
#include <stdio.h>

#define US_TO_ITERS(us) ((us) * 40LL)

/* ── SINGLE-CORE (MONO) TASKS ───────────────────────────────────────────
 * Data table - each task is launched as a separate FreeRTOS task
 * without core affinity (tskNO_AFFINITY). The system decides on
 * which core to run it. */
typedef struct {
    const char *name;
    uint32_t    duration_us;
} mono_task_def_t;

static const mono_task_def_t mono_tasks[] = {
    { "task1", 67732 },   /* dominujące, podliniowe, c=0.535 */
    { "task2", 14530 },   /* dominujące, podliniowe, c=0.518 */
    { "task3", 4780 },   /* dominujące, podliniowe, c=0.559 */
    { "task4", 1761 },   /* dominujące, podliniowe, c=0.562 */
    { "task5", 132 },   /* nadliniowe, c=0.356 */
    { "task6", 119 },   /* nadliniowe, c=0.328 */
    { "task7", 106 },   /* podliniowe, c=0.906 */
    { "task8", 101 },   /* podliniowe, c=0.733 */
    { "task9", 100 },   /* podliniowe, c=0.760 */
    { "task10", 96 },   /* podliniowe, c=0.896 */
    { "task11", 92 },   /* podliniowe, c=0.783 */
    { "task12", 84 },   /* podliniowe, c=0.798 */
    { "task13", 74 },   /* podliniowe, c=0.595 */
    { "task14", 67 },   /* podliniowe, c=0.806 */
    { "task15", 66 },   /* podliniowe, c=0.803 */
    { "task16", 54 },   /* podliniowe, c=0.889 */
    { "task17", 54 },   /* podliniowe, c=0.574 */
    { "task18", 52 },   /* podliniowe, c=0.596 */
};
#define N_MONO_TASKS (sizeof(mono_tasks) / sizeof(mono_tasks[0]))

static SemaphoreHandle_t xMonoDoneSem = NULL;

/* ── DUAL-CORE TASK SYNCHRONISATION ──────────────────────────────────── */
static SemaphoreHandle_t xDualStartSem = NULL;
static SemaphoreHandle_t xDualEndSem   = NULL;
static volatile uint32_t    dual_workload_iters = 0;
static const char* volatile dual_task_name_c1   = NULL;

/* Busy loop simulating a synthetic CPU workload */
static void do_work(volatile long long iters) {
    volatile long long acc = 0;
    for (long long i = 0; i < iters; i++) acc += i;
    (void)acc;
}

/* Executes and traces a single single-core (Mono) task.
 * The core is read at run time - it is the one on which the scheduler
 * actually placed the current task. */
static void execute_mono_task(const char* name, uint32_t duration_us) {
    uint8_t core = esp_cpu_get_core_id();
    uint8_t prio = uxTaskPriorityGet(NULL);
    tracer_record(TRACER_EVT_SWITCHED_IN,  name, core, prio);
    do_work(US_TO_ITERS(duration_us));
    tracer_record(TRACER_EVT_SWITCHED_OUT, name, core, prio);
}

/* Short-lived FreeRTOS task wrapping a single mono task.
 * Created with tskNO_AFFINITY - the system assigns its core.
 * When done, it signals completion and deletes itself. */
static void vMonoTaskEntry(void *arg) {
    const mono_task_def_t *def = (const mono_task_def_t *)arg;
    execute_mono_task(def->name, def->duration_us);
    xSemaphoreGive(xMonoDoneSem);
    vTaskDelete(NULL);
}

/* Synchronises both cores to execute and trace a dual-core (Dual) task.
 *
 * `core_duration_us` is the task execution time, i.e. the wall-clock time
 * during which EACH core works. Both cores run in parallel for the same
 * time, so the total work is 2 * core_duration_us. */
static void execute_dual_task(const char* name_c0, const char* name_c1,
                               uint32_t core_duration_us) {
    uint8_t prio = uxTaskPriorityGet(NULL);
    dual_workload_iters = US_TO_ITERS(core_duration_us);
    dual_task_name_c1   = name_c1;
    xSemaphoreGive(xDualStartSem);
    tracer_record(TRACER_EVT_SWITCHED_IN,  name_c0, 0, prio);
    do_work(dual_workload_iters);
    xSemaphoreTake(xDualEndSem, portMAX_DELAY);
    tracer_record(TRACER_EVT_SWITCHED_OUT, name_c0, 0, prio);
}

/* ── CORE 1: HELPER THREAD FOR DUAL TASKS ────────────────────────────── */
static void vDualWorkerTaskCore1(void *arg) {
    uint8_t prio = uxTaskPriorityGet(NULL);
    while (1) {
        if (xSemaphoreTake(xDualStartSem, portMAX_DELAY) == pdTRUE) {
            tracer_record(TRACER_EVT_SWITCHED_IN,  dual_task_name_c1, 1, prio);
            do_work(dual_workload_iters);
            tracer_record(TRACER_EVT_SWITCHED_OUT, dual_task_name_c1, 1, prio);
            xSemaphoreGive(xDualEndSem);
        }
    }
}

/* ── MAIN CONTROL LOOP (n=36, 10% superlinear) ──
 * This task itself is created with tskNO_AFFINITY (see app_main),
 * so the scheduler may freely migrate it between cores. */
static void task_runner(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1) {
        /* Mono phase: each task is launched as a separate FreeRTOS task
         * without core affinity - the system decides the placement. */
        for (size_t k = 0; k < N_MONO_TASKS; k++) {
            xTaskCreatePinnedToCore(vMonoTaskEntry, mono_tasks[k].name, 2048,
                                     (void *)&mono_tasks[k], 5, NULL,
                                     tskNO_AFFINITY);
        }
        for (size_t k = 0; k < N_MONO_TASKS; k++) {
            xSemaphoreTake(xMonoDoneSem, portMAX_DELAY);
        }

        /* Dual phase */
        execute_dual_task("task1_0_d", "task1_1_d", 36246);
        execute_dual_task("task2_0_d", "task2_1_d", 7526);
        execute_dual_task("task3_0_d", "task3_1_d", 2673);
        execute_dual_task("task4_0_d", "task4_1_d", 989);
        execute_dual_task("task5_0_d", "task5_1_d", 47);
        execute_dual_task("task6_0_d", "task6_1_d", 39);
        execute_dual_task("task7_0_d", "task7_1_d", 96);
        execute_dual_task("task8_0_d", "task8_1_d", 74);
        execute_dual_task("task9_0_d", "task9_1_d", 76);
        execute_dual_task("task10_0_d", "task10_1_d", 86);
        execute_dual_task("task11_0_d", "task11_1_d", 72);
        execute_dual_task("task12_0_d", "task12_1_d", 67);
        execute_dual_task("task13_0_d", "task13_1_d", 44);
        execute_dual_task("task14_0_d", "task14_1_d", 54);
        execute_dual_task("task15_0_d", "task15_1_d", 53);
        execute_dual_task("task16_0_d", "task16_1_d", 48);
        execute_dual_task("task17_0_d", "task17_1_d", 31);
        execute_dual_task("task18_0_d", "task18_1_d", 31);

        /* Short pause, then start the next cycle */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ── SYSTEM INITIALISATION ────────────────────────────────────────────── */
void app_main(void) {
    tracer_init();
    xDualStartSem = xSemaphoreCreateBinary();
    xDualEndSem   = xSemaphoreCreateBinary();
    xMonoDoneSem  = xSemaphoreCreateCounting(N_MONO_TASKS, 0);

    xTaskCreatePinnedToCore(vDualWorkerTaskCore1, "dual_worker", 2048, NULL, 12, NULL, 1);
    xTaskCreatePinnedToCore(task_runner, "runner", 4096, NULL, 5, NULL, tskNO_AFFINITY);
}
