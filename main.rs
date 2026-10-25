//! Project: RTT-Task Analyser
//! Author: Marcel Gruszecki
//! Description: runs the GreedySpdp and SpeedupSplit heuristics and the exact DuLeung1989
//!       algorithm on task times loaded from `struktura_zadan_weryfikacja.csv`.
//!
//! Build and run (no Cargo needed; `heuristics.rs` must be in the same directory):
//! ```text
//! rustc -O main.rs -o sched && ./sched
//! ```
//!
//! Usage examples:
//! ```text
//! ./sched                                  # all scenarios, all algorithms
//! ./sched --scenario 10 --tasks 18         # a single instance
//! ./sched --algo speedup --gantt           # SpeedupSplit only + Gantt chart
//! ./sched --out other_file.csv             # custom output file name
//! ```
//!
//! Timing results (Cmax and algorithm running time) are always written to a CSV file,
//! `wyniki_czasowe.csv` by default.

use std::collections::BTreeMap;
use std::fmt::Write as _;
use std::path::PathBuf;

// ===== Input file settings — edit manually if needed =====

/// Column separator of the task structure file: `,`, `;` or `'\t'` (tab).
const SEPARATOR: char = ',';

/// Decimal separator used in the time columns: `.` or `,`.
const DECIMAL_SEPARATOR: char = '.';

// ============================================================================

// The `heuristics` module expects the `super::scheduler::TaskExecution` type, so a minimal
// stand-in for that part of the original application's module hierarchy is recreated here.
mod scheduler {
    /// A single recorded task execution (equivalent of a microcontroller measurement).
    #[derive(Debug, Clone)]
    pub struct TaskExecution {
        pub name: String,
        pub start_us: u64,
        pub end_us: u64,
    }

    impl TaskExecution {
        pub fn duration_us(&self) -> u64 {
            self.end_us.saturating_sub(self.start_us)
        }
    }
}

#[path = "heuristics.rs"]
mod heuristics;

use heuristics::SchedulingResult;
use scheduler::TaskExecution;

/// Mode in which a task was measured and in which it may be scheduled.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Mode {
    /// The task has both single-core and dual-core times — it may go to $S_1$ or $S_2$.
    MonoAndDual,
    /// Single-core only — must go to $S_1$.
    MonoOnly,
    /// Dual-core only — must go to $S_2$.
    DualOnly,
}

impl Mode {
    fn label(self) -> &'static str {
        match self {
            Mode::MonoAndDual => "Mono&Dual",
            Mode::MonoOnly => "Mono",
            Mode::DualOnly => "Dual",
        }
    }

    /// Number of times the task appears in the pool after `heuristics::aggregate_tasks`.
    /// The aggregator inserts every task exactly once, so this is always 1.
    fn multiplicity(self) -> usize {
        let _ = self;
        1
    }
}

/// A row of the task structure file.
#[derive(Debug, Clone)]
struct TaskRow {
    scenario: i64,
    task_count: usize,
    name: String,
    mode: Mode,
    p_single: f64,
    p_dual: f64,
}

/// Run configuration parsed from command-line arguments.
struct Config {
    file: PathBuf,
    scenario: Option<i64>,
    tasks: Option<usize>,
    algos: Vec<usize>,
    gantt: bool,
    verbose: bool,
    out: PathBuf,
    max_exact: usize,
}

const ALGO_NAMES: [&str; 3] = ["GreedySpdp", "SpeedupSplit", "DuLeung1989"];

