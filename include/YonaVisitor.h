#pragma once

#include <iostream>
#include "YonaParserBaseVisitor.h"
#include "ast.h"

namespace yonac {
    using namespace std;
    using namespace ast;

    class YonaVisitor : public YonaParserBaseVisitor {
    private:
        int lambdaCount = 0;

    public:
        unique_ptr<AstNode> visitInput(YonaParser::InputContext* ctx) override {
            return make_unique<AstNode>(visit(ctx->expression()));
        }

        unique_ptr<FunctionExpr> visitFunction(YonaParser::FunctionContext* ctx) override {
            vector<unique_ptr<BodyWithoutGuards>> bodies;
            if (ctx->functionBody()->bodyWithoutGuard() != nullptr) {
                bodies.push_back(visitBodyWithoutGuard(ctx->functionBody()->bodyWithoutGuard()));
            }
            else {
                for (auto& guard : ctx->functionBody()->bodyWithGuards()) {
                    bodies.push_back(visitBodyWithGuards(guard));
                }
            }
            return make_unique<FunctionExpr>(FunctionExpr(
                ctx->functionName()->name()->LOWERCASE_NAME()->getText(),
                visitPatterns(ctx->pattern()),
                bodies));
        }

        unique_ptr<BodyWithoutGuards> visitBodyWithoutGuard(YonaParser::BodyWithoutGuardContext* ctx) override {
            return make_unique<BodyWithoutGuards>(BodyWithoutGuards(visit(ctx->expression())));
        }

        unique_ptr<BodyWithGuards> visitBodyWithGuards(YonaParser::BodyWithGuardsContext* ctx) override {
            return make_unique<BodyWithGuards>(BodyWithGuards(visit(ctx->guard), visit(ctx->expr)));
        }

        unique_ptr<ExprNode> visitNegationExpression(YonaParser::NegationExpressionContext* ctx) override {
            if (ctx->OP_LOGIC_NOT() != nullptr) {
                return make_unique<LogicalNotOpExpr>(LogicalNotOpExpr(visit(ctx->expression())));
            }
            else {
                return make_unique<BinaryNotOpExpr>(BinaryNotOpExpr(visit(ctx->expression())));
            }
        }

        unique_ptr<ExprNode> visitAdditiveExpression(YonaParser::AdditiveExpressionContext* ctx) override {
            if (ctx->OP_PLUS() != nullptr) {
                return make_unique<AddExpr>(AddExpr(visit(ctx->left), visit(ctx->right)));
            }
            else {
                return make_unique<SubtractExpr>(SubtractExpr(visit(ctx->left), visit(ctx->right)));
            }
        }

