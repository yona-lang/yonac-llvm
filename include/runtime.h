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

enum RuntimeObjectType { Int, Float, Byte, Char, String, Bool, Unit, Symbol, Dict, Seq, Set, Tuple, FQN, Module, Function };

inline string RuntimeObjectTypes[] = {"Int",  "Float", "Byte", "Char",  "String", "Bool",   "Unit",    "Symbol",
                                      "Dict", "Seq",   "Set",  "Tuple", "FQN",    "Module", "Function"};

struct SymbolValue {
  string name;
};

struct DictValue {
  vector<pair<shared_ptr<RuntimeObject>, shared_ptr<RuntimeObject>>> fields{};
};

struct SeqValue {
  vector<shared_ptr<RuntimeObject>> fields{};
};

struct SetValue {
  vector<shared_ptr<RuntimeObject>> fields{};
};

struct ApplyValue {
  shared_ptr<RuntimeObject> func;
  shared_ptr<RuntimeObject> arg;
};

struct TupleValue {
  vector<shared_ptr<RuntimeObject>> fields{};
};

struct FqnValue {
  vector<string> parts;
};

struct FunctionValue {
  shared_ptr<FqnValue> fqn;
  function<shared_ptr<RuntimeObject>(shared_ptr<RuntimeObject>)> code;
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
};

std::ostream &operator<<(std::ostream &strm, const RuntimeObject &obj);

} // namespace yona::interp::runtime
