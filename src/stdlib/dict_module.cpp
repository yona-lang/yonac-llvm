#include "stdlib/dict_module.h"
#include "stdlib/native_args.h"

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

DictModule::DictModule() : NativeModule({"Std", "Dict"}) {}

void DictModule::initialize() {
    module->exports["get"] = make_native_function("get", 2, get);
    module->exports["put"] = make_native_function("put", 3, put);
    module->exports["remove"] = make_native_function("remove", 2, remove);
    module->exports["containsKey"] = make_native_function("containsKey", 2, containsKey);
    module->exports["keys"] = make_native_function("keys", 1, keys);
    module->exports["values"] = make_native_function("values", 1, values);
    module->exports["size"] = make_native_function("size", 1, size);
    module->exports["toList"] = make_native_function("toList", 1, toList);
    module->exports["fromList"] = make_native_function("fromList", 1, fromList);
    module->exports["merge"] = make_native_function("merge", 2, merge);
    module->exports["fold"] = make_native_function("fold", 3, fold);
    module->exports["map"] = make_native_function("map", 2, map);
    module->exports["filter"] = make_native_function("filter", 2, filter);
    module->exports["isEmpty"] = make_native_function("isEmpty", 1, isEmpty);
    module->exports["lookup"] = make_native_function("lookup", 2, lookup);
}

static shared_ptr<DictValue> require_dict(const RuntimeObjectPtr& arg, const string& func) {
    if (arg->type != yona::interp::runtime::Dict) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, func + ": argument must be a dictionary");
    }
    return arg->get<shared_ptr<DictValue>>();
}

RuntimeObjectPtr DictModule::get(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("get", 2);
    auto dict = require_dict(args[1], "get");
    for (const auto& [k, v] : dict->fields) {
        if (*k == *args[0]) return make_some(v);
    }
    return make_none();
}

RuntimeObjectPtr DictModule::put(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("put", 3);
    auto dict = require_dict(args[2], "put");
    auto result = make_shared<DictValue>();
    bool replaced = false;
    for (const auto& [k, v] : dict->fields) {
        if (*k == *args[0]) {
            result->fields.emplace_back(k, args[1]);
            replaced = true;
        } else {
            result->fields.emplace_back(k, v);
        }
    }
    if (!replaced) {
        result->fields.emplace_back(args[0], args[1]);
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Dict, result);
}

RuntimeObjectPtr DictModule::remove(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("remove", 2);
    auto dict = require_dict(args[1], "remove");
    auto result = make_shared<DictValue>();
    for (const auto& [k, v] : dict->fields) {
        if (!(*k == *args[0])) {
            result->fields.emplace_back(k, v);
        }
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Dict, result);
}

RuntimeObjectPtr DictModule::containsKey(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("containsKey", 2);
    auto dict = require_dict(args[1], "containsKey");
    for (const auto& [k, v] : dict->fields) {
        if (*k == *args[0]) return make_bool(true);
    }
    return make_bool(false);
}

RuntimeObjectPtr DictModule::keys(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("keys", 1);
    auto dict = require_dict(args[0], "keys");
    auto result = make_shared<SeqValue>();
    for (const auto& [k, v] : dict->fields) {
        result->fields.push_back(k);
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr DictModule::values(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("values", 1);
    auto dict = require_dict(args[0], "values");
    auto result = make_shared<SeqValue>();
    for (const auto& [k, v] : dict->fields) {
        result->fields.push_back(v);
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr DictModule::size(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("size", 1);
    auto dict = require_dict(args[0], "size");
    return make_int(static_cast<int>(dict->fields.size()));
}

RuntimeObjectPtr DictModule::toList(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("toList", 1);
    auto dict = require_dict(args[0], "toList");
    auto result = make_shared<SeqValue>();
    for (const auto& [k, v] : dict->fields) {
        auto pair = make_shared<TupleValue>();
        pair->fields.push_back(k);
        pair->fields.push_back(v);
        result->fields.push_back(make_shared<RuntimeObject>(Tuple, pair));
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr DictModule::fromList(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("fromList", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "fromList: argument must be a sequence of (key, value) tuples");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    auto result = make_shared<DictValue>();
    for (const auto& elem : seq->fields) {
        if (elem->type != Tuple) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "fromList: each element must be a (key, value) tuple");
        }
        auto tuple = elem->get<shared_ptr<TupleValue>>();
        if (tuple->fields.size() != 2) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "fromList: each tuple must have exactly 2 elements");
        }
        result->fields.emplace_back(tuple->fields[0], tuple->fields[1]);
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Dict, result);
}

RuntimeObjectPtr DictModule::merge(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("merge", 2);
    auto d1 = require_dict(args[0], "merge");
    auto d2 = require_dict(args[1], "merge");
    auto result = make_shared<DictValue>();
    result->fields = d1->fields;
    for (const auto& [k2, v2] : d2->fields) {
        bool found = false;
        for (auto& [k, v] : result->fields) {
            if (*k == *k2) { v = v2; found = true; break; }
        }
        if (!found) result->fields.emplace_back(k2, v2);
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Dict, result);
}

RuntimeObjectPtr DictModule::fold(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("fold", 3);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "fold: first argument must be a function");
    }
    auto dict = require_dict(args[2], "fold");
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto acc = args[1];
    for (const auto& [k, v] : dict->fields) {
        auto pair = make_shared<TupleValue>();
        pair->fields.push_back(k);
        pair->fields.push_back(v);
        auto tuple_obj = make_shared<RuntimeObject>(Tuple, pair);
        acc = func->code({acc, tuple_obj});
    }
    return acc;
}

RuntimeObjectPtr DictModule::map(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("map", 2);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "map: first argument must be a function");
    }
    auto dict = require_dict(args[1], "map");
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto result = make_shared<DictValue>();
    for (const auto& [k, v] : dict->fields) {
        auto mapped = func->code({k, v});
        if (mapped->type != Tuple) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "map: function must return a (key, value) tuple");
        }
        auto tuple = mapped->get<shared_ptr<TupleValue>>();
        if (tuple->fields.size() != 2) {
            throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "map: function must return a tuple with exactly 2 elements");
        }
        result->fields.emplace_back(tuple->fields[0], tuple->fields[1]);
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Dict, result);
}

RuntimeObjectPtr DictModule::filter(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("filter", 2);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "filter: first argument must be a function");
    }
    auto dict = require_dict(args[1], "filter");
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto result = make_shared<DictValue>();
    for (const auto& [k, v] : dict->fields) {
        auto pred = func->code({k, v});
        if (pred->type == Bool && pred->get<bool>()) {
            result->fields.emplace_back(k, v);
        }
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Dict, result);
}

RuntimeObjectPtr DictModule::isEmpty(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isEmpty", 1);
    auto dict = require_dict(args[0], "isEmpty");
    return make_bool(dict->fields.empty());
}

RuntimeObjectPtr DictModule::lookup(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("lookup", 2);
    auto dict = require_dict(args[1], "lookup");
    for (const auto& [k, v] : dict->fields) {
        if (*k == *args[0]) return v;
    }
    return make_unit();
}

} // namespace yona::stdlib
