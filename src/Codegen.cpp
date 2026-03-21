//
// LLVM Code Generation for Yona
//

#include "Codegen.h"

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

#include <iostream>
#include <sstream>

namespace yona::compiler::codegen {

using namespace llvm;
// Alias to avoid collision with llvm::Type
using YonaType = types::Type;

Codegen::Codegen(const std::string& module_name) {
    context_ = std::make_unique<LLVMContext>();
    module_ = std::make_unique<Module>(module_name, *context_);
    builder_ = std::make_unique<IRBuilder<>>(*context_);
    init_target();
    declare_runtime();
}

Codegen::~Codegen() = default;

void Codegen::init_target() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    auto target_triple = sys::getDefaultTargetTriple();
    module_->setTargetTriple(Triple(target_triple));

    std::string error;
    auto target = TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
        std::cerr << "Target lookup failed: " << error << std::endl;
        return;
    }

    auto cpu = "generic";
    auto features = "";
    TargetOptions opt;
    target_machine_ = target->createTargetMachine(
        target_triple, cpu, features, opt, Reloc::PIC_);
    module_->setDataLayout(target_machine_->createDataLayout());
}

void Codegen::declare_runtime() {
    auto i64_ty = llvm::Type::getInt64Ty(*context_);
    auto double_ty = llvm::Type::getDoubleTy(*context_);
    auto void_ty = llvm::Type::getVoidTy(*context_);
    auto i1_ty = llvm::Type::getInt1Ty(*context_);
    auto i8ptr_ty = llvm::PointerType::get(llvm::Type::getInt8Ty(*context_), 0);

    // void yona_rt_print_int(i64)
    rt_print_int_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty, {i64_ty}, false),
        llvm::Function::ExternalLinkage, "yona_rt_print_int", module_.get());

    // void yona_rt_print_float(double)
    rt_print_float_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty, {double_ty}, false),
        llvm::Function::ExternalLinkage, "yona_rt_print_float", module_.get());

    // void yona_rt_print_string(i8*)
    rt_print_string_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty, {i8ptr_ty}, false),
        llvm::Function::ExternalLinkage, "yona_rt_print_string", module_.get());

    // void yona_rt_print_bool(i1)
    rt_print_bool_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty, {i1_ty}, false),
        llvm::Function::ExternalLinkage, "yona_rt_print_bool", module_.get());

    // void yona_rt_print_newline()
    rt_print_newline_ = llvm::Function::Create(
        llvm::FunctionType::get(void_ty, {}, false),
        llvm::Function::ExternalLinkage, "yona_rt_print_newline", module_.get());

    // i8* yona_rt_string_concat(i8*, i8*)
    rt_string_concat_ = llvm::Function::Create(
        llvm::FunctionType::get(i8ptr_ty, {i8ptr_ty, i8ptr_ty}, false),
        llvm::Function::ExternalLinkage, "yona_rt_string_concat", module_.get());
}

// ===== Public API =====

Module* Codegen::compile(AstNode* node) {
    auto main_fn = codegen_main(node);
    if (!main_fn) return nullptr;

    // Verify the module
    std::string verify_err;
    raw_string_ostream verify_os(verify_err);
    if (verifyModule(*module_, &verify_os)) {
        std::cerr << "Module verification failed:\n" << verify_err << std::endl;
        return nullptr;
    }

    return module_.get();
}

bool Codegen::emit_object_file(const std::string& output_path) {
    if (!target_machine_) return false;

    std::error_code ec;
    raw_fd_ostream dest(output_path, ec, sys::fs::OF_None);
    if (ec) {
        std::cerr << "Could not open file: " << ec.message() << std::endl;
        return false;
    }

    legacy::PassManager pass;
    if (target_machine_->addPassesToEmitFile(pass, dest, nullptr,
                                             CodeGenFileType::ObjectFile)) {
        std::cerr << "Target machine can't emit object file" << std::endl;
        return false;
    }

    pass.run(*module_);
    dest.flush();
    return true;
}

std::string Codegen::emit_ir() {
    std::string ir;
    raw_string_ostream os(ir);
    module_->print(os, nullptr);
    return ir;
}

