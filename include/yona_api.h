/*
 * Yona Language C Embedding API
 *
 * This header provides a stable C interface for embedding the Yona
 * interpreter in host applications. All Yona internals are hidden
 * behind opaque handles.
 *
 * Usage:
 *   yona_interp_t interp = yona_create();
 *   yona_value_t result;
 *   if (yona_eval(interp, "1 + 2", &result) == YONA_OK) {
 *       printf("result: %d\n", yona_as_int(result));
 *       yona_value_free(result);
 *   }
 *   yona_destroy(interp);
 *
 * Thread safety: each yona_interp_t is independent. Do not share
 * an interpreter across threads without external synchronization.
 */

#ifndef YONA_API_H
#define YONA_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
  #ifdef YONA_BUILDING_DLL
    #define YONA_CAPI __declspec(dllexport)
  #else
    #define YONA_CAPI __declspec(dllimport)
  #endif
#else
  #define YONA_CAPI __attribute__((visibility("default")))
#endif

/* ===== Opaque handle types ===== */

typedef struct YonaInterpreter* yona_interp_t;
typedef struct YonaValue*       yona_value_t;

/* ===== Status codes ===== */

typedef enum {
    YONA_OK = 0,
    YONA_ERROR,           /* General runtime error */
    YONA_PARSE_ERROR,     /* Source code parse failure */
    YONA_TYPE_ERROR,      /* Type mismatch */
    YONA_NOT_FOUND,       /* Module/function not found */
    YONA_INVALID_ARG      /* Invalid argument to API function */
} yona_status_t;

/* ===== Value type tags ===== */

typedef enum {
    YONA_TYPE_INT = 0,
    YONA_TYPE_FLOAT,
    YONA_TYPE_STRING,
    YONA_TYPE_BOOL,
    YONA_TYPE_UNIT,
    YONA_TYPE_SYMBOL,
    YONA_TYPE_SEQ,
    YONA_TYPE_SET,
    YONA_TYPE_DICT,
    YONA_TYPE_TUPLE,
    YONA_TYPE_FUNCTION,
    YONA_TYPE_MODULE,
    YONA_TYPE_RECORD,
    YONA_TYPE_BYTE,
    YONA_TYPE_CHAR,
    YONA_TYPE_OTHER
} yona_type_t;

/* ===== Native function callback ===== */

/*
 * Signature for host-provided native functions.
 * The callback receives the interpreter, arguments array, argument count,
 * and a user-provided context pointer.
 * Must return a yona_value_t (caller takes ownership) or NULL on error
 * (set error message via yona_set_error).
 */
typedef yona_value_t (*yona_native_fn)(yona_interp_t interp,
                                       yona_value_t* args, size_t nargs,
                                       void* userdata);

/* ===== Interpreter lifecycle ===== */

/* Create a new interpreter instance. Returns NULL on failure. */
YONA_CAPI yona_interp_t yona_create(void);

/* Destroy an interpreter and free all associated resources. */
YONA_CAPI void yona_destroy(yona_interp_t interp);

/* ===== Evaluation ===== */

/* Evaluate a Yona expression string. Result is written to *out. */
YONA_CAPI yona_status_t yona_eval(yona_interp_t interp, const char* code, yona_value_t* out);

/* Evaluate a Yona source file. Result is written to *out. */
YONA_CAPI yona_status_t yona_eval_file(yona_interp_t interp, const char* path, yona_value_t* out);

/* ===== Value creation (host → Yona) ===== */

YONA_CAPI yona_value_t yona_int(int value);
YONA_CAPI yona_value_t yona_float(double value);
YONA_CAPI yona_value_t yona_string(const char* value);
YONA_CAPI yona_value_t yona_bool(int value);
YONA_CAPI yona_value_t yona_symbol(const char* name);
YONA_CAPI yona_value_t yona_unit(void);
YONA_CAPI yona_value_t yona_seq(yona_value_t* elements, size_t count);
YONA_CAPI yona_value_t yona_tuple(yona_value_t* elements, size_t count);

/* ===== Value inspection (Yona → host) ===== */

/* Get the type of a value. */
YONA_CAPI yona_type_t yona_typeof(yona_value_t value);