fn main() {
    let cfg = match parse_args() {
        Ok(cfg) => cfg,
        Err(msg) => {
            eprintln!("Błąd argumentów: {msg}\n");
            eprintln!("{}", usage());
            std::process::exit(2);
        }
    };

    heuristics::VERBOSE.store(cfg.verbose, std::sync::atomic::Ordering::Relaxed);

    let rows = match load_rows(&cfg.file) {
        Ok(rows) => rows,
        Err(msg) => {
            eprintln!("Nie udało się wczytać pliku {}: {msg}", cfg.file.display());
            std::process::exit(1);
        }
    };

    // Group rows into instances (scenario, task count); BTreeMap keeps the order deterministic.
    let mut instances: BTreeMap<(i64, usize), Vec<TaskRow>> = BTreeMap::new();
    for row in rows {
        if cfg.scenario.is_some_and(|s| s != row.scenario) {
            continue;
        }
        if cfg.tasks.is_some_and(|n| n != row.task_count) {
            continue;
        }
        instances
            .entry((row.scenario, row.task_count))
            .or_default()
            .push(row);
    }

    if instances.is_empty() {
        eprintln!("Brak zadań spełniających podane filtry.");
        std::process::exit(1);
    }

    let mut summary: Vec<SummaryLine> = Vec::new();

    for ((scenario, task_count), rows) in &instances {
        let execs = build_executions(rows);
        let pool_size: usize = rows.iter().map(|r| r.mode.multiplicity()).sum();
        let baseline = sequential_baseline(rows);

        println!("\n################################################################");
        println!(
            "# Scenariusz {scenario}, deklarowana liczba zadań {task_count} \
             (wierszy: {}, zadań w puli: {pool_size})",
            rows.len()
        );
        println!(
            "# Tryby: Mono&Dual={}, Mono={}, Dual={}",
            rows.iter().filter(|r| r.mode == Mode::MonoAndDual).count(),
            rows.iter().filter(|r| r.mode == Mode::MonoOnly).count(),
            rows.iter().filter(|r| r.mode == Mode::DualOnly).count(),
        );
        println!("# Baseline sekwencyjny: {baseline:.1} us");
        println!("################################################################");

        if cfg.gantt {
            println!("# Wczytane zadania:");
            for row in rows {
                println!(
                    "#   {:<10} {:<10} p_single={:>10.1} p_dual={:>10.1} x{}",
                    row.name,
                    row.mode.label(),
                    row.p_single,
                    row.p_dual,
                    row.mode.multiplicity()
                );
            }
        }

        for &algo in &cfg.algos {
            if algo == 2 && pool_size > cfg.max_exact {
                println!(
                    "\n--- {} pominięty: {pool_size} zadań > limit {} ---",
                    ALGO_NAMES[algo], cfg.max_exact
                );
                summary.push(SummaryLine {
                    scenario: *scenario,
                    task_count: *task_count,
                    pool_size,
                    algorithm: ALGO_NAMES[algo],
                    result: None,
                });
                continue;
            }

            println!("\n--- {} ---", ALGO_NAMES[algo]);
            let mut result = heuristics::run_one(&execs, algo);

            // `heuristics::baseline_cmax` relies on measurement timestamps; for data loaded
            // from a file the baseline is computed directly from the task times.
            result.baseline_us = baseline;
            result.improvement = if baseline > 0.0 {
                (baseline - result.cmax_us) / baseline
            } else {
                0.0
            };

            println!(
                "  Cmax = {:.1} us | baseline = {:.1} us | poprawa = {:.2}% | czas algorytmu = {} ns",
                result.cmax_us,
                result.baseline_us,
                result.improvement * 100.0,
                result.algo_time_ns
            );

            if cfg.gantt {
                print_gantt(&result);
            }

            summary.push(SummaryLine {
                scenario: *scenario,
                task_count: *task_count,
                pool_size,
                algorithm: ALGO_NAMES[algo],
                result: Some((
                    result.cmax_us,
                    result.baseline_us,
                    result.improvement,
                    result.algo_time_ns,
                )),
            });
        }
    }

    print_summary(&summary);

    match write_csv(&cfg.out, &summary) {
        Ok(()) => println!("\nWyniki zapisane do {}", cfg.out.display()),
        Err(msg) => {
            eprintln!("\nNie udało się zapisać {}: {msg}", cfg.out.display());
            std::process::exit(1);
        }
    }
}

struct SummaryLine {
    scenario: i64,
    task_count: usize,
    pool_size: usize,
    algorithm: &'static str,
    /// `None` if the algorithm was skipped (instance too large for exhaustive search).
    result: Option<(f64, f64, f64, u128)>,
}

// ---------------------------------------------------------------------------
// Loading the task structure file
// ---------------------------------------------------------------------------

