#include "stdlib/range_module.h"
#include "stdlib/native_args.h"

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

// Range is represented as (:range, start, end) tuple
RangeModule::RangeModule() : NativeModule({"Std", "Range"}) {}

void RangeModule::initialize() {
    module->exports["range"] = make_native_function("range", 2, range);
    module->exports["toList"] = make_native_function("toList", 1, toList);
    module->exports["contains"] = make_native_function("contains", 2, contains);
    module->exports["length"] = make_native_function("length", 1, length);
    module->exports["take"] = make_native_function("take", 2, take);
    module->exports["drop"] = make_native_function("drop", 2, drop);
}

static RuntimeObjectPtr make_range(int start, int end) {
    auto tuple = make_shared<TupleValue>();
    tuple->fields.push_back(make_symbol("range"));
    tuple->fields.push_back(make_int(start));
    tuple->fields.push_back(make_int(end));
    return make_shared<RuntimeObject>(Tuple, tuple);
}

struct RangeInfo {
    int start, end;
};

static RangeInfo extract_range(const RuntimeObjectPtr& arg, const string& func) {
    if (arg->type != Tuple) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, func + ": argument must be a range");
    }
    auto tuple = arg->get<shared_ptr<TupleValue>>();
    if (tuple->fields.size() != 3 || tuple->fields[0]->type != Symbol ||
        tuple->fields[0]->get<shared_ptr<SymbolValue>>()->name != "range") {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, func + ": argument must be a range");
    }
    return {tuple->fields[1]->get<int>(), tuple->fields[2]->get<int>()};
}

RuntimeObjectPtr RangeModule::range(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("range", 2);
    int start = ARG_INT(0);
    int end = ARG_INT(1);
    return make_range(start, end);
}

RuntimeObjectPtr RangeModule::toList(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("toList", 1);
    auto [start, end] = extract_range(args[0], "toList");
    auto result = make_shared<SeqValue>();
    for (int i = start; i <= end; i++) {
        result->fields.push_back(make_int(i));
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr RangeModule::contains(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("contains", 2);
    int value = ARG_INT(0);
    auto [start, end] = extract_range(args[1], "contains");
    return make_bool(value >= start && value <= end);
}

RuntimeObjectPtr RangeModule::length(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("length", 1);
    auto [start, end] = extract_range(args[0], "length");
    return make_int(end - start + 1);
}

RuntimeObjectPtr RangeModule::take(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("take", 2);
    int n = ARG_INT(0);
    auto [start, end] = extract_range(args[1], "take");
    int new_end = std::min(start + n - 1, end);
    return make_range(start, new_end);
}

RuntimeObjectPtr RangeModule::drop(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("drop", 2);
    int n = ARG_INT(0);
    auto [start, end] = extract_range(args[1], "drop");
    int new_start = std::min(start + n, end + 1);
    return make_range(new_start, end);
}

} // namespace yona::stdlib
