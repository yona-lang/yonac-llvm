#include "stdlib/list_module.h"
#include "stdlib/native_args.h"
#include "Interpreter.h"

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

ListModule::ListModule() : NativeModule({"Std", "List"}) {}

void ListModule::initialize() {
    module->exports["map"] = make_native_function("map", 2, map);
    module->exports["filter"] = make_native_function("filter", 2, filter);
    module->exports["fold"] = make_native_function("fold", 3, fold);
    module->exports["length"] = make_native_function("length", 1, length);
    module->exports["head"] = make_native_function("head", 1, head);
    module->exports["tail"] = make_native_function("tail", 1, tail);
    module->exports["reverse"] = make_native_function("reverse", 1, reverse);
}

static RuntimeObjectPtr call_function(const shared_ptr<FunctionValue>& func, const vector<RuntimeObjectPtr>& args) {
    return func->code(args);
}

RuntimeObjectPtr ListModule::map(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("map", 2);

    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "map: first argument must be a function");
    }
    if (!nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "map: second argument must be a sequence");
    }

    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto seq = args[1]->get<shared_ptr<SeqValue>>();

    auto result = make_shared<SeqValue>();
    for (const auto& elem : seq->fields) {
        result->fields.push_back(call_function(func, {elem}));
    }

    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr ListModule::filter(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("filter", 2);

    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "filter: first argument must be a function");
    }
    if (!nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "filter: second argument must be a sequence");
    }

    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto seq = args[1]->get<shared_ptr<SeqValue>>();

    auto result = make_shared<SeqValue>();
    for (const auto& elem : seq->fields) {
        auto pred = call_function(func, {elem});
        if (pred->type == Bool && pred->get<bool>()) {
            result->fields.push_back(elem);
        }
    }

    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr ListModule::fold(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("fold", 3);

    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "fold: first argument must be a function");
    }
    if (!nargs.is_type(2, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "fold: third argument must be a sequence");
    }

    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto acc = args[1];
    auto seq = args[2]->get<shared_ptr<SeqValue>>();

    for (const auto& elem : seq->fields) {
        acc = call_function(func, {acc, elem});
    }

    return acc;
}

RuntimeObjectPtr ListModule::length(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("length", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "length: argument must be a sequence");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    return make_int(static_cast<int>(seq->fields.size()));
}

RuntimeObjectPtr ListModule::head(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("head", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "head: argument must be a sequence");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    if (seq->fields.empty()) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "head: empty sequence");
    }
    return seq->fields[0];
}

RuntimeObjectPtr ListModule::tail(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("tail", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "tail: argument must be a sequence");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    if (seq->fields.empty()) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "tail: empty sequence");
    }
    auto result = make_shared<SeqValue>();
    for (size_t i = 1; i < seq->fields.size(); i++) {
        result->fields.push_back(seq->fields[i]);
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr ListModule::reverse(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("reverse", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "reverse: argument must be a sequence");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    auto result = make_shared<SeqValue>();
    for (auto it = seq->fields.rbegin(); it != seq->fields.rend(); ++it) {
        result->fields.push_back(*it);
    }
    return make_shared<RuntimeObject>(Seq, result);
}

} // namespace yona::stdlib
