#!/usr/bin/env python3
"""FreeRTOS test scenario generator for RTT-Task Analyser.

Author:  Marcel Gruszecki

"""

import argparse
import math
import os
import random
import sys
from datetime import datetime

# Allowed total task counts (50% mono + 50% dual).
# The scheduling instance has n/2 tasks, each with a mono and a dual time.
DOZWOLONE_N = [12, 24, 36, 48, 60, 72, 84, 96, 200]

# Allowed percentages of superlinear tasks (the rest are sublinear)
DOZWOLONE_PROCENTY = [0, 10, 25, 40]

# Ranges of the scaling factor c = p_dual / p_single.
# c < 0.5 means superlinear speedup (greater than the number of cores M = 2).
ZAKRES_NADLINIOWE = (0.30, 0.45)
ZAKRES_PODLINIOWE = (0.55, 0.95)

# Range of c for dominant (longest) tasks, which are never superlinear.
# Guarantees a decent speedup (1.67x-2x, not necessarily superlinear),
# so that parallelizing the tasks that determine C_max can pay off.
ZAKRES_DOMINUJACE = (0.50, 0.60)

# Mean single-core time per task (us).
# The total pool budget is T_1 = liczba_zadan * SREDNI_CZAS_ZADANIA_US.
SREDNI_CZAS_ZADANIA_US = 5000

# Minimum time of a single task (us) — prevents tasks at the end of the
# cascade, where the remaining budget is small, from shrinking to zero.
MIN_CZAS_US = 50

# Share of the remaining budget taken by the dominant task at a cascade step:
# p_k = u * T_k, with u drawn from this range. The lower bound must be
# clearly above 0.5 so that the task actually determines C_max.
ZAKRES_DOMINACJI = (0.55, 0.80)

# Maximum number of cascade steps (the budget decays exponentially, so
# further steps would drop below MIN_CZAS_US anyway).
MAX_KROKOW_DOMINACJI = 5


def liczba_krokow_dominacji(liczba_zadan):
    """Number of dominant tasks in the cascade.

    The budget shrinks by a factor of ~(1-u) at every step, so a sensible
    number of steps grows logarithmically with the instance size."""
    if liczba_zadan < 3:
        return 1
    return max(1, min(MAX_KROKOW_DOMINACJI, round(math.log2(liczba_zadan))))


def _rozdziel_losowo(budzet, ile, rng):
    """Splits `budzet` (us) into `ile` random integer parts, each >= MIN_CZAS_US."""
    if ile <= 0:
        return []

    baza = MIN_CZAS_US * ile
    if budzet < baza:
        raise ValueError(
            f"budżet {budzet} us jest za mały, by dać {ile} zadaniom po "
            f"{MIN_CZAS_US} us — zwiększ SREDNI_CZAS_ZADANIA_US"
        )

    swobodne = budzet - baza
    wagi = [rng.random() for _ in range(ile)]
    suma_wag = sum(wagi) or 1.0

    czasy = [MIN_CZAS_US + int(swobodne * w / suma_wag) for w in wagi]
    # Add the rounding remainder to a random task so that the sum is exact.
    czasy[rng.randrange(ile)] += budzet - sum(czasy)
    return czasy


def wygeneruj_czasy_jednordzeniowe(liczba_zadan, rng):
    """Builds an imbalanced pool of single-core times (us) — the cascade model.

    Returns `(czasy, liczba_dominujacych)`, where `czasy` is sorted in
    descending order and the first `liczba_dominujacych` entries are the
    consecutive cascade tasks (each longer than half of the budget remaining
    at its step)."""
    budzet_calkowity = liczba_zadan * SREDNI_CZAS_ZADANIA_US
    kroki = liczba_krokow_dominacji(liczba_zadan)

    dominujace = []
    pozostalo = budzet_calkowity

    for _ in range(kroki):
        # How many tasks remain to be filled after adding the current one.
        do_obsadzenia = liczba_zadan - len(dominujace) - 1
        if do_obsadzenia < 1:
            break

        # The task must leave at least MIN_CZAS_US for each remaining task.
        dol = pozostalo * ZAKRES_DOMINACJI[0]
        gora = min(pozostalo * ZAKRES_DOMINACJI[1],
                   pozostalo - do_obsadzenia * MIN_CZAS_US)
        if gora <= dol:
            break

        p = int(rng.uniform(dol, gora))
        dominujace.append(p)
        pozostalo -= p

    niedominujace = _rozdziel_losowo(pozostalo, liczba_zadan - len(dominujace), rng)
    niedominujace.sort(reverse=True)

    return dominujace + niedominujace, len(dominujace)


