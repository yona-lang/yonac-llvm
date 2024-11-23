//
// Created by akovari on 22.11.24.
//

#pragma once

#include <string>
#include <unordered_map>

#include "Interpreter.h"

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
        SymbolTable() = default;
    };

} // yona::interp
