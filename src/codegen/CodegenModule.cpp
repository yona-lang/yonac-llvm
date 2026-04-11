//
// Codegen — Module system code generation
//
// Interface file I/O, FQN resolution, imports, extern declarations.
//

#include "Codegen.h"
#include "Parser.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <map>

namespace yona::compiler::codegen {
using namespace llvm;
using LType = llvm::Type;

// Forward declarations for type annotation support
static CType yona_type_to_ctype(const types::Type& t);
static std::string yona_type_adt_name(const types::Type& t);
static std::pair<std::vector<CType>, CType> uncurry_type_signature(const types::Type& t);

std::string Codegen::ctype_to_type_name(CType ct) {
    switch (ct) {
        case CType::INT: return "Int";
        case CType::FLOAT: return "Float";
        case CType::BOOL: return "Bool";
        case CType::STRING: return "String";
        case CType::SYMBOL: return "Symbol";
        case CType::SEQ: return "Seq";
        case CType::SET: return "Set";
        case CType::DICT: return "Dict";
        case CType::TUPLE: return "Tuple";
        case CType::UNIT: return "Unit";
        case CType::FUNCTION: return "Function";
        case CType::PROMISE: return "Promise";
        case CType::ADT: return "ADT";
        case CType::BYTE_ARRAY: return "ByteArray";
        case CType::INT_ARRAY: return "IntArray";
        case CType::FLOAT_ARRAY: return "FloatArray";
        case CType::CHANNEL: return "Channel";
    }
    return "Int";
}

std::string Codegen::resolve_trait_method(const std::string& method_name, CType arg_type,
                                           const std::string& adt_type_name) {
    std::string type_name = ctype_to_type_name(arg_type);

    // Phase 2: For ADT types, use the specific ADT type name instead of generic "ADT"
    if (arg_type == CType::ADT && !adt_type_name.empty()) {
        type_name = adt_type_name;
    }

    // Search all trait instances for one that provides this method for the given type
    for (auto& [key, inst] : types_.trait_instances) {
        if (inst.type_name == type_name) {
            auto it = inst.method_mangled_names.find(method_name);
            if (it != inst.method_mangled_names.end()) {
                return it->second;
            }
        }
    }

    // Also check if trait registry has this method (for cross-module resolution)
    for (auto& [trait_name, trait] : types_.traits) {
        for (auto& mn : trait.method_names) {
            if (mn == method_name) {
                // Found the trait, now look for instance
                std::string key = trait_name + ":" + type_name;
                auto inst_it = types_.trait_instances.find(key);
                if (inst_it != types_.trait_instances.end()) {
                    auto meth_it = inst_it->second.method_mangled_names.find(method_name);
                    if (meth_it != inst_it->second.method_mangled_names.end())
                        return meth_it->second;
                }
            }
        }
    }

    return "";
}

static std::string ctype_to_string(CType ct) {
    switch (ct) {
        case CType::INT: return "INT";
        case CType::FLOAT: return "FLOAT";
        case CType::BOOL: return "BOOL";
        case CType::STRING: return "STRING";
        case CType::SEQ: return "SEQ";
        case CType::TUPLE: return "TUPLE";
        case CType::UNIT: return "UNIT";
        case CType::FUNCTION: return "FUNCTION";
        case CType::SYMBOL: return "SYMBOL";
        case CType::PROMISE: return "PROMISE";
        case CType::SET: return "SET";
        case CType::DICT: return "DICT";
        case CType::ADT: return "ADT";
    }
    return "INT";
}

static CType string_to_ctype(const std::string& s) {
    if (s == "INT") return CType::INT;
    if (s == "FLOAT") return CType::FLOAT;
    if (s == "BOOL") return CType::BOOL;
    if (s == "STRING") return CType::STRING;
    if (s == "SEQ") return CType::SEQ;
    if (s == "TUPLE") return CType::TUPLE;
    if (s == "UNIT") return CType::UNIT;
    if (s == "FUNCTION") return CType::FUNCTION;
    if (s == "SYMBOL") return CType::SYMBOL;
    if (s == "PROMISE") return CType::PROMISE;
    if (s == "SET") return CType::SET;
    if (s == "DICT") return CType::DICT;
    if (s == "ADT") return CType::ADT;
    if (s == "BYTE_ARRAY") return CType::BYTE_ARRAY;
    if (s == "INT_ARRAY") return CType::INT_ARRAY;
    if (s == "FLOAT_ARRAY") return CType::FLOAT_ARRAY;
    if (s == "CHANNEL") return CType::CHANNEL;
    if (s == "SUM") return CType::SUM;
    if (s == "RECORD") return CType::RECORD;
    return CType::INT;
}

bool Codegen::emit_interface_file(const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    // Write ADT definitions
    // Group constructors by type name
    std::map<std::string, std::vector<const AdtInfo*>> adts;
    for (auto& [name, info] : types_.adt_constructors)
        adts[info.type_name].push_back(&info);

    for (auto& [type_name, ctors] : adts) {
        int max_arity = 0;
        bool is_recursive = false;
        for (auto* c : ctors) {
            if (c->arity > max_arity) max_arity = c->arity;
            if (c->is_recursive) is_recursive = true;
        }
        out << "ADT " << type_name << " " << ctors.size() << " " << max_arity
            << (is_recursive ? " recursive" : "") << "\n";
        for (auto* ctor : ctors) {
            for (auto& [cname, cinfo] : types_.adt_constructors) {
                if (&cinfo == ctor)
                    out << "CTOR " << cname << " " << ctor->tag << " " << ctor->arity;
                    if (!ctor->field_names.empty()) {
                        out << " fields";
                        for (size_t fi = 0; fi < ctor->field_names.size(); fi++)
                            out << " " << ctor->field_names[fi] << ":" << ctype_to_string(
                                fi < ctor->field_types.size() ? ctor->field_types[fi] : CType::INT);
                    }
                    out << "\n";
            }
        }
    }

    // Write trait definitions
    for (auto& [name, trait] : types_.traits) {
        out << "TRAIT " << name;
        for (auto& tp : trait.type_params) out << " " << tp;
        if (trait.type_params.empty()) out << " " << trait.type_param;
        out << " " << trait.method_names.size() << "\n";
        for (auto& method : trait.method_names) {
            out << "  METHOD " << method << "\n";
        }
    }

    // Write trait instances
    for (auto& [key, inst] : types_.trait_instances) {
        out << "INSTANCE " << inst.trait_name;
        for (auto& tn : inst.type_names) out << " " << tn;
        if (inst.type_names.empty()) out << " " << inst.type_name;
        out << "\n";
        for (auto& [method, mangled_name] : inst.method_mangled_names) {
            out << "  IMPL " << method << " " << mangled_name << "\n";
        }
    }

    // Write function signatures
    for (auto& [mangled, meta] : imports_.meta) {
        out << "FN " << mangled << " " << meta.param_types.size();
        for (auto ct : meta.param_types) out << " " << ctype_to_string(ct);
        out << " -> " << ctype_to_string(meta.return_type) << "\n";
    }

    // Write generic function source for cross-module monomorphization
    for (auto& [mangled, source] : imports_.function_source) {
        // Extract local name from mangled: yona_Pkg_Mod__funcname -> funcname
        auto pos = mangled.rfind("__");
        std::string local_name = (pos != std::string::npos) ? mangled.substr(pos + 2) : mangled;
        out << "GENFN_BEGIN " << mangled << " " << local_name << "\n";
        out << source << "\n";
        out << "GENFN_END\n";
    }

    return true;
}

bool Codegen::load_interface_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    std::string current_adt;
    int current_total_variants = 0;
    int current_max_arity = 0;
    bool current_is_recursive = false;
    std::vector<std::string> current_ctor_names;
    std::string last_instance_key;  // tracks most recently registered INSTANCE
    std::string last_trait_name;    // tracks most recently registered TRAIT

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "ADT") {
            // Finalize previous ADT's total_variants/max_arity
            for (auto& cn : current_ctor_names) {
                types_.adt_constructors[cn].total_variants = current_total_variants;
                types_.adt_constructors[cn].max_arity = current_max_arity;
            }
            current_ctor_names.clear();

            iss >> current_adt >> current_total_variants >> current_max_arity;
            std::string recursive_flag;
            if (iss >> recursive_flag && recursive_flag == "recursive")
                current_is_recursive = true;
            else
                current_is_recursive = false;
        } else if (keyword == "CTOR") {
            std::string name;
            int tag, arity;
            iss >> name >> tag >> arity;
            std::vector<std::string> fnames;
            std::vector<CType> ftypes;
            std::string token;
            if (iss >> token && token == "fields") {
                while (iss >> token) {
                    auto colon = token.find(':');
                    if (colon != std::string::npos) {
                        fnames.push_back(token.substr(0, colon));
                        ftypes.push_back(string_to_ctype(token.substr(colon + 1)));
                    }
                }
            }
            types_.adt_constructors[name] = {current_adt, tag, arity, current_total_variants,
                                        current_max_arity, current_is_recursive, fnames, ftypes};
            current_ctor_names.push_back(name);
        } else if (keyword == "TRAIT") {
            // Format: TRAIT name param1 [param2 ...] method_count
            std::string name;
            iss >> name;
            std::vector<std::string> tokens;
            std::string tok;
            while (iss >> tok) tokens.push_back(tok);
            // Last token is method_count (integer), rest are type params
            TraitInfo ti;
            ti.name = name;
            if (!tokens.empty()) {
                for (size_t i = 0; i + 1 < tokens.size(); i++)
                    ti.type_params.push_back(tokens[i]);
                if (!ti.type_params.empty())
                    ti.type_param = ti.type_params[0];
            }
            types_.traits[name] = ti;
            last_trait_name = name;
        } else if (keyword == "METHOD") {
            std::string method_name;
            iss >> method_name;
            // Add to the most recently registered TRAIT
            if (!last_trait_name.empty()) {
                auto it = types_.traits.find(last_trait_name);
                if (it != types_.traits.end())
                    it->second.method_names.push_back(method_name);
            }
        } else if (keyword == "INSTANCE") {
            // Format: INSTANCE trait_name type1 [type2 ...]
            std::string trait_name;
            iss >> trait_name;
            std::vector<std::string> type_names;
            std::string tn;
            while (iss >> tn) type_names.push_back(tn);
            std::string type_name = type_names.empty() ? "" : type_names[0];
            std::string key = trait_name;
            for (auto& t : type_names) key += ":" + t;
            TraitInstanceInfo tii;
            tii.trait_name = trait_name;
            tii.type_name = type_name;
            tii.type_names = type_names;
            types_.trait_instances[key] = tii;
            last_instance_key = key;
        } else if (keyword == "IMPL") {
            std::string method_name, mangled_name;
            iss >> method_name >> mangled_name;
            // Add to the most recently registered INSTANCE
            if (!last_instance_key.empty()) {
                auto it = types_.trait_instances.find(last_instance_key);
                if (it != types_.trait_instances.end())
                    it->second.method_mangled_names[method_name] = mangled_name;
            }
        } else if (keyword == "FN" || keyword == "AFN" || keyword == "IO") {
            bool is_thread_async = (keyword == "AFN");
            bool is_io_async = (keyword == "IO");
            std::string mangled;
            int param_count;
            iss >> mangled >> param_count;

            ModuleFunctionMeta meta;
            for (int i = 0; i < param_count; i++) {
                std::string type_str;
                iss >> type_str;
                meta.param_types.push_back(string_to_ctype(type_str));
            }
            std::string arrow;
            iss >> arrow; // "->"
            std::string ret_str;
            iss >> ret_str;
            bool is_any_async = is_thread_async || is_io_async;
            meta.return_type = is_any_async ? CType::PROMISE : string_to_ctype(ret_str);
            meta.is_async = is_thread_async;
            meta.is_io_async = is_io_async;
            if (is_any_async) meta.async_inner_type = string_to_ctype(ret_str);

            imports_.meta[mangled] = meta;
        } else if (keyword == "GENFN_BEGIN") {
            // Parse: GENFN_BEGIN mangled_name local_name
            std::string mangled, local_name;
            iss >> mangled >> local_name;
            std::string source;
            while (std::getline(in, line)) {
                if (line == "GENFN_END") break;
                if (!source.empty()) source += "\n";
                source += line;
            }
            imports_.imported_sources[mangled] = {source, local_name};
        }
    }
    return true;
}

