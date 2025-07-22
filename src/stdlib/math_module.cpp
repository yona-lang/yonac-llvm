#include "stdlib/math_module.h"
#include "stdlib/native_args.h"
#include <cmath>
#include "common.h"

// Define math constants if not already defined (Windows doesn't define these)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_E
#define M_E 2.71828182845904523536
#endif

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

MathModule::MathModule() : NativeModule({"Std", "Math", "Native"}) {}

void MathModule::initialize() {
    // Trigonometric functions
    module->exports["sin"] = make_native_function("sin", 1, sin);
    module->exports["cos"] = make_native_function("cos", 1, cos);
    module->exports["tan"] = make_native_function("tan", 1, tan);
    module->exports["asin"] = make_native_function("asin", 1, asin);
    module->exports["acos"] = make_native_function("acos", 1, acos);
    module->exports["atan"] = make_native_function("atan", 1, atan);
    module->exports["atan2"] = make_native_function("atan2", 2, atan2);

    // Exponential and logarithmic functions
    module->exports["exp"] = make_native_function("exp", 1, exp);
    module->exports["log"] = make_native_function("log", 1, log);
    module->exports["log10"] = make_native_function("log10", 1, log10);
    module->exports["pow"] = make_native_function("pow", 2, pow);
    module->exports["sqrt"] = make_native_function("sqrt", 1, sqrt);

    // Rounding functions
    module->exports["ceil"] = make_native_function("ceil", 1, ceil);
    module->exports["floor"] = make_native_function("floor", 1, floor);
    module->exports["round"] = make_native_function("round", 1, round);
    module->exports["abs"] = make_native_function("abs", 1, abs);

    // Constants
    module->exports["pi"] = make_native_function("pi", 0, pi);
    module->exports["e"] = make_native_function("e", 0, e);
}

// No longer needed - using NativeArgs::get_numeric() instead

RuntimeObjectPtr MathModule::sin(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("sin");
    return make_float(std::sin(x));
}

RuntimeObjectPtr MathModule::cos(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("cos");
    return make_float(std::cos(x));
}

RuntimeObjectPtr MathModule::tan(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("tan");
    return make_float(std::tan(x));
}

RuntimeObjectPtr MathModule::asin(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("asin");
    if (x < -1.0 || x > 1.0) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "asin: argument out of range [-1, 1]");
    }
    return make_float(std::asin(x));
}

RuntimeObjectPtr MathModule::acos(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("acos");
    if (x < -1.0 || x > 1.0) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "acos: argument out of range [-1, 1]");
    }
    return make_float(std::acos(x));
}

RuntimeObjectPtr MathModule::atan(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("atan");
    return make_float(std::atan(x));
}

RuntimeObjectPtr MathModule::atan2(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_2("atan2");
    return make_float(std::atan2(x, y));
}

RuntimeObjectPtr MathModule::exp(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("exp");
    return make_float(std::exp(x));
}

RuntimeObjectPtr MathModule::log(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("log");
    if (x <= 0) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "log: argument must be positive");
    }
    return make_float(std::log(x));
}

RuntimeObjectPtr MathModule::log10(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("log10");
    if (x <= 0) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "log10: argument must be positive");
    }
    return make_float(std::log10(x));
}

RuntimeObjectPtr MathModule::pow(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_2("pow");
    return make_float(std::pow(x, y));
}

RuntimeObjectPtr MathModule::sqrt(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("sqrt");
    if (x < 0) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "sqrt: argument must be non-negative");
    }
    return make_float(std::sqrt(x));
}

RuntimeObjectPtr MathModule::ceil(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("ceil");
    return make_int(static_cast<int>(std::ceil(x)));
}

RuntimeObjectPtr MathModule::floor(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("floor");
    return make_int(static_cast<int>(std::floor(x)));
}

RuntimeObjectPtr MathModule::round(const vector<RuntimeObjectPtr>& args) {
    MATH_FUNC_1("round");
    return make_int(static_cast<int>(std::round(x)));
}

RuntimeObjectPtr MathModule::abs(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("abs", 1);

    if (nargs.is_type(0, Int)) {
        int x = ARG_INT(0);
        return make_int(std::abs(x));
    } else if (nargs.is_type(0, Float)) {
        double x = ARG_DOUBLE(0);
        return make_float(std::abs(x));
    } else {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "abs expects numeric argument");
    }
}

RuntimeObjectPtr MathModule::pi(const vector<RuntimeObjectPtr>& args) {
    NATIVE_FUNC_0("pi");
    return make_float(M_PI);
}

RuntimeObjectPtr MathModule::e(const vector<RuntimeObjectPtr>& args) {
    NATIVE_FUNC_0("e");
    return make_float(M_E);
}

} // namespace yona::stdlib
