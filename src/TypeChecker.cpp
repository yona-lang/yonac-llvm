//
// Created by akovari on 6.1.25.
//

#include <sstream>
#include <algorithm>

#include "TypeChecker.h"

namespace yona::typechecker {

using namespace compiler::types;

// Use the helper functions from types.h instead of defining our own

// TypeSubstitution implementation
Type TypeSubstitution::apply(const Type& type) const {
    // If it's a type variable, check if we have a substitution for it
    if (auto named_type = get_if<shared_ptr<NamedType>>(&type)) {
        // Check if this is a type variable (starts with lowercase or is a number)
        const string& name = (*named_type)->name;
        if (!name.empty() && (islower(name[0]) || isdigit(name[0]))) {
            // This is a type variable, check for substitution
            // For now, we'll use the name as the ID (this needs improvement)
            try {
                int var_id = stoi(name);
                auto it = substitutions.find(var_id);
                if (it != substitutions.end()) {
                    return apply(it->second); // Recursively apply substitutions
                }
            } catch (...) {
                // Not a numeric type variable
            }
        }
    }
    
    // Apply substitution recursively to complex types
    if (auto func_type = get_if<shared_ptr<FunctionType>>(&type)) {
        auto new_func = make_shared<FunctionType>();
        new_func->argumentType = apply((*func_type)->argumentType);
        new_func->returnType = apply((*func_type)->returnType);
        return Type(new_func);
    }
    
    if (auto seq_type = get_if<shared_ptr<SingleItemCollectionType>>(&type)) {
        auto new_seq = make_shared<SingleItemCollectionType>();
        new_seq->kind = (*seq_type)->kind;
        new_seq->valueType = apply((*seq_type)->valueType);
        return Type(new_seq);
    }
    
    if (auto dict_type = get_if<shared_ptr<DictCollectionType>>(&type)) {
        auto new_dict = make_shared<DictCollectionType>();
        new_dict->keyType = apply((*dict_type)->keyType);
        new_dict->valueType = apply((*dict_type)->valueType);
        return Type(new_dict);
    }
    
    if (auto product_type = get_if<shared_ptr<ProductType>>(&type)) {
        auto new_product = make_shared<ProductType>();
        for (const auto& t : (*product_type)->types) {
            new_product->types.push_back(apply(t));
        }
        return Type(new_product);
    }
    
    if (auto sum_type = get_if<shared_ptr<SumType>>(&type)) {
        auto new_sum = make_shared<SumType>();
        for (const auto& t : (*sum_type)->types) {
            new_sum->types.insert(apply(t));
        }
        return Type(new_sum);
    }
    
    // No substitution needed
    return type;
}

TypeSubstitution TypeSubstitution::compose(const TypeSubstitution& other) const {
    TypeSubstitution result;
    
    // First apply this substitution to all of other's substitutions
    for (const auto& [var_id, type] : other.substitutions) {
        result.bind(var_id, apply(type));
    }
    
    // Then add all of this substitution's bindings (they override other's)
    for (const auto& [var_id, type] : substitutions) {
        result.bind(var_id, type);
    }
    
    return result;
}

// TypeChecker implementation
Type TypeChecker::check(AstNode* node) const {
    if (!node) {
        context.add_error(SourceLocation::unknown(), "Null AST node");
        return Type(nullptr);
    }
    
    auto result = node->accept(*this);
    
    // Extract Type from any
    try {
        return any_cast<Type>(result);
    } catch (const bad_any_cast& e) {
        context.add_error(node->source_context, 
                         "Internal error: visitor did not return Type");
        return Type(nullptr);
    }
}

void TypeChecker::import_module_types(const string& module_name, 
                                    const unordered_map<string, RecordTypeInfo>& records,
                                    const unordered_map<string, Type>& exports) {
    module_records[module_name] = records;
    module_exports[module_name] = exports;
}

UnificationResult TypeChecker::unify(const Type& t1, const Type& t2) const {
    // If both are the same type, unification succeeds
    if (t1.index() == t2.index()) {
        // Handle built-in types
        if (holds_alternative<BuiltinType>(t1)) {
            if (get<BuiltinType>(t1) == get<BuiltinType>(t2)) {
                return UnificationResult::ok(TypeSubstitution{});
            }
            return UnificationResult::error("Cannot unify different built-in types");
        }
        
        // Handle function types
        if (auto f1 = get_if<shared_ptr<FunctionType>>(&t1)) {
            if (auto f2 = get_if<shared_ptr<FunctionType>>(&t2)) {
                auto arg_result = unify((*f1)->argumentType, (*f2)->argumentType);
                if (!arg_result.success) return arg_result;
                
                auto ret_result = unify((*f1)->returnType, (*f2)->returnType);
                if (!ret_result.success) return ret_result;
                
                return UnificationResult::ok(arg_result.substitution.compose(ret_result.substitution));
            }
        }
        
        // Handle collection types
        if (auto s1 = get_if<shared_ptr<SingleItemCollectionType>>(&t1)) {
            if (auto s2 = get_if<shared_ptr<SingleItemCollectionType>>(&t2)) {
                if ((*s1)->kind != (*s2)->kind) {
                    return UnificationResult::error("Cannot unify different collection kinds");
                }
                return unify((*s1)->valueType, (*s2)->valueType);
            }
        }
        
        // Handle named types (including type variables)
        if (auto n1 = get_if<shared_ptr<NamedType>>(&t1)) {
            if (auto n2 = get_if<shared_ptr<NamedType>>(&t2)) {
                if ((*n1)->name == (*n2)->name) {
                    return UnificationResult::ok(TypeSubstitution{});
                }
                // If one is a type variable, create substitution
                if (islower((*n1)->name[0]) || isdigit((*n1)->name[0])) {
                    TypeSubstitution sub;
                    try {
                        sub.bind(stoi((*n1)->name), t2);
                        return UnificationResult::ok(sub);
                    } catch (...) {}
                }
                if (islower((*n2)->name[0]) || isdigit((*n2)->name[0])) {
                    TypeSubstitution sub;
                    try {
                        sub.bind(stoi((*n2)->name), t1);
                        return UnificationResult::ok(sub);
                    } catch (...) {}
                }
            }
        }
    }
    
    // Handle type variable on one side
    if (auto n1 = get_if<shared_ptr<NamedType>>(&t1)) {
        if (islower((*n1)->name[0]) || isdigit((*n1)->name[0])) {
            TypeSubstitution sub;
            try {
                sub.bind(stoi((*n1)->name), t2);
                return UnificationResult::ok(sub);
            } catch (...) {}
        }
    }
    if (auto n2 = get_if<shared_ptr<NamedType>>(&t2)) {
        if (islower((*n2)->name[0]) || isdigit((*n2)->name[0])) {
            TypeSubstitution sub;
            try {
                sub.bind(stoi((*n2)->name), t1);
                return UnificationResult::ok(sub);
            } catch (...) {}
        }
    }
    
    stringstream ss;
    ss << "Cannot unify types";
    return UnificationResult::error(ss.str());
}

Type TypeChecker::instantiate(const Type& type) const {
    // For now, just return the type as-is
    // TODO: Implement proper instantiation of polymorphic types
    return type;
}

Type TypeChecker::generalize(const Type& type, const TypeEnvironment& env) const {
    // For now, just return the type as-is
    // TODO: Implement proper generalization
    return type;
}

// Visitor implementations for literals
any TypeChecker::visit(IntegerExpr *node) const {
    return Type(compiler::types::SignedInt64);
}

any TypeChecker::visit(FloatExpr *node) const {
    return Type(compiler::types::Float64);
}

any TypeChecker::visit(ByteExpr *node) const {
    return Type(compiler::types::Byte);
}

any TypeChecker::visit(CharacterExpr *node) const {
    return Type(compiler::types::Char);
}

any TypeChecker::visit(StringExpr *node) const {
    return Type(compiler::types::String);
}

any TypeChecker::visit(TrueLiteralExpr *node) const {
    return Type(compiler::types::Bool);
}

any TypeChecker::visit(FalseLiteralExpr *node) const {
    return Type(compiler::types::Bool);
}

any TypeChecker::visit(UnitExpr *node) const {
    return Type(compiler::types::Unit);
}

any TypeChecker::visit(SymbolExpr *node) const {
    return Type(compiler::types::Symbol);
}

// Visitor for identifiers
any TypeChecker::visit(IdentifierExpr *node) const {
    if (!node->name) {
        context.add_error(node->source_context, 
                         "Identifier has no name");
        return Type(nullptr);
    }
    
    string name = node->name->value;
    auto type_opt = env->lookup(name);
    
    if (!type_opt) {
        context.add_error(node->source_context, 
                         "Undefined variable: " + name);
        return Type(nullptr);
    }
    
    return instantiate(*type_opt);
}

// Visitor for collections
any TypeChecker::visit(TupleExpr *node) const {
    auto product = make_shared<ProductType>();
    
    for (auto* expr : node->values) {
        Type elem_type = check(expr);
        product->types.push_back(elem_type);
    }
    
    return Type(product);
}

any TypeChecker::visit(ValuesSequenceExpr *node) const {
    if (!node->values.empty()) {
        // Infer element type from first element
        Type elem_type = check(node->values[0]);
        
        // Check that all elements have the same type
        for (size_t i = 1; i < node->values.size(); i++) {
            Type t = check(node->values[i]);
            auto unif_result = unify(elem_type, t);
            if (!unif_result.success) {
                context.add_error(node->values[i]->source_context,
                                "Type mismatch in sequence: " + unif_result.error_message.value_or(""));
            }
            elem_type = unif_result.substitution.apply(elem_type);
        }
        
        auto seq_type = make_shared<SingleItemCollectionType>();
        seq_type->kind = SingleItemCollectionType::Seq;
        seq_type->valueType = elem_type;
        return Type(seq_type);
    }
    
    // Empty sequence - use type variable
    auto var = context.fresh_type_var();
    auto elem_type = make_shared<NamedType>();
    elem_type->name = to_string(var->id);
    elem_type->type = nullptr;
    
    auto seq_type = make_shared<SingleItemCollectionType>();
    seq_type->kind = SingleItemCollectionType::Seq;
    seq_type->valueType = Type(elem_type);
    return Type(seq_type);
}

any TypeChecker::visit(SetExpr *node) const {
    if (!node->values.empty()) {
        // Infer element type from first element
        Type elem_type = check(node->values[0]);
        
        // Check that all elements have the same type
        for (size_t i = 1; i < node->values.size(); i++) {
            Type t = check(node->values[i]);
            auto unif_result = unify(elem_type, t);
            if (!unif_result.success) {
                context.add_error(node->values[i]->source_context,
                                "Type mismatch in set: " + unif_result.error_message.value_or(""));
            }
            elem_type = unif_result.substitution.apply(elem_type);
        }
        
        auto set_type = make_shared<SingleItemCollectionType>();
        set_type->kind = SingleItemCollectionType::Set;
        set_type->valueType = elem_type;
        return Type(set_type);
    }
    
    // Empty set - use type variable
    auto var = context.fresh_type_var();
    auto elem_type = make_shared<NamedType>();
    elem_type->name = to_string(var->id);
    elem_type->type = nullptr;
    
    auto set_type = make_shared<SingleItemCollectionType>();
    set_type->kind = SingleItemCollectionType::Set;
    set_type->valueType = Type(elem_type);
    return Type(set_type);
}

any TypeChecker::visit(DictExpr *node) const {
    if (!node->values.empty()) {
        // Infer key and value types from first pair
        Type key_type = check(node->values[0].first);
        Type value_type = check(node->values[0].second);
        
        // Check that all pairs have consistent types
        for (size_t i = 1; i < node->values.size(); i++) {
            Type k = check(node->values[i].first);
            Type v = check(node->values[i].second);
            
            auto key_unif = unify(key_type, k);
            if (!key_unif.success) {
                context.add_error(node->values[i].first->source_context,
                                "Type mismatch in dict key: " + key_unif.error_message.value_or(""));
            }
            key_type = key_unif.substitution.apply(key_type);
            
            auto val_unif = unify(value_type, v);
            if (!val_unif.success) {
                context.add_error(node->values[i].second->source_context,
                                "Type mismatch in dict value: " + val_unif.error_message.value_or(""));
            }
            value_type = val_unif.substitution.apply(value_type);
        }
        
        auto dict_type = make_shared<DictCollectionType>();
        dict_type->keyType = key_type;
        dict_type->valueType = value_type;
        return Type(dict_type);
    }
    
    // Empty dict - use type variables
    auto key_var = context.fresh_type_var();
    auto key_type = make_shared<NamedType>();
    key_type->name = to_string(key_var->id);
    key_type->type = nullptr;
    
    auto val_var = context.fresh_type_var();
    auto val_type = make_shared<NamedType>();
    val_type->name = to_string(val_var->id);
    val_type->type = nullptr;
    
    auto dict_type = make_shared<DictCollectionType>();
    dict_type->keyType = Type(key_type);
    dict_type->valueType = Type(val_type);
    return Type(dict_type);
}

// Binary operators
any TypeChecker::visit(AddExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    // Addition works on numeric types and strings
    if (is_numeric(left_type) && is_numeric(right_type)) {
        return derive_bin_op_result_type(left_type, right_type);
    }
    
    if (holds_alternative<BuiltinType>(left_type) && 
        get<BuiltinType>(left_type) == compiler::types::String &&
        holds_alternative<BuiltinType>(right_type) && 
        get<BuiltinType>(right_type) == compiler::types::String) {
        return Type(compiler::types::String);
    }
    
    context.add_error(node->source_context,
                     "Type error: + operator requires numeric types or strings");
    return Type(nullptr);
}

any TypeChecker::visit(SubtractExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: - operator requires numeric types");
        return Type(nullptr);
    }
    
