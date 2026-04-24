# Yona-LLVM - Status and Roadmap

## Current Snapshot

- Compiler: Yona -> LLVM IR -> native executable via `yonac`
- REPL: `yona` compile-and-run interactive mode
- Tests: 1275 assertions across 216 test cases (all passing)
- Windows benchmark run (2026-04-22): 35/35 Yona rows passing, full language matrix (0 missing), peak RSS captured
- Runtime backends: Linux + Windows native paths in place

## Active Priorities

### 1) Benchmarking hardening

- [ ] Add a benchmark reference conformance check (validate expected output per language lane)
- [x] Normalize startup-RSS probe cache so startup RSS table is always populated
- [x] Keep `docs/benchmark-results-windows.md` synced with the latest rerun JSON

### 2) Platform/runtime closure

- [ ] macOS platform layer (kqueue path + per-OS runtime files)

### 3) Distribution readiness

- [ ] Windows installer productionization (upgrade behavior, signing, final UX polish)
- [x] Draft WiX v4 installer scaffold (MSI package + staging/build script)
- [x] Add Windows release CI job emitting zip + MSI artifacts
- [ ] Final packaging pass for sysroot-based CLI/REPL distribution layout
- [x] Add dual linker mode control (`--linker-mode` / `YONAC_LINKER_MODE`: `auto|bundled|system|inprocess`)
- [x] Wire CLI/REPL link-mode plumbing to prefer bundled `lld` in `auto`
- [x] Refactor `yonac` linking into an internal "link plan" abstraction
- [x] Integrate prebuilt runtime objects/static libs into final packaging + compiler flow
- [x] Add optional in-process linker mode scaffold (`inprocess`) with external fallback
- [x] Add CI reporting for embedded-LLD activation status and disable reason
- [x] Add strict CI gate support via `YONAC_REQUIRE_INPROCESS_LLD=1`
- [x] Extract embedded-LLD CMake logic into `cmake/YonaInProcessLld.cmake`
- [ ] Enable embedded LLD backend by default across supported toolchains (resolve remaining dependency gates, e.g. MSVC-compatible LibXml2 on Windows)
- [x] Resolve Windows mixed-libxml link closure (`LLVMWindowsManifest` now retargeted to fetched `LibXml2`)
- [ ] Implement true embedded LLD backend for Linux/macOS in-process mode

## Backlog (Open, Not Immediate)

### Code quality

- [ ] O(1) transfer-scope basic block detection
- [ ] Revisit `transferred_seqs_` / `transferred_maps_` unification only if semantics remain explicit
- [ ] Relax stream-fusion gating only with benchmark evidence

### Performance

- [ ] LLVM EH migration (`invoke` / `landingpad`) if correctness requires it
- [ ] Profile-guided optimization
- [ ] JIT feasibility/design study (ORC/Cranelift/etc.)

### Language/runtime research

- [ ] Gradual typing with contracts
- [ ] Borrow annotations for read-only parameters
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

## Notes

- Benchmark reports:
  - `docs/benchmark-results.md` (Linux baseline)
  - `docs/benchmark-results-windows.md` (Windows reruns)
- Distribution assumption: normal `yonac` usage should not require an external C compiler;
  packaged runtime artifacts are the default path. System C compiler use is
  treated as an explicit advanced/fallback mode.
- Process hygiene: update this todo list after each implementation round.
- Keep this file focused on actionable open work and short milestone summaries.
