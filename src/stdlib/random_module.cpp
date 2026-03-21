#include "stdlib/random_module.h"
#include "stdlib/native_args.h"
#include <random>
#include <algorithm>

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

static mt19937& get_engine() {
    thread_local mt19937 engine{random_device{}()};
    return engine;
}

RandomModule::RandomModule() : NativeModule({"Std", "Random"}) {}

void RandomModule::initialize() {
    module->exports["int"] = make_native_function("random_int", 2, random_int);
    module->exports["float"] = make_native_function("random_float", 0, random_float);
    module->exports["choice"] = make_native_function("random_choice", 1, random_choice);
    module->exports["shuffle"] = make_native_function("random_shuffle", 1, random_shuffle);
}

RuntimeObjectPtr RandomModule::random_int(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("random_int", 2);
    int min_val = ARG_INT(0);
    int max_val = ARG_INT(1);

    if (min_val > max_val) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
            "random_int: min must be <= max");
    }

    uniform_int_distribution<int> dist(min_val, max_val);
    return make_int(dist(get_engine()));
}

RuntimeObjectPtr RandomModule::random_float(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("random_float", 0);
    uniform_real_distribution<double> dist(0.0, 1.0);
    return make_float(dist(get_engine()));
}

RuntimeObjectPtr RandomModule::random_choice(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("random_choice", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "random_choice: argument must be a sequence");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    if (seq->fields.empty()) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "random_choice: sequence must not be empty");
    }

    uniform_int_distribution<size_t> dist(0, seq->fields.size() - 1);
    return seq->fields[dist(get_engine())];
}

RuntimeObjectPtr RandomModule::random_shuffle(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("random_shuffle", 1);
    if (!nargs.is_type(0, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "random_shuffle: argument must be a sequence");
    }
    auto seq = args[0]->get<shared_ptr<SeqValue>>();
    auto result = make_shared<SeqValue>();
    result->fields = seq->fields;
    shuffle(result->fields.begin(), result->fields.end(), get_engine());
    return make_shared<RuntimeObject>(Seq, result);
}

} // namespace yona::stdlib
