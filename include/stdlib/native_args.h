#pragma once

#include "runtime.h"
#include "common.h"
#include <string>
#include <vector>
#include <optional>
#include <typeinfo>

namespace yona::stdlib {

using namespace yona::interp::runtime;

// Helper to get runtime type name as string
inline std::string get_runtime_type_name(RuntimeObjectType type) {
    switch (type) {
        case Int: return "integer";
        case Float: return "float";
        case String: return "string";
        case Bool: return "boolean";
        case Symbol: return "symbol";
        case Char: return "character";
        case Byte: return "byte";
        case Unit: return "unit";
        case Tuple: return "tuple";
        case Seq: return "sequence";
        case Set: return "set";
        case Dict: return "dictionary";
        case Function: return "function";
        case Module: return "module";
        case Record: return "record";
        default: return "unknown";
    }
}

// Template to get expected type name
template<typename T>
std::string get_expected_type_name() {
    if constexpr (std::is_same_v<T, int>) return "an integer";
    else if constexpr (std::is_same_v<T, double>) return "a number";
    else if constexpr (std::is_same_v<T, std::string>) return "a string";
    else if constexpr (std::is_same_v<T, bool>) return "a boolean";
    else if constexpr (std::is_same_v<T, std::byte>) return "a byte";
    else return "a value";
}

// Base argument extractor template
template<typename T>
struct ArgExtractor {
    static std::optional<T> extract(const RuntimeObjectPtr& arg);
};

// Specializations for common types
template<>
struct ArgExtractor<int> {
    static std::optional<int> extract(const RuntimeObjectPtr& arg) {
        if (arg->type == Int) {
            return arg->get<int>();
        }
        return std::nullopt;
    }
};

template<>
struct ArgExtractor<double> {
    static std::optional<double> extract(const RuntimeObjectPtr& arg) {
        if (arg->type == Float) {
            return arg->get<double>();
        } else if (arg->type == Int) {
            return static_cast<double>(arg->get<int>());
        }
        return std::nullopt;
    }
};

template<>
struct ArgExtractor<std::string> {
    static std::optional<std::string> extract(const RuntimeObjectPtr& arg) {
        if (arg->type == String) {
            return arg->get<std::string>();
        }
        return std::nullopt;
    }
};

template<>
struct ArgExtractor<bool> {
    static std::optional<bool> extract(const RuntimeObjectPtr& arg) {
        if (arg->type == Bool) {
            return arg->get<bool>();
        }
        return std::nullopt;
    }
};

template<>
struct ArgExtractor<std::byte> {
    static std::optional<std::byte> extract(const RuntimeObjectPtr& arg) {
        if (arg->type == Byte) {
            return arg->get<std::byte>();
        }
        return std::nullopt;
    }
};

// Helper class for argument validation and extraction
class NativeArgs {
private:
    const std::vector<RuntimeObjectPtr>& args;
    const std::string& func_name;
    size_t current_index = 0;

public:
    NativeArgs(const std::vector<RuntimeObjectPtr>& args, const std::string& func_name)
        : args(args), func_name(func_name) {}