// Build FQN string and filesystem path from an FqnExpr
std::pair<std::string, std::filesystem::path> Codegen::build_fqn_path(FqnExpr* fqn) {
    std::string mod_fqn;
    std::filesystem::path mod_path;
    if (fqn->packageName.has_value()) {
        auto* pkg = fqn->packageName.value();
        for (size_t i = 0; i < pkg->parts.size(); i++) {
            if (i > 0) mod_fqn += "\\";
            mod_fqn += pkg->parts[i]->value;
            mod_path /= pkg->parts[i]->value;
        }
        mod_fqn += "\\";
    }
    mod_fqn += fqn->moduleName->value;
    mod_path /= fqn->moduleName->value;
    return {mod_fqn, mod_path};
}

// Load .yonai interface file for a module, falling back to .yona source.
void Codegen::load_module_interface(const std::filesystem::path& mod_path) {
    auto yonai_name = mod_path;
    yonai_name.replace_extension(".yonai");
    for (auto& search_path : module_paths_) {
        auto candidate = std::filesystem::path(search_path) / yonai_name;
        if (std::filesystem::exists(candidate)) {
            load_interface_file(candidate.string());
            return;
        }
    }
    // Fallback: pure-Yona stdlib source. Parse and register declarations.
    auto yona_name = mod_path;
    yona_name.replace_extension(".yona");
    for (auto& search_path : module_paths_) {
        auto candidate = std::filesystem::path(search_path) / yona_name;
        if (std::filesystem::exists(candidate)) {
            load_yona_module(candidate);
            return;
        }
    }
}

