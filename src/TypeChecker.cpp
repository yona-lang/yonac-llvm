//
// Created by akovari on 6.1.25.
//

#include <sstream>
#include <algorithm>

#include "TypeChecker.h"
#include "ast_visitor_impl.h"

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

    return node->accept(*this);
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
    // Instantiate a polymorphic type by replacing type variables with fresh type variables

    // If it's a Var type, create a fresh type variable
    if (holds_alternative<BuiltinType>(type)) {
        auto builtin = get<BuiltinType>(type);
        if (builtin == Var) {
            // Create a fresh type variable for instantiation
            auto fresh_var = context.fresh_type_var();
            // For now, return Var type (in a real implementation, we'd track the mapping)
            return Type(Var);
        }
    }

    // Handle function types recursively
    if (holds_alternative<shared_ptr<FunctionType>>(type)) {
        auto func = get<shared_ptr<FunctionType>>(type);
        auto new_func = make_shared<FunctionType>();
        new_func->argumentType = instantiate(func->argumentType);
        new_func->returnType = instantiate(func->returnType);
        return Type(new_func);
    }

    // Handle collection types recursively
    if (holds_alternative<shared_ptr<SingleItemCollectionType>>(type)) {
        auto coll = get<shared_ptr<SingleItemCollectionType>>(type);
        auto new_coll = make_shared<SingleItemCollectionType>();
        new_coll->kind = coll->kind;
        new_coll->valueType = instantiate(coll->valueType);
        return Type(new_coll);
    }

    if (holds_alternative<shared_ptr<DictCollectionType>>(type)) {
        auto dict = get<shared_ptr<DictCollectionType>>(type);
        auto new_dict = make_shared<DictCollectionType>();
        new_dict->keyType = instantiate(dict->keyType);
        new_dict->valueType = instantiate(dict->valueType);
        return Type(new_dict);
    }

    // Handle product types (tuples)
    if (holds_alternative<shared_ptr<ProductType>>(type)) {
        auto prod = get<shared_ptr<ProductType>>(type);
        auto new_prod = make_shared<ProductType>();
        for (const auto& t : prod->types) {
            new_prod->types.push_back(instantiate(t));
        }
        return Type(new_prod);
    }

    // Handle sum types (unions)
    if (holds_alternative<shared_ptr<SumType>>(type)) {
        auto sum = get<shared_ptr<SumType>>(type);
        auto new_sum = make_shared<SumType>();
        for (const auto& t : sum->types) {
            new_sum->types.insert(instantiate(t));
        }
        return Type(new_sum);
    }

    // Handle named types
    if (holds_alternative<shared_ptr<NamedType>>(type)) {
        auto named = get<shared_ptr<NamedType>>(type);
        auto new_named = make_shared<NamedType>();
        new_named->name = named->name;
        new_named->type = instantiate(named->type);
        return Type(new_named);
    }

    // For non-polymorphic types, return as-is
    return type;
}

Type TypeChecker::generalize(const Type& type, const TypeEnvironment& env) const {
    // Generalize a type by converting free type variables to polymorphic type variables
    // Free type variables are those not bound in the environment

    // For now, we implement a simple generalization that marks unconstrained types as Var
    // In a full implementation, we would track which type variables are free

    // Handle function types recursively
    if (holds_alternative<shared_ptr<FunctionType>>(type)) {
        auto func = get<shared_ptr<FunctionType>>(type);
        auto new_func = make_shared<FunctionType>();
        new_func->argumentType = generalize(func->argumentType, env);
        new_func->returnType = generalize(func->returnType, env);
        return Type(new_func);
    }

    // Handle collection types recursively
    if (holds_alternative<shared_ptr<SingleItemCollectionType>>(type)) {
        auto coll = get<shared_ptr<SingleItemCollectionType>>(type);
        auto new_coll = make_shared<SingleItemCollectionType>();
        new_coll->kind = coll->kind;
        new_coll->valueType = generalize(coll->valueType, env);
        return Type(new_coll);
    }

    if (holds_alternative<shared_ptr<DictCollectionType>>(type)) {
        auto dict = get<shared_ptr<DictCollectionType>>(type);
        auto new_dict = make_shared<DictCollectionType>();
        new_dict->keyType = generalize(dict->keyType, env);
        new_dict->valueType = generalize(dict->valueType, env);
        return Type(new_dict);
    }

    // Handle product types (tuples)
    if (holds_alternative<shared_ptr<ProductType>>(type)) {
        auto prod = get<shared_ptr<ProductType>>(type);
        auto new_prod = make_shared<ProductType>();
        for (const auto& t : prod->types) {
            new_prod->types.push_back(generalize(t, env));
        }
        return Type(new_prod);
    }

    // Handle sum types (unions)
    if (holds_alternative<shared_ptr<SumType>>(type)) {
        auto sum = get<shared_ptr<SumType>>(type);
        auto new_sum = make_shared<SumType>();
        for (const auto& t : sum->types) {
            new_sum->types.insert(generalize(t, env));
        }
        return Type(new_sum);
    }

    // Handle named types
    if (holds_alternative<shared_ptr<NamedType>>(type)) {
        auto named = get<shared_ptr<NamedType>>(type);
        auto new_named = make_shared<NamedType>();
        new_named->name = named->name;
        new_named->type = generalize(named->type, env);
        return Type(new_named);
    }

    // Return the type as-is for concrete types
    return type;
}

