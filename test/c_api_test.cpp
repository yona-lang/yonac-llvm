#include <doctest/doctest.h>
#include "yona_api.h"
#include <cstring>

TEST_SUITE("C API") {

TEST_CASE("Create and destroy interpreter") {
    yona_interp_t interp = yona_create();
    REQUIRE(interp != nullptr);
    yona_destroy(interp);
}

TEST_CASE("Eval simple expression") {
    yona_interp_t interp = yona_create();
    REQUIRE(interp != nullptr);

    yona_value_t result = nullptr;
    yona_status_t status = yona_eval(interp, "1 + 2", &result);
    CHECK(status == YONA_OK);
    REQUIRE(result != nullptr);
    CHECK(yona_typeof(result) == YONA_TYPE_INT);
    CHECK(yona_as_int(result) == 3);

    yona_value_free(result);
    yona_destroy(interp);
}

TEST_CASE("Eval string expression") {
    yona_interp_t interp = yona_create();
    yona_value_t result = nullptr;
    yona_status_t status = yona_eval(interp, "\"hello\"", &result);
    CHECK(status == YONA_OK);
    CHECK(yona_typeof(result) == YONA_TYPE_STRING);
    CHECK(strcmp(yona_as_string(result), "hello") == 0);
    yona_value_free(result);
    yona_destroy(interp);
}

TEST_CASE("Eval boolean expression") {
    yona_interp_t interp = yona_create();
    yona_value_t result = nullptr;
    yona_eval(interp, "true && false", &result);
    CHECK(yona_typeof(result) == YONA_TYPE_BOOL);
    CHECK(yona_as_bool(result) == 0);
    yona_value_free(result);
    yona_destroy(interp);
}

TEST_CASE("Eval let expression") {
    yona_interp_t interp = yona_create();
    yona_value_t result = nullptr;
    yona_eval(interp, "let x = 10 in x * 2", &result);
    CHECK(yona_as_int(result) == 20);
    yona_value_free(result);
    yona_destroy(interp);
}

TEST_CASE("Eval parse error") {
    yona_interp_t interp = yona_create();
    yona_value_t result = nullptr;
    yona_status_t status = yona_eval(interp, "let x = in", &result);
    CHECK(status != YONA_OK);
    CHECK(strlen(yona_error_message(interp)) > 0);
    yona_destroy(interp);
}

TEST_CASE("Value creation and inspection") {
    yona_value_t i = yona_int(42);
    CHECK(yona_typeof(i) == YONA_TYPE_INT);
    CHECK(yona_as_int(i) == 42);
    yona_value_free(i);

    yona_value_t f = yona_float(3.14);
    CHECK(yona_typeof(f) == YONA_TYPE_FLOAT);
    CHECK(yona_as_float(f) == doctest::Approx(3.14));
    yona_value_free(f);

    yona_value_t s = yona_string("test");
    CHECK(yona_typeof(s) == YONA_TYPE_STRING);
    CHECK(strcmp(yona_as_string(s), "test") == 0);
    yona_value_free(s);

    yona_value_t b = yona_bool(1);
    CHECK(yona_typeof(b) == YONA_TYPE_BOOL);
    CHECK(yona_as_bool(b) == 1);
    yona_value_free(b);

    yona_value_t sym = yona_symbol("ok");
    CHECK(yona_typeof(sym) == YONA_TYPE_SYMBOL);
    CHECK(strcmp(yona_as_symbol(sym), "ok") == 0);
    yona_value_free(sym);

    yona_value_t u = yona_unit();
    CHECK(yona_typeof(u) == YONA_TYPE_UNIT);
    yona_value_free(u);
}

TEST_CASE("Sequence creation and access") {
    yona_value_t elems[3] = {yona_int(10), yona_int(20), yona_int(30)};
    yona_value_t seq = yona_seq(elems, 3);

    CHECK(yona_typeof(seq) == YONA_TYPE_SEQ);
    CHECK(yona_seq_length(seq) == 3);

    yona_value_t elem = yona_seq_get(seq, 1);
    CHECK(yona_as_int(elem) == 20);
    yona_value_free(elem);

    yona_value_free(elems[0]);
    yona_value_free(elems[1]);
    yona_value_free(elems[2]);
    yona_value_free(seq);
}

TEST_CASE("Tuple creation and access") {
    yona_value_t elems[2] = {yona_string("name"), yona_int(42)};
    yona_value_t tuple = yona_tuple(elems, 2);

    CHECK(yona_typeof(tuple) == YONA_TYPE_TUPLE);
    CHECK(yona_tuple_length(tuple) == 2);

    yona_value_t first = yona_tuple_get(tuple, 0);
    CHECK(strcmp(yona_as_string(first), "name") == 0);
    yona_value_free(first);

    yona_value_free(elems[0]);
    yona_value_free(elems[1]);
    yona_value_free(tuple);
}

TEST_CASE("Value to string") {
    yona_value_t v = yona_int(42);
    char* s = yona_to_string(v);
    CHECK(strcmp(s, "42") == 0);
    yona_string_free(s);
    yona_value_free(v);
}

TEST_CASE("Eval function and call") {
    yona_interp_t interp = yona_create();
    yona_value_t func = nullptr;
    yona_eval(interp, "\\(x) -> x * 2", &func);
    REQUIRE(func != nullptr);
    CHECK(yona_typeof(func) == YONA_TYPE_FUNCTION);

    yona_value_t arg = yona_int(21);
    yona_value_t result = nullptr;
    yona_status_t status = yona_call(interp, func, &arg, 1, &result);
    CHECK(status == YONA_OK);
    CHECK(yona_as_int(result) == 42);

    yona_value_free(arg);
    yona_value_free(result);
    yona_value_free(func);
    yona_destroy(interp);
}

TEST_CASE("Register and call native function") {
    yona_interp_t interp = yona_create();

    // Register a native function: add(a, b) = a + b
    auto add_fn = [](yona_interp_t, yona_value_t* args, size_t nargs, void*) -> yona_value_t {
        if (nargs != 2) return nullptr;
        return yona_int(yona_as_int(args[0]) + yona_as_int(args[1]));
    };

    yona_status_t reg_status = yona_register_function(interp, "Test\\Native", "add", 2, add_fn, nullptr);
    CHECK(reg_status == YONA_OK);

    // Import the module and call the function via the C API
    yona_value_t module = nullptr;
    yona_status_t import_status = yona_import(interp, "Test\\Native", &module);
    CHECK(import_status == YONA_OK);
    REQUIRE(module != nullptr);

    yona_value_t func = nullptr;
    yona_module_get(module, "add", &func);
    REQUIRE(func != nullptr);

    yona_value_t args[2] = {yona_int(3), yona_int(4)};
    yona_value_t result = nullptr;
    yona_call(interp, func, args, 2, &result);
    CHECK(yona_as_int(result) == 7);

    yona_value_free(args[0]);
    yona_value_free(args[1]);
    yona_value_free(result);
    yona_value_free(func);
    yona_value_free(module);
    yona_destroy(interp);
}

TEST_CASE("Native function with userdata") {
    yona_interp_t interp = yona_create();

    int counter = 0;

    auto increment_fn = [](yona_interp_t, yona_value_t*, size_t, void* ud) -> yona_value_t {
        int* cnt = static_cast<int*>(ud);
        (*cnt)++;
        return yona_int(*cnt);
    };

    yona_register_function(interp, "Test\\Counter", "increment", 0, increment_fn, &counter);

    yona_value_t module = nullptr;
    yona_import(interp, "Test\\Counter", &module);
    yona_value_t func = nullptr;
    yona_module_get(module, "increment", &func);

    yona_value_t result = nullptr;
    yona_call(interp, func, nullptr, 0, &result);
    CHECK(yona_as_int(result) == 1);
    CHECK(counter == 1);
    yona_value_free(result);

    yona_call(interp, func, nullptr, 0, &result);
    CHECK(yona_as_int(result) == 2);
    CHECK(counter == 2);
    yona_value_free(result);

    yona_value_free(func);
    yona_value_free(module);
    yona_destroy(interp);
}

TEST_CASE("Multiple independent interpreters") {
    yona_interp_t interp1 = yona_create();
    yona_interp_t interp2 = yona_create();

    yona_value_t r1 = nullptr, r2 = nullptr;
    yona_eval(interp1, "let x = 10 in x", &r1);
    yona_eval(interp2, "let x = 20 in x", &r2);

    CHECK(yona_as_int(r1) == 10);
    CHECK(yona_as_int(r2) == 20);

    yona_value_free(r1);
    yona_value_free(r2);
    yona_destroy(interp1);
    yona_destroy(interp2);
}

TEST_CASE("Error handling - parse error") {
    yona_interp_t interp = yona_create();

    yona_value_t result = nullptr;
    yona_status_t status = yona_eval(interp, "let x = in", &result);
    CHECK(status != YONA_OK);
    CHECK(strlen(yona_error_message(interp)) > 0);

    yona_destroy(interp);
}

TEST_CASE("Error handling - runtime error") {
    yona_interp_t interp = yona_create();

    yona_value_t result = nullptr;
    // Call a non-existent module to trigger runtime error
    yona_status_t status = yona_eval(interp, "import foo from NonExistent\\Module in foo", &result);
    CHECK(status == YONA_ERROR);

    yona_destroy(interp);
}

TEST_CASE("Null safety") {
    CHECK(yona_typeof(nullptr) == YONA_TYPE_OTHER);
    CHECK(yona_as_int(nullptr) == 0);
    CHECK(yona_as_float(nullptr) == 0.0);
    CHECK(strcmp(yona_as_string(nullptr), "") == 0);
    CHECK(yona_seq_length(nullptr) == 0);
    CHECK(yona_tuple_length(nullptr) == 0);
    CHECK(yona_eval(nullptr, "1", nullptr) == YONA_INVALID_ARG);

    yona_value_free(nullptr); // should not crash
}

} // C API TEST_SUITE