bool Codegen::load_yona_module(const std::filesystem::path& yona_path) {
    auto canonical = std::filesystem::canonical(yona_path).string();
    if (loaded_yona_paths_.count(canonical)) return true; // already loaded
    loaded_yona_paths_.insert(canonical);

    std::ifstream in(yona_path);
    if (!in.is_open()) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    parser::Parser parser;
    // Pre-register known constructors so pattern matching parses correctly.
    for (auto& [name, info] : types_.adt_constructors)
        parser.register_constructor(name, info.type_name, info.tag, info.arity, info.field_names);

    auto result = parser.parse_module(source, yona_path.string());
    if (!result.has_value()) {
        if (diag_) {
            for (auto& e : result.error())
                diag_->error(e.location, compiler::ErrorCode::E0301,
                             "in stdlib module " + yona_path.filename().string() + ": " + e.message);
        }
        return false;
    }

    auto* mod_ptr = result.value().get();
    // Keep AST alive — deferred functions hold pointers into it.
    loaded_yona_modules_.push_back(std::move(result.value()));
    register_yona_module_decls(mod_ptr);
    return true;
}

void Codegen::register_yona_module_decls(ast::ModuleDecl* mod) {
    // ===== ADTs =====
    for (auto* adt : mod->adt_declarations) {
        int max_arity = 0;
        bool is_recursive = false;
        for (auto* ctor : adt->variants) {
            int a = static_cast<int>(ctor->field_type_names.size());
            if (a > max_arity) max_arity = a;
            for (auto& ft : ctor->field_type_names) {
                if (ft.is_function_type || ft.name == adt->name ||
                    ft.name == "Fn" || ft.name == "fn" || ft.name == "Function") {
                    is_recursive = true; break;
                }
            }
        }
        for (size_t ci = 0; ci < adt->variants.size(); ci++) {
            auto* ctor = adt->variants[ci];
            int arity = static_cast<int>(ctor->field_type_names.size());
            std::vector<CType> ftypes;
            for (auto& ft : ctor->field_type_names) {
                if (ft.is_function_type) ftypes.push_back(CType::FUNCTION);
                else if (ft.name == "Int") ftypes.push_back(CType::INT);
                else if (ft.name == "Float") ftypes.push_back(CType::FLOAT);
                else if (ft.name == "String") ftypes.push_back(CType::STRING);
                else if (ft.name == "Bool") ftypes.push_back(CType::BOOL);
                else if (ft.name == "Symbol") ftypes.push_back(CType::SYMBOL);
                else if (ft.name == "Channel") ftypes.push_back(CType::CHANNEL);
                else if (ft.name == adt->name) ftypes.push_back(CType::ADT);
                else if (ft.name == "()") ftypes.push_back(CType::FUNCTION);
                else ftypes.push_back(CType::INT);
            }
            // Don't overwrite if already registered (e.g., from Prelude)
            if (types_.adt_constructors.find(ctor->name) == types_.adt_constructors.end()) {
                types_.adt_constructors[ctor->name] = {adt->name, static_cast<int>(ci), arity,
                    static_cast<int>(adt->variants.size()), max_arity, is_recursive,
                    ctor->field_names, ftypes};
            }
        }
    }

    // ===== Trait declarations =====
    for (auto* trait : mod->trait_declarations) {
        if (types_.traits.count(trait->name)) continue;
        TraitInfo ti;
        ti.name = trait->name;
        ti.type_param = trait->type_param;
        ti.type_params = trait->type_params;
        for (auto& m : trait->methods) {
            ti.method_names.push_back(m.name);
            if (m.default_impl) ti.default_impls[m.name] = m.default_impl;
        }
        types_.traits[trait->name] = ti;
    }

    // ===== Trait instances =====
    for (auto* inst : mod->instance_declarations) {
        std::string key = inst->trait_name;
        for (auto& tn : inst->type_names) key += ":" + tn;
        if (inst->type_names.empty()) key += ":" + inst->type_name;
        if (types_.trait_instances.count(key)) continue;
        TraitInstanceInfo tii;
        tii.trait_name = inst->trait_name;
        tii.type_name = inst->type_name;
        tii.type_names = inst->type_names;
        for (auto* method : inst->methods) {
            std::string mangled = inst->trait_name + "_" + inst->type_name + "__" + method->name;
            tii.method_mangled_names[method->name] = mangled;
            codegen_function_def(method, mangled);
        }
        types_.trait_instances[key] = tii;
    }

    // ===== Extern declarations =====
    for (auto* ext : mod->extern_declarations) {
        codegen_extern_decl(ext);
    }

    // ===== Functions (all deferred — compile on demand at call sites) =====
    for (auto* func : mod->functions) {
        codegen_function_def(func, func->name);
    }
}