        unique_ptr<ExprNode> visitPipeRightExpression(YonaParser::PipeRightExpressionContext* ctx) override {
            return make_unique<PipeRightExpr>(PipeRightExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitBinaryShiftExpression(YonaParser::BinaryShiftExpressionContext* ctx) override {
            if (ctx->OP_LEFTSHIFT() != nullptr) {
                return make_unique<LeftShiftExpr>(LeftShiftExpr(visit(ctx->left), visit(ctx->right)));
            }
            else if (ctx->OP_RIGHTSHIFT() != nullptr) {
                return make_unique<RightShiftExpr>(RightShiftExpr(visit(ctx->left), visit(ctx->right)));
            }
            else {
                return make_unique<ZerofillRightShiftExpr>(ZerofillRightShiftExpr(visit(ctx->left), visit(ctx->right)));
            }
        }

        unique_ptr<ApplyExpr> visitFunctionApplicationExpression(YonaParser::FunctionApplicationExpressionContext* ctx) override {
            return visitApply(ctx->apply());
        }

        unique_ptr<ExprNode> visitFieldAccessExpression(YonaParser::FieldAccessExpressionContext* ctx) override {
            return visitFieldAccessExpr(ctx->fieldAccessExpr());
        }

        unique_ptr<ExprNode> visitBacktickExpression(YonaParser::BacktickExpressionContext* ctx) override {
            return make_unique<ApplyExpr>(ApplyExpr(visitCall(ctx->call()), vector<unique_ptr<ExprNode>>{ visit(ctx->left), visit(ctx->right) }));
        }

        unique_ptr<ExprNode> visitCaseExpression(YonaParser::CaseExpressionContext* ctx) override {
            return visitCaseExpr(ctx->caseExpr());
        }

        unique_ptr<ExprNode> visitTryCatchExpression(YonaParser::TryCatchExpressionContext* ctx) override {
            return visitTryCatchExpr(ctx->tryCatchExpr());
        }

        unique_ptr<ExprNode> visitBitwiseAndExpression(YonaParser::BitwiseAndExpressionContext* ctx) override {
            return make_unique<BitwiseAndExpr>(BitwiseAndExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitLetExpression(YonaParser::LetExpressionContext* ctx) override {
            return visitLet(ctx->let());
        }

        unique_ptr<ExprNode> visitDoExpression(YonaParser::DoExpressionContext* ctx) override {
            return visitDoExpr(ctx->doExpr());
        }

        unique_ptr<ExprNode> visitLogicalAndExpression(YonaParser::LogicalAndExpressionContext* ctx) override {
            return make_unique<LogicalAndExpr>(LogicalAndExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitConsRightExpression(YonaParser::ConsRightExpressionContext* ctx) override {
            return make_unique<ConsRightExpr>(ConsRightExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitExpressionInParents(YonaParser::ExpressionInParentsContext* ctx) override {
            return visit(ctx->expression());
        }

        unique_ptr<ExprNode> visitConsLeftExpression(YonaParser::ConsLeftExpressionContext* ctx) override {
            return make_unique<ConsLeftExpr>(ConsLeftExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitBitwiseXorExpression(YonaParser::BitwiseXorExpressionContext* ctx) override {
            return make_unique<BitwiseXorExpr>(BitwiseXorExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitGeneratorExpression(YonaParser::GeneratorExpressionContext* ctx) override {
            return visitGeneratorExpr(ctx->generatorExpr());
        }

        unique_ptr<ExprNode> visitConditionalExpression(YonaParser::ConditionalExpressionContext* ctx) override {
            return visitConditional(ctx->conditional());
        }

        unique_ptr<ExprNode> visitMultiplicativeExpression(YonaParser::MultiplicativeExpressionContext* ctx) override {
            if (ctx->OP_POWER() != nullptr) {
                return make_unique<PowerExpr>(PowerExpr(visit(ctx->left), visit(ctx->right)));
            }
            else if (ctx->OP_MULTIPLY() != nullptr) {
                return make_unique<MultiplyExpr>(MultiplyExpr(visit(ctx->left), visit(ctx->right)));
            }
            else if (ctx->OP_DIVIDE() != nullptr) {
                return make_unique<DivideExpr>(DivideExpr(visit(ctx->left), visit(ctx->right)));
            }
            else {
                return make_unique<ModuloExpr>(ModuloExpr(visit(ctx->left), visit(ctx->right)));
            }
        }

        unique_ptr<ExprNode> visitLogicalOrExpression(YonaParser::LogicalOrExpressionContext* ctx) override {
            return make_unique<LogicalOrExpr>(LogicalOrExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitPipeLeftExpression(YonaParser::PipeLeftExpressionContext* ctx) override {
            return make_unique<PipeLeftExpr>(PipeLeftExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitComparativeExpression(YonaParser::ComparativeExpressionContext* ctx) override {
            if (ctx->OP_GTE() != nullptr) {
                return make_unique<GteExpr>(GteExpr(visit(ctx->left), visit(ctx->right)));
            }
            else if (ctx->OP_LTE() != nullptr) {
                return make_unique<LteExpr>(LteExpr(visit(ctx->left), visit(ctx->right)));
            }
            else if (ctx->OP_GT() != nullptr) {
                return make_unique<GtExpr>(GtExpr(visit(ctx->left), visit(ctx->right)));
            }
            else if (ctx->OP_LT() != nullptr) {
                return make_unique<LtExpr>(LtExpr(visit(ctx->left), visit(ctx->right)));
            }
            else if (ctx->OP_EQ() != nullptr) {
                return make_unique<EqExpr>(EqExpr(visit(ctx->left), visit(ctx->right)));
            }
            else {
                return make_unique<NeqExpr>(NeqExpr(visit(ctx->left), visit(ctx->right)));
            }
        }

        unique_ptr<ExprNode> visitBitwiseOrExpression(YonaParser::BitwiseOrExpressionContext* ctx) override {
            return make_unique<BitwiseOrExpr>(BitwiseOrExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitInExpression(YonaParser::InExpressionContext* ctx) override {
            return make_unique<InExpr>(InExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitRaiseExpression(YonaParser::RaiseExpressionContext* ctx) override {
            return visitRaiseExpr(ctx->raiseExpr());
        }

        unique_ptr<ExprNode> visitWithExpression(YonaParser::WithExpressionContext* ctx) override {
            return visitWithExpr(ctx->withExpr());
        }

        unique_ptr<ExprNode> visitFieldUpdateExpression(YonaParser::FieldUpdateExpressionContext* ctx) override {
            return visitFieldUpdateExpr(ctx->fieldUpdateExpr());
        }

        unique_ptr<ExprNode> visitJoinExpression(YonaParser::JoinExpressionContext* ctx) override {
            return make_unique<JoinExpr>(JoinExpr(visit(ctx->left), visit(ctx->right)));
        }

        unique_ptr<ExprNode> visitImportExpression(YonaParser::ImportExpressionContext* ctx) override {
            return visitImportExpr(ctx->importExpr());
        }

        unique_ptr<PatternValue> visitPatternValue(YonaParser::PatternValueContext* ctx) override {
            if (ctx->unit() != nullptr) {
                return make_unique<PatternValue>(PatternValue(UnitExpr()));
            }
            else if (ctx->literal() != nullptr) {
                return make_unique<PatternValue>(PatternValue(dynamic_pointer_cast<LiteralExpr<void>>(visit(ctx->literal()))));
            }
            else if (ctx->symbol() != nullptr) {
                return make_unique<PatternValue>(PatternValue(visitSymbol(ctx->symbol())));
            }
            else {
                return make_unique<PatternValue>(PatternValue(visitIdentifier(ctx->identifier())));
            }
        }

        unique_ptr<NameExpr> visitName(YonaParser::NameContext* ctx) override {
            return make_unique<NameExpr>(NameExpr(ctx->LOWERCASE_NAME()->getText()));
        }

        unique_ptr<LetExpr> visitLet(YonaParser::LetContext* ctx) override {
            vector<unique_ptr<AliasExpr>> aliases;
            for (auto& alias : ctx->alias()) {
                aliases.push_back(visitAlias(alias));
            }
            return make_unique<LetExpr>(LetExpr(aliases, visit(ctx->expression())));
        }

        unique_ptr<AliasExpr> visitAlias(YonaParser::AliasContext* ctx) override {
            if (ctx->lambdaAlias() != nullptr) {
                return visitLambdaAlias(ctx->lambdaAlias());
            }
            else if (ctx->moduleAlias() != nullptr) {
                return visitModuleAlias(ctx->moduleAlias());
            }
            else if (ctx->valueAlias() != nullptr) {
                return visitValueAlias(ctx->valueAlias());
            }
            else if (ctx->patternAlias() != nullptr) {
                return visitPatternAlias(ctx->patternAlias());
            }
            else {
                return visitFqnAlias(ctx->fqnAlias());
            }
        }

        unique_ptr<LambdaAlias> visitLambdaAlias(YonaParser::LambdaAliasContext* ctx) override {
            return make_unique<LambdaAlias>(LambdaAlias(visitName(ctx->name()), visitLambda(ctx->lambda())));
        }

        unique_ptr<ModuleAlias> visitModuleAlias(YonaParser::ModuleAliasContext* ctx) override {
            return make_unique<ModuleAlias>(ModuleAlias(visitName(ctx->name()), visitModule(ctx->module())));
        }

        unique_ptr<ValueAlias> visitValueAlias(YonaParser::ValueAliasContext* ctx) override {
            return make_unique<ValueAlias>(ValueAlias(visitIdentifier(ctx->identifier()), visit(ctx->expression())));
        }

        unique_ptr<PatternAlias> visitPatternAlias(YonaParser::PatternAliasContext* ctx) override {
            return make_unique<PatternAlias>(PatternAlias(visitPattern(ctx->pattern()), visit(ctx->expression())));
        }

        unique_ptr<FqnAlias> visitFqnAlias(YonaParser::FqnAliasContext* ctx) override {
            return make_unique<FqnAlias>(FqnAlias(visitName(ctx->name()), visitFqn(ctx->fqn())));
        }

        unique_ptr<IfExpr> visitConditional(YonaParser::ConditionalContext* ctx) override {
            unique_ptr<ExprNode> elseExpr = nullptr;
            if (ctx->expression().size() > 2) {
                elseExpr = visit(ctx->expression(2));
            }
            return make_unique<IfExpr>(IfExpr(visit(ctx->expression(0)), visit(ctx->expression(1)), elseExpr));
        }

        unique_ptr<ApplyExpr> visitApply(YonaParser::ApplyContext* ctx) override {
            vector<unique_ptr<ExprNode>> args;
            for (auto& arg : ctx->funArg()) {
                args.push_back(visitFunArg(arg));
            }
            return make_unique<ApplyExpr>(ApplyExpr(visitCall(ctx->call()), args));
        }

        unique_ptr<ExprNode> visitFunArg(YonaParser::FunArgContext* ctx) override {
            if (ctx->value() != nullptr) {
                return visit(ctx->value());
            }
            else {
                return visit(ctx->expression());
            }
        }

        unique_ptr<CallExpr> visitCall(YonaParser::CallContext* ctx) override {
            if (ctx->name() != nullptr) {
                return make_unique<NameCall>(visitName(ctx->name()));
            }
            else if (ctx->nameCall() != nullptr) {
                return visitNameCall(ctx->nameCall());
            }
            else {
                return visitModuleCall(ctx->moduleCall());
            }
        }

        unique_ptr<ModuleCall> visitModuleCall(YonaParser::ModuleCallContext* ctx) override {
            if (ctx->fqn() != nullptr) {
                return make_unique<ModuleCall>(ModuleCall(visitFqn(ctx->fqn()), visitName(ctx->name())));
            }
            else {
                return make_unique<ModuleCall>(ModuleCall(visit(ctx->expression()), visitName(ctx->name())));
            }
        }

        unique_ptr<NameCall> visitNameCall(YonaParser::NameCallContext* ctx) override {
            return make_unique<NameCall>(NameCall(visitName(ctx->varName), visitName(ctx->funName)));
        }

        unique_ptr<ModuleExpr> visitModule(YonaParser::ModuleContext* ctx) override {
            vector<string> names;
            for (auto& name : ctx->nonEmptyListOfNames()->name()) {
                names.push_back(name->LOWERCASE_NAME()->getText());
            }
            vector<unique_ptr<RecordExpr>> records;
            for (auto& record : ctx->record()) {
                records.push_back(visitRecord(record));
            }
            vector<unique_ptr<FunctionExpr>> functions;
            for (auto& function : ctx->function()) {
                functions.push_back(visitFunction(function));
            }
            return make_unique<ModuleExpr>(ModuleExpr(visitFqn(ctx->fqn()), names, records, functions));
        }

        unique_ptr<ExprNode> visitNonEmptyListOfNames(YonaParser::NonEmptyListOfNamesContext* ctx) override {
            return YonaParserBaseVisitor::visitNonEmptyListOfNames(ctx);
        }

        unique_ptr<UnitExpr> visitUnit(YonaParser::UnitContext* ctx) override {
            return make_unique<UnitExpr>(UnitExpr());
        }

        unique_ptr<ByteExpr> visitByteLiteral(YonaParser::ByteLiteralContext* ctx) override {
            return make_unique<ByteExpr>(ByteExpr(stoi(ctx->BYTE()->getText())));
        }

        unique_ptr<FloatExpr> visitFloatLiteral(YonaParser::FloatLiteralContext* ctx) override {
            return make_unique<FloatExpr>(FloatExpr(stof(ctx->FLOAT()->getText())));
        }

        unique_ptr<IntegerExpr> visitIntegerLiteral(YonaParser::IntegerLiteralContext* ctx) override {
            return make_unique<IntegerExpr>(IntegerExpr(stoi(ctx->INTEGER()->getText())));
        }

        unique_ptr<StringExpr> visitStringLiteral(YonaParser::StringLiteralContext* ctx) override {
            return make_unique<StringExpr>(StringExpr(ctx->getText()));
        }

        unique_ptr<ExprNode> visitInterpolatedStringPart(YonaParser::InterpolatedStringPartContext* ctx) override {
            return YonaParserBaseVisitor::visitInterpolatedStringPart(ctx);
        }

        unique_ptr<CharacterExpr> visitCharacterLiteral(YonaParser::CharacterLiteralContext* ctx) override {
            return make_unique<CharacterExpr>(CharacterExpr(ctx->CHARACTER_LITERAL()->getText()[0]));
        }

        unique_ptr<ExprNode> visitBooleanLiteral(YonaParser::BooleanLiteralContext* ctx) override {
            if (ctx->KW_TRUE() != nullptr) {
                return make_unique<TrueLiteralExpr>(TrueLiteralExpr());
            }
            else {
                return make_unique<FalseLiteralExpr>(FalseLiteralExpr());
            }
        }

        unique_ptr<TupleExpr> visitTuple(YonaParser::TupleContext* ctx) override {
            vector<unique_ptr<ExprNode>> elements;
            for (auto& expr : ctx->expression()) {
                elements.push_back(visit(expr));
            }
            return make_unique<TupleExpr>(TupleExpr(elements));
        }

        unique_ptr<DictExpr> visitDict(YonaParser::DictContext* ctx) override {
            unordered_map<unique_ptr<ExprNode>, unique_ptr<ExprNode>> elements;
            for (size_t i = 0; i < ctx->dictKey().size(); ++i) {
                elements[visit(ctx->dictKey(i))] = visit(ctx->dictVal(i));
            }
            return make_unique<DictExpr>(DictExpr(elements);)
        }

        unique_ptr<SequenceExpr> visitSequence(YonaParser::SequenceContext* ctx) override {
            if (ctx->emptySequence() != nullptr) {
                return visitEmptySequence(ctx->emptySequence());
            }
            else if (ctx->otherSequence() != nullptr) {
                return visitOtherSequence(ctx->otherSequence());
            }
            else {
                return visitRangeSequence(ctx->rangeSequence());
            }
        }

        unique_ptr<SetExpr> visitSet(YonaParser::SetContext* ctx) override {
            vector<unique_ptr<ExprNode>> elements;
            for (auto& expr : ctx->expression()) {
                elements.push_back(visit(expr));
            }
            return make_unique<SetExpr>(SetExpr(elements));
        }

        unique_ptr<FqnExpr> visitFqn(YonaParser::FqnContext* ctx) override {
            return make_unique<FqnExpr>(FqnExpr(visitPackageName(ctx->packageName()), visitModuleName(ctx->moduleName())));
        }

        unique_ptr<PackageNameExpr> visitPackageName(YonaParser::PackageNameContext* ctx) override {
            vector<unique_ptr<NameExpr>> names;
            for (auto& name : ctx->LOWERCASE_NAME()) {
                names.push_back(make_unique<NameExpr>(name->getText()));
            }
            return make_unique<PackageNameExpr>(PackageNameExpr(names));
        }

        unique_ptr<NameExpr> visitModuleName(YonaParser::ModuleNameContext* ctx) override {
            return make_unique<NameExpr>(NameExpr(ctx->UPPERCASE_NAME()->getText()));
        }

        unique_ptr<SymbolExpr> visitSymbol(YonaParser::SymbolContext* ctx) override {
            return make_unique<SymbolExpr>(SymbolExpr(ctx->SYMBOL()->getText()));
        }

        unique_ptr<IdentifierExpr> visitIdentifier(YonaParser::IdentifierContext* ctx) override {
            return make_unique<IdentifierExpr>(IdentifierExpr(visitName(ctx->name())));
        }

        unique_ptr<FunctionExpr> visitLambda(YonaParser::LambdaContext* ctx) override {
            return make_unique<FunctionExpr>(FunctionExpr(nextLambdaName(), visitPatterns(ctx->pattern()), vector<unique_ptr<BodyWithoutGuards>>{ make_unique<BodyWithoutGuards>(visit(ctx->expression())) }));
        }

        unique_ptr<UnderscorePattern> visitUnderscore(YonaParser::UnderscoreContext* ctx) override {
            return make_unique<UnderscorePattern>(UnderscorePattern());
        }

        unique_ptr<ValuesSequenceExpr> visitEmptySequence(YonaParser::EmptySequenceContext* ctx) override {
            return make_unique<ValuesSequenceExpr>(ValuesSequenceExpr(vector<unique_ptr<ExprNode>>()));
        }

        unique_ptr<ValuesSequenceExpr> visitOtherSequence(YonaParser::OtherSequenceContext* ctx) override {
            vector<unique_ptr<ExprNode>> elements;
            for (auto& expr : ctx->expression()) {
                elements.push_back(visit(expr));
            }
            return make_unique<ValuesSequenceExpr>(ValuesSequenceExpr(elements));
        }

        unique_ptr<RangeSequenceExpr> visitRangeSequence(YonaParser::RangeSequenceContext* ctx) override {
            return make_unique<RangeSequenceExpr>(RangeSequenceExpr(visit(ctx->start), visit(ctx->end), visit(ctx->step)));
        }

        unique_ptr<CaseExpr> visitCaseExpr(YonaParser::CaseExprContext* ctx) override {
            vector<unique_ptr<PatternExpr>> patterns;
            for (auto& patternExpr : ctx->patternExpression()) {
                patterns.push_back(visitPatternExpression(patternExpr));
            }
            return make_unique<CaseExpr>(CaseExpr(visit(ctx->expression()), patterns));
        }

        unique_ptr<PatternExpr> visitPatternExpression(YonaParser::PatternExpressionContext* ctx) override {
            if (ctx->patternExpressionWithoutGuard() != nullptr) {
                return make_unique<PatternExpr>(PatternExpr(visitPattern(ctx->pattern()), visitPatternExpressionWithoutGuard(ctx->patternExpressionWithoutGuard())));
            }
            else {
                vector<unique_ptr<PatternWithGuards>> guards;
                for (auto& guard : ctx->patternExpressionWithGuard()) {
                    guards.push_back(visitPatternExpressionWithGuard(guard));
                }
                return make_unique<PatternExpr>(PatternExpr(visitPattern(ctx->pattern()), guards));
            }
        }

        unique_ptr<DoExpr> visitDoExpr(YonaParser::DoExprContext* ctx) override {
            vector<unique_ptr<AliasExpr>> steps;
            for (auto& step : ctx->doOneStep()) {
                steps.push_back(visitDoOneStep(step));
            }
            return make_unique<DoExpr>(DoExpr(steps));
        }

        unique_ptr<AliasExpr> visitDoOneStep(YonaParser::DoOneStepContext* ctx) override {
            if (ctx->alias() != nullptr) {
                return visitAlias(ctx->alias());
            }
            else {
                return visit(AliasExpr(ctx->expression()));
            }
        }

        unique_ptr<PatternWithoutGuards> visitPatternExpressionWithoutGuard(YonaParser::PatternExpressionWithoutGuardContext* ctx) override {
            return make_unique<PatternWithoutGuards>(PatternWithoutGuards(visit(ctx->expression())));
        }

        unique_ptr<PatternWithGuards> visitPatternExpressionWithGuard(YonaParser::PatternExpressionWithGuardContext* ctx) override {
            return make_unique<PatternWithGuards>(PatternWithGuards(visit(ctx->guard), visit(ctx->expr)));
        }

        unique_ptr<Pattern> visitPattern(YonaParser::PatternContext* ctx) override {
            if (ctx->pattern() != nullptr) {
                return visitPattern(ctx->pattern());
            }
            else if (ctx->underscore() != nullptr) {
                return make_unique<UnderscorePattern>();
            }
            else if (ctx->patternValue() != nullptr) {
                return visitPatternValue(ctx->patternValue());
            }
            else if (ctx->dataStructurePattern() != nullptr) {
                return visitDataStructurePattern(ctx->dataStructurePattern());
            }
            else {
                return visitAsDataStructurePattern(ctx->asDataStructurePattern());
            }
        }

        unique_ptr<DataStructurePattern> visitDataStructurePattern(YonaParser::DataStructurePatternContext* ctx) override {
            if (ctx->tuplePattern() != nullptr) {
                return visitTuplePattern(ctx->tuplePattern());
            }
            else if (ctx->sequencePattern() != nullptr) {
                return visitSequencePattern(ctx->sequencePattern());
            }
            else if (ctx->dictPattern() != nullptr) {
                return visitDictPattern(ctx->dictPattern());
            }
            else {
                return visitRecordPattern(ctx->recordPattern());
            }
        }

        unique_ptr<AsDataStructurePattern> visitAsDataStructurePattern(YonaParser::AsDataStructurePatternContext* ctx) override {
            return make_unique<AsDataStructurePattern>(AsDataStructurePattern(visitIdentifier(ctx->identifier()), visitDataStructurePattern(ctx->dataStructurePattern())));
        }

        unique_ptr<PatternWithoutSequence> visitPatternWithoutSequence(YonaParser::PatternWithoutSequenceContext* ctx) override {
            if (ctx->underscore() != nullptr) {
                return make_unique<UnderscorePattern>(UnderscorePattern());
            }
            else if (ctx->patternValue() != nullptr) {
                return visitPatternValue(ctx->patternValue());
            }
            else if (ctx->tuplePattern() != nullptr) {
                return visitTuplePattern(ctx->tuplePattern());
            }
            else {
                return visitDictPattern(ctx->dictPattern());
            }
        }

        unique_ptr<TuplePattern> visitTuplePattern(YonaParser::TuplePatternContext* ctx) override {
            vector<unique_ptr<Pattern>> patterns;
            for (auto& pattern : ctx->pattern()) {
                patterns.push_back(visitPattern(pattern));
            }
            return make_unique<TuplePattern>(TuplePattern(patterns));
        }

        unique_ptr<SequencePattern> visitSequencePattern(YonaParser::SequencePatternContext* ctx) override {
            if (ctx->pattern() != nullptr) {
                vector<unique_ptr<Pattern>> patterns;
                for (auto& pattern : ctx->pattern()) {
                    patterns.push_back(visitPattern(pattern));
                }
                return make_unique<SeqPattern>(SeqPattern(patterns));
            }
            else if (ctx->headTails() != nullptr) {
                return visitHeadTails(ctx->headTails());
            }
            else if (ctx->tailsHead() != nullptr) {
                return visitTailsHead(ctx->tailsHead());
            }
            else {
                return visitHeadTailsHead(ctx->headTailsHead());
            }
        }

        unique_ptr<HeadTailsPattern> visitHeadTails(YonaParser::HeadTailsContext* ctx) override {
            vector<unique_ptr<PatternWithoutSequence>> headPatterns;
            for (auto& pattern : ctx->patternWithoutSequence()) {
                headPatterns.push_back(visitPatternWithoutSequence(pattern));
            }
            return make_unique<HeadTailsPattern>(HeadTailsPattern(headPatterns, visitTails(ctx->tails())));
        }

        unique_ptr<TailsHeadPattern> visitTailsHead(YonaParser::TailsHeadContext* ctx) override {
            vector<unique_ptr<PatternWithoutSequence>> headPatterns;
            for (auto& pattern : ctx->patternWithoutSequence()) {
                headPatterns.push_back(visitPatternWithoutSequence(pattern));
            }
            return make_unique<TailsHeadPattern>(TailsHeadPattern(visitTails(ctx->tails()), headPatterns));
        }

        unique_ptr<HeadTailsHeadPattern> visitHeadTailsHead(YonaParser::HeadTailsHeadContext* ctx) override {
            vector<unique_ptr<PatternWithoutSequence>> leftPatterns;
            for (auto& pattern : ctx->leftPattern()) {
                leftPatterns.push_back(visitLeftPattern(pattern));
            }
            vector<unique_ptr<PatternWithoutSequence>> rightPatterns;
            for (auto& pattern : ctx->rightPattern()) {
                rightPatterns.push_back(visitRightPattern(pattern));
            }
            return make_unique<HeadTailsHeadPattern>(HeadTailsHeadPattern(leftPatterns, visitTails(ctx->tails()), rightPatterns));
        }

        unique_ptr<PatternWithoutSequence> visitLeftPattern(YonaParser::LeftPatternContext* ctx) override {
            return visitPatternWithoutSequence(PatternWithoutSequence(ctx->patternWithoutSequence()));
        }

        unique_ptr<PatternWithoutSequence> visitRightPattern(YonaParser::RightPatternContext* ctx) override {
            return visitPatternWithoutSequence(PatternWithoutSequence(ctx->patternWithoutSequence()));
        }

        unique_ptr<TailPattern> visitTails(YonaParser::TailsContext* ctx) override {
            if (ctx->identifier() != nullptr) {
                return visitIdentifier(ctx->identifier());
            }
            else if (ctx->sequence() != nullptr) {
                return visitSequence(ctx->sequence());
            }
            else if (ctx->underscore() != nullptr) {
                return make_unique<UnderscorePattern>(UnderscorePattern());
            }
            else {
                return visitStringLiteral(ctx->stringLiteral());
            }
        }

        unique_ptr<DictPattern> visitDictPattern(YonaParser::DictPatternContext* ctx) override {
            unordered_map<unique_ptr<PatternValue>, unique_ptr<Pattern>> elements;
            for (size_t i = 0; i < ctx->patternValue().size(); ++i) {
                elements[visitPatternValue(ctx->patternValue(i))] = visitPattern(ctx->pattern(i));
            }
            return make_unique<DictPattern>(DictPattern(elements));
        }

        unique_ptr<RecordPattern> visitRecordPattern(YonaParser::RecordPatternContext* ctx) override {
            unordered_map<unique_ptr<NameExpr>, unique_ptr<Pattern>> elements;
            for (size_t i = 0; i < ctx->name().size(); ++i) {
                elements[visitName(ctx->name(i))] = visitPattern(ctx->pattern(i));
            }
            return make_unique<RecordPattern>(RecordPattern(ctx->recordType()->UPPERCASE_NAME()->getText(), elements));
        }

        unique_ptr<ImportExpr> visitImportExpr(YonaParser::ImportExprContext* ctx) override {
            vector<unique_ptr<ImportClauseExpr>> clauses;
            for (auto& clause : ctx->importClause()) {
                clauses.push_back(visitImportClause(clause));
            }
            return make_unique<ImportExpr>(ImportExpr(clauses, visit(ctx->expression())));
        }

        unique_ptr<ImportClauseExpr> visitImportClause(YonaParser::ImportClauseContext* ctx) override {
            if (ctx->moduleImport() != nullptr) {
                return visitModuleImport(ctx->moduleImport());
            }
            else {
                return visitFunctionsImport(ctx->functionsImport());
            }
        }

        unique_ptr<ModuleImport> visitModuleImport(YonaParser::ModuleImportContext* ctx) override {
            return make_unique<ModuleImport>(ModuleImport(visitFqn(ctx->fqn()), visitName(ctx->name())));
        }

        unique_ptr<FunctionsImport> visitFunctionsImport(YonaParser::FunctionsImportContext* ctx) override {
            vector<unique_ptr<FunctionAlias>> functionAliases;
            for (auto& alias : ctx->functionAlias()) {
                functionAliases.push_back(visitFunctionAlias(alias));
            }
            return make_unique<FunctionsImport>(FunctionsImport(functionAliases, visitFqn(ctx->fqn())));
        }

        unique_ptr<FunctionAlias> visitFunctionAlias(YonaParser::FunctionAliasContext* ctx) override {
            return make_unique<FunctionAlias>(FunctionAlias(visitName(ctx->funName), visitName(ctx->funAlias)));
        }

        string nextLambdaName() {
            return "lambda_" + to_string(lambdaCount++);
        }

        vector<unique_ptr<Pattern>> visitPatterns(const vector<YonaParser::PatternContext*>& patterns) {
            vector<unique_ptr<Pattern>> result;
            for (auto& pattern : patterns) {
                result.push_back(visitPattern(pattern));
            }
            return result;
        }
    };
}
