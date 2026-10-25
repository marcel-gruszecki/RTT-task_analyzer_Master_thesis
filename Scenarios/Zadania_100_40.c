/**
 * @file Zadania_100_40.c
 * @brief Automatically generated test scenario (n = 200 tasks, 40% superlinear)
 *
 * Project: RTT-Task Analyser
 * Author: Marcel Gruszecki (UAM)
 * Generated: 2026-09-23 14:00
 * Description: 100 single-core (mono) tasks and 100 dual-core (dual)
 *       tasks, i.e. 200 tasks in total (50% / 50%).
 *       Share of superlinear tasks: 40%
 *       (40 superlinear / 60 sublinear).
 *
 *       Single-core times follow the imbalanced (cascade) model: the first
 *       3 tasks are dominant tasks, each longer than half of the budget
 *       remaining at its cascade step. This forces idle time in McNaughton's
 *       rule, so the lower bound LB is not attainable and differences
 *       between the scheduling algorithms become visible.
 *
 *       Sum of p_single    : 500000 us
 *       C_max, no parallel.: 323303.0 us
 *       LB                 : 249377.5 us
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
    { "task1", 323303 },   /* dominujące, podliniowe, c=0.501 */
    { "task2", 133061 },   /* dominujące, podliniowe, c=0.506 */
    { "task3", 34176 },   /* dominujące, podliniowe, c=0.535 */
    { "task4", 173 },   /* nadliniowe, c=0.416 */
    { "task5", 145 },   /* nadliniowe, c=0.393 */
    { "task6", 144 },   /* nadliniowe, c=0.396 */
    { "task7", 144 },   /* nadliniowe, c=0.319 */
    { "task8", 143 },   /* nadliniowe, c=0.364 */
    { "task9", 141 },   /* nadliniowe, c=0.333 */
    { "task10", 139 },   /* nadliniowe, c=0.432 */
    { "task11", 138 },   /* nadliniowe, c=0.435 */
    { "task12", 137 },   /* nadliniowe, c=0.409 */
    { "task13", 137 },   /* nadliniowe, c=0.365 */
    { "task14", 136 },   /* nadliniowe, c=0.434 */
    { "task15", 132 },   /* nadliniowe, c=0.333 */
    { "task16", 131 },   /* nadliniowe, c=0.351 */
    { "task17", 131 },   /* nadliniowe, c=0.427 */
    { "task18", 129 },   /* nadliniowe, c=0.426 */
    { "task19", 129 },   /* nadliniowe, c=0.450 */
    { "task20", 129 },   /* nadliniowe, c=0.411 */
    { "task21", 129 },   /* nadliniowe, c=0.372 */
    { "task22", 129 },   /* nadliniowe, c=0.341 */
    { "task23", 127 },   /* nadliniowe, c=0.386 */
    { "task24", 127 },   /* nadliniowe, c=0.339 */
    { "task25", 126 },   /* nadliniowe, c=0.302 */
    { "task26", 125 },   /* nadliniowe, c=0.328 */
    { "task27", 124 },   /* nadliniowe, c=0.395 */
    { "task28", 123 },   /* nadliniowe, c=0.398 */
    { "task29", 122 },   /* nadliniowe, c=0.328 */
    { "task30", 121 },   /* nadliniowe, c=0.339 */
    { "task31", 119 },   /* nadliniowe, c=0.370 */
    { "task32", 118 },   /* nadliniowe, c=0.356 */
    { "task33", 118 },   /* nadliniowe, c=0.449 */
    { "task34", 118 },   /* nadliniowe, c=0.347 */
    { "task35", 117 },   /* nadliniowe, c=0.385 */
    { "task36", 113 },   /* nadliniowe, c=0.301 */
    { "task37", 113 },   /* nadliniowe, c=0.442 */
    { "task38", 111 },   /* nadliniowe, c=0.432 */
    { "task39", 110 },   /* nadliniowe, c=0.318 */
    { "task40", 108 },   /* nadliniowe, c=0.352 */
    { "task41", 106 },   /* nadliniowe, c=0.340 */
    { "task42", 101 },   /* nadliniowe, c=0.356 */
    { "task43", 100 },   /* nadliniowe, c=0.370 */
    { "task44", 100 },   /* podliniowe, c=0.750 */
    { "task45", 100 },   /* podliniowe, c=0.860 */
    { "task46", 100 },   /* podliniowe, c=0.900 */
    { "task47", 99 },   /* podliniowe, c=0.596 */
    { "task48", 98 },   /* podliniowe, c=0.673 */
    { "task49", 96 },   /* podliniowe, c=0.719 */
    { "task50", 96 },   /* podliniowe, c=0.927 */
    { "task51", 96 },   /* podliniowe, c=0.698 */
    { "task52", 95 },   /* podliniowe, c=0.716 */
    { "task53", 94 },   /* podliniowe, c=0.840 */
    { "task54", 94 },   /* podliniowe, c=0.894 */
    { "task55", 94 },   /* podliniowe, c=0.798 */
    { "task56", 94 },   /* podliniowe, c=0.894 */
    { "task57", 93 },   /* podliniowe, c=0.720 */
    { "task58", 91 },   /* podliniowe, c=0.813 */
    { "task59", 91 },   /* podliniowe, c=0.571 */
    { "task60", 90 },   /* podliniowe, c=0.656 */
    { "task61", 89 },   /* podliniowe, c=0.798 */
    { "task62", 88 },   /* podliniowe, c=0.773 */
    { "task63", 87 },   /* podliniowe, c=0.828 */
    { "task64", 87 },   /* podliniowe, c=0.897 */
    { "task65", 86 },   /* podliniowe, c=0.570 */
    { "task66", 85 },   /* podliniowe, c=0.741 */
    { "task67", 82 },   /* podliniowe, c=0.890 */
    { "task68", 79 },   /* podliniowe, c=0.759 */
    { "task69", 78 },   /* podliniowe, c=0.910 */
    { "task70", 78 },   /* podliniowe, c=0.628 */
    { "task71", 77 },   /* podliniowe, c=0.597 */
    { "task72", 76 },   /* podliniowe, c=0.842 */
    { "task73", 76 },   /* podliniowe, c=0.566 */
    { "task74", 76 },   /* podliniowe, c=0.895 */
    { "task75", 75 },   /* podliniowe, c=0.653 */
    { "task76", 74 },   /* podliniowe, c=0.797 */
    { "task77", 74 },   /* podliniowe, c=0.608 */
    { "task78", 73 },   /* podliniowe, c=0.918 */
    { "task79", 71 },   /* podliniowe, c=0.620 */
    { "task80", 71 },   /* podliniowe, c=0.620 */
    { "task81", 69 },   /* podliniowe, c=0.826 */
    { "task82", 69 },   /* podliniowe, c=0.797 */
    { "task83", 68 },   /* podliniowe, c=0.868 */
    { "task84", 66 },   /* podliniowe, c=0.606 */
    { "task85", 63 },   /* podliniowe, c=0.587 */
    { "task86", 62 },   /* podliniowe, c=0.565 */
    { "task87", 62 },   /* podliniowe, c=0.806 */
    { "task88", 62 },   /* podliniowe, c=0.758 */
    { "task89", 61 },   /* podliniowe, c=0.820 */
    { "task90", 61 },   /* podliniowe, c=0.574 */
    { "task91", 59 },   /* podliniowe, c=0.932 */
    { "task92", 59 },   /* podliniowe, c=0.797 */
    { "task93", 58 },   /* podliniowe, c=0.638 */
    { "task94", 56 },   /* podliniowe, c=0.893 */
    { "task95", 55 },   /* podliniowe, c=0.745 */
    { "task96", 55 },   /* podliniowe, c=0.582 */
    { "task97", 55 },   /* podliniowe, c=0.618 */
    { "task98", 53 },   /* podliniowe, c=0.736 */
    { "task99", 51 },   /* podliniowe, c=0.765 */
    { "task100", 50 },   /* podliniowe, c=0.820 */
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