std::unique_ptr<ast::ModuleDecl> Codegen::reparse_genfn(
    const std::string& local_name, const std::string& source_text) {
    parser::Parser parser;
    // Register known constructors so pattern matching parses correctly
    for (auto& [name, info] : types_.adt_constructors) {
        parser.register_constructor(name, info.type_name, info.tag, info.arity, info.field_names);
    }
    std::string mod_source = "module __Import\nexport " + local_name + "\n" + source_text + "\n";
    auto result = parser.parse_module(mod_source, "<imported>");
    if (result.has_value()) return std::move(result.value());
    return nullptr;
}

// Helper: get the LLVM type for an ADT based on its type name.
// Non-recursive ADTs use flat struct {i64 tag, i64 * max_arity}.
// Recursive ADTs use a pointer (heap-allocated).
LType* Codegen::adt_llvm_type(const std::string& type_name) {
    auto i64_ty = LType::getInt64Ty(*context_);
    // Find a constructor for this ADT type to get struct info
    for (auto& [name, info] : types_.adt_constructors) {
        if (info.type_name == type_name) {
            if (info.is_recursive)
                return PointerType::get(*context_, 0);
            std::vector<LType*> fields = {i64_ty};
            for (int f = 0; f < info.max_arity; f++) fields.push_back(i64_ty);
            return StructType::get(*context_, fields);
        }
    }
    return i64_ty; // fallback
}

