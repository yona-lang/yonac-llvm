//
// LLVM Code Generation for Yona — Type-directed codegen (core)
//
// Every expression produces a TypedValue (LLVM Value + CType tag).
// Types propagate structurally: codegen_integer returns {i64, INT},
// codegen_add checks operand CTypes to choose iadd vs fadd, etc.
// Functions are deferred until call sites where arg types are known.
//

#include "Codegen.h"

#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/BinaryFormat/Dwarf.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <map>

namespace yona::compiler::codegen {

using namespace llvm;
using LType = llvm::Type; // avoid collision with yona::compiler::types::Type

// ===== Constructor / Init =====

Codegen::Codegen(const std::string& module_name, compiler::DiagnosticEngine* diag) {
    if (diag) {
        diag_ = diag;
    } else {
        owned_diag_ = std::make_unique<compiler::DiagnosticEngine>();
        diag_ = owned_diag_.get();
    }
    context_ = std::make_unique<LLVMContext>();
    module_ = std::make_unique<Module>(module_name, *context_);
    builder_ = std::make_unique<IRBuilder<>>(*context_);
    init_target();
    declare_runtime();

    // Built-in Closeable trait (prelude) — enables `with` expression
    TraitInfo closeable;
    closeable.name = "Closeable";
    closeable.type_param = "a";
    closeable.method_names.push_back("close");
    trait_registry_["Closeable"] = closeable;

    // Closeable Int — for file descriptors and socket handles
    TraitInstanceInfo closeable_int;
    closeable_int.trait_name = "Closeable";
    closeable_int.type_name = "Int";
    closeable_int.method_mangled_names["close"] = "Closeable_Int__close";
    trait_instances_["Closeable:Int"] = closeable_int;
    // Register rt_close as the implementation
    compiled_functions_["Closeable_Int__close"] = {rt_close_, CType::UNIT, {CType::INT}};
}
Codegen::~Codegen() = default;

// ===== DWARF Debug Info =====

void Codegen::set_debug_info(bool enabled, const std::string& filename) {
    debug_info_ = enabled;
    if (enabled) init_debug_info(filename);
}

void Codegen::init_debug_info(const std::string& filename) {
    di_builder_ = std::make_unique<DIBuilder>(*module_);
    // Use DW_LANG_C as closest match for Yona
    auto file_path = std::filesystem::path(filename);
    auto dir = file_path.parent_path().string();
    auto file = file_path.filename().string();
    if (dir.empty()) dir = ".";
    di_file_ = di_builder_->createFile(file, dir);
    di_cu_ = di_builder_->createCompileUnit(
        dwarf::DW_LANG_C, di_file_, "yonac", false, "", 0);
    di_scope_ = di_cu_;
    // Add debug info flag to module
    module_->addModuleFlag(Module::Warning, "Debug Info Version",
                           DEBUG_METADATA_VERSION);
    module_->addModuleFlag(Module::Warning, "Dwarf Version", 4);
}

void Codegen::finalize_debug_info() {
    if (di_builder_) di_builder_->finalize();
}

void Codegen::set_debug_loc(const SourceLocation& loc) {
    if (!debug_info_ || !di_scope_) return;
    if (loc.line == 0) return; // skip unknown locations
    builder_->SetCurrentDebugLocation(
        DILocation::get(*context_, loc.line, loc.column, di_scope_));
}

DIType* Codegen::di_type_for(CType ct) {
    if (!di_builder_) return nullptr;
    switch (ct) {
        case CType::INT:    return di_builder_->createBasicType("Int", 64, dwarf::DW_ATE_signed);
        case CType::FLOAT:  return di_builder_->createBasicType("Float", 64, dwarf::DW_ATE_float);
        case CType::BOOL:   return di_builder_->createBasicType("Bool", 8, dwarf::DW_ATE_boolean);
        case CType::STRING: return di_builder_->createPointerType(
            di_builder_->createBasicType("Char", 8, dwarf::DW_ATE_signed_char), 64);
        case CType::SYMBOL: return di_builder_->createBasicType("Symbol", 64, dwarf::DW_ATE_signed);
        case CType::UNIT:   return di_builder_->createBasicType("Unit", 64, dwarf::DW_ATE_signed);
        case CType::FUNCTION: return di_builder_->createPointerType(nullptr, 64);
        case CType::SEQ:
        case CType::SET:
        case CType::DICT:
        case CType::ADT:
        case CType::PROMISE:
            return di_builder_->createPointerType(
                di_builder_->createBasicType("Opaque", 8, dwarf::DW_ATE_unsigned), 64);
        case CType::TUPLE:
            return di_builder_->createBasicType("Tuple", 64, dwarf::DW_ATE_signed);
    }
    return di_builder_->createBasicType("Unknown", 64, dwarf::DW_ATE_signed);
}

DISubroutineType* Codegen::di_func_type(const std::vector<CType>& param_types, CType ret_type) {
    SmallVector<Metadata*, 8> types;
    types.push_back(di_type_for(ret_type)); // return type first
    for (auto ct : param_types)
        types.push_back(di_type_for(ct));
    return di_builder_->createSubroutineType(di_builder_->getOrCreateTypeArray(types));
}

void Codegen::init_target() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    auto triple = sys::getDefaultTargetTriple();
    module_->setTargetTriple(Triple(triple));
    std::string err;
    auto target = TargetRegistry::lookupTarget(triple, err);
    if (!target) { std::cerr << "Target error: " << err << "\n"; return; }
    TargetOptions opt;
    target_machine_ = target->createTargetMachine(triple, "generic", "", opt, Reloc::PIC_);
    module_->setDataLayout(target_machine_->createDataLayout());
}

LType* Codegen::llvm_type(CType ct) {
    switch (ct) {
        case CType::INT:    return LType::getInt64Ty(*context_);
        case CType::FLOAT:  return LType::getDoubleTy(*context_);
        case CType::BOOL:   return LType::getInt1Ty(*context_);
        case CType::STRING: return PointerType::get(LType::getInt8Ty(*context_), 0);
        case CType::SEQ:    return PointerType::get(LType::getInt64Ty(*context_), 0);
        case CType::TUPLE:  return LType::getInt64Ty(*context_); // overridden per-tuple
        case CType::UNIT:   return LType::getInt64Ty(*context_);
        case CType::FUNCTION: return PointerType::get(LType::getInt8Ty(*context_), 0);
        case CType::SYMBOL: return LType::getInt64Ty(*context_); // interned symbol ID
        case CType::SET:    return PointerType::get(LType::getInt64Ty(*context_), 0);
        case CType::DICT:   return PointerType::get(LType::getInt64Ty(*context_), 0);
        case CType::PROMISE: return PointerType::get(LType::getInt8Ty(*context_), 0);
        case CType::ADT:    return LType::getInt64Ty(*context_); // overridden per-ADT
        case CType::BYTES:  return PointerType::get(LType::getInt8Ty(*context_), 0);
    }
    return LType::getInt64Ty(*context_);
}

