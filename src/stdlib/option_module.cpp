#include "stdlib/option_module.h"
#include "stdlib/native_args.h"

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

OptionModule::OptionModule() : NativeModule({"Std", "Option"}) {}

void OptionModule::initialize() {
    module->exports["some"] = make_native_function("some", 1, some);
    module->exports["none"] = make_native_function("none", 0, none);
    module->exports["isSome"] = make_native_function("isSome", 1, isSome);
    module->exports["isNone"] = make_native_function("isNone", 1, isNone);
    module->exports["unwrapOr"] = make_native_function("unwrapOr", 2, unwrapOr);
    module->exports["map"] = make_native_function("map", 2, map);
}

// Option is represented as (:some, value) or :none
RuntimeObjectPtr OptionModule::some(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("some", 1);
    return make_some(args[0]);
}

RuntimeObjectPtr OptionModule::none(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_NONE("none");
    return make_none();
}

RuntimeObjectPtr OptionModule::isSome(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isSome", 1);
    if (args[0]->type == Tuple) {
        auto tuple = args[0]->get<shared_ptr<TupleValue>>();
        if (!tuple->fields.empty() && tuple->fields[0]->type == Symbol) {
            return make_bool(tuple->fields[0]->get<shared_ptr<SymbolValue>>()->name == "some");
        }
    }
    return make_bool(false);
}

RuntimeObjectPtr OptionModule::isNone(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isNone", 1);
    if (args[0]->type == Symbol) {
        return make_bool(args[0]->get<shared_ptr<SymbolValue>>()->name == "none");
    }
    return make_bool(false);
}

RuntimeObjectPtr OptionModule::unwrapOr(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("unwrapOr", 2);
    auto default_val = args[0];
    auto opt = args[1];

    if (opt->type == Tuple) {
        auto tuple = opt->get<shared_ptr<TupleValue>>();
        if (tuple->fields.size() == 2 && tuple->fields[0]->type == Symbol &&
            tuple->fields[0]->get<shared_ptr<SymbolValue>>()->name == "some") {
            return tuple->fields[1];
        }
    }
    return default_val;
}

RuntimeObjectPtr OptionModule::map(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("map", 2);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "map: first argument must be a function");
    }

    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto opt = args[1];

    if (opt->type == Tuple) {
        auto tuple = opt->get<shared_ptr<TupleValue>>();
        if (tuple->fields.size() == 2 && tuple->fields[0]->type == Symbol &&
            tuple->fields[0]->get<shared_ptr<SymbolValue>>()->name == "some") {
            auto mapped = func->code({tuple->fields[1]});
            return make_some(mapped);
        }
    }
    return opt; // none passes through
}

} // namespace yona::stdlib
