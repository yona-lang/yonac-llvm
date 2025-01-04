//
// Created by akovari on 4.12.24.
//

#pragma once
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
  Byte,
  Bool,
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
  Unit
};

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
} // namespace yona::compiler::types
