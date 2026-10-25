/**
 * @file Zadania_42_25.c
 * @brief Automatically generated test scenario (n = 84 tasks, 25% superlinear)
 *
 * Project: RTT-Task Analyser
 * Author: Marcel Gruszecki (UAM)
 * Generated: 2026-09-23 14:00
 * Description: 42 single-core (mono) tasks and 42 dual-core (dual)
 *       tasks, i.e. 84 tasks in total (50% / 50%).
 *       Share of superlinear tasks: 25%
 *       (10 superlinear / 32 sublinear).
 *
 *       Single-core times follow the imbalanced (cascade) model: the first
 *       4 tasks are dominant tasks, each longer than half of the budget
 *       remaining at its cascade step. This forces idle time in McNaughton's
 *       rule, so the lower bound LB is not attainable and differences
 *       between the scheduling algorithms become visible.
 *
 *       Sum of p_single    : 210000 us
 *       C_max, no parallel.: 155902.0 us
 *       LB                 : 104921.5 us
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
    { "task1", 155902 },   /* dominujące, podliniowe, c=0.527 */
    { "task2", 36199 },   /* dominujące, podliniowe, c=0.561 */
    { "task3", 12794 },   /* dominujące, podliniowe, c=0.564 */
    { "task4", 3132 },   /* dominujące, podliniowe, c=0.553 */
    { "task5", 68 },   /* nadliniowe, c=0.353 */
    { "task6", 53 },   /* nadliniowe, c=0.321 */
    { "task7", 53 },   /* nadliniowe, c=0.396 */
    { "task8", 53 },   /* nadliniowe, c=0.340 */
    { "task9", 53 },   /* nadliniowe, c=0.358 */
    { "task10", 53 },   /* nadliniowe, c=0.302 */
    { "task11", 53 },   /* nadliniowe, c=0.415 */
    { "task12", 53 },   /* nadliniowe, c=0.358 */
    { "task13", 53 },   /* nadliniowe, c=0.340 */
    { "task14", 53 },   /* nadliniowe, c=0.377 */
    { "task15", 52 },   /* podliniowe, c=0.808 */
    { "task16", 52 },   /* podliniowe, c=0.558 */
    { "task17", 52 },   /* podliniowe, c=0.673 */
    { "task18", 52 },   /* podliniowe, c=0.577 */
    { "task19", 52 },   /* podliniowe, c=0.846 */
    { "task20", 52 },   /* podliniowe, c=0.885 */
    { "task21", 52 },   /* podliniowe, c=0.808 */
    { "task22", 52 },   /* podliniowe, c=0.596 */
    { "task23", 52 },   /* podliniowe, c=0.750 */
    { "task24", 52 },   /* podliniowe, c=0.731 */
    { "task25", 52 },   /* podliniowe, c=0.615 */
    { "task26", 51 },   /* podliniowe, c=0.824 */
    { "task27", 51 },   /* podliniowe, c=0.608 */
    { "task28", 51 },   /* podliniowe, c=0.706 */
    { "task29", 51 },   /* podliniowe, c=0.922 */
    { "task30", 51 },   /* podliniowe, c=0.882 */
    { "task31", 51 },   /* podliniowe, c=0.627 */
    { "task32", 50 },   /* podliniowe, c=0.560 */
    { "task33", 50 },   /* podliniowe, c=0.900 */
    { "task34", 50 },   /* podliniowe, c=0.860 */
    { "task35", 50 },   /* podliniowe, c=0.860 */
    { "task36", 50 },   /* podliniowe, c=0.680 */
    { "task37", 50 },   /* podliniowe, c=0.740 */
    { "task38", 50 },   /* podliniowe, c=0.780 */
    { "task39", 50 },   /* podliniowe, c=0.860 */
    { "task40", 50 },   /* podliniowe, c=0.920 */
    { "task41", 50 },   /* podliniowe, c=0.780 */
    { "task42", 50 },   /* podliniowe, c=0.860 */
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

/* ── MAIN CONTROL LOOP (n=84, 25% superlinear) ──
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
        execute_dual_task("task1_0_d", "task1_1_d", 82212);
        execute_dual_task("task2_0_d", "task2_1_d", 20316);
        execute_dual_task("task3_0_d", "task3_1_d", 7214);
        execute_dual_task("task4_0_d", "task4_1_d", 1732);
        execute_dual_task("task5_0_d", "task5_1_d", 24);
        execute_dual_task("task6_0_d", "task6_1_d", 17);
        execute_dual_task("task7_0_d", "task7_1_d", 21);
        execute_dual_task("task8_0_d", "task8_1_d", 18);
        execute_dual_task("task9_0_d", "task9_1_d", 19);
        execute_dual_task("task10_0_d", "task10_1_d", 16);
        execute_dual_task("task11_0_d", "task11_1_d", 22);
        execute_dual_task("task12_0_d", "task12_1_d", 19);
        execute_dual_task("task13_0_d", "task13_1_d", 18);
        execute_dual_task("task14_0_d", "task14_1_d", 20);
        execute_dual_task("task15_0_d", "task15_1_d", 42);
        execute_dual_task("task16_0_d", "task16_1_d", 29);
        execute_dual_task("task17_0_d", "task17_1_d", 35);
        execute_dual_task("task18_0_d", "task18_1_d", 30);
        execute_dual_task("task19_0_d", "task19_1_d", 44);
        execute_dual_task("task20_0_d", "task20_1_d", 46);
        execute_dual_task("task21_0_d", "task21_1_d", 42);
        execute_dual_task("task22_0_d", "task22_1_d", 31);
        execute_dual_task("task23_0_d", "task23_1_d", 39);
        execute_dual_task("task24_0_d", "task24_1_d", 38);
        execute_dual_task("task25_0_d", "task25_1_d", 32);
        execute_dual_task("task26_0_d", "task26_1_d", 42);
        execute_dual_task("task27_0_d", "task27_1_d", 31);
        execute_dual_task("task28_0_d", "task28_1_d", 36);
        execute_dual_task("task29_0_d", "task29_1_d", 47);
        execute_dual_task("task30_0_d", "task30_1_d", 45);
        execute_dual_task("task31_0_d", "task31_1_d", 32);
        execute_dual_task("task32_0_d", "task32_1_d", 28);
        execute_dual_task("task33_0_d", "task33_1_d", 45);
        execute_dual_task("task34_0_d", "task34_1_d", 43);
        execute_dual_task("task35_0_d", "task35_1_d", 43);
        execute_dual_task("task36_0_d", "task36_1_d", 34);
        execute_dual_task("task37_0_d", "task37_1_d", 37);
        execute_dual_task("task38_0_d", "task38_1_d", 39);
        execute_dual_task("task39_0_d", "task39_1_d", 43);
        execute_dual_task("task40_0_d", "task40_1_d", 46);
        execute_dual_task("task41_0_d", "task41_1_d", 39);
        execute_dual_task("task42_0_d", "task42_1_d", 43);

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
