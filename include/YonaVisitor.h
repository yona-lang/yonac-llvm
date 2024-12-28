#pragma once

#include <boost/log/trivial.hpp>
#include <queue>

#include "YonaParserBaseVisitor.h"
#include "ast.h"
#include "common.h"

namespace yona {
using namespace std;
using namespace ast;

template <typename T, typename... Args>
  requires derived_from<T, AstNode>
any wrap_expr(SourceContext source_ctx, Args &&...args) {
  return any(expr_wrapper(new T(source_ctx, std::forward<Args>(args)...)));
}

class YonaVisitor : public YonaParserBaseVisitor {
private:
  int lambda_count_ = 0;
  stack<string> module_stack_;
  vector<string> names_;

  string nextLambdaName();
  ModuleImportQueue module_imports_;

  template <typename T>
    requires derived_from<T, AstNode>
  T *visit_expr(tree::ParseTree *tree) {
    if (tree == nullptr) {
      return nullptr;
    }
    return any_cast<expr_wrapper>(visit(tree)).get_node<T>();
  }

  template <typename T, typename PT>
    requires derived_from<T, AstNode>
  vector<T *> visit_exprs(vector<PT *> trees) {
    vector<T *> exprs;
    for (auto tree : trees) {
      exprs.push_back(visit_expr<T>(tree));
    }
    return exprs;
  }

  [[nodiscard]] string fqn() const;

public:
  explicit YonaVisitor(ModuleImportQueue module_imports) : module_imports_(std::move(module_imports)) {}
  ~YonaVisitor() override = default;