    return derive_bin_op_result_type(left_type, right_type);
}

any TypeChecker::visit(MultiplyExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: * operator requires numeric types");
        return Type(nullptr);
    }
    
    return derive_bin_op_result_type(left_type, right_type);
}

any TypeChecker::visit(DivideExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: / operator requires numeric types");
        return Type(nullptr);
    }
    
    // Division always returns float
    return Type(compiler::types::Float64);
}

any TypeChecker::visit(ModuloExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!is_integer(left_type) || !is_integer(right_type)) {
        context.add_error(node->source_context,
                         "Type error: % operator requires integer types");
        return Type(nullptr);
    }
    
    return derive_bin_op_result_type(left_type, right_type);
}

any TypeChecker::visit(PowerExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: ** operator requires numeric types");
        return Type(nullptr);
    }
    
    // Power always returns float
    return Type(compiler::types::Float64);
}

// Comparison operators
any TypeChecker::visit(EqExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    auto unif_result = unify(left_type, right_type);
    if (!unif_result.success) {
        context.add_error(node->source_context,
                         "Type error: == operator requires compatible types");
    }
    
    return Type(compiler::types::Bool);
}

any TypeChecker::visit(NeqExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    auto unif_result = unify(left_type, right_type);
    if (!unif_result.success) {
        context.add_error(node->source_context,
                         "Type error: != operator requires compatible types");
    }
    
    return Type(compiler::types::Bool);
}

