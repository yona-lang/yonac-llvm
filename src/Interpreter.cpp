//
// Created by akovari on 17.11.24.
//

#include "Interpreter.h"
#include <boost/log/trivial.hpp>

#include "common.h"

namespace yona::interp
{
  Interpreter::Interpreter()
  {
    stack_ = stack<Frame>();
    stack_.emplace();
  }

  any Interpreter::visit(AddExpr* node) const
  {
    auto left  = visit(node->left);
    auto right = visit(node->right);
    return any_cast<expr_wrapper>(left).get_node<IntegerExpr>()->value +
           any_cast<expr_wrapper>(right).get_node<IntegerExpr>()->value;
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
  any Interpreter::visit(BodyWithoutGuards* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ByteExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(CaseExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(CatchExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(CatchPatternExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(CharacterExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ConsLeftExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ConsRightExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(DictExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(DictGeneratorExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(DictGeneratorReducer* node) const { return expr_wrapper(node); }
  any Interpreter::visit(DictPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(DivideExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(DoExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(EqExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FalseLiteralExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FieldAccessExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FieldUpdateExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FloatExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FqnAlias* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FqnExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FunctionAlias* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FunctionBody* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FunctionExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(FunctionsImport* node) const { return expr_wrapper(node); }
  any Interpreter::visit(GtExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(GteExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(HeadTailsHeadPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(HeadTailsPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(IdentifierExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(IfExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ImportClauseExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ImportExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(InExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(IntegerExpr* node) const { return node->value; }
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
  any Interpreter::visit(PipeLeftExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(PipeRightExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(PowerExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RaiseExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RangeSequenceExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RecordInstanceExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RecordNode* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RecordPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(RightShiftExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(SeqGeneratorExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(SeqPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(SequenceExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(SetExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(SetGeneratorExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(StringExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(SubtractExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(SymbolExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(TailsHeadPattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(TrueLiteralExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(TryCatchExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(TupleExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(TuplePattern* node) const { return expr_wrapper(node); }
  any Interpreter::visit(UnderscoreNode* node) const { return expr_wrapper(node); }
  any Interpreter::visit(UnitExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ValueAlias* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ValueCollectionExtractorExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ValueExpr* node) const { return expr_wrapper(node); }
  any Interpreter::visit(ValuesSequenceExpr* node) const { return expr_wrapper(node); }
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
} // yona::interp
