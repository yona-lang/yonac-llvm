#include "stdlib/system_module.h"
#include "stdlib/native_args.h"
#include <cstdlib>
#include <chrono>
#include <thread>
#include <filesystem>
#include "common.h"

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

// Static storage for command line arguments
static vector<string> command_line_args;

SystemModule::SystemModule() : NativeModule({"Std", "System"}) {}

void SystemModule::initialize() {
    // Register all System functions
    module->exports["getEnv"] = make_native_function("getEnv", 1, getEnv);
    module->exports["setEnv"] = make_native_function("setEnv", 2, setEnv);
    module->exports["exit"] = make_native_function("exit", 1, exit);
    module->exports["currentTimeMillis"] = make_native_function("currentTimeMillis", 0, currentTimeMillis);
    module->exports["sleep"] = make_native_function("sleep", 1, sleep);
    module->exports["getArgs"] = make_native_function("getArgs", 0, getArgs);
    module->exports["getCwd"] = make_native_function("getCwd", 0, getCwd);
    module->exports["setCwd"] = make_native_function("setCwd", 1, setCwd);
}

RuntimeObjectPtr SystemModule::getEnv(const vector<RuntimeObjectPtr>& args) {
    SYS_FUNC_ENV("getEnv");

    const char* value = std::getenv(var_name.c_str());
    if (value) {
        return make_some(make_string(value));
    } else {
        return make_none();
    }
}

RuntimeObjectPtr SystemModule::setEnv(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("setEnv", 2);
    NEXT_STRING(var_name);
    NEXT_STRING(value);

#ifdef _WIN32
    int result = _putenv_s(var_name.c_str(), value.c_str());
#else
    int result = setenv(var_name.c_str(), value.c_str(), 1);
#endif

    return make_bool(result == 0);
}

RuntimeObjectPtr SystemModule::exit(const vector<RuntimeObjectPtr>& args) {
    int exit_code = 0;

    if (!args.empty() && args[0]->type == Int) {
        exit_code = args[0]->get<int>();
    }

    std::exit(exit_code);
    // This line will never be reached
    return make_shared<RuntimeObject>(Unit, nullptr);
}

RuntimeObjectPtr SystemModule::currentTimeMillis(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_NONE("currentTimeMillis");

    auto now = chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = chrono::duration_cast<chrono::milliseconds>(duration).count();

    // Return as Float to avoid overflow (timestamps are too large for 32-bit int)
    return make_float(static_cast<double>(millis));
}

RuntimeObjectPtr SystemModule::sleep(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("sleep", 1);
    int millis = ARG_INT(0);
    if (millis > 0) {
        this_thread::sleep_for(chrono::milliseconds(millis));
    }

    return make_unit();
}

RuntimeObjectPtr SystemModule::getArgs(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_NONE("getArgs");

    auto seq = make_shared<SeqValue>();
    for (const auto& arg : command_line_args) {
        seq->fields.push_back(make_string(arg));
    }

    return make_shared<RuntimeObject>(Seq, seq);
}

RuntimeObjectPtr SystemModule::getCwd(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_NONE("getCwd");

    try {
        auto cwd = filesystem::current_path();
        return make_string(cwd.string());
    } catch (const filesystem::filesystem_error& e) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
                        string("Failed to get current directory: ") + e.what());
    }
}

RuntimeObjectPtr SystemModule::setCwd(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("setCwd", 1);
    NEXT_STRING(path);

    try {
        filesystem::current_path(path);
        return make_bool(true);
    } catch (const filesystem::filesystem_error&) {
        return make_bool(false);
    }
}

// Function to set command line arguments (called from main)
void set_command_line_args(int argc, char* argv[]) {
    command_line_args.clear();
    for (int i = 0; i < argc; ++i) {
        command_line_args.push_back(string(argv[i]));
    }
}

} // namespace yona::stdlib
