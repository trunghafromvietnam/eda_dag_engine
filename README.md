# High-Performance EDA Gate-Level Logic Simulation Engine

## 1. Specifications & Requirements
- **Goal:** Build an industrial-grade, deterministic gate-level logic simulator for large-scale digital circuits (2M+ gates).
- **Input:** ISCAS'85 `.bench` netlist files & random circuit topology generators.
- **Output:** Correct 64-bit parallel logic state vectors across all Primary Outputs.
- **Performance Requirement:** Sub-10ms evaluation time for 2M-gate circuits using multi-threading and event-driven optimizations.

## 2. Program Structure & System Design
The engine is architected using the **Structure of Arrays (SoA)** memory layout to optimize CPU L1/L2 cache line utilization.

## 3. Software Correctness & Verification
- **Deterministic Validation Harness:** Every optimization phase (Bit-Parallel, Multi-Threaded, Event-Driven) is verified against a unoptimized sequential baseline engine.
- **100% State Agreement:** 0-bit divergence across 64-bit test vectors on standard ISCAS'85 benchmarks.

## 4. Algorithmic Optimization & Performance Benchmarks

### Event-Driven Paradigm vs. Level-Based Baseline (2 Million Gates)
| Simulation Engine | Algorithm Paradigm | Execution Time (ms) | Speedup |
| :--- | :--- | :---: | :---: |
| **Phase 3 Baseline** | Level-Based (100% Gates Evaluated) | 16.91 ms | 1.00x |
| **Phase 4 Engine** | Event-Driven (Active-Gate Propagation) | **7.18 ms** | **2.36x** |

### Key Hardware & Concurrency Optimizations
1. **64-Way Bit-Parallelism:** Evaluates 64 simulation vectors simultaneously in a single CPU instruction via `uint64_t` bitwise operations.
2. **Cache-Line Alignment:** Structures memory buffers aligned to 64-byte boundaries to eliminate **False Sharing** across CPU worker threads.
3. **Static Chunking Work Distribution:** Eliminates synchronization overhead by partitioning level gates into continuous memory chunks per thread.

## 5. Build & Execution Guide
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./eda_dag_engine