bool TypeChecker::check_pattern_type(PatternNode* pattern, const Type& type, shared_ptr<TypeEnvironment> env) const {
    // Check if a pattern is compatible with a type and extract bindings

    if (auto pattern_value = dynamic_cast<PatternValue*>(pattern)) {
        // PatternValue contains an expression variant
        if (holds_alternative<IdentifierExpr*>(pattern_value->expr)) {
            auto id_expr = get<IdentifierExpr*>(pattern_value->expr);
            if (auto name_expr = dynamic_cast<NameExpr*>(id_expr->name)) {
                // Variable pattern - always matches, bind the variable
                env->bind(name_expr->value, type);
                return true;
            }
        }
        // Literal pattern - for now assume it matches
        return true;
    } else if (auto underscore = dynamic_cast<UnderscoreNode*>(pattern)) {
        // Wildcard always matches
        return true;
    } else if (auto tuple_pattern = dynamic_cast<TuplePattern*>(pattern)) {
        // Tuple pattern - check structure matches
        if (holds_alternative<shared_ptr<ProductType>>(type)) {
            auto prod_type = get<shared_ptr<ProductType>>(type);
            if (tuple_pattern->patterns.size() != prod_type->types.size()) {
                return false;  // Arity mismatch
            }
            for (size_t i = 0; i < tuple_pattern->patterns.size(); ++i) {
                if (!check_pattern_type(tuple_pattern->patterns[i], prod_type->types[i], env)) {
                    return false;
                }
            }
            return true;
        }
        return false;  // Not a tuple type
    } else if (auto seq_pattern = dynamic_cast<SeqPattern*>(pattern)) {
        // Sequence pattern
        if (holds_alternative<shared_ptr<SingleItemCollectionType>>(type)) {
            auto seq_type = get<shared_ptr<SingleItemCollectionType>>(type);
            for (auto elem_pattern : seq_pattern->patterns) {
                if (!check_pattern_type(elem_pattern, seq_type->valueType, env)) {
                    return false;
                }
            }
            return true;
        }
        return false;
    } else if (auto or_pattern = dynamic_cast<OrPattern*>(pattern)) {
        // OR pattern - at least one alternative must match
        for (const auto& alt_pattern : or_pattern->patterns) {
            auto temp_env = make_shared<TypeEnvironment>();
            if (check_pattern_type(alt_pattern.get(), type, temp_env)) {
                // Copy bindings from successful match
                for (const auto& binding : temp_env->bindings) {
                    env->bind(binding.first, binding.second);
                }
                return true;
            }
        }
        return false;
    } else if (auto as_pattern = dynamic_cast<AsDataStructurePattern*>(pattern)) {
        // As pattern - check inner pattern and bind name
        if (check_pattern_type(as_pattern->pattern, type, env)) {
            if (auto name_expr = dynamic_cast<NameExpr*>(as_pattern->identifier->name)) {
                env->bind(name_expr->value, type);
            }
            return true;
        }
        return false;
    }

    // For other complex patterns, extract bindings and assume match
    extract_pattern_bindings(pattern, type, env);
    return true;
}

