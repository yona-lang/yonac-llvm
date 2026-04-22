# Benchmark Results - Windows

**Date**: 2026-04-22  
**Platform**: Windows 11 (build 26220)

**Provenance**: Tables regenerated with:

- `python bench/runner.py --yonac out/build/x64-debug/yonac.exe -n 10 -O 2 --compare "c,erl,java,hs,js,py" --json`
- Raw output: `bench/windows-full-bench-2026-04-22-n10-winrefs-rss.json`

## Summary

- **35/35 benchmarks passing** on Windows (Yona lane).
- Comparison coverage now has **0 missing cells**.
- C lanes use Windows-specific C refs when available (`*.win.c`).

## Startup-adjusted numbers

The runner reports adjusted values (`app_time = max(total - startup, 0.01ms)`).

### Measured cold-start floors (this machine, this run)

| Runtime | Startup time | Startup RSS |
|---------|-------------:|------------:|
| C | 9.05 ms | 3.9 MB |
| Yona | 10.7 ms | 6.2 MB |
| Haskell | 26 ms | 10.5 MB |
| Java | 54.2 ms | 40.3 MB |
| Python | 22.8 ms | 10.9 MB |
| Node.js | 40.4 ms | 32.3 MB |
| Erlang | 1155 ms | 53.2 MB |

## CPU / Algorithms (adjusted ms)

| Benchmark | Yona | C | Erlang | Haskell | Java | Node.js | Python |
|-----------|-----:|--:|-------:|--------:|-----:|--------:|-------:|
| fibonacci(35) | 102 | 112 | 48.9 | 25 | 7.19 | 53.9 | 680 |
| tak(30,20,10) | 156 | 161 | 122 | 81.1 | 44.3 | 205 | 2403 |
| sieve(500) | 3.5 | 0.23 | 0.01 | 0.506 | 0.01 | 0.01 | 0.01 |
| sort(200) | 0.01 | 0.01 | 0.01 | 0.702 | 11.4 | 1.59 | 2.03 |
| ackermann(3,11) | 259 | 259 | 349 | 294 | 141 | 228 | 10691 |
| queens(10) | 15.5 | 20.1 | 13.7 | 11.1 | 7.89 | 13.6 | 85.9 |
| sum_squares(1M) | 393 | 2.16 | 0.01 | 1.71 | 0.01 | 11 | 69.2 |

## Collections (adjusted ms)

| Benchmark | Yona | C | Erlang | Haskell | Java | Node.js | Python |
|-----------|-----:|--:|-------:|--------:|-----:|--------:|-------:|
| dict_build (10K) | 13 | 0.01 | 3.44 | 12.4 | 37.5 | 0.01 | 0.01 |
| set_build (10K) | 11.2 | 0.02 | 0.01 | 6.3 | 0.914 | 0.01 | 0.01 |
| list_map_filter | 0.01 | 1.59 | 0.01 | 0.01 | 2.53 | 0.01 | 0.01 |
| list_reverse | 0.01 | 1.57 | 0.01 | 6.42 | 0.01 | 0.01 | 0.01 |
| list_sum | 0.01 | 0.01 | 0.01 | 0.231 | 0.348 | 0.01 | 0.01 |
| int_array_fill_sum | 0.01 | 0.027 | 0.01 | 1.34 | 0.01 | 0.01 | 0.01 |
| int_array_map | 0.01 | 0.418 | 0.01 | 0.673 | 0.01 | 0.01 | 0.01 |
| int_array_sum | 0.261 | 0.294 | 0.01 | 0.433 | 0.01 | 0.01 | 0.01 |

## Concurrency (adjusted ms)

| Benchmark | Yona | C | Erlang | Haskell | Java | Node.js | Python |
|-----------|-----:|--:|-------:|--------:|-----:|--------:|-------:|
| par_map (20 cubes) | 2.28 | 3.26 | 0.01 | 1.68 | 0.01 | 0.01 | 14.5 |
| parallel_async | 109 | 111 | 107 | 99.5 | 110 | 104 | 191 |
| sequential_async | 436 | 436 | 423 | 404 | 421 | 424 | 504 |
| channel_pipeline | 0.01 | 1.85 | 46.8 | 2.22 | 10.9 | 0.01 | 8.06 |
| channel_fanin | 0.01 | 0.01 | 0.01 | 1.57 | 3.55 | 0.01 | 8.31 |
| channel_throughput | 0.01 | 0.01 | 50.2 | 2.38 | 16.5 | 0.01 | 11.2 |
| seq_map | 0.01 | 2.02 | 0.01 | 0.539 | 0.01 | 0.01 | 0.01 |
| task_group_arena | 0.01 | 0.01 | 0.01 | 0.645 | 0.01 | 0.01 | 0.01 |

## I/O - small files (adjusted ms)

| Benchmark | Yona | C | Erlang | Haskell | Java | Node.js | Python |
|-----------|-----:|--:|-------:|--------:|-----:|--------:|-------:|
| binary_read_chunks | 1.09 | 0.274 | 0.01 | 8.52 | 0.01 | 0.01 | 0.01 |
| binary_write_read | 1.67 | 7.85 | 0.01 | 10.7 | 5.56 | 1.19 | 0.075 |
| file_read | 1.1 | 0.01 | 0.01 | 5.87 | 5.08 | 0.01 | 0.01 |
| file_write_read | 0.01 | 10.5 | 0.01 | 13.9 | 0.01 | 0.01 | 0.01 |
| file_parallel_read | 0.01 | 0.01 | 0.08 | 6.37 | 11.5 | 0.01 | 15.7 |
| file_readlines | 0.01 | 2.4 | 0.459 | 20.6 | 30.9 | 0.01 | 0.01 |
| process_exec | 21.9 | 43.5 | 33.4 | 51 | 56.3 | 46 | 40.7 |
| process_spawn | 272 | 228 | 215 | 214 | 255 | 229 | 217 |

## I/O - large files (50 MB, adjusted ms)

| Benchmark | Yona | C | Erlang | Haskell | Java | Node.js | Python |
|-----------|-----:|--:|-------:|--------:|-----:|--------:|-------:|
| file_read_large | 7.56 | 26.2 | 4.99 | 18.1 | 21.5 | 45.7 | 15.9 |
| file_write_read_large | 108 | 150 | 305 | 325 | 85.6 | 138 | 485 |
| file_parallel_read_large | 6.69 | 0.01 | 1.46 | 17.8 | 29.9 | 6.61 | 25.7 |
| file_readlines_large | 65.7 | 140 | 23.6 | 22.8 | 79 | 103 | 69 |

## Memory (raw peak RSS, MB)

Windows runner now captures per-process peak working set (KB) via native APIs.
The table below summarizes per-runtime peak RSS across benchmark rows.

| Runtime | Min MB | Median MB | Max MB |
|---------|-------:|----------:|-------:|
| Yona | 6.2 | 6.6 | 111.3 |
| C | 4.0 | 4.1 | 8.7 |
| Erlang | 54.6 | 55.0 | 160.7 |
| Haskell | 10.6 | 11.8 | 115.3 |
| Java | 40.4 | 41.6 | 211.1 |
| Node.js | 33.2 | 34.4 | 138.9 |
| Python | 10.9 | 11.9 | 115.9 |

## Caveats

- Startup-adjusted floor can exaggerate ratios when values clamp to `0.01ms`.
- This report uses warm-cache behavior for file workloads.
- Startup RSS values are cached per runtime; rerun after toolchain/runtime changes.