  std::any visitInput(YonaParser::InputContext *ctx) override;
  std::any visitFunction(YonaParser::FunctionContext *ctx) override;
  std::any visitFunctionName(YonaParser::FunctionNameContext *ctx) override;
  std::any visitBodyWithGuards(YonaParser::BodyWithGuardsContext *ctx) override;
  std::any visitBodyWithoutGuard(YonaParser::BodyWithoutGuardContext *ctx) override;
  std::any visitNegationExpression(YonaParser::NegationExpressionContext *ctx) override;
  std::any visitValueExpression(YonaParser::ValueExpressionContext *ctx) override;
  std::any visitAdditiveExpression(YonaParser::AdditiveExpressionContext *ctx) override;
  std::any visitPipeRightExpression(YonaParser::PipeRightExpressionContext *ctx) override;
  std::any visitBinaryShiftExpression(YonaParser::BinaryShiftExpressionContext *ctx) override;
  std::any visitFunctionApplicationExpression(YonaParser::FunctionApplicationExpressionContext *ctx) override;
  std::any visitFieldAccessExpression(YonaParser::FieldAccessExpressionContext *ctx) override;
  std::any visitBacktickExpression(YonaParser::BacktickExpressionContext *ctx) override;
  std::any visitCaseExpression(YonaParser::CaseExpressionContext *ctx) override;
  std::any visitTryCatchExpression(YonaParser::TryCatchExpressionContext *ctx) override;
  std::any visitBitwiseAndExpression(YonaParser::BitwiseAndExpressionContext *ctx) override;
  std::any visitLetExpression(YonaParser::LetExpressionContext *ctx) override;
  std::any visitDoExpression(YonaParser::DoExpressionContext *ctx) override;
  std::any visitLogicalAndExpression(YonaParser::LogicalAndExpressionContext *ctx) override;
  std::any visitConsRightExpression(YonaParser::ConsRightExpressionContext *ctx) override;
  std::any visitExpressionInParents(YonaParser::ExpressionInParentsContext *ctx) override;
  std::any visitConsLeftExpression(YonaParser::ConsLeftExpressionContext *ctx) override;
  std::any visitBitwiseXorExpression(YonaParser::BitwiseXorExpressionContext *ctx) override;
  std::any visitGeneratorExpression(YonaParser::GeneratorExpressionContext *ctx) override;
  std::any visitConditionalExpression(YonaParser::ConditionalExpressionContext *ctx) override;
  std::any visitMultiplicativeExpression(YonaParser::MultiplicativeExpressionContext *ctx) override;
  std::any visitLogicalOrExpression(YonaParser::LogicalOrExpressionContext *ctx) override;
  std::any visitPipeLeftExpression(YonaParser::PipeLeftExpressionContext *ctx) override;
  std::any visitComparativeExpression(YonaParser::ComparativeExpressionContext *ctx) override;
  std::any visitBitwiseOrExpression(YonaParser::BitwiseOrExpressionContext *ctx) override;
  std::any visitInExpression(YonaParser::InExpressionContext *ctx) override;
  std::any visitRaiseExpression(YonaParser::RaiseExpressionContext *ctx) override;
  std::any visitWithExpression(YonaParser::WithExpressionContext *ctx) override;
  std::any visitFieldUpdateExpression(YonaParser::FieldUpdateExpressionContext *ctx) override;
  std::any visitJoinExpression(YonaParser::JoinExpressionContext *ctx) override;
  std::any visitImportExpression(YonaParser::ImportExpressionContext *ctx) override;
  std::any visitLiteral(YonaParser::LiteralContext *ctx) override;
  std::any visitValue(YonaParser::ValueContext *ctx) override;
  std::any visitPatternValue(YonaParser::PatternValueContext *ctx) override;
  std::any visitName(YonaParser::NameContext *ctx) override;
  std::any visitLet(YonaParser::LetContext *ctx) override;
  std::any visitAlias(YonaParser::AliasContext *ctx) override;
  std::any visitLambdaAlias(YonaParser::LambdaAliasContext *ctx) override;
  std::any visitModuleAlias(YonaParser::ModuleAliasContext *ctx) override;
  std::any visitValueAlias(YonaParser::ValueAliasContext *ctx) override;
  std::any visitPatternAlias(YonaParser::PatternAliasContext *ctx) override;
  std::any visitFqnAlias(YonaParser::FqnAliasContext *ctx) override;
  std::any visitConditional(YonaParser::ConditionalContext *ctx) override;
  std::any visitApply(YonaParser::ApplyContext *ctx) override;
  std::any visitFunArg(YonaParser::FunArgContext *ctx) override;
  std::any visitCall(YonaParser::CallContext *ctx) override;
  std::any visitModuleCall(YonaParser::ModuleCallContext *ctx) override;
  std::any visitNameCall(YonaParser::NameCallContext *ctx) override;
  std::any visitModule(YonaParser::ModuleContext *ctx) override;
  std::any visitNonEmptyListOfNames(YonaParser::NonEmptyListOfNamesContext *ctx) override;
  std::any visitUnit(YonaParser::UnitContext *ctx) override;
  std::any visitByteLiteral(YonaParser::ByteLiteralContext *ctx) override;
  std::any visitFloatLiteral(YonaParser::FloatLiteralContext *ctx) override;
  std::any visitIntegerLiteral(YonaParser::IntegerLiteralContext *ctx) override;
  std::any visitStringLiteral(YonaParser::StringLiteralContext *ctx) override;
  std::any visitInterpolatedStringPart(YonaParser::InterpolatedStringPartContext *ctx) override;
  std::any visitCharacterLiteral(YonaParser::CharacterLiteralContext *ctx) override;
  std::any visitBooleanLiteral(YonaParser::BooleanLiteralContext *ctx) override;
  std::any visitTuple(YonaParser::TupleContext *ctx) override;
  std::any visitDict(YonaParser::DictContext *ctx) override;
  std::any visitDictKey(YonaParser::DictKeyContext *ctx) override;
  std::any visitDictVal(YonaParser::DictValContext *ctx) override;
  std::any visitSequence(YonaParser::SequenceContext *ctx) override;
  std::any visitSet(YonaParser::SetContext *ctx) override;
  std::any visitFqn(YonaParser::FqnContext *ctx) override;
  std::any visitPackageName(YonaParser::PackageNameContext *ctx) override;
  std::any visitModuleName(YonaParser::ModuleNameContext *ctx) override;
  std::any visitSymbol(YonaParser::SymbolContext *ctx) override;
  std::any visitIdentifier(YonaParser::IdentifierContext *ctx) override;
  std::any visitLambda(YonaParser::LambdaContext *ctx) override;
  std::any visitUnderscore(YonaParser::UnderscoreContext *ctx) override;
  std::any visitEmptySequence(YonaParser::EmptySequenceContext *ctx) override;
  std::any visitOtherSequence(YonaParser::OtherSequenceContext *ctx) override;
  std::any visitRangeSequence(YonaParser::RangeSequenceContext *ctx) override;
  std::any visitCaseExpr(YonaParser::CaseExprContext *ctx) override;
  std::any visitPatternExpression(YonaParser::PatternExpressionContext *ctx) override;
  std::any visitDoExpr(YonaParser::DoExprContext *ctx) override;
  std::any visitDoOneStep(YonaParser::DoOneStepContext *ctx) override;
  std::any visitPatternExpressionWithoutGuard(YonaParser::PatternExpressionWithoutGuardContext *ctx) override;
  std::any visitPatternExpressionWithGuard(YonaParser::PatternExpressionWithGuardContext *ctx) override;
  std::any visitPattern(YonaParser::PatternContext *ctx) override;
  std::any visitDataStructurePattern(YonaParser::DataStructurePatternContext *ctx) override;
  std::any visitAsDataStructurePattern(YonaParser::AsDataStructurePatternContext *ctx) override;
  std::any visitPatternWithoutSequence(YonaParser::PatternWithoutSequenceContext *ctx) override;
  std::any visitTuplePattern(YonaParser::TuplePatternContext *ctx) override;
  std::any visitSequencePattern(YonaParser::SequencePatternContext *ctx) override;
  std::any visitHeadTails(YonaParser::HeadTailsContext *ctx) override;
  std::any visitTailsHead(YonaParser::TailsHeadContext *ctx) override;
  std::any visitHeadTailsHead(YonaParser::HeadTailsHeadContext *ctx) override;
  std::any visitLeftPattern(YonaParser::LeftPatternContext *ctx) override;
  std::any visitRightPattern(YonaParser::RightPatternContext *ctx) override;
  std::any visitTails(YonaParser::TailsContext *ctx) override;
  std::any visitDictPattern(YonaParser::DictPatternContext *ctx) override;
  std::any visitRecordPattern(YonaParser::RecordPatternContext *ctx) override;
  std::any visitImportExpr(YonaParser::ImportExprContext *ctx) override;
  std::any visitImportClause(YonaParser::ImportClauseContext *ctx) override;
  std::any visitModuleImport(YonaParser::ModuleImportContext *ctx) override;
  std::any visitFunctionsImport(YonaParser::FunctionsImportContext *ctx) override;
  std::any visitFunctionAlias(YonaParser::FunctionAliasContext *ctx) override;
  std::any visitTryCatchExpr(YonaParser::TryCatchExprContext *ctx) override;
  std::any visitCatchExpr(YonaParser::CatchExprContext *ctx) override;
  std::any visitCatchPatternExpression(YonaParser::CatchPatternExpressionContext *ctx) override;
  std::any visitTriplePattern(YonaParser::TriplePatternContext *ctx) override;
  std::any visitCatchPatternExpressionWithoutGuard(YonaParser::CatchPatternExpressionWithoutGuardContext *ctx) override;
  std::any visitCatchPatternExpressionWithGuard(YonaParser::CatchPatternExpressionWithGuardContext *ctx) override;
  std::any visitRaiseExpr(YonaParser::RaiseExprContext *ctx) override;
  std::any visitWithExpr(YonaParser::WithExprContext *ctx) override;
  std::any visitGeneratorExpr(YonaParser::GeneratorExprContext *ctx) override;
  std::any visitSequenceGeneratorExpr(YonaParser::SequenceGeneratorExprContext *ctx) override;
  std::any visitSetGeneratorExpr(YonaParser::SetGeneratorExprContext *ctx) override;
  std::any visitDictGeneratorExpr(YonaParser::DictGeneratorExprContext *ctx) override;
  std::any visitDictGeneratorReducer(YonaParser::DictGeneratorReducerContext *ctx) override;
  std::any visitCollectionExtractor(YonaParser::CollectionExtractorContext *ctx) override;
  std::any visitValueCollectionExtractor(YonaParser::ValueCollectionExtractorContext *ctx) override;
  std::any visitKeyValueCollectionExtractor(YonaParser::KeyValueCollectionExtractorContext *ctx) override;
  std::any visitIdentifierOrUnderscore(YonaParser::IdentifierOrUnderscoreContext *ctx) override;
  std::any visitRecord(YonaParser::RecordContext *ctx) override;
  std::any visitRecordInstance(YonaParser::RecordInstanceContext *ctx) override;
  std::any visitRecordType(YonaParser::RecordTypeContext *ctx) override;
  std::any visitFieldAccessExpr(YonaParser::FieldAccessExprContext *ctx) override;
  std::any visitFieldUpdateExpr(YonaParser::FieldUpdateExprContext *ctx) override;
  std::any visitFunctionDecl(YonaParser::FunctionDeclContext *ctx) override;
  std::any visitType(YonaParser::TypeContext *ctx) override;
  std::any visitTypeDecl(YonaParser::TypeDeclContext *ctx) override;
  std::any visitTypeDef(YonaParser::TypeDefContext *ctx) override;
  std::any visitTypeName(YonaParser::TypeNameContext *ctx) override;
  std::any visitTypeVar(YonaParser::TypeVarContext *ctx) override;
  std::any visitTypeInstance(YonaParser::TypeInstanceContext *ctx) override;
};
} // namespace yona
