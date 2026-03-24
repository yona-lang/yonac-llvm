#include "stdlib/native_module.h"
#include "stdlib/io_module.h"
#include "stdlib/system_module.h"
#include "stdlib/math_module.h"
#include "stdlib/list_module.h"
#include "stdlib/option_module.h"
#include "stdlib/result_module.h"
#include "stdlib/tuple_module.h"
#include "stdlib/range_module.h"
#include "stdlib/string_module.h"
#include "stdlib/set_module.h"
#include "stdlib/dict_module.h"
#include "stdlib/timer_module.h"
#include "stdlib/http_module.h"
#include "stdlib/json_module.h"
#include "stdlib/regexp_module.h"
#include "stdlib/file_module.h"
#include "stdlib/random_module.h"
#include "stdlib/time_module.h"
#include "stdlib/types_module.h"
#include <sstream>

namespace yona::stdlib {

NativeModule::NativeModule(const std::vector<std::string>& fqn_parts) {
    module = std::make_shared<ModuleValue>();
    module->fqn = std::make_shared<FqnValue>();
    module->fqn->parts = fqn_parts;
}

std::shared_ptr<FunctionValue> NativeModule::make_native_function(
    const std::string& name,
    size_t arity,
    std::function<RuntimeObjectPtr(const std::vector<RuntimeObjectPtr>&)> impl
) {
    auto func = std::make_shared<FunctionValue>();
    func->fqn = std::make_shared<FqnValue>();
    func->fqn->parts = {name};
    func->arity = arity;
    func->code = impl;
    func->is_native = true;
    return func;
}

std::shared_ptr<FunctionValue> NativeModule::make_native_async_function(
    const std::string& name,
    size_t arity,
    std::function<RuntimeObjectPtr(const std::vector<RuntimeObjectPtr>&)> impl
) {
    auto func = make_native_function(name, arity, impl);
    func->is_async = true;
    return func;
}

std::string NativeModule::get_cache_key() const {
    std::stringstream ss;
    for (size_t i = 0; i < module->fqn->parts.size(); ++i) {
        if (i > 0) ss << "/";
        ss << module->fqn->parts[i];
    }
    return ss.str();
}

// Registry implementation
NativeModuleRegistry* NativeModuleRegistry::instance = nullptr;

NativeModuleRegistry& NativeModuleRegistry::get_instance() {
    if (!instance) {
        instance = new NativeModuleRegistry();
    }
    return *instance;
}

void NativeModuleRegistry::register_module(std::unique_ptr<NativeModule> module) {
    modules.push_back(std::move(module));
}

void NativeModuleRegistry::register_all_modules() {
    // Register all standard native modules
    register_module(std::make_unique<IOModule>());
    register_module(std::make_unique<SystemModule>());
    register_module(std::make_unique<MathModule>());
    register_module(std::make_unique<ListModule>());
    register_module(std::make_unique<OptionModule>());
    register_module(std::make_unique<ResultModule>());
    register_module(std::make_unique<TupleModule>());
    register_module(std::make_unique<RangeModule>());
    register_module(std::make_unique<StringModule>());
    register_module(std::make_unique<SetModule>());
    register_module(std::make_unique<DictModule>());
    register_module(std::make_unique<TimerModule>());
    register_module(std::make_unique<HttpModule>());
    register_module(std::make_unique<JsonModule>());
    register_module(std::make_unique<RegexpModule>());
    register_module(std::make_unique<FileModule>());
    register_module(std::make_unique<RandomModule>());
    register_module(std::make_unique<TimeModule>());
    register_module(std::make_unique<TypesModule>());
}

void NativeModuleRegistry::apply_to_interpreter(
    std::unordered_map<std::string, std::shared_ptr<ModuleValue>>& module_cache
) {
    for (auto& native_module : modules) {
        native_module->initialize();
        auto mod = native_module->get_module();
        mod->is_native = true;
        module_cache[native_module->get_cache_key()] = mod;
    }
}

} // namespace yona::stdlib