def wylosuj_kategorie(czasy, liczba_dominujacych, procent_nadliniowych, rng, tryb):
    """Assigns a scaling category (superlinear / sublinear) to every task.

    Dominant tasks are NEVER superlinear. A superlinear task is better run on
    two cores in terms of both time and work (2*p_dual < p_single), so every
    algorithm trivially parallelizes it — if that applied to a dominant task,
    the only non-trivial decision would disappear and all algorithms would
    reach LB. Instead, dominant tasks get a decent but sublinear speedup
    (ZAKRES_DOMINUJACE), where parallelization costs extra work and the
    algorithm has to decide whether it pays off.

    The superlinear quota is distributed among non-dominant tasks:
    `tryb='dlugie-pierwsze'` assigns it to the longest ones first,
    `tryb='losowe'` assigns it randomly."""
    n = len(czasy)
    niedominujace = list(range(liczba_dominujacych, n))
    liczba_nadliniowych = min(round(n * procent_nadliniowych / 100), len(niedominujace))

    if tryb == 'dlugie-pierwsze':
        kolejnosc = sorted(niedominujace, key=lambda i: -czasy[i])
    else:
        kolejnosc = niedominujace
        rng.shuffle(kolejnosc)

    kategorie = ['podliniowe'] * n
    for i in kolejnosc[:liczba_nadliniowych]:
        kategorie[i] = 'nadliniowe'

    # Dominant (longest) tasks always get a guaranteed decent speedup,
    # regardless of the mode — see `losuj_wspolczynnik`.
    uprzywilejowane = set(range(liczba_dominujacych))

    return kategorie, uprzywilejowane


def losuj_wspolczynnik(kategoria, uprzywilejowane, rng):
    """Draws c = p_dual / p_single for the given scaling category.

    Privileged tasks (dominant, always sublinear) get c from
    ZAKRES_DOMINUJACE: the speedup is still at most linear, but decent
    enough that parallelizing a dominant task may pay off."""
    if uprzywilejowane:
        dol, gora = ZAKRES_DOMINUJACE
    elif kategoria == 'nadliniowe':
        dol, gora = ZAKRES_NADLINIOWE
    else:
        dol, gora = ZAKRES_PODLINIOWE
    return rng.uniform(dol, gora)


def dolne_oszacowanie(czasy_single, czasy_dual):
    """Lower bound LB for P2 | pmtn, spdp-any | C_max.

    Every task consumes at least min(p^(1), 2*p^(2)) of processor time, and
    two cores process this work fastest with zero idle time. Independently,
    no task can take less than min(p^(1), p^(2))."""
    praca = sum(min(s, 2 * d) for s, d in zip(czasy_single, czasy_dual))
    najdluzsze = max((min(s, d) for s, d in zip(czasy_single, czasy_dual)), default=0)
    return max(praca / 2.0, najdluzsze)


def cmax_bez_zrownoleglania(czasy_single):
    """C_max of McNaughton's rule when no task is moved to S2 (no parallelization)."""
    if not czasy_single:
        return 0.0
    return max(max(czasy_single), sum(czasy_single) / 2.0)


def zbuduj_zadania(liczba_zadan, procent_nadliniowych, rng, tryb_przyspieszenia):
    """Returns a list of dicts describing the tasks of an instance."""
    czasy, liczba_dominujacych = wygeneruj_czasy_jednordzeniowe(liczba_zadan, rng)
    kategorie, uprzywilejowane = wylosuj_kategorie(
        czasy, liczba_dominujacych, procent_nadliniowych, rng, tryb_przyspieszenia
    )

    zadania = []
    for i, (p_single, kategoria) in enumerate(zip(czasy, kategorie)):
        c = losuj_wspolczynnik(kategoria, i in uprzywilejowane, rng)
        p_dual = max(1, round(p_single * c))
        zadania.append({
            "nazwa": f"task{i + 1}",
            "p_single": p_single,
            "p_dual": p_dual,
            "kategoria": kategoria,
            "dominujace": i < liczba_dominujacych,
        })

    return zadania, liczba_dominujacych


