//
// Created by akovari on 17.11.24.
//

#pragma once

#include "ast.h"

namespace yona::interp
{
  using namespace std;
  using namespace ast;

  using any = any;
  using symbol_ref_t = any;

  struct Symbol
  {
private:
    string name;
    symbol_ref_t reference;
  };

  struct Frame
  {
private:
    vector<symbol_ref_t> args_;
    unordered_map<string, Symbol> locals_;

    int64_t ria{ 0 }, rib{ 0 };
    double rda{ 0.0 }, rdb{ 0.0 };
    wchar_t rca{ L'\0' }, rcb{ L'\0' };
    bool rba{ false }, rbb{ false };
    byte rya{ 0 }, ryb{ 0 };
    string rsa, rsb;
  };

  class Interpreter final : public AstVisitor
  {
private:
    stack<Frame> stack_;

public:
    explicit Interpreter();
    [[nodiscard]] any visit(AddExpr* node) const override;
    [[nodiscard]] any visit(AliasCall* node) const override;
    [[nodiscard]] any visit(AliasExpr* node) const override;
    [[nodiscard]] any visit(ApplyExpr* node) const override;
    [[nodiscard]] any visit(AsDataStructurePattern* node) const override;
    [[nodiscard]] any visit(BinaryNotOpExpr* node) const override;
    [[nodiscard]] any visit(BitwiseAndExpr* node) const override;
    [[nodiscard]] any visit(BitwiseOrExpr* node) const override;
    [[nodiscard]] any visit(BitwiseXorExpr* node) const override;
    [[nodiscard]] any visit(BodyWithGuards* node) const override;
    [[nodiscard]] any visit(BodyWithoutGuards* node) const override;
    [[nodiscard]] any visit(ByteExpr* node) const override;
    [[nodiscard]] any visit(CaseExpr* node) const override;
    [[nodiscard]] any visit(CatchExpr* node) const override;
    [[nodiscard]] any visit(CatchPatternExpr* node) const override;
    [[nodiscard]] any visit(CharacterExpr* node) const override;
    [[nodiscard]] any visit(ConsLeftExpr* node) const override;
    [[nodiscard]] any visit(ConsRightExpr* node) const override;
    [[nodiscard]] any visit(DictExpr* node) const override;
    [[nodiscard]] any visit(DictGeneratorExpr* node) const override;
    [[nodiscard]] any visit(DictGeneratorReducer* node) const override;
    [[nodiscard]] any visit(DictPattern* node) const override;
    [[nodiscard]] any visit(DivideExpr* node) const override;
    [[nodiscard]] any visit(DoExpr* node) const override;
    [[nodiscard]] any visit(EqExpr* node) const override;
    [[nodiscard]] any visit(FalseLiteralExpr* node) const override;
    [[nodiscard]] any visit(FieldAccessExpr* node) const override;
    [[nodiscard]] any visit(FieldUpdateExpr* node) const override;
    [[nodiscard]] any visit(FloatExpr* node) const override;
    [[nodiscard]] any visit(FqnAlias* node) const override;
    [[nodiscard]] any visit(FqnExpr* node) const override;
    [[nodiscard]] any visit(FunctionAlias* node) const override;
    [[nodiscard]] any visit(FunctionBody* node) const override;
    [[nodiscard]] any visit(FunctionExpr* node) const override;
    [[nodiscard]] any visit(FunctionsImport* node) const override;
    [[nodiscard]] any visit(GtExpr* node) const override;
    [[nodiscard]] any visit(GteExpr* node) const override;
    [[nodiscard]] any visit(HeadTailsHeadPattern* node) const override;
    [[nodiscard]] any visit(HeadTailsPattern* node) const override;
    [[nodiscard]] any visit(IdentifierExpr* node) const override;
    [[nodiscard]] any visit(IfExpr* node) const override;
    [[nodiscard]] any visit(ImportClauseExpr* node) const override;
    [[nodiscard]] any visit(ImportExpr* node) const override;
    [[nodiscard]] any visit(InExpr* node) const override;
    [[nodiscard]] any visit(IntegerExpr* node) const override;
    [[nodiscard]] any visit(JoinExpr* node) const override;
    [[nodiscard]] any visit(KeyValueCollectionExtractorExpr* node) const override;
    [[nodiscard]] any visit(LambdaAlias* node) const override;
    [[nodiscard]] any visit(LeftShiftExpr* node) const override;
    [[nodiscard]] any visit(LetExpr* node) const override;
    [[nodiscard]] any visit(LogicalAndExpr* node) const override;
    [[nodiscard]] any visit(LogicalNotOpExpr* node) const override;
    [[nodiscard]] any visit(LogicalOrExpr* node) const override;
    [[nodiscard]] any visit(LtExpr* node) const override;
    [[nodiscard]] any visit(LteExpr* node) const override;
    [[nodiscard]] any visit(ModuloExpr* node) const override;
    [[nodiscard]] any visit(ModuleAlias* node) const override;
    [[nodiscard]] any visit(ModuleCall* node) const override;
    [[nodiscard]] any visit(ModuleExpr* node) const override;
    [[nodiscard]] any visit(ModuleImport* node) const override;
    [[nodiscard]] any visit(MultiplyExpr* node) const override;
    [[nodiscard]] any visit(NameCall* node) const override;
    [[nodiscard]] any visit(NameExpr* node) const override;
    [[nodiscard]] any visit(NeqExpr* node) const override;
    [[nodiscard]] any visit(OpExpr* node) const override;
    [[nodiscard]] any visit(PackageNameExpr* node) const override;
    [[nodiscard]] any visit(PatternAlias* node) const override;
    [[nodiscard]] any visit(PatternExpr* node) const override;
    [[nodiscard]] any visit(PatternValue* node) const override;
    [[nodiscard]] any visit(PatternWithGuards* node) const override;
    [[nodiscard]] any visit(PatternWithoutGuards* node) const override;
    [[nodiscard]] any visit(PipeLeftExpr* node) const override;
    [[nodiscard]] any visit(PipeRightExpr* node) const override;
    [[nodiscard]] any visit(PowerExpr* node) const override;
    [[nodiscard]] any visit(RaiseExpr* node) const override;
    [[nodiscard]] any visit(RangeSequenceExpr* node) const override;
    [[nodiscard]] any visit(RecordInstanceExpr* node) const override;
    [[nodiscard]] any visit(RecordNode* node) const override;
    [[nodiscard]] any visit(RecordPattern* node) const override;
    [[nodiscard]] any visit(RightShiftExpr* node) const override;
    [[nodiscard]] any visit(SeqGeneratorExpr* node) const override;
    [[nodiscard]] any visit(SeqPattern* node) const override;
    [[nodiscard]] any visit(SequenceExpr* node) const override;
    [[nodiscard]] any visit(SetExpr* node) const override;
    [[nodiscard]] any visit(SetGeneratorExpr* node) const override;
    [[nodiscard]] any visit(StringExpr* node) const override;
    [[nodiscard]] any visit(SubtractExpr* node) const override;
    [[nodiscard]] any visit(SymbolExpr* node) const override;
    [[nodiscard]] any visit(TailsHeadPattern* node) const override;
    [[nodiscard]] any visit(TrueLiteralExpr* node) const override;
    [[nodiscard]] any visit(TryCatchExpr* node) const override;
    [[nodiscard]] any visit(TupleExpr* node) const override;
    [[nodiscard]] any visit(TuplePattern* node) const override;
    [[nodiscard]] any visit(UnderscoreNode* node) const override;
    [[nodiscard]] any visit(UnitExpr* node) const override;
    [[nodiscard]] any visit(ValueAlias* node) const override;
    [[nodiscard]] any visit(ValueCollectionExtractorExpr* node) const override;
    [[nodiscard]] any visit(ValueExpr* node) const override;
    [[nodiscard]] any visit(ValuesSequenceExpr* node) const override;
    [[nodiscard]] any visit(WithExpr* node) const override;
    [[nodiscard]] any visit(ZerofillRightShiftExpr* node) const override;
    ~Interpreter() override = default;
    [[nodiscard]] any visit(ExprNode* node) const override;
    [[nodiscard]] any visit(AstNode* node) const override;
    [[nodiscard]] any visit(ScopedNode* node) const override;
    [[nodiscard]] any visit(PatternNode* node) const override;
  };

} // yonac::interp