// Register trait instance methods as extern function declarations
// so that re-parsed GENFN bodies can call them via trait dispatch.
void Codegen::register_trait_externs() {
    for (auto& [key, inst] : types_.trait_instances) {
        for (auto& [method_name, mangled] : inst.method_mangled_names) {
            if (compiled_functions_.count(mangled) > 0) continue;
            auto meta_it = imports_.meta.find(mangled);
            if (meta_it != imports_.meta.end()) {
                auto& meta = meta_it->second;
                std::vector<LType*> param_types;
                for (size_t i = 0; i < meta.param_types.size(); i++) {
                    if (meta.param_types[i] == CType::ADT) {
                        param_types.push_back(adt_llvm_type(inst.type_name));
                    } else {
                        param_types.push_back(llvm_type(meta.param_types[i]));
                    }
                }
                auto* ret_llvm = (meta.return_type == CType::ADT)
                    ? adt_llvm_type(inst.type_name)
                    : llvm_type(meta.return_type);
                auto* fn_type = llvm::FunctionType::get(ret_llvm, param_types, false);
                auto* fn = module_->getFunction(mangled);
                if (!fn)
                    fn = Function::Create(fn_type, Function::ExternalLinkage, mangled, module_.get());
                CompiledFunction cf;
                cf.fn = fn;
                cf.return_type = meta.return_type;
                cf.param_types = meta.param_types;
                compiled_functions_[mangled] = cf;
            }
        }
    }
}