    // Check if we have at least n arguments
    void require_count(size_t n) const {
        if (args.size() < n) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                func_name + " expects " + std::to_string(n) + " argument(s), got " + std::to_string(args.size()));
        }
    }

    // Check exact argument count
    void require_exact_count(size_t n) const {
        if (args.size() != n) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                func_name + " expects exactly " + std::to_string(n) + " argument(s), got " + std::to_string(args.size()));
        }
    }

    // Get argument at specific index with type checking
    template<typename T>
    T get(size_t index, const std::string& param_name = "") {
        if (index >= args.size()) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                func_name + ": missing argument" + (param_name.empty() ? "" : " '" + param_name + "'"));
        }

        auto result = ArgExtractor<T>::extract(args[index]);
        if (!result.has_value()) {
            std::string type_name = get_expected_type_name<T>();
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                func_name + ": " + (param_name.empty() ? "argument " + std::to_string(index) : "'" + param_name + "'") +
                " must be " + type_name + ", got " + get_runtime_type_name(args[index]->type));
        }
        return result.value();
    }

    // Get next argument in sequence
    template<typename T>
    T next(const std::string& param_name = "") {
        T result = get<T>(current_index, param_name);
        current_index++;
        return result;
    }

    // Get optional argument
    template<typename T>
    std::optional<T> get_optional(size_t index) {
        if (index >= args.size()) {
            return std::nullopt;
        }
        return ArgExtractor<T>::extract(args[index]);
    }

    // Get numeric argument (int or float)
    double get_numeric(size_t index, const std::string& param_name = "") {
        if (index >= args.size()) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                func_name + ": missing numeric argument" + (param_name.empty() ? "" : " '" + param_name + "'"));
        }

        const auto& arg = args[index];
        if (arg->type == Int) {
            return static_cast<double>(arg->get<int>());
        } else if (arg->type == Float) {
            return arg->get<double>();
        } else {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                func_name + ": " + (param_name.empty() ? "argument " + std::to_string(index) : "'" + param_name + "'") +
                " must be numeric");
        }
    }

    // Get raw argument
    RuntimeObjectPtr get_raw(size_t index) const {
        if (index >= args.size()) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                func_name + ": missing argument at index " + std::to_string(index));
        }
        return args[index];
    }

    // Check if argument at index is of specific type
    bool is_type(size_t index, RuntimeObjectType type) const {
        return index < args.size() && args[index]->type == type;
    }

    size_t count() const { return args.size(); }
    bool empty() const { return args.empty(); }
};

// Convenience macros for common patterns

// For functions with no arguments
#define NATIVE_ARGS_NONE(func_name) \
    NativeArgs nargs(args, func_name); \
    nargs.require_exact_count(0);

// For functions with fixed argument count
#define NATIVE_ARGS(func_name, count) \
    NativeArgs nargs(args, func_name); \
    nargs.require_count(count);

#define NATIVE_ARGS_EXACT(func_name, count) \
    NativeArgs nargs(args, func_name); \
    nargs.require_exact_count(count);

// For extracting typed arguments
#define ARG_INT(index) nargs.get<int>(index)
#define ARG_DOUBLE(index) nargs.get<double>(index)
#define ARG_STRING(index) nargs.get<std::string>(index)
#define ARG_BOOL(index) nargs.get<bool>(index)
#define ARG_NUMERIC(index) nargs.get_numeric(index)
#define ARG_RAW(index) nargs.get_raw(index)

// For sequential argument extraction
#define NEXT_INT(name) auto name = nargs.next<int>(#name)
#define NEXT_DOUBLE(name) auto name = nargs.next<double>(#name)
#define NEXT_STRING(name) auto name = nargs.next<std::string>(#name)
#define NEXT_BOOL(name) auto name = nargs.next<bool>(#name)
#define NEXT_NUMERIC(name) auto name = nargs.get_numeric(nargs.current_index++, #name)

// Advanced macros for complete function signatures

// For functions that take no arguments and return Unit
#define NATIVE_FUNC_VOID(func_name) \
    NATIVE_ARGS_NONE(func_name);

// For functions that take no arguments and return a value
#define NATIVE_FUNC_0(func_name) \
    NATIVE_ARGS_NONE(func_name);

// For functions that take 1 argument
#define NATIVE_FUNC_1(func_name, type1, name1) \
    NATIVE_ARGS_EXACT(func_name, 1); \
    NEXT_##type1(name1);

// For functions that take 2 arguments
#define NATIVE_FUNC_2(func_name, type1, name1, type2, name2) \
    NATIVE_ARGS_EXACT(func_name, 2); \
    NEXT_##type1(name1); \
    NEXT_##type2(name2);

