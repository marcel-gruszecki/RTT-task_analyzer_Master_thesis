"""Builds the task structure table (`struktura_zadan_weryfikacja.csv`) from .c scenario files.

Author:  Marcel Gruszecki

"""

import os
import re

# CONFIG: path to the folder with the .c files
SRC_FOLDER = "./Scenariusze"

# Column separator of the output file — must match the SEPARATOR constant
# expected by `sched` (main.rs).
CSV_SEPARATOR = ","

# Marker in the .c file header indicating that the execute_dual_task()
# argument is the execution time rather than the total work of both cores.
MARKER_FORMAT_CZASU = "FORMAT_CZASU_DUAL: czas"


def get_base_name(task_name):
    """
    Extracts the base task name.
    Strips suffixes such as _0_d, _1_d, _d, _0, _1.
    E.g. "fx1_0_d" -> "fx1", "fx10_1_d" -> "fx10", "fx5_0" -> "fx5"
    """
    name = re.sub(r'_[01]_d$', '', task_name)
    name = re.sub(r'_d$', '', name)
    name = re.sub(r'_[01]$', '', name)
    return name

def parse_c_file(file_path):
    """Parses a .c file and merges tasks sharing the same base name."""
    filename = os.path.basename(file_path)
    
    # File name format: Zadania_<task_count>_<scenario>
    match = re.search(r"Zadania_(\d+)_(\d+)", filename, re.IGNORECASE)
    if not match:
        return None, None, []

    ilosc_zadan = int(match.group(1)) # first number: task count
    scenariusz = int(match.group(2))  # second number: scenario (% of superlinear tasks)

    grouped_tasks = {}

    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()

        # Convention of the third execute_dual_task() argument: time or work?
        dual_to_czas = MARKER_FORMAT_CZASU in content

        # 1a. Extract Mono tasks - NEW FORMAT (array of structs)
        mono_block_match = re.search(r'mono_tasks\[\]\s*=\s*\{(.*?)\};', content, re.DOTALL)
        if mono_block_match:
            # Look for the { "name", time } pattern inside the array block
            mono_matches_new = re.findall(r'\{\s*"([^"]+)"\s*,\s*(\d+)\s*\}', mono_block_match.group(1))
            for name, duration in mono_matches_new:
                base_name = get_base_name(name)
                if base_name not in grouped_tasks:
                    grouped_tasks[base_name] = {"mono_us": "", "dual_us": ""}
                grouped_tasks[base_name]["mono_us"] = duration

        # 1b. Extract Mono tasks - OLD FORMAT (fallback for older files)
        mono_matches_old = re.findall(r'execute_mono_task\(\s*"([^"]+)"\s*,\s*(\d+)\s*\)', content)
        for name, duration in mono_matches_old:
            base_name = get_base_name(name)
            if base_name not in grouped_tasks:
                grouped_tasks[base_name] = {"mono_us": "", "dual_us": ""}
            grouped_tasks[base_name]["mono_us"] = duration

        # 2. Extract Dual tasks. In the old convention the argument is the work
        #    of both cores, so the task execution time is half of it.
        dual_matches = re.findall(r'execute_dual_task\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(\d+)\s*\)', content)
        for name_c0, name_c1, argument in dual_matches:
            base_name = get_base_name(name_c0)
            if base_name not in grouped_tasks:
                grouped_tasks[base_name] = {"mono_us": "", "dual_us": ""}
            wartosc = int(argument)
            czas = wartosc if dual_to_czas else wartosc / 2
            # Write integer values without ".0" to keep the table readable.
            grouped_tasks[base_name]["dual_us"] = (
                str(int(czas)) if float(czas).is_integer() else f"{czas:.1f}"
            )

    if dual_matches and not dual_to_czas:
        print(f"[UWAGA] {filename}: stara konwencja (argument execute_dual_task to "
              f"praca obu rdzeni) — kolumna p_dual dzielona przez 2.")

    file_tasks = []
    for base_name, info in grouped_tasks.items():
        mono_time = info["mono_us"]
        dual_time = info["dual_us"]

        if mono_time and dual_time:
            tryb = "Mono&Dual"
        elif dual_time:
            tryb = "Dual"
        else:
            tryb = "Mono"

        file_tasks.append({
            "scenariusz": scenariusz,
            "ilosc_zadan": ilosc_zadan,
            "nazwa": base_name,
            "tryb": tryb,
            "p_single": mono_time,
            "p_dual": dual_time
        })

    return scenariusz, ilosc_zadan, file_tasks

def main():
    if not os.path.exists(SRC_FOLDER):
        print(f"[BŁĄD] Folder {SRC_FOLDER} nie istnieje!")
        return

    raw_files = [f for f in os.listdir(SRC_FOLDER) if f.endswith(".c")]

    parsed_files_data = []
    for file in raw_files:
        file_path = os.path.join(SRC_FOLDER, file)
        scen, n, tasks = parse_c_file(file_path)
        if scen is not None:
            parsed_files_data.append((scen, n, tasks))

    # Numeric sort: scenario first, then task count (n)
    parsed_files_data.sort(key=lambda x: (x[0], x[1]))

    all_tasks = []
    for scen, n, tasks in parsed_files_data:
        all_tasks.extend(tasks)

    print("Scenariusz\tIlosc zadan\tNazwa zadania\tTryb\tp_single (us)\tp_dual (us)")
    for t in all_tasks:
        print(f"{t['scenariusz']}\t{t['ilosc_zadan']}\t{t['nazwa']}\t{t['tryb']}\t{t['p_single']}\t{t['p_dual']}")

    output_csv = "struktura_zadan_weryfikacja.csv"
    sep = CSV_SEPARATOR
    with open(output_csv, "w", encoding="utf-8") as out:
        out.write(sep.join(["Scenariusz", "Ilosc zadan", "Nazwa zadania", "Tryb",
                            "p_single (us)", "p_dual (us)"]) + "\n")
        for t in all_tasks:
            out.write(sep.join([str(t['scenariusz']), str(t['ilosc_zadan']),
                                t['nazwa'], t['tryb'],
                                str(t['p_single']), str(t['p_dual'])]) + "\n")

    print(f"\n[SUKCES] Dane posortowane numerycznie i nadpisane w pliku: {output_csv}")

if __name__ == "__main__":
    main()
