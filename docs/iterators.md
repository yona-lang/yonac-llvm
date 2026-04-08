# Iterators & Streaming I/O

## Overview

Yona's `Iterator` type enables streaming data processing — reading files, splitting strings, or traversing collections element by element without loading everything into memory.

```yona
type Iterator a = Iterator (() -> Option a)
```

An Iterator wraps a function that returns `Some element` on each call, or `None` when exhausted. Generators automatically detect Iterator sources and emit streaming loops.

## Usage

### File Streaming

```yona
import readLines from Std\File, length from Std\String in
-- Streams line-by-line with O(64KB) memory, not O(file_size)
[length line for line = readLines "large_file.txt"]
```

`readLines` returns an `Iterator String` backed by a 64KB buffered reader. Each generator iteration calls the iterator's `next()` function, which reads from the buffer and refills from disk as needed.

### String Operations

```yona
import chars, split from Std\String in

-- Character codes of "hello": [104, 101, 108, 108, 111]
[c for c = chars "hello"]

-- Split and process: ["alpha", "beta", "gamma"]
[length s for s = split "," "alpha,beta,gamma"]
```

`chars` yields character codes (integers). `split` and `lines` yield substrings on demand.

## How It Works

### Generator Codegen

When a generator's source has `CType::ADT` (indicating an Iterator), the codegen emits:

```
loop:
  option = closure.next()     -- call the iterator's closure
  if option.tag == None: goto done
  elem = option.value         -- extract from Some
  result = reducer(elem)      -- evaluate the generator body
  seq = seq_snoc(seq, result) -- append to growing result
  goto loop
done:
  return seq
```

Key properties:
- **Dynamic growth**: result seq grows via `seq_snoc` (O(1) amortized append)
- **No size limit**: works with any number of elements
- **Streaming**: only one element is in memory at a time

### Seq vs Iterator Sources

The generator codegen dispatches based on the source type:

| Source Type | Loop Strategy | Memory |
|-------------|--------------|--------|
| `Seq` (CType::SEQ) | head/tail iteration | O(n) — seq materialized |
| `Iterator` (CType::ADT) | next() call loop | O(1) — streaming |

### File Line Iterator (Runtime)

`readLines` creates a `yona_iterator_t` in C with:
- 64KB read buffer (`LINE_ITER_BUF_SIZE`)
- File descriptor from `open()`
- Line scanning: finds `\n` in buffer, yields substring
- Buffer refill: when scan reaches end, reads next 64KB chunk
- EOF: returns `None`

The iterator allocates one line string per `next()` call. The 64KB buffer is reused across calls.

## Stdlib Iterator Functions

| Module | Function | Returns | Elements |
|--------|----------|---------|----------|
| Std\File | `readLines path` | Iterator String | File lines (64KB buffered) |
| Std\String | `chars str` | Iterator Int | Character codes |
| Std\String | `split delim str` | Iterator String | Substrings |
| Std\String | `lines str` | Iterator String | Lines (split by \n) |

## Creating Custom Iterators

```yona
-- Create an iterator that counts from n
let countFrom n =
    let state = n in
    Iterator (\() -> Some state)  -- NOTE: needs mutable state (future work)
in countFrom 1
```

Currently, custom iterators require C runtime backing for mutable state. Pure Yona iterators need closure-based state threading (future: cooperative suspension via effects).

## Performance

For small collections (<32 elements), both Seq and Iterator paths are fast. For large data:

| Scenario | Seq (eager) | Iterator (streaming) |
|----------|-------------|---------------------|
| 50MB file, count lines | O(50MB) memory | O(64KB) memory |
| 1M-char string, process chars | O(1M) allocations | O(1) per char |
| Split 10K-field CSV | O(10K) strings upfront | O(1) per field |

## Limitations

- **No random access**: iterators are sequential (forward-only)
- **No length**: can't know the total count without exhausting the iterator
- **Single use**: once exhausted, an iterator can't be rewound
- **Dict/Set iteration**: not yet implemented (needs HAMT traversal iterator)
- **Result seq limit**: generated result is a Seq (materialized), not lazy
