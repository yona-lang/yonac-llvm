#include "yona_api.h"
#include "Parser.h"
#include "Interpreter.h"
#include "runtime.h"
#include "common.h"
#include <sstream>
#include <fstream>
#include <cstring>

using namespace std;
using namespace yona;
using namespace yona::ast;
using namespace yona::interp;

namespace rt = yona::interp::runtime;

// ===== Internal structures =====

struct YonaValue {
    rt::RuntimeObjectPtr obj;
};

struct YonaInterpreter {
    unique_ptr<Interpreter> interp;
    string last_error;
    yona_status_t last_status = YONA_OK;

    // Keep parse results alive so function AST nodes don't dangle
    vector<parser::ParseResult> parse_results;

    // Registry of host-provided native modules
    struct NativeCallback {
        yona_native_fn fn;
        void* userdata;
        size_t arity;
    };
    // module_fqn -> (func_name -> callback)
    unordered_map<string, unordered_map<string, NativeCallback>> host_modules;

    YonaInterpreter() : interp(make_unique<Interpreter>()) {}

    void set_error(yona_status_t status, const string& msg) {
        last_status = status;
        last_error = msg;
    }

    void clear_error() {
        last_status = YONA_OK;
        last_error.clear();
    }
};

// ===== Helpers =====

static yona_value_t wrap(rt::RuntimeObjectPtr obj) {
    if (!obj) return nullptr;
    return new YonaValue{obj};
}

static rt::RuntimeObjectPtr unwrap(yona_value_t val) {
    if (!val) return nullptr;
    return val->obj;
}

static yona_type_t to_api_type(rt::RuntimeObjectType t) {
    switch (t) {
        case rt::Int:       return YONA_TYPE_INT;
        case rt::Float:     return YONA_TYPE_FLOAT;
        case rt::String:    return YONA_TYPE_STRING;
        case rt::Bool:      return YONA_TYPE_BOOL;
        case rt::Unit:      return YONA_TYPE_UNIT;
        case rt::Symbol:    return YONA_TYPE_SYMBOL;
        case rt::Seq:       return YONA_TYPE_SEQ;
        case rt::Set:       return YONA_TYPE_SET;
        case rt::Dict:      return YONA_TYPE_DICT;
        case rt::Tuple:     return YONA_TYPE_TUPLE;
        case rt::Function:  return YONA_TYPE_FUNCTION;
        case rt::Module:    return YONA_TYPE_MODULE;
        case rt::Record:    return YONA_TYPE_RECORD;
        case rt::Byte:      return YONA_TYPE_BYTE;
        case rt::Char:      return YONA_TYPE_CHAR;
        default:        return YONA_TYPE_OTHER;
    }
}

// ===== Interpreter lifecycle =====

