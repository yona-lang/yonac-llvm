#include "stdlib/result_module.h"
#include "stdlib/native_args.h"

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

ResultModule::ResultModule() : NativeModule({"Std", "Result"}) {}

void ResultModule::initialize() {
    module->exports["ok"] = make_native_function("ok", 1, ok);
    module->exports["err"] = make_native_function("err", 1, err);
    module->exports["isOk"] = make_native_function("isOk", 1, isOk);
    module->exports["isErr"] = make_native_function("isErr", 1, isErr);
    module->exports["unwrapOr"] = make_native_function("unwrapOr", 2, unwrapOr);
    module->exports["map"] = make_native_function("map", 2, map);
    module->exports["mapErr"] = make_native_function("mapErr", 2, mapErr);
}

// Result is represented as (:ok, value) or (:err, error)

RuntimeObjectPtr ResultModule::ok(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("ok", 1);
    return make_ok(args[0]);
}

RuntimeObjectPtr ResultModule::err(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("err", 1);
    return make_result("err", args[0]);
}

static bool is_tag(const RuntimeObjectPtr& obj, const string& tag) {
    if (obj->type != Tuple) return false;
    auto tuple = obj->get<shared_ptr<TupleValue>>();
    return tuple->fields.size() == 2 && tuple->fields[0]->type == Symbol &&
           tuple->fields[0]->get<shared_ptr<SymbolValue>>()->name == tag;
}

RuntimeObjectPtr ResultModule::isOk(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isOk", 1);
    return make_bool(is_tag(args[0], "ok"));
}

RuntimeObjectPtr ResultModule::isErr(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isErr", 1);
    return make_bool(is_tag(args[0], "err"));
}

RuntimeObjectPtr ResultModule::unwrapOr(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("unwrapOr", 2);
    if (is_tag(args[1], "ok")) {
        return args[1]->get<shared_ptr<TupleValue>>()->fields[1];
    }
    return args[0]; // default value
}

RuntimeObjectPtr ResultModule::map(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("map", 2);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "map: first argument must be a function");
    }
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    if (is_tag(args[1], "ok")) {
        auto val = args[1]->get<shared_ptr<TupleValue>>()->fields[1];
        return make_ok(func->code({val}));
    }
    return args[1]; // err passes through
}

RuntimeObjectPtr ResultModule::mapErr(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("mapErr", 2);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "mapErr: first argument must be a function");
    }
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    if (is_tag(args[1], "err")) {
        auto val = args[1]->get<shared_ptr<TupleValue>>()->fields[1];
        return make_result("err", func->code({val}));
    }
    return args[1]; // ok passes through
}

} // namespace yona::stdlib