void Codegen::declare_runtime() {
    auto i64 = LType::getInt64Ty(*context_);
    auto f64 = LType::getDoubleTy(*context_);
    auto vd = LType::getVoidTy(*context_);
    auto i1 = LType::getInt1Ty(*context_);
    auto ptr = PointerType::get(LType::getInt8Ty(*context_), 0);
    auto i64p = PointerType::get(i64, 0);

    auto decl = [&](const char* name, LType* ret, std::vector<LType*> args) {
        return Function::Create(llvm::FunctionType::get(ret, args, false),
                                Function::ExternalLinkage, name, module_.get());
    };

    rt_print_int_     = decl("yona_rt_print_int", vd, {i64});
    rt_print_float_   = decl("yona_rt_print_float", vd, {f64});
    rt_print_string_  = decl("yona_rt_print_string", vd, {ptr});
    rt_print_bool_    = decl("yona_rt_print_bool", vd, {i1});
    rt_print_newline_ = decl("yona_rt_print_newline", vd, {});
    rt_print_seq_     = decl("yona_rt_print_seq", vd, {i64p});
    rt_string_concat_ = decl("yona_rt_string_concat", ptr, {ptr, ptr});
    rt_seq_alloc_     = decl("yona_rt_seq_alloc", i64p, {i64});
    rt_seq_set_       = decl("yona_rt_seq_set", vd, {i64p, i64, i64});
    rt_seq_get_       = decl("yona_rt_seq_get", i64, {i64p, i64});
    rt_seq_length_    = decl("yona_rt_seq_length", i64, {i64p});
    rt_seq_cons_      = decl("yona_rt_seq_cons", i64p, {i64, i64p});
    rt_seq_join_      = decl("yona_rt_seq_join", i64p, {i64p, i64p});
    rt_seq_head_      = decl("yona_rt_seq_head", i64, {i64p});
    rt_seq_tail_      = decl("yona_rt_seq_tail", i64p, {i64p});
    rt_seq_is_empty_  = decl("yona_rt_seq_is_empty", i64, {i64p});
    rt_seq_lazy_cons_ = decl("yona_rt_seq_lazy_cons", ptr, {i64, ptr, ptr});
    rt_print_symbol_  = decl("yona_rt_print_symbol", vd, {ptr}); // takes char* name

    // Set runtime
    rt_set_alloc_     = decl("yona_rt_set_alloc", i64p, {i64});
    rt_set_put_       = decl("yona_rt_set_put", vd, {i64p, i64, i64});
    rt_print_set_     = decl("yona_rt_print_set", vd, {i64p});

    // Dict runtime
    rt_dict_alloc_    = decl("yona_rt_dict_alloc", i64p, {i64});
    rt_dict_set_      = decl("yona_rt_dict_set", vd, {i64p, i64, i64, i64});
    rt_print_dict_    = decl("yona_rt_print_dict", vd, {i64p});

    // Async runtime: promise = async_call(fn_ptr, arg), result = async_await(promise)
    // fn_ptr type: i64 (*)(i64) — function pointer taking and returning i64
    auto fn_ptr_ty = PointerType::get(llvm::FunctionType::get(i64, {i64}, false), 0);
    auto promise_ptr = ptr; // opaque pointer to yona_promise_t
    rt_async_call_    = decl("yona_rt_async_call", promise_ptr, {fn_ptr_ty, i64});
    auto thunk_ptr_ty = PointerType::get(llvm::FunctionType::get(i64, {}, false), 0);
    rt_async_call_thunk_ = decl("yona_rt_async_call_thunk", promise_ptr, {thunk_ptr_ty});
    rt_async_await_   = decl("yona_rt_async_await", i64, {promise_ptr});

    // ADT runtime (recursive types)
    auto i8 = LType::getInt8Ty(*context_);
    rt_adt_alloc_     = decl("yona_rt_adt_alloc", ptr, {i64, i64});
    rt_adt_get_tag_   = decl("yona_rt_adt_get_tag", i64, {ptr});
    rt_adt_get_field_ = decl("yona_rt_adt_get_field", i64, {ptr, i64});
    rt_adt_set_field_ = decl("yona_rt_adt_set_field", vd, {ptr, i64, i64});

    // General closures: {fn_ptr, ret_tag, arity, cap0, ...} with env-passing
    rt_closure_create_  = decl("yona_rt_closure_create", ptr, {ptr, i64, i64, i64});
    rt_closure_set_cap_ = decl("yona_rt_closure_set_cap", vd, {ptr, i64, i64});
    rt_closure_get_cap_ = decl("yona_rt_closure_get_cap", i64, {ptr, i64});

    // Reference counting
    rt_rc_inc_ = decl("yona_rt_rc_inc", vd, {ptr});
    rt_rc_dec_ = decl("yona_rt_rc_dec", vd, {ptr});

    // Arena allocator
    rt_arena_create_  = decl("yona_rt_arena_create", ptr, {i64});
    rt_arena_alloc_   = decl("yona_rt_arena_alloc", ptr, {ptr, i64, i64});
    rt_arena_destroy_ = decl("yona_rt_arena_destroy", vd, {ptr});

    // io_uring await
    rt_io_await_ = decl("yona_rt_io_await", i64, {i64});

    // Resource cleanup (with expression)
    // Bytes
    rt_bytes_alloc_       = decl("yona_rt_bytes_alloc", ptr, {i64});
    rt_bytes_length_      = decl("yona_rt_bytes_length", i64, {ptr});
    rt_bytes_get_         = decl("yona_rt_bytes_get", i64, {ptr, i64});
    rt_bytes_set_         = decl("yona_rt_bytes_set", vd, {ptr, i64, i64});
    rt_bytes_concat_      = decl("yona_rt_bytes_concat", ptr, {ptr, ptr});
    rt_bytes_slice_       = decl("yona_rt_bytes_slice", ptr, {ptr, i64, i64});
    rt_bytes_from_string_ = decl("yona_rt_bytes_from_string", ptr, {ptr});
    rt_bytes_to_string_   = decl("yona_rt_bytes_to_string", ptr, {ptr});
    rt_bytes_from_seq_    = decl("yona_rt_bytes_from_seq", ptr, {ptr});
    rt_bytes_to_seq_      = decl("yona_rt_bytes_to_seq", ptr, {ptr});
    rt_print_bytes_       = decl("yona_rt_print_bytes", vd, {ptr});

    rt_box_ = decl("yona_rt_box", ptr, {ptr, i64});
    rt_close_ = decl("yona_rt_close", vd, {i64});

    // Exception handling (setjmp/longjmp)
    auto i32 = LType::getInt32Ty(*context_);
    rt_try_begin_     = decl("yona_rt_try_push", ptr, {});  // returns jmp_buf*
    rt_try_end_       = decl("yona_rt_try_end", vd, {});
    rt_raise_         = decl("yona_rt_raise", vd, {i64, ptr});
    rt_get_exc_sym_   = decl("yona_rt_get_exception_symbol", i64, {});
    rt_get_exc_msg_   = decl("yona_rt_get_exception_message", ptr, {});
    // Declare C setjmp — must be called from the compiled function's stack frame
    auto setjmp_fn = decl("setjmp", i32, {ptr});
    setjmp_fn->addFnAttr(llvm::Attribute::ReturnsTwice);
    rt_raise_->addFnAttr(llvm::Attribute::NoReturn);
}

void Codegen::report_error(const SourceLocation& loc, const std::string& message) {
    error_count_++;
    diag_->error(loc, message);
}

