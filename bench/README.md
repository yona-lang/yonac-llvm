# Yona Benchmark Suite

## Quick Start

```bash
# Run all benchmarks (3 iterations, -O2)
python3 bench/runner.py

# Run specific benchmark
python3 bench/runner.py fibonacci

# Compare against C reference implementations
python3 bench/runner.py --compare-c

# Compare all optimization levels (O0, O1, O2, O3)
python3 bench/runner.py --all-opt-levels

# Compare everything
python3 bench/runner.py --all-opt-levels --compare-c

# Custom iterations and optimization level
python3 bench/runner.py -n 5 -O3

# JSON output for CI/scripting
python3 bench/runner.py --json
```

## What It Measures

- **Wall-clock time** (ms) — min/avg/max over N iterations
- **Peak RSS** (KB) — maximum resident set size via `/usr/bin/time -v`
- **Correctness** — each benchmark has a `.expected` file verified before timing
- **C comparison** — optional ratio vs equivalent C implementation

## Structure

```
bench/
  runner.py              # Benchmark runner
  README.md              # This file
  core/                  # Core language benchmarks
    fibonacci.yona       # Recursive Fibonacci (function call overhead)
    tak.yona             # Takeuchi function (deep recursion)
    sieve.yona           # Sieve of Eratosthenes (list filtering)
  collections/           # Collection operation benchmarks
    list_sum.yona        # Sum 100K elements (fold)
    list_reverse.yona    # Reverse 10K list (foldl + cons)
    list_map_filter.yona # Map + filter + fold pipeline
  numeric/               # Numeric computation benchmarks
    ackermann.yona       # Ackermann function (deep recursion)
    sum_squares.yona     # Sum of squares (tight loop, TCE)
  reference/             # C reference implementations
    fibonacci.c
    tak.c
    ackermann.c
    sum_squares.c
```

## Adding a Benchmark

1. Create `bench/<category>/<name>.yona`
2. Create `bench/<category>/<name>.expected` — expected output
3. Optionally `bench/reference/<name>.c` — C equivalent

The runner discovers all `.yona` files automatically. Each benchmark has a
10-second timeout to prevent runaway execution.

## Known Limitations

- N-queens benchmark requires closure capture of outer function parameters
  (tracked as a codegen bug — nested let functions don't capture enclosing
  function parameters correctly)
- Sieve uses O(n²) list filtering — expected to be slow
- No REPL-level benchmarks yet (compile time not measured)
