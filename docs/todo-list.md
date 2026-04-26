# Yona-LLVM - Status and Roadmap

## Current Snapshot

- Compiler: Yona -> LLVM IR -> native executable via `yonac`
- REPL: `yona` compile-and-run interactive mode
- Tests: 1361 assertions across 233 test cases (all passing)
- Windows benchmark run (2026-04-26): 35/35 Yona rows passing, report refreshed, perf deltas reviewed
- Runtime backends: Linux + Windows native paths in place

## Active Priorities

### 1) Benchmarking hardening

- [ ] Add a benchmark reference conformance check (validate expected output per language lane)
- [ ] Investigate/resolve local Erlang lane toolchain crash (`erl.exe`/`erlc.exe` exit `0xC0000005`) affecting comparison coverage on this host

### 2) Platform/runtime closure

- [ ] macOS platform layer (kqueue path + per-OS runtime files)

### 3) Distribution readiness

- [ ] Windows installer productionization (upgrade behavior, signing, final UX polish)
- [ ] Final packaging pass for sysroot-based CLI/REPL distribution layout
- [ ] Enable embedded LLD backend by default across supported toolchains (resolve remaining dependency gates, e.g. MSVC-compatible LibXml2 on Windows)
- [ ] Implement true embedded LLD backend for Linux/macOS in-process mode

## Backlog (Open, Not Immediate)

### Code quality

- [ ] Relax stream-fusion gating only with benchmark evidence

### Performance

- [ ] LLVM EH migration (`invoke` / `landingpad`) if correctness requires it
- [ ] Profile-guided optimization
- [ ] JIT feasibility/design study (ORC/Cranelift/etc.)

### Language/runtime research

- [ ] Gradual typing with contracts
- [ ] Supervisors as effect handlers
- [ ] Channel deadlock detection
- [ ] LLVM coroutine lowering for async
- [ ] Distributed Yona
- [ ] Serialization system
- [ ] STM
- [ ] Content-addressed code model
- [ ] Multi-stage programming and compile-time evaluator
- [ ] User-defined derives and quasiquotes
- [ ] GPU module (`Std\GPU`) and later transparent GPU lowering
- [ ] Vulkan compute feasibility spike for dataframe-style ops (filter/map/group-by/join/aggregate)
- [ ] Prototype columnar GPU execution path (host->device transfer, kernel launch, result materialization)
- [ ] Benchmark crossover model for CPU vs GPU offload (row count + transfer overhead thresholds)

### Tooling

- [ ] Package manager/build tool
- [ ] LSP server

## Completed Milestones (Condensed)

- [x] Full frontend + LLVM codegen pipeline (modules, generics, traits, ADTs)
- [x] Effects + async + structured concurrency foundations
- [x] Persistent collections + RC/arena/perceus optimizations
- [x] Windows parity closure: runtime backends, tests green, skips removed
- [x] CLI/REPL path hardening (`--sysroot`, `YONA_HOME`, packaged runtime discovery)
- [x] Platform ABI freeze (`YONA_PLATFORM_ABI_VERSION=1`) and platform architecture docs
- [x] Handle-level `Std\File` operations routed through platform wrappers
- [x] CI/CD across Linux/macOS/Windows + Linux arm64 coverage in matrix
- [x] Docker + Homebrew + RPM + DEB + release workflow
- [x] Windows benchmark matrix parity (all language cells filled) + native peak RSS capture
- [x] Startup probe cache normalization (versioned cache + stale Windows RSS invalidation)
- [x] Windows benchmark refresh + 3x stability reruns (perf deltas + large-file I/O variance)
- [x] O(1) transfer-scope basic block detection (scope-entry ordinal watermark + O(1) droppability checks)
- [x] Unified seq/map transfer bookkeeping into explicit domain-tagged transfer tracking
- [x] Borrow inference metadata across `.yonai` module boundaries, with caller temp cleanup and unwind-safe owned fallbacks
- [x] Stdlib `.yonai` regeneration blockers resolved across all `lib/Std/**/*.yona` modules
- [x] Linker/distribution milestone: dual modes, prebuilt runtime packaging, in-process LLD scaffold, CI reporting, and CMake modularization
- [x] Windows installer milestone: WiX scaffold + Windows release CI artifacts (ZIP + MSI)

## Notes

- Benchmark reports:
  - `docs/benchmark-results.md` (Linux baseline)
  - `docs/benchmark-results-windows.md` (Windows reruns)
- Stream-fusion evidence gate (required before relaxing fusion constraints):
  - run full benchmark matrix on Linux + Windows with 3 reruns (`-n 10`) and compare medians
  - require >=5% median win on the targeted fusion rows and no correctness/test regressions
  - allow no >3% median regression on non-targeted rows (or require documented root cause + follow-up fix)
- Distribution assumption: normal `yonac` usage should not require an external C compiler;
  packaged runtime artifacts are the default path. System C compiler use is
  treated as an explicit advanced/fallback mode.
- Process hygiene: update this todo list after each implementation round.
- Keep this file focused on actionable open work and short milestone summaries.
