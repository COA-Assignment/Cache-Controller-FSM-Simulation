
---

&nbsp;

&nbsp;

&nbsp;

&nbsp;

&nbsp;

# Cache Controller FSM Simulation

**Computer Organization and Architecture (COA)**
**Assignment 2**

&nbsp;

| | |
|---|---|
| **Course** | Computer Organization and Architecture (COA) |
| **Assignment** | Assignment 2 — Finite State Machine Simulation |
| **Submission Date** | April 16, 2026 |
| **Repository** | https://github.com/AbdullahIbnYousuf/Cache-Controller-FSM-Simulation |

&nbsp;

> *Submitted in partial fulfilment of the requirements for the Computer Organization and Architecture course.*

---

&nbsp;

&nbsp;

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Background and Theory](#2-background-and-theory)
   - 2.1 [What is a Cache?](#21-what-is-a-cache)
   - 2.2 [Cache Policies](#22-cache-policies)
   - 2.3 [Finite State Machines in Cache Controllers](#23-finite-state-machines-in-cache-controllers)
3. [Simulator Setup and Architecture](#3-simulator-setup-and-architecture)
   - 3.1 [Technology Stack](#31-technology-stack)
   - 3.2 [Project Structure](#32-project-structure)
   - 3.3 [System Architecture](#33-system-architecture)
   - 3.4 [Data Structures](#34-data-structures)
   - 3.5 [Interface Signals](#35-interface-signals)
   - 3.6 [Build and Run Instructions](#36-build-and-run-instructions)
   - 3.7 [Input Format](#37-input-format)
4. [Finite State Machine Design](#4-finite-state-machine-design)
   - 4.1 [State List and Descriptions](#41-state-list-and-descriptions)
   - 4.2 [State Transition Diagram](#42-state-transition-diagram)
   - 4.3 [Signal Output per State (Moore Machine)](#43-signal-output-per-state-moore-machine)
   - 4.4 [State Paths — Read Hit, Write Hit, Miss](#44-state-paths)
   - 4.5 [Write-Back Sub-Path](#45-write-back-sub-path)
   - 4.6 [Write-Allocate Sub-Path](#46-write-allocate-sub-path)
5. [Experimental Workloads and Results](#5-experimental-workloads-and-results)
   - 5.1 [Workload 1: Full Functionality](#51-workload-1-full-functionality)
   - 5.2 [Workload 2: Hit-Focused](#52-workload-2-hit-focused)
   - 5.3 [Workload 3: Write-Back Stress](#53-workload-3-write-back-stress)
   - 5.4 [Workload 4: Conflict-Miss Sequence](#54-workload-4-conflict-miss-sequence)
   - 5.5 [Comparative Performance Summary](#55-comparative-performance-summary)
6. [Detailed Signal Analysis](#6-detailed-signal-analysis)
7. [Validation Against FSM Specification](#7-validation-against-fsm-specification)
8. [Limitations and Future Work](#8-limitations-and-future-work)
9. [Conclusion](#9-conclusion)
10. [Appendix](#10-appendix)
    - A. [Complete Source Code (cache_sim.cpp)](#appendix-a-complete-source-code)
    - B. [Full Trace — Workload 1](#appendix-b-full-trace-workload-1)
    - C. [FSM Mermaid Diagram Source](#appendix-c-fsm-mermaid-diagram-source)

---

## 1. Introduction

Modern processors are orders of magnitude faster than main memory (DRAM). This speed mismatch—known as the **memory wall**—makes it necessary to insert a small, fast memory hierarchy between the CPU and main memory. A **cache** serves exactly this role: it stores recently accessed memory blocks close to the processor so that future accesses can be served in one or two clock cycles rather than dozens.

Managing a cache is itself a non-trivial control problem. The hardware block that orchestrates every read and write request, decides whether a given memory access is a *hit* (data is already in cache) or a *miss* (data must be fetched), and enforces coherency policies is called the **cache controller**. Because its behaviour is deterministic and can be described as a sequence of discrete, well-defined phases (tag comparison, memory wait cycles, block installation, dirty eviction, etc.), it is naturally modelled as a **Finite State Machine (FSM)**.

This project implements a cycle-accurate, event-driven simulation of such a cache controller FSM in C++17. The simulator models:

- A **CPU** that issues read and write requests from a configurable queue.
- A **direct-mapped, write-back, write-allocate cache** with a configurable number of lines.
- A **main memory** that never misses and has a fixed **2-cycle latency** for both reads and writes.
- A **22-state FSM** that drives all cache operations and exposes all interface signals on every clock cycle.

For a specific and comprehensive set of input workloads, the report demonstrates: (1) how the FSM internal state changes cycle by cycle; (2) how those state changes assert or deassert every interface signal; and (3) the resulting performance metrics (hit rate, cycles, writebacks).

The simulator, source code, and all workload files are publicly available at:
**https://github.com/AbdullahIbnYousuf/Cache-Controller-FSM-Simulation**

---

## 2. Background and Theory

### 2.1 What is a Cache?

A cache is a small, fast memory placed between the CPU and main memory in a hierarchical storage system. It exploits two fundamental principles of program behaviour:

- **Temporal locality:** A recently accessed memory location is likely to be accessed again soon.
- **Spatial locality:** Memory locations near a recently accessed location are likely to be accessed soon.

When the CPU requests data, the cache controller first checks if the data is already stored in cache. If yes, a **cache hit** occurs and the data is returned quickly. If not, a **cache miss** occurs and the block must be fetched from main memory (at significantly higher latency).

### 2.2 Cache Policies

This simulator implements the following well-established cache policies:

#### Direct-Mapped Organisation
Each memory address maps to exactly one cache line, determined by:

```
index = address % num_cache_lines
tag   = address / num_cache_lines
```

A hit is confirmed when the cache line at the computed `index` is *valid* and its stored `tag` matches `tag_of(address)`.

#### Write-Back Policy
When the CPU writes data, the data is written only into the cache (not immediately to main memory). The cache line is marked as **dirty**. When a dirty line is evicted (because a new block needs to use its slot), its content is written back to main memory before the slot is reused.

#### Write-Allocate Policy
On a **write miss**, the target block is first fetched from memory into the cache (allocation), and then the write is applied to the newly allocated cache line. This ensures subsequent writes or reads to the same block will hit in cache.

### 2.3 Finite State Machines in Cache Controllers

A Finite State Machine consists of:

- A finite set of **states** *S*
- An **input alphabet** *Σ* (events that trigger transitions)
- A **transition function** *δ: S × Σ → S*
- An **output function** (Moore machine: outputs depend only on the current state)
- An **initial state** *s₀*

This simulator implements a **Moore-style FSM**: all interface signals (outputs) are determined solely by the current state, and transitions are triggered by request attributes (read vs. write, hit vs. miss, dirty vs. clean victim).

---

## 3. Simulator Setup and Architecture

### 3.1 Technology Stack

| Component | Technology |
|-----------|-----------|
| Language | C++17 (ISO/IEC 14882:2017) |
| Compiler | GCC / MinGW-w64 (g++ -std=c++17) |
| Build System | GNU Make / mingw32-make |
| Libraries | C++ Standard Library only (no external dependencies) |
| Output formats | Console (formatted table), CSV trace, TXT summary |

The choice of C++ provides direct, cycle-level control of state, bit-level data structures, and efficient simulation without abstraction overhead. The single-file approach (`cache_sim.cpp`) makes the project portable and easy to compile on any platform with a C++17-compliant compiler.

### 3.2 Project Structure

```
Cache-Controller-FSM-Simulation/
│
├── cache_sim.cpp              ← Main simulator source (single file, 616 lines)
├── fsm_diagram.md             ← FSM state diagram in Mermaid notation
├── Makefile                   ← Build and multi-workload run commands
├── README.md                  ← Full build/run/test documentation
│
├── inputs/
│   ├── workload_full.txt      ← Full-coverage workload (7 requests)
│   ├── workload_hits.txt      ← Hit-focused workload (6 requests)
│   ├── workload_writeback.txt ← Writeback-stress workload (6 requests)
│   └── workload_conflict.txt  ← Conflict-miss workload (11 requests)
│
├── outputs/
│   ├── trace.csv              ← Cycle trace (most recent run)
│   ├── summary.txt            ← Statistics summary (most recent run)
│   └── reference/             ← Pre-generated reference outputs for all 4 workloads
│       ├── full_trace.csv / full_summary.txt
│       ├── hits_trace.csv / hits_summary.txt
│       ├── writeback_trace.csv / writeback_summary.txt
│       └── conflict_trace.csv / conflict_summary.txt
│
└── docs/
    └── report_outline.md      ← Report planning notes
```

### 3.3 System Architecture

The simulation models three interacting components as illustrated below:

![System Architecture Diagram](C:\Users\My\.gemini\antigravity\brain\b025368a-5e18-424f-945b-11038b6e2b4a\cache_architecture_1776300316863.png)

**Component 1 — CPU (Simulated Request Driver)**
The CPU is modelled as a sequential queue of `Request` objects. On each clock cycle, if the cache controller is ready (in `IDLE` with no pending request), the next request is dequeued and handed to the controller. The CPU then waits (stalls) until the controller transitions back to a ready state.

**Component 2 — Cache Controller FSM**
This is the heart of the simulation. It is an instance of the `CacheController` class, which:
- Maintains the 22-state FSM as a `State` enum.
- Holds the cache array (`vector<CacheLine>`).
- Computes output signals (Moore-style) based on the current state.
- Executes transition logic and actions (reads/writes to cache and memory) based on the current state.
- Records a `TraceRow` every cycle for logging.

**Component 3 — Main Memory**
The `MainMemory` class is a simple array of 64 words, pre-initialised to the pattern `1000 + index * 10` (so address 0 holds 1000, address 1 holds 1010, etc.). It never misses, and all reads and writes are modelled as taking **2 clock cycles** via wait states in the FSM (not a real timer). It exposes `read_word()` and `write_word()` methods.

### 3.4 Data Structures

```cpp
// A single CPU request
struct Request {
    ReqType  type;   // Read or Write
    uint32_t addr;   // Target word address
    uint32_t data;   // Data to write (ignored for reads)
};

// A single cache line
struct CacheLine {
    bool     valid;  // Is this line populated?
    bool     dirty;  // Has it been written since last memory sync?
    uint32_t tag;    // Tag portion of the address
    uint32_t data;   // Cached data word
};

// All interface signals for one clock cycle
struct Signals {
    bool     cpu_req_ready;    // Controller is ready for a new request
    bool     cpu_stall;        // CPU must wait (operation in progress)
    bool     cache_hit;        // Tag matched a valid line
    bool     cache_miss;       // No valid tag match
    bool     mem_rd;           // Memory read request asserted
    bool     mem_wr;           // Memory write request asserted
    bool     mem_ready;        // Memory write acknowledged
    bool     mem_data_valid;   // Memory read data available
    bool     data_out_valid;   // Output data to CPU is valid
    uint32_t data_out;         // Data returned to CPU (for reads)
    string   note;             // Debug label (e.g., "data=1000")
};
```

### 3.5 Interface Signals

The cache controller exposes **10 interface signals** that are logged on every clock cycle. They are grouped by interface:

**CPU Interface (Controller → CPU)**

| Signal | Direction | Description |
|--------|-----------|-------------|
| `cpu_req_ready` | → CPU | High in IDLE when no request is pending; the CPU may submit the next request. |
| `cpu_stall` | → CPU | High during all processing states. CPU must pause and wait. |
| `data_out_valid` | → CPU | High when a read result is available (READ_HIT, DONE). |
| `data_out` | → CPU | The data word returned from a successful read. |

**Cache Status Flags**

| Signal | Description |
|--------|-------------|
| `cache_hit` | Asserted for one cycle (READ_HIT or WRITE_HIT) when tag matched. |
| `cache_miss` | Asserted for one cycle (MISS) when tag did not match. |

**Memory Interface (Controller → Memory)**

| Signal | Direction | Description |
|--------|-----------|-------------|
| `mem_rd` | → Memory | Asserted during MEM_READ and MEM_WAIT_READ_1 to initiate a block read. |
| `mem_wr` | → Memory | Asserted during MEM_WRITE and MEM_WAIT_WRITE_1 to initiate a writeback. |
| `mem_ready` | ← Memory | High in MEM_WAIT_WRITE_2; simulates memory acknowledging the write is done. |
| `mem_data_valid` | ← Memory | High in MEM_WAIT_READ_2; simulates memory providing the read data. |

### 3.6 Build and Run Instructions

**Prerequisites:** GCC with C++17 support (g++ ≥ 8.0). On Windows, install MinGW-w64.

**Step 1: Build**
```bash
# Linux / macOS
make

# Windows (MinGW) — use mingw32-make throughout
mingw32-make
```

This compiles `cache_sim.cpp` with the flags `-std=c++17 -Wall -Wextra -O2`.

Alternatively, compile manually on any platform:
```bash
g++ -std=c++17 -Wall -Wextra -O2 cache_sim.cpp -o cache_sim
```

**Step 2: Run all workloads**
```bash
make run-all          # Linux / macOS
mingw32-make run-all  # Windows
```

This runs all four workloads and writes their trace CSVs and summaries to `outputs/reference/`.

**Step 3: Run with a custom input file**
```bash
./cache_sim --input inputs/workload_full.txt \
            --trace outputs/trace.csv \
            --summary outputs/summary.txt \
            --cache-lines 4
```

### 3.7 Input Format

Input files are plain text files. Each non-blank, non-comment line is one request:

```
# This is a comment and is ignored
R 0x00          # Read from address 0x00
W 0x08 999      # Write value 999 to address 0x08
R 12            # Decimal addresses are also accepted
```

- `R <address>` — Read request
- `W <address> <data>` — Write request
- Addresses may be hexadecimal (`0x...`) or decimal.

---

## 4. Finite State Machine Design

### 4.1 State List and Descriptions

The FSM has **22 states**. Each state represents one phase of the cache controller's operation. A single clock cycle is spent in each state; the next state is determined at the end of that cycle.

| # | State Name | Role |
|---|-----------|------|
| 1 | `IDLE` | Waiting for a CPU request. Asserts `cpu_req_ready`. |
| 2 | `COMPARE_TAG` | Reads the cache line at the computed index; compares its tag with the request's tag; checks the valid bit. |
| 3 | `READ_HIT` | Tag matched, valid, read request. Returns cached data on `data_out`. Asserts `cache_hit`. |
| 4 | `WRITE_HIT` | Tag matched, valid, write request. Prepares to update the cached word. |
| 5 | `MISS` | No match or invalid. Asserts `cache_miss`. |
| 6 | `CHECK_DIRTY` | Inspects whether the victim line (to be evicted) is dirty. |
| 7 | `WRITE_BACK` | Initiates writeback of the dirty victim line. |
| 8 | `MEM_WRITE` | Drives `mem_wr` high; memory write cycle 1. |
| 9 | `MEM_WAIT_WRITE_1` | Memory write is in progress; continues `mem_wr`. |
| 10 | `MEM_WAIT_WRITE_2` | Memory acknowledges write; `mem_ready` goes high. |
| 11 | `ALLOCATE` | Prepares to fetch the new block from memory. |
| 12 | `MEM_READ` | Drives `mem_rd` high; memory read cycle 1. |
| 13 | `MEM_WAIT_READ_1` | Memory read in progress; continues `mem_rd`. |
| 14 | `MEM_WAIT_READ_2` | Memory provides data; `mem_data_valid` goes high. |
| 15 | `UPDATE_CACHE_BLOCK` | Stores the fetched memory word into the cache line's data field. |
| 16 | `UPDATE_TAG` | Updates the tag field of the cache line. |
| 17 | `SET_VALID` | Marks the cache line as valid and clears the dirty bit. |
| 18 | `CHECK_WRITE_AFTER_ALLOC` | Decides: was this a read miss (→ DONE) or write miss (→ WRITE_AFTER_ALLOC)? |
| 19 | `WRITE_AFTER_ALLOC` | Bridges to the shared write path for a post-allocation write. |
| 20 | `UPDATE_CACHE` | Writes the CPU's data word into the cache line. |
| 21 | `SET_DIRTY` | Sets the dirty bit to indicate this line diverges from memory. |
| 22 | `DONE` | Operation complete. Asserts `data_out_valid` for reads. Transitions back to `IDLE`. |

### 4.2 State Transition Diagram

The FSM diagram below illustrates all 22 states and their transitions, colour-coded by execution path:

![Cache Controller FSM Diagram](C:\Users\My\.gemini\antigravity\brain\b025368a-5e18-424f-945b-11038b6e2b4a\fsm_diagram_1776300106489.png)

*Figure 1: Full 22-state FSM for the cache controller. Green = read-hit path, Blue = write-hit path, Red = miss detection, Orange = dirty writeback sub-path, Purple = block allocation sub-path.*

The Mermaid source for this diagram is included in `fsm_diagram.md` within the repository. A text-based representation of all transitions is given in Section 4.4–4.6.

### 4.3 Signal Output per State (Moore Machine)

In a Moore machine, outputs depend **only** on the current state, not on the inputs. The table below shows which signals are asserted in each state:

| State | `cpu_req_ready` | `cpu_stall` | `cache_hit` | `cache_miss` | `mem_rd` | `mem_wr` | `mem_ready` | `mem_data_valid` | `data_out_valid` |
|-------|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| IDLE | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| COMPARE_TAG | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| READ_HIT | 0 | **1** | **1** | 0 | 0 | 0 | 0 | 0 | **1** |
| WRITE_HIT | 0 | **1** | **1** | 0 | 0 | 0 | 0 | 0 | 0 |
| MISS | 0 | **1** | 0 | **1** | 0 | 0 | 0 | 0 | 0 |
| CHECK_DIRTY | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| WRITE_BACK | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| MEM_WRITE | 0 | **1** | 0 | 0 | 0 | **1** | 0 | 0 | 0 |
| MEM_WAIT_WRITE_1 | 0 | **1** | 0 | 0 | 0 | **1** | 0 | 0 | 0 |
| MEM_WAIT_WRITE_2 | 0 | **1** | 0 | 0 | 0 | 0 | **1** | 0 | 0 |
| ALLOCATE | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| MEM_READ | 0 | **1** | 0 | 0 | **1** | 0 | 0 | 0 | 0 |
| MEM_WAIT_READ_1 | 0 | **1** | 0 | 0 | **1** | 0 | 0 | 0 | 0 |
| MEM_WAIT_READ_2 | 0 | **1** | 0 | 0 | 0 | 0 | 0 | **1** | 0 |
| UPDATE_CACHE_BLOCK | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| UPDATE_TAG | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| SET_VALID | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| CHECK_WRITE_AFTER_ALLOC | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| WRITE_AFTER_ALLOC | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| UPDATE_CACHE | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| SET_DIRTY | 0 | **1** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| DONE | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | **1** (read only) |

*Note: `cpu_req_ready` in IDLE is only asserted when no request is already pending.*

### 4.4 State Paths

The FSM has four distinct execution paths through its states:

#### Path 1 — Read Hit (3 cycles)

```
IDLE → COMPARE_TAG → READ_HIT → DONE → IDLE
```

Triggered when: valid bit is set AND stored tag matches the request tag AND request type is Read.

- **IDLE:** Accept request, move to COMPARE_TAG.
- **COMPARE_TAG:** Compute index and tag; confirm hit; go to READ_HIT.
- **READ_HIT:** Assert `cache_hit`, `data_out_valid`; place cached word onto `data_out`.
- **DONE:** CPU unstalls, data is valid; clear pending request, return to IDLE.

**Total latency: 3 active cycles + 1 IDLE cycle = 4 cycles per request** (counting the IDLE cycle where the request is accepted).

#### Path 2 — Write Hit (5 cycles)

```
IDLE → COMPARE_TAG → WRITE_HIT → UPDATE_CACHE → SET_DIRTY → DONE → IDLE
```

Triggered when: valid AND tag match AND request type is Write.

- **WRITE_HIT:** Assert `cache_hit`; queue write to cache.
- **UPDATE_CACHE:** Write the CPU's data word into `current_line->data`.
- **SET_DIRTY:** Set `current_line->dirty = true`.
- **DONE:** Unstall; return to IDLE.

**Total latency: 5 active cycles + 1 IDLE cycle = 6 cycles per request.**

#### Path 3 — Read Miss, Clean Victim (13 cycles)

```
IDLE → COMPARE_TAG → MISS → CHECK_DIRTY →
ALLOCATE → MEM_READ → MEM_WAIT_READ_1 → MEM_WAIT_READ_2 →
UPDATE_CACHE_BLOCK → UPDATE_TAG → SET_VALID →
CHECK_WRITE_AFTER_ALLOC → DONE → IDLE
```

Triggered when: miss (invalid or tag mismatch) AND victim is NOT dirty AND request type is Read.

- **CHECK_DIRTY:** Determines `victim_dirty_ = false`; skips write-back, goes to ALLOCATE.
- **MEM_READ / MEM_WAIT_READ_1 / MEM_WAIT_READ_2:** Simulates 2-cycle memory read latency.
- After installation: CHECK_WRITE_AFTER_ALLOC routes to DONE (since it is a read).
- **DONE:** Returns fetched data to CPU.

**Total latency: 13 active cycles + 1 IDLE.**

#### Path 4 — Write Miss, Clean Victim (15 cycles)

```
IDLE → COMPARE_TAG → MISS → CHECK_DIRTY →
ALLOCATE → MEM_READ → MEM_WAIT_READ_1 → MEM_WAIT_READ_2 →
UPDATE_CACHE_BLOCK → UPDATE_TAG → SET_VALID →
CHECK_WRITE_AFTER_ALLOC → WRITE_AFTER_ALLOC →
UPDATE_CACHE → SET_DIRTY → DONE → IDLE
```

After allocation, CHECK_WRITE_AFTER_ALLOC determines the original request was a write and routes through WRITE_AFTER_ALLOC → UPDATE_CACHE → SET_DIRTY before DONE.

**Total latency: 15 active cycles + 1 IDLE.**

### 4.5 Write-Back Sub-Path

When a miss occurs on a **dirty** victim line, the eviction path adds four states before ALLOCATE:

```
CHECK_DIRTY → WRITE_BACK → MEM_WRITE → MEM_WAIT_WRITE_1 → MEM_WAIT_WRITE_2 → ALLOCATE
```

| State | Key Action | Signals |
|-------|-----------|---------|
| WRITE_BACK | Start writeback; increment writeback counter | `cpu_stall=1` |
| MEM_WRITE | Initiate memory write of victim data | `mem_wr=1, cpu_stall=1` |
| MEM_WAIT_WRITE_1 | Cycle 1 of 2-cycle memory write | `mem_wr=1, cpu_stall=1` |
| MEM_WAIT_WRITE_2 | Write completes; actually call `mem.write_word()` | `mem_ready=1, cpu_stall=1` |

After MEM_WAIT_WRITE_2, the dirty victim has been committed to memory and the controller moves to ALLOCATE to fetch the new block.

**Total latency added by dirty miss over clean miss: +4 cycles (WRITE_BACK + MEM_WRITE + MEM_WAIT_WRITE_1 + MEM_WAIT_WRITE_2).**

A dirty read miss therefore takes **17 active cycles + 1 IDLE**, and a dirty write miss takes **19 active cycles + 1 IDLE**.

### 4.6 Write-Allocate Sub-Path

The write-allocate policy guarantees that a write miss always fetches the target block into cache before writing. This is implemented at the `CHECK_WRITE_AFTER_ALLOC` decision state:

```
SET_VALID → CHECK_WRITE_AFTER_ALLOC ──(write)──→ WRITE_AFTER_ALLOC → UPDATE_CACHE → SET_DIRTY → DONE
                                   └─(read) ──→ DONE
```

- If the original request was a **read miss**: route directly to DONE (data was already placed in cache by UPDATE_CACHE_BLOCK).
- If the original request was a **write miss**: route through WRITE_AFTER_ALLOC → UPDATE_CACHE → SET_DIRTY to apply the write.

This ensures the cache always holds a fully consistent view of the block after a write miss.

---

## 5. Experimental Workloads and Results

The simulator was validated against **four workloads** designed to exercise distinct cache behaviours. All experiments use **4 cache lines** and **2-cycle memory latency**.

### 5.1 Workload 1: Full Functionality

**Purpose:** Exercise all major FSM paths in a single workload — cold miss, read hit, write hit, dirty miss with writeback, write miss with allocate.

**Input sequence:**
```
R 0x00    # Cold read miss → allocate from memory
R 0x00    # Read hit (just allocated)
W 0x00 777 # Write hit → update cache, mark dirty
R 0x04    # Maps to line 0 → dirty miss → write back 0x00, then read 0x04
W 0x08 888 # Write miss → allocate 0x08, then write 888
W 0x08 999 # Write hit → overwrite with 999
R 0x0C    # Maps to line 0 → dirty miss → write back 0x08 (data=999), allocate 0x0C
```

**Address-to-Index Mapping (4 lines):**

| Address | Index | Tag |
|---------|-------|-----|
| 0x00 (0) | 0 | 0 |
| 0x04 (4) | 0 | 1 |
| 0x08 (8) | 0 | 2 |
| 0x0C (12) | 0 | 3 |

All four addresses map to **line 0**, which causes conflict-driven evictions.

**Results:**

| Metric | Value |
|--------|-------|
| Total Cycles | 79 |
| Total Requests | 7 |
| Reads | 4 |
| Writes | 3 |
| Hits | 3 |
| Misses | 4 |
| Writebacks | 2 |
| Hit Rate | **42.86%** |

**Final Cache State:**
```
Line 0: valid=1, dirty=0, tag=3, data=1120
Line 1: valid=0, dirty=0, tag=0, data=0
Line 2: valid=0, dirty=0, tag=0, data=0
Line 3: valid=0, dirty=0, tag=0, data=0
```

**Cycle-by-Cycle Trace Excerpt (first two requests):**

| Cycle | State | Request | Rdy | Stall | Hit | Miss | MRd | MWr | MRdy | MDVld | DOVld | DataOut | Note |
|-------|-------|---------|:---:|:-----:|:---:|:----:|:---:|:---:|:----:|:-----:|:-----:|---------|------|
| 1 | IDLE | READ 0x0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | |
| 2 | COMPARE_TAG | READ 0x0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | |
| 3 | MISS | READ 0x0 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | — | |
| 4 | CHECK_DIRTY | READ 0x0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | dirty=0 |
| 5 | ALLOCATE | READ 0x0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | |
| 6 | MEM_READ | READ 0x0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | — | |
| 7 | MEM_WAIT_READ_1 | READ 0x0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | — | |
| 8 | MEM_WAIT_READ_2 | READ 0x0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | — | |
| 9 | UPDATE_CACHE_BLOCK | READ 0x0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | |
| 10 | UPDATE_TAG | READ 0x0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | |
| 11 | SET_VALID | READ 0x0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | |
| 12 | CHECK_WRITE_AFTER_ALLOC | READ 0x0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | |
| 13 | DONE | READ 0x0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 1000 | data=1000 |
| 14 | IDLE | READ 0x0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | |
| 15 | COMPARE_TAG | READ 0x0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | |
| 16 | READ_HIT | READ 0x0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 1000 | data=1000 |
| 17 | DONE | READ 0x0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 1000 | data=1000 |

*The first request (R 0x00) takes 13 cycles (cycles 1–13): classic cold miss path. The second request (R 0x00) takes only 4 cycles (cycles 14–17): read hit. The contrast illustrates the cache's primary purpose — eliminating repeated memory latency.*

**Dirty Miss and Write-Back (R 0x04 — cycles 24–40):**

| Cycle | State | Request | MWr | MRd | MRdy | Note |
|-------|-------|---------|:---:|:---:|:----:|------|
| 24 | IDLE | READ 0x4 | 0 | 0 | 0 | |
| 25 | COMPARE_TAG | READ 0x4 | 0 | 0 | 0 | |
| 26 | MISS | READ 0x4 | 0 | 0 | 0 | |
| 27 | CHECK_DIRTY | READ 0x4 | 0 | 0 | 0 | dirty=1 |
| 28 | WRITE_BACK | READ 0x4 | 0 | 0 | 0 | |
| 29 | MEM_WRITE | READ 0x4 | **1** | 0 | 0 | |
| 30 | MEM_WAIT_WRITE_1 | READ 0x4 | **1** | 0 | 0 | |
| 31 | MEM_WAIT_WRITE_2 | READ 0x4 | 0 | 0 | **1** | |
| 32 | ALLOCATE | READ 0x4 | 0 | 0 | 0 | |
| 33 | MEM_READ | READ 0x4 | 0 | **1** | 0 | |
| 34 | MEM_WAIT_READ_1 | READ 0x4 | 0 | **1** | 0 | |
| 35 | MEM_WAIT_READ_2 | READ 0x4 | 0 | 0 | 0 | mem_data_valid=1 |
| 36–39 | UPDATE/TAG/VALID/CHECK | READ 0x4 | 0 | 0 | 0 | |
| 40 | DONE | READ 0x4 | 0 | 0 | 0 | data=1040 |

*R 0x04 causes a dirty miss: it hits line 0, which was dirtied by W 0x00 777. The controller writes the dirty block back to memory (MEM_WRITE/WAIT×2), then fetches the new block (MEM_READ/WAIT×2). Total: 17 cycles.*

### 5.2 Workload 2: Hit-Focused

**Purpose:** Demonstrate cache performance after cold-start warmup. Shows that once a block is loaded, repeated reads and writes are served quickly.

**Input sequence:**
```
R 0x00    # Cold miss → allocate (13 cycles)
R 0x00    # Read hit (4 cycles)
W 0x00 111 # Write hit (6 cycles)
R 0x00    # Read hit (4 cycles)
W 0x00 222 # Write hit (6 cycles)
R 0x00    # Read hit (4 cycles)
```

**Results:**

| Metric | Value |
|--------|-------|
| Total Cycles | 37 |
| Total Requests | 6 |
| Reads | 4 |
| Writes | 2 |
| Hits | **5** |
| Misses | 1 |
| Writebacks | **0** |
| Hit Rate | **83.33%** |

**Final Cache State:**
```
Line 0: valid=1, dirty=1, tag=0, data=222
```

**Analysis:** The single cold miss costs 13 cycles. All 5 subsequent accesses are hits (4 + 6 + 4 + 6 + 4 = 24 cycles). Average latency after warmup is **3.8 cycles/request** vs. 13 cycles for the initial miss. No evictions occur, so `mem_wr` is never asserted after cycle 1. The cache line ends dirty (`data=222`) because the last write has not been flushed to memory.

**Write-Hit Path Trace (W 0x00 111 — cycles 19–23):**

| Cycle | State | Stall | Hit | DataOut |
|-------|-------|:-----:|:---:|---------|
| 18 | IDLE | 0 | 0 | — |
| 19 | COMPARE_TAG | 1 | 0 | — |
| 20 | WRITE_HIT | 1 | **1** | — |
| 21 | UPDATE_CACHE | 1 | 0 | — |
| 22 | SET_DIRTY | 1 | 0 | — |
| 23 | DONE | 0 | 0 | — |

*A write hit completes in 5 active cycles. The cache line data is updated in UPDATE_CACHE and marked dirty in SET_DIRTY, with no memory interaction whatsoever.*

### 5.3 Workload 3: Write-Back Stress

**Purpose:** Stress-test the writeback mechanism. All writes target the same cache index (line 0) with different tags, causing every subsequent write to evict the previous dirty line.

**Input sequence:**
```
W 0x00 100   # Write miss → allocate line 0 (tag=0), write 100, dirty
W 0x04 200   # Conflict miss on line 0: dirty evict tag=0, allocate tag=1, write 200, dirty
W 0x08 300   # Conflict miss on line 0: dirty evict tag=1, allocate tag=2, write 300, dirty
W 0x0C 400   # Conflict miss on line 0: dirty evict tag=2, allocate tag=3, write 400, dirty
R 0x00       # Conflict miss on line 0: dirty evict tag=3, allocate tag=0 (read)
R 0x10       # Conflict miss on line 0: clean evict tag=0, allocate tag=4 (read)
```

**Results:**

| Metric | Value |
|--------|-------|
| Total Cycles | **106** |
| Total Requests | 6 |
| Reads | 2 |
| Writes | 4 |
| Hits | **0** |
| Misses | 6 |
| Writebacks | **4** |
| Hit Rate | **0.00%** |

**Final Cache State:**
```
Line 0: valid=1, dirty=0, tag=4, data=1160
```

**Analysis:** This is the worst-case scenario for a direct-mapped cache with write-back policy: **100% miss rate** with **4 writebacks out of 6 requests**. Each request that hits the same-index line must first flush the previous dirty occupant to memory (4+1 = 5 write-back paths; only 4 were dirty). The 106-cycle total reflects the extreme overhead: each dirty write miss costs ~19 cycles (4 write-back + 15 write-miss). This workload illustrates the *thrashing* problem unique to direct-mapped caches.

**Cycle Count Comparison — Dirty vs. Clean Writeback:**

| Request | Hit/Miss | Dirty? | Approx. Cycles | Writeback? |
|---------|----------|:------:|:--------------:|:----------:|
| W 0x00 100 | Write miss | No | 15 | No |
| W 0x04 200 | Write miss | Yes (evict 0x00) | 19 | **Yes** |
| W 0x08 300 | Write miss | Yes (evict 0x04) | 19 | **Yes** |
| W 0x0C 400 | Write miss | Yes (evict 0x08) | 19 | **Yes** |
| R 0x00 | Read miss | Yes (evict 0x0C) | 17 | **Yes** |
| R 0x10 | Read miss | No (evict 0x00) | 13 | No |

*Total ≈ 15+19+19+19+17+13 = 102 cycles; the remaining 4 cycles account for the IDLE state of each request.*

### 5.4 Workload 4: Conflict-Miss Sequence

**Purpose:** Demonstrate multi-line cache behaviour — fill all 4 lines, exercise hits across all lines, create one dirty eviction, and one clean eviction.

**Input sequence:**
```
# Phase 1: Fill all 4 cache lines (compulsory misses)
R 0   # → line 0
R 1   # → line 1
R 2   # → line 2
R 3   # → line 3

# Phase 2: Read hits on all 4 lines
R 0   R 1   R 2   R 3

# Phase 3: Write to line 0 (write hit, makes it dirty)
W 0 500

# Phase 4: R 4 → maps to line 0 → dirty eviction of addr 0, then allocate
R 4

# Phase 5: R 5 → maps to line 1 → clean eviction of addr 1, then allocate
R 5
```

**Results:**

| Metric | Value |
|--------|-------|
| Total Cycles | 104 |
| Total Requests | 11 |
| Reads | 10 |
| Writes | 1 |
| Hits | **5** |
| Misses | 6 |
| Writebacks | **1** |
| Hit Rate | **45.45%** |

**Final Cache State:**
```
Line 0: valid=1, dirty=0, tag=1, data=1040   (addr 4 allocated)
Line 1: valid=1, dirty=0, tag=1, data=1050   (addr 5 allocated)
Line 2: valid=1, dirty=0, tag=0, data=1020   (addr 2 still in place)
Line 3: valid=1, dirty=0, tag=0, data=1030   (addr 3 still in place)
```

**Phase 3 — W 0 500 (Write Hit):** Line 0 becomes dirty with data=500.

**Phase 4 — R 4 (Dirty Eviction):** Address 4 maps to line 0 (index = 4 % 4 = 0, tag = 4/4 = 1). Line 0 is occupied by address 0 (tag=0) which is dirty. FSM path: MISS → CHECK_DIRTY (dirty=1) → WRITE_BACK → MEM_WRITE/WAIT×2 → ALLOCATE → MEM_READ/WAIT×2 → UPDATE/TAG/VALID → DONE. One writeback is counted.

**Phase 5 — R 5 (Clean Eviction):** Address 5 maps to line 1. Line 1 holds address 1 (tag=0), which is clean (never written). FSM path: MISS → CHECK_DIRTY (dirty=0) → ALLOCATE → MEM_READ/WAIT×2 → UPDATE/TAG/VALID → DONE. No writeback needed.

**Hit and Miss Breakdown:**

| Phase | Requests | Hits | Misses | Writebacks |
|-------|----------|:----:|:------:|:----------:|
| Phase 1 (compulsory fill) | 4 | 0 | 4 | 0 |
| Phase 2 (all hits) | 4 | **4** | 0 | 0 |
| Phase 3 (write hit) | 1 | **1** | 0 | 0 |
| Phase 4 (dirty conflict) | 1 | 0 | 1 | **1** |
| Phase 5 (clean conflict) | 1 | 0 | 1 | 0 |
| **Total** | **11** | **5** | **6** | **1** |

### 5.5 Comparative Performance Summary

| Workload | Requests | Cycles | Hits | Misses | Writebacks | Hit Rate | Avg Cycles/Req |
|----------|:--------:|:------:|:----:|:------:|:----------:|:--------:|:--------------:|
| Full Functionality | 7 | 79 | 3 | 4 | 2 | 42.86% | 11.3 |
| Hit-Focused | 6 | 37 | 5 | 1 | 0 | **83.33%** | 6.2 |
| Write-Back Stress | 6 | **106** | 0 | 6 | 4 | **0.00%** | 17.7 |
| Conflict-Miss | 11 | 104 | 5 | 6 | 1 | 45.45% | 9.5 |

These results confirm the expected performance characteristics:
- **Hit rate directly controls latency.** The hit-focused workload at 83.33% hit rate achieves 6.2 cycles/request; the write-back stress workload at 0% hit rate requires 17.7 cycles/request — nearly 3× slower.
- **Writebacks are expensive.** Each writeback adds 4 cycles to the miss penalty, compounding the total latency.
- **Direct mapping creates conflict vulnerability.** Workloads 1, 3, and 4 all exploit the single-line-per-index structure to force evictions that would not occur in a 2-way or 4-way set-associative cache.

---

## 6. Detailed Signal Analysis

This section analyses each interface signal's temporal behaviour across the FSM lifecycle.

### 6.1 `cpu_req_ready` and `cpu_stall`

These two signals are complementary indicators of the controller's readiness:

- `cpu_req_ready` is asserted **only in IDLE** when no request is pending. It acts as a "green light" for the CPU to submit the next request. The CPU checks this every cycle before enqueuing new work.
- `cpu_stall` is asserted in **every state except IDLE and DONE**. It is the "red light" that forces the CPU to pause.

In the DONE state, both signals are deasserted simultaneously: `cpu_req_ready` remains 0 (the controller is still processing), but `cpu_stall` goes to 0, allowing the CPU to receive the result data (`data_out_valid`). On the very next cycle, the controller returns to IDLE and raises `cpu_req_ready`.

**Timeline Pattern for a Read Hit:**
```
Cycle:    1      2           3        4
State:  IDLE  COMPARE_TAG  READ_HIT  DONE  → IDLE
Rdy:      1      0            0        0     1
Stall:    0      1            1        0     0
```

### 6.2 `cache_hit` and `cache_miss`

These signals act as **one-cycle informational pulses** that signal the result of tag comparison:

- `cache_hit` is asserted **only in READ_HIT or WRITE_HIT** — always exactly one cycle after COMPARE_TAG.
- `cache_miss` is asserted **only in MISS** — the state immediately after COMPARE_TAG on a miss.

Neither signal is held; they pulse for exactly one cycle. In the trace data, they serve as markers identifying which branch the FSM took at the COMPARE_TAG decision point.

### 6.3 Memory Signals: `mem_rd` and `mem_wr`

These signals drive the memory bus and model realistic memory request protocols:

**`mem_wr` (write-back path):**
```
MEM_WRITE:       mem_wr = 1  (initiate write)
MEM_WAIT_WRITE_1: mem_wr = 1  (write in progress, cycle 1 of 2)
MEM_WAIT_WRITE_2: mem_wr = 0, mem_ready = 1  (write acknowledged)
```

The controller holds `mem_wr` high for **2 consecutive cycles** before the memory asserts `mem_ready`. The actual `mem.write_word()` call is made at the transition out of MEM_WAIT_WRITE_2 — modelling that data is committed only when memory confirms readiness.

**`mem_rd` (block allocation path):**
```
MEM_READ:       mem_rd = 1  (initiate read)
MEM_WAIT_READ_1: mem_rd = 1  (read in progress, cycle 1 of 2)
MEM_WAIT_READ_2: mem_rd = 0, mem_data_valid = 1  (data available)
```

Similarly, `mem_rd` is held for 2 cycles; the actual `mem.read_word()` call occurs at the transition out of MEM_WAIT_READ_2, when `mem_data_valid` confirms the data is ready.

This protocol closely mirrors real bus handshaking: the requester asserts the command, the memory acknowledges with a valid/ready signal, and only then does the data transfer complete.

### 6.4 `mem_ready` and `mem_data_valid`

These are **response signals from memory** (modelled as Moore outputs in the FSM):

- `mem_ready` is asserted in **MEM_WAIT_WRITE_2** only: one cycle, confirming the write-back is done.
- `mem_data_valid` is asserted in **MEM_WAIT_READ_2** only: one cycle, confirming the fetched block is valid.

Both signals are deasserted on the very next transition. This models a synchronous request-acknowledge protocol without a bus hold.

### 6.5 `data_out_valid` and `data_out`

These signals deliver read data to the CPU:

- `data_out_valid` is asserted in **READ_HIT** and in **DONE** (when the pending request was a read). It is never asserted for write operations.
- `data_out` holds the valid word **whenever `data_out_valid` is high**, and is meaningless otherwise.

There are two moments where `data_out_valid` goes high during a read:
1. **READ_HIT state:** One cycle of early valid (the tag was just confirmed). `data_out = current_line->data`.
2. **DONE state (for reads):** Final valid assertion at the end of the pipeline. This applies to both hit-path reads and miss-path reads after block installation.

The CPU should register `data_out` on the cycle when `data_out_valid` is high and `cpu_stall` returns to low.

---

## 7. Validation Against FSM Specification

The simulator was verified to correctly implement the `fsm_diagram.md` specification by tracing every request through all workloads and confirming the exact state sequence.

### 7.1 Read Miss Verification

**Expected path (from FSM spec):**
```
IDLE → COMPARE_TAG → MISS → CHECK_DIRTY → ALLOCATE →
MEM_READ → MEM_WAIT_READ_1 → MEM_WAIT_READ_2 →
UPDATE_CACHE_BLOCK → UPDATE_TAG → SET_VALID →
CHECK_WRITE_AFTER_ALLOC → DONE
```

**Observed (from trace, R 0x00, cycles 1–13):**
```
IDLE, COMPARE_TAG, MISS, CHECK_DIRTY, ALLOCATE,
MEM_READ, MEM_WAIT_READ_1, MEM_WAIT_READ_2,
UPDATE_CACHE_BLOCK, UPDATE_TAG, SET_VALID,
CHECK_WRITE_AFTER_ALLOC, DONE
```
✅ **Exact match** — 13 active states.

### 7.2 Write Miss + Write-Allocate Verification

**Expected path:**
```
IDLE → COMPARE_TAG → MISS → CHECK_DIRTY → ALLOCATE →
MEM_READ → MEM_WAIT_READ_1 → MEM_WAIT_READ_2 →
UPDATE_CACHE_BLOCK → UPDATE_TAG → SET_VALID →
CHECK_WRITE_AFTER_ALLOC → WRITE_AFTER_ALLOC →
UPDATE_CACHE → SET_DIRTY → DONE
```

**Observed (from trace, W 0x08 888, cycles 41–56):**
```
IDLE, COMPARE_TAG, MISS, CHECK_DIRTY, ALLOCATE,
MEM_READ, MEM_WAIT_READ_1, MEM_WAIT_READ_2,
UPDATE_CACHE_BLOCK, UPDATE_TAG, SET_VALID,
CHECK_WRITE_AFTER_ALLOC, WRITE_AFTER_ALLOC,
UPDATE_CACHE, SET_DIRTY, DONE
```
✅ **Exact match** — 15 active states. Write-allocate correctly fetched the block before applying the write.

### 7.3 Dirty-Miss + Write-Back Verification

**Expected path:**
```
IDLE → COMPARE_TAG → MISS → CHECK_DIRTY → WRITE_BACK →
MEM_WRITE → MEM_WAIT_WRITE_1 → MEM_WAIT_WRITE_2 → ALLOCATE →
MEM_READ → ... → DONE
```

**Observed (from trace, R 0x04, cycles 24–40 / R 0x0C, cycles 63–79):**
```
IDLE, COMPARE_TAG, MISS, CHECK_DIRTY, WRITE_BACK,
MEM_WRITE, MEM_WAIT_WRITE_1, MEM_WAIT_WRITE_2, ALLOCATE,
MEM_READ, MEM_WAIT_READ_1, MEM_WAIT_READ_2,
UPDATE_CACHE_BLOCK, UPDATE_TAG, SET_VALID,
CHECK_WRITE_AFTER_ALLOC, DONE
```
✅ **Exact match** — dirty bit inspection correctly triggers writeback before allocation.

### 7.4 Write-Hit Verification

**Expected path:** `IDLE → COMPARE_TAG → WRITE_HIT → UPDATE_CACHE → SET_DIRTY → DONE`

**Observed (from trace, W 0x08 999, cycles 57–62 and W 0x00 111 in hits workload):**
```
IDLE, COMPARE_TAG, WRITE_HIT, UPDATE_CACHE, SET_DIRTY, DONE
```
✅ **Exact match.**

### 7.5 Read-Hit Verification

**Expected path:** `IDLE → COMPARE_TAG → READ_HIT → DONE`

**Observed (from trace, R 0x00 second occurrence, cycles 14–17):**
```
IDLE, COMPARE_TAG, READ_HIT, DONE
```
✅ **Exact match** — data_out_valid asserted in READ_HIT and DONE, data_out=1000.

---

## 8. Limitations and Future Work

### 8.1 Current Limitations

**Single-Word Cache Lines**
Each cache line holds exactly one 32-bit word. Real caches use multi-word blocks (typically 64 bytes = 16 words) to exploit spatial locality. Expanding block size would require a wider data bus and additional wait states per word.

**Direct-Mapped Only**
The direct-mapped organisation causes unnecessary conflict misses when multiple frequently-used addresses hash to the same index. An N-way set-associative cache with an LRU replacement policy would dramatically improve hit rates for realistic workloads.

**Blocking (Stalling) Cache**
The CPU is completely halted during every cache miss. Modern high-performance caches are *non-blocking* (or *lockup-free*): they can queue multiple in-flight misses (Miss Status Holding Registers, MSHRs) and continue serving hits while a miss is being resolved. This requires a significantly more complex FSM.

**Fixed Memory Latency**
Main memory is modelled with a constant 2-cycle latency. Real DRAM has variable latency (row hit, row miss, refresh, etc.) and burst transfer modes. A more realistic memory model would use a separate stochastic latency module.

**No Cache Coherency**
In multiprocessor systems, multiple caches may hold copies of the same memory block. The MESI/MOESI coherence protocols add several additional FSM states (Shared, Exclusive, Modified, Invalid) and require inter-cache snooping signals. This is outside the scope of the current simulator.

### 8.2 Potential Extensions

- Implement a 2-way set-associative cache with LRU replacement and compare hit rates with the direct-mapped version on the same workloads.
- Add a graphical real-time visualisation using a simple terminal UI or web frontend to show cache state changes cycle by cycle.
- Introduce a configurable memory latency model to study the effect of varying memory speed on overall CPI (cycles per instruction).
- Extend the write policy to support *write-through* (immediate memory write on every cache write) and compare it with write-back performance.
- Implement a non-blocking cache with MSHRs to allow multiple simultaneous in-flight misses.

---

## 9. Conclusion

This project successfully designed and implemented a cycle-accurate simulation of a finite state machine-based cache controller in C++17. The simulator faithfully models a direct-mapped, write-back, write-allocate cache with a 22-state FSM that accurately captures all the timing and signalling behaviour of a hardware cache controller.

**Key achievements:**

1. All 22 FSM states are implemented with correct Moore-style signal outputs and next-state logic, validated against the formal FSM specification in `fsm_diagram.md`.
2. Four carefully designed workloads cover every FSM execution path: read hit, write hit, read miss (clean), read miss (dirty writeback), write miss (clean, with write-allocate), and write miss (dirty writeback + write-allocate).
3. The cycle-by-cycle trace confirms that all interface signals (`cpu_req_ready`, `cpu_stall`, `cache_hit`, `cache_miss`, `mem_rd`, `mem_wr`, `mem_ready`, `mem_data_valid`, `data_out_valid`, `data_out`) are asserted and deasserted at precisely the correct clock cycles.
4. The simulator produces machine-readable CSV traces and human-readable summary statistics, facilitating automated testing and reproducibility.
5. The comparative workload analysis clearly demonstrates the performance impact of cache policy choices: an 83.33% hit rate delivers 6.2 cycles/request; a 0% hit rate (worst-case write-back thrashing) costs 17.7 cycles/request — a 185% degradation solely from cache conflict and writeback overhead.

The project demonstrates that a Finite State Machine is an elegant, correct, and practical model for hardware cache controller logic. The clear mapping from FSM specification to C++ `switch`-based implementation also highlights how hardware description language (HDL) FSMs translate directly into software simulation.

The complete source code, all workload files, and this report are available at:
**https://github.com/AbdullahIbnYousuf/Cache-Controller-FSM-Simulation**

---

## 10. Appendix

### Appendix A: Complete Source Code

The complete source is in `cache_sim.cpp` (616 lines). Key sections are reproduced below for reference.

**FSM State Enum:**
```cpp
enum class State {
    Idle, CompareTag, ReadHit, WriteHit, Miss,
    CheckDirty, WriteBack, MemWrite, MemWaitWrite1, MemWaitWrite2,
    Allocate, MemRead, MemWaitRead1, MemWaitRead2,
    UpdateCacheBlock, UpdateTag, SetValid,
    CheckWriteAfterAlloc, WriteAfterAlloc,
    UpdateCache, SetDirty, Done
};
```

**Moore Output Logic (signal generation per state):**
```cpp
switch (cur) {
case State::Idle:
    sig.cpu_req_ready = !pending_.has_value(); break;
case State::CompareTag:
    sig.cpu_stall = true; break;
case State::ReadHit:
    sig.cache_hit = true; sig.cpu_stall = true;
    sig.data_out_valid = true; sig.data_out = current_line_->data; break;
case State::WriteHit:
    sig.cache_hit = true; sig.cpu_stall = true; break;
case State::Miss:
    sig.cache_miss = true; sig.cpu_stall = true; break;
case State::MemWrite:
    sig.mem_wr = true; sig.cpu_stall = true; break;
case State::MemWaitWrite1:
    sig.mem_wr = true; sig.cpu_stall = true; break;
case State::MemWaitWrite2:
    sig.mem_ready = true; sig.cpu_stall = true; break;
case State::MemRead:
    sig.mem_rd = true; sig.cpu_stall = true; break;
case State::MemWaitRead1:
    sig.mem_rd = true; sig.cpu_stall = true; break;
case State::MemWaitRead2:
    sig.mem_data_valid = true; sig.cpu_stall = true; break;
// ... [all other stall states elided for space]
case State::Done:
    sig.cpu_stall = false;
    if (pending_->type == ReqType::Read)
        { sig.data_out_valid = true; sig.data_out = current_line_->data; } break;
}
```

**Address Decomposition:**
```cpp
size_t   index_of(uint32_t a) const { return a % lines_.size(); }
uint32_t tag_of  (uint32_t a) const { return a / (uint32_t)lines_.size(); }
```

### Appendix B: Full Trace — Workload 1

The complete 80-row cycle trace for Workload 1 is available in `outputs/reference/full_trace.csv`. A representative excerpt is shown in Section 5.1. The CSV format is:

```
cycle, state, request, cpu_req_ready, cpu_stall, cache_hit, cache_miss,
mem_rd, mem_wr, mem_ready, mem_data_valid, data_out_valid, data_out, note
```

### Appendix C: FSM Mermaid Diagram Source

The complete Mermaid stateDiagram-v2 source from `fsm_diagram.md`:

```
stateDiagram-v2
    [*] --> IDLE

    IDLE --> COMPARE_TAG : cpu_req_vld=1

    COMPARE_TAG --> READ_HIT  : hit=1 & valid=1 & read
    COMPARE_TAG --> WRITE_HIT : hit=1 & valid=1 & write
    COMPARE_TAG --> MISS      : hit=0 OR valid=0

    READ_HIT  --> DONE : data_out_vld=1, cpu_stall=0, cache_hit=1

    WRITE_HIT    --> UPDATE_CACHE
    UPDATE_CACHE --> SET_DIRTY
    SET_DIRTY    --> DONE : cpu_stall=0

    MISS --> CHECK_DIRTY : cpu_stall=1, cache_miss=1

    CHECK_DIRTY --> WRITE_BACK : dirty=1
    CHECK_DIRTY --> ALLOCATE   : dirty=0

    WRITE_BACK       --> MEM_WRITE
    MEM_WRITE        --> MEM_WAIT_WRITE_1 : mem_wr=1, cpu_stall=1
    MEM_WAIT_WRITE_1 --> MEM_WAIT_WRITE_2
    MEM_WAIT_WRITE_2 --> ALLOCATE : mem_ready=1

    ALLOCATE         --> MEM_READ
    MEM_READ         --> MEM_WAIT_READ_1 : mem_rd=1, cpu_stall=1
    MEM_WAIT_READ_1  --> MEM_WAIT_READ_2
    MEM_WAIT_READ_2  --> UPDATE_CACHE_BLOCK : mem_data_valid=1

    UPDATE_CACHE_BLOCK --> UPDATE_TAG
    UPDATE_TAG         --> SET_VALID
    SET_VALID          --> CHECK_WRITE_AFTER_ALLOC

    CHECK_WRITE_AFTER_ALLOC --> WRITE_AFTER_ALLOC : write_miss=1
    CHECK_WRITE_AFTER_ALLOC --> DONE              : read_miss=1

    WRITE_AFTER_ALLOC --> UPDATE_CACHE : write_data
    UPDATE_CACHE      --> SET_DIRTY

    DONE --> IDLE
```

---

*End of Report*

---

> **GitHub Repository:** https://github.com/AbdullahIbnYousuf/Cache-Controller-FSM-Simulation
> 
> Build: `g++ -std=c++17 -Wall -Wextra -O2 cache_sim.cpp -o cache_sim`  
> Run all workloads: `make run-all` (Linux/macOS) or `mingw32-make run-all` (Windows/MinGW)
