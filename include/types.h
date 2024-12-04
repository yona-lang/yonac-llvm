//
// Created by akovari on 4.12.24.
//

#pragma once

#include <string>

namespace yona::compiler
{
    struct Type;

    using namespace std;

    enum ValType
    {
        Int,
        Float,
        Bool,
        String,
        Char,
        Unit,
        List,
        Tuple,
        Set,
        Dict,
        FQN
    };

    using TypeVar = Type<ValType>;
    using TypeFunc = Type<pair<Type, Type>>;
    using TypeRecord = Type<string>;
    using TypeModule = Type<string>;

    using VariantType = variant<TypeVar, TypeFunc, TypeRecord, TypeModule>;

    struct Type
    {
        enum Kind
        {
            Val,
            Func,
            Record,
            Module
        } kind;
        ValType value;

        Type unify_with(const Type& type);
    };
}
