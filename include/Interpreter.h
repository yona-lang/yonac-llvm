//
// Created by akovari on 17.11.24.
//

#pragma once

#include <utility>

#include "ast.h"
#include "runtime.h"

namespace yona::interp {
using namespace std;
using namespace ast;
using namespace runtime;

using symbol_ref_t = shared_ptr<RuntimeObject>;

struct Frame {
private:
  vector<symbol_ref_t> args_;
  unordered_map<string, symbol_ref_t> locals_;

public:
  shared_ptr<Frame> parent;

  explicit Frame(shared_ptr<Frame> parent) : parent(std::move(parent)) {}

  void write(const string &name, symbol_ref_t value);
  void write(const string &name, any value);
  symbol_ref_t lookup(SourceInfo source_token, const string &name);
};

inline shared_ptr<Frame> frame(nullptr);

class Interpreter final : public AstVisitor {
public:
  template <RuntimeObjectType ROT, typename VT> optional<VT> get_value(AstNode *node) const;

  template <RuntimeObjectType ROT, typename VT> optional<any> map_value(initializer_list<AstNode *> nodes, function<VT(vector<VT>)> cb) const;

  template <RuntimeObjectType actual, RuntimeObjectType... expected> static void type_error(AstNode *node);

  any visit(AddExpr *node) const override;
  any visit(AliasCall *node) const override;
  any visit(ApplyExpr *node) const override;
  any visit(AsDataStructurePattern *node) const override;
  any visit(BinaryNotOpExpr *node) const override;
  any visit(BitwiseAndExpr *node) const override;
  any visit(BitwiseOrExpr *node) const override;
  any visit(BitwiseXorExpr *node) const override;
  any visit(BodyWithGuards *node) const override;
  any visit(BodyWithoutGuards *node) const override;
  any visit(ByteExpr *node) const override;
  any visit(CaseExpr *node) const override;
  any visit(CatchExpr *node) const override;
  any visit(CatchPatternExpr *node) const override;
  any visit(CharacterExpr *node) const override;
  any visit(ConsLeftExpr *node) const override;
  any visit(ConsRightExpr *node) const override;
  any visit(DictExpr *node) const override;
  any visit(DictGeneratorExpr *node) const override;
  any visit(DictGeneratorReducer *node) const override;
  any visit(DictPattern *node) const override;
  any visit(DivideExpr *node) const override;
  any visit(DoExpr *node) const override;
  any visit(EqExpr *node) const override;
  any visit(FalseLiteralExpr *node) const override;
  any visit(FieldAccessExpr *node) const override;
  any visit(FieldUpdateExpr *node) const override;
  any visit(FloatExpr *node) const override;
  any visit(FqnAlias *node) const override;
  any visit(FqnExpr *node) const override;
  any visit(FunctionAlias *node) const override;
  any visit(FunctionExpr *node) const override;
  any visit(FunctionsImport *node) const override;
  any visit(GtExpr *node) const override;
  any visit(GteExpr *node) const override;
  any visit(HeadTailsHeadPattern *node) const override;
  any visit(HeadTailsPattern *node) const override;
  any visit(IfExpr *node) const override;
  any visit(ImportClauseExpr *node) const override;
  any visit(ImportExpr *node) const override;
  any visit(InExpr *node) const override;
  any visit(IntegerExpr *node) const override;
  any visit(JoinExpr *node) const override;
  any visit(KeyValueCollectionExtractorExpr *node) const override;
  any visit(LambdaAlias *node) const override;
  any visit(LeftShiftExpr *node) const override;
  any visit(LetExpr *node) const override;
  any visit(LogicalAndExpr *node) const override;
  any visit(LogicalNotOpExpr *node) const override;
  any visit(LogicalOrExpr *node) const override;
  any visit(LtExpr *node) const override;
  any visit(LteExpr *node) const override;
  any visit(ModuloExpr *node) const override;
  any visit(ModuleAlias *node) const override;
  any visit(ModuleCall *node) const override;
  any visit(ModuleExpr *node) const override;
  any visit(ModuleImport *node) const override;
  any visit(MultiplyExpr *node) const override;
  any visit(NameCall *node) const override;
  any visit(NameExpr *node) const override;
  any visit(NeqExpr *node) const override;
  any visit(PackageNameExpr *node) const override;
  any visit(PatternAlias *node) const override;
  any visit(PatternExpr *node) const override;
  any visit(PatternValue *node) const override;
  any visit(PatternWithGuards *node) const override;
  any visit(PatternWithoutGuards *node) const override;
  any visit(PowerExpr *node) const override;
  any visit(RaiseExpr *node) const override;
  any visit(RangeSequenceExpr *node) const override;
  any visit(RecordInstanceExpr *node) const override;
  any visit(RecordNode *node) const override;
  any visit(RecordPattern *node) const override;
  any visit(RightShiftExpr *node) const override;
  any visit(SeqGeneratorExpr *node) const override;
  any visit(SeqPattern *node) const override;
  any visit(SetExpr *node) const override;
  any visit(SetGeneratorExpr *node) const override;
  any visit(StringExpr *node) const override;
  any visit(SubtractExpr *node) const override;
  any visit(SymbolExpr *node) const override;
  any visit(TailsHeadPattern *node) const override;
  any visit(TrueLiteralExpr *node) const override;
  any visit(TryCatchExpr *node) const override;
  any visit(TupleExpr *node) const override;
  any visit(TuplePattern *node) const override;
  any visit(UnderscoreNode *node) const override;
  any visit(UnitExpr *node) const override;
  any visit(ValueAlias *node) const override;
  any visit(ValueCollectionExtractorExpr *node) const override;
  any visit(ValuesSequenceExpr *node) const override;
  any visit(WithExpr *node) const override;
  any visit(ZerofillRightShiftExpr *node) const override;
  any visit(FunctionDeclaration *node) const override;
  any visit(TypeDeclaration *node) const override;
  any visit(TypeDefinition *node) const override;
  any visit(TypeNode *node) const override;
  any visit(TypeInstance *node) const override;
  ~Interpreter() override = default;
  any visit(ExprNode *node) const override;
  any visit(AstNode *node) const override;
  any visit(ScopedNode *node) const override;
  any visit(PatternNode *node) const override;
  any visit(ValueExpr *node) const override;
  any visit(SequenceExpr *node) const override;
  any visit(FunctionBody *node) const override;
  any visit(IdentifierExpr *node) const override;
  any visit(AliasExpr *node) const override;
  any visit(OpExpr *node) const override;
  any visit(BinaryOpExpr *node) const override;
};
} // namespace yona::interp
