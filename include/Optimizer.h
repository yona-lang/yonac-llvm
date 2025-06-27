//
// Created by akovari on 17.11.24.
//

#pragma once

#include "ast.h"
#include "optimizer_result.h"

namespace yona::compiler {
using namespace std;
using namespace ast;

class YONA_API Optimizer final : public AstVisitor<OptimizerResult> {
public:
  [[nodiscard]] OptimizerResult visit(AddExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(AliasCall *node) const override;
  [[nodiscard]] OptimizerResult visit(ApplyExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(AsDataStructurePattern *node) const override;
  [[nodiscard]] OptimizerResult visit(BinaryNotOpExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(BitwiseAndExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(BitwiseOrExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(BitwiseXorExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(BodyWithGuards *node) const override;
  [[nodiscard]] OptimizerResult visit(BodyWithoutGuards *node) const override;
  [[nodiscard]] OptimizerResult visit(ByteExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(CaseExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(CaseClause *node) const override;
  [[nodiscard]] OptimizerResult visit(CatchExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(CatchPatternExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(CharacterExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(ConsLeftExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(ConsRightExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(DictExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(DictGeneratorExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(DictGeneratorReducer *node) const override;
  [[nodiscard]] OptimizerResult visit(DictPattern *node) const override;
  [[nodiscard]] OptimizerResult visit(DivideExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(DoExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(EqExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(FalseLiteralExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(FieldAccessExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(FieldUpdateExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(FloatExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(FqnAlias *node) const override;
  [[nodiscard]] OptimizerResult visit(FqnExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(FunctionAlias *node) const override;
  [[nodiscard]] OptimizerResult visit(FunctionExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(FunctionsImport *node) const override;
  [[nodiscard]] OptimizerResult visit(GtExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(GteExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(HeadTailsHeadPattern *node) const override;
  [[nodiscard]] OptimizerResult visit(HeadTailsPattern *node) const override;
  [[nodiscard]] OptimizerResult visit(IfExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(ImportClauseExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(ImportExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(InExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(IntegerExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(JoinExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(KeyValueCollectionExtractorExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(LambdaAlias *node) const override;
  [[nodiscard]] OptimizerResult visit(LeftShiftExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(LetExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(LogicalAndExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(LogicalNotOpExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(LogicalOrExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(LtExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(LteExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(ModuloExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(ModuleAlias *node) const override;
  [[nodiscard]] OptimizerResult visit(ModuleCall *node) const override;
  [[nodiscard]] OptimizerResult visit(ExprCall *node) const override;
  [[nodiscard]] OptimizerResult visit(ModuleExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(ModuleImport *node) const override;
  [[nodiscard]] OptimizerResult visit(MultiplyExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(NameCall *node) const override;
  [[nodiscard]] OptimizerResult visit(NameExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(NeqExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(PackageNameExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(PatternAlias *node) const override;
  [[nodiscard]] OptimizerResult visit(PatternExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(PatternValue *node) const override;
  [[nodiscard]] OptimizerResult visit(PatternWithGuards *node) const override;
  [[nodiscard]] OptimizerResult visit(PatternWithoutGuards *node) const override;
  [[nodiscard]] OptimizerResult visit(PipeLeftExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(PipeRightExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(PowerExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(RaiseExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(RangeSequenceExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(RecordInstanceExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(RecordNode *node) const override;
  [[nodiscard]] OptimizerResult visit(RecordPattern *node) const override;
  [[nodiscard]] OptimizerResult visit(RightShiftExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(SeqGeneratorExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(SeqPattern *node) const override;
  [[nodiscard]] OptimizerResult visit(SetExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(SetGeneratorExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(StringExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(SubtractExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(SymbolExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(TailsHeadPattern *node) const override;
  [[nodiscard]] OptimizerResult visit(TrueLiteralExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(TryCatchExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(TupleExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(TuplePattern *node) const override;
  [[nodiscard]] OptimizerResult visit(UnderscoreNode *node) const override;
  [[nodiscard]] OptimizerResult visit(UnitExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(ValueAlias *node) const override;
  [[nodiscard]] OptimizerResult visit(ValueCollectionExtractorExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(ValuesSequenceExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(WithExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(ZerofillRightShiftExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(FunctionDeclaration *node) const override;
  [[nodiscard]] OptimizerResult visit(TypeDeclaration *node) const override;
  [[nodiscard]] OptimizerResult visit(TypeDefinition *node) const override;
  [[nodiscard]] OptimizerResult visit(TypeNode *node) const override;
  [[nodiscard]] OptimizerResult visit(TypeInstance *node) const override;
  ~Optimizer() override = default;
  [[nodiscard]] OptimizerResult visit(ExprNode *node) const override;
  [[nodiscard]] OptimizerResult visit(AstNode *node) const override;
  [[nodiscard]] OptimizerResult visit(ScopedNode *node) const override;
  [[nodiscard]] OptimizerResult visit(PatternNode *node) const override;
  [[nodiscard]] OptimizerResult visit(ValueExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(SequenceExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(FunctionBody *node) const override;
  [[nodiscard]] OptimizerResult visit(IdentifierExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(AliasExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(OpExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(BinaryOpExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(MainNode *node) const override;
  [[nodiscard]] OptimizerResult visit(BuiltinTypeNode *node) const override;
  [[nodiscard]] OptimizerResult visit(UserDefinedTypeNode *node) const override;
  [[nodiscard]] OptimizerResult visit(TypeNameNode *node) const override;
  [[nodiscard]] OptimizerResult visit(CallExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(GeneratorExpr *node) const override;
  [[nodiscard]] OptimizerResult visit(CollectionExtractorExpr *node) const override;
};
} // namespace yona::compiler