def wygeneruj_c(n, procent_nadliniowych, sciezka_wyjsciowa, rng, tryb_przyspieszenia):
    if n % 2 != 0:
        print(f"Błąd: n={n} musi być liczbą parzystą!")
        sys.exit(1)

    liczba_zadan = n // 2
    zadania, liczba_dominujacych = zbuduj_zadania(
        liczba_zadan, procent_nadliniowych, rng, tryb_przyspieszenia
    )

    mono_wpisy = []
    dual_wywolania = []
    for z in zadania:
        znacznik = "dominujące, " if z["dominujace"] else ""
        mono_wpisy.append(
            f'    {{ "{z["nazwa"]}", {z["p_single"]} }},   '
            f'/* {znacznik}{z["kategoria"]}, c={z["p_dual"] / z["p_single"]:.3f} */'
        )
        dual_wywolania.append(
            f'        execute_dual_task("{z["nazwa"]}_0_d", "{z["nazwa"]}_1_d", '
            f'{z["p_dual"]});'
        )

    mono_table = "\n".join(mono_wpisy)
    dual_blocks = "\n".join(dual_wywolania)
    data_gen = datetime.now().strftime("%Y-%m-%d %H:%M")

    czasy_single = [z["p_single"] for z in zadania]
    czasy_dual = [z["p_dual"] for z in zadania]
    lb = dolne_oszacowanie(czasy_single, czasy_dual)
    cmax_sekw = cmax_bez_zrownoleglania(czasy_single)
    liczba_nadliniowych = sum(1 for z in zadania if z["kategoria"] == 'nadliniowe')
    liczba_podliniowych = liczba_zadan - liczba_nadliniowych

    c_template = f"""/**
 * @file {os.path.basename(sciezka_wyjsciowa)}
 * @brief Automatically generated test scenario (n = {n} tasks, {procent_nadliniowych}% superlinear)
 *
 * Project: RTT-Task Analyser
 * Author: Marcel Gruszecki (UAM)
 * Generated: {data_gen}
 * Description: {liczba_zadan} single-core (mono) tasks and {liczba_zadan} dual-core (dual)
 *       tasks, i.e. {n} tasks in total (50% / 50%).
 *       Share of superlinear tasks: {procent_nadliniowych}%
 *       ({liczba_nadliniowych} superlinear / {liczba_podliniowych} sublinear).
 *
 *       Single-core times follow the imbalanced (cascade) model: the first
 *       {liczba_dominujacych} tasks are dominant tasks, each longer than half of the budget
 *       remaining at its cascade step. This forces idle time in McNaughton's
 *       rule, so the lower bound LB is not attainable and differences
 *       between the scheduling algorithms become visible.
 *
 *       Sum of p_single    : {sum(czasy_single)} us
 *       C_max, no parallel.: {cmax_sekw:.1f} us
 *       LB                 : {lb:.1f} us
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
typedef struct {{
    const char *name;
    uint32_t    duration_us;
}} mono_task_def_t;

static const mono_task_def_t mono_tasks[] = {{
{mono_table}
}};
#define N_MONO_TASKS (sizeof(mono_tasks) / sizeof(mono_tasks[0]))

static SemaphoreHandle_t xMonoDoneSem = NULL;

/* ── DUAL-CORE TASK SYNCHRONISATION ──────────────────────────────────── */
static SemaphoreHandle_t xDualStartSem = NULL;
static SemaphoreHandle_t xDualEndSem   = NULL;
static volatile uint32_t    dual_workload_iters = 0;
static const char* volatile dual_task_name_c1   = NULL;

/* Busy loop simulating a synthetic CPU workload */
static void do_work(volatile long long iters) {{
    volatile long long acc = 0;
    for (long long i = 0; i < iters; i++) acc += i;
    (void)acc;
}}

/* Executes and traces a single single-core (Mono) task.
 * The core is read at run time - it is the one on which the scheduler
 * actually placed the current task. */
static void execute_mono_task(const char* name, uint32_t duration_us) {{
    uint8_t core = esp_cpu_get_core_id();
    uint8_t prio = uxTaskPriorityGet(NULL);
    tracer_record(TRACER_EVT_SWITCHED_IN,  name, core, prio);
    do_work(US_TO_ITERS(duration_us));
    tracer_record(TRACER_EVT_SWITCHED_OUT, name, core, prio);
}}

/* Short-lived FreeRTOS task wrapping a single mono task.
 * Created with tskNO_AFFINITY - the system assigns its core.
 * When done, it signals completion and deletes itself. */
static void vMonoTaskEntry(void *arg) {{
    const mono_task_def_t *def = (const mono_task_def_t *)arg;
    execute_mono_task(def->name, def->duration_us);
    xSemaphoreGive(xMonoDoneSem);
    vTaskDelete(NULL);
}}

/* Synchronises both cores to execute and trace a dual-core (Dual) task.
 *
 * `core_duration_us` is the task execution time, i.e. the wall-clock time
 * during which EACH core works. Both cores run in parallel for the same
 * time, so the total work is 2 * core_duration_us. */
static void execute_dual_task(const char* name_c0, const char* name_c1,
                               uint32_t core_duration_us) {{
    uint8_t prio = uxTaskPriorityGet(NULL);
    dual_workload_iters = US_TO_ITERS(core_duration_us);
    dual_task_name_c1   = name_c1;
    xSemaphoreGive(xDualStartSem);
    tracer_record(TRACER_EVT_SWITCHED_IN,  name_c0, 0, prio);
    do_work(dual_workload_iters);
    xSemaphoreTake(xDualEndSem, portMAX_DELAY);
    tracer_record(TRACER_EVT_SWITCHED_OUT, name_c0, 0, prio);
}}

/* ── CORE 1: HELPER THREAD FOR DUAL TASKS ────────────────────────────── */
static void vDualWorkerTaskCore1(void *arg) {{
    uint8_t prio = uxTaskPriorityGet(NULL);
    while (1) {{
        if (xSemaphoreTake(xDualStartSem, portMAX_DELAY) == pdTRUE) {{
            tracer_record(TRACER_EVT_SWITCHED_IN,  dual_task_name_c1, 1, prio);
            do_work(dual_workload_iters);
            tracer_record(TRACER_EVT_SWITCHED_OUT, dual_task_name_c1, 1, prio);
            xSemaphoreGive(xDualEndSem);
        }}
    }}
}}

/* ── MAIN CONTROL LOOP (n={n}, {procent_nadliniowych}% superlinear) ──
 * This task itself is created with tskNO_AFFINITY (see app_main),
 * so the scheduler may freely migrate it between cores. */
static void task_runner(void *arg) {{
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1) {{
        /* Mono phase: each task is launched as a separate FreeRTOS task
         * without core affinity - the system decides the placement. */
        for (size_t k = 0; k < N_MONO_TASKS; k++) {{
            xTaskCreatePinnedToCore(vMonoTaskEntry, mono_tasks[k].name, 2048,
                                     (void *)&mono_tasks[k], 5, NULL,
                                     tskNO_AFFINITY);
        }}
        for (size_t k = 0; k < N_MONO_TASKS; k++) {{
            xSemaphoreTake(xMonoDoneSem, portMAX_DELAY);
        }}

        /* Dual phase */
{dual_blocks}

        /* Short pause, then start the next cycle */
        vTaskDelay(pdMS_TO_TICKS(20));
    }}
}}

/* ── SYSTEM INITIALISATION ────────────────────────────────────────────── */
void app_main(void) {{
    tracer_init();
    xDualStartSem = xSemaphoreCreateBinary();
    xDualEndSem   = xSemaphoreCreateBinary();
    xMonoDoneSem  = xSemaphoreCreateCounting(N_MONO_TASKS, 0);

    xTaskCreatePinnedToCore(vDualWorkerTaskCore1, "dual_worker", 2048, NULL, 12, NULL, 1);
    xTaskCreatePinnedToCore(task_runner, "runner", 4096, NULL, 5, NULL, tskNO_AFFINITY);
}}
"""

    with open(sciezka_wyjsciowa, "w", encoding="utf-8") as f:
        f.write(c_template)

    print(f"Wygenerowano {sciezka_wyjsciowa}: zadań={liczba_zadan}, "
          f"dominujących={liczba_dominujacych}, nadliniowych={liczba_nadliniowych}, "
          f"podliniowych={liczba_podliniowych}, "
          f"LB={lb:.1f} us, Cmax bez zrównoleglania={cmax_sekw:.1f} us "
          f"(zapas {100.0 * (cmax_sekw - lb) / cmax_sekw:.1f}%)")

    return zadania