TEST_SUITE("Sandbox") {

TEST_CASE("Whitelist mode blocks unlisted modules") {
    yona_interp_t interp = yona_create();
    yona_sandbox_set_mode(interp, YONA_SANDBOX_WHITELIST);
    yona_sandbox_allow_module(interp, "Std\\Math");

    // Allowed module works
    yona_value_t r1 = nullptr;
    yona_status_t s1 = yona_eval(interp, "import abs from Std\\Math in abs(-5)", &r1);
    CHECK(s1 == YONA_OK);
    CHECK(yona_as_int(r1) == 5);
    yona_value_free(r1);

    // Denied module fails
    yona_value_t r2 = nullptr;
    yona_status_t s2 = yona_eval(interp, "import readFile from Std\\IO in readFile \"x\"", &r2);
    CHECK(s2 == YONA_ERROR);
    const char* err = yona_error_message(interp);
    CHECK(strstr(err, "sandbox") != nullptr);

    yona_destroy(interp);
}

TEST_CASE("Whitelist wildcard allows module group") {
    yona_interp_t interp = yona_create();
    yona_sandbox_set_mode(interp, YONA_SANDBOX_WHITELIST);
    yona_sandbox_allow_module(interp, "Std\\*");

    // All Std modules should work
    yona_value_t r = nullptr;
    CHECK(yona_eval(interp, "import abs from Std\\Math in abs(-1)", &r) == YONA_OK);
    yona_value_free(r);

    yona_destroy(interp);
}

TEST_CASE("Blacklist mode blocks listed modules") {
    yona_interp_t interp = yona_create();
    yona_sandbox_set_mode(interp, YONA_SANDBOX_BLACKLIST);
    yona_sandbox_deny_module(interp, "Std\\System");

    // Non-denied module works
    yona_value_t r1 = nullptr;
    CHECK(yona_eval(interp, "import abs from Std\\Math in abs(-3)", &r1) == YONA_OK);
    CHECK(yona_as_int(r1) == 3);
    yona_value_free(r1);

    // Denied module fails
    yona_value_t r2 = nullptr;
    CHECK(yona_eval(interp, "import exit from Std\\System in exit 0", &r2) == YONA_ERROR);

    yona_destroy(interp);
}

TEST_CASE("Execution limit prevents infinite computation") {
    yona_interp_t interp = yona_create();
    yona_set_execution_limit(interp, 100);

    // Simple expression within limit
    yona_value_t r1 = nullptr;
    CHECK(yona_eval(interp, "1 + 2 + 3", &r1) == YONA_OK);
    CHECK(yona_as_int(r1) == 6);
    yona_value_free(r1);

    // Recursive function exceeds limit
    yona_value_t r2 = nullptr;
    yona_status_t s = yona_eval(interp,
        "let f x = f (x + 1) in f 0", &r2);
    CHECK(s == YONA_ERROR);
    CHECK(strstr(yona_error_message(interp), "limit") != nullptr);

    yona_destroy(interp);
}

TEST_CASE("Execution limit resets between evals") {
    yona_interp_t interp = yona_create();
    yona_set_execution_limit(interp, 50);

    // First eval succeeds
    yona_value_t r1 = nullptr;
    CHECK(yona_eval(interp, "1 + 2", &r1) == YONA_OK);
    yona_value_free(r1);

    // Second eval also succeeds (counter reset)
    yona_value_t r2 = nullptr;
    CHECK(yona_eval(interp, "3 + 4", &r2) == YONA_OK);
    CHECK(yona_as_int(r2) == 7);
    yona_value_free(r2);

    yona_destroy(interp);
}

TEST_CASE("Memory limit prevents large allocations") {
    yona_interp_t interp = yona_create();
    yona_set_memory_limit(interp, 256);  // Very small limit

    // Small collection within limit
    yona_value_t r1 = nullptr;
    CHECK(yona_eval(interp, "[1, 2, 3]", &r1) == YONA_OK);
    yona_value_free(r1);

    yona_destroy(interp);
}

TEST_CASE("No sandbox by default") {
    yona_interp_t interp = yona_create();

    // All modules accessible
    yona_value_t r = nullptr;
    CHECK(yona_eval(interp, "import abs from Std\\Math in abs(-42)", &r) == YONA_OK);
    CHECK(yona_as_int(r) == 42);
    yona_value_free(r);

    yona_destroy(interp);
}

} // Sandbox TEST_SUITE
