#include "ast.h"

namespace yonac::ast
{
    template <typename T>
    LiteralExpr<T>::LiteralExpr(T value) : value(std::move(value))
    {
    }

    BinaryOpExpr::BinaryOpExpr(const ExprNode& left, const ExprNode& right) :
        left(std::move(left)), right(std::move(right))
    {
    }

    NameExpr::NameExpr(const string& value) : value(std::move(value)) {}

    IdentifierExpr::IdentifierExpr(const NameExpr& name) : name(std::move(name)) {}

    RecordNode::RecordNode(const NameExpr& recordType, const vector<IdentifierExpr>& identifiers) :
        AstNode(), recordType(std::move(recordType)), identifiers(std::move(identifiers))
    {
    }

    TrueLiteralExpr::TrueLiteralExpr() : LiteralExpr<bool>(true) {}

    FalseLiteralExpr::FalseLiteralExpr() : LiteralExpr<bool>(false) {}

    FloatExpr::FloatExpr(const float value) : LiteralExpr<float>(value) {}

    IntegerExpr::IntegerExpr(const int value) : LiteralExpr<int>(value) {}

    ByteExpr::ByteExpr(unsigned const char value) : LiteralExpr<unsigned char>(value) {}

    StringExpr::StringExpr(const string& value) : LiteralExpr<string>(std::move(value)) {}

    CharacterExpr::CharacterExpr(const char value) : LiteralExpr<char>(value) {}

    UnitExpr::UnitExpr() : LiteralExpr<nullptr_t>(nullptr) {}

    TupleExpr::TupleExpr(const vector<ExprNode>& values) : values(std::move(values)) {}

    DictExpr::DictExpr(const vector<pair<ExprNode, ExprNode>>& values) : values(std::move(values)) {}

    ValuesSequenceExpr::ValuesSequenceExpr(const vector<ExprNode>& values) : values(std::move(values)) {}

    RangeSequenceExpr::RangeSequenceExpr(const ExprNode& start, const ExprNode& end, const ExprNode& step) :
        start(std::move(start)), end(std::move(end)), step(std::move(step))
    {
    }

    SetExpr::SetExpr(const vector<ExprNode>& values) : values(std::move(values)) {}

    SymbolExpr::SymbolExpr(const string& value) : value(std::move(value)) {}

    PackageNameExpr::PackageNameExpr(const vector<NameExpr>& parts) : parts(std::move(parts)) {}

    FqnExpr::FqnExpr(const PackageNameExpr& packageName, const NameExpr& moduleName) :
        packageName(std::move(packageName)), moduleName(std::move(moduleName))
    {
    }

    FunctionExpr::FunctionExpr(const string& name, const vector<PatternNode>& patterns,
                               const vector<FunctionBody>& bodies) :
        name(std::move(name)), patterns(std::move(patterns)), bodies(std::move(bodies))
    {
    }

    ModuleExpr::ModuleExpr(const FqnExpr& fqn, const vector<string>& exports, const vector<RecordNode>& records,
                           const vector<FunctionExpr>& functions) :
        fqn(std::move(fqn)), exports(std::move(exports)), records(std::move(records)), functions(std::move(functions))
    {
    }

    RecordInstanceExpr::RecordInstanceExpr(const NameExpr& recordType, const vector<pair<NameExpr, ExprNode>>& items) :
        recordType(std::move(recordType)), items(std::move(items))
    {
    }

    BodyWithGuards::BodyWithGuards(const ExprNode& guard, const vector<ExprNode>& expr) :
        guard(std::move(guard)), exprs(expr)
    {
    }

    BodyWithoutGuards::BodyWithoutGuards(const ExprNode& expr) : expr(std::move(expr)) {}

    LogicalNotOpExpr::LogicalNotOpExpr(const ExprNode& expr) : expr(std::move(expr)) {}

    BinaryNotOpExpr::BinaryNotOpExpr(const ExprNode& expr) : expr(std::move(expr)) {}

    PowerExpr::PowerExpr(const ExprNode& left, const ExprNode& right) : BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    MultiplyExpr::MultiplyExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    DivideExpr::DivideExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    ModuloExpr::ModuloExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    AddExpr::AddExpr(const ExprNode& left, const ExprNode& right) : BinaryOpExpr(std::move(left), std::move(right)) {}

    SubtractExpr::SubtractExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    LeftShiftExpr::LeftShiftExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    RightShiftExpr::RightShiftExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    ZerofillRightShiftExpr::ZerofillRightShiftExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    GteExpr::GteExpr(const ExprNode& left, const ExprNode& right) : BinaryOpExpr(std::move(left), std::move(right)) {}

    LteExpr::LteExpr(const ExprNode& left, const ExprNode& right) : BinaryOpExpr(std::move(left), std::move(right)) {}

    GtExpr::GtExpr(const ExprNode& left, const ExprNode& right) : BinaryOpExpr(std::move(left), std::move(right)) {}