any TypeChecker::visit(LtExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: < operator requires numeric types");
    }
    
    return Type(compiler::types::Bool);
}

any TypeChecker::visit(GtExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: > operator requires numeric types");
    }
    
    return Type(compiler::types::Bool);
}

any TypeChecker::visit(LteExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: <= operator requires numeric types");
    }
    
    return Type(compiler::types::Bool);
}

any TypeChecker::visit(GteExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: >= operator requires numeric types");
    }
    
    return Type(compiler::types::Bool);
}

// Logical operators
any TypeChecker::visit(LogicalAndExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!holds_alternative<BuiltinType>(left_type) || get<BuiltinType>(left_type) != compiler::types::Bool ||
        !holds_alternative<BuiltinType>(right_type) || get<BuiltinType>(right_type) != compiler::types::Bool) {
        context.add_error(node->source_context,
                         "Type error: && operator requires boolean types");
    }
    
    return Type(compiler::types::Bool);
}

any TypeChecker::visit(LogicalOrExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);
    
    if (!holds_alternative<BuiltinType>(left_type) || get<BuiltinType>(left_type) != compiler::types::Bool ||
        !holds_alternative<BuiltinType>(right_type) || get<BuiltinType>(right_type) != compiler::types::Bool) {
        context.add_error(node->source_context,
                         "Type error: || operator requires boolean types");
    }
    
    return Type(compiler::types::Bool);
}

