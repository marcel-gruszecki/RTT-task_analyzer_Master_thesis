# RTT Task Analyser

Code accompanying a master's thesis by Marcel Gruszecki at Adam Mickiewicz University in Poznan. It covers preemptive task scheduling on a dual core embedded system, where every task can run on one core or be parallelized across both. The goal is to minimize the schedule length, called the makespan. The problem is weakly NP hard. For a fixed split of tasks into single core and dual core ones, the optimal makespan follows from a simple formula, so the real decision is the execution mode of each task.

## Algorithms

Implemented in `heuristics.rs`.

| Algorithm | Type | Complexity | Idea |
|---|---|---|---|
| `GreedySpdp` | heuristic | O(n³) | Starts with every task on one core. Each step moves the task that reduces the makespan the most to dual core mode, and stops when no move helps. |
| `SpeedupSplit` | heuristic | O(n³) | Ranks tasks by half of the single core time minus the dual core time. Moves them while it pays off and repeats passes until nothing changes. |
| `DuLeung1989` | exact | O(n * 2ⁿ) | Checks all 2ⁿ splits using bit masks. Practical up to about 30 tasks. |

The remaining tasks are placed with McNaughton's rule.

## Repository contents

| File | Purpose |
|---|---|
| `generator.py` | Generates test scenarios as FreeRTOS programs in C and can also write the task table as CSV. |
| `generowanie_tabeli.py` | Builds `struktura_zadan_weryfikacja.csv` from the generated `.c` files. |
| `main.rs` | The `sched` program. Loads the CSV, runs the algorithms and saves the results. |
| `heuristics.rs` | The scheduling algorithms, loaded as a module by `main.rs`. |

## Quick start

You need Rust (`rustc`) and Python 3. Cargo is not required.

```bash
# 1. Generate all scenarios plus the task table
python3 generator.py --wszystkie --katalog Scenariusze --seed 42 --csv struktura_zadan_weryfikacja.csv

# 2. Build the analyser, heuristics.rs must be next to main.rs
rustc -O main.rs -o sched

# 3. Run
./sched
```

The table can also be rebuilt from existing `.c` files with `python3 generowanie_tabeli.py`. It reads the `./Scenariusze` folder.

## Generator

Instances follow a cascade model. A few dominant tasks each take 55 to 80% of the budget remaining at their step. This forces idle time in McNaughton's rule, so the lower bound cannot be reached and the algorithms can be told apart. Instances with only moderately different task times are easy and are solved optimally in linear time.

Main options:

- `--n`: total number of tasks. Allowed values are 12, 24, 36, 48, 60, 72, 84, 96 and 200. Half are mono and half are dual, so the scheduling instance has n/2 tasks.
- `--procent`: share of superlinear tasks. Allowed values are 0, 10, 25 and 40.
- `--wszystkie` and `--katalog`: generate every combination into a folder.
- `--seed`: makes results reproducible.
- `--csv` and `--csv-separator`: also write the task table.
- `--przyspieszenie` and `--srednia-dlugosc`: how superlinear tasks are chosen and the mean task length in microseconds.

Files are named `Zadania_<tasks>_<percent>.c`.

## Analyser

```bash
./sched --scenario 10 --tasks 18      # a single instance
./sched --algo speedup --gantt        # one algorithm plus the schedule
./sched --max-exact 25 --verbose      # lower limit for the exact algorithm, diagnostics
./sched --file other.csv --out results.csv
```

Options: `--file`, `--scenario`, `--tasks`, `--algo greedy|speedup|duleung|all`, `--gantt`, `--verbose`, `--out`, `--max-exact`, `-h`.

The program prints a summary and writes `wyniki_czasowe.csv` with the columns Scenariusz, Ilosc zadan, Algorytm, Cmax and algorithm running time in nanoseconds. The improvement is measured against a baseline where all tasks run one after another without parallelization.

### Input format

A CSV file with this header:

```
Scenariusz,Ilosc zadan,Nazwa zadania,Tryb,p_single (us),p_dual (us)
```

The `Tryb` column is `Mono&Dual`, `Mono` or `Dual`. A missing time means the mode is unavailable. A `Mono` task always runs on one core and a `Dual` task always on both. The column separator and decimal separator are set by the constants `SEPARATOR` and `DECIMAL_SEPARATOR` at the top of `main.rs`. The default is a comma and a dot. The table script uses `CSV_SEPARATOR` and the two must match.

## Main results

Based on 36 synthetic instances with 6 to 100 tasks:

- The heuristics are on average 0.68 percentage points worse than the exact algorithm, at most 1.36, and they achieve at least 96% of its improvement.
- Quality does not drop as instances grow. The gap to the lower bound is 10.90% for large instances and 10.61% for small ones.
- Deliberate parallelization shortens the schedule by 12 to 25% on average compared with no parallelization.

## Limitations

The instances are synthetic and were not measured on the target ESP32-P4 microcontroller. Results therefore ignore context switching, core synchronization and interrupt overhead. The lower bound is not tight, so for more than 30 tasks heuristic quality can only be judged indirectly.
