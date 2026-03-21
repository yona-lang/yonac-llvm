#include "stdlib/timer_module.h"
#include "stdlib/native_args.h"
#include <chrono>
#include <thread>

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

TimerModule::TimerModule() : NativeModule({"Std", "Timer"}) {}

void TimerModule::initialize() {
    // sleep is async — runs in thread pool, returns Promise that resolves after delay
    module->exports["sleep"] = make_native_async_function("sleep", 1, sleep);
    // Time queries are synchronous
    module->exports["millis"] = make_native_function("millis", 0, millis);
    module->exports["nanos"] = make_native_function("nanos", 0, nanos);
    module->exports["measure"] = make_native_function("measure", 1, measure);
}

RuntimeObjectPtr TimerModule::sleep(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("sleep", 1);
    int ms = ARG_INT(0);
    if (ms < 0) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "sleep: duration must be non-negative");
    }
    this_thread::sleep_for(chrono::milliseconds(ms));
    return make_unit();
}

RuntimeObjectPtr TimerModule::millis(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_NONE("millis");
    auto now = chrono::system_clock::now().time_since_epoch();
    return make_int(static_cast<int>(chrono::duration_cast<chrono::milliseconds>(now).count()));
}

RuntimeObjectPtr TimerModule::nanos(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_NONE("nanos");
    auto now = chrono::high_resolution_clock::now().time_since_epoch();
    return make_int(static_cast<int>(chrono::duration_cast<chrono::nanoseconds>(now).count()));
}

RuntimeObjectPtr TimerModule::measure(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("measure", 1);
    if (!nargs.is_type(0, Function)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "measure: argument must be a function");
    }

    auto func = args[0]->get<shared_ptr<FunctionValue>>();
    auto start = chrono::high_resolution_clock::now();
    auto result = func->code({});
    auto end = chrono::high_resolution_clock::now();

    auto duration_ns = chrono::duration_cast<chrono::nanoseconds>(end - start).count();

    auto tuple = make_shared<TupleValue>();
    tuple->fields.push_back(make_int(static_cast<int>(duration_ns)));
    tuple->fields.push_back(result);
    return make_shared<RuntimeObject>(Tuple, tuple);
}

} // namespace yona::stdlib