any TypeChecker::visit(LogicalNotOpExpr *node) const {
    Type operand_type = check(node->expr);
    
    if (!holds_alternative<BuiltinType>(operand_type) || get<BuiltinType>(operand_type) != compiler::types::Bool) {
        context.add_error(node->source_context,
                         "Type error: ! operator requires boolean type");
    }
    
    return Type(compiler::types::Bool);
}

// Control flow
any TypeChecker::visit(IfExpr *node) const {
    Type cond_type = check(node->condition);
    
    if (!holds_alternative<BuiltinType>(cond_type) || get<BuiltinType>(cond_type) != compiler::types::Bool) {
        context.add_error(node->condition->source_context,
                         "Type error: if condition must be boolean");
    }
    
    Type then_type = check(node->thenExpr);
    Type else_type = check(node->elseExpr);
    
    auto unif_result = unify(then_type, else_type);
    if (!unif_result.success) {
        context.add_error(node->source_context,
                         "Type error: if branches must have same type");
        return Type(nullptr);
    }
    
    return unif_result.substitution.apply(then_type);
}

any TypeChecker::visit(LetExpr *node) const {
    // Create new environment for let bindings
    auto new_env = env->extend();
    
    // Process each alias
    for (auto* alias : node->aliases) {
        if (auto val_alias = dynamic_cast<ValueAlias*>(alias)) {
            Type val_type = check(val_alias->expr);
            string name = val_alias->identifier->name->value;
            new_env->bind(name, generalize(val_type, *env));
        } else if (auto lambda_alias = dynamic_cast<LambdaAlias*>(alias)) {
            Type func_type = check(lambda_alias->lambda);
            string name = lambda_alias->name->value;
            new_env->bind(name, generalize(func_type, *env));
        } else if (auto pattern_alias = dynamic_cast<PatternAlias*>(alias)) {
            // Pattern aliases need special handling
            Type expr_type = check(pattern_alias->expr);
            // TODO: Extract bindings from pattern and add to environment
        }
        // TODO: Handle other alias types
    }
    
    // Type check body in new environment
    auto old_env = env;
    env = new_env;
    Type body_type = check(node->expr);
    env = old_env;
    
    return body_type;
}

