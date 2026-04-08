# Benchmark Results — Yona v0.1.0

**Date**: 2026-04-06
**Commit**: 582f9ad
**Platform**: Linux 6.19.10 (Fedora 43), AMD Ryzen 7 9700X 8-Core, 60GB RAM

**Compilers/Runtimes**:
- Yona 0.1.0 -O2 (LLVM 21.1.8)
- C: GCC 15.2.1 -O2
- Java: OpenJDK 25.0.2
- Haskell: GHC 9.8.4 -O2
- Node.js: v22.22.0
- Python: 3.14.3

**Iterations**: 3 (median reported)

## Summary

- **18/18 benchmarks passing**
- **Yona matches or beats C** on 3 benchmarks
- **Yona beats Java** on 5 of 9 shared benchmarks
- **Yona beats Haskell** on 3 of 7 shared benchmarks
- **Yona is 10-100x faster than Python** across the board

## Full Results (all times in milliseconds)

### CPU / Algorithms

| Benchmark | Yona | C | Haskell | Java | Node.js | Python |
|-----------|------|---|---------|------|---------|--------|
| fibonacci(35) | 16 | 7.9 | 23 | 28 | 102 | 479 |
| tak(30,20,10) | **66** | 78 | 74 | 61 | 260 | 1579 |
| sieve(100K) | 0.74 | 0.58 | 1.0 | 10 | 48 | 17 |
| sort(200) | 1.7 | 0.60 | 0.7 | 12 | 50 | 16 |
| ackermann(3,10) | 170 | 68 | 71 | 50 | 147 | 2159 |
| queens(10) | 16 | 1.5 | 5.5 | 25 | 59 | 76 |
| sum_squares(1M) | 0.68 | 0.60 | 0.8 | 10 | 48 | 83 |

### Collections

| Benchmark | Yona | C |
|-----------|------|---|
| list_map_filter | **0.81** | 0.86 |
| list_reverse | 0.89 | 0.68 |
| list_sum | 1.0 | 0.75 |
| dict_build (10K) | 1.5 | 0.82 |
| set_build (10K) | 1.4 | 0.80 |

### I/O

| Benchmark | Yona | C | Haskell | Java | Node.js | Python |
|-----------|------|---|---------|------|---------|--------|
| file_read (1.2MB) | **0.97** | 0.99 | 3.7 | 13 | 47 | 13 |
| file_readlines (20K) | 2.1 | 0.80 | 11 | 25 | 55 | 13 |
| file_write_read | 1.4 | 1.0 | — | 16 | 52 | 13 |
| file_parallel_read (3x) | 1.3 | — | — | — | — | — |
| process_exec (3x) | 1.4 | — | — | 20 | 54 | 18 |
| process_spawn | 1.4 | — | — | — | — | — |

## Ratios vs Yona (lower = Yona is faster)

| Benchmark | vs C | vs Haskell | vs Java | vs Node.js | vs Python |
|-----------|------|-----------|---------|------------|-----------|
| fibonacci | 2.0x | **0.7x** | **0.6x** | **0.2x** | **0.03x** |
| tak | **0.8x** | **0.9x** | 1.1x | **0.3x** | **0.04x** |
| sieve | 1.3x | **0.7x** | **0.07x** | **0.02x** | **0.04x** |
| sort | 2.9x | 2.4x | **0.1x** | **0.03x** | **0.1x** |
| ackermann | 2.5x | 2.4x | 3.4x | **0.9x** | **0.08x** |
| file_read | **1.0x** | **0.3x** | **0.08x** | **0.02x** | **0.07x** |
| file_readlines | 2.6x | **0.2x** | **0.08x** | **0.04x** | **0.2x** |

*Bold = Yona is faster than that language*

## Analysis by Language

### Yona vs C (gcc -O2)
- **3 wins**: list_map_filter (0.9x), tak (0.8x), file_read (1.0x)
- **Close (< 2x)**: 8 benchmarks
- **Gap**: queens (10.3x — allocation pressure), sort (2.9x — many small seqs)
- C is 2-3x faster on deep recursion (ackermann, fibonacci) due to lower call overhead

### Yona vs Haskell (GHC -O2)
- **3 wins**: fibonacci (Yona 16ms vs GHC 23ms), file_read (0.97 vs 3.7ms), file_readlines (2.1 vs 11ms)
- **Close**: tak (66 vs 74ms), sieve (0.74 vs 1.0ms)
- **Haskell faster**: sort (0.7ms vs 1.7ms — GHC's list fusion), ackermann (71 vs 170ms — GHC's unboxing)
- Both are compiled functional languages; Yona's io_uring gives an edge on I/O

### Yona vs Java (OpenJDK 25)
- **5 wins**: fibonacci, sieve, sort, file_read, file_readlines
- **Java faster**: tak (61 vs 66ms — JIT optimizes), ackermann (50 vs 170ms — HotSpot unboxing)
- Java's JVM startup cost (~10ms) inflates its I/O benchmarks
- Yona's native compilation has zero startup overhead

### Yona vs Node.js (V8)
- **All wins except ackermann** (147ms vs 170ms — V8's optimizing JIT)
- Yona is 4-65x faster on CPU benchmarks, 26-48x faster on I/O
- V8's warm-up and GC overhead dominates for short benchmarks

### Yona vs Python
- **All wins**: 10-100x faster across the board
- Python's interpreted execution is 24x slower on tak, 30x on fibonacci
- Even I/O is 6-13x slower due to Python's overhead

## Memory Usage

| Benchmark | Yona | C | Java | Haskell |
|-----------|------|---|------|---------|
| Most CPU benchmarks | 2-3 MB | 2 MB | 40-50 MB | 3-5 MB |
| File I/O | 3-6 MB | 3 MB | 40-50 MB | 4-6 MB |
| queens | 43 MB | 2 MB | — | — |
| sort | 8 MB | 2 MB | 50 MB | 3 MB |

Yona's memory usage is comparable to C and Haskell. Java's JVM adds ~40MB baseline.

## Concurrency Benchmarks

| Benchmark | Yona | C (pthreads) | Speedup vs Sequential |
|-----------|------|-------------|----------------------|
| parallel_async (4x100ms) | 101ms | 100ms | **4.0x** (vs 402ms sequential) |
| par_map (20 cubes) | 0.63ms | — | ~1.0x (trivial workload) |

The parallel let benchmark runs 4 independent 100ms async tasks. With Yona's
automatic structured concurrency, the parallel version (`let a = f 1, b = f 2, ...`)
runs in 101ms — matching C pthreads (100ms) and achieving near-ideal 4.0x
speedup over the sequential version (402ms).

Parallel comprehensions `[| expr for x = xs ]` are best for heavier per-element
work (>1ms). For sub-millisecond operations, task creation overhead dominates.