std::string Codegen::suggest_similar(const std::string& name) const {
    // Levenshtein distance for "did you mean?" suggestions
    auto edit_distance = [](const std::string& a, const std::string& b) -> int {
        int m = a.size(), n = b.size();
        std::vector<int> dp(n + 1);
        for (int j = 0; j <= n; j++) dp[j] = j;
        for (int i = 1; i <= m; i++) {
            int prev = dp[0];
            dp[0] = i;
            for (int j = 1; j <= n; j++) {
                int tmp = dp[j];
                dp[j] = std::min({dp[j] + 1, dp[j-1] + 1, prev + (a[i-1] != b[j-1] ? 1 : 0)});
                prev = tmp;
            }
        }
        return dp[n];
    };

    std::string best;
    int best_dist = 4; // max distance threshold
    for (auto& [k, _] : named_values_) {
        int d = edit_distance(name, k);
        if (d < best_dist) { best = k; best_dist = d; }
    }
    for (auto& [k, _] : deferred_functions_) {
        int d = edit_distance(name, k);
        if (d < best_dist) { best = k; best_dist = d; }
    }
    for (auto& [k, _] : extern_functions_) {
        int d = edit_distance(name, k);
        if (d < best_dist) { best = k; best_dist = d; }
    }
    for (auto& [k, _] : adt_constructors_) {
        int d = edit_distance(name, k);
        if (d < best_dist) { best = k; best_dist = d; }
    }
    return best;
}

// ===== Public API =====

std::string Codegen::mangle_name(const std::string& module_fqn, const std::string& func_name) {
    // Replace backslash with underscore: Test\Test → Test_Test
    std::string mangled = "yona_";
    for (char c : module_fqn) {
        mangled += (c == '\\' || c == '/') ? '_' : c;
    }
    mangled += "__";
    mangled += func_name;
    return mangled;
}

Module* Codegen::compile(AstNode* node) {
    auto fn = codegen_main(node);
    if (!fn) return nullptr;
    finalize_debug_info();
    std::string err;
    raw_string_ostream os(err);
    if (verifyModule(*module_, &os)) {
        std::cerr << "Module verification failed:\n" << err << "\n";
        return nullptr;
    }
    optimize();
    return module_.get();
}

// Forward declarations for type annotation support
static CType yona_type_to_ctype(const types::Type& t);
static std::pair<std::vector<CType>, CType> uncurry_type_signature(const types::Type& t);