/* Extract primitive values. Behavior undefined if type doesn't match. */
YONA_CAPI int         yona_as_int(yona_value_t value);
YONA_CAPI double      yona_as_float(yona_value_t value);
YONA_CAPI const char* yona_as_string(yona_value_t value);  /* valid until value freed */
YONA_CAPI int         yona_as_bool(yona_value_t value);
YONA_CAPI const char* yona_as_symbol(yona_value_t value);  /* valid until value freed */

/* Collection access. Returned values must be freed by caller. */
YONA_CAPI size_t      yona_seq_length(yona_value_t seq);
YONA_CAPI yona_value_t yona_seq_get(yona_value_t seq, size_t index);
YONA_CAPI size_t      yona_tuple_length(yona_value_t tuple);
YONA_CAPI yona_value_t yona_tuple_get(yona_value_t tuple, size_t index);

/* Convert any value to its string representation. Caller must free result. */
YONA_CAPI char* yona_to_string(yona_value_t value);

/* ===== Function calling ===== */

/* Call a Yona function with arguments. Result written to *out. */
YONA_CAPI yona_status_t yona_call(yona_interp_t interp, yona_value_t func,
                                   yona_value_t* args, size_t nargs,
                                   yona_value_t* out);

/* ===== Module access ===== */

/* Import a module by FQN (e.g., "Std\\Math"). Result written to *out. */
YONA_CAPI yona_status_t yona_import(yona_interp_t interp, const char* module_fqn,
                                     yona_value_t* out);

/* Get an exported function from a module by name. Result written to *out. */
YONA_CAPI yona_status_t yona_module_get(yona_value_t module, const char* name,
                                         yona_value_t* out);

/* ===== Native function registration ===== */

/*
 * Register a native C function that Yona code can call via import.
 * module_fqn: module path, e.g., "MyApp\\Handlers"
 * name: function name within the module
 * arity: number of arguments the function expects
 * fn: callback function
 * userdata: opaque pointer passed to every invocation of fn
 */
YONA_CAPI yona_status_t yona_register_function(yona_interp_t interp,
                                                const char* module_fqn,
                                                const char* name,
                                                size_t arity,
                                                yona_native_fn fn,
                                                void* userdata);

/* ===== Configuration ===== */

/* Add a directory to the module search path. */
YONA_CAPI yona_status_t yona_add_module_path(yona_interp_t interp, const char* path);

/* ===== Sandboxing ===== */

typedef enum {
    YONA_SANDBOX_NONE = 0,      /* No restrictions (default) */
    YONA_SANDBOX_WHITELIST,     /* Only allowed modules can be imported */
    YONA_SANDBOX_BLACKLIST      /* All modules except denied ones */
} yona_sandbox_mode_t;

/* Set the sandbox mode. */
YONA_CAPI yona_status_t yona_sandbox_set_mode(yona_interp_t interp, yona_sandbox_mode_t mode);

/* Allow a module (whitelist mode). Supports wildcards: "Std\\*" */
YONA_CAPI yona_status_t yona_sandbox_allow_module(yona_interp_t interp, const char* pattern);

/* Deny a module (blacklist mode). Supports wildcards: "Std\\IO" */
YONA_CAPI yona_status_t yona_sandbox_deny_module(yona_interp_t interp, const char* pattern);

/* Set maximum execution steps (0 = unlimited). Resets on each yona_eval. */
YONA_CAPI yona_status_t yona_set_execution_limit(yona_interp_t interp, size_t max_steps);

/* Set maximum memory usage in bytes (0 = unlimited). */
YONA_CAPI yona_status_t yona_set_memory_limit(yona_interp_t interp, size_t max_bytes);

/* ===== Error handling ===== */

/* Get the error message from the last failed operation. */
YONA_CAPI const char* yona_error_message(yona_interp_t interp);

/* Set an error message (for use inside native function callbacks). */
YONA_CAPI void yona_set_error(yona_interp_t interp, const char* message);

/* ===== Memory management ===== */

/* Free a value returned by the API. */
YONA_CAPI void yona_value_free(yona_value_t value);

/* Free a string returned by yona_to_string. */
YONA_CAPI void yona_string_free(char* str);

#ifdef __cplusplus
}
#endif

#endif /* YONA_API_H */
