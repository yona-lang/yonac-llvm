//
// Created by akovari on 4.12.24.
//

#pragma once

#include <antlr4-runtime.h>

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

  struct RecordType final
  {
    string name;
  };

  using FunctionTypes = unordered_map<string, Type>; // FQN -> Type map
  inline FunctionTypes FUNCTION_TYPES;
}
