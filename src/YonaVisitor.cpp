//
// Created by akovari on 16.11.24.
//
#include "YonaVisitor.h"

#include <boost/algorithm/string.hpp>
#include <boost/proto/make_expr.hpp>

#include "utils.h"

namespace yona {
string YonaVisitor::nextLambdaName() { return fqn() + "_lambda_" + to_string(lambda_count_++); }

any YonaVisitor::visitInput(YonaParser::InputContext *ctx) { return wrap_expr<MainNode>(*ctx, visit_expr<ExprNode>(ctx->expression())); }

any YonaVisitor::visitFunction(YonaParser::FunctionContext *ctx) {
  vector<FunctionBody *> bodies;
  if (ctx->functionBody()->bodyWithoutGuard() != nullptr) {
    bodies.push_back(visit_expr<BodyWithoutGuards>(ctx->functionBody()->bodyWithoutGuard()));
  } else {
    for (const auto &guard : ctx->functionBody()->bodyWithGuards()) {
      bodies.push_back(visit_expr<BodyWithGuards>(guard));
    }
  }

  string function_name = ctx->functionName()->name()->LOWERCASE_NAME()->getText();
  names_.push_back(function_name);
  auto result = wrap_expr<FunctionExpr>(*ctx, function_name, visit_exprs<PatternNode>(ctx->pattern()), bodies);
  names_.pop_back();

  return result;
}

std::any YonaVisitor::visitFunctionName(YonaParser::FunctionNameContext *ctx) { return visitName(ctx->name()); }

any YonaVisitor::visitBodyWithoutGuard(YonaParser::BodyWithoutGuardContext *ctx) {
  return wrap_expr<BodyWithoutGuards>(*ctx, visit_expr<ExprNode>(ctx->expression()));
}

any YonaVisitor::visitBodyWithGuards(YonaParser::BodyWithGuardsContext *ctx) {
  return wrap_expr<BodyWithGuards>(*ctx, visit_expr<ExprNode>(ctx->guard), visit_expr<ExprNode>(ctx->expr));
}

any YonaVisitor::visitNegationExpression(YonaParser::NegationExpressionContext *ctx) {
  if (ctx->OP_LOGIC_NOT() != nullptr) {
    return wrap_expr<LogicalNotOpExpr>(*ctx, visit_expr<ExprNode>(ctx->expression()));
  } else {
    return wrap_expr<BinaryNotOpExpr>(*ctx, visit_expr<ExprNode>(ctx->expression()));
  }
}
std::any YonaVisitor::visitValueExpression(YonaParser::ValueExpressionContext *ctx) { return visitValue(ctx->value()); }

any YonaVisitor::visitAdditiveExpression(YonaParser::AdditiveExpressionContext *ctx) {
  if (ctx->OP_PLUS() != nullptr) {
    return wrap_expr<AddExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else {
    return wrap_expr<SubtractExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  }
}

any YonaVisitor::visitBinaryShiftExpression(YonaParser::BinaryShiftExpressionContext *ctx) {
  if (ctx->OP_LEFTSHIFT() != nullptr) {
    return wrap_expr<LeftShiftExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else if (ctx->OP_RIGHTSHIFT() != nullptr) {
    return wrap_expr<RightShiftExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else {
    return wrap_expr<ZerofillRightShiftExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  }
}

any YonaVisitor::visitFunctionApplicationExpression(YonaParser::FunctionApplicationExpressionContext *ctx) { return visitApply(ctx->apply()); }

any YonaVisitor::visitFieldAccessExpression(YonaParser::FieldAccessExpressionContext *ctx) { return visit(ctx->fieldAccessExpr()); }

any YonaVisitor::visitBacktickExpression(YonaParser::BacktickExpressionContext *ctx) {
  return wrap_expr<ApplyExpr>(
      *ctx, visit_expr<CallExpr>(ctx->call()),
      vector{any_cast<variant<ExprNode *, ValueExpr *>>(visit(ctx->left)), any_cast<variant<ExprNode *, ValueExpr *>>(visit(ctx->right))});
}

any YonaVisitor::visitCaseExpression(YonaParser::CaseExpressionContext *ctx) { return visit(ctx->caseExpr()); }

any YonaVisitor::visitTryCatchExpression(YonaParser::TryCatchExpressionContext *ctx) { return visit(ctx->tryCatchExpr()); }

any YonaVisitor::visitBitwiseAndExpression(YonaParser::BitwiseAndExpressionContext *ctx) {
  return wrap_expr<BitwiseAndExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
}

any YonaVisitor::visitLetExpression(YonaParser::LetExpressionContext *ctx) { return visitLet(ctx->let()); }

any YonaVisitor::visitDoExpression(YonaParser::DoExpressionContext *ctx) { return visit(ctx->doExpr()); }

any YonaVisitor::visitLogicalAndExpression(YonaParser::LogicalAndExpressionContext *ctx) {
  return wrap_expr<LogicalAndExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
}

any YonaVisitor::visitConsRightExpression(YonaParser::ConsRightExpressionContext *ctx) {
  return wrap_expr<ConsRightExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
}

any YonaVisitor::visitExpressionInParents(YonaParser::ExpressionInParentsContext *ctx) { return visit(ctx->expression()); }

any YonaVisitor::visitConsLeftExpression(YonaParser::ConsLeftExpressionContext *ctx) {
  return wrap_expr<ConsLeftExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
}

any YonaVisitor::visitBitwiseXorExpression(YonaParser::BitwiseXorExpressionContext *ctx) {
  return wrap_expr<BitwiseXorExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
}

any YonaVisitor::visitGeneratorExpression(YonaParser::GeneratorExpressionContext *ctx) { return visit(ctx->generatorExpr()); }

any YonaVisitor::visitConditionalExpression(YonaParser::ConditionalExpressionContext *ctx) { return visitConditional(ctx->conditional()); }

any YonaVisitor::visitMultiplicativeExpression(YonaParser::MultiplicativeExpressionContext *ctx) {
  if (ctx->OP_POWER() != nullptr) {
    return wrap_expr<PowerExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else if (ctx->OP_MULTIPLY() != nullptr) {
    return wrap_expr<MultiplyExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else if (ctx->OP_DIVIDE() != nullptr) {
    return wrap_expr<DivideExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else {
    return wrap_expr<ModuloExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  }
}

any YonaVisitor::visitLogicalOrExpression(YonaParser::LogicalOrExpressionContext *ctx) {
  return wrap_expr<LogicalOrExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
}

any YonaVisitor::visitPipeLeftExpression(YonaParser::PipeLeftExpressionContext *ctx) {
  return wrap_expr<ApplyExpr>(*ctx, visit_expr<CallExpr>(ctx->left), vector<variant<ExprNode *, ValueExpr *>>{visit_expr<ExprNode>(ctx->right)});
}

any YonaVisitor::visitPipeRightExpression(YonaParser::PipeRightExpressionContext *ctx) {
  return wrap_expr<ApplyExpr>(*ctx, visit_expr<CallExpr>(ctx->right), vector<variant<ExprNode *, ValueExpr *>>{visit_expr<ExprNode>(ctx->left)});
}

any YonaVisitor::visitComparativeExpression(YonaParser::ComparativeExpressionContext *ctx) {
  if (ctx->OP_GTE() != nullptr) {
    return wrap_expr<GteExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else if (ctx->OP_LTE() != nullptr) {
    return wrap_expr<LteExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else if (ctx->OP_GT() != nullptr) {
    return wrap_expr<GtExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else if (ctx->OP_LT() != nullptr) {
    return wrap_expr<LtExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else if (ctx->OP_EQ() != nullptr) {
    return wrap_expr<EqExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  } else {
    return wrap_expr<NeqExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
  }
}

any YonaVisitor::visitBitwiseOrExpression(YonaParser::BitwiseOrExpressionContext *ctx) {
  return wrap_expr<BitwiseOrExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
}

any YonaVisitor::visitInExpression(YonaParser::InExpressionContext *ctx) {
  return wrap_expr<InExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
}

any YonaVisitor::visitRaiseExpression(YonaParser::RaiseExpressionContext *ctx) { return visit(ctx->raiseExpr()); }

any YonaVisitor::visitWithExpression(YonaParser::WithExpressionContext *ctx) { return visit(ctx->withExpr()); }

any YonaVisitor::visitFieldUpdateExpression(YonaParser::FieldUpdateExpressionContext *ctx) { return visitFieldUpdateExpr(ctx->fieldUpdateExpr()); }

any YonaVisitor::visitJoinExpression(YonaParser::JoinExpressionContext *ctx) {
  return wrap_expr<JoinExpr>(*ctx, visit_expr<ExprNode>(ctx->left), visit_expr<ExprNode>(ctx->right));
}

any YonaVisitor::visitImportExpression(YonaParser::ImportExpressionContext *ctx) { return visit(ctx->importExpr()); }
std::any YonaVisitor::visitLiteral(YonaParser::LiteralContext *ctx) {
  if (ctx->booleanLiteral() != nullptr) {
    return visitBooleanLiteral(ctx->booleanLiteral());
  } else if (ctx->integerLiteral() != nullptr) {
    return visitIntegerLiteral(ctx->integerLiteral());
  } else if (ctx->floatLiteral() != nullptr) {
    return visitFloatLiteral(ctx->floatLiteral());
  } else if (ctx->byteLiteral() != nullptr) {
    return visitByteLiteral(ctx->byteLiteral());
  } else if (ctx->stringLiteral() != nullptr) {
    return visitStringLiteral(ctx->stringLiteral());
  } else {
    return visitCharacterLiteral(ctx->characterLiteral());
  }
}
std::any YonaVisitor::visitValue(YonaParser::ValueContext *ctx) {
  if (ctx->unit() != nullptr) {
    return visitUnit(ctx->unit());
  } else if (ctx->literal() != nullptr) {
    return visitLiteral(ctx->literal());
  } else if (ctx->tuple() != nullptr) {
    return visitTuple(ctx->tuple());
  } else if (ctx->dict() != nullptr) {
    return visitDict(ctx->dict());
  } else if (ctx->sequence() != nullptr) {
    return visitSequence(ctx->sequence());
  } else if (ctx->set() != nullptr) {
    return visitSet(ctx->set());
  } else if (ctx->symbol() != nullptr) {
    return visitSymbol(ctx->symbol());
  } else if (ctx->identifier() != nullptr) {
    return visitIdentifier(ctx->identifier());
  } else if (ctx->fqn() != nullptr) {
    return visitFqn(ctx->fqn());
  } else if (ctx->lambda() != nullptr) {
    return visitLambda(ctx->lambda());
  } else if (ctx->module() != nullptr) {
    return visitModule(ctx->module());
  } else {
    return visitRecordInstance(ctx->recordInstance());
  }
}

any YonaVisitor::visitPatternValue(YonaParser::PatternValueContext *ctx) {
  if (ctx->unit() != nullptr) {
    return wrap_expr<PatternValue>(*ctx, new UnitExpr(*ctx));
  } else if (ctx->literal() != nullptr) {
    return wrap_expr<PatternValue>(*ctx, visit_expr<LiteralExpr<void *>>(ctx->literal()));
  } else if (ctx->symbol() != nullptr) {
    return wrap_expr<PatternValue>(*ctx, visit_expr<SymbolExpr>(ctx->symbol()));
  } else {
    return wrap_expr<PatternValue>(*ctx, visit_expr<IdentifierExpr>(ctx->identifier()));
  }
}

any YonaVisitor::visitName(YonaParser::NameContext *ctx) { return wrap_expr<NameExpr>(*ctx, ctx->LOWERCASE_NAME()->getText()); }

any YonaVisitor::visitLet(YonaParser::LetContext *ctx) {
  vector<AliasExpr *> aliases;
  for (const auto &alias : ctx->alias()) {
    aliases.push_back(visit_expr<AliasExpr>(alias));
  }
  return wrap_expr<LetExpr>(*ctx, aliases, visit_expr<ExprNode>(ctx->expression()));
}

any YonaVisitor::visitAlias(YonaParser::AliasContext *ctx) {
  if (ctx->lambdaAlias() != nullptr) {
    return visitLambdaAlias(ctx->lambdaAlias());
  } else if (ctx->moduleAlias() != nullptr) {
    return visitModuleAlias(ctx->moduleAlias());
  } else if (ctx->valueAlias() != nullptr) {
    return visitValueAlias(ctx->valueAlias());
  } else if (ctx->patternAlias() != nullptr) {
    return visitPatternAlias(ctx->patternAlias());
  } else {
    return visitFqnAlias(ctx->fqnAlias());
  }
}

any YonaVisitor::visitLambdaAlias(YonaParser::LambdaAliasContext *ctx) { return visitLambda(ctx->lambda()); }

any YonaVisitor::visitModuleAlias(YonaParser::ModuleAliasContext *ctx) { return visitModule(ctx->module()); }

any YonaVisitor::visitValueAlias(YonaParser::ValueAliasContext *ctx) {
  return wrap_expr<ValueAlias>(*ctx, visit_expr<IdentifierExpr>(ctx->identifier()), visit_expr<ExprNode>(ctx->expression()));
}

any YonaVisitor::visitPatternAlias(YonaParser::PatternAliasContext *ctx) {
  return wrap_expr<PatternAlias>(*ctx, visit_expr<PatternNode>(ctx->pattern()), visit_expr<ExprNode>(ctx->expression()));
}

any YonaVisitor::visitFqnAlias(YonaParser::FqnAliasContext *ctx) { return visitFqn(ctx->fqn()); }

any YonaVisitor::visitConditional(YonaParser::ConditionalContext *ctx) {
  return wrap_expr<IfExpr>(*ctx, any_cast<ExprNode *>(visit(ctx->expression(0))), any_cast<ExprNode *>(visit(ctx->expression(1))),
                           ctx->expression().size() > 2 ? any_cast<ExprNode *>(visit(ctx->expression(2))) : nullptr);
}

any YonaVisitor::visitApply(YonaParser::ApplyContext *ctx) {
  vector<variant<ExprNode *, ValueExpr *>> args;
  for (const auto &arg : ctx->funArg()) {
    args.push_back(any_cast<variant<ExprNode *, ValueExpr *>>(visitFunArg(arg)));
  }
  return wrap_expr<ApplyExpr>(*ctx, visit_expr<CallExpr>(ctx->call()), args);
}

any YonaVisitor::visitFunArg(YonaParser::FunArgContext *ctx) {
  if (ctx->value() != nullptr) {
    return visit(ctx->value());
  } else {
    return visit(ctx->expression());
  }
}

any YonaVisitor::visitCall(YonaParser::CallContext *ctx) {
  if (ctx->name() != nullptr) {
    return wrap_expr<NameCall>(*ctx, visit_expr<NameExpr>(ctx->name()));
  } else if (ctx->nameCall() != nullptr) {
    return visitNameCall(ctx->nameCall());
  } else {
    return visitModuleCall(ctx->moduleCall());
  }
}

any YonaVisitor::visitModuleCall(YonaParser::ModuleCallContext *ctx) {
  if (ctx->fqn() != nullptr) {
    return visitFqn(ctx->fqn());
  } else {
    return visitName(ctx->name());
  }
}

any YonaVisitor::visitNameCall(YonaParser::NameCallContext *ctx) {
  return wrap_expr<AliasCall>(*ctx, visit_expr<NameExpr>(ctx->varName), visit_expr<NameExpr>(ctx->funName));
}

any YonaVisitor::visitModule(YonaParser::ModuleContext *ctx) {
  vector<string> exports;
  for (const auto &name : ctx->nonEmptyListOfNames()->name()) {
    exports.push_back(name->LOWERCASE_NAME()->getText());
  }
  vector<RecordNode *> records;
  for (const auto &record : ctx->record()) {
    records.push_back(visit_expr<RecordNode>(record));
  }
  vector<FunctionExpr *> functions;
  for (const auto &function : ctx->function()) {
    functions.push_back(visit_expr<FunctionExpr>(function));
  }
  vector<FunctionDeclaration *> function_declarations;
  for (const auto &declaration : ctx->functionDecl()) {
    function_declarations.push_back(visit_expr<FunctionDeclaration>(declaration));
  }

  auto fqn = visit_expr<FqnExpr>(ctx->fqn());
  const string fqn_str = fqn->to_string();
  module_stack_.push(fqn_str);
  names_.push_back(fqn_str);
  auto result = wrap_expr<ModuleExpr>(*ctx, fqn, exports, records, functions, function_declarations);
  names_.pop_back();
  module_stack_.pop();
  return result;
}

any YonaVisitor::visitNonEmptyListOfNames(YonaParser::NonEmptyListOfNamesContext *ctx) {
  vector<NameExpr *> parts = visit_exprs<NameExpr>(ctx->name());
  NameExpr *moduleName = parts.back();

  parts.pop_back();
  return wrap_expr<FqnExpr>(*ctx, new PackageNameExpr(*ctx, parts), moduleName);
}

any YonaVisitor::visitUnit(YonaParser::UnitContext *ctx) { return wrap_expr<UnitExpr>(*ctx); }

any YonaVisitor::visitByteLiteral(YonaParser::ByteLiteralContext *ctx) { return wrap_expr<ByteExpr>(*ctx, stoi(ctx->BYTE()->getText())); }

any YonaVisitor::visitFloatLiteral(YonaParser::FloatLiteralContext *ctx) { return wrap_expr<FloatExpr>(*ctx, stof(ctx->FLOAT()->getText())); }

any YonaVisitor::visitIntegerLiteral(YonaParser::IntegerLiteralContext *ctx) { return wrap_expr<IntegerExpr>(*ctx, stoi(ctx->INTEGER()->getText())); }

any YonaVisitor::visitStringLiteral(YonaParser::StringLiteralContext *ctx) {
  string text = ctx->getText();
  trim_if(text, boost::is_any_of("\""));
  return wrap_expr<StringExpr>(*ctx, text);
}

any YonaVisitor::visitInterpolatedStringPart(YonaParser::InterpolatedStringPartContext *ctx) {
  return make_any<nullptr_t>(); // TODO
}

any YonaVisitor::visitCharacterLiteral(YonaParser::CharacterLiteralContext *ctx) {
  return wrap_expr<CharacterExpr>(*ctx, ctx->CHARACTER_LITERAL()->getText()[0]);
}

any YonaVisitor::visitBooleanLiteral(YonaParser::BooleanLiteralContext *ctx) {
  if (ctx->KW_TRUE() != nullptr) {
    return wrap_expr<TrueLiteralExpr>(*ctx);
  }
  return wrap_expr<FalseLiteralExpr>(*ctx);
}

any YonaVisitor::visitTuple(YonaParser::TupleContext *ctx) {
  vector<ExprNode *> elements;
  for (const auto &expr : ctx->expression()) {
    elements.push_back(visit_expr<ExprNode>(expr));
  }
  return wrap_expr<TupleExpr>(*ctx, elements);
}

any YonaVisitor::visitDict(YonaParser::DictContext *ctx) {
  vector<pair<ExprNode *, ExprNode *>> elements;
  for (size_t i = 0; i < ctx->dictKey().size(); ++i) {
    elements.emplace_back(visit_expr<ExprNode>(ctx->dictKey(i)), visit_expr<ExprNode>(ctx->dictVal(i)));
  }
  return wrap_expr<DictExpr>(*ctx, elements);
}
std::any YonaVisitor::visitDictKey(YonaParser::DictKeyContext *ctx) { return visit(ctx->expression()); }
std::any YonaVisitor::visitDictVal(YonaParser::DictValContext *ctx) { return visit(ctx->expression()); }

any YonaVisitor::visitSequence(YonaParser::SequenceContext *ctx) {
  if (ctx->emptySequence() != nullptr) {
    return visitEmptySequence(ctx->emptySequence());
  } else if (ctx->otherSequence() != nullptr) {
    return visitOtherSequence(ctx->otherSequence());
  } else {
    return visitRangeSequence(ctx->rangeSequence());
  }
}

any YonaVisitor::visitSet(YonaParser::SetContext *ctx) {
  vector<ExprNode *> elements;
  for (const auto &expr : ctx->expression()) {
    elements.push_back(visit_expr<ExprNode>(expr));
  }
  return wrap_expr<SetExpr>(*ctx, elements);
}

any YonaVisitor::visitFqn(YonaParser::FqnContext *ctx) {
  vector<string> fqn_parts;
  optional<PackageNameExpr *> package_name_expr;

  if (ctx->packageName()) {
    package_name_expr = visit_expr<PackageNameExpr>(ctx->packageName());

    for (const auto name : package_name_expr.value()->parts) {
      fqn_parts.push_back(name->value);
    }
  }

  const auto module_name_expr = visit_expr<NameExpr>(ctx->moduleName());

  fqn_parts.push_back(module_name_expr->value);

  module_imports_.push(fqn_parts);

  return wrap_expr<FqnExpr>(*ctx, package_name_expr, module_name_expr);
}

any YonaVisitor::visitPackageName(YonaParser::PackageNameContext *ctx) {
  vector<NameExpr *> names;
  for (const auto &name : ctx->LOWERCASE_NAME()) {
    names.push_back(new NameExpr(*ctx, name->getText()));
  }
  return wrap_expr<PackageNameExpr>(*ctx, names);
}

any YonaVisitor::visitModuleName(YonaParser::ModuleNameContext *ctx) { return wrap_expr<NameExpr>(*ctx, ctx->UPPERCASE_NAME()->getText()); }

any YonaVisitor::visitSymbol(YonaParser::SymbolContext *ctx) { return wrap_expr<SymbolExpr>(*ctx, ctx->SYMBOL()->getText().substr(1)); }

any YonaVisitor::visitIdentifier(YonaParser::IdentifierContext *ctx) { return wrap_expr<IdentifierExpr>(*ctx, visit_expr<NameExpr>(ctx->name())); }

any YonaVisitor::visitLambda(YonaParser::LambdaContext *ctx) {
  string function_name = nextLambdaName();
  names_.push_back(function_name);
  auto result = wrap_expr<FunctionExpr>(
      *ctx, function_name, visit_exprs<PatternNode>(ctx->pattern()),
      vector{static_cast<FunctionBody *>(new BodyWithoutGuards(*ctx->expression(), visit_expr<ExprNode>(ctx->expression())))});
  names_.pop_back();

  return result;
}

any YonaVisitor::visitUnderscore(YonaParser::UnderscoreContext *ctx) { return UnderscorePattern(*ctx); }

any YonaVisitor::visitEmptySequence(YonaParser::EmptySequenceContext *ctx) { return wrap_expr<ValuesSequenceExpr>(*ctx, vector<ExprNode *>()); }

any YonaVisitor::visitOtherSequence(YonaParser::OtherSequenceContext *ctx) {
  vector<ExprNode *> elements;
  for (const auto &expr : ctx->expression()) {
    elements.push_back(visit_expr<ExprNode>(expr));
  }
  return wrap_expr<ValuesSequenceExpr>(*ctx, elements);
}

any YonaVisitor::visitRangeSequence(YonaParser::RangeSequenceContext *ctx) {
  return wrap_expr<RangeSequenceExpr>(*ctx, visit_expr<ExprNode>(ctx->start), visit_expr<ExprNode>(ctx->end), visit_expr<ExprNode>(ctx->step));
}

any YonaVisitor::visitCaseExpr(YonaParser::CaseExprContext *ctx) {
  vector<PatternExpr *> patterns;
  for (const auto &patternExpr : ctx->patternExpression()) {
    patterns.push_back(visit_expr<PatternExpr>(patternExpr));
  }
  return wrap_expr<CaseExpr>(*ctx, visit_expr<ExprNode>(ctx->expression()), patterns);
}

any YonaVisitor::visitPatternExpression(YonaParser::PatternExpressionContext *ctx) {
  if (ctx->patternExpressionWithoutGuard() != nullptr) {
    return visitPatternExpressionWithoutGuard(ctx->patternExpressionWithoutGuard());
  } else {
    vector<PatternWithGuards> guards;
    for (const auto &guard : ctx->patternExpressionWithGuard()) {
      guards.push_back(any_cast<PatternWithGuards>(visitPatternExpressionWithGuard(guard)));
    }
    vector<PatternWithGuards> patterns;
    for (const auto pattern_expression_with_guard : ctx->patternExpressionWithGuard()) {
      patterns.push_back(any_cast<PatternWithGuards>(visitPatternExpressionWithGuard(pattern_expression_with_guard)));
    }
    return patterns;
  }
}

any YonaVisitor::visitDoExpr(YonaParser::DoExprContext *ctx) {
  vector<ExprNode *> steps;
  for (const auto &step : ctx->doOneStep()) {
    steps.push_back(any_cast<ExprNode *>(visitDoOneStep(step)));
  }
  return wrap_expr<DoExpr>(*ctx, steps);
}

any YonaVisitor::visitDoOneStep(YonaParser::DoOneStepContext *ctx) {
  if (ctx->alias() != nullptr) {
    return visitAlias(ctx->alias());
  } else {
    return visit(ctx->expression());
  }
}

any YonaVisitor::visitPatternExpressionWithoutGuard(YonaParser::PatternExpressionWithoutGuardContext *ctx) {
  return wrap_expr<PatternWithoutGuards>(*ctx, visit_expr<ExprNode>(ctx->expression()));
}

any YonaVisitor::visitPatternExpressionWithGuard(YonaParser::PatternExpressionWithGuardContext *ctx) {
  return wrap_expr<PatternWithGuards>(*ctx, visit_expr<ExprNode>(ctx->guard), visit_expr<ExprNode>(ctx->expr));
}

any YonaVisitor::visitPattern(YonaParser::PatternContext *ctx) {
  if (ctx->pattern() != nullptr) {
    return visitPattern(ctx->pattern());
  } else if (ctx->underscore() != nullptr) {
    return any{UnderscorePattern(*ctx->underscore())};
  } else if (ctx->patternValue() != nullptr) {
    return visitPatternValue(ctx->patternValue());
  } else if (ctx->dataStructurePattern() != nullptr) {
    return visitDataStructurePattern(ctx->dataStructurePattern());
  } else {
    return visitAsDataStructurePattern(ctx->asDataStructurePattern());
  }
}

any YonaVisitor::visitDataStructurePattern(YonaParser::DataStructurePatternContext *ctx) {
  if (ctx->tuplePattern() != nullptr) {
    return visitTuplePattern(ctx->tuplePattern());
  } else if (ctx->sequencePattern() != nullptr) {
    return visitSequencePattern(ctx->sequencePattern());
  } else if (ctx->dictPattern() != nullptr) {
    return visitDictPattern(ctx->dictPattern());
  } else {
    return visitRecordPattern(ctx->recordPattern());
  }
}

any YonaVisitor::visitAsDataStructurePattern(YonaParser::AsDataStructurePatternContext *ctx) {
  return visitDataStructurePattern(ctx->dataStructurePattern());
}

any YonaVisitor::visitPatternWithoutSequence(YonaParser::PatternWithoutSequenceContext *ctx) {
  if (ctx->underscore() != nullptr) {
    return wrap_expr<UnderscorePattern>(*ctx->underscore());
  } else if (ctx->patternValue() != nullptr) {
    return visitPatternValue(ctx->patternValue());
  } else if (ctx->tuplePattern() != nullptr) {
    return visitTuplePattern(ctx->tuplePattern());
  } else {
    return visitDictPattern(ctx->dictPattern());
  }
}

any YonaVisitor::visitTuplePattern(YonaParser::TuplePatternContext *ctx) {
  vector<Pattern *> patterns;
  for (const auto &pattern : ctx->pattern()) {
    patterns.push_back(any_cast<Pattern *>(visitPattern(pattern)));
  }
  return TuplePattern(*ctx, patterns);
}

any YonaVisitor::visitSequencePattern(YonaParser::SequencePatternContext *ctx) {
  if (!ctx->pattern().empty()) {
    vector<Pattern *> patterns;
    for (const auto &pattern : ctx->pattern()) {
      patterns.push_back(any_cast<Pattern *>(visitPattern(pattern)));
    }
    return SeqPattern(*ctx, patterns);
  } else if (ctx->headTails() != nullptr) {
    return visitHeadTails(ctx->headTails());
  } else if (ctx->tailsHead() != nullptr) {
    return visitTailsHead(ctx->tailsHead());
  } else {
    return visitHeadTailsHead(ctx->headTailsHead());
  }
}

any YonaVisitor::visitHeadTails(YonaParser::HeadTailsContext *ctx) {
  vector<PatternWithoutSequence *> headPatterns;
  for (const auto &pattern : ctx->patternWithoutSequence()) {
    headPatterns.push_back(any_cast<PatternWithoutSequence *>(visitPatternWithoutSequence(pattern)));
  }
  return visitTails(ctx->tails());
}

any YonaVisitor::visitTailsHead(YonaParser::TailsHeadContext *ctx) {
  vector<PatternWithoutSequence *> headPatterns;
  for (const auto &pattern : ctx->patternWithoutSequence()) {
    headPatterns.push_back(any_cast<PatternWithoutSequence *>(visitPatternWithoutSequence(pattern)));
  }
  return wrap_expr<TailsHeadPattern>(*ctx, visit_expr<PatternNode>(ctx->tails()), headPatterns);
}

any YonaVisitor::visitHeadTailsHead(YonaParser::HeadTailsHeadContext *ctx) {
  vector<PatternWithoutSequence *> leftPatterns;
  for (const auto &pattern : ctx->leftPattern()) {
    leftPatterns.push_back(any_cast<PatternWithoutSequence *>(visitLeftPattern(pattern)));
  }
  vector<PatternWithoutSequence *> rightPatterns;
  for (const auto &pattern : ctx->rightPattern()) {
    rightPatterns.push_back(any_cast<PatternWithoutSequence *>(visitRightPattern(pattern)));
  }
  return wrap_expr<HeadTailsHeadPattern>(*ctx, leftPatterns, visit_expr<PatternNode>(ctx->tails()), rightPatterns);
}

any YonaVisitor::visitLeftPattern(YonaParser::LeftPatternContext *ctx) { return visitPatternWithoutSequence(ctx->patternWithoutSequence()); }

any YonaVisitor::visitRightPattern(YonaParser::RightPatternContext *ctx) { return visitPatternWithoutSequence(ctx->patternWithoutSequence()); }

any YonaVisitor::visitTails(YonaParser::TailsContext *ctx) {
  if (ctx->identifier() != nullptr) {
    return visitIdentifier(ctx->identifier());
  } else if (ctx->sequence() != nullptr) {
    return visitSequence(ctx->sequence());
  } else if (ctx->underscore() != nullptr) {
    return UnderscorePattern(*ctx->underscore());
  } else {
    return visitStringLiteral(ctx->stringLiteral());
  }
}

any YonaVisitor::visitDictPattern(YonaParser::DictPatternContext *ctx) {
  vector<pair<PatternValue *, Pattern *>> elements;
  for (size_t i = 0; i < ctx->patternValue().size(); ++i) {
    elements.emplace_back(visit_expr<PatternValue>(ctx->patternValue(i)), visit_expr<PatternNode>(ctx->pattern(i)));
  }
  return wrap_expr<DictPattern>(*ctx, elements);
}

any YonaVisitor::visitRecordPattern(YonaParser::RecordPatternContext *ctx) {
  vector<pair<NameExpr *, Pattern *>> elements;
  for (size_t i = 0; i < ctx->name().size(); ++i) {
    elements.emplace_back(visit_expr<NameExpr>(ctx->name(i)), visit_expr<PatternNode>(ctx->pattern(i)));
  }
  return RecordPattern(*ctx, ctx->recordType()->UPPERCASE_NAME()->getText(), elements);
}

any YonaVisitor::visitImportExpr(YonaParser::ImportExprContext *ctx) {
  vector<ImportClauseExpr *> clauses;
  for (const auto &clause : ctx->importClause()) {
    clauses.push_back(any_cast<ImportClauseExpr *>(visitImportClause(clause)));
  }
  return wrap_expr<ImportExpr>(*ctx, clauses, visit_expr<ExprNode>(ctx->expression()));
}

any YonaVisitor::visitImportClause(YonaParser::ImportClauseContext *ctx) {
  if (ctx->moduleImport() != nullptr) {
    return visitModuleImport(ctx->moduleImport());
  } else {
    return visitFunctionsImport(ctx->functionsImport());
  }
}

any YonaVisitor::visitModuleImport(YonaParser::ModuleImportContext *ctx) {
  return wrap_expr<ModuleImport>(*ctx, visit_expr<FqnExpr>(ctx->fqn()), visit_expr<NameExpr>(ctx->name()));
}

any YonaVisitor::visitFunctionsImport(YonaParser::FunctionsImportContext *ctx) {
  return wrap_expr<FunctionsImport>(*ctx, visit_exprs<FunctionAlias>(ctx->functionAlias()), visit_expr<FqnExpr>(ctx->fqn()));
}

any YonaVisitor::visitFunctionAlias(YonaParser::FunctionAliasContext *ctx) { return visitName(ctx->funAlias); }

any YonaVisitor::visitTryCatchExpr(YonaParser::TryCatchExprContext *ctx) {
  return wrap_expr<TryCatchExpr>(*ctx, visit_expr<ExprNode>(ctx->expression()), visit_expr<CatchExpr>(ctx->catchExpr()));
}
std::any YonaVisitor::visitCatchExpr(YonaParser::CatchExprContext *ctx) {
  return wrap_expr<CatchExpr>(*ctx, visit_exprs<CatchPatternExpr>(ctx->catchPatternExpression()));
}
std::any YonaVisitor::visitCatchPatternExpression(YonaParser::CatchPatternExpressionContext *ctx) {
  return wrap_expr<CatchPatternExpr>(
      *ctx,
      ctx->triplePattern() != nullptr ? visit_expr<Pattern>(ctx->triplePattern())
                                      : static_cast<PatternNode *>(new UnderscorePattern(*ctx->underscore())),
      ctx->catchPatternExpressionWithoutGuard() != nullptr
          ? variant<PatternWithoutGuards *, vector<PatternWithGuards *>>(visit_expr<PatternWithoutGuards>(ctx->catchPatternExpressionWithoutGuard()))
          : variant<PatternWithoutGuards *, vector<PatternWithGuards *>>(visit_exprs<PatternWithGuards>(ctx->catchPatternExpressionWithGuard())));
}
std::any YonaVisitor::visitTriplePattern(YonaParser::TriplePatternContext *ctx) {
  return wrap_expr<TuplePattern>(*ctx, visit_exprs<Pattern>(ctx->pattern()));
}
std::any YonaVisitor::visitCatchPatternExpressionWithoutGuard(YonaParser::CatchPatternExpressionWithoutGuardContext *ctx) {
  return wrap_expr<PatternWithoutGuards>(*ctx, visit_expr<ExprNode>(ctx->expression()));
}
std::any YonaVisitor::visitCatchPatternExpressionWithGuard(YonaParser::CatchPatternExpressionWithGuardContext *ctx) {
  return wrap_expr<PatternWithGuards>(*ctx, visit_expr<ExprNode>(ctx->guard), visit_expr<ExprNode>(ctx->expr));
}

any YonaVisitor::visitRaiseExpr(YonaParser::RaiseExprContext *ctx) {
  return wrap_expr<RaiseExpr>(*ctx, visit_expr<SymbolExpr>(ctx->symbol()), visit_expr<StringExpr>(ctx->stringLiteral()));
}

any YonaVisitor::visitWithExpr(YonaParser::WithExprContext *ctx) {
  return wrap_expr<WithExpr>(*ctx, ctx->KW_DAEMON() != nullptr, visit_expr<ExprNode>(ctx->context),
                             ctx->name() == nullptr ? nullptr : visit_expr<NameExpr>(ctx->name()), visit_expr<ExprNode>(ctx->body));
}
std::any YonaVisitor::visitGeneratorExpr(YonaParser::GeneratorExprContext *ctx) {
  if (ctx->sequenceGeneratorExpr() != nullptr) {
    return visitSequenceGeneratorExpr(ctx->sequenceGeneratorExpr());
  } else if (ctx->dictGeneratorExpr() != nullptr) {
    return visitDictGeneratorExpr(ctx->dictGeneratorExpr());
  } else {
    return visitSetGeneratorExpr(ctx->setGeneratorExpr());
  }
}

any YonaVisitor::visitSequenceGeneratorExpr(YonaParser::SequenceGeneratorExprContext *ctx) {
  return wrap_expr<SeqGeneratorExpr>(*ctx, visit_expr<ExprNode>(ctx->reducer), visit_expr<CollectionExtractorExpr>(ctx->collectionExtractor()),
                                     visit_expr<ExprNode>(ctx->stepExpression));
}

any YonaVisitor::visitSetGeneratorExpr(YonaParser::SetGeneratorExprContext *ctx) {
  return wrap_expr<SetGeneratorExpr>(*ctx, visit_expr<ExprNode>(ctx->reducer), visit_expr<CollectionExtractorExpr>(ctx->collectionExtractor()),
                                     visit_expr<ExprNode>(ctx->stepExpression));
}

any YonaVisitor::visitDictGeneratorExpr(YonaParser::DictGeneratorExprContext *ctx) {
  return wrap_expr<DictGeneratorExpr>(*ctx, visit_expr<DictGeneratorReducer>(ctx->dictGeneratorReducer()),
                                      visit_expr<CollectionExtractorExpr>(ctx->collectionExtractor()), visit_expr<ExprNode>(ctx->stepExpression));
}

any YonaVisitor::visitDictGeneratorReducer(YonaParser::DictGeneratorReducerContext *ctx) {
  return wrap_expr<DictGeneratorReducer>(*ctx, visit_expr<ExprNode>(ctx->dictKey()->expression()),
                                         visit_expr<ExprNode>(ctx->dictVal()->expression()));
}
std::any YonaVisitor::visitCollectionExtractor(YonaParser::CollectionExtractorContext *ctx) {
  if (ctx->valueCollectionExtractor() != nullptr) {
    return visitValueCollectionExtractor(ctx->valueCollectionExtractor());
  } else {
    return visitKeyValueCollectionExtractor(ctx->keyValueCollectionExtractor());
  }
}
std::any YonaVisitor::visitValueCollectionExtractor(YonaParser::ValueCollectionExtractorContext *ctx) {
  return visitIdentifierOrUnderscore(ctx->identifierOrUnderscore());
}

std::any YonaVisitor::visitKeyValueCollectionExtractor(YonaParser::KeyValueCollectionExtractorContext *ctx) {
  vector<IdentifierExpr *> identifier_exprs;
  for (const auto &identifier_or_underscore : ctx->identifierOrUnderscore()) {
    identifier_exprs.push_back(visit_expr<IdentifierExpr>(identifier_or_underscore));
  }
  return any{identifier_exprs};
}

any YonaVisitor::visitIdentifierOrUnderscore(YonaParser::IdentifierOrUnderscoreContext *ctx) {
  if (ctx->identifier() != nullptr) {
    return visitIdentifier(ctx->identifier());
  }
  return wrap_expr<UnderscorePattern>(*ctx);
}

any YonaVisitor::visitRecord(YonaParser::RecordContext *ctx) {
  vector<pair<IdentifierExpr *, TypeDefinition *>> elements;
  for (size_t i = 0; i < ctx->identifier().size(); ++i) {
    elements.emplace_back(visit_expr<IdentifierExpr>(ctx->identifier(i)), visit_expr<TypeDefinition>(ctx->typeDef(i)));
  }

  return wrap_expr<RecordNode>(*ctx, visit_expr<NameExpr>(ctx->recordType()), elements);
}

any YonaVisitor::visitRecordType(YonaParser::RecordTypeContext *ctx) { return wrap_expr<NameExpr>(*ctx, ctx->UPPERCASE_NAME()->getText()); }

any YonaVisitor::visitRecordInstance(YonaParser::RecordInstanceContext *ctx) {
  vector<pair<NameExpr *, ExprNode *>> elements;
  for (size_t i = 0; i < ctx->name().size(); ++i) {
    elements.emplace_back(visit_expr<NameExpr>(ctx->name(i)), visit_expr<ExprNode>(ctx->expression(i)));
  }
  return wrap_expr<RecordInstanceExpr>(*ctx, visit_expr<NameExpr>(ctx->recordType()), elements);
}

any YonaVisitor::visitFieldAccessExpr(YonaParser::FieldAccessExprContext *ctx) {
  return wrap_expr<FieldAccessExpr>(*ctx, visit_expr<IdentifierExpr>(ctx->identifier()), visit_expr<NameExpr>(ctx->name()));
}

any YonaVisitor::visitFieldUpdateExpr(YonaParser::FieldUpdateExprContext *ctx) {
  vector<pair<NameExpr *, ExprNode *>> elements;
  for (size_t i = 0; i < ctx->name().size(); ++i) {
    elements.emplace_back(any_cast<NameExpr *>(visitName(ctx->name(i))), any_cast<ExprNode *>(visit(ctx->expression(i))));
  }
  return wrap_expr<FieldUpdateExpr>(*ctx, visit_expr<IdentifierExpr>(ctx->identifier()), elements);
}

std::any YonaVisitor::visitFunctionDecl(YonaParser::FunctionDeclContext *ctx) {
  return wrap_expr<FunctionDeclaration>(*ctx, visit_expr<NameExpr>(ctx->functionName()), visit_exprs<TypeDefinition>(ctx->typeDef()));
}

std::any YonaVisitor::visitType(YonaParser::TypeContext *ctx) {
  return wrap_expr<TypeNode>(*ctx, visit_expr<TypeDeclaration>(ctx->declaration),
                             visit_exprs<TypeDeclaration>(vector(ctx->typeDecl().begin() + 1, ctx->typeDecl().end())));
}

std::any YonaVisitor::visitTypeDecl(YonaParser::TypeDeclContext *ctx) {
  return wrap_expr<TypeDeclaration>(*ctx, visit_expr<TypeNameNode>(ctx->typeName()), visit_exprs<NameExpr>(ctx->typeVar()));
}

std::any YonaVisitor::visitTypeDef(YonaParser::TypeDefContext *ctx) {
  vector<TypeNameNode *> types;
  for (size_t i = 1; i < ctx->typeName().size(); i++) {
    auto res = visit(ctx->typeName(i));
    types.emplace_back(visit_expr<TypeNameNode>(ctx->typeName(i)));
  }

  auto name_wrapper = any_cast<expr_wrapper>(visitTypeName(ctx->typeName(0)));
  auto name = name_wrapper.get_node<TypeNameNode>();

  return wrap_expr<TypeDefinition>(*ctx, name, types);
}

std::any YonaVisitor::visitTypeName(YonaParser::TypeNameContext *ctx) {
  if (ctx->UPPERCASE_NAME()) {
    return wrap_expr<UserDefinedTypeNode>(*ctx, new NameExpr(*ctx, ctx->UPPERCASE_NAME()->getText()));
  }
  if (ctx->UNIT()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Unit);
  }
  return visitBuiltInTypeName(ctx->builtInTypeName());
}

std::any YonaVisitor::visitTypeVar(YonaParser::TypeVarContext *ctx) { return wrap_expr<NameExpr>(*ctx, ctx->LOWERCASE_NAME()->getText()); }

std::any YonaVisitor::visitTypeInstance(YonaParser::TypeInstanceContext *ctx) {
  return wrap_expr<TypeInstance>(*ctx, visit_expr<TypeNameNode>(ctx->typeName()), visit_exprs<ExprNode>(ctx->expression()));
}

std::any YonaVisitor::visitBuiltInTypeName(YonaParser::BuiltInTypeNameContext *ctx) {
  if (ctx->KW_BYTE()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Byte);
  }
  if (ctx->KW_BOOL()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Bool);
  }
  if (ctx->KW_INT() || ctx->KW_INT32()) {
    if (ctx->KW_UNSIGNED()) {
      return wrap_expr<BuiltinTypeNode>(*ctx, UnsignedInt32);
    }
    return wrap_expr<BuiltinTypeNode>(*ctx, SignedInt32);
  }
  if (ctx->KW_INT16()) {
    if (ctx->KW_UNSIGNED()) {
      return wrap_expr<BuiltinTypeNode>(*ctx, UnsignedInt16);
    }
    return wrap_expr<BuiltinTypeNode>(*ctx, SignedInt16);
  }
  if (ctx->KW_INT64()) {
    if (ctx->KW_UNSIGNED()) {
      return wrap_expr<BuiltinTypeNode>(*ctx, UnsignedInt64);
    }
    return wrap_expr<BuiltinTypeNode>(*ctx, SignedInt64);
  }
  if (ctx->KW_INT128()) {
    if (ctx->KW_UNSIGNED()) {
      return wrap_expr<BuiltinTypeNode>(*ctx, UnsignedInt128);
    }
    return wrap_expr<BuiltinTypeNode>(*ctx, SignedInt128);
  }
  if (ctx->KW_FLOAT() || ctx->KW_FLOAT32()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Float32);
  }
  if (ctx->KW_FLOAT64()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Float64);
  }
  if (ctx->KW_FLOAT128()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Float128);
  }
  if (ctx->KW_CHAR()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Char);
  }
  if (ctx->KW_STRING()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, String);
  }
  if (ctx->KW_SYMBOL()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Symbol);
  }
  if (ctx->KW_DICT()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Dict);
  }
  if (ctx->KW_SEQ()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Seq);
  }
  if (ctx->KW_SET()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Set);
  }
  if (ctx->KW_VAR()) {
    return wrap_expr<BuiltinTypeNode>(*ctx, Var);
  }
  return wrap_expr<BuiltinTypeNode>(*ctx, Unit);
}

string YonaVisitor::fqn() const { return boost::algorithm::join(names_, NAME_DELIMITER); }
} // namespace yona