def zapisz_csv(sciezka, wiersze, separator):
    """Writes the task structure table in the format read by `sched` (main.rs)."""
    with open(sciezka, "w", encoding="utf-8") as out:
        naglowek = separator.join(
            ["Scenariusz", "Ilosc zadan", "Nazwa zadania", "Tryb",
             "p_single (us)", "p_dual (us)"]
        )
        out.write(naglowek + "\n")
        for w in wiersze:
            out.write(separator.join(str(x) for x in w) + "\n")
    print(f"\nZapisano tabelę struktury zadań: {sciezka} ({len(wiersze)} wierszy)")


def main():
    global SREDNI_CZAS_ZADANIA_US

    parser = argparse.ArgumentParser(
        description="Generator scenariuszy FreeRTOS dla RTT-Task Analyser "
                    "(niezrównoważone czasy wymuszające przestój McNaughtona)"
    )
    parser.add_argument('--n', type=int, choices=DOZWOLONE_N,
                        help="Całkowita liczba zadań (50%% mono + 50%% dual). "
                             f"Dozwolone wartości: {DOZWOLONE_N}")
    parser.add_argument('--procent', type=int, choices=DOZWOLONE_PROCENTY,
                        help="Procent zadań nadliniowych (reszta jest "
                             f"podliniowa). Dozwolone wartości: {DOZWOLONE_PROCENTY}")
    parser.add_argument('--out', type=str, default=None,
                        help="Nazwa wygenerowanego pliku "
                             "(domyślnie: Zadania_<zadan>_<procent>.c)")
    parser.add_argument('--wszystkie', action='store_true',
                        help="Generuje WSZYSTKIE kombinacje n x procent "
                             f"({len(DOZWOLONE_N)}x{len(DOZWOLONE_PROCENTY)} plików) "
                             "do katalogu podanego przez --katalog, ignorując --n/--procent/--out")
    parser.add_argument('--katalog', type=str, default="scenariusze",
                        help="Katalog docelowy przy użyciu --wszystkie (domyślnie: scenariusze)")
    parser.add_argument('--seed', type=int, default=None,
                        help="Ziarno generatora losowego (dla powtarzalności wyników)")
    parser.add_argument('--csv', type=str, default=None,
                        help="Dodatkowo zapisuje tabelę struktury zadań (kolumny "
                             "Scenariusz, Ilosc zadan, Nazwa zadania, Tryb, p_single, p_dual) "
                             "prosto z generatora, bez pośrednictwa parsowania plików .c")
    parser.add_argument('--csv-separator', type=str, default=',',
                        help="Separator kolumn w pliku --csv (domyślnie przecinek)")
    parser.add_argument('--przyspieszenie', choices=['dlugie-pierwsze', 'losowe'],
                        default='dlugie-pierwsze',
                        help="Sposób przypisania kategorii nadliniowej wśród "
                             "zadań niedominujących (dominujące są zawsze "
                             "podliniowe z przyzwoitym przyspieszeniem). "
                             "'dlugie-pierwsze' (domyślnie) wybiera najdłuższe "
                             "z nich; 'losowe' wybiera losowo.")
    parser.add_argument('--srednia-dlugosc', type=int, default=SREDNI_CZAS_ZADANIA_US,
                        help=f"Średni czas jednordzeniowy na zadanie w us "
                             f"(domyślnie {SREDNI_CZAS_ZADANIA_US}); wyznacza budżet "
                             f"T_1 = liczba_zadan * ta wartość")

    args = parser.parse_args()

    SREDNI_CZAS_ZADANIA_US = args.srednia_dlugosc

    rng = random.Random(args.seed)

    if args.wszystkie:
        os.makedirs(args.katalog, exist_ok=True)
        wiersze_csv = []
        for n in DOZWOLONE_N:
            for procent in DOZWOLONE_PROCENTY:
                liczba_zadan = n // 2
                nazwa = f"Zadania_{liczba_zadan}_{procent}.c"
                sciezka = os.path.join(args.katalog, nazwa)
                zadania = wygeneruj_c(n, procent, sciezka, rng, args.przyspieszenie)
                for z in zadania:
                    wiersze_csv.append((procent, liczba_zadan, z["nazwa"],
                                        "Mono&Dual", z["p_single"], z["p_dual"]))

        print(f"\nGotowe. Wygenerowano {len(DOZWOLONE_N) * len(DOZWOLONE_PROCENTY)} "
              f"plików w katalogu '{args.katalog}/'.")
        if args.csv:
            # Same order as in the original table: scenario first, then task count.
            wiersze_csv.sort(key=lambda w: (w[0], w[1]))
            zapisz_csv(args.csv, wiersze_csv, args.csv_separator)
        return

    if args.n is None or args.procent is None:
        parser.error("--n i --procent są wymagane, chyba że użyto --wszystkie")

    liczba_zadan = args.n // 2
    sciezka_wyjsciowa = args.out or f"Zadania_{liczba_zadan}_{args.procent}.c"
    zadania = wygeneruj_c(args.n, args.procent, sciezka_wyjsciowa, rng, args.przyspieszenie)

    if args.csv:
        wiersze_csv = [(args.procent, liczba_zadan, z["nazwa"], "Mono&Dual",
                        z["p_single"], z["p_dual"]) for z in zadania]
        zapisz_csv(args.csv, wiersze_csv, args.csv_separator)


if __name__ == "__main__":
    main()
