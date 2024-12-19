//
// Created by akovari on 17.11.24.
//

#include "Optimizer.h"
#include "common.h"

namespace yona::compiler
{
  using namespace std;
  any Optimizer::visit(AddExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(AliasCall* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(AliasExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ApplyExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(AsDataStructurePattern* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(BinaryNotOpExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(BitwiseAndExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(BitwiseOrExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(BitwiseXorExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(BodyWithGuards* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(BodyWithoutGuards* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ByteExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(CaseExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(CatchExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(CatchPatternExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(CharacterExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ConsLeftExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ConsRightExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(DictExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(DictGeneratorExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(DictGeneratorReducer* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(DictPattern* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(DivideExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(DoExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(EqExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(FalseLiteralExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(FieldAccessExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(FieldUpdateExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(FloatExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(FqnAlias* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(FqnExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(FunctionAlias* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(FunctionBody* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(FunctionExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(FunctionsImport* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(GtExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(GteExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(HeadTailsHeadPattern* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(HeadTailsPattern* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(IdentifierExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(IfExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ImportClauseExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ImportExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(InExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(IntegerExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(JoinExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(KeyValueCollectionExtractorExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(LambdaAlias* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(LeftShiftExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(LetExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(LogicalAndExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(LogicalNotOpExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(LogicalOrExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(LtExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(LteExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ModuloExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ModuleAlias* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ModuleCall* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ModuleExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ModuleImport* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(MultiplyExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(NameCall* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(NameExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(NeqExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(OpExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(PackageNameExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(PatternAlias* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(PatternExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(PatternValue* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(PatternWithGuards* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(PatternWithoutGuards* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(PipeLeftExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(PipeRightExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(PowerExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(RaiseExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(RangeSequenceExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(RecordInstanceExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(RecordNode* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(RecordPattern* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(RightShiftExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(SeqGeneratorExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(SeqPattern* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(SequenceExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(SetExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(SetGeneratorExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(StringExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(SubtractExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(SymbolExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(TailsHeadPattern* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(TrueLiteralExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(TryCatchExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(TupleExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(TuplePattern* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(UnderscoreNode* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(UnitExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ValueAlias* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ValueCollectionExtractorExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ValueExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ValuesSequenceExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(WithExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ZerofillRightShiftExpr* node) const { return any(expr_wrapper(node)); }
  any Optimizer::visit(ExprNode* node) const { return AstVisitor::visit(node); }
  any Optimizer::visit(AstNode* node) const { return AstVisitor::visit(node); }
  any Optimizer::visit(ScopedNode* node) const { return AstVisitor::visit(node); }
  any Optimizer::visit(PatternNode* node) const { return AstVisitor::visit(node); }
}