/* ── MAIN CONTROL LOOP (n=200, 40% superlinear) ──
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
        execute_dual_task("task1_0_d", "task1_1_d", 162095);
        execute_dual_task("task2_0_d", "task2_1_d", 67276);
        execute_dual_task("task3_0_d", "task3_1_d", 18286);
        execute_dual_task("task4_0_d", "task4_1_d", 72);
        execute_dual_task("task5_0_d", "task5_1_d", 57);
        execute_dual_task("task6_0_d", "task6_1_d", 57);
        execute_dual_task("task7_0_d", "task7_1_d", 46);
        execute_dual_task("task8_0_d", "task8_1_d", 52);
        execute_dual_task("task9_0_d", "task9_1_d", 47);
        execute_dual_task("task10_0_d", "task10_1_d", 60);
        execute_dual_task("task11_0_d", "task11_1_d", 60);
        execute_dual_task("task12_0_d", "task12_1_d", 56);
        execute_dual_task("task13_0_d", "task13_1_d", 50);
        execute_dual_task("task14_0_d", "task14_1_d", 59);
        execute_dual_task("task15_0_d", "task15_1_d", 44);
        execute_dual_task("task16_0_d", "task16_1_d", 46);
        execute_dual_task("task17_0_d", "task17_1_d", 56);
        execute_dual_task("task18_0_d", "task18_1_d", 55);
        execute_dual_task("task19_0_d", "task19_1_d", 58);
        execute_dual_task("task20_0_d", "task20_1_d", 53);
        execute_dual_task("task21_0_d", "task21_1_d", 48);
        execute_dual_task("task22_0_d", "task22_1_d", 44);
        execute_dual_task("task23_0_d", "task23_1_d", 49);
        execute_dual_task("task24_0_d", "task24_1_d", 43);
        execute_dual_task("task25_0_d", "task25_1_d", 38);
        execute_dual_task("task26_0_d", "task26_1_d", 41);
        execute_dual_task("task27_0_d", "task27_1_d", 49);
        execute_dual_task("task28_0_d", "task28_1_d", 49);
        execute_dual_task("task29_0_d", "task29_1_d", 40);
        execute_dual_task("task30_0_d", "task30_1_d", 41);
        execute_dual_task("task31_0_d", "task31_1_d", 44);
        execute_dual_task("task32_0_d", "task32_1_d", 42);
        execute_dual_task("task33_0_d", "task33_1_d", 53);
        execute_dual_task("task34_0_d", "task34_1_d", 41);
        execute_dual_task("task35_0_d", "task35_1_d", 45);
        execute_dual_task("task36_0_d", "task36_1_d", 34);
        execute_dual_task("task37_0_d", "task37_1_d", 50);
        execute_dual_task("task38_0_d", "task38_1_d", 48);
        execute_dual_task("task39_0_d", "task39_1_d", 35);
        execute_dual_task("task40_0_d", "task40_1_d", 38);
        execute_dual_task("task41_0_d", "task41_1_d", 36);
        execute_dual_task("task42_0_d", "task42_1_d", 36);
        execute_dual_task("task43_0_d", "task43_1_d", 37);
        execute_dual_task("task44_0_d", "task44_1_d", 75);
        execute_dual_task("task45_0_d", "task45_1_d", 86);
        execute_dual_task("task46_0_d", "task46_1_d", 90);
        execute_dual_task("task47_0_d", "task47_1_d", 59);
        execute_dual_task("task48_0_d", "task48_1_d", 66);
        execute_dual_task("task49_0_d", "task49_1_d", 69);
        execute_dual_task("task50_0_d", "task50_1_d", 89);
        execute_dual_task("task51_0_d", "task51_1_d", 67);
        execute_dual_task("task52_0_d", "task52_1_d", 68);
        execute_dual_task("task53_0_d", "task53_1_d", 79);
        execute_dual_task("task54_0_d", "task54_1_d", 84);
        execute_dual_task("task55_0_d", "task55_1_d", 75);
        execute_dual_task("task56_0_d", "task56_1_d", 84);
        execute_dual_task("task57_0_d", "task57_1_d", 67);
        execute_dual_task("task58_0_d", "task58_1_d", 74);
        execute_dual_task("task59_0_d", "task59_1_d", 52);
        execute_dual_task("task60_0_d", "task60_1_d", 59);
        execute_dual_task("task61_0_d", "task61_1_d", 71);
        execute_dual_task("task62_0_d", "task62_1_d", 68);
        execute_dual_task("task63_0_d", "task63_1_d", 72);
        execute_dual_task("task64_0_d", "task64_1_d", 78);
        execute_dual_task("task65_0_d", "task65_1_d", 49);
        execute_dual_task("task66_0_d", "task66_1_d", 63);
        execute_dual_task("task67_0_d", "task67_1_d", 73);
        execute_dual_task("task68_0_d", "task68_1_d", 60);
        execute_dual_task("task69_0_d", "task69_1_d", 71);
        execute_dual_task("task70_0_d", "task70_1_d", 49);
        execute_dual_task("task71_0_d", "task71_1_d", 46);
        execute_dual_task("task72_0_d", "task72_1_d", 64);
        execute_dual_task("task73_0_d", "task73_1_d", 43);
        execute_dual_task("task74_0_d", "task74_1_d", 68);
        execute_dual_task("task75_0_d", "task75_1_d", 49);
        execute_dual_task("task76_0_d", "task76_1_d", 59);
        execute_dual_task("task77_0_d", "task77_1_d", 45);
        execute_dual_task("task78_0_d", "task78_1_d", 67);
        execute_dual_task("task79_0_d", "task79_1_d", 44);
        execute_dual_task("task80_0_d", "task80_1_d", 44);
        execute_dual_task("task81_0_d", "task81_1_d", 57);
        execute_dual_task("task82_0_d", "task82_1_d", 55);
        execute_dual_task("task83_0_d", "task83_1_d", 59);
        execute_dual_task("task84_0_d", "task84_1_d", 40);
        execute_dual_task("task85_0_d", "task85_1_d", 37);
        execute_dual_task("task86_0_d", "task86_1_d", 35);
        execute_dual_task("task87_0_d", "task87_1_d", 50);
        execute_dual_task("task88_0_d", "task88_1_d", 47);
        execute_dual_task("task89_0_d", "task89_1_d", 50);
        execute_dual_task("task90_0_d", "task90_1_d", 35);
        execute_dual_task("task91_0_d", "task91_1_d", 55);
        execute_dual_task("task92_0_d", "task92_1_d", 47);
        execute_dual_task("task93_0_d", "task93_1_d", 37);
        execute_dual_task("task94_0_d", "task94_1_d", 50);
        execute_dual_task("task95_0_d", "task95_1_d", 41);
        execute_dual_task("task96_0_d", "task96_1_d", 32);
        execute_dual_task("task97_0_d", "task97_1_d", 34);
        execute_dual_task("task98_0_d", "task98_1_d", 39);
        execute_dual_task("task99_0_d", "task99_1_d", 39);
        execute_dual_task("task100_0_d", "task100_1_d", 41);

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
