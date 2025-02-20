//
// Created by Adam Kovari on 23.12.2024.
//

#pragma once

#include <codecvt>
#include <functional>
#include <locale>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace yona::interp::runtime {
using namespace std;

struct SetValue;
struct SeqValue;
struct DictValue;
struct TupleValue;
struct FunctionValue;
struct ModuleValue;
struct SymbolValue;
struct ApplyValue;
struct RuntimeObject;
struct FqnValue;

inline wstring_convert<codecvt_utf8<wchar_t>> STRING_CONVERTER;

using RuntimeObjectData =
    variant<int /*Int*/, double /*Float*/, byte /*Byte*/, wchar_t /*Char*/, string /*String*/, bool /*Bool*/, nullptr_t /*Unit*/,
            shared_ptr<SymbolValue> /*Symbol*/, shared_ptr<TupleValue> /*Tuple/Record*/, shared_ptr<DictValue> /*Dict*/, shared_ptr<SeqValue> /*Seq*/,
            shared_ptr<SetValue> /*Set*/, shared_ptr<FqnValue> /*FQN*/, shared_ptr<ModuleValue> /*Module*/, shared_ptr<FunctionValue> /*Function*/,
            shared_ptr<ApplyValue> /*Apply*/>;

using RuntimeObjectPtr = shared_ptr<RuntimeObject>;

enum RuntimeObjectType { Int, Float, Byte, Char, String, Bool, Unit, Symbol, Dict, Seq, Set, Tuple, FQN, Module, Function };

inline string RuntimeObjectTypes[] = {"Int",  "Float", "Byte", "Char",  "String", "Bool",   "Unit",    "Symbol",
                                      "Dict", "Seq",   "Set",  "Tuple", "FQN",    "Module", "Function"};

struct SymbolValue {
  string name;
};

struct DictValue {
  vector<pair<RuntimeObjectPtr, RuntimeObjectPtr>> fields{};
};

struct SeqValue {
  vector<RuntimeObjectPtr> fields{};
};

struct SetValue {
  vector<RuntimeObjectPtr> fields{};
};

struct ApplyValue {
  RuntimeObjectPtr func;
  RuntimeObjectPtr arg;
};

struct TupleValue {
  vector<RuntimeObjectPtr> fields{};
};

struct FqnValue {
  vector<string> parts;
};

struct FunctionValue {
  shared_ptr<FqnValue> fqn;
  function<RuntimeObjectPtr(const vector<RuntimeObjectPtr> &)> code; // nullptr - nomatch
};

struct ModuleValue {
  shared_ptr<FqnValue> fqn;
  vector<shared_ptr<FunctionValue>> functions;
  vector<shared_ptr<TupleValue>> records;
};

struct RuntimeObject {
  RuntimeObjectType type;
  RuntimeObjectData data;

  RuntimeObject(RuntimeObjectType t, RuntimeObjectData d) : type(t), data(std::move(d)) {}

  template <typename T> T &get() { return std::get<T>(data); }

  template <typename T> const T &get() const { return std::get<T>(data); }

  friend bool operator==(const RuntimeObject &lhs, const RuntimeObject &rhs) { return lhs.type == rhs.type && lhs.data == rhs.data; }
  friend bool operator!=(const RuntimeObject &lhs, const RuntimeObject &rhs) { return !(lhs == rhs); }
};

std::ostream &operator<<(std::ostream &strm, const RuntimeObject &obj);

} // namespace yona::interp::runtime