Module* Codegen::compile_module(ModuleDecl* mod) {
    // Build the module FQN string
    std::string fqn;
    if (mod->fqn->packageName.has_value()) {
        auto* pkg = mod->fqn->packageName.value();
        for (size_t i = 0; i < pkg->parts.size(); i++) {
            if (i > 0) fqn += "\\";
            fqn += pkg->parts[i]->value;
        }
        fqn += "\\";
    }
    fqn += mod->fqn->moduleName->value;

    // Build export set for visibility control
    std::unordered_set<std::string> export_set(mod->exports.begin(), mod->exports.end());

    // Build exported types set — constructors will be added after ADT processing
    std::unordered_set<std::string> exported_type_set(mod->exported_types.begin(), mod->exported_types.end());

    // Process ADT declarations: register constructors, detect recursion
    for (auto* adt : mod->adt_declarations) {
        int max_arity = 0;
        bool is_recursive = false;
        for (auto* ctor : adt->variants) {
            int a = static_cast<int>(ctor->field_type_names.size());
            if (a > max_arity) max_arity = a;
            // Check if any field type references the ADT itself or contains
            // a function type — function fields make ADTs heap-allocated because
            // closures may return values of the same type (lazy data structures)
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
            // Map field types to CTypes
            std::vector<CType> ftypes;
            for (auto& ft : ctor->field_type_names) {
                if (ft.is_function_type) ftypes.push_back(CType::FUNCTION);
                else if (ft.name == "Int" || ft.name == "a" || ft.name == "b" || ft.name == "e" || ft.name == "s") ftypes.push_back(CType::INT);
                else if (ft.name == "Float") ftypes.push_back(CType::FLOAT);
                else if (ft.name == "String") ftypes.push_back(CType::STRING);
                else if (ft.name == "Bool") ftypes.push_back(CType::BOOL);
                else if (ft.name == "Symbol") ftypes.push_back(CType::SYMBOL);
                else if (ft.name == adt->name) ftypes.push_back(CType::ADT);
                else if (ft.name == "()" || ft.name == "Fn" || ft.name == "fn" || ft.name == "Function") ftypes.push_back(CType::FUNCTION);
                else ftypes.push_back(CType::INT);
            }

            adt_constructors_[ctor->name] = {adt->name, static_cast<int>(ci), arity,
                                              static_cast<int>(adt->variants.size()), max_arity, is_recursive,
                                              ctor->field_names, ftypes};
        }
    }

    // Expand exported types: add all constructors of exported types to export_set
    for (auto* adt : mod->adt_declarations) {
        if (exported_type_set.count(adt->name) > 0) {
            for (auto* ctor : adt->variants) {
                export_set.insert(ctor->name);
            }
        }
    }

    // Load re-export source modules so their functions are available
    // for use within this module's own functions
    for (auto& re : mod->re_exports) {
        std::string path_str;
        for (char c : re.source_module) path_str += (c == '\\') ? '/' : c;
        load_module_interface(std::filesystem::path(path_str));
        for (auto& name : re.names)
            register_import(re.source_module, name, name);
    }

    // Register trait declarations (Phase 3: with superclasses and default impls)
    for (auto* trait : mod->trait_declarations) {
        TraitInfo ti;
        ti.name = trait->name;
        ti.type_param = trait->type_param;
        ti.superclasses = std::vector<std::pair<std::string, std::string>>(
            trait->superclasses.begin(), trait->superclasses.end());
        for (auto& m : trait->methods) {
            ti.method_names.push_back(m.name);
            if (m.default_impl) {
                ti.default_impls[m.name] = m.default_impl;
            }
        }
        trait_registry_[trait->name] = ti;
    }

    // Register trait instances: mangle method names and register as deferred functions
    // Phase 2: handle constrained instances with ADT type names
    // Phase 3: fill in default methods from trait declaration
    for (auto* inst : mod->instance_declarations) {
        std::string key = inst->trait_name + ":" + inst->type_name;
        TraitInstanceInfo tii;
        tii.trait_name = inst->trait_name;
        tii.type_name = inst->type_name;
        tii.constraints = std::vector<std::pair<std::string, std::string>>(
            inst->constraints.begin(), inst->constraints.end());

        // Phase 3: Verify superclass instances exist
        auto trait_it = trait_registry_.find(inst->trait_name);
        if (trait_it != trait_registry_.end()) {
            for (auto& [sc_trait, sc_var] : trait_it->second.superclasses) {
                std::string sc_key = sc_trait + ":" + inst->type_name;
                if (trait_instances_.find(sc_key) == trait_instances_.end()) {
                    std::cerr << "Warning: instance " << inst->trait_name << " " << inst->type_name
                              << " requires " << sc_trait << " " << inst->type_name
                              << " (superclass), but no such instance found yet\n";
                }
            }
        }

        // Collect provided method names
        std::unordered_set<std::string> provided_methods;
        for (auto* method : inst->methods) {
            // Mangle: TraitName_TypeName__methodName
            std::string mangled = inst->trait_name + "_" + inst->type_name + "__" + method->name;
            tii.method_mangled_names[method->name] = mangled;
            provided_methods.insert(method->name);

            // Register as deferred function
            codegen_function_def(method, mangled);
        }

        // Phase 3: Fill in default implementations for missing methods
        if (trait_it != trait_registry_.end()) {
            for (auto& [method_name, default_fn] : trait_it->second.default_impls) {
                if (provided_methods.find(method_name) == provided_methods.end()) {
                    // Method not provided by instance — use default from trait
                    std::string mangled = inst->trait_name + "_" + inst->type_name + "__" + method_name;
                    tii.method_mangled_names[method_name] = mangled;
                    codegen_function_def(default_fn, mangled);
                }
            }
        }

        trait_instances_[key] = tii;
    }

    // Process module-level extern declarations
    for (auto* ext : mod->extern_declarations) {
        codegen_extern_decl(ext);
        // Add to export set if exported
        if (export_set.count(ext->name)) {
            std::string mangled = mangle_name(fqn, ext->name);
            // The extern is already declared; create a wrapper with the mangled name
            auto cf_it = compiled_functions_.find(ext->name);
            if (cf_it != compiled_functions_.end()) {
                auto& cf = cf_it->second;
                module_meta_[mangled] = {cf.param_types, cf.return_type};
                // Create a forwarding wrapper
                if (cf.fn->getName() != mangled) {
                    auto* wrapper = Function::Create(
                        cf.fn->getFunctionType(),
                        Function::ExternalLinkage,
                        mangled, module_.get());
                    auto* bb = BasicBlock::Create(*context_, "entry", wrapper);
                    builder_->SetInsertPoint(bb);
                    std::vector<Value*> args;
                    for (auto& arg : wrapper->args()) args.push_back(&arg);
                    auto* result = builder_->CreateCall(cf.fn, args);
                    builder_->CreateRet(result);
                }
            }
        }
    }

    // First pass: register all functions as deferred
    for (auto* func : mod->functions) {
        std::string fn_name = func->name;
        codegen_function_def(func, fn_name);

        // Store source text for exported generic functions (.yonai emission)
        if (export_set.count(fn_name) && !func->source_text.empty()) {
            std::string mangled = mangle_name(fqn, fn_name);
            module_function_source_[mangled] = func->source_text;
        }
    }

    // Second pass: compile only EXPORTED functions with inferred types.
    // Non-exported functions stay deferred and compile on demand at call sites
    // with correct argument types (monomorphization).
    for (auto* func : mod->functions) {
        std::string fn_name = func->name;
        bool is_exported = export_set.count(fn_name) > 0;
        if (!is_exported) continue; // internal functions compile at call sites

        auto def_it = deferred_functions_.find(fn_name);
        if (def_it == deferred_functions_.end()) continue;

        // Use explicit type annotation if present, otherwise infer from patterns/body
        std::vector<CType> annotated_param_types;
        if (func->type_signature.has_value()) {
            auto [params, ret] = uncurry_type_signature(*func->type_signature);
            annotated_param_types = params;
        }

        auto inferred = func->type_signature.has_value()
            ? std::vector<InferredParamType>() // not needed when annotated
            : infer_param_types(func);

        std::vector<TypedValue> typed_args;
        for (size_t i = 0; i < func->patterns.size(); i++) {
            CType ct;
            if (!annotated_param_types.empty() && i < annotated_param_types.size())
                ct = annotated_param_types[i];
            else
                ct = (i < inferred.size()) ? inferred[i].type : CType::INT;
            auto* dummy_val = ConstantInt::get(LType::getInt64Ty(*context_), 0);

            if (ct == CType::TUPLE) {
                // Find the tuple pattern (from direct pattern or body case pattern)
                PatternNode* src = (i < inferred.size()) ? inferred[i].source_pattern : nullptr;
                auto* tp = src ? dynamic_cast<TuplePattern*>(src) : nullptr;
                if (tp) {
                    std::vector<LType*> elem_types;
                    std::vector<CType> elem_ctypes;
                    for (size_t j = 0; j < tp->patterns.size(); j++) {
                        CType et = infer_type_from_pattern(static_cast<PatternNode*>(tp->patterns[j]));
                        elem_types.push_back(llvm_type(et));
                        elem_ctypes.push_back(et);
                    }
                    auto* st = StructType::get(*context_, elem_types);
                    typed_args.push_back({UndefValue::get(st), CType::TUPLE, elem_ctypes});
                } else {
                    typed_args.push_back({dummy_val, ct});
                }
            } else if (ct == CType::SEQ) {
                auto* ptr_type = PointerType::get(*context_, 0);
                typed_args.push_back({ConstantPointerNull::get(ptr_type), CType::SEQ});
            } else if (ct == CType::STRING) {
                auto* ptr_type = PointerType::get(*context_, 0);
                typed_args.push_back({ConstantPointerNull::get(ptr_type), CType::STRING});
            } else if (ct == CType::FLOAT) {
                typed_args.push_back({ConstantFP::get(LType::getDoubleTy(*context_), 0.0), CType::FLOAT});
            } else if (ct == CType::BOOL) {
                typed_args.push_back({ConstantInt::get(LType::getInt1Ty(*context_), 0), CType::BOOL});
            } else if (ct == CType::SYMBOL) {
                typed_args.push_back({ConstantInt::get(LType::getInt64Ty(*context_), 0), CType::SYMBOL});
            } else if (ct == CType::FUNCTION) {
                auto* ptr_type = PointerType::get(*context_, 0);
                typed_args.push_back({ConstantPointerNull::get(ptr_type), CType::FUNCTION});
            } else if (ct == CType::ADT) {
                // Build the ADT struct type based on the constructor pattern
                // Find which ADT type by looking at the case patterns
                PatternNode* src = (i < inferred.size()) ? inferred[i].source_pattern : nullptr;
                // Find constructor name from either ConstructorPattern or RecordPattern
                std::string ctor_name_lookup;
                if (auto* cp = src ? dynamic_cast<ConstructorPattern*>(src) : nullptr)
                    ctor_name_lookup = cp->constructor_name;
                else if (auto* rp = src ? dynamic_cast<RecordPattern*>(src) : nullptr)
                    ctor_name_lookup = rp->recordType;
                if (!ctor_name_lookup.empty()) {
                    auto ctor_it = adt_constructors_.find(ctor_name_lookup);
                    if (ctor_it != adt_constructors_.end()) {
                        TypedValue tv;
                        tv.type = CType::ADT;
                        tv.adt_type_name = ctor_it->second.type_name;
                        if (ctor_it->second.is_recursive) {
                            auto* ptr_type = PointerType::get(*context_, 0);
                            tv.val = ConstantPointerNull::get(ptr_type);
                        } else {
                            auto tag_ty = LType::getInt64Ty(*context_);
                            auto i64_ty = LType::getInt64Ty(*context_);
                            std::vector<LType*> fields = {tag_ty};
                            for (int f = 0; f < ctor_it->second.max_arity; f++)
                                fields.push_back(i64_ty);
                            auto* st = StructType::get(*context_, fields);
                            tv.val = UndefValue::get(st);
                        }
                        typed_args.push_back(tv);
                    } else {
                        typed_args.push_back({dummy_val, ct});
                    }
                } else {
                    // No constructor pattern — inferred from field access or other usage.
                    // Search adt_constructors_ for a constructor with matching field names.
                    bool found = false;
                    for (auto& [cname, cinfo] : adt_constructors_) {
                        if (!cinfo.field_names.empty()) {
                            TypedValue tv;
                            tv.type = CType::ADT;
                            tv.adt_type_name = cinfo.type_name;
                            if (cinfo.is_recursive) {
                                auto* ptr_type = PointerType::get(*context_, 0);
                                tv.val = ConstantPointerNull::get(ptr_type);
                            } else {
                                auto tag_ty = LType::getInt64Ty(*context_);
                                auto i64_ty = LType::getInt64Ty(*context_);
                                std::vector<LType*> fields = {tag_ty};
                                for (int f = 0; f < cinfo.max_arity; f++) fields.push_back(i64_ty);
                                auto* st = StructType::get(*context_, fields);
                                tv.val = UndefValue::get(st);
                            }
                            typed_args.push_back(tv);
                            found = true;
                            break;
                        }
                    }
                    if (!found) typed_args.push_back({dummy_val, ct});
                }
            } else {
                typed_args.push_back({dummy_val, ct});
            }
        }

        auto cf = compile_function(fn_name, def_it->second, typed_args);

        if (is_exported) {
            // Store type metadata for importers
            std::string mangled = mangle_name(fqn, fn_name);
            module_meta_[mangled] = {cf.param_types, cf.return_type};

            // Check if the function already has the right linkage
            if (cf.fn->getName() != mangled) {
                // Create a wrapper function with external linkage and mangled name
                auto* wrapper = Function::Create(
                    cf.fn->getFunctionType(),
                    Function::ExternalLinkage,
                    mangled, module_.get());

                auto* bb = BasicBlock::Create(*context_, "entry", wrapper);
                builder_->SetInsertPoint(bb);

                std::vector<Value*> args;
                for (auto& arg : wrapper->args()) args.push_back(&arg);
                auto* result = builder_->CreateCall(cf.fn, args);
                builder_->CreateRet(result);
            } else {
                cf.fn->setLinkage(Function::ExternalLinkage);
            }
        }
    }

    // Third pass: compile trait instance methods with ExternalLinkage
    // so importing modules can call them via resolved trait dispatch.
    for (auto& [key, inst] : trait_instances_) {
        for (auto& [method_name, mangled] : inst.method_mangled_names) {
            auto cf_it = compiled_functions_.find(mangled);
            if (cf_it != compiled_functions_.end()) {
                cf_it->second.fn->setLinkage(Function::ExternalLinkage);
                // Emit FN metadata for the .yonai file
                if (module_meta_.find(mangled) == module_meta_.end()) {
                    module_meta_[mangled] = {cf_it->second.param_types, cf_it->second.return_type};
                }
                continue;
            }
            auto def_it = deferred_functions_.find(mangled);
            if (def_it != deferred_functions_.end()) {
                // Compile with inferred types, using correct LLVM types for ADTs
                auto inferred = infer_param_types(def_it->second.ast);
                std::vector<TypedValue> typed_args;
                for (size_t i = 0; i < def_it->second.param_names.size(); i++) {
                    CType ct = (i < inferred.size()) ? inferred[i].type : CType::INT;
                    TypedValue tv;
                    tv.type = ct;
                    if (ct == CType::ADT) {
                        // Find the ADT type from the instance or pattern
                        PatternNode* src = (i < inferred.size()) ? inferred[i].source_pattern : nullptr;
                        std::string ctor_name;
                        if (auto* cp = src ? dynamic_cast<ConstructorPattern*>(src) : nullptr)
                            ctor_name = cp->constructor_name;
                        if (!ctor_name.empty()) {
                            auto ctor_it = adt_constructors_.find(ctor_name);
                            if (ctor_it != adt_constructors_.end()) {
                                tv.adt_type_name = ctor_it->second.type_name;
                                if (ctor_it->second.is_recursive) {
                                    tv.val = ConstantPointerNull::get(PointerType::get(*context_, 0));
                                } else {
                                    std::vector<LType*> fields = {LType::getInt64Ty(*context_)};
                                    for (int f = 0; f < ctor_it->second.max_arity; f++)
                                        fields.push_back(LType::getInt64Ty(*context_));
                                    auto* st = StructType::get(*context_, fields);
                                    tv.val = UndefValue::get(st);
                                }
                            } else {
                                tv.val = ConstantInt::get(LType::getInt64Ty(*context_), 0);
                            }
                        } else {
                            // Try the instance type name
                            tv.adt_type_name = inst.type_name;
                            // Find any constructor for this type
                            for (auto& [cn, ci] : adt_constructors_) {
                                if (ci.type_name == inst.type_name) {
                                    if (ci.is_recursive) {
                                        tv.val = ConstantPointerNull::get(PointerType::get(*context_, 0));
                                    } else {
                                        std::vector<LType*> fields = {LType::getInt64Ty(*context_)};
                                        for (int f = 0; f < ci.max_arity; f++)
                                            fields.push_back(LType::getInt64Ty(*context_));
                                        auto* st = StructType::get(*context_, fields);
                                        tv.val = UndefValue::get(st);
                                    }
                                    break;
                                }
                            }
                            if (!tv.val) tv.val = ConstantInt::get(LType::getInt64Ty(*context_), 0);
                        }
                    } else {
                        tv.val = ConstantInt::get(LType::getInt64Ty(*context_), 0);
                    }
                    typed_args.push_back(tv);
                }
                auto cf = compile_function(mangled, def_it->second, typed_args);
                if (cf.fn) {
                    cf.fn->setLinkage(Function::ExternalLinkage);
                    module_meta_[mangled] = {cf.param_types, cf.return_type};
                }
            }
        }
    }

    // Process re-exports: load source module interfaces and create forwarding wrappers
    for (auto& re : mod->re_exports) {
        // Build filesystem path from FQN
        std::filesystem::path mod_path;
        std::string src_fqn = re.source_module;
        for (char& c : mod_path.string()) { /* unused, build below */ }
        // Convert backslash FQN to filesystem path
        std::string path_str;
        for (char c : src_fqn) path_str += (c == '\\') ? '/' : c;
        mod_path = std::filesystem::path(path_str);

        // Load the source module's interface
        load_module_interface(mod_path);

        for (auto& name : re.names) {
            // Check if it's an ADT constructor
            auto ctor_it = adt_constructors_.find(name);
            if (ctor_it != adt_constructors_.end()) {
                // ADT constructors are re-exported via the interface file (no wrapper needed)
                continue;
            }

            // Function re-export: create a forwarding wrapper
            std::string src_mangled = mangle_name(src_fqn, name);
            std::string dst_mangled = mangle_name(fqn, name);

            // Look up source function metadata
            auto meta_it = module_meta_.find(src_mangled);
            if (meta_it == module_meta_.end()) {
                report_error(mod->source_context,
                    "re-export: function '" + name + "' not found in module '" + src_fqn + "'");
                continue;
            }

            auto& meta = meta_it->second;

            // Build function type from metadata
            std::vector<LType*> param_types;
            for (auto ct : meta.param_types) param_types.push_back(llvm_type(ct));
            auto* ret_llvm = llvm_type(meta.return_type);
            auto* fn_type = llvm::FunctionType::get(ret_llvm, param_types, false);

            // Declare the source function (external)
            auto* src_fn = module_->getFunction(src_mangled);
            if (!src_fn)
                src_fn = Function::Create(fn_type, Function::ExternalLinkage, src_mangled, module_.get());

            // Create forwarding wrapper with this module's mangled name
            auto* wrapper = Function::Create(fn_type, Function::ExternalLinkage, dst_mangled, module_.get());
            auto* bb = BasicBlock::Create(*context_, "entry", wrapper);
            builder_->SetInsertPoint(bb);
            std::vector<Value*> args;
            for (auto& arg : wrapper->args()) args.push_back(&arg);
            auto* result = builder_->CreateCall(src_fn, args);
            builder_->CreateRet(result);

            // Register in module_meta_ so the interface file includes it
            module_meta_[dst_mangled] = meta;
        }
    }

    // Clear builder insert point after module compilation
    builder_->ClearInsertionPoint();

    finalize_debug_info();

    // Verify
    std::string err;
    raw_string_ostream os(err);
    if (verifyModule(*module_, &os)) {
        std::cerr << "Module verification failed:\n" << err << "\n";
        return nullptr;
    }
    optimize();
    return module_.get();
}