/// Parses the task structure file (separator from [`SEPARATOR`]) with columns:
/// `Scenariusz | Ilosc zadan | Nazwa zadania | Tryb | p_single (us) | p_dual (us)`.
fn load_rows(path: &PathBuf) -> Result<Vec<TaskRow>, String> {
    let raw = std::fs::read_to_string(path).map_err(|e| e.to_string())?;
    let raw = raw.trim_start_matches('\u{feff}');

    let mut rows = Vec::new();

    for (lineno, line) in raw.lines().enumerate() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }

        let cols: Vec<&str> = line.split(SEPARATOR).map(|c| c.trim()).collect();

        // The header row is detected by its first column not being a number.
        if cols[0].parse::<i64>().is_err() {
            continue;
        }
        if cols.len() < 5 {
            return Err(format!("wiersz {}: zbyt mało kolumn", lineno + 1));
        }

        let scenario: i64 = cols[0]
            .parse()
            .map_err(|_| format!("wiersz {}: zły numer scenariusza", lineno + 1))?;
        let task_count: usize = cols[1]
            .parse()
            .map_err(|_| format!("wiersz {}: zła liczba zadań", lineno + 1))?;
        let name = cols[2].to_string();

        let p_single = parse_time(cols.get(4).copied().unwrap_or(""));
        let p_dual = parse_time(cols.get(5).copied().unwrap_or(""));

        let mode = resolve_mode(cols[3], p_single, p_dual)
            .map_err(|e| format!("wiersz {}: {e}", lineno + 1))?;

        // Times not used in the given mode are zeroed so that they do not affect the baseline.
        let (p_single, p_dual) = match mode {
            Mode::MonoAndDual => (p_single.unwrap(), p_dual.unwrap()),
            Mode::MonoOnly => (p_single.unwrap(), 0.0),
            Mode::DualOnly => (0.0, p_dual.unwrap()),
        };

        rows.push(TaskRow {
            scenario,
            task_count,
            name,
            mode,
            p_single,
            p_dual,
        });
    }

    if rows.is_empty() {
        return Err("plik nie zawiera wierszy z danymi".into());
    }
    Ok(rows)
}

/// Converts a time field to a number, honouring [`DECIMAL_SEPARATOR`].
/// An empty field, `-`, `NA`, `inf` or a non-positive value means "no measurement".
fn parse_time(field: &str) -> Option<f64> {
    let f = field.trim();
    if f.is_empty() || f == "-" {
        return None;
    }
    let lower = f.to_ascii_lowercase();
    if lower == "na" || lower == "n/a" || lower == "nan" || lower == "inf" || lower == "brak" {
        return None;
    }
    let v: f64 = f.replace(DECIMAL_SEPARATOR, ".").parse().ok()?;
    if v.is_finite() && v > 0.0 {
        Some(v)
    } else {
        None
    }
}