void TypeChecker::extract_pattern_bindings(PatternNode* pattern, const Type& type, shared_ptr<TypeEnvironment> env) const {
    // Extract variable bindings from a pattern and add them to the environment

    if (auto pattern_value = dynamic_cast<PatternValue*>(pattern)) {
        // PatternValue contains an expression variant
        if (holds_alternative<IdentifierExpr*>(pattern_value->expr)) {
            auto id_expr = get<IdentifierExpr*>(pattern_value->expr);
            if (auto name_expr = dynamic_cast<NameExpr*>(id_expr->name)) {
                // Simple variable binding
                env->bind(name_expr->value, type);
            }
        }
    } else if (auto tuple_pattern = dynamic_cast<TuplePattern*>(pattern)) {
        // Tuple pattern - extract bindings for each element
        if (holds_alternative<shared_ptr<ProductType>>(type)) {
            auto prod_type = get<shared_ptr<ProductType>>(type);
            size_t min_size = min(tuple_pattern->patterns.size(), prod_type->types.size());
            for (size_t i = 0; i < min_size; ++i) {
                extract_pattern_bindings(tuple_pattern->patterns[i], prod_type->types[i], env);
            }
        }
    } else if (auto seq_pattern = dynamic_cast<SeqPattern*>(pattern)) {
        // Sequence pattern - extract bindings for each element
        Type element_type = Type(Var);  // Default to polymorphic type
        if (holds_alternative<shared_ptr<SingleItemCollectionType>>(type)) {
            auto seq_type = get<shared_ptr<SingleItemCollectionType>>(type);
            element_type = seq_type->valueType;
        }
        for (auto elem_pattern : seq_pattern->patterns) {
            extract_pattern_bindings(elem_pattern, element_type, env);
        }
    } else if (auto dict_pattern = dynamic_cast<DictPattern*>(pattern)) {
        // Dict pattern - extract bindings for values
        Type value_type = Type(Var);  // Default to polymorphic type
        if (holds_alternative<shared_ptr<DictCollectionType>>(type)) {
            auto dict_type = get<shared_ptr<DictCollectionType>>(type);
            value_type = dict_type->valueType;
        }
        // DictPattern has keyValuePairs member
        for (const auto& [key_pattern, value_pattern] : dict_pattern->keyValuePairs) {
            // Only extract bindings from the value pattern
            extract_pattern_bindings(value_pattern, value_type, env);
        }
    } else if (auto record_pattern = dynamic_cast<RecordPattern*>(pattern)) {
        // Record pattern - extract bindings for each field
        // For now, treat all fields as polymorphic
        // RecordPattern has items member
        for (const auto& [field_name, field_pattern] : record_pattern->items) {
            extract_pattern_bindings(field_pattern, Type(Var), env);
        }
    } else if (auto head_tails = dynamic_cast<HeadTailsPattern*>(pattern)) {
        // Head|Tails pattern
        Type element_type = Type(Var);
        if (holds_alternative<shared_ptr<SingleItemCollectionType>>(type)) {
            auto seq_type = get<shared_ptr<SingleItemCollectionType>>(type);
            element_type = seq_type->valueType;
        }
        // HeadTailsPattern has heads and tail members
        for (auto head : head_tails->heads) {
            extract_pattern_bindings(head, element_type, env);
        }
        extract_pattern_bindings(head_tails->tail, type, env);  // Tail has same type as whole list
    } else if (auto or_pattern = dynamic_cast<OrPattern*>(pattern)) {
        // OR pattern - all alternatives must bind the same variables with same types
        // For simplicity, extract bindings from the first alternative
        if (!or_pattern->patterns.empty()) {
            extract_pattern_bindings(or_pattern->patterns[0].get(), type, env);
        }
    } else if (auto as_pattern = dynamic_cast<AsDataStructurePattern*>(pattern)) {
        // As pattern - bind both the name and the inner pattern
        if (auto name_expr = dynamic_cast<NameExpr*>(as_pattern->identifier->name)) {
            env->bind(name_expr->value, type);
        }
        extract_pattern_bindings(as_pattern->pattern, type, env);
    }
    // Other pattern types (literals, underscore) don't bind variables
}

// Visitor implementations for literals
Type TypeChecker::visit(IntegerExpr *node) const {
    return Type(compiler::types::SignedInt64);
}

Type TypeChecker::visit(FloatExpr *node) const {
    return Type(compiler::types::Float64);
}

Type TypeChecker::visit(ByteExpr *node) const {
    return Type(compiler::types::Byte);
}

Type TypeChecker::visit(CharacterExpr *node) const {
    return Type(compiler::types::Char);
}

Type TypeChecker::visit(StringExpr *node) const {
    return Type(compiler::types::String);
}

Type TypeChecker::visit(TrueLiteralExpr *node) const {
    return Type(compiler::types::Bool);
}

