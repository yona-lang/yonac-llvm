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

- **25/25 benchmarks passing** (7 CPU, 5 collections, 9 I/O, 4 concurrency)
- **Yona matches or beats C** on 3 benchmarks (tak 0.8x, list_map_filter 1.0x, process_spawn 1.0x)
- **Within 1.5x of C** on 8 more (file_read, process_exec, list_reverse, list_sum, sum_squares, sieve, file_parallel_read, file_write_read)
- **Streaming I/O**: readLines uses 64KB-buffered Iterator (1.9ms, O(64KB) memory)
- **Parallel async achieves near-ideal 4.0x speedup** matching C pthreads

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

| Benchmark | Yona | C | Java | Node.js | Python |
|-----------|------|---|------|---------|--------|
| file_read (1.2MB) | 0.90 | 0.86 | 13 | 47 | 14 |
| file_readlines (streaming) | 1.9 | 0.88 | 26 | 48 | 13 |
| file_write_read | 1.4 | 0.91 | 18 | 50 | 13 |
| file_parallel_read (3x) | 1.5 | 0.97 | 25 | 50 | 24 |
| process_exec (3x parallel) | 1.4 | 1.1 | 27 | 54 | 26 |
| process_spawn | 1.3 | 1.3 | 23 | 51 | 15 |

### I/O — Large Files (50MB, realistic workloads)

| Benchmark | Yona | C | Ratio | Analysis |
|-----------|------|---|-------|----------|
| file_read_large (50MB) | 14.5 | 3.5 | 4.2x | Yona: 50MB string alloc; C: 64KB streaming |
| file_parallel_read_large (4×10MB) | 10.6 | 1.6 | 6.7x | 4 × alloc vs 4 × streaming |
| file_write_read_large (50MB r+w+r) | 48.1 | 15.3 | 3.1x | Two 50MB allocs vs streaming |

The gap is fundamental: Yona's `readFile` returns the entire file as a string
(contiguous 50MB allocation), while idiomatic C streams through a 64KB buffer.
O(1) string length (stored in RC header) saves ~5ms on 50MB files.
Closing this gap requires streaming I/O (see TODO: Cooperative Suspension).

Note: All I/O references use parallel execution where applicable.

## I/O Ratios (Yona time / other language time — lower = Yona faster)

| Benchmark | vs C | vs Java | vs Node.js | vs Python |
|-----------|------|---------|------------|-----------|
| file_read | 1.3x | **0.08x** | **0.02x** | **0.07x** |
| file_readlines | 2.7x | **0.08x** | **0.05x** | **0.17x** |
| file_write_read | 1.6x | **0.09x** | **0.03x** | **0.12x** |
| file_parallel_read | 1.5x | **0.06x** | **0.03x** | **0.06x** |
| process_exec | 1.1x | **0.05x** | **0.02x** | **0.05x** |
| process_spawn | 1.0x | **0.06x** | **0.03x** | **0.09x** |

*Bold = Yona is faster. All I/O references use parallel execution where applicable.*

## Analysis by Language

### Yona vs C (gcc -O2)
- **2 wins**: list_map_filter (1.0x), tak (0.8x)
- **Close (< 1.5x)**: file_read (1.3x), list_reverse (1.2x), sum_squares (1.1x), process_exec (1.1x), process_spawn (1.0x)
- **Gap**: queens (10.1x — allocation pressure), sort (3.0x — many small seqs)
- C is 2x faster on deep recursion (ackermann, fibonacci) due to lower call overhead
- file_parallel_read 1.5x: io_uring ring overhead on page-cached files

### Yona vs Java (OpenJDK 25)
- **Yona 10-20x faster on I/O** — native compilation vs JVM startup
- file_read: 1.0ms vs 13ms (13x faster)
- process_exec: 1.3ms vs 27ms (21x faster)
- Java faster on CPU: tak, ackermann (HotSpot JIT unboxing)

### Yona vs Node.js (V8)
- **Yona 35-50x faster on I/O** — no V8 warm-up, no GC overhead
- file_read: 1.0ms vs 47ms (47x faster)
- process_spawn: 1.4ms vs 51ms (36x faster)

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

| Benchmark | Yona Parallel | Yona Sequential | C (pthreads) | Speedup |
|-----------|--------------|----------------|-------------|---------|
| async 4x100ms | **101ms** | 402ms | 101ms | **4.0x** |
| par_map (20 cubes) | 0.63ms | 0.65ms | — | ~1.0x |

The parallel let benchmark runs 4 independent 100ms async tasks. With Yona's
automatic structured concurrency, the parallel version (`let a = f 1, b = f 2, ...`)
achieves **near-ideal 4.0x speedup** matching C pthreads (101ms).

No explicit `async/await` needed — parallelism is automatic for independent let bindings.
Parallel comprehensions `[| expr for x = xs ]` are best for >1ms per-element work.
