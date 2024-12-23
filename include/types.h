//
// Created by akovari on 4.12.24.
//

#pragma once

namespace yona::compiler::types
{
  using namespace std;

  struct SingleItemCollectionType;
  struct DictCollectionType;
  struct DictCollectionType;
  struct NamedType;
  struct FunctionType;
  struct SumType;
  struct ProductType;
  struct UnionType;

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
    Module,
    Record,
    Exception
  };

  using Type =
      variant<ValueType, shared_ptr<SingleItemCollectionType>, shared_ptr<DictCollectionType>, shared_ptr<FunctionType>,
              shared_ptr<NamedType>, shared_ptr<SumType>, shared_ptr<ProductType>, nullptr_t>;

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

  struct FunctionType final
  {
    Type returnType;
    Type argumentType;
  };

  struct SumType final
  {
    unordered_set<Type> types;
  };

  struct ProductType final
  {
    vector<Type> types;
  };

  struct NamedType final
  {
    string name;
    Type type;
  };

  using FunctionTypes = unordered_map<string, FunctionType>; // FQN -> Type map
  inline FunctionTypes FUNCTION_TYPES;
}
