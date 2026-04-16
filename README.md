# Simple Cache Controller FSM Simulator (C++)

This project simulates a blocking cache controller finite state machine (FSM) using C++. It models:

- A simple CPU that issues read/write requests from a queue
- A direct-mapped cache with write-back and write-allocate policy
- A simple main memory that never misses and takes 2 cycles to read or write blocks

## FSM States (22 states, matching fsm_diagram.md)

Note: The diagram in `fsm_diagram.md` uses Mermaid. If your Markdown preview does not render Mermaid diagrams, install a Mermaid-capable preview extension (for example, Markdown Preview Mermaid Support in VS Code).

```
IDLE → COMPARE_TAG → READ_HIT / WRITE_HIT / MISS
```

- **Hit path (read):** `COMPARE_TAG → READ_HIT → DONE`
- **Hit path (write):** `COMPARE_TAG → WRITE_HIT → UPDATE_CACHE → SET_DIRTY → DONE`
- **Miss path (clean):** `MISS → CHECK_DIRTY → ALLOCATE → MEM_READ → MEM_WAIT_READ_1 → MEM_WAIT_READ_2 → UPDATE_CACHE_BLOCK → UPDATE_TAG → SET_VALID → CHECK_WRITE_AFTER_ALLOC → DONE`
- **Miss path (dirty writeback):** adds `WRITE_BACK → MEM_WRITE → MEM_WAIT_WRITE_1 → MEM_WAIT_WRITE_2` before ALLOCATE
- **Write-miss post-alloc:** `CHECK_WRITE_AFTER_ALLOC → WRITE_AFTER_ALLOC → UPDATE_CACHE → SET_DIRTY → DONE`

## Interface Signals

Logged cycle by cycle:

- **CPU side:** `cpu_req_ready`, `cpu_stall`, `data_out_valid`, `data_out`
- **Cache status:** `cache_hit`, `cache_miss`
- **Memory side:** `mem_rd`, `mem_wr`, `mem_ready`, `mem_data_valid`

## Repository Structure

```
cache_sim.cpp              Main simulator source (single file)
fsm_diagram.md             FSM state diagram (Mermaid)
inputs/                    Workload input files
  workload_full.txt          Full coverage workload
  workload_hits.txt          Hit-focused workload
  workload_writeback.txt     Writeback-focused workload
  workload_conflict.txt      Conflict-miss workload (uses all cache lines)
outputs/reference/         Pre-generated reference outputs
docs/report_outline.md     Report writing checklist
Makefile                   Build and run commands
```

## Build

### Linux / macOS

```bash
make
```

### Windows (MinGW)

**🚨 IMPORTANT FOR WINDOWS USERS:** If you are using MinGW, the standard `make` command will usually result in an error like `'make' is not recognized`.
MinGW installs it as `mingw32-make` instead.

**You must use `mingw32-make` instead of `make` for all commands in this README.**

```powershell
mingw32-make
```

### Manual build (any platform)

```bash
g++ -std=c++17 -Wall -Wextra -O2 cache_sim.cpp -o cache_sim
```

## Run

### Default run (built-in request queue)

```bash
./cache_sim
```

### Run with custom input

```bash
./cache_sim --input inputs/workload_full.txt --trace outputs/trace.csv --summary outputs/summary.txt
```

### Run all predefined workloads

```bash
make run-all
# Windows users: mingw32-make run-all
```

Or individually:

```bash
make run-full       # Windows: mingw32-make run-full
make run-hits       # Windows: mingw32-make run-hits
make run-writeback  # Windows: mingw32-make run-writeback
make run-conflict   # Windows: mingw32-make run-conflict
```

## Input File Format

Each non-comment line is one request:

- Read: `R <address>`
- Write: `W <address> <data>`

```text
# Comments start with #
R 0x00
W 0x08 999
R 12
```

Both hexadecimal (`0x...`) and decimal addresses are accepted.

## Command-Line Options

```
--input <path>       Input request file
--trace <path>       Trace CSV output (default: outputs/trace.csv)
--summary <path>     Summary text output (default: outputs/summary.txt)
--cache-lines <N>    Number of cache lines (default: 4)
--help               Show usage
```

## Output

- **Console:** Formatted cycle-by-cycle table showing state and all signal values
- **Trace CSV:** Machine-readable cycle trace for analysis
- **Summary TXT:** Request statistics (hits, misses, writebacks, hit rate) and final cache state

## Assumptions and Simplifications

- **Direct-mapped cache** with word-level addressing (`index = addr % num_lines`, `tag = addr / num_lines`)
- **Single-word cache lines** (each line holds one data word)
- **Blocking cache** (CPU stalls until the current request completes)
- **Write-back, write-allocate** policy
- **Memory never misses** and has a fixed 2-cycle latency

## Reproducibility

Generate all reference artifacts:

```bash
make run-all
# Windows users: mingw32-make run-all
```

This produces trace CSVs and summary files in `outputs/reference/` for all 4 workloads
