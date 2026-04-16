# Cache Controller FSM Simulation Report


- **Assignment Title:** Cache Controller FSM Simulation
- **Course Name/Code:** Computer Organization and Architecture (COA)
- **Submission Date:** April 16, 2026

## 1. Introduction
This project aims to design and simulate the exact cycle-by-cycle logic of a cache controller using a finite-state machine (FSM). The primary goal is to validate how read and write requests are handled, simulating hit/miss scenarios and write-back functionality accurately over multiple clock cycles. A finite-state machine is particularly well-suited for cache controller logic since modern CPU cache operations involve deterministic sequences of distinct phases (e.g., Tag Checking, Memory Wait, Update).

The simulation runs under the scope of a simplified CPU single request queue, a single-word block size, and a fixed-latency main memory of 2 clock cycles.

## 2. Simulator Setup
The simulator is written in modern C++ (C++17). The project includes the main source file (`cache_sim.cpp`) and custom input trace files to validate exact CPU workloads.
**Project Structure:**
- `cache_sim.cpp`: Contains the FSM logic and states.
- `fsm_diagram.md`: The exact FSM specification designed using Mermaid.js.
- `inputs/`: A collection of input trace workloads to drive the simulation.
- `outputs/`: Trace results and statistical summaries output by the simulator.
- `Makefile`: Script to build and run workloads systematically (using `mingw32-make` or `make`).

**Build & Run:**
The simulator can be built via `mingw32-make` and run via `mingw32-make run-all` to generate all experimental CSV outputs and summaries.
The input workload format follows simple text syntax containing operators and hex addresses:
- `R 0x00` (Read from memory address 0x0)
- `W 0x00 777` (Write value 777 to memory address 0x0)

## 3. Cache Controller FSM Design
The designed Cache Controller strictly adheres to a **direct-mapped**, **write-back**, and **write-allocate** policy. 
- **Transitions and States:** The FSM begins in an `IDLE` state, awaiting a valid CPU request. Upon receiving a request, it transitions to `COMPARE_TAG` where it incurs a stall to check the tag validity arrays. Depending on a hit or miss, it executes states like `READ_HIT`, `WRITE_HIT`, or transitions into a sub-FSM dealing with miss handling (`CHECK_DIRTY`). A dirty miss initiates `WRITE_BACK`, triggering explicit memory write wait cycles before branching into `ALLOCATE` and `MEM_READ` wait cycles.
- **Signals:** During states like `MEM_WAIT_WRITE_1/2`, `cpu_stall` is held high, and `mem_wr` commands are dispatched. Similarly, `data_out_valid` is asserted during `READ_HIT` and `DONE` states.

## 4. Experimental Workloads

We validate the FSM against three primary workloads designed to isolate specific edge cases.

### Workload 1: Full Functionality Workload
- **Input sequence:** `R 0x00`, `R 0x00`, `W 0x00 777`, `R 0x04`, `W 0x08 888`, `W 0x08 999`, `R 0x0C`
- **Summary:** Total cycles: 79. Hits: 3. Misses: 4. Writebacks: 2. Hit rate: 42.86%.
- **Behavior Interpretation:** Tests the complete spectrum of controller operations: cold start read misses, read hits, write hits, and eviction of dirty lines causing write-backs. When `R 0x04` is executed, it forces the eviction of block 0 (which was made dirty by `W 0x00 777`), causing a writeback.

### Workload 2: Hit-focused Workload
- **Input sequence:** `R 0x00`, `R 0x00`, `W 0x00 111`, `R 0x00`, `W 0x00 222`, `R 0x00`
- **Summary:** Total cycles: 37. Hits: 5. Misses: 1. Writebacks: 0. Hit rate: 83.33%.
- **Behavior Interpretation:** Designed to measure optimized latency after the initial cold miss overhead. The hit rate is very high, keeping `cpu_stall` minimized after the first cache allocation. No evictions occurred.

### Workload 3: Writeback-focused Workload
- **Input sequence:** `W 0x00 100`, `W 0x04 200`, `W 0x08 300`, `W 0x0C 400`, `R 0x00`, `R 0x10`
- **Summary:** Total cycles: 106. Hits: 0. Misses: 6. Writebacks: 4. Hit rate: 0.00%.
- **Behavior Interpretation:** Tests extreme thrashing. We write repeatedly to the cache mapping perfectly to block 0 using different tags. Every subsequent request is a miss, and because the policy is write-back, every new request triggers the `WRITE_BACK` mechanism, leading to very low performance and extended memory stalls.

## 5. Detailed Signal Analysis
- **`cpu_req_ready` & `cpu_stall`:** The simulator uses `cpu_req_ready` during `IDLE` to indicate readiness for the next instruction. As soon as a request enters the pipeline, `cpu_stall` drives to `1` during `COMPARE_TAG` and all memory-handling cycles. `cpu_stall` only goes back to `0` at the `DONE` state.
- **`cache_hit` & `cache_miss`:** These signals act as internal FSM flags during tag evaluation, effectively steering the branch prediction into hit-handling arrays (`READ_HIT`/`WRITE_HIT`) or miss memory sequences (`MISS`).
- **Memory Signals (`mem_rd`, `mem_wr`, etc.):** When transferring data via main memory, the controller explicitly outputs `mem_rd` (for line allocation) or `mem_wr` (for writeback operations). The simulated main memory then provides `mem_ready` or `mem_data_valid` after fixed two-cycle emulation.
- **`data_out_valid`:** Only toggled true during final states where read cycles successfully conclude and valid memory buses emit fetched cache arrays to the CPU.

## 6. Validation Against FSM Idea
The trace files effectively validate our initial Mermaid state diagram. For example, in the case of a write miss, the transition trace correctly logs navigating through `COMPARE_TAG` → `MISS` → `CHECK_DIRTY` → `ALLOCATE` → `MEM_READ` → `MEM_WAIT_READ_1/2` → `UPDATE_CACHE_BLOCK` → `UPDATE_TAG` → `SET_VALID` → `CHECK_WRITE_AFTER_ALLOC` → `WRITE_AFTER_ALLOC` → `UPDATE_CACHE` → `SET_DIRTY` → `DONE`. 
The write-allocate methodology successfully ensures blocks are safely initialized from memory prior to data mutation, maintaining cache coherency according strictly to the FSM.

## 7. Limitations and Future Work
- **Single-Word Line Model:** Currently, the block size is identically 1 word. Spatial locality optimizations are impossible unless block size expansion is added in the future.
- **Blocking Cache Only:** CPU operations are strictly stalled during memory latency operations. Advanced non-blocking or out-of-order execution controllers are a viable extension.
- **Direct Mapped:** The direct-mapping restricts capacity concurrency. Expanding to an N-way set-associative cache with an LRU replacement policy would reduce conflict misses drastically. 

## 8. Conclusion
This project successfully implemented an accurate, cycle-by-cycle Cache Controller FSM that replicates industrial direct-mapped, write-back, and write-allocate policies in C++. We analyzed its latency behaviors via varying extreme workloads. The trace results completely validated precisely scheduled state branching that adhered perfectly to the theoretical FSM design flow taught in the course.

## 9. Appendix
Selected code snippets, complete trace excerpts, and the FSM diagram can be found inside the accompanying project repository.