    LtExpr::LtExpr(const ExprNode& left, const ExprNode& right) : BinaryOpExpr(std::move(left), std::move(right)) {}

    EqExpr::EqExpr(const ExprNode& left, const ExprNode& right) : BinaryOpExpr(std::move(left), std::move(right)) {}

    NeqExpr::NeqExpr(const ExprNode& left, const ExprNode& right) : BinaryOpExpr(std::move(left), std::move(right)) {}

    ConsLeftExpr::ConsLeftExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    ConsRightExpr::ConsRightExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    JoinExpr::JoinExpr(const ExprNode& left, const ExprNode& right) : BinaryOpExpr(std::move(left), std::move(right)) {}

    BitwiseAndExpr::BitwiseAndExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    BitwiseXorExpr::BitwiseXorExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    BitwiseOrExpr::BitwiseOrExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    LogicalAndExpr::LogicalAndExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    LogicalOrExpr::LogicalOrExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    InExpr::InExpr(const ExprNode& left, const ExprNode& right) : BinaryOpExpr(std::move(left), std::move(right)) {}

    PipeLeftExpr::PipeLeftExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    PipeRightExpr::PipeRightExpr(const ExprNode& left, const ExprNode& right) :
        BinaryOpExpr(std::move(left), std::move(right))
    {
    }

    LetExpr::LetExpr(const vector<AliasExpr>& aliases, const ExprNode& expr) :
        aliases(std::move(aliases)), expr(std::move(expr))
    {
    }

    IfExpr::IfExpr(const ExprNode& condition, const ExprNode& thenExpr, const optional<ExprNode>& elseExpr) :
        condition(std::move(condition)), thenExpr(std::move(thenExpr)), elseExpr(std::move(elseExpr))
    {
    }

    ApplyExpr::ApplyExpr(const CallExpr& call, const vector<variant<ExprNode, ValueExpr>>& args) :
        call(std::move(call)), args(std::move(args))
    {
    }

    DoExpr::DoExpr(const vector<variant<AliasExpr, ExprNode>>& steps) : steps(std::move(steps)) {}

    ImportExpr::ImportExpr(const vector<ImportClauseExpr>& clauses, const ExprNode& expr) :
        clauses(std::move(clauses)), expr(std::move(expr))
    {
    }

    RaiseExpr::RaiseExpr(const SymbolExpr& symbol, const LiteralExpr<string>& message) :
        symbol(std::move(symbol)), message(std::move(message))
    {
    }

    WithExpr::WithExpr(const ExprNode& contextExpr, const optional<NameExpr>& name, const ExprNode& bodyExpr) :
        contextExpr(std::move(contextExpr)), name(std::move(name)), bodyExpr(std::move(bodyExpr))
    {
    }

    FieldAccessExpr::FieldAccessExpr(const IdentifierExpr& identifier, const NameExpr& name) :
        identifier(std::move(identifier)), name(std::move(name))
    {
    }

    FieldUpdateExpr::FieldUpdateExpr(const IdentifierExpr& identifier,
                                     const vector<pair<NameExpr, ExprNode>>& updates) :
        identifier(std::move(identifier)), updates(std::move(updates))
    {
    }

    LambdaAlias::LambdaAlias(const NameExpr& name, const FunctionExpr& lambda) :
        name(std::move(name)), lambda(std::move(lambda))
    {
    }

    ModuleAlias::ModuleAlias(const NameExpr& name, const ModuleExpr& module) :
        name(std::move(name)), module(std::move(module))
    {
    }

    ValueAlias::ValueAlias(const IdentifierExpr& identifier, const ExprNode& expr) :
        identifier(std::move(identifier)), expr(std::move(expr))
    {
    }

    PatternAlias::PatternAlias(const PatternNode& pattern, const ExprNode& expr) :
        pattern(std::move(pattern)), expr(std::move(expr))
    {
    }

    FqnAlias::FqnAlias(const NameExpr& name, const FqnExpr& fqn) : name(std::move(name)), fqn(std::move(fqn)) {}

    FunctionAlias::FunctionAlias(const NameExpr& name, const NameExpr& alias) :
        name(std::move(name)), alias(std::move(alias))
    {
    }

    AliasCall::AliasCall(const NameExpr& alias, const NameExpr& funName) :
        alias(std::move(alias)), funName(std::move(funName))
    {
    }

    NameCall::NameCall(const NameExpr& name) : name(std::move(name)) {}

    ModuleCall::ModuleCall(const variant<FqnExpr, ExprNode>& fqn, const NameExpr& funName) :
        fqn(std::move(fqn)), funName(std::move(funName))
    {
    }

    ModuleImport::ModuleImport(const FqnExpr& fqn, const NameExpr& name) : fqn(std::move(fqn)), name(std::move(name)) {}

