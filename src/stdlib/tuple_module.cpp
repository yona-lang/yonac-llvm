#include "stdlib/tuple_module.h"
#include "stdlib/native_args.h"

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

TupleModule::TupleModule() : NativeModule({"Std", "Tuple"}) {}

void TupleModule::initialize() {
    module->exports["fst"] = make_native_function("fst", 1, fst);
    module->exports["snd"] = make_native_function("snd", 1, snd);
    module->exports["swap"] = make_native_function("swap", 1, swap);
    module->exports["mapBoth"] = make_native_function("mapBoth", 3, mapBoth);
    module->exports["zip"] = make_native_function("zip", 2, zip);
    module->exports["unzip"] = make_native_function("unzip", 1, unzip);
}

static shared_ptr<TupleValue> require_pair(const RuntimeObjectPtr& arg, const string& func) {
    if (arg->type != Tuple) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, func + ": argument must be a tuple");
    }
    auto tuple = arg->get<shared_ptr<TupleValue>>();
    if (tuple->fields.size() < 2) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, func + ": tuple must have at least 2 elements");
    }
    return tuple;
}

static RuntimeObjectPtr make_tuple_pair(const RuntimeObjectPtr& a, const RuntimeObjectPtr& b) {
    auto tuple = make_shared<TupleValue>();
    tuple->fields.push_back(a);
    tuple->fields.push_back(b);
    return make_shared<RuntimeObject>(Tuple, tuple);
}

RuntimeObjectPtr TupleModule::fst(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("fst", 1);
    return require_pair(args[0], "fst")->fields[0];
}

RuntimeObjectPtr TupleModule::snd(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("snd", 1);
    return require_pair(args[0], "snd")->fields[1];
}

RuntimeObjectPtr TupleModule::swap(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("swap", 1);
    auto t = require_pair(args[0], "swap");
    return make_tuple_pair(t->fields[1], t->fields[0]);
}

RuntimeObjectPtr TupleModule::mapBoth(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("mapBoth", 3);
    if (!nargs.is_type(0, Function) || !nargs.is_type(1, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "mapBoth: first two arguments must be functions");
    }
    auto f = args[0]->get<shared_ptr<FunctionValue>>();
    auto g = args[1]->get<shared_ptr<FunctionValue>>();
    auto t = require_pair(args[2], "mapBoth");
    return make_tuple_pair(f->code({t->fields[0]}), g->code({t->fields[1]}));
}

RuntimeObjectPtr TupleModule::zip(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("zip", 2);
    if (!nargs.is_type(0, Seq) || !nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "zip: both arguments must be sequences");
    }
    auto s1 = args[0]->get<shared_ptr<SeqValue>>();
    auto s2 = args[1]->get<shared_ptr<SeqValue>>();
    auto result = make_shared<SeqValue>();
    size_t len = std::min(s1->fields.size(), s2->fields.size());
    for (size_t i = 0; i < len; i++) {
        result->fields.push_back(make_tuple_pair(s1->fields[i], s2->fields[i]));
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr TupleModule::unzip(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("unzip", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "unzip: argument must be a sequence");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    auto firsts = make_shared<SeqValue>();
    auto seconds = make_shared<SeqValue>();
    for (const auto& elem : seq->fields) {
        auto t = require_pair(elem, "unzip");
        firsts->fields.push_back(t->fields[0]);
        seconds->fields.push_back(t->fields[1]);
    }
    return make_tuple_pair(
        make_shared<RuntimeObject>(Seq, firsts),
        make_shared<RuntimeObject>(Seq, seconds)
    );
}

} // namespace yona::stdlib
