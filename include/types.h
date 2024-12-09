//
// Created by akovari on 4.12.24.
//

#pragma once

#include <antlr4-runtime.h>
#include <string>

#include "types.h"

using Token = const antlr4::ParserRuleContext&;

namespace yona::compiler::types
{
    using namespace std;

    struct SingleItemCollectionType;
    struct DictCollectionType;
    struct DictCollectionType;
    struct TupleType;
    struct FunctionType;
    struct SumType;

    enum ValueType
    {
        Int,
        Float,
        Byte,
        Char,
        String,
        Bool,
        Unit,
        Symbol,
        Module
    };

    // TODO implement comparators
    using Type = variant<ValueType, shared_ptr<SingleItemCollectionType>, shared_ptr<DictCollectionType>,
                         shared_ptr<FunctionType>, shared_ptr<TupleType>, shared_ptr<SumType>, nullptr_t>;

    struct SingleItemCollectionType
    {
        enum CollectionKind
        {
            Set,
            Seq
        } kind;
        Type valueType;
    };

    struct DictCollectionType
    {
        Type keyType;
        Type valueType;
    };

    struct TupleType
    {
        vector<Type> fieldTypes;
        explicit TupleType(const vector<Type>& field_types) : fieldTypes(field_types) {}
    };

    struct FunctionType
    {
        Type returnType;
        Type argumentType;
    };

    struct SumType
    {
        unordered_set<Type> types;
    };

    struct TypeError
    {
        Token token;
        string message;

        explicit TypeError(Token token, string message) : token(token), message(std::move(message)) {}
    };

    class TypeInferenceContext final
    {
    private:
        vector<TypeError> errors;

    public:
        void addError(const TypeError& error) { errors.push_back(error); }
        [[nodiscard]] const vector<TypeError>& getErrors() const { return errors; }
    };
}
