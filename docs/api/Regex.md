# Std.Regex

Regex — PCRE2-backed regular expressions.

```
let re = compile "[a-z]+" in
matches re "hello 123"
```

## Functions

### `extern`

```yona
extern yona_regex_compile : String -> Int
```

### `extern`

```yona
extern yona_regex_matches : Int -> String -> Bool
```

### `extern`

```yona
extern yona_regex_find : Int -> String -> Seq
```

### `extern`

```yona
extern yona_regex_findAll : Int -> String -> Seq
```

### `extern`

```yona
extern yona_regex_replace : Int -> String -> String -> String
```

### `extern`

```yona
extern yona_regex_replaceAll : Int -> String -> String -> String
```

### `extern`

```yona
extern yona_regex_split : Int -> String -> Seq
```

### `compile`

```yona
compile pattern = yona_regex_compile pattern
```

Compile a regex pattern. Returns a compiled handle (RC-managed, JIT).

### `matches`

```yona
matches re text = yona_regex_matches re text
```

Test if the pattern matches anywhere in the text.

### `find`

```yona
find re text = yona_regex_find re text
```

Find the first match. Returns [matched, group1, ...] or [].

### `findAll`

```yona
findAll re text = yona_regex_findAll re text
```

Find all non-overlapping matches. Returns [[matched, g1, ...], ...].

### `replace`

```yona
replace re text repl = yona_regex_replace re text repl
```

Replace the first match. Use $1, $2 for backreferences.

### `replaceAll`

```yona
replaceAll re text repl = yona_regex_replaceAll re text repl
```

Replace all matches.

### `split`

```yona
split re text = yona_regex_split re text
```

Split the text by pattern matches.