bool Codegen::emit_object_file(const std::string& path) {
    if (!target_machine_) return false;
    // Ensure parent directory exists
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::error_code ec;
    raw_fd_ostream dest(path, ec, sys::fs::OF_None);
    if (ec) return false;
    legacy::PassManager pass;
    if (target_machine_->addPassesToEmitFile(pass, dest, nullptr, CodeGenFileType::ObjectFile))
        return false;
    pass.run(*module_);
    dest.flush();
    return true;
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

std::string Codegen::emit_ir() {
    std::string ir;
    raw_string_ostream os(ir);
    module_->print(os, nullptr);
    return ir;
}

void Codegen::optimize() {
    if (!module_) return;

    // Use the new PassManager for all optimization levels
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    llvm::PassBuilder PB;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    // Select optimization level
    llvm::OptimizationLevel level;
    switch (opt_level_) {
        case 0: level = llvm::OptimizationLevel::O0; break;
        case 1: level = llvm::OptimizationLevel::O1; break;
        case 3: level = llvm::OptimizationLevel::O3; break;
        default: level = llvm::OptimizationLevel::O2; break;
    }

    llvm::ModulePassManager MPM;
    if (opt_level_ == 0) {
        // O0: only run AlwaysInliner for marked functions
        MPM = PB.buildO0DefaultPipeline(level);
    } else {
        // O1-O3: full pipeline including:
        // - Function inlining (cost-based at O2+)
        // - SROA (scalar replacement of aggregates — decomposes structs to registers)
        // - Loop optimizations (LICM, unrolling at O2+, vectorization at O3)
        // - Dead argument elimination
        // - Tail call elimination
        // - GVN, instcombine, CFG simplification, mem2reg
        MPM = PB.buildPerModuleDefaultPipeline(level);
    }

    MPM.run(*module_, MAM);
}

// ===== Entry Point =====

Function* Codegen::codegen_main(AstNode* node) {
    auto i32 = LType::getInt32Ty(*context_);
    auto fn = Function::Create(llvm::FunctionType::get(i32, {}, false),
                                Function::ExternalLinkage, "main", module_.get());
    // Create debug info for main function
    if (debug_info_ && di_builder_ && di_file_) {
        auto* di_func_ty = di_builder_->createSubroutineType(
            di_builder_->getOrCreateTypeArray({di_builder_->createBasicType("Int", 32, dwarf::DW_ATE_signed)}));
        auto* di_sp = di_builder_->createFunction(
            di_file_, "main", "main", di_file_,
            node->source_context.line, di_func_ty, node->source_context.line,
            DINode::FlagZero, DISubprogram::SPFlagDefinition);
        fn->setSubprogram(di_sp);
        di_scope_ = di_sp;
    }
    auto bb = BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(bb);
    set_debug_loc(node->source_context);

    auto result = codegen(node);
    // Don't add print/ret if the block is already terminated (e.g., by raise)
    if (!builder_->GetInsertBlock()->getTerminator()) {
        if (result) codegen_print(result);
        builder_->CreateRet(ConstantInt::get(i32, 0));
    }
    return fn;
}

// ===== Print (type-directed) =====

void Codegen::codegen_print_value(const TypedValue& tv) {
    if (!tv.val) return;
    switch (tv.type) {
        case CType::INT:    builder_->CreateCall(rt_print_int_, {tv.val}); break;
        case CType::FLOAT:  builder_->CreateCall(rt_print_float_, {tv.val}); break;
        case CType::BOOL:   builder_->CreateCall(rt_print_bool_, {tv.val}); break;
        case CType::STRING: builder_->CreateCall(rt_print_string_, {tv.val}); break;
        case CType::SEQ:    builder_->CreateCall(rt_print_seq_, {tv.val}); break;
        case CType::TUPLE: {
            builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr("(")});
            if (tv.val->getType()->isStructTy()) {
                auto* st = cast<StructType>(tv.val->getType());
                for (unsigned i = 0; i < st->getNumElements(); i++) {
                    if (i > 0)
                        builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr(", ")});
                    auto elem = builder_->CreateExtractValue(tv.val, {i});
                    CType et = (i < tv.subtypes.size()) ? tv.subtypes[i] : CType::INT;
                    codegen_print_value({elem, et});
                }
            }
            builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr(")")});
            break;
        }
        case CType::SET:    builder_->CreateCall(rt_print_set_, {tv.val}); break;
        case CType::DICT:   builder_->CreateCall(rt_print_dict_, {tv.val}); break;
        case CType::UNIT:     builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr("()")}); break;
        case CType::FUNCTION: builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr("<function>")}); break;
        case CType::SYMBOL: {
            // Symbol is an interned i64 ID. Look up the string for printing.
            if (auto* ci = dyn_cast<ConstantInt>(tv.val)) {
                int64_t id = ci->getSExtValue();
                if (id >= 0 && id < (int64_t)symbol_strings_.size()) {
                    builder_->CreateCall(rt_print_symbol_, {symbol_strings_[id]});
                }
            } else {
                // Runtime symbol value — need a table lookup.
                // Emit a GEP into the symbol names table (emitted at finalization).
                // For now, emit a placeholder.
                builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr(":<dynamic>")});
            }
            break;
        }
        case CType::ADT: {
            builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr("<adt>")});
            break;
        }
        case CType::BYTES: {
            builder_->CreateCall(rt_print_bytes_, {tv.val});
            break;
        }
        default: break;
    }
}

