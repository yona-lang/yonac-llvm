//
// Created by akovari on 4.12.24.
//

#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace yona::compiler::types {
using namespace std;

struct SingleItemCollectionType;
struct DictCollectionType;
struct DictCollectionType;
struct NamedType;
struct FunctionType;
struct SumType;
struct ProductType;
struct UnionType;

enum BuiltinType {
  Bool,
  Byte,
  SignedInt16,
  SignedInt32,
  SignedInt64,
  SignedInt128,
  UnsignedInt16,
  UnsignedInt32,
  UnsignedInt64,
  UnsignedInt128,
  Float32,
  Float64,
  Float128,
  Char,
  String,
  Symbol,
  Dict,
  Set,
  Seq,
  Var,
  Unit
};

static const std::string BuiltinTypeStrings[] = {"Bool",          "Byte",          "SignedInt16",   "SignedInt32",    "SignedInt64", "SignedInt128",
                                                 "UnsignedInt16", "UnsignedInt32", "UnsignedInt64", "UnsignedInt128", "Float32",     "Float64",
                                                 "Float128",      "Char",          "String",        "Symbol",         "Dict",        "Set",
                                                 "Seq",           "Var",           "Unit"};

using Type = variant<BuiltinType, shared_ptr<SingleItemCollectionType>, shared_ptr<DictCollectionType>, shared_ptr<FunctionType>,
                     shared_ptr<NamedType>, shared_ptr<SumType>, shared_ptr<ProductType>, nullptr_t>;

struct SingleItemCollectionType final {
  enum CollectionKind { Set, Seq } kind;
  Type valueType;
};

struct DictCollectionType final {
  Type keyType;
  Type valueType;
};

struct FunctionType final {
  Type returnType;
  Type argumentType;
};

struct SumType final {
  unordered_set<Type> types;
};

struct ProductType final {
  vector<Type> types;
};

struct NamedType final {
  string name;
  Type type;
};

inline bool is_signed(const Type &type) {
  if (holds_alternative<BuiltinType>(type)) {
    const auto btype = get<BuiltinType>(type);
    return btype == SignedInt16 || btype == SignedInt32 || btype == SignedInt64 || btype == SignedInt128;
  }
  return false;
}

inline bool is_unsigned(const Type &type) {
  if (holds_alternative<BuiltinType>(type)) {
    const auto btype = get<BuiltinType>(type);
    return btype == UnsignedInt16 || btype == UnsignedInt32 || btype == UnsignedInt64 || btype == UnsignedInt128;
  }
  return false;
}

inline bool is_float(const Type &type) {
  if (holds_alternative<BuiltinType>(type)) {
    const auto btype = get<BuiltinType>(type);
    return btype == Float32 || btype == Float64 || btype == Float128;
  }
  return false;
}

inline bool is_integer(const Type &type) {
  if (holds_alternative<BuiltinType>(type)) {
    const auto btype = get<BuiltinType>(type);
    return is_signed(type) || is_unsigned(type);
  }
  return false;
}

inline bool is_numeric(const Type &type) {
  if (holds_alternative<BuiltinType>(type)) {
    const auto btype = get<BuiltinType>(type);
    return btype == Byte || is_integer(type) || is_float(type);
  }

  return false;
}

inline Type derive_bin_op_result_type(const Type &lhs, const Type &rhs) {
  if (lhs == rhs && is_numeric(lhs)) {
    return lhs;
  }

  if (is_numeric(lhs) && is_numeric(rhs)) {
    return max(get<BuiltinType>(lhs), get<BuiltinType>(rhs));
  }

  return nullptr;
}
} // namespace yona::compiler::types