any TypeChecker::visit(DoExpr *node) const {
    Type result_type = Type(compiler::types::Unit);
    
    for (auto* expr : node->steps) {
        result_type = check(expr);
    }
    
    return result_type;
}

// Function-related
any TypeChecker::visit(FunctionExpr *node) const {
    // TODO: Implement function type inference
    // This requires pattern matching and multiple clauses
    auto var = context.fresh_type_var();
    auto type_name = make_shared<NamedType>();
    type_name->name = to_string(var->id);
    type_name->type = nullptr;
    return Type(type_name);
}

any TypeChecker::visit(ApplyExpr *node) const {
    // Type check the function
    Type func_type = check(node->call);
    
    // Type check arguments
    vector<Type> arg_types;
    for (const auto& arg : node->args) {
        if (holds_alternative<ExprNode*>(arg)) {
            arg_types.push_back(check(get<ExprNode*>(arg)));
        }
    }
    
    // For now, create a type variable for the result
    auto result_var = context.fresh_type_var();
    auto result_type = make_shared<NamedType>();
    result_type->name = to_string(result_var->id);
    result_type->type = nullptr;
    
    // TODO: Properly unify with function type
    return Type(result_type);
}

any TypeChecker::visit(RecordInstanceExpr *node) const {
    // Look up record type
    string record_name = node->recordType->value;
    
    // Search in all imported modules
    const RecordTypeInfo* record_info = nullptr;
    for (const auto& [module, records] : module_records) {
        auto it = records.find(record_name);
        if (it != records.end()) {
            record_info = &it->second;
            break;
        }
    }
    
    if (!record_info) {
        context.add_error(node->source_context,
                         "Unknown record type: " + record_name);
        return Type(nullptr);
    }
    
    // Check that all fields are provided with correct types
    if (node->items.size() != record_info->field_names.size()) {
        context.add_error(node->source_context,
                         "Wrong number of fields for record " + record_name);
        return Type(nullptr);
    }
    
    // Type check each field
    for (size_t i = 0; i < node->items.size(); i++) {
        string provided_name = node->items[i].first->value;
        Type provided_type = check(node->items[i].second);
        
        // Find matching field
        bool found = false;
        for (size_t j = 0; j < record_info->field_names.size(); j++) {
            if (record_info->field_names[j] == provided_name) {
                auto unif_result = unify(record_info->field_types[j], provided_type);
                if (!unif_result.success) {
                    context.add_error(node->items[i].second->source_context,
                                    "Type mismatch for field " + provided_name);
                }
                found = true;
                break;
            }
        }
        
        if (!found) {
            context.add_error(node->source_context,
                            "Unknown field " + provided_name + " for record " + record_name);
        }
    }
    
    // Return the record type
    auto named_type = make_shared<NamedType>();
    named_type->name = record_name;
    named_type->type = nullptr; // TODO: Store full record type info
    return Type(named_type);
}