// Register a single imported function/constructor by name
void Codegen::register_import(const std::string& mod_fqn,
                               const std::string& func_name,
                               const std::string& import_name) {
    // Check if it's an ADT constructor
    auto ctor_it = types_.adt_constructors.find(func_name);
    if (ctor_it != types_.adt_constructors.end()) {
        if (ctor_it->second.arity > 0)
            named_values_[import_name] = {nullptr, CType::FUNCTION};
        if (import_name != func_name)
            types_.adt_constructors[import_name] = ctor_it->second;
        return;
    }

    // .yona stdlib fallback: function already registered as deferred
    // (e.g., from a Std/X.yona file loaded by load_module_interface).
    // Just alias the name if needed; resolution finds it via deferred_functions_.
    auto def_it = deferred_functions_.find(func_name);
    if (def_it != deferred_functions_.end()) {
        if (import_name != func_name)
            deferred_functions_[import_name] = def_it->second;
        return;
    }

    std::string mangled = mangle_name(mod_fqn, func_name);

    // Register as extern — the pre-compiled version from the module is the default.
    // GENFN source (if available) is stored in imports_.imported_sources for
    // on-demand monomorphization when call-site types differ from the module's.
    auto meta_it = imports_.meta.find(mangled);
    if (meta_it != imports_.meta.end() && meta_it->second.is_io_async) {
        // IO: C function submits to io_uring and returns i64 (uring ID).
        // Called directly (no thread pool). Result tagged as PROMISE.
        // auto_await calls yona_rt_io_await to complete.
        auto& meta = meta_it->second;
        auto i64_ty = LType::getInt64Ty(*context_);
        std::vector<LType*> param_types;
        for (auto ct : meta.param_types) param_types.push_back(llvm_type(ct));
        auto* fn_type = llvm::FunctionType::get(i64_ty, param_types, false);
        auto* fn = module_->getFunction(mangled);
        if (!fn) fn = Function::Create(fn_type, Function::ExternalLinkage, mangled, module_.get());
        CompiledFunction cf;
        cf.fn = fn;
        cf.return_type = CType::PROMISE;
        cf.param_types = meta.param_types;
        cf.is_io_async = true;
        compiled_functions_[import_name] = cf;
        named_values_[import_name] = {fn, CType::FUNCTION, {meta.async_inner_type}};
    } else if (meta_it != imports_.meta.end() && meta_it->second.is_async) {
        // AFN: thread-pool async (existing mechanism)
        auto& meta = meta_it->second;
        std::vector<LType*> param_types;
        for (auto ct : meta.param_types) param_types.push_back(llvm_type(ct));
        auto* fn_type = llvm::FunctionType::get(llvm_type(meta.async_inner_type), param_types, false);
        auto* fn = module_->getFunction(mangled);
        if (!fn) fn = Function::Create(fn_type, Function::ExternalLinkage, mangled, module_.get());
        CompiledFunction cf;
        cf.fn = fn;
        cf.return_type = CType::PROMISE;
        cf.param_types = meta.param_types;
        compiled_functions_[import_name] = cf;
        named_values_[import_name] = {fn, CType::FUNCTION, {meta.async_inner_type}};
    } else if (meta_it != imports_.meta.end() && meta_it->second.param_types.empty()) {
        // Zero-arity function: create extern declaration so it can be called.
        // Don't set named_values_ — let codegen_identifier find it in compiled_functions_
        // and return it as a callable function reference.
        auto& meta = meta_it->second;
        auto* ret_ty = llvm_type(meta.return_type);
        auto* fn_type = llvm::FunctionType::get(ret_ty, {}, false);
        auto* fn = module_->getFunction(mangled);
        if (!fn) fn = Function::Create(fn_type, Function::ExternalLinkage, mangled, module_.get());
        CompiledFunction cf;
        cf.fn = fn;
        cf.return_type = meta.return_type;
        compiled_functions_[import_name] = cf;
        imports_.extern_functions[import_name] = mangled;
    } else {
        named_values_[import_name] = {nullptr, CType::FUNCTION};
        imports_.extern_functions[import_name] = mangled;
    }
}

