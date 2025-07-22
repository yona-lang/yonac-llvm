#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "runtime.h"
#include "yona_export.h"

namespace yona::stdlib {

using namespace yona::interp::runtime;

// Base class for native modules
class YONA_API NativeModule {
protected:
    std::shared_ptr<ModuleValue> module;

    // Helper to create a native function
    std::shared_ptr<FunctionValue> make_native_function(
        const std::string& name,
        size_t arity,
        std::function<RuntimeObjectPtr(const std::vector<RuntimeObjectPtr>&)> impl
    );

public:
    NativeModule(const std::vector<std::string>& fqn_parts);
    virtual ~NativeModule() = default;

    // Initialize the module with its functions
    virtual void initialize() = 0;

    // Get the module
    std::shared_ptr<ModuleValue> get_module() const { return module; }

    // Get the cache key for this module
    std::string get_cache_key() const;
};

// Registry for all native modules
class YONA_API NativeModuleRegistry {
private:
    static NativeModuleRegistry* instance;
    std::vector<std::unique_ptr<NativeModule>> modules;

    NativeModuleRegistry() = default;
    NativeModuleRegistry(const NativeModuleRegistry&) = delete;
    NativeModuleRegistry& operator=(const NativeModuleRegistry&) = delete;

public:
    static NativeModuleRegistry& get_instance();

    // Register a native module
    void register_module(std::unique_ptr<NativeModule> module);

    // Register all standard native modules
    void register_all_modules();

    // Apply all native modules to the interpreter state
    void apply_to_interpreter(std::unordered_map<std::string, std::shared_ptr<ModuleValue>>& module_cache);
};

} // namespace yona::stdlib