// ===== Codegen Entry Point =====

Function* Codegen::codegen_main(AstNode* node) {
    // Create main() function
    auto i32_ty = llvm::Type::getInt32Ty(*context_);
    auto main_type = llvm::FunctionType::get(i32_ty, {}, false);
    auto main_fn = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage,
                                     "main", module_.get());

    auto entry_bb = llvm::BasicBlock::Create(*context_, "entry", main_fn);
    builder_->SetInsertPoint(entry_bb);

    // Generate code for the expression
    auto result = codegen(node);

    if (result) {
        // Print the result
        codegen_print_value(result);
    }

    // Return 0
    builder_->CreateRet(llvm::ConstantInt::get(i32_ty, 0));
    return main_fn;
}

// ===== Expression Codegen =====

Value* Codegen::codegen(AstNode* node) {
    if (!node) return nullptr;

    switch (node->get_type()) {
        case AST_MAIN:
            return codegen_main_node(static_cast<MainNode*>(node));
        case AST_INTEGER_EXPR:
            return codegen_integer(static_cast<IntegerExpr*>(node));
        case AST_FLOAT_EXPR:
            return codegen_float(static_cast<FloatExpr*>(node));
        case AST_TRUE_LITERAL_EXPR:
            return codegen_bool(static_cast<TrueLiteralExpr*>(node));
        case AST_FALSE_LITERAL_EXPR:
            return codegen_bool_false(static_cast<FalseLiteralExpr*>(node));
        case AST_STRING_EXPR:
            return codegen_string(static_cast<StringExpr*>(node));
        case AST_ADD_EXPR:
            return codegen_add(static_cast<AddExpr*>(node));
        case AST_SUBTRACT_EXPR:
            return codegen_subtract(static_cast<SubtractExpr*>(node));
        case AST_MULTIPLY_EXPR:
            return codegen_multiply(static_cast<MultiplyExpr*>(node));
        case AST_DIVIDE_EXPR:
            return codegen_divide(static_cast<DivideExpr*>(node));
        case AST_MODULO_EXPR:
            return codegen_modulo(static_cast<ModuloExpr*>(node));
        case AST_EQ_EXPR:
            return codegen_eq(static_cast<EqExpr*>(node));
        case AST_NEQ_EXPR:
            return codegen_neq(static_cast<NeqExpr*>(node));
        case AST_LT_EXPR:
            return codegen_lt(static_cast<LtExpr*>(node));
        case AST_GT_EXPR:
            return codegen_gt(static_cast<GtExpr*>(node));
        case AST_LTE_EXPR:
            return codegen_lte(static_cast<LteExpr*>(node));
        case AST_GTE_EXPR:
            return codegen_gte(static_cast<GteExpr*>(node));
        case AST_LOGICAL_AND_EXPR:
            return codegen_and(static_cast<LogicalAndExpr*>(node));
        case AST_LOGICAL_OR_EXPR:
            return codegen_or(static_cast<LogicalOrExpr*>(node));
        case AST_LOGICAL_NOT_OP_EXPR:
            return codegen_not(static_cast<LogicalNotOpExpr*>(node));
        case AST_LET_EXPR:
            return codegen_let(static_cast<LetExpr*>(node));
        case AST_IF_EXPR:
            return codegen_if(static_cast<IfExpr*>(node));
        case AST_IDENTIFIER_EXPR:
            return codegen_identifier(static_cast<IdentifierExpr*>(node));
        default:
            std::cerr << "Codegen: unsupported AST node type " << node->get_type() << std::endl;
            return nullptr;
    }
}

Value* Codegen::codegen_main_node(MainNode* node) {
    return codegen(node->node);
}

// ===== Literals =====

Value* Codegen::codegen_integer(IntegerExpr* node) {
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), node->value);
}

Value* Codegen::codegen_float(FloatExpr* node) {
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), node->value);
}

Value* Codegen::codegen_bool(TrueLiteralExpr*) {
    return llvm::ConstantInt::getTrue(*context_);
}