// Register ALL exports from a loaded .yonai (wildcard import)
void Codegen::register_all_imports(const std::string& mod_fqn) {
    // For .yona fallback modules, deferred_functions_ already holds them by
    // local name. No additional setup needed — call sites resolve directly.

    // Register all functions from imports_.meta
    for (auto& [mangled, meta] : imports_.meta) {
        // Extract function name from mangled: yona_Pkg_Mod__func -> func
        auto pos = mangled.rfind("__");
        if (pos != std::string::npos) {
            std::string func_name = mangled.substr(pos + 2);
            // Only register if this function belongs to this module
            std::string expected_prefix = "yona_";
            for (char c : mod_fqn) expected_prefix += (c == '\\') ? '_' : c;
            expected_prefix += "__";
            if (mangled.find(expected_prefix) == 0) {
                if (meta.is_io_async) {
                    // IO: submit-and-return, returns i64 uring ID
                    auto i64_ty = LType::getInt64Ty(*context_);
                    std::vector<LType*> param_types;
                    for (auto ct : meta.param_types) param_types.push_back(llvm_type(ct));
                    auto* fn_type = llvm::FunctionType::get(i64_ty, param_types, false);
                    auto* fn = module_->getFunction(mangled);
                    if (!fn) fn = Function::Create(fn_type, Function::ExternalLinkage, mangled, module_.get());
                    CompiledFunction cf;
                    cf.fn = fn;
                    cf.return_type = CType::PROMISE;
                    cf.param_types = meta.param_types;
                    cf.is_io_async = true;
                    compiled_functions_[func_name] = cf;
                    named_values_[func_name] = {fn, CType::FUNCTION, {meta.async_inner_type}};
                } else if (meta.is_async) {
                    // AFN: thread-pool async
                    std::vector<LType*> param_types;
                    for (auto ct : meta.param_types) param_types.push_back(llvm_type(ct));
                    auto* fn_type = llvm::FunctionType::get(llvm_type(meta.async_inner_type), param_types, false);
                    auto* fn = module_->getFunction(mangled);
                    if (!fn) fn = Function::Create(fn_type, Function::ExternalLinkage, mangled, module_.get());
                    CompiledFunction cf;
                    cf.fn = fn;
                    cf.return_type = CType::PROMISE;
                    cf.param_types = meta.param_types;
                    compiled_functions_[func_name] = cf;
                    named_values_[func_name] = {fn, CType::FUNCTION, {meta.async_inner_type}};
                } else {
                    named_values_[func_name] = {nullptr, CType::FUNCTION};
                    imports_.extern_functions[func_name] = mangled;
                }
            }
        }
    }
    // Register all constructors
    for (auto& [name, info] : types_.adt_constructors) {
        if (info.type_name.find(mod_fqn) != std::string::npos ||
            imports_.meta.count(mangle_name(mod_fqn, name)) > 0) {
            // Already registered by load_interface_file
        }
    }
}

TypedValue Codegen::codegen_import(ImportExpr* node) {
    for (auto* clause : node->clauses) {
        if (clause->get_type() == AST_FUNCTIONS_IMPORT) {
            auto* fi = static_cast<FunctionsImport*>(clause);
            auto [mod_fqn, mod_path] = build_fqn_path(fi->fromFqn);
            load_module_interface(mod_path);

            for (auto* alias : fi->aliases) {
                std::string func_name = alias->name->value;
                std::string import_name = alias->alias ? alias->alias->value : func_name;
                register_import(mod_fqn, func_name, import_name);
            }
            register_trait_externs();
        } else if (clause->get_type() == AST_MODULE_IMPORT) {
            // Wildcard import: import Std\List in expr
            auto* mi = static_cast<ModuleImport*>(clause);
            auto [mod_fqn, mod_path] = build_fqn_path(mi->fqn);
            load_module_interface(mod_path);
            register_all_imports(mod_fqn);
            register_trait_externs();
        }
    }

    return codegen(node->expr);
}

TypedValue Codegen::codegen_extern_decl(ExternDeclExpr* node) {
    // Extract parameter types and return type from the declared type
    std::vector<LType*> param_types;
    std::vector<CType> param_ctypes;
    CType ret_ctype = CType::INT;

    auto current_type = node->declared_type;

    // Uncurry: A -> B -> C becomes params=[A, B], ret=C
    while (std::holds_alternative<std::shared_ptr<types::FunctionType>>(current_type)) {
        auto ft = std::get<std::shared_ptr<types::FunctionType>>(current_type);
        CType param_ct = yona_type_to_ctype(ft->argumentType);
        param_ctypes.push_back(param_ct);
        param_types.push_back(llvm_type(param_ct));
        current_type = ft->returnType;
    }
    ret_ctype = yona_type_to_ctype(current_type);
    std::string ret_adt_name = (ret_ctype == CType::ADT)
        ? yona_type_adt_name(current_type) : "";
    // For ADT returns, externs always come from the C runtime returning a
    // heap-allocated ADT pointer (i64 cast). Use ptr in the function signature.
    auto ret_llvm = (ret_ctype == CType::ADT)
        ? static_cast<LType*>(PointerType::get(*context_, 0))
        : llvm_type(ret_ctype);

    // The C ABI symbol may differ from the local Yona name (e.g.
    // `extern channel_new : Int -> Channel = "yona_Std_Channel__channel"`).
    // The LLVM function takes the ABI symbol; we register it locally under
    // the Yona name so call sites resolve naturally.
    const std::string& c_sym = node->c_symbol.empty() ? node->name : node->c_symbol;

    auto fn_type = llvm::FunctionType::get(ret_llvm, param_types, false);
    auto* fn = module_->getFunction(c_sym);
    if (!fn) {
        fn = Function::Create(fn_type, Function::ExternalLinkage,
                               c_sym, module_.get());
    }

    // Register as a compiled function
    CompiledFunction cf;
    cf.fn = fn;
    cf.return_type = node->is_async ? CType::PROMISE : ret_ctype;
    cf.param_types = param_ctypes;
    cf.return_adt_name = ret_adt_name;
    compiled_functions_[node->name] = cf;
    named_values_[node->name] = {fn, CType::FUNCTION,
                                  node->is_async ? std::vector<CType>{ret_ctype} : std::vector<CType>{}};

    // Compile the body (nullptr for module-level externs)
    if (node->body) return codegen(node->body);
    return {};
}

