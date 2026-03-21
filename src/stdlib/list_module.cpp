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
    module->exports["take"] = make_native_function("take", 2, take);
    module->exports["drop"] = make_native_function("drop", 2, drop);
    module->exports["flatten"] = make_native_function("flatten", 1, flatten);
    module->exports["zip"] = make_native_function("zip", 2, zip);
    module->exports["foldl"] = make_native_function("foldl", 3, foldl);
    module->exports["foldr"] = make_native_function("foldr", 3, foldr);
    module->exports["lookup"] = make_native_function("lookup", 2, lookup);
    module->exports["splitAt"] = make_native_function("splitAt", 2, splitAt);
    module->exports["any"] = make_native_function("any", 2, any);
    module->exports["all"] = make_native_function("all", 2, all);
    module->exports["contains"] = make_native_function("contains", 2, contains);
    module->exports["isEmpty"] = make_native_function("isEmpty", 1, isEmpty);
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

RuntimeObjectPtr ListModule::take(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("take", 2);
    int n = ARG_INT(0);
    if (!nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "take: second argument must be a sequence");
    }
    auto seq = args[1]->get<shared_ptr<SeqValue>>();
    auto result = make_shared<SeqValue>();
    size_t count = static_cast<size_t>(max(0, n));
    for (size_t i = 0; i < min(count, seq->fields.size()); i++) {
        result->fields.push_back(seq->fields[i]);
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr ListModule::drop(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("drop", 2);
    int n = ARG_INT(0);
    if (!nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "drop: second argument must be a sequence");
    }
    auto seq = args[1]->get<shared_ptr<SeqValue>>();
    auto result = make_shared<SeqValue>();
    size_t start = static_cast<size_t>(max(0, n));
    for (size_t i = start; i < seq->fields.size(); i++) {
        result->fields.push_back(seq->fields[i]);
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr ListModule::flatten(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("flatten", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "flatten: argument must be a sequence");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    auto result = make_shared<SeqValue>();
    for (const auto& elem : seq->fields) {
        if (elem->type == Seq) {
            auto inner = elem->get<shared_ptr<SeqValue>>();
            for (const auto& inner_elem : inner->fields) {
                result->fields.push_back(inner_elem);
            }
        } else {
            result->fields.push_back(elem);
        }
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr ListModule::zip(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("zip", 2);
    if (!nargs.is_type(0, Seq) || !nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "zip: both arguments must be sequences");
    }
    auto seq1 = args[0]->get<shared_ptr<SeqValue>>();
    auto seq2 = args[1]->get<shared_ptr<SeqValue>>();
    auto result = make_shared<SeqValue>();
    size_t len = min(seq1->fields.size(), seq2->fields.size());
    for (size_t i = 0; i < len; i++) {
        auto pair = make_shared<TupleValue>();
        pair->fields.push_back(seq1->fields[i]);
        pair->fields.push_back(seq2->fields[i]);
        result->fields.push_back(make_shared<RuntimeObject>(Tuple, pair));
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr ListModule::foldl(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("foldl", 3);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "foldl: first argument must be a function");
    }
    if (!nargs.is_type(2, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "foldl: third argument must be a sequence");
    }
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto acc = args[1];
    auto seq = args[2]->get<shared_ptr<SeqValue>>();
    for (const auto& elem : seq->fields) {
        acc = call_function(func, {acc, elem});
    }
    return acc;
}

RuntimeObjectPtr ListModule::foldr(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("foldr", 3);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "foldr: first argument must be a function");
    }
    if (!nargs.is_type(2, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "foldr: third argument must be a sequence");
    }
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto acc = args[1];
    auto seq = args[2]->get<shared_ptr<SeqValue>>();
    for (auto it = seq->fields.rbegin(); it != seq->fields.rend(); ++it) {
        acc = call_function(func, {*it, acc});
    }
    return acc;
}

RuntimeObjectPtr ListModule::lookup(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("lookup", 2);
    int index = ARG_INT(0);
    if (!nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "lookup: second argument must be a sequence");
    }
    auto seq = args[1]->get<shared_ptr<SeqValue>>();
    if (index < 0 || static_cast<size_t>(index) >= seq->fields.size()) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
            "lookup: index " + to_string(index) + " out of bounds for sequence of length " + to_string(seq->fields.size()));
    }
    return seq->fields[static_cast<size_t>(index)];
}

RuntimeObjectPtr ListModule::splitAt(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("splitAt", 2);
    int index = ARG_INT(0);
    if (!nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "splitAt: second argument must be a sequence");
    }
    auto seq = args[1]->get<shared_ptr<SeqValue>>();
    size_t split_pos = static_cast<size_t>(max(0, min(index, static_cast<int>(seq->fields.size()))));
    auto left = make_shared<SeqValue>();
    auto right = make_shared<SeqValue>();
    for (size_t i = 0; i < seq->fields.size(); i++) {
        if (i < split_pos) {
            left->fields.push_back(seq->fields[i]);
        } else {
            right->fields.push_back(seq->fields[i]);
        }
    }
    auto result = make_shared<TupleValue>();
    result->fields.push_back(make_shared<RuntimeObject>(Seq, left));
    result->fields.push_back(make_shared<RuntimeObject>(Seq, right));
    return make_shared<RuntimeObject>(Tuple, result);
}

RuntimeObjectPtr ListModule::any(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("any", 2);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "any: first argument must be a function");
    }
    if (!nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "any: second argument must be a sequence");
    }
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto seq = args[1]->get<shared_ptr<SeqValue>>();
    for (const auto& elem : seq->fields) {
        auto result = call_function(func, {elem});
        if (result->type == Bool && result->get<bool>()) {
            return make_bool(true);
        }
    }
    return make_bool(false);
}

RuntimeObjectPtr ListModule::all(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("all", 2);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "all: first argument must be a function");
    }
    if (!nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "all: second argument must be a sequence");
    }
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto seq = args[1]->get<shared_ptr<SeqValue>>();
    for (const auto& elem : seq->fields) {
        auto result = call_function(func, {elem});
        if (result->type != Bool || !result->get<bool>()) {
            return make_bool(false);
        }
    }
    return make_bool(true);
}

RuntimeObjectPtr ListModule::contains(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("contains", 2);
    if (!nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "contains: second argument must be a sequence");
    }
    auto seq = args[1]->get<shared_ptr<SeqValue>>();
    for (const auto& elem : seq->fields) {
        if (*elem == *args[0]) {
            return make_bool(true);
        }
    }
    return make_bool(false);
}

RuntimeObjectPtr ListModule::isEmpty(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isEmpty", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "isEmpty: argument must be a sequence");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    return make_bool(seq->fields.empty());
}

} // namespace yona::stdlib