void Codegen::codegen_print(const TypedValue& tv) {
    auto resolved = auto_await(tv);
    codegen_print_value(resolved);
    builder_->CreateCall(rt_print_newline_, {});
}

// ===== Core Dispatch =====

TypedValue Codegen::codegen(AstNode* node) {
    if (!node) return {};
    switch (node->get_type()) {
        case AST_MAIN:            return codegen_main_node(static_cast<MainNode*>(node));
        case AST_INTEGER_EXPR:    return codegen_integer(static_cast<IntegerExpr*>(node));
        case AST_FLOAT_EXPR:      return codegen_float(static_cast<FloatExpr*>(node));
        case AST_TRUE_LITERAL_EXPR:  return codegen_bool_true(static_cast<TrueLiteralExpr*>(node));
        case AST_FALSE_LITERAL_EXPR: return codegen_bool_false(static_cast<FalseLiteralExpr*>(node));
        case AST_STRING_EXPR:     return codegen_string(static_cast<StringExpr*>(node));
        case AST_UNIT_EXPR:       return codegen_unit(static_cast<UnitExpr*>(node));
        case AST_SYMBOL_EXPR:    return codegen_symbol(static_cast<SymbolExpr*>(node));
        case AST_ADD_EXPR:        return codegen_binary(static_cast<AddExpr*>(node)->left, static_cast<AddExpr*>(node)->right, "+");
        case AST_SUBTRACT_EXPR:   return codegen_binary(static_cast<SubtractExpr*>(node)->left, static_cast<SubtractExpr*>(node)->right, "-");
        case AST_MULTIPLY_EXPR:   return codegen_binary(static_cast<MultiplyExpr*>(node)->left, static_cast<MultiplyExpr*>(node)->right, "*");
        case AST_DIVIDE_EXPR:     return codegen_binary(static_cast<DivideExpr*>(node)->left, static_cast<DivideExpr*>(node)->right, "/");
        case AST_MODULO_EXPR:     return codegen_binary(static_cast<ModuloExpr*>(node)->left, static_cast<ModuloExpr*>(node)->right, "%");
        case AST_EQ_EXPR:         return codegen_comparison(static_cast<EqExpr*>(node)->left, static_cast<EqExpr*>(node)->right, "==");
        case AST_NEQ_EXPR:        return codegen_comparison(static_cast<NeqExpr*>(node)->left, static_cast<NeqExpr*>(node)->right, "!=");
        case AST_LT_EXPR:         return codegen_comparison(static_cast<LtExpr*>(node)->left, static_cast<LtExpr*>(node)->right, "<");
        case AST_GT_EXPR:         return codegen_comparison(static_cast<GtExpr*>(node)->left, static_cast<GtExpr*>(node)->right, ">");
        case AST_LTE_EXPR:        return codegen_comparison(static_cast<LteExpr*>(node)->left, static_cast<LteExpr*>(node)->right, "<=");
        case AST_GTE_EXPR:        return codegen_comparison(static_cast<GteExpr*>(node)->left, static_cast<GteExpr*>(node)->right, ">=");
        case AST_LOGICAL_AND_EXPR: { auto l = codegen(static_cast<LogicalAndExpr*>(node)->left); auto r = codegen(static_cast<LogicalAndExpr*>(node)->right); return {builder_->CreateAnd(l.val, r.val), CType::BOOL}; }
        case AST_LOGICAL_OR_EXPR:  { auto l = codegen(static_cast<LogicalOrExpr*>(node)->left); auto r = codegen(static_cast<LogicalOrExpr*>(node)->right); return {builder_->CreateOr(l.val, r.val), CType::BOOL}; }
        case AST_LOGICAL_NOT_OP_EXPR: { auto v = codegen(static_cast<LogicalNotOpExpr*>(node)->expr); return {builder_->CreateNot(v.val), CType::BOOL}; }
        case AST_PIPE_RIGHT_EXPR: {
            // x |> f  →  f(x)
            auto* pe = static_cast<PipeRightExpr*>(node);
            auto arg = codegen(pe->left);
            if (!arg) return {};
            // Evaluate the function side — get its name for apply lookup
            std::string fn_name;
            if (pe->right->get_type() == AST_IDENTIFIER_EXPR)
                fn_name = static_cast<IdentifierExpr*>(pe->right)->name->value;
            if (!fn_name.empty()) {
                // Use the same apply logic: find compiled/deferred function, call with arg
                std::vector<TypedValue> all_args = {arg};
                auto cf_it = compiled_functions_.find(fn_name);
                if (cf_it == compiled_functions_.end()) {
                    auto def_it = deferred_functions_.find(fn_name);
                    if (def_it != deferred_functions_.end()) {
                        compile_function(fn_name, def_it->second, all_args);
                        cf_it = compiled_functions_.find(fn_name);
                    }
                }
                if (cf_it != compiled_functions_.end()) {
                    return {builder_->CreateCall(cf_it->second.fn, {arg.val}), cf_it->second.return_type};
                }
                // Check extern functions
                auto ext_it = extern_functions_.find(fn_name);
                if (ext_it != extern_functions_.end()) {
                    auto* ext_fn = module_->getFunction(ext_it->second);
                    if (!ext_fn) {
                        auto fn_type = llvm::FunctionType::get(arg.val->getType(), {arg.val->getType()}, false);
                        ext_fn = Function::Create(fn_type, Function::ExternalLinkage, ext_it->second, module_.get());
                    }
                    return {builder_->CreateCall(ext_fn, {arg.val}), CType::INT};
                }
                // Indirect call via named_values_
                auto nv_it = named_values_.find(fn_name);
                if (nv_it != named_values_.end() && nv_it->second.type == CType::FUNCTION && nv_it->second.val) {
                    auto fn_type = llvm::FunctionType::get(arg.val->getType(), {arg.val->getType()}, false);
                    return {builder_->CreateCall(fn_type, nv_it->second.val, {arg.val}), arg.type};
                }
            }
            report_error(pe->source_context, "pipe: right side must be a function");
            return {};
        }
        case AST_PIPE_LEFT_EXPR: {
            // f <| x  →  f(x) — same logic, swapped sides
            auto* pe = static_cast<PipeLeftExpr*>(node);
            auto arg = codegen(pe->right);
            if (!arg) return {};
            std::string fn_name;
            if (pe->left->get_type() == AST_IDENTIFIER_EXPR)
                fn_name = static_cast<IdentifierExpr*>(pe->left)->name->value;
            if (!fn_name.empty()) {
                std::vector<TypedValue> all_args = {arg};
                auto cf_it = compiled_functions_.find(fn_name);
                if (cf_it == compiled_functions_.end()) {
                    auto def_it = deferred_functions_.find(fn_name);
                    if (def_it != deferred_functions_.end()) {
                        compile_function(fn_name, def_it->second, all_args);
                        cf_it = compiled_functions_.find(fn_name);
                    }
                }
                if (cf_it != compiled_functions_.end())
                    return {builder_->CreateCall(cf_it->second.fn, {arg.val}), cf_it->second.return_type};
                auto ext_it = extern_functions_.find(fn_name);
                if (ext_it != extern_functions_.end()) {
                    auto* ext_fn = module_->getFunction(ext_it->second);
                    if (!ext_fn) {
                        auto fn_type = llvm::FunctionType::get(arg.val->getType(), {arg.val->getType()}, false);
                        ext_fn = Function::Create(fn_type, Function::ExternalLinkage, ext_it->second, module_.get());
                    }
                    return {builder_->CreateCall(ext_fn, {arg.val}), CType::INT};
                }
                auto nv_it = named_values_.find(fn_name);
                if (nv_it != named_values_.end() && nv_it->second.type == CType::FUNCTION && nv_it->second.val) {
                    auto fn_type = llvm::FunctionType::get(arg.val->getType(), {arg.val->getType()}, false);
                    return {builder_->CreateCall(fn_type, nv_it->second.val, {arg.val}), arg.type};
                }
            }
            report_error(pe->source_context, "pipe: left side must be a function");
            return {};
        }
        case AST_LET_EXPR:        return codegen_let(static_cast<LetExpr*>(node));
        case AST_IF_EXPR:         return codegen_if(static_cast<IfExpr*>(node));
        case AST_CASE_EXPR:       return codegen_case(static_cast<CaseExpr*>(node));
        case AST_DO_EXPR:         return codegen_do(static_cast<DoExpr*>(node));
        case AST_RAISE_EXPR:      return codegen_raise(static_cast<RaiseExpr*>(node));
        case AST_TRY_CATCH_EXPR:  return codegen_try_catch(static_cast<TryCatchExpr*>(node));
        case AST_WITH_EXPR:      return codegen_with(static_cast<WithExpr*>(node));
        case AST_IDENTIFIER_EXPR: return codegen_identifier(static_cast<IdentifierExpr*>(node));
        case AST_FUNCTION_EXPR:   return codegen_function_def(static_cast<FunctionExpr*>(node), "");
        case AST_APPLY_EXPR:      return codegen_apply(static_cast<ApplyExpr*>(node));
        case AST_LAMBDA_ALIAS:    return codegen_lambda_alias(static_cast<LambdaAlias*>(node));
        case AST_IMPORT_EXPR:     return codegen_import(static_cast<ImportExpr*>(node));
        case AST_EXTERN_DECL:    return codegen_extern_decl(static_cast<ExternDeclExpr*>(node));
        case AST_FIELD_UPDATE_EXPR: {
            auto* fu = static_cast<FieldUpdateExpr*>(node);
            auto obj = codegen(fu->identifier);
            if (!obj || obj.type != CType::ADT) {
                report_error(fu->source_context, "field update requires ADT value");
                return {};
            }
            // Find the constructor with the matching field names
            for (auto& [ctor_name, info] : adt_constructors_) {
                if (info.field_names.empty()) continue;
                // Copy the struct, replace updated fields
                Value* result = obj.val;
                for (auto& [name_expr, val_expr] : fu->updates) {
                    auto new_val = codegen(val_expr);
                    if (!new_val) return {};
                    for (size_t fi = 0; fi < info.field_names.size(); fi++) {
                        if (info.field_names[fi] == name_expr->value) {
                            if (info.is_recursive) {
                                // Heap ADT: create new node, copy all fields, replace one
                                // For simplicity, not supported yet for recursive types
                                report_error(fu->source_context, "field update on recursive ADT not supported");
                                return {};
                            }
                            Value* store_val = new_val.val;
                            if (store_val->getType() != LType::getInt64Ty(*context_)) {
                                if (store_val->getType()->isPointerTy())
                                    store_val = builder_->CreatePtrToInt(store_val, LType::getInt64Ty(*context_));
                            }
                            result = builder_->CreateInsertValue(result, store_val, {(unsigned)(fi + 1)});
                            break;
                        }
                    }
                }
                return {result, CType::ADT};
            }
            report_error(fu->source_context, "no ADT constructor found for field update");
            return {};
        }
        case AST_FIELD_ACCESS_EXPR: {
            auto* fa = static_cast<FieldAccessExpr*>(node);
            auto obj = codegen(fa->identifier);
            if (!obj) return {};
            std::string field_name = fa->name->value;
            if (obj.type == CType::ADT) {
                for (auto& [ctor_name, info] : adt_constructors_) {
                    for (size_t fi = 0; fi < info.field_names.size(); fi++) {
                        if (info.field_names[fi] == field_name) {
                            CType ftype = (fi < info.field_types.size()) ? info.field_types[fi] : CType::INT;
                            if (info.is_recursive) {
                                auto val = builder_->CreateCall(rt_adt_get_field_,
                                    {obj.val, ConstantInt::get(LType::getInt64Ty(*context_), fi)});
                                if (ftype == CType::STRING || ftype == CType::SEQ || ftype == CType::ADT)
                                    return {builder_->CreateIntToPtr(val, PointerType::get(*context_, 0)), ftype};
                                return {val, ftype};
                            } else {
                                auto val = builder_->CreateExtractValue(obj.val, {(unsigned)(fi + 1)});
                                // Cast i64 to ptr if field type is pointer-based
                                if (ftype == CType::STRING || ftype == CType::SEQ ||
                                    ftype == CType::SET || ftype == CType::DICT ||
                                    ftype == CType::FUNCTION || ftype == CType::ADT) {
                                    val = builder_->CreateIntToPtr(val, PointerType::get(*context_, 0));
                                }
                                return {val, ftype};
                            }
                        }
                    }
                }
            }
            report_error(fa->source_context, "unknown field '" + field_name + "'");
            return {};
        }
        case AST_TUPLE_EXPR:      return codegen_tuple(static_cast<TupleExpr*>(node));
        case AST_VALUES_SEQUENCE_EXPR: return codegen_seq(static_cast<ValuesSequenceExpr*>(node));
        case AST_SET_EXPR:        return codegen_set(static_cast<SetExpr*>(node));
        case AST_DICT_EXPR:       return codegen_dict(static_cast<DictExpr*>(node));
        case AST_CONS_LEFT_EXPR:  return codegen_cons(static_cast<ConsLeftExpr*>(node));
        case AST_JOIN_EXPR:       return codegen_join(static_cast<JoinExpr*>(node));
        case AST_SEQ_GENERATOR_EXPR: return codegen_seq_generator(static_cast<SeqGeneratorExpr*>(node));
        case AST_SET_GENERATOR_EXPR: return codegen_set_generator(static_cast<SetGeneratorExpr*>(node));
        case AST_DICT_GENERATOR_EXPR: return codegen_dict_generator(static_cast<DictGeneratorExpr*>(node));
        case AST_ADT_DECL:       return {}; // handled at module level
        case AST_ADT_CONSTRUCTOR: return {};
        case AST_CONSTRUCTOR_PATTERN: return {};
        default:
            report_error(node->source_context, "unsupported expression type");
            return {};
    }
}

// ===== Symbol interning =====

int64_t Codegen::intern_symbol(const std::string& name) {
    auto it = symbol_ids_.find(name);
    if (it != symbol_ids_.end()) return it->second;
    int64_t id = static_cast<int64_t>(symbol_strings_.size());
    symbol_ids_[name] = id;
    symbol_strings_.push_back(builder_->CreateGlobalStringPtr(name, "sym." + name));
    return id;
}

// ===== CFFI =====

void Codegen::register_cffi_signatures() {
    // TODO: Register known C library function signatures
}

bool Codegen::is_cffi_import(const std::string& mod_fqn) {
    return mod_fqn.size() >= 2 && mod_fqn[0] == 'C' && mod_fqn[1] == '\\';
}

// ===== Type annotation helpers (local to this TU for compile_module) =====

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
    if (std::holds_alternative<std::shared_ptr<types::NamedType>>(t))
        return CType::ADT;
    if (std::holds_alternative<std::shared_ptr<types::PromiseType>>(t))
        return CType::PROMISE;
    return CType::INT;
}

// Decompose a curried function type (Int -> Int -> Int) into param types + return type
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
