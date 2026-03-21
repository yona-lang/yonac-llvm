# C Embedding API

The Yona C API (`yona_api.h`) provides a stable C interface for embedding the Yona interpreter in host applications. All Yona internals are hidden behind opaque handles, ensuring ABI stability across versions.

## Quick Start

```c
#include "yona_api.h"

int main() {
    yona_interp_t interp = yona_create();

    yona_value_t result;
    if (yona_eval(interp, "1 + 2", &result) == YONA_OK) {
        printf("result: %d\n", yona_as_int(result));  // 3
        yona_value_free(result);
    }

    yona_destroy(interp);
    return 0;
}
```

## API Reference

### Interpreter Lifecycle

```c
yona_interp_t yona_create(void);       // Create new interpreter
void yona_destroy(yona_interp_t);       // Destroy and free all resources
```

Each interpreter is independent. Multiple interpreters can coexist. Do not share an interpreter across threads without external synchronization.

### Evaluation

```c
yona_status_t yona_eval(yona_interp_t, const char* code, yona_value_t* out);
yona_status_t yona_eval_file(yona_interp_t, const char* path, yona_value_t* out);
```

Returns `YONA_OK` on success, error code on failure. The result value is written to `*out` and must be freed with `yona_value_free()`.

### Value Creation (Host → Yona)

```c
yona_value_t yona_int(int value);
yona_value_t yona_float(double value);
yona_value_t yona_string(const char* value);
yona_value_t yona_bool(int value);         // 0 = false, non-zero = true
yona_value_t yona_symbol(const char* name);
yona_value_t yona_unit(void);
yona_value_t yona_seq(yona_value_t* elements, size_t count);
yona_value_t yona_tuple(yona_value_t* elements, size_t count);
```

All returned values must be freed with `yona_value_free()`.

### Value Inspection (Yona → Host)

```c
yona_type_t   yona_typeof(yona_value_t);
int           yona_as_int(yona_value_t);
double        yona_as_float(yona_value_t);
const char*   yona_as_string(yona_value_t);   // valid until value freed
int           yona_as_bool(yona_value_t);
const char*   yona_as_symbol(yona_value_t);
size_t        yona_seq_length(yona_value_t);
yona_value_t  yona_seq_get(yona_value_t, size_t index);  // must free
size_t        yona_tuple_length(yona_value_t);
yona_value_t  yona_tuple_get(yona_value_t, size_t index); // must free
char*         yona_to_string(yona_value_t);  // free with yona_string_free
```

Type tags:
```c
YONA_TYPE_INT, YONA_TYPE_FLOAT, YONA_TYPE_STRING, YONA_TYPE_BOOL,
YONA_TYPE_UNIT, YONA_TYPE_SYMBOL, YONA_TYPE_SEQ, YONA_TYPE_SET,
YONA_TYPE_DICT, YONA_TYPE_TUPLE, YONA_TYPE_FUNCTION, YONA_TYPE_MODULE,
YONA_TYPE_RECORD, YONA_TYPE_BYTE, YONA_TYPE_CHAR, YONA_TYPE_OTHER
```

### Function Calling

```c
yona_status_t yona_call(yona_interp_t, yona_value_t func,
                         yona_value_t* args, size_t nargs, yona_value_t* out);
```

Call a Yona function obtained from `yona_eval` or `yona_module_get`.

### Module Access

```c
yona_status_t yona_import(yona_interp_t, const char* fqn, yona_value_t* out);
yona_status_t yona_module_get(yona_value_t module, const char* name, yona_value_t* out);
```

Example:
```c
yona_value_t math_module;
yona_import(interp, "Std\\Math", &math_module);

yona_value_t abs_func;
yona_module_get(math_module, "abs", &abs_func);

yona_value_t arg = yona_int(-42);
yona_value_t result;
yona_call(interp, abs_func, &arg, 1, &result);
// yona_as_int(result) == 42
```

### Native Function Registration

Register C functions that Yona code can call via `import`:

```c
typedef yona_value_t (*yona_native_fn)(yona_interp_t, yona_value_t* args,
                                       size_t nargs, void* userdata);

yona_status_t yona_register_function(yona_interp_t, const char* module_fqn,
                                      const char* name, size_t arity,
                                      yona_native_fn fn, void* userdata);
```

Example (registering a host function):
```c
yona_value_t my_notify(yona_interp_t interp, yona_value_t* args,
                        size_t nargs, void* userdata) {
    const char* channel = yona_as_string(args[0]);
    const char* message = yona_as_string(args[1]);
    // ... send notification via host application ...
    return yona_unit();
}

yona_register_function(interp, "App\\Notify", "send", 2, my_notify, app_context);
```

Yona code can then use it:
```yona
import send from App\Notify in
  send "team-channel" "Build completed"
```

The `userdata` pointer is passed to every invocation, enabling stateful callbacks without global variables.

### Sandboxing

```c
// Modes
yona_sandbox_set_mode(interp, YONA_SANDBOX_WHITELIST);  // or BLACKLIST, NONE

// Module access control (supports wildcards)
yona_sandbox_allow_module(interp, "Std\\Math");
yona_sandbox_allow_module(interp, "Std\\List");
yona_sandbox_allow_module(interp, "App\\*");  // all App modules

yona_sandbox_deny_module(interp, "Std\\System");  // for blacklist mode

// Resource limits
yona_set_execution_limit(interp, 100000);   // max steps per eval
yona_set_memory_limit(interp, 10485760);    // 10 MB
```

### Error Handling

```c
yona_status_t status = yona_eval(interp, code, &result);
if (status != YONA_OK) {
    printf("Error: %s\n", yona_error_message(interp));
}
```

Status codes: `YONA_OK`, `YONA_ERROR`, `YONA_PARSE_ERROR`, `YONA_TYPE_ERROR`, `YONA_NOT_FOUND`, `YONA_INVALID_ARG`.

Inside native function callbacks, use `yona_set_error(interp, message)` and return `NULL` to signal an error.

### Memory Management

- Values from API functions: free with `yona_value_free()`
- Strings from `yona_to_string()`: free with `yona_string_free()`
- `NULL` is safe to pass to free functions
- String pointers from `yona_as_string()` are valid until the value is freed

## Thread Safety

Each `yona_interp_t` is fully independent. Multiple interpreters can run in separate threads. Do not use a single interpreter from multiple threads without a mutex.

## Linking

The C API is part of `libyona_lib.so` (shared) or `libyona_lib_static.a` (static). Link with:

```bash
gcc my_app.c -lyona_lib -L/path/to/build -I/path/to/include
```