    FunctionsImport::FunctionsImport(const vector<FunctionAlias>& aliases, const FqnExpr& fromFqn) :
        aliases(std::move(aliases)), fromFqn(std::move(fromFqn))
    {
    }

    SeqGeneratorExpr::SeqGeneratorExpr(const ExprNode& reducerExpr, const CollectionExtractorExpr& collectionExtractor,
                                       ExprNode stepExpression) :
        reducerExpr(std::move(reducerExpr)), collectionExtractor(std::move(collectionExtractor)),
        stepExpression(std::move(stepExpression))
    {
    }

    SetGeneratorExpr::SetGeneratorExpr(const ExprNode& reducerExpr, const CollectionExtractorExpr& collectionExtractor,
                                       const ExprNode& stepExpression) :
        reducerExpr(std::move(reducerExpr)), collectionExtractor(std::move(collectionExtractor)),
        stepExpression(std::move(stepExpression))
    {
    }

    DictGeneratorReducer::DictGeneratorReducer(const ExprNode& key, const ExprNode& value) :
        key(std::move(key)), value(std::move(value))
    {
    }

    DictGeneratorExpr::DictGeneratorExpr(const DictGeneratorReducer& reducerExpr,
                                         const CollectionExtractorExpr& collectionExtractor,
                                         const ExprNode& stepExpression) :
        reducerExpr(std::move(reducerExpr)), collectionExtractor(std::move(collectionExtractor)),
        stepExpression(std::move(stepExpression))
    {
    }

    ValueCollectionExtractorExpr::ValueCollectionExtractorExpr(const IdentifierOrUnderscore& expr) :
        expr(std::move(expr))
    {
    }

    KeyValueCollectionExtractorExpr::KeyValueCollectionExtractorExpr(const IdentifierOrUnderscore& keyExpr,
                                                                     const IdentifierOrUnderscore& valueExpr) :
        keyExpr(std::move(keyExpr)), valueExpr(std::move(valueExpr))
    {
    }

    PatternWithGuards::PatternWithGuards(const ExprNode& guard, const ExprNode& exprNode) :
        PatternNode(), guard(std::move(guard)), exprNode(std::move(exprNode)) {};

    PatternWithoutGuards::PatternWithoutGuards(const ExprNode& exprNode) : PatternNode(), exprNode(std::move(exprNode))
    {
    }

    PatternExpr::PatternExpr(const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>>& patternExpr) :
        patternExpr(std::move(patternExpr))
    {
    }

    CatchPatternExpr::CatchPatternExpr(const Pattern& matchPattern,
                                       const variant<PatternWithoutGuards, vector<PatternWithGuards>>& pattern) :
        matchPattern(std::move(matchPattern)), pattern(std::move(pattern))
    {
    }

    CatchExpr::CatchExpr(const vector<CatchPatternExpr>& patterns) : patterns(std::move(patterns)) {}

    TryCatchExpr::TryCatchExpr(const ExprNode& tryExpr, const CatchExpr& catchExpr) :
        tryExpr(std::move(tryExpr)), catchExpr(std::move(catchExpr))
    {
    }

    PatternValue::PatternValue(
        const variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr>& expr) :
        expr(std::move(expr))
    {
    }

    AsDataStructurePattern::AsDataStructurePattern(const IdentifierExpr& identifier,
                                                   const DataStructurePattern& pattern) :
        identifier(std::move(identifier)), pattern(std::move(pattern))
    {
    }

    TuplePattern::TuplePattern(const vector<Pattern>& patterns) : patterns(std::move(patterns)) {}

    SeqPattern::SeqPattern(const vector<Pattern>& patterns) : patterns(std::move(patterns)) {}

    HeadTailsPattern::HeadTailsPattern(const vector<PatternWithoutSequence>& heads, const TailPattern& tail) :
        heads(std::move(heads)), tail(std::move(tail))
    {
    }

    TailsHeadPattern::TailsHeadPattern(const TailPattern& tail, const vector<PatternWithoutSequence>& heads) :
        tail(std::move(tail)), heads(std::move(heads))
    {
    }

    HeadTailsHeadPattern::HeadTailsHeadPattern(const vector<PatternWithoutSequence>& left, const TailPattern& tail,
                                               const vector<PatternWithoutSequence>& right) :
        left(std::move(left)), tail(std::move(tail)), right(std::move(right))
    {
    }

    DictPattern::DictPattern(const vector<pair<PatternValue, Pattern>>& keyValuePairs) :
        keyValuePairs(std::move(keyValuePairs))
    {
    }

    RecordPattern::RecordPattern(const string& recordType, const vector<pair<NameExpr, Pattern>>& items) :
        recordType(std::move(recordType)), items(std::move(items))
    {
    }

    CaseExpr::CaseExpr(const ExprNode& expr, const vector<PatternExpr>& patterns) :
        expr(std::move(expr)), patterns(std::move(patterns))
    {
    }
}
