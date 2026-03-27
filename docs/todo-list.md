# TODO List - Yona-LLVM

## Summary
- **Compiler**: Yona → LLVM IR → native executable via `yonac`
- **REPL**: `yona` — compile-and-run interactive mode
- **Tests**: 222 codegen assertions
- **Stdlib**: Option, Result, List, Tuple, Range, Test (45 exported functions)

## Roadmap

### Standard Library (all operations non-blocking via `extern async`)

Every stdlib I/O function is declared `extern async` — submitted to the thread
pool, returns a Promise, auto-awaited at use sites. Pure functions (no I/O) are
regular `extern` (synchronous). This gives transparent async with zero syntax
overhead: `let content = readFile "data.txt"` is non-blocking.

#### Std\String — text manipulation
- [ ] `length : String -> Int` (sync, exists as C shim)
- [ ] `split : String -> String -> Seq` — split by delimiter
- [ ] `join : String -> Seq -> String` — join with separator
- [ ] `trim : String -> String` — strip whitespace
- [ ] `replace : String -> String -> String -> String` — find/replace
- [ ] `indexOf : String -> String -> Int` — first occurrence (-1 if not found)
- [ ] `substring : String -> Int -> Int -> String` — slice by start+length
- [ ] `startsWith : String -> String -> Bool`
- [ ] `endsWith : String -> String -> Bool`
- [ ] `contains : String -> String -> Bool`
- [ ] `charAt : String -> Int -> Int` — character at index
- [ ] `toUpperCase : String -> String` (exists as C shim)
- [ ] `toLowerCase : String -> String` (exists as C shim)
- [ ] `chars : String -> Seq` — string to character code sequence
- [ ] `fromChars : Seq -> String` — character codes to string

#### Std\Math — numeric functions (all sync)
- [ ] `abs : Int -> Int` (exists)
- [ ] `max : Int -> Int -> Int` (exists)
- [ ] `min : Int -> Int -> Int` (exists)
- [ ] `pow : Float -> Float -> Float`
- [ ] `sqrt : Float -> Float` (exists)
- [ ] `floor : Float -> Int`
- [ ] `ceil : Float -> Int`
- [ ] `round : Float -> Int`
- [ ] `log : Float -> Float`
- [ ] `log10 : Float -> Float`
- [ ] `exp : Float -> Float`
- [ ] `sin : Float -> Float` (exists)
- [ ] `cos : Float -> Float` (exists)
- [ ] `tan : Float -> Float`
- [ ] `atan2 : Float -> Float -> Float`
- [ ] `pi : Float` (constant)
- [ ] `e : Float` (constant)

#### Std\Types — type inspection and conversion (sync)
- [ ] `toInt : String -> Int` (exists)
- [ ] `toFloat : String -> Float` (exists)
- [ ] `intToString : Int -> String`
- [ ] `floatToString : Float -> String`
- [ ] `boolToString : Bool -> String`

#### Std\IO — console I/O (async)
- [ ] `print : String -> Unit` (async — write to stdout)
- [ ] `println : String -> Unit` (async)
- [ ] `readLine : Unit -> String` (async — read line from stdin)
- [ ] `eprint : String -> Unit` (async — write to stderr)
- [ ] `eprintln : String -> Unit` (async)

#### Std\File — filesystem operations (all async)
- [ ] `readFile : String -> String` — read entire file to string
- [ ] `writeFile : String -> String -> Unit` — write string to file
- [ ] `appendFile : String -> String -> Unit` — append to file
- [ ] `readBytes : String -> Seq` — read file as byte sequence
- [ ] `writeBytes : String -> Seq -> Unit` — write bytes to file
- [ ] `exists : String -> Bool` — file/dir exists?
- [ ] `isFile : String -> Bool`
- [ ] `isDir : String -> Bool`
- [ ] `listDir : String -> Seq` — directory listing
- [ ] `mkdir : String -> Unit` — create directory
- [ ] `remove : String -> Unit` — delete file/directory
- [ ] `rename : String -> String -> Unit` — rename/move

#### Std\System — process and environment (async where blocking)
- [ ] `getEnv : String -> String` (sync)
- [ ] `setEnv : String -> String -> Unit` (sync)
- [ ] `getArgs : Unit -> Seq` (sync — command line args)
- [ ] `exit : Int -> Unit` (sync)
- [ ] `exec : String -> String` (async — run command, return output)
- [ ] `sleep : Int -> Unit` (async — sleep N milliseconds)
- [ ] `currentTimeMillis : Unit -> Int` (sync)

