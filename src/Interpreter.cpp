//
// Created by akovari on 17.11.24.
//

#include "Interpreter.h"
#include <boost/log/trivial.hpp>

#include "common.h"

namespace yona::interp
{
  void Frame::write(const string& name, symbol_ref_t value) { locals_[name] = std::move(value); }

  symbol_ref_t Frame::lookup(const string& name)
  {
    if (locals_.contains(name))
    {
      return locals_[name];
    }
    else if (parent_)
    {
      return parent_->lookup(name);
    }
    return nullptr;
  }

  template <RuntimeObjectType ROT, typename VT>
  VT& Interpreter::get_value(AstNode* node) const
  {
    const shared_ptr<RuntimeObject> runtime_object = any_cast<shared_ptr<RuntimeObject>>(visit(node));
    if (runtime_object->type != ROT)
    {
      throw yona_error(node->token, yona_error::Type::TYPE,
                       "Type mismatch: expected " + RuntimeObjectTypes[ROT] + ", got " +
                           RuntimeObjectTypes[runtime_object->type]);
    }
    return runtime_object->get<VT>();
  }

  any Interpreter::visit(AddExpr* node) const
  {
    const auto left  = get_value<Int, int>(node->left);
    const auto right = get_value<Int, int>(node->right);

    return make_shared<RuntimeObject>(Int, left + right);
  }
  any Interpreter::visit(AliasCall* node) const { return expr_wrapper(node); }
  any Interpreter::visit(AliasExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ApplyExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(AsDataStructurePattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(BinaryNotOpExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(BitwiseAndExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(BitwiseOrExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(BitwiseXorExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(BodyWithGuards* node) const { return expr_wrapper(node); }
  any Interpreter::visit(BodyWithoutGuards* node) const { return visit(node->expr); }
  any Interpreter::visit(ByteExpr* node) const { return make_shared<RuntimeObject>(Byte, node->value); }
  any Interpreter::visit(CaseExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(CatchExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(CatchPatternExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(CharacterExpr* node) const { return make_shared<RuntimeObject>(Char, node->value); }
  any Interpreter::visit(ConsLeftExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ConsRightExpr* node) const { return expr_wrapper(node); }

  any Interpreter::visit(DictExpr* node) const
  {
    vector<pair<shared_ptr<RuntimeObject>, shared_ptr<RuntimeObject>>> fields;

    fields.reserve(node->values.size());
    for (const auto [fst, snd] : node->values)
    {
      fields.emplace_back(any_cast<shared_ptr<RuntimeObject>>(visit(fst)),
                          any_cast<shared_ptr<RuntimeObject>>(visit(snd)));
    }

    return make_shared<RuntimeObject>(Dict, make_shared<DictValue>(fields));
  }
  any Interpreter::visit(DictGeneratorExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(DictGeneratorReducer* node) const { return expr_wrapper(node); }
  any Interpreter::visit(DictPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(DivideExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(DoExpr* node) const
  {
    any result;
    for (const auto child : node->steps)
    {
      result = visit(child);
    }
    return result;
  }
  any Interpreter::visit(EqExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FalseLiteralExpr* node) const { return make_shared<RuntimeObject>(Bool, false); }
  any Interpreter::visit(FieldAccessExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FieldUpdateExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FloatExpr* node) const { return make_shared<RuntimeObject>(Float, node->value); }
  any Interpreter::visit(FqnAlias* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FqnExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FunctionAlias* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FunctionExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FunctionsImport* node) const { return expr_wrapper(node); }
  any Interpreter::visit(GtExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(GteExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(HeadTailsHeadPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(HeadTailsPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(IfExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ImportClauseExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ImportExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(InExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(IntegerExpr* node) const { return make_shared<RuntimeObject>(Int, node->value); }
  any Interpreter::visit(JoinExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(KeyValueCollectionExtractorExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(LambdaAlias* node) const { return expr_wrapper(node); }
  any Interpreter::visit(LeftShiftExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(LetExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(LogicalAndExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(LogicalNotOpExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(LogicalOrExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(LtExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(LteExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ModuloExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ModuleAlias* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ModuleCall* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ModuleExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ModuleImport* node) const { return expr_wrapper(node); }
  any Interpreter::visit(MultiplyExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(NameCall* node) const { return expr_wrapper(node); }
  any Interpreter::visit(NameExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(NeqExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(OpExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(PackageNameExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(PatternAlias* node) const { return expr_wrapper(node); }
  any Interpreter::visit(PatternExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(PatternValue* node) const { return expr_wrapper(node); }
  any Interpreter::visit(PatternWithGuards* node) const { return expr_wrapper(node); }
  any Interpreter::visit(PatternWithoutGuards* node) const { return expr_wrapper(node); }
  any Interpreter::visit(PowerExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RaiseExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RangeSequenceExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RecordInstanceExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RecordNode* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RecordPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RightShiftExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(SeqGeneratorExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(SeqPattern* node) const { return expr_wrapper(node); }

  any Interpreter::visit(SetExpr* node) const
  {
    vector<shared_ptr<RuntimeObject>> fields;

    fields.reserve(node->values.size());
    for (const auto value : node->values)
    {
      fields.push_back(any_cast<shared_ptr<RuntimeObject>>(visit(value)));
    }

    return make_shared<RuntimeObject>(Set, make_shared<SetValue>(fields));
  }

  any Interpreter::visit(SetGeneratorExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(StringExpr* node) const { return make_shared<RuntimeObject>(String, node->value); }
  any Interpreter::visit(SubtractExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(SymbolExpr* node) const
  {
    return make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(node->value));
  }
  any Interpreter::visit(TailsHeadPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(TrueLiteralExpr* node) const { return make_shared<RuntimeObject>(Bool, true); }
  any Interpreter::visit(TryCatchExpr* node) const { return expr_wrapper(node); }

  any Interpreter::visit(TupleExpr* node) const
  {
    vector<shared_ptr<RuntimeObject>> fields;

    fields.reserve(node->values.size());
    for (const auto value : node->values)
    {
      fields.push_back(any_cast<shared_ptr<RuntimeObject>>(visit(value)));
    }

    return make_shared<RuntimeObject>(Tuple, make_shared<TupleValue>(fields));
  }

  any Interpreter::visit(TuplePattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(UnderscoreNode* node) const { return make_shared<RuntimeObject>(Bool, true); }
  any Interpreter::visit(UnitExpr* node) const { return make_shared<RuntimeObject>(Unit, nullptr); }
  any Interpreter::visit(ValueAlias* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ValueCollectionExtractorExpr* node) const { return expr_wrapper(node); }

  any Interpreter::visit(ValuesSequenceExpr* node) const
  {
    vector<shared_ptr<RuntimeObject>> fields;

    fields.reserve(node->values.size());
    for (const auto value : node->values)
    {
      fields.push_back(any_cast<shared_ptr<RuntimeObject>>(visit(value)));
    }

    return make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(fields));
  }

  any Interpreter::visit(WithExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ZerofillRightShiftExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FunctionDeclaration* node) const { return expr_wrapper(node); }
  any Interpreter::visit(TypeDeclaration* node) const { return expr_wrapper(node); }
  any Interpreter::visit(TypeDefinition* node) const { return expr_wrapper(node); }
  any Interpreter::visit(TypeNode* node) const { return expr_wrapper(node); }
  any Interpreter::visit(TypeInstance* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ExprNode* node) const { return AstVisitor::visit(node); }
  any Interpreter::visit(AstNode* node) const { return AstVisitor::visit(node); }
  any Interpreter::visit(ScopedNode* node) const { return AstVisitor::visit(node); }
  any Interpreter::visit(PatternNode* node) const { return AstVisitor::visit(node); }
  any Interpreter::visit(ValueExpr* node) const { return AstVisitor::visit(node); }
  any Interpreter::visit(SequenceExpr* node) const { return AstVisitor::visit(node); }
  any Interpreter::visit(FunctionBody* node) const { return AstVisitor::visit(node); }
  any Interpreter::visit(IdentifierExpr* node) const { return AstVisitor::visit(node); }
} // yona::interp
