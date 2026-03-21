#include "stdlib/set_module.h"
#include "stdlib/native_args.h"
#include <algorithm>

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

SetModule::SetModule() : NativeModule({"Std", "Set"}) {}

void SetModule::initialize() {
    module->exports["contains"] = make_native_function("contains", 2, contains);
    module->exports["add"] = make_native_function("add", 2, add);
    module->exports["remove"] = make_native_function("remove", 2, remove);
    module->exports["size"] = make_native_function("size", 1, size);
    module->exports["toList"] = make_native_function("toList", 1, toList);
    module->exports["fromList"] = make_native_function("fromList", 1, fromList);
    module->exports["union"] = make_native_function("union", 2, union_);
    module->exports["intersection"] = make_native_function("intersection", 2, intersection);
    module->exports["difference"] = make_native_function("difference", 2, difference);
    module->exports["fold"] = make_native_function("fold", 3, fold);
    module->exports["map"] = make_native_function("map", 2, map);
    module->exports["filter"] = make_native_function("filter", 2, filter);
    module->exports["isEmpty"] = make_native_function("isEmpty", 1, isEmpty);
}

static bool set_contains(const shared_ptr<SetValue>& set, const RuntimeObjectPtr& elem) {
    return any_of(set->fields.begin(), set->fields.end(),
                  [&](const RuntimeObjectPtr& e) { return *e == *elem; });
}

RuntimeObjectPtr SetModule::contains(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("contains", 2);
    if (!nargs.is_type(1, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "contains: second argument must be a set");
    }
    auto set = args[1]->get<shared_ptr<SetValue>>();
    return make_bool(set_contains(set, args[0]));
}

RuntimeObjectPtr SetModule::add(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("add", 2);
    if (!nargs.is_type(1, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "add: second argument must be a set");
    }
    auto set = args[1]->get<shared_ptr<SetValue>>();
    auto result = make_shared<SetValue>();
    result->fields = set->fields;
    if (!set_contains(set, args[0])) {
        result->fields.push_back(args[0]);
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Set, result);
}

RuntimeObjectPtr SetModule::remove(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("remove", 2);
    if (!nargs.is_type(1, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "remove: second argument must be a set");
    }
    auto set = args[1]->get<shared_ptr<SetValue>>();
    auto result = make_shared<SetValue>();
    for (const auto& elem : set->fields) {
        if (!(*elem == *args[0])) {
            result->fields.push_back(elem);
        }
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Set, result);
}

RuntimeObjectPtr SetModule::size(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("size", 1);
    if (!nargs.is_type(0, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "size: argument must be a set");
    }
    auto set = args[0]->get<shared_ptr<SetValue>>();
    return make_int(static_cast<int>(set->fields.size()));
}

RuntimeObjectPtr SetModule::toList(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("toList", 1);
    if (!nargs.is_type(0, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "toList: argument must be a set");
    }
    auto set = args[0]->get<shared_ptr<SetValue>>();
    auto result = make_shared<SeqValue>();
    result->fields = set->fields;
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr SetModule::fromList(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("fromList", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "fromList: argument must be a sequence");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    auto result = make_shared<SetValue>();
    for (const auto& elem : seq->fields) {
        if (!set_contains(result, elem)) {
            result->fields.push_back(elem);
        }
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Set, result);
}

RuntimeObjectPtr SetModule::union_(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("union", 2);
    if (!nargs.is_type(0, yona::interp::runtime::Set) || !nargs.is_type(1, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "union: both arguments must be sets");
    }
    auto s1 = args[0]->get<shared_ptr<SetValue>>();
    auto s2 = args[1]->get<shared_ptr<SetValue>>();
    auto result = make_shared<SetValue>();
    result->fields = s1->fields;
    for (const auto& elem : s2->fields) {
        if (!set_contains(result, elem)) {
            result->fields.push_back(elem);
        }
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Set, result);
}

RuntimeObjectPtr SetModule::intersection(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("intersection", 2);
    if (!nargs.is_type(0, yona::interp::runtime::Set) || !nargs.is_type(1, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "intersection: both arguments must be sets");
    }
    auto s1 = args[0]->get<shared_ptr<SetValue>>();
    auto s2 = args[1]->get<shared_ptr<SetValue>>();
    auto result = make_shared<SetValue>();
    for (const auto& elem : s1->fields) {
        if (set_contains(s2, elem)) {
            result->fields.push_back(elem);
        }
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Set, result);
}

RuntimeObjectPtr SetModule::difference(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("difference", 2);
    if (!nargs.is_type(0, yona::interp::runtime::Set) || !nargs.is_type(1, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "difference: both arguments must be sets");
    }
    auto s1 = args[0]->get<shared_ptr<SetValue>>();
    auto s2 = args[1]->get<shared_ptr<SetValue>>();
    auto result = make_shared<SetValue>();
    for (const auto& elem : s1->fields) {
        if (!set_contains(s2, elem)) {
            result->fields.push_back(elem);
        }
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Set, result);
}

RuntimeObjectPtr SetModule::fold(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("fold", 3);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "fold: first argument must be a function");
    }
    if (!nargs.is_type(2, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "fold: third argument must be a set");
    }
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto acc = args[1];
    auto set = args[2]->get<shared_ptr<SetValue>>();
    for (const auto& elem : set->fields) {
        acc = func->code({acc, elem});
    }
    return acc;
}

RuntimeObjectPtr SetModule::map(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("map", 2);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "map: first argument must be a function");
    }
    if (!nargs.is_type(1, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "map: second argument must be a set");
    }
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto set = args[1]->get<shared_ptr<SetValue>>();
    auto result = make_shared<SetValue>();
    for (const auto& elem : set->fields) {
        auto mapped = func->code({elem});
        if (!set_contains(result, mapped)) {
            result->fields.push_back(mapped);
        }
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Set, result);
}

RuntimeObjectPtr SetModule::filter(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("filter", 2);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "filter: first argument must be a function");
    }
    if (!nargs.is_type(1, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "filter: second argument must be a set");
    }
    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto set = args[1]->get<shared_ptr<SetValue>>();
    auto result = make_shared<SetValue>();
    for (const auto& elem : set->fields) {
        auto pred = func->code({elem});
        if (pred->type == Bool && pred->get<bool>()) {
            result->fields.push_back(elem);
        }
    }
    return make_shared<RuntimeObject>(yona::interp::runtime::Set, result);
}

RuntimeObjectPtr SetModule::isEmpty(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isEmpty", 1);
    if (!nargs.is_type(0, yona::interp::runtime::Set)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "isEmpty: argument must be a set");
    }
    auto set = args[0]->get<shared_ptr<SetValue>>();
    return make_bool(set->fields.empty());
}

} // namespace yona::stdlib