Value* Codegen::codegen_bool_false(FalseLiteralExpr*) {
    return llvm::ConstantInt::getFalse(*context_);
}

Value* Codegen::codegen_string(StringExpr* node) {
    return builder_->CreateGlobalStringPtr(node->value, "str");
}

// ===== Arithmetic =====

Value* Codegen::codegen_add(AddExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;

    if (left->getType()->isIntegerTy(64) && right->getType()->isIntegerTy(64))
        return builder_->CreateAdd(left, right, "addtmp");
    if (left->getType()->isDoubleTy() && right->getType()->isDoubleTy())
        return builder_->CreateFAdd(left, right, "addtmp");
    // Mixed: promote int to float
    if (left->getType()->isIntegerTy(64) && right->getType()->isDoubleTy()) {
        left = builder_->CreateSIToFP(left, llvm::Type::getDoubleTy(*context_));
        return builder_->CreateFAdd(left, right, "addtmp");
    }
    if (left->getType()->isDoubleTy() && right->getType()->isIntegerTy(64)) {
        right = builder_->CreateSIToFP(right, llvm::Type::getDoubleTy(*context_));
        return builder_->CreateFAdd(left, right, "addtmp");
    }
    // String concatenation
    if (left->getType()->isPointerTy() && right->getType()->isPointerTy())
        return builder_->CreateCall(rt_string_concat_, {left, right}, "concat");
    return nullptr;
}

Value* Codegen::codegen_subtract(SubtractExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;

    if (left->getType()->isIntegerTy(64))
        return builder_->CreateSub(left, right, "subtmp");
    return builder_->CreateFSub(left, right, "subtmp");
}

Value* Codegen::codegen_multiply(MultiplyExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;

    if (left->getType()->isIntegerTy(64))
        return builder_->CreateMul(left, right, "multmp");
    return builder_->CreateFMul(left, right, "multmp");
}

Value* Codegen::codegen_divide(DivideExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;

    if (left->getType()->isIntegerTy(64))
        return builder_->CreateSDiv(left, right, "divtmp");
    return builder_->CreateFDiv(left, right, "divtmp");
}

Value* Codegen::codegen_modulo(ModuloExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;
    return builder_->CreateSRem(left, right, "modtmp");
}

// ===== Comparison =====

Value* Codegen::codegen_eq(EqExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;

    if (left->getType()->isIntegerTy(64))
        return builder_->CreateICmpEQ(left, right, "eqtmp");
    return builder_->CreateFCmpOEQ(left, right, "eqtmp");
}

Value* Codegen::codegen_neq(NeqExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;

    if (left->getType()->isIntegerTy(64))
        return builder_->CreateICmpNE(left, right, "neqtmp");
    return builder_->CreateFCmpONE(left, right, "neqtmp");
}

Value* Codegen::codegen_lt(LtExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;

    if (left->getType()->isIntegerTy(64))
        return builder_->CreateICmpSLT(left, right, "lttmp");
    return builder_->CreateFCmpOLT(left, right, "lttmp");
}

Value* Codegen::codegen_gt(GtExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;

    if (left->getType()->isIntegerTy(64))
        return builder_->CreateICmpSGT(left, right, "gttmp");
    return builder_->CreateFCmpOGT(left, right, "gttmp");
}

Value* Codegen::codegen_lte(LteExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;

    if (left->getType()->isIntegerTy(64))
        return builder_->CreateICmpSLE(left, right, "letmp");
    return builder_->CreateFCmpOLE(left, right, "letmp");
}

Value* Codegen::codegen_gte(GteExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;

    if (left->getType()->isIntegerTy(64))
        return builder_->CreateICmpSGE(left, right, "getmp");
    return builder_->CreateFCmpOGE(left, right, "getmp");
}

// ===== Logical =====

Value* Codegen::codegen_and(LogicalAndExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;
    return builder_->CreateAnd(left, right, "andtmp");
}

Value* Codegen::codegen_or(LogicalOrExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return nullptr;
    return builder_->CreateOr(left, right, "ortmp");
}