#### Std\Http — HTTP client (all async)
- [ ] `get : String -> String` — HTTP GET, returns body
- [ ] `post : String -> String -> String` — HTTP POST with body
- [ ] `getWithHeaders : String -> Seq -> String` — GET with custom headers
- [ ] `postWithHeaders : String -> Seq -> String -> String` — POST with headers

Implementation: C shims using libcurl, declared `extern async`.

#### Std\Json — JSON parsing (sync)
- [ ] `parse : String -> a` — parse JSON string to Yona value
- [ ] `stringify : a -> String` — serialize Yona value to JSON

Implementation: C shims using a lightweight JSON parser (cJSON or custom).

#### Std\Regexp — regular expressions (sync)
- [ ] `match : String -> String -> Option` — first match
- [ ] `matchAll : String -> String -> Seq` — all matches
- [ ] `replace : String -> String -> String -> String` — regex replace
- [ ] `split : String -> String -> Seq` — split by regex
- [ ] `test : String -> String -> Bool` — does pattern match?

Implementation: C shims using POSIX regex or PCRE2.

#### Std\Random — random numbers (sync)
- [ ] `int : Int -> Int -> Int` — random integer in range
- [ ] `float : Unit -> Float` — random float 0.0-1.0
- [ ] `choice : Seq -> a` — random element from sequence
- [ ] `shuffle : Seq -> Seq` — random permutation

#### Std\Time — date and time (sync)
- [ ] `now : Unit -> Int` — unix timestamp seconds
- [ ] `nowMillis : Unit -> Int` — unix timestamp milliseconds
- [ ] `format : Int -> String -> String` — format timestamp
- [ ] `parse : String -> String -> Int` — parse datetime string

### Language Features
- [ ] Generators/comprehensions — parsed but not compiled (`[x * 2 for x in list]`)
- [ ] Case guard expressions (`case x of n | n > 0 -> "positive" end`)
- [ ] `with` expression (resource management / auto-close)
- [ ] Module system improvements (wildcard import, re-exports)
- [ ] Error propagation operator (`?`) for Result types
- [ ] Lazy sequences / iterators

### Codegen Optimizations
- [ ] Inlining pass (AlwaysInline + function inlining for small functions)
- [ ] SROA (Scalar Replacement of Aggregates — decompose ADT structs)
- [ ] Loop optimizations (LICM, unrolling, vectorization)
- [ ] Dead argument elimination (unused wildcard params)
- [ ] LTO (link-time optimization across modules)
- [ ] Mutual tail call optimization
- [ ] Escape analysis (stack-allocate non-escaping ADTs)

### Memory Management
- [ ] Reference counting GC — every heap allocation currently leaks
- [ ] Arena allocator for short-lived computations

### Tooling
- [ ] LLVM debug info (DWARF) — source-level debugging
- [ ] Richer error messages — source line display, error spans
- [ ] Package manager / build system
- [ ] LSP server

### Type System
- [ ] Type annotations (optional signatures)
- [ ] Traits / type classes (dictionary passing)
- [ ] Cross-module generics (.yonai with AST)

## Completed
- Lexer, Parser, AST (newline-aware, juxtaposition, string interpolation)
- LLVM codegen with TypedValue (type-directed, CType tags, monomorphization)
- Lambda lifting, higher-order functions, partial application
- Pipe operators (`|>`, `<|`)
- Case expressions (integer, symbol, wildcard, head-tail, tuple, constructor, or-pattern)
- Symbol interning (i64 IDs, icmp eq comparison)
- Dict/Set construction with typed elements
- ADTs: non-recursive (flat struct) and recursive (heap nodes)
- ADT named fields (construction, dot access, functional update, named patterns)
- ADT constructors as first-class functions
- Exhaustiveness checking for ADT case expressions
- Interface files (.yonai) for cross-module type-safe linking
- Module compilation with C-ABI exports
- Module type metadata (pattern + body-based inference)
- Exception handling (raise/try/catch via setjmp/longjmp, ADT exceptions)
- Stack traces on unhandled exceptions (Linux/macOS/Windows)
- Async codegen (CType::PROMISE, thread pool, auto-await, parallel let)
- Multi-arg async support (thunk-based thread pool calls)
- Optimization passes (TCE, constant folding, GVN, DCE)
- `extern` / `extern async` for C FFI
- Compiler error messages with source locations and type validation
- REPL (compile-and-run mode)
- All 6 stdlib modules compile: Option (7), Result (8), List (17), Tuple (4), Range (6), Test (3)