Type TypeChecker::visit(FalseLiteralExpr *node) const {
    return Type(compiler::types::Bool);
}

Type TypeChecker::visit(UnitExpr *node) const {
    return Type(compiler::types::Unit);
}

Type TypeChecker::visit(SymbolExpr *node) const {
    return Type(compiler::types::Symbol);
}

// Visitor for identifiers
Type TypeChecker::visit(IdentifierExpr *node) const {
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
Type TypeChecker::visit(TupleExpr *node) const {
    auto product = make_shared<ProductType>();

    for (auto* expr : node->values) {
        Type elem_type = check(expr);
        product->types.push_back(elem_type);
    }

    return Type(product);
}

Type TypeChecker::visit(ValuesSequenceExpr *node) const {
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

Type TypeChecker::visit(SetExpr *node) const {
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

Type TypeChecker::visit(DictExpr *node) const {
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
Type TypeChecker::visit(AddExpr *node) const {
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

Type TypeChecker::visit(SubtractExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: - operator requires numeric types");
        return Type(nullptr);
    }

    return derive_bin_op_result_type(left_type, right_type);
}

Type TypeChecker::visit(MultiplyExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: * operator requires numeric types");
        return Type(nullptr);
    }

    return derive_bin_op_result_type(left_type, right_type);
}

Type TypeChecker::visit(DivideExpr *node) const {
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

Type TypeChecker::visit(ModuloExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    if (!is_integer(left_type) || !is_integer(right_type)) {
        context.add_error(node->source_context,
                         "Type error: % operator requires integer types");
        return Type(nullptr);
    }

    return derive_bin_op_result_type(left_type, right_type);
}

Type TypeChecker::visit(PowerExpr *node) const {
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
Type TypeChecker::visit(EqExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    auto unif_result = unify(left_type, right_type);
    if (!unif_result.success) {
        context.add_error(node->source_context,
                         "Type error: == operator requires compatible types");
    }

    return Type(compiler::types::Bool);
}

Type TypeChecker::visit(NeqExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    auto unif_result = unify(left_type, right_type);
    if (!unif_result.success) {
        context.add_error(node->source_context,
                         "Type error: != operator requires compatible types");
    }

    return Type(compiler::types::Bool);
}

Type TypeChecker::visit(LtExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: < operator requires numeric types");
    }

    return Type(compiler::types::Bool);
}

Type TypeChecker::visit(GtExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: > operator requires numeric types");
    }

    return Type(compiler::types::Bool);
}

Type TypeChecker::visit(LteExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: <= operator requires numeric types");
    }

    return Type(compiler::types::Bool);
}

Type TypeChecker::visit(GteExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    if (!is_numeric(left_type) || !is_numeric(right_type)) {
        context.add_error(node->source_context,
                         "Type error: >= operator requires numeric types");
    }

    return Type(compiler::types::Bool);
}

// Logical operators
Type TypeChecker::visit(LogicalAndExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    if (!holds_alternative<BuiltinType>(left_type) || get<BuiltinType>(left_type) != compiler::types::Bool ||
        !holds_alternative<BuiltinType>(right_type) || get<BuiltinType>(right_type) != compiler::types::Bool) {
        context.add_error(node->source_context,
                         "Type error: && operator requires boolean types");
    }

    return Type(compiler::types::Bool);
}

Type TypeChecker::visit(LogicalOrExpr *node) const {
    Type left_type = check(node->left);
    Type right_type = check(node->right);

    if (!holds_alternative<BuiltinType>(left_type) || get<BuiltinType>(left_type) != compiler::types::Bool ||
        !holds_alternative<BuiltinType>(right_type) || get<BuiltinType>(right_type) != compiler::types::Bool) {
        context.add_error(node->source_context,
                         "Type error: || operator requires boolean types");
    }

    return Type(compiler::types::Bool);
}

Type TypeChecker::visit(LogicalNotOpExpr *node) const {
    Type operand_type = check(node->expr);

    if (!holds_alternative<BuiltinType>(operand_type) || get<BuiltinType>(operand_type) != compiler::types::Bool) {
        context.add_error(node->source_context,
                         "Type error: ! operator requires boolean type");
    }

    return Type(compiler::types::Bool);
}

// Control flow
Type TypeChecker::visit(IfExpr *node) const {
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

Type TypeChecker::visit(LetExpr *node) const {
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
            // Extract bindings from pattern and add to environment
            extract_pattern_bindings(pattern_alias->pattern, expr_type, new_env);
        } else if (auto module_alias = dynamic_cast<ModuleAlias*>(alias)) {
            // Module aliases bind a module to a name
            string name = module_alias->name->value;
            // For now, treat modules as an opaque type
            new_env->bind(name, Type(nullptr));
        } else if (auto fqn_alias = dynamic_cast<FqnAlias*>(alias)) {
            // FQN aliases bind a fully qualified name
            string name = fqn_alias->name->value;
            // For now, treat FQNs as an opaque type
            new_env->bind(name, Type(nullptr));
        } else if (auto function_alias = dynamic_cast<FunctionAlias*>(alias)) {
            // Function aliases bind an existing function
            string name = function_alias->name->value;
            string alias_name = function_alias->alias->value;
            auto alias_type = env->lookup(alias_name);
            if (alias_type.has_value()) {
                new_env->bind(name, alias_type.value());
            }
        }
    }

    // Type check body in new environment
    auto old_env = env;
    env = new_env;
    Type body_type = check(node->expr);
    env = old_env;

    return body_type;
}

Type TypeChecker::visit(DoExpr *node) const {
    Type result_type = Type(compiler::types::Unit);

    for (auto* expr : node->steps) {
        result_type = check(expr);
    }

    return result_type;
}

// Function-related
Type TypeChecker::visit(FunctionExpr *node) const {
    // Implement function type inference
    // Infer the type of a function from its patterns and bodies

    // Create fresh type variables for arguments
    vector<Type> arg_types;
    for (auto pattern : node->patterns) {
        // Create a fresh type variable for each argument
        arg_types.push_back(Type(Var));
    }

    // Create a new environment for the function body
    auto func_env = env->extend();

    // Bind patterns to argument types
    for (size_t i = 0; i < node->patterns.size(); ++i) {
        extract_pattern_bindings(node->patterns[i], arg_types[i], func_env);
    }

    // Type check all function bodies and unify their types
    Type result_type = Type(nullptr);
    auto old_env = env;
    env = func_env;

    for (auto body : node->bodies) {
        Type body_type = check(body);

        if (holds_alternative<nullptr_t>(result_type)) {
            // First body - use its type as the result type
            result_type = body_type;
        } else {
            // Unify with previous result type
            auto unify_result = unify(result_type, body_type);
            if (!unify_result.success) {
                context.add_error(node->source_context,
                    "Function bodies have inconsistent types: " +
                    unify_result.error_message.value_or("type mismatch"));
            }
        }
    }

    env = old_env;

    // Build the function type from arguments and result
    Type func_type = result_type;
    for (auto it = arg_types.rbegin(); it != arg_types.rend(); ++it) {
        auto fn_type = make_shared<FunctionType>();
        fn_type->argumentType = *it;
        fn_type->returnType = func_type;
        func_type = Type(fn_type);
    }

    return func_type;
}

Type TypeChecker::visit(ApplyExpr *node) const {
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

Type TypeChecker::visit(RecordInstanceExpr *node) const {
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
    // Create a record type with full field information
    auto record_type = make_shared<RecordType>();
    record_type->name = record_name;

    // Store field types from the record definition
    if (module_records.count(current_module_name) > 0 &&
        module_records[current_module_name].count(record_name) > 0) {
        auto& record_info = module_records[current_module_name][record_name];
        // Convert parallel vectors to map
        for (size_t i = 0; i < record_info.field_names.size() && i < record_info.field_types.size(); ++i) {
            record_type->field_types[record_info.field_names[i]] = record_info.field_types[i];
        }
    }

    return Type(record_type);
}

// Pattern matching
Type TypeChecker::visit(CaseExpr *node) const {
    Type scrutinee_type = check(node->expr);

    // Check all clauses and ensure they have the same type
    optional<Type> result_type;
    for (auto* clause : node->clauses) {
        // Create a new environment for this clause
        auto clause_env = env->extend();

        // Check pattern compatibility with scrutinee type and extract bindings
        if (!check_pattern_type(clause->pattern, scrutinee_type, clause_env)) {
            context.add_error(clause->source_context,
                "Pattern type incompatible with case expression type");
        }

        // Type check the body in the new environment
        auto old_env = env;
        env = clause_env;
        Type body_type = check(clause->body);
        env = old_env;

        if (!result_type) {
            result_type = body_type;
        } else {
            // Unify result types from different clauses
            auto unify_result = unify(result_type.value(), body_type);
            if (!unify_result.success) {
                context.add_error(clause->source_context,
                    "Case clause results have inconsistent types: " +
                    unify_result.error_message.value_or("type mismatch"));
            } else {
                result_type = unify_result.substitution.apply(result_type.value());
            }
        }
    }

    // Return the unified type of all clauses
    return result_type.value_or(Type(compiler::types::Unit));
}

Type TypeChecker::visit(CaseClause *node) const {
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
Type TypeChecker::visit(RaiseExpr *node) const {
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

Type TypeChecker::visit(TryCatchExpr *node) const {
    Type try_type = check(node->tryExpr);

    // Implement proper try-catch type checking
    // The result type should be the unification of try and catch types
    if (node->catchExpr) {
        // Type check catch clauses
        Type catch_type = check(node->catchExpr);

        // Unify try and catch types
        auto unify_result = unify(try_type, catch_type);
        if (!unify_result.success) {
            context.add_error(node->source_context,
                "Try and catch blocks have incompatible types: " +
                unify_result.error_message.value_or("type mismatch"));
            return try_type;  // Return try type on error
        }

        // Return the unified type
        return unify_result.substitution.apply(try_type);
    }

    return try_type;
}

// Module system
Type TypeChecker::visit(ImportExpr *node) const {
    // Process imports but don't change the type
    if (node->expr) {
        return check(node->expr);
    }
    return Type(compiler::types::Unit);
}

// Helper function to convert TypeNameNode to Type
Type TypeChecker::type_node_to_type(TypeNameNode* type_node) const {
    if (auto builtin = dynamic_cast<BuiltinTypeNode*>(type_node)) {
        return Type(builtin->type);
    } else if (auto user_defined = dynamic_cast<UserDefinedTypeNode*>(type_node)) {
        // Create a named type
        auto named_type = make_shared<NamedType>();
        named_type->name = user_defined->name->value;
        named_type->type = nullptr; // Will be resolved during type checking
        return Type(named_type);
    }

    // Unknown type node
    return Type(nullptr);
}

Type TypeChecker::visit(RecordNode *node) const {
    // Type check record definition
    string record_name = node->recordType->value;

    // Check each field has a valid type
    for (const auto& [field_id, field_type_def] : node->identifiers) {
        if (!field_type_def || !field_type_def->name) {
            context.add_error(node->source_context,
                            "Field '" + field_id->name->value +
                            "' in record '" + record_name + "' must have a type annotation");
        }
    }

    // Record definitions don't have a value type themselves
    return Type(compiler::types::Unit);
}

Type TypeChecker::visit(ModuleExpr *node) const {
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

            // Convert TypeDefinition to Type
            if (field_type_def && field_type_def->name) {
                Type field_type = type_node_to_type(field_type_def->name);
                info.field_types.push_back(field_type);
            } else {
                // No type specified, use Any type or report error
                context.add_error(record->source_context,
                                "Missing type annotation for field '" + field_id->name->value +
                                "' in record '" + record->recordType->value + "'");
                info.field_types.push_back(Type(nullptr));
            }
        }

        // Store in current module's record types
        string module_name = "";
        if (node->fqn->packageName.has_value()) {
            for (auto* name : node->fqn->packageName.value()->parts) {
                if (!module_name.empty()) module_name += "\\";
                module_name += name->value;
            }
            if (!module_name.empty()) module_name += "\\";
        }
        module_name += node->fqn->moduleName->value;

        // Update current module name for context
        current_module_name = module_name;

        module_records[module_name][info.name] = info;
    }

    return Type(compiler::types::Unit);
}

// With expressions
Type TypeChecker::visit(WithExpr *node) const {
    // Type check context expression
    if (node->contextExpr) {
        check(node->contextExpr);
    }

    // Type check body
    return check(node->bodyExpr);
}

// Alias implementations
Type TypeChecker::visit(ValueAlias *node) const {
    // Type check the expression being bound
    return check(node->expr);
}

Type TypeChecker::visit(LambdaAlias *node) const {
    // Type check the lambda function
    return check(node->lambda);
}

Type TypeChecker::visit(PatternAlias *node) const {
    // Type check the expression being matched
    Type expr_type = check(node->expr);
    // TODO: Type check pattern against expression type
    return expr_type;
}

// Default implementations that need to be overridden
Type TypeChecker::visit(ExprNode *node) const {
    context.add_error(node->source_context,
                     "Unhandled expression type in type checker");
    return Type(nullptr);
}

} // namespace yona::typechecker
