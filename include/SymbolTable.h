//
// Created by akovari on 22.11.24.
//

#pragma once

#include <any>
#include <string>
#include <unordered_map>
#include "interpretation.h"

namespace yona::interp
{
    using namespace std;

    using symbol_ref = result_t;

    struct Symbol
    {
    private:
        string name;
        symbol_ref reference;
    };

    struct SymbolTable
    {
    private:
        unordered_map<string, Symbol> symbols;

    public:
        virtual ~SymbolTable() = default;
    };

} // yona::interp
