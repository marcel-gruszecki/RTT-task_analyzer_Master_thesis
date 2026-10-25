/**
 * @file Zadania_100_10.c
 * @brief Automatically generated test scenario (n = 200 tasks, 10% superlinear)
 *
 * Project: RTT-Task Analyser
 * Author: Marcel Gruszecki (UAM)
 * Generated: 2026-09-23 14:00
 * Description: 100 single-core (mono) tasks and 100 dual-core (dual)
 *       tasks, i.e. 200 tasks in total (50% / 50%).
 *       Share of superlinear tasks: 10%
 *       (10 superlinear / 90 sublinear).
 *
 *       Single-core times follow the imbalanced (cascade) model: the first
 *       4 tasks are dominant tasks, each longer than half of the budget
 *       remaining at its cascade step. This forces idle time in McNaughton's
 *       rule, so the lower bound LB is not attainable and differences
 *       between the scheduling algorithms become visible.
 *
 *       Sum of p_single    : 500000 us
 *       C_max, no parallel.: 351439.0 us
 *       LB                 : 249932.0 us
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
    { "task1", 351439 },   /* dominujące, podliniowe, c=0.547 */
    { "task2", 86906 },   /* dominujące, podliniowe, c=0.509 */
    { "task3", 46414 },   /* dominujące, podliniowe, c=0.582 */
    { "task4", 9960 },   /* dominujące, podliniowe, c=0.598 */
    { "task5", 96 },   /* nadliniowe, c=0.438 */
    { "task6", 59 },   /* nadliniowe, c=0.373 */
    { "task7", 59 },   /* nadliniowe, c=0.339 */
    { "task8", 59 },   /* nadliniowe, c=0.407 */
    { "task9", 59 },   /* nadliniowe, c=0.441 */
    { "task10", 59 },   /* nadliniowe, c=0.407 */
    { "task11", 59 },   /* nadliniowe, c=0.322 */
    { "task12", 59 },   /* nadliniowe, c=0.424 */
    { "task13", 59 },   /* nadliniowe, c=0.424 */
    { "task14", 58 },   /* nadliniowe, c=0.310 */
    { "task15", 58 },   /* podliniowe, c=0.897 */
    { "task16", 58 },   /* podliniowe, c=0.586 */
    { "task17", 58 },   /* podliniowe, c=0.828 */
    { "task18", 58 },   /* podliniowe, c=0.690 */
    { "task19", 58 },   /* podliniowe, c=0.603 */
    { "task20", 58 },   /* podliniowe, c=0.828 */
    { "task21", 57 },   /* podliniowe, c=0.842 */
    { "task22", 57 },   /* podliniowe, c=0.702 */
    { "task23", 57 },   /* podliniowe, c=0.754 */
    { "task24", 57 },   /* podliniowe, c=0.877 */
    { "task25", 57 },   /* podliniowe, c=0.842 */
    { "task26", 57 },   /* podliniowe, c=0.789 */
    { "task27", 57 },   /* podliniowe, c=0.667 */
    { "task28", 57 },   /* podliniowe, c=0.825 */
    { "task29", 57 },   /* podliniowe, c=0.614 */
    { "task30", 56 },   /* podliniowe, c=0.929 */
    { "task31", 56 },   /* podliniowe, c=0.607 */
    { "task32", 56 },   /* podliniowe, c=0.696 */
    { "task33", 56 },   /* podliniowe, c=0.732 */
    { "task34", 56 },   /* podliniowe, c=0.750 */
    { "task35", 56 },   /* podliniowe, c=0.929 */
    { "task36", 56 },   /* podliniowe, c=0.696 */
    { "task37", 56 },   /* podliniowe, c=0.839 */
    { "task38", 56 },   /* podliniowe, c=0.786 */
    { "task39", 56 },   /* podliniowe, c=0.643 */
    { "task40", 56 },   /* podliniowe, c=0.804 */
    { "task41", 56 },   /* podliniowe, c=0.768 */
    { "task42", 55 },   /* podliniowe, c=0.873 */
    { "task43", 55 },   /* podliniowe, c=0.618 */
    { "task44", 55 },   /* podliniowe, c=0.927 */
    { "task45", 55 },   /* podliniowe, c=0.673 */
    { "task46", 55 },   /* podliniowe, c=0.709 */
    { "task47", 55 },   /* podliniowe, c=0.873 */
    { "task48", 55 },   /* podliniowe, c=0.909 */
    { "task49", 54 },   /* podliniowe, c=0.593 */
    { "task50", 54 },   /* podliniowe, c=0.907 */
    { "task51", 54 },   /* podliniowe, c=0.759 */
    { "task52", 54 },   /* podliniowe, c=0.852 */
    { "task53", 54 },   /* podliniowe, c=0.778 */
    { "task54", 54 },   /* podliniowe, c=0.907 */
    { "task55", 54 },   /* podliniowe, c=0.833 */
    { "task56", 54 },   /* podliniowe, c=0.704 */
    { "task57", 54 },   /* podliniowe, c=0.778 */
    { "task58", 54 },   /* podliniowe, c=0.833 */
    { "task59", 54 },   /* podliniowe, c=0.722 */
    { "task60", 54 },   /* podliniowe, c=0.630 */
    { "task61", 54 },   /* podliniowe, c=0.870 */
    { "task62", 54 },   /* podliniowe, c=0.648 */
    { "task63", 54 },   /* podliniowe, c=0.704 */
    { "task64", 54 },   /* podliniowe, c=0.796 */
    { "task65", 54 },   /* podliniowe, c=0.667 */
    { "task66", 54 },   /* podliniowe, c=0.630 */
    { "task67", 54 },   /* podliniowe, c=0.815 */
    { "task68", 53 },   /* podliniowe, c=0.604 */
    { "task69", 53 },   /* podliniowe, c=0.642 */
    { "task70", 53 },   /* podliniowe, c=0.623 */
    { "task71", 53 },   /* podliniowe, c=0.925 */
    { "task72", 53 },   /* podliniowe, c=0.925 */
    { "task73", 53 },   /* podliniowe, c=0.774 */
    { "task74", 53 },   /* podliniowe, c=0.566 */
    { "task75", 53 },   /* podliniowe, c=0.943 */
    { "task76", 53 },   /* podliniowe, c=0.906 */
    { "task77", 53 },   /* podliniowe, c=0.585 */
    { "task78", 53 },   /* podliniowe, c=0.943 */
    { "task79", 53 },   /* podliniowe, c=0.830 */
    { "task80", 53 },   /* podliniowe, c=0.849 */
    { "task81", 52 },   /* podliniowe, c=0.577 */
    { "task82", 52 },   /* podliniowe, c=0.808 */
    { "task83", 52 },   /* podliniowe, c=0.769 */
    { "task84", 52 },   /* podliniowe, c=0.673 */
    { "task85", 52 },   /* podliniowe, c=0.808 */
    { "task86", 52 },   /* podliniowe, c=0.942 */
    { "task87", 52 },   /* podliniowe, c=0.673 */
    { "task88", 52 },   /* podliniowe, c=0.904 */
    { "task89", 51 },   /* podliniowe, c=0.804 */
    { "task90", 51 },   /* podliniowe, c=0.941 */
    { "task91", 51 },   /* podliniowe, c=0.882 */
    { "task92", 51 },   /* podliniowe, c=0.843 */
    { "task93", 51 },   /* podliniowe, c=0.784 */
    { "task94", 51 },   /* podliniowe, c=0.745 */
    { "task95", 50 },   /* podliniowe, c=0.880 */
    { "task96", 50 },   /* podliniowe, c=0.700 */
    { "task97", 50 },   /* podliniowe, c=0.860 */
    { "task98", 50 },   /* podliniowe, c=0.860 */
    { "task99", 50 },   /* podliniowe, c=0.560 */
    { "task100", 50 },   /* podliniowe, c=0.780 */
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

