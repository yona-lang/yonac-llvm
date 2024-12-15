//
// Created by akovari on 4.12.24.
//

#pragma once

#include <antlr4-runtime.h>
#include <ostream>
#include <string>

#include "colors.h"

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

    struct SingleItemCollectionType final
    {
        enum CollectionKind
        {
            Set,
            Seq
        } kind;
        Type valueType;
    };

    struct DictCollectionType final
    {
        Type keyType;
        Type valueType;
    };

    struct TupleType final
    {
        vector<Type> fieldTypes;
        explicit TupleType(const vector<Type>& field_types) : fieldTypes(field_types) {}
    };

    struct FunctionType final
    {
        Type returnType;
        Type argumentType;
    };

    struct SumType final
    {
        unordered_set<Type> types;
    };

    struct TokenLocation final
    {
        unsigned int start_line;
        unsigned int start_col;
        unsigned int stop_line;
        unsigned int stop_col;
        string text;

        TokenLocation(Token token) :
            start_line(token.getStart()->getLine()), start_col(token.getStart()->getCharPositionInLine()),
            stop_line(token.getStop()->getLine()), stop_col(token.getStop()->getCharPositionInLine()),
            text(token.getStart()->getText())
        {
        }
    };

    inline std::ostream& operator<<(std::ostream& os, const TokenLocation& rhs)
    {
        os << "[" << rhs.start_line << ":" << rhs.start_col << "-" << rhs.stop_line << ":" << rhs.stop_col << "] "
           << rhs.text;
        return os;
    }

    struct TypeError final
    {
        TokenLocation source_token;
        string message;

        explicit TypeError(Token token, string message) : source_token(token), message(std::move(message)) {}
    };

    inline std::ostream& operator<<(std::ostream& os, const TypeError& rhs)
    {
        os << ANSI_COLOR_RED << "Type error at " << rhs.source_token << ANSI_COLOR_RESET << ": " << rhs.message;
        return os;
    }

    class TypeInferenceContext final
    {
    private:
        vector<TypeError> errors;

    public:
        void addError(const TypeError& error) { errors.push_back(error); }
        [[nodiscard]] const vector<TypeError>& getErrors() const { return errors; }
    };
}