// ===== Local static helpers for type annotations =====

static CType yona_type_to_ctype(const types::Type& t) {
    if (std::holds_alternative<types::BuiltinType>(t)) {
        switch (std::get<types::BuiltinType>(t)) {
            case types::SignedInt64: case types::SignedInt32:
            case types::SignedInt16: case types::SignedInt128:
            case types::UnsignedInt64: case types::UnsignedInt32:
            case types::UnsignedInt16: case types::UnsignedInt128:
                return CType::INT;
            case types::Float64: case types::Float32: case types::Float128:
                return CType::FLOAT;
            case types::Bool: return CType::BOOL;
            case types::String: return CType::STRING;
            case types::Symbol: return CType::SYMBOL;
            case types::Unit: return CType::UNIT;
            case types::Seq: return CType::SEQ;
            case types::Set: return CType::SET;
            case types::Dict: return CType::DICT;
            default: return CType::INT;
        }
    }
    if (std::holds_alternative<std::shared_ptr<types::FunctionType>>(t))
        return CType::FUNCTION;
    if (std::holds_alternative<std::shared_ptr<types::SingleItemCollectionType>>(t)) {
        auto& col = std::get<std::shared_ptr<types::SingleItemCollectionType>>(t);
        return (col->kind == types::SingleItemCollectionType::Seq) ? CType::SEQ : CType::SET;
    }
    if (std::holds_alternative<std::shared_ptr<types::DictCollectionType>>(t))
        return CType::DICT;
    if (std::holds_alternative<std::shared_ptr<types::ProductType>>(t))
        return CType::TUPLE;
    if (std::holds_alternative<std::shared_ptr<types::NamedType>>(t)) {
        auto& nt = std::get<std::shared_ptr<types::NamedType>>(t);
        if (nt->name == "Channel") return CType::CHANNEL;
        return CType::ADT;
    }
    if (std::holds_alternative<std::shared_ptr<types::PromiseType>>(t))
        return CType::PROMISE;
    if (std::holds_alternative<std::shared_ptr<types::SumType>>(t))
        return CType::SUM;
    if (std::holds_alternative<std::shared_ptr<types::RefinedType>>(t))
        return yona_type_to_ctype(std::get<std::shared_ptr<types::RefinedType>>(t)->base_type);
    return CType::INT;
}

// Extract the ADT type name from a Yona type, if present.
// Returns the name (e.g., "Option", "Result") for NamedType, empty otherwise.
// Channel maps to CType::CHANNEL — not an ADT — so we exclude it.
static std::string yona_type_adt_name(const types::Type& t) {
    if (std::holds_alternative<std::shared_ptr<types::NamedType>>(t)) {
        auto& nt = std::get<std::shared_ptr<types::NamedType>>(t);
        if (nt->name == "Channel") return "";
        return nt->name;
    }
    if (std::holds_alternative<std::shared_ptr<types::RefinedType>>(t))
        return yona_type_adt_name(std::get<std::shared_ptr<types::RefinedType>>(t)->base_type);
    return "";
}

static std::pair<std::vector<CType>, CType> uncurry_type_signature(const types::Type& t) {
    std::vector<CType> params;
    const types::Type* current = &t;
    while (std::holds_alternative<std::shared_ptr<types::FunctionType>>(*current)) {
        auto& ft = std::get<std::shared_ptr<types::FunctionType>>(*current);
        params.push_back(yona_type_to_ctype(ft->argumentType));
        current = &ft->returnType;
    }
    return {params, yona_type_to_ctype(*current)};
}

} // namespace yona::compiler::codegen