Value* Codegen::codegen_not(LogicalNotOpExpr* node) {
    auto val = codegen(node->expr);
    if (!val) return nullptr;
    return builder_->CreateNot(val, "nottmp");
}

// ===== Let Bindings =====

Value* Codegen::codegen_let(LetExpr* node) {
    // Process each alias binding
    for (auto* alias : node->aliases) {
        if (auto* val_alias = dynamic_cast<ValueAlias*>(alias)) {
            auto val = codegen(val_alias->expr);
            if (val) {
                named_values_[val_alias->identifier->name->value] = val;
            }
        }
        // TODO: LambdaAlias, PatternAlias
    }

    // Evaluate and return the body
    return codegen(node->expr);
}

// ===== If Expression =====

Value* Codegen::codegen_if(IfExpr* node) {
    auto cond = codegen(node->condition);
    if (!cond) return nullptr;

    // Convert condition to i1 if it's an i64
    if (cond->getType()->isIntegerTy(64)) {
        cond = builder_->CreateICmpNE(cond, llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0), "ifcond");
    }

    auto parent_fn = builder_->GetInsertBlock()->getParent();

    auto then_bb = llvm::BasicBlock::Create(*context_, "then", parent_fn);
    auto else_bb = llvm::BasicBlock::Create(*context_, "else");
    auto merge_bb = llvm::BasicBlock::Create(*context_, "ifcont");

    builder_->CreateCondBr(cond, then_bb, else_bb);

    // Then block
    builder_->SetInsertPoint(then_bb);
    auto then_val = codegen(node->thenExpr);
    if (!then_val) return nullptr;
    builder_->CreateBr(merge_bb);
    then_bb = builder_->GetInsertBlock();

    // Else block
    parent_fn->insert(parent_fn->end(), else_bb);
    builder_->SetInsertPoint(else_bb);
    auto else_val = codegen(node->elseExpr);
    if (!else_val) return nullptr;
    builder_->CreateBr(merge_bb);
    else_bb = builder_->GetInsertBlock();

    // Merge block
    parent_fn->insert(parent_fn->end(), merge_bb);
    builder_->SetInsertPoint(merge_bb);

    // PHI node to select the result
    auto phi = builder_->CreatePHI(then_val->getType(), 2, "iftmp");
    phi->addIncoming(then_val, then_bb);
    phi->addIncoming(else_val, else_bb);
    return phi;
}

// ===== Identifier =====

Value* Codegen::codegen_identifier(IdentifierExpr* node) {
    auto it = named_values_.find(node->name->value);
    if (it != named_values_.end()) {
        return it->second;
    }
    std::cerr << "Codegen: unknown variable '" << node->name->value << "'" << std::endl;
    return nullptr;
}

// ===== Print Helper =====

void Codegen::codegen_print_value(Value* val) {
    if (!val) return;

    if (val->getType()->isIntegerTy(64)) {
        builder_->CreateCall(rt_print_int_, {val});
    } else if (val->getType()->isDoubleTy()) {
        builder_->CreateCall(rt_print_float_, {val});
    } else if (val->getType()->isIntegerTy(1)) {
        builder_->CreateCall(rt_print_bool_, {val});
    } else if (val->getType()->isPointerTy()) {
        builder_->CreateCall(rt_print_string_, {val});
    }
    builder_->CreateCall(rt_print_newline_, {});
}

// ===== Type Mapping =====

llvm::Type* Codegen::get_llvm_type(const types::Type& yona_type) {
    if (std::holds_alternative<types::BuiltinType>(yona_type)) {
        switch (std::get<types::BuiltinType>(yona_type)) {
            case types::SignedInt64: return llvm::Type::getInt64Ty(*context_);
            case types::Float64: return llvm::Type::getDoubleTy(*context_);
            case types::Bool: return llvm::Type::getInt1Ty(*context_);
            case types::Unit: return llvm::Type::getVoidTy(*context_);
            case types::String: return llvm::PointerType::get(llvm::Type::getInt8Ty(*context_), 0);
            default: return llvm::Type::getInt64Ty(*context_);
        }
    }
    return llvm::Type::getInt64Ty(*context_); // fallback
}

} // namespace yona::compiler::codegen