// Pattern matching
any TypeChecker::visit(CaseExpr *node) const {
    Type scrutinee_type = check(node->expr);
    
    // Check all clauses and ensure they have the same type
    optional<Type> result_type;
    for (auto* clause : node->clauses) {
        // TODO: Implement pattern type checking against scrutinee_type
        // For now, just check the body
        Type body_type = check(clause->body);
        
        if (!result_type) {
            result_type = body_type;
        } else {
            // TODO: Unify result types from different clauses
        }
    }
    
    // Return the unified type of all clauses
    return result_type.value_or(Type(compiler::types::Unit));
}

any TypeChecker::visit(CaseClause *node) const {
    // This method should not be called directly - CaseClause is handled within CaseExpr
    // But if it is called, just return a type variable
    auto var = context.fresh_type_var();
    auto type_name = make_shared<NamedType>();
    type_name->name = to_string(var->id);
    type_name->type = nullptr;
    return Type(type_name);
}

Type TypeChecker::infer_pattern_type(PatternNode* pattern, const Type& scrutinee_type) const {
    // TODO: Implement pattern type inference
    // This should bind variables in patterns and check pattern compatibility
    return scrutinee_type;
}

// Exception handling
any TypeChecker::visit(RaiseExpr *node) const {
    // Type check the symbol and message
    if (node->symbol) check(node->symbol);
    if (node->message) check(node->message);
    
    // Raise never returns normally
    auto var = context.fresh_type_var();
    auto type_name = make_shared<NamedType>();
    type_name->name = to_string(var->id);
    type_name->type = nullptr;
    return Type(type_name);
}

any TypeChecker::visit(TryCatchExpr *node) const {
    Type try_type = check(node->tryExpr);
    
    // TODO: Implement proper try-catch type checking
    // For now, just return the try type
    if (node->catchExpr) {
        check(node->catchExpr);
    }
    
    return try_type;
}

// Module system
any TypeChecker::visit(ImportExpr *node) const {
    // Process imports but don't change the type
    if (node->expr) {
        return check(node->expr);
    }
    return Type(compiler::types::Unit);
}

any TypeChecker::visit(ModuleExpr *node) const {
    // Type check all functions in module
    for (auto* func : node->functions) {
        check(func);
    }
    
    // Type check all records
    for (auto* record : node->records) {
        // Store record type info
        RecordTypeInfo info;
        info.name = record->recordType->value;
        
        for (const auto& [field_id, field_type_def] : record->identifiers) {
            info.field_names.push_back(field_id->name->value);
            // TODO: Convert TypeDefinition to Type
            info.field_types.push_back(Type(compiler::types::String)); // Placeholder
        }
        
        // Store in current module's record types
        string module_name = ""; // TODO: Get module name
        if (node->fqn->packageName.has_value()) {
            for (auto* name : node->fqn->packageName.value()->parts) {
                if (!module_name.empty()) module_name += "\\";
                module_name += name->value;
            }
        }
        module_name += "\\" + node->fqn->moduleName->value;
        
        module_records[module_name][info.name] = info;
    }
    
    return Type(compiler::types::Unit);
}

// With expressions
any TypeChecker::visit(WithExpr *node) const {
    // Type check context expression
    if (node->contextExpr) {
        check(node->contextExpr);
    }
    
    // Type check body
    return check(node->bodyExpr);
}

// Alias implementations
any TypeChecker::visit(ValueAlias *node) const {
    // Type check the expression being bound
    return check(node->expr);
}

any TypeChecker::visit(LambdaAlias *node) const {
    // Type check the lambda function
    return check(node->lambda);
}

any TypeChecker::visit(PatternAlias *node) const {
    // Type check the expression being matched
    Type expr_type = check(node->expr);
    // TODO: Type check pattern against expression type
    return expr_type;
}

// Default implementations that need to be overridden
any TypeChecker::visit(ExprNode *node) const {
    context.add_error(node->source_context,
                     "Unhandled expression type in type checker");
    return Type(nullptr);
}

} // namespace yona::typechecker