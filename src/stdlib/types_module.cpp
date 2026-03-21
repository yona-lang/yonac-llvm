#include "stdlib/types_module.h"
#include "stdlib/native_args.h"
#include <sstream>

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

TypesModule::TypesModule() : NativeModule({"Std", "Types"}) {}

void TypesModule::initialize() {
    module->exports["typeOf"] = make_native_function("typeOf", 1, typeOf);
    module->exports["isInt"] = make_native_function("isInt", 1, isInt);
    module->exports["isFloat"] = make_native_function("isFloat", 1, isFloat);
    module->exports["isString"] = make_native_function("isString", 1, isString);
    module->exports["isBool"] = make_native_function("isBool", 1, isBool);
    module->exports["isSeq"] = make_native_function("isSeq", 1, isSeq);
    module->exports["isSet"] = make_native_function("isSet", 1, isSet);
    module->exports["isDict"] = make_native_function("isDict", 1, isDict);
    module->exports["isTuple"] = make_native_function("isTuple", 1, isTuple);
    module->exports["isFunction"] = make_native_function("isFunction", 1, isFunction);
    module->exports["toInt"] = make_native_function("toInt", 1, toInt);
    module->exports["toFloat"] = make_native_function("toFloat", 1, toFloat);
    module->exports["toStr"] = make_native_function("toStr", 1, toStr);
}

RuntimeObjectPtr TypesModule::typeOf(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("typeOf", 1);
    switch (args[0]->type) {
        case Int: return make_symbol("int");
        case Float: return make_symbol("float");
        case String: return make_symbol("string");
        case Bool: return make_symbol("bool");
        case Unit: return make_symbol("unit");
        case Symbol: return make_symbol("symbol");
        case Seq: return make_symbol("seq");
        case yona::interp::runtime::Set: return make_symbol("set");
        case yona::interp::runtime::Dict: return make_symbol("dict");
        case Tuple: return make_symbol("tuple");
        case Record: return make_symbol("record");
        case Function: return make_symbol("function");
        case Module: return make_symbol("module");
        case Byte: return make_symbol("byte");
        case Char: return make_symbol("char");
        case FQN: return make_symbol("fqn");
        case Promise: return make_symbol("promise");
        case Error: return make_symbol("error");
        default: return make_symbol("unknown");
    }
}

RuntimeObjectPtr TypesModule::isInt(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isInt", 1);
    return make_bool(args[0]->type == Int);
}

RuntimeObjectPtr TypesModule::isFloat(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isFloat", 1);
    return make_bool(args[0]->type == Float);
}

RuntimeObjectPtr TypesModule::isString(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isString", 1);
    return make_bool(args[0]->type == String);
}

RuntimeObjectPtr TypesModule::isBool(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isBool", 1);
    return make_bool(args[0]->type == Bool);
}

RuntimeObjectPtr TypesModule::isSeq(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isSeq", 1);
    return make_bool(args[0]->type == Seq);
}

RuntimeObjectPtr TypesModule::isSet(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isSet", 1);
    return make_bool(args[0]->type == yona::interp::runtime::Set);
}

RuntimeObjectPtr TypesModule::isDict(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isDict", 1);
    return make_bool(args[0]->type == yona::interp::runtime::Dict);
}

RuntimeObjectPtr TypesModule::isTuple(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isTuple", 1);
    return make_bool(args[0]->type == Tuple);
}

RuntimeObjectPtr TypesModule::isFunction(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isFunction", 1);
    return make_bool(args[0]->type == Function);
}

RuntimeObjectPtr TypesModule::toInt(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("toInt", 1);
    auto& arg = args[0];
    switch (arg->type) {
        case Int:
            return arg;
        case Float:
            return make_int(static_cast<int>(arg->get<double>()));
        case String: {
            try {
                return make_int(stoi(arg->get<string>()));
            } catch (...) {
                throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
                    "toInt: cannot convert string '" + arg->get<string>() + "' to integer");
            }
        }
        case Bool:
            return make_int(arg->get<bool>() ? 1 : 0);
        default:
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                "toInt: cannot convert " + get_runtime_type_name(arg->type) + " to integer");
    }
}

RuntimeObjectPtr TypesModule::toFloat(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("toFloat", 1);
    auto& arg = args[0];
    switch (arg->type) {
        case Float:
            return arg;
        case Int:
            return make_float(static_cast<double>(arg->get<int>()));
        case String: {
            try {
                return make_float(stod(arg->get<string>()));
            } catch (...) {
                throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
                    "toFloat: cannot convert string '" + arg->get<string>() + "' to float");
            }
        }
        case Bool:
            return make_float(arg->get<bool>() ? 1.0 : 0.0);
        default:
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                "toFloat: cannot convert " + get_runtime_type_name(arg->type) + " to float");
    }
}

RuntimeObjectPtr TypesModule::toStr(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("toStr", 1);
    ostringstream oss;
    oss << *args[0];
    return make_string(oss.str());
}

} // namespace yona::stdlib