/* ── MAIN CONTROL LOOP (n=200, 10% superlinear) ──
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
        execute_dual_task("task1_0_d", "task1_1_d", 192163);
        execute_dual_task("task2_0_d", "task2_1_d", 44195);
        execute_dual_task("task3_0_d", "task3_1_d", 26996);
        execute_dual_task("task4_0_d", "task4_1_d", 5956);
        execute_dual_task("task5_0_d", "task5_1_d", 42);
        execute_dual_task("task6_0_d", "task6_1_d", 22);
        execute_dual_task("task7_0_d", "task7_1_d", 20);
        execute_dual_task("task8_0_d", "task8_1_d", 24);
        execute_dual_task("task9_0_d", "task9_1_d", 26);
        execute_dual_task("task10_0_d", "task10_1_d", 24);
        execute_dual_task("task11_0_d", "task11_1_d", 19);
        execute_dual_task("task12_0_d", "task12_1_d", 25);
        execute_dual_task("task13_0_d", "task13_1_d", 25);
        execute_dual_task("task14_0_d", "task14_1_d", 18);
        execute_dual_task("task15_0_d", "task15_1_d", 52);
        execute_dual_task("task16_0_d", "task16_1_d", 34);
        execute_dual_task("task17_0_d", "task17_1_d", 48);
        execute_dual_task("task18_0_d", "task18_1_d", 40);
        execute_dual_task("task19_0_d", "task19_1_d", 35);
        execute_dual_task("task20_0_d", "task20_1_d", 48);
        execute_dual_task("task21_0_d", "task21_1_d", 48);
        execute_dual_task("task22_0_d", "task22_1_d", 40);
        execute_dual_task("task23_0_d", "task23_1_d", 43);
        execute_dual_task("task24_0_d", "task24_1_d", 50);
        execute_dual_task("task25_0_d", "task25_1_d", 48);
        execute_dual_task("task26_0_d", "task26_1_d", 45);
        execute_dual_task("task27_0_d", "task27_1_d", 38);
        execute_dual_task("task28_0_d", "task28_1_d", 47);
        execute_dual_task("task29_0_d", "task29_1_d", 35);
        execute_dual_task("task30_0_d", "task30_1_d", 52);
        execute_dual_task("task31_0_d", "task31_1_d", 34);
        execute_dual_task("task32_0_d", "task32_1_d", 39);
        execute_dual_task("task33_0_d", "task33_1_d", 41);
        execute_dual_task("task34_0_d", "task34_1_d", 42);
        execute_dual_task("task35_0_d", "task35_1_d", 52);
        execute_dual_task("task36_0_d", "task36_1_d", 39);
        execute_dual_task("task37_0_d", "task37_1_d", 47);
        execute_dual_task("task38_0_d", "task38_1_d", 44);
        execute_dual_task("task39_0_d", "task39_1_d", 36);
        execute_dual_task("task40_0_d", "task40_1_d", 45);
        execute_dual_task("task41_0_d", "task41_1_d", 43);
        execute_dual_task("task42_0_d", "task42_1_d", 48);
        execute_dual_task("task43_0_d", "task43_1_d", 34);
        execute_dual_task("task44_0_d", "task44_1_d", 51);
        execute_dual_task("task45_0_d", "task45_1_d", 37);
        execute_dual_task("task46_0_d", "task46_1_d", 39);
        execute_dual_task("task47_0_d", "task47_1_d", 48);
        execute_dual_task("task48_0_d", "task48_1_d", 50);
        execute_dual_task("task49_0_d", "task49_1_d", 32);
        execute_dual_task("task50_0_d", "task50_1_d", 49);
        execute_dual_task("task51_0_d", "task51_1_d", 41);
        execute_dual_task("task52_0_d", "task52_1_d", 46);
        execute_dual_task("task53_0_d", "task53_1_d", 42);
        execute_dual_task("task54_0_d", "task54_1_d", 49);
        execute_dual_task("task55_0_d", "task55_1_d", 45);
        execute_dual_task("task56_0_d", "task56_1_d", 38);
        execute_dual_task("task57_0_d", "task57_1_d", 42);
        execute_dual_task("task58_0_d", "task58_1_d", 45);
        execute_dual_task("task59_0_d", "task59_1_d", 39);
        execute_dual_task("task60_0_d", "task60_1_d", 34);
        execute_dual_task("task61_0_d", "task61_1_d", 47);
        execute_dual_task("task62_0_d", "task62_1_d", 35);
        execute_dual_task("task63_0_d", "task63_1_d", 38);
        execute_dual_task("task64_0_d", "task64_1_d", 43);
        execute_dual_task("task65_0_d", "task65_1_d", 36);
        execute_dual_task("task66_0_d", "task66_1_d", 34);
        execute_dual_task("task67_0_d", "task67_1_d", 44);
        execute_dual_task("task68_0_d", "task68_1_d", 32);
        execute_dual_task("task69_0_d", "task69_1_d", 34);
        execute_dual_task("task70_0_d", "task70_1_d", 33);
        execute_dual_task("task71_0_d", "task71_1_d", 49);
        execute_dual_task("task72_0_d", "task72_1_d", 49);
        execute_dual_task("task73_0_d", "task73_1_d", 41);
        execute_dual_task("task74_0_d", "task74_1_d", 30);
        execute_dual_task("task75_0_d", "task75_1_d", 50);
        execute_dual_task("task76_0_d", "task76_1_d", 48);
        execute_dual_task("task77_0_d", "task77_1_d", 31);
        execute_dual_task("task78_0_d", "task78_1_d", 50);
        execute_dual_task("task79_0_d", "task79_1_d", 44);
        execute_dual_task("task80_0_d", "task80_1_d", 45);
        execute_dual_task("task81_0_d", "task81_1_d", 30);
        execute_dual_task("task82_0_d", "task82_1_d", 42);
        execute_dual_task("task83_0_d", "task83_1_d", 40);
        execute_dual_task("task84_0_d", "task84_1_d", 35);
        execute_dual_task("task85_0_d", "task85_1_d", 42);
        execute_dual_task("task86_0_d", "task86_1_d", 49);
        execute_dual_task("task87_0_d", "task87_1_d", 35);
        execute_dual_task("task88_0_d", "task88_1_d", 47);
        execute_dual_task("task89_0_d", "task89_1_d", 41);
        execute_dual_task("task90_0_d", "task90_1_d", 48);
        execute_dual_task("task91_0_d", "task91_1_d", 45);
        execute_dual_task("task92_0_d", "task92_1_d", 43);
        execute_dual_task("task93_0_d", "task93_1_d", 40);
        execute_dual_task("task94_0_d", "task94_1_d", 38);
        execute_dual_task("task95_0_d", "task95_1_d", 44);
        execute_dual_task("task96_0_d", "task96_1_d", 35);
        execute_dual_task("task97_0_d", "task97_1_d", 43);
        execute_dual_task("task98_0_d", "task98_1_d", 43);
        execute_dual_task("task99_0_d", "task99_1_d", 28);
        execute_dual_task("task100_0_d", "task100_1_d", 39);

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