/// Determines the task mode from the `Tryb` (mode) column, checked against the available times.
///
/// The label in the file takes precedence, but a missing time for the declared mode is an error;
/// if the label is empty or unknown, the mode is inferred solely from which times are present.
fn resolve_mode(label: &str, p_single: Option<f64>, p_dual: Option<f64>) -> Result<Mode, String> {
    let l = label.to_ascii_lowercase();
    let says_mono = l.contains("mono") || l.contains("single") || l.contains("jedno");
    let says_dual = l.contains("dual") || l.contains("par") || l.contains("dwu");

    let declared = match (says_mono, says_dual) {
        (true, true) => Some(Mode::MonoAndDual),
        (true, false) => Some(Mode::MonoOnly),
        (false, true) => Some(Mode::DualOnly),
        (false, false) => None,
    };

    let inferred = match (p_single, p_dual) {
        (Some(_), Some(_)) => Some(Mode::MonoAndDual),
        (Some(_), None) => Some(Mode::MonoOnly),
        (None, Some(_)) => Some(Mode::DualOnly),
        (None, None) => None,
    };

    match (declared, inferred) {
        (None, None) => Err("brak trybu i brak jakiegokolwiek czasu".into()),
        (None, Some(m)) => Ok(m),
        (Some(Mode::MonoAndDual), _) => {
            if p_single.is_some() && p_dual.is_some() {
                Ok(Mode::MonoAndDual)
            } else {
                Err("tryb Mono&Dual, ale brakuje jednego z czasów".into())
            }
        }
        (Some(Mode::MonoOnly), _) => {
            if p_single.is_some() {
                Ok(Mode::MonoOnly)
            } else {
                Err("tryb Mono, ale brak p_single".into())
            }
        }
        (Some(Mode::DualOnly), _) => {
            if p_dual.is_some() {
                Ok(Mode::DualOnly)
            } else {
                Err("tryb Dual, ale brak p_dual".into())
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Building synthetic measurement traces
// ---------------------------------------------------------------------------

/// Recreates the execution trace expected by `heuristics::aggregate_tasks`.
///
/// The aggregator ignores the first occurrence of each name (start-up artefacts) and recognises
/// the dual-core mode by the `_0_d` / `_1_d` suffixes, so every task is emitted twice and
/// dual-core times are reported separately for core 0 and core 1. `Mono` tasks get no `_*_d`
/// entries (giving `p_dual = INF`, i.e. forced into $S_1$), and `Dual` tasks get no single-core
/// entry (`p_single = INF`, forced into $S_2$).
fn build_executions(rows: &[TaskRow]) -> Vec<TaskExecution> {
    let mut execs = Vec::new();
    let mut t: u64 = 0;

    let push = |execs: &mut Vec<TaskExecution>, t: &mut u64, name: String, dur: f64| {
        let d = dur.round().max(1.0) as u64;
        execs.push(TaskExecution {
            name,
            start_us: *t,
            end_us: *t + d,
        });
        *t += d;
    };

    // Two identical passes: the first one is discarded by the aggregator.
    for _pass in 0..2 {
        for row in rows {
            match row.mode {
                Mode::MonoAndDual => {
                    push(&mut execs, &mut t, row.name.clone(), row.p_single);
                    push(&mut execs, &mut t, format!("{}_0_d", row.name), row.p_dual);
                    push(&mut execs, &mut t, format!("{}_1_d", row.name), row.p_dual);
                }
                Mode::MonoOnly => {
                    push(&mut execs, &mut t, row.name.clone(), row.p_single);
                }
                Mode::DualOnly => {
                    push(&mut execs, &mut t, format!("{}_0_d", row.name), row.p_dual);
                    push(&mut execs, &mut t, format!("{}_1_d", row.name), row.p_dual);
                }
            }
        }
    }

    execs
}

/// Baseline makespan: the whole pool executed one task after another, each in its own mode
/// (`Dual` tasks occupy both cores for `p_dual`, the others one core for `p_single`).
fn sequential_baseline(rows: &[TaskRow]) -> f64 {
    rows.iter()
        .map(|r| {
            let per_copy = match r.mode {
                Mode::MonoAndDual | Mode::MonoOnly => r.p_single,
                Mode::DualOnly => r.p_dual,
            };
            per_copy * r.mode.multiplicity() as f64
        })
        .sum()
}

// ---------------------------------------------------------------------------
// Presenting results
// ---------------------------------------------------------------------------

fn print_gantt(result: &SchedulingResult) {
    let mut segs: Vec<_> = result.segments.iter().collect();
    segs.sort_by(|a, b| {
        a.core
            .cmp(&b.core)
            .then(a.start_us.partial_cmp(&b.start_us).unwrap())
    });

    println!("  Uszeregowanie (rdzeń: start -> koniec, zadanie):");
    for s in segs {
        println!(
            "    C{}: {:>12.1} -> {:>12.1}  {}",
            s.core, s.start_us, s.end_us, s.name
        );
    }
}

fn print_summary(summary: &[SummaryLine]) {
    println!("\n\n================================ PODSUMOWANIE ================================");
    println!(
        "{:>4} {:>6} {:>6} {:>14} {:>14} {:>14} {:>10} {:>12}",
        "Scen", "Zadan", "Pula", "Algorytm", "Cmax [us]", "Baseline [us]", "Poprawa", "Czas [ns]"
    );
    println!("{}", "-".repeat(90));

    for line in summary {
        match line.result {
            Some((cmax, baseline, improvement, ns)) => println!(
                "{:>4} {:>6} {:>6} {:>14} {:>14.1} {:>14.1} {:>9.2}% {:>12}",
                line.scenario,
                line.task_count,
                line.pool_size,
                line.algorithm,
                cmax,
                baseline,
                improvement * 100.0,
                ns
            ),
            None => println!(
                "{:>4} {:>6} {:>6} {:>14} {:>14} {:>14} {:>10} {:>12}",
                line.scenario,
                line.task_count,
                line.pool_size,
                line.algorithm,
                "-",
                "-",
                "pominiety",
                "-"
            ),
        }
    }
    println!("{}", "-".repeat(90));
}

/// Writes timing results: the schedule makespan and the running time of the algorithm itself.
fn write_csv(path: &PathBuf, summary: &[SummaryLine]) -> Result<(), String> {
    let mut out = String::new();
    out.push_str("Scenariusz;Ilosc zadan;Algorytm;Cmax (us);Czas algorytmu (ns)\n");
    for line in summary {
        match line.result {
            Some((cmax, _, _, ns)) => {
                let _ = writeln!(
                    out,
                    "{};{};{};{:.1};{}",
                    line.scenario, line.task_count, line.algorithm, cmax, ns
                );
            }
            None => {
                let _ = writeln!(
                    out,
                    "{};{};{};;",
                    line.scenario, line.task_count, line.algorithm
                );
            }
        }
    }
    std::fs::write(path, out).map_err(|e| e.to_string())
}

// ---------------------------------------------------------------------------
// Command-line arguments
// ---------------------------------------------------------------------------

fn usage() -> String {
    "Użycie: sched [opcje]\n\
     \n\
     Opcje:\n\
     \x20 --file <sciezka>     plik struktury zadań (domyślnie struktura_zadan_weryfikacja.csv)\n\
     \x20 --scenario <n>       ogranicz do jednego scenariusza\n\
     \x20 --tasks <n>          ogranicz do instancji o danej liczbie zadań\n\
     \x20 --algo <nazwa>       greedy | speedup | duleung | all (domyślnie all)\n\
     \x20 --gantt              wypisz segmenty uszeregowania\n\
     \x20 --verbose            wypisz diagnostykę algorytmów (pula zadań, podział S1/S2)\n\
     \x20 --out <plik.csv>     plik wyników czasowych (domyślnie wyniki_czasowe.csv)\n\
     \x20 --max-exact <n>      górny limit liczby zadań dla algorytmu dokładnego (domyślnie 29)\n\
     \x20 -h, --help           ta pomoc"
        .to_string()
}

fn parse_args() -> Result<Config, String> {
    let mut cfg = Config {
        file: PathBuf::from("struktura_zadan_weryfikacja.csv"),
        scenario: None,
        tasks: None,
        algos: vec![0, 1, 2],
        gantt: false,
        verbose: false,
        out: PathBuf::from("wyniki_czasowe.csv"),
        max_exact: 31,
    };

    let args: Vec<String> = std::env::args().skip(1).collect();
    let mut i = 0;

    while i < args.len() {
        let arg = args[i].as_str();
        let mut next = |what: &str| -> Result<String, String> {
            i += 1;
            args.get(i)
                .cloned()
                .ok_or_else(|| format!("{what} wymaga wartości"))
        };

        match arg {
            "-h" | "--help" => {
                println!("{}", usage());
                std::process::exit(0);
            }
            "--file" => cfg.file = PathBuf::from(next("--file")?),
            "--scenario" => {
                let v = next("--scenario")?;
                cfg.scenario = Some(v.parse().map_err(|_| "--scenario: nie liczba".to_string())?);
            }
            "--tasks" => {
                let v = next("--tasks")?;
                cfg.tasks = Some(v.parse().map_err(|_| "--tasks: nie liczba".to_string())?);
            }
            "--max-exact" => {
                let v = next("--max-exact")?;
                cfg.max_exact = v.parse().map_err(|_| "--max-exact: nie liczba".to_string())?;
            }
            "--out" => cfg.out = PathBuf::from(next("--out")?),
            "--gantt" => cfg.gantt = true,
            "--verbose" | "-v" => cfg.verbose = true,
            "--algo" => {
                let v = next("--algo")?.to_ascii_lowercase();
                cfg.algos = match v.as_str() {
                    "all" | "wszystkie" => vec![0, 1, 2],
                    "greedy" | "greedyspdp" | "0" => vec![0],
                    "speedup" | "speedupsplit" | "1" => vec![1],
                    "duleung" | "exact" | "dokladny" | "2" => vec![2],
                    other => return Err(format!("nieznany algorytm: {other}")),
                };
            }
            other => return Err(format!("nieznana opcja: {other}")),
        }
        i += 1;
    }

    Ok(cfg)
}