extern "C" {

yona_interp_t yona_create(void) {
    try {
        return new YonaInterpreter();
    } catch (...) {
        return nullptr;
    }
}

void yona_destroy(yona_interp_t interp) {
    delete interp;
}

// ===== Evaluation =====

yona_status_t yona_eval(yona_interp_t interp, const char* code, yona_value_t* out) {
    if (!interp || !code) return YONA_INVALID_ARG;
    interp->clear_error();
    // Reset step counter for each eval (fresh execution budget)
    interp->interp->get_state().step_count = 0;

    try {
        parser::Parser parser;
        istringstream stream(code);
        auto parse_result = parser.parse_input(stream);

        if (!parse_result.node) {
            interp->set_error(YONA_PARSE_ERROR, "Failed to parse expression");
            return YONA_PARSE_ERROR;
        }

        auto result = interp->interp->visit(parse_result.node.get());
        // Keep parse result alive — function closures capture AST node pointers
        interp->parse_results.push_back(std::move(parse_result));
        if (out) *out = wrap(result.value);
        return YONA_OK;
    } catch (const yona_error& e) {
        interp->set_error(YONA_ERROR, e.what());
        return YONA_ERROR;
    } catch (const exception& e) {
        interp->set_error(YONA_ERROR, e.what());
        return YONA_ERROR;
    }
}

yona_status_t yona_eval_file(yona_interp_t interp, const char* path, yona_value_t* out) {
    if (!interp || !path) return YONA_INVALID_ARG;

    ifstream file(path);
    if (!file.is_open()) {
        interp->set_error(YONA_NOT_FOUND, string("Cannot open file: ") + path);
        return YONA_NOT_FOUND;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    return yona_eval(interp, buffer.str().c_str(), out);
}

// ===== Value creation =====

yona_value_t yona_int(int value) {
    return wrap(make_shared<RuntimeObject>(rt::Int, value));
}

yona_value_t yona_float(double value) {
    return wrap(make_shared<RuntimeObject>(rt::Float, value));
}

yona_value_t yona_string(const char* value) {
    if (!value) return wrap(make_shared<RuntimeObject>(rt::String, string("")));
    return wrap(make_shared<RuntimeObject>(rt::String, string(value)));
}

yona_value_t yona_bool(int value) {
    return wrap(make_shared<RuntimeObject>(rt::Bool, value != 0));
}

yona_value_t yona_symbol(const char* name) {
    if (!name) return nullptr;
    return wrap(make_shared<RuntimeObject>(rt::Symbol, make_shared<rt::SymbolValue>(rt::SymbolValue{string(name)})));
}

yona_value_t yona_unit(void) {
    return wrap(make_shared<RuntimeObject>(rt::Unit, nullptr));
}

yona_value_t yona_seq(yona_value_t* elements, size_t count) {
    auto seq = make_shared<rt::SeqValue>();
    if (elements) {
        for (size_t i = 0; i < count; i++) {
            seq->fields.push_back(unwrap(elements[i]));
        }
    }
    return wrap(make_shared<RuntimeObject>(rt::Seq, seq));
}

yona_value_t yona_tuple(yona_value_t* elements, size_t count) {
    auto tuple = make_shared<rt::TupleValue>();
    if (elements) {
        for (size_t i = 0; i < count; i++) {
            tuple->fields.push_back(unwrap(elements[i]));
        }
    }
    return wrap(make_shared<RuntimeObject>(rt::Tuple, tuple));
}

// ===== Value inspection =====

yona_type_t yona_typeof(yona_value_t value) {
    if (!value || !value->obj) return YONA_TYPE_OTHER;
    return to_api_type(value->obj->type);
}

int yona_as_int(yona_value_t value) {
    if (!value || !value->obj || value->obj->type != rt::Int) return 0;
    return value->obj->get<int>();
}

double yona_as_float(yona_value_t value) {
    if (!value || !value->obj || value->obj->type != rt::Float) return 0.0;
    return value->obj->get<double>();
}

const char* yona_as_string(yona_value_t value) {
    if (!value || !value->obj || value->obj->type != rt::String) return "";
    return value->obj->get<string>().c_str();
}

int yona_as_bool(yona_value_t value) {
    if (!value || !value->obj || value->obj->type != rt::Bool) return 0;
    return value->obj->get<bool>() ? 1 : 0;
}

const char* yona_as_symbol(yona_value_t value) {
    if (!value || !value->obj || value->obj->type != rt::Symbol) return "";
    return value->obj->get<shared_ptr<rt::SymbolValue>>()->name.c_str();
}

size_t yona_seq_length(yona_value_t seq) {
    if (!seq || !seq->obj || seq->obj->type != rt::Seq) return 0;
    return seq->obj->get<shared_ptr<rt::SeqValue>>()->fields.size();
}

yona_value_t yona_seq_get(yona_value_t seq, size_t index) {
    if (!seq || !seq->obj || seq->obj->type != rt::Seq) return nullptr;
    auto& fields = seq->obj->get<shared_ptr<rt::SeqValue>>()->fields;
    if (index >= fields.size()) return nullptr;
    return wrap(fields[index]);
}

size_t yona_tuple_length(yona_value_t tuple) {
    if (!tuple || !tuple->obj || tuple->obj->type != rt::Tuple) return 0;
    return tuple->obj->get<shared_ptr<rt::TupleValue>>()->fields.size();
}

yona_value_t yona_tuple_get(yona_value_t tuple, size_t index) {
    if (!tuple || !tuple->obj || tuple->obj->type != rt::Tuple) return nullptr;
    auto& fields = tuple->obj->get<shared_ptr<rt::TupleValue>>()->fields;
    if (index >= fields.size()) return nullptr;
    return wrap(fields[index]);
}

char* yona_to_string(yona_value_t value) {
    if (!value || !value->obj) return strdup("()");
    ostringstream oss;
    oss << *value->obj;
    return strdup(oss.str().c_str());
}

// ===== Function calling =====

yona_status_t yona_call(yona_interp_t interp, yona_value_t func,
                         yona_value_t* args, size_t nargs,
                         yona_value_t* out) {
    if (!interp || !func || !func->obj) return YONA_INVALID_ARG;
    if (func->obj->type != rt::Function) {
        interp->set_error(YONA_TYPE_ERROR, "Cannot call non-function value");
        return YONA_TYPE_ERROR;
    }
    interp->clear_error();

    try {
        auto func_val = func->obj->get<shared_ptr<rt::FunctionValue>>();
        vector<rt::RuntimeObjectPtr> call_args;
        if (args) {
            for (size_t i = 0; i < nargs; i++) {
                call_args.push_back(unwrap(args[i]));
            }
        }

        auto result = func_val->code(call_args);
        if (out) *out = wrap(result);
        return YONA_OK;
    } catch (const yona_error& e) {
        interp->set_error(YONA_ERROR, e.what());
        return YONA_ERROR;
    } catch (const exception& e) {
        interp->set_error(YONA_ERROR, e.what());
        return YONA_ERROR;
    }
}

// ===== Helpers for module FQN =====

static string fqn_to_cache_key(const string& fqn_str) {
    string cache_key;
    string remaining = fqn_str;
    size_t pos;
    while ((pos = remaining.find('\\')) != string::npos) {
        if (!cache_key.empty()) cache_key += "/";
        cache_key += remaining.substr(0, pos);
        remaining = remaining.substr(pos + 1);
    }
    if (!cache_key.empty()) cache_key += "/";
    cache_key += remaining;
    return cache_key;
}

static shared_ptr<rt::ModuleValue> build_host_module(YonaInterpreter* interp, const string& cache_key) {
    auto host_it = interp->host_modules.find(cache_key);
    if (host_it == interp->host_modules.end()) return nullptr;

    auto mod = make_shared<rt::ModuleValue>();
    mod->fqn = make_shared<rt::FqnValue>();
    for (auto& [name, cb] : host_it->second) {
        auto func = make_shared<rt::FunctionValue>();
        func->fqn = make_shared<rt::FqnValue>();
        func->fqn->parts = {name};
        func->arity = cb.arity;
        func->is_native = true;
        auto callback = cb.fn;
        auto userdata = cb.userdata;
        auto interp_ptr = interp;
        func->code = [callback, userdata, interp_ptr](const vector<rt::RuntimeObjectPtr>& args) -> rt::RuntimeObjectPtr {
            vector<yona_value_t> c_args;
            for (auto& a : args) c_args.push_back(new YonaValue{a});
            auto result = callback(interp_ptr, c_args.data(), c_args.size(), userdata);
            for (auto a : c_args) delete a;
            if (!result) {
                throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::RUNTIME,
                                interp_ptr->last_error.empty() ? "Native function returned null" : interp_ptr->last_error);
            }
            auto obj = result->obj;
            delete result;
            return obj;
        };
        mod->exports[name] = func;
    }
    return mod;
}

// ===== Module access =====

yona_status_t yona_import(yona_interp_t interp, const char* module_fqn,
                           yona_value_t* out) {
    if (!interp || !module_fqn) return YONA_INVALID_ARG;
    interp->clear_error();

    string fqn_str(module_fqn);
    string cache_key = fqn_to_cache_key(fqn_str);

    // Check host-registered modules first
    auto host_mod = build_host_module(interp, cache_key);
    if (host_mod) {
        if (out) *out = wrap(make_shared<rt::RuntimeObject>(rt::Module, host_mod));
        return YONA_OK;
    }

    // Try loading via the interpreter (native or file-based modules)
    try {
        string import_code = "import " + fqn_str + " in ()";
        parser::Parser parser;
        istringstream stream(import_code);
        auto parse_result = parser.parse_input(stream);
        if (!parse_result.node) {
            interp->set_error(YONA_NOT_FOUND, string("Cannot parse module FQN: ") + module_fqn);
            return YONA_NOT_FOUND;
        }
        interp->interp->visit(parse_result.node.get());
        interp->parse_results.push_back(std::move(parse_result));
        if (out) *out = nullptr; // Built-in modules available via Yona import
        return YONA_OK;
    } catch (const yona_error& e) {
        interp->set_error(YONA_ERROR, e.what());
        return YONA_ERROR;
    } catch (const exception& e) {
        interp->set_error(YONA_ERROR, e.what());
        return YONA_ERROR;
    }
}

yona_status_t yona_module_get(yona_value_t module, const char* name,
                               yona_value_t* out) {
    if (!module || !module->obj || !name) return YONA_INVALID_ARG;
    if (module->obj->type != rt::Module) return YONA_TYPE_ERROR;

    auto mod = module->obj->get<shared_ptr<rt::ModuleValue>>();
    auto it = mod->exports.find(string(name));
    if (it == mod->exports.end()) return YONA_NOT_FOUND;

    if (out) *out = wrap(make_shared<RuntimeObject>(rt::Function, it->second));
    return YONA_OK;
}

// ===== Native function registration =====

yona_status_t yona_register_function(yona_interp_t interp,
                                      const char* module_fqn,
                                      const char* name,
                                      size_t arity,
                                      yona_native_fn fn,
                                      void* userdata) {
    if (!interp || !module_fqn || !name || !fn) return YONA_INVALID_ARG;

    string cache_key = fqn_to_cache_key(string(module_fqn));
    interp->host_modules[cache_key][string(name)] = {fn, userdata, arity};
    // Access via a simple eval that binds it
    return YONA_OK;
}

// ===== Sandboxing =====

static string normalize_module_pattern(const char* pattern) {
    // Convert backslash-separated FQN to slash-separated cache key, preserve trailing *
    string result;
    string p(pattern);
    size_t pos;
    string remaining = p;
    while ((pos = remaining.find('\\')) != string::npos) {
        if (!result.empty()) result += "/";
        result += remaining.substr(0, pos);
        remaining = remaining.substr(pos + 1);
    }
    if (!result.empty()) result += "/";
    result += remaining;
    return result;
}

yona_status_t yona_sandbox_set_mode(yona_interp_t interp, yona_sandbox_mode_t mode) {
    if (!interp) return YONA_INVALID_ARG;
    auto& sandbox = interp->interp->get_state().sandbox;
    switch (mode) {
        case YONA_SANDBOX_NONE: sandbox.mode = InterpreterState::SandboxConfig::NONE; break;
        case YONA_SANDBOX_WHITELIST: sandbox.mode = InterpreterState::SandboxConfig::WHITELIST; break;
        case YONA_SANDBOX_BLACKLIST: sandbox.mode = InterpreterState::SandboxConfig::BLACKLIST; break;
        default: return YONA_INVALID_ARG;
    }
    return YONA_OK;
}

yona_status_t yona_sandbox_allow_module(yona_interp_t interp, const char* pattern) {
    if (!interp || !pattern) return YONA_INVALID_ARG;
    interp->interp->get_state().sandbox.allowed_modules.insert(normalize_module_pattern(pattern));
    return YONA_OK;
}

yona_status_t yona_sandbox_deny_module(yona_interp_t interp, const char* pattern) {
    if (!interp || !pattern) return YONA_INVALID_ARG;
    interp->interp->get_state().sandbox.denied_modules.insert(normalize_module_pattern(pattern));
    return YONA_OK;
}

yona_status_t yona_set_execution_limit(yona_interp_t interp, size_t max_steps) {
    if (!interp) return YONA_INVALID_ARG;
    interp->interp->get_state().sandbox.execution_limit = max_steps;
    return YONA_OK;
}

yona_status_t yona_set_memory_limit(yona_interp_t interp, size_t max_bytes) {
    if (!interp) return YONA_INVALID_ARG;
    interp->interp->get_state().sandbox.memory_limit = max_bytes;
    return YONA_OK;
}

// ===== Configuration =====

yona_status_t yona_add_module_path(yona_interp_t interp, const char* path) {
    if (!interp || !path) return YONA_INVALID_ARG;

    // Evaluate a dummy expression to ensure the interpreter is initialized,
    // then we'd need to access IS.module_paths. Since we can't directly,
    // set YONA_PATH environment variable instead.
    // TODO: Add direct module path API to Interpreter class
    return YONA_OK;
}

// ===== Error handling =====

const char* yona_error_message(yona_interp_t interp) {
    if (!interp) return "Invalid interpreter handle";
    return interp->last_error.c_str();
}

void yona_set_error(yona_interp_t interp, const char* message) {
    if (!interp || !message) return;
    interp->set_error(YONA_ERROR, string(message));
}

// ===== Memory management =====

void yona_value_free(yona_value_t value) {
    delete value;
}

void yona_string_free(char* str) {
    free(str);
}

} // extern "C"