// For functions that take 3 arguments
#define NATIVE_FUNC_3(func_name, type1, name1, type2, name2, type3, name3) \
    NATIVE_ARGS_EXACT(func_name, 3); \
    NEXT_##type1(name1); \
    NEXT_##type2(name2); \
    NEXT_##type3(name3);

// For optional arguments
#define NATIVE_FUNC_OPT_1(func_name, min_args) \
    NativeArgs nargs(args, func_name); \
    nargs.require_count(min_args);

// Common patterns for specific function types

// Math function that takes one numeric argument
#define MATH_FUNC_1(func_name) \
    NATIVE_ARGS_EXACT(func_name, 1); \
    double x = ARG_NUMERIC(0);

// Math function that takes two numeric arguments
#define MATH_FUNC_2(func_name) \
    NATIVE_ARGS_EXACT(func_name, 2); \
    double x = ARG_NUMERIC(0); \
    double y = ARG_NUMERIC(1);

// IO function that takes a filename
#define IO_FUNC_FILE(func_name) \
    NATIVE_FUNC_1(func_name, STRING, filename)

// IO function that takes filename and content
#define IO_FUNC_FILE_CONTENT(func_name) \
    NATIVE_FUNC_2(func_name, STRING, filename, STRING, content)

// System function that takes an environment variable name
#define SYS_FUNC_ENV(func_name) \
    NATIVE_FUNC_1(func_name, STRING, var_name)

// Conditional argument handling
#define IF_ARG_EXISTS(index, type, name, action) \
    if (nargs.count() > index && nargs.is_type(index, type)) { \
        auto name = ARG_##type(index); \
        action \
    }

// For functions with optional arguments at the end
#define OPTIONAL_ARG(index, type, name, default_value) \
    auto name = (nargs.count() > index) ? nargs.get<type>(index) : default_value;

// Helper functions for creating return values
inline RuntimeObjectPtr make_int(int value) {
    return std::make_shared<RuntimeObject>(Int, value);
}

inline RuntimeObjectPtr make_float(double value) {
    return std::make_shared<RuntimeObject>(Float, value);
}

inline RuntimeObjectPtr make_string(const std::string& value) {
    return std::make_shared<RuntimeObject>(String, value);
}

inline RuntimeObjectPtr make_bool(bool value) {
    return std::make_shared<RuntimeObject>(Bool, value);
}

inline RuntimeObjectPtr make_unit() {
    return std::make_shared<RuntimeObject>(Unit, nullptr);
}

inline RuntimeObjectPtr make_byte(std::byte value) {
    return std::make_shared<RuntimeObject>(Byte, value);
}

inline RuntimeObjectPtr make_char(char value) {
    return std::make_shared<RuntimeObject>(Char, value);
}

inline RuntimeObjectPtr make_symbol(const std::string& name) {
    return std::make_shared<RuntimeObject>(Symbol, std::make_shared<SymbolValue>(SymbolValue{name}));
}

// Helper for creating Result-like tuples (symbol, value)
inline RuntimeObjectPtr make_result(const std::string& tag, const RuntimeObjectPtr& value) {
    auto tuple = std::make_shared<TupleValue>();
    tuple->fields.push_back(make_symbol(tag));
    tuple->fields.push_back(value);
    return std::make_shared<RuntimeObject>(Tuple, tuple);
}

inline RuntimeObjectPtr make_ok(const RuntimeObjectPtr& value) {
    return make_result("ok", value);
}

inline RuntimeObjectPtr make_error(const std::string& error_type, const RuntimeObjectPtr& value) {
    return make_result(error_type, value);
}

// Helper for creating Option types
inline RuntimeObjectPtr make_some(const RuntimeObjectPtr& value) {
    auto tuple = std::make_shared<TupleValue>();
    tuple->fields.push_back(make_symbol("some"));
    tuple->fields.push_back(value);
    return std::make_shared<RuntimeObject>(Tuple, tuple);
}

inline RuntimeObjectPtr make_none() {
    return make_symbol("none");
}

} // namespace yona::stdlib
