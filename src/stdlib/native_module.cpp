#include "stdlib/native_module.h"
#include "stdlib/io_module.h"
#include "stdlib/system_module.h"
#include "stdlib/math_module.h"
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
}

void NativeModuleRegistry::apply_to_interpreter(
    std::unordered_map<std::string, std::shared_ptr<ModuleValue>>& module_cache
) {
    for (auto& native_module : modules) {
        native_module->initialize();
        module_cache[native_module->get_cache_key()] = native_module->get_module();
    }
}

} // namespace yona::stdlib
