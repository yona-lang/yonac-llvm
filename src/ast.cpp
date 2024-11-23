#include "ast.h"

#include <cmath>
#include <utility>
#include <variant>

namespace yona::ast
{
    template <typename T>
    LiteralExpr<T>::LiteralExpr(Token token, T value) : ValueExpr(token), value(std::move(value))
    {
    }

    ScopedNode* ScopedNode::getParentScopedNode() const
    {
        if (parent == nullptr)
        {
            return nullptr;
        }
        else
        {
            AstNode* tmp = parent;
            do
            {
                if (const auto result = dynamic_cast<ScopedNode*>(tmp); result != nullptr)
                {
                    return result;
                }
                else
                {
                    tmp = tmp->parent;
                }
            }
            while (tmp != nullptr);
            return nullptr;
        }
    }

    BinaryOpExpr::BinaryOpExpr(Token token, ExprNode left, ExprNode right) :
        OpExpr(token), left(*left.with_parent<ExprNode>(this)), right(*right.with_parent<ExprNode>(this))
    {
    }

    NameExpr::NameExpr(Token token, string value) : ExprNode(token), value(std::move(value)) {}

    IdentifierExpr::IdentifierExpr(Token token, NameExpr name) :
        ValueExpr(token), name(*name.with_parent<NameExpr>(this))
    {
    }

    RecordNode::RecordNode(Token token, NameExpr recordType, const vector<IdentifierExpr>& identifiers) :
        AstNode(token), recordType(*recordType.with_parent<NameExpr>(this)),
        identifiers(nodes_with_parent(identifiers, this))
    {
    }

    TrueLiteralExpr::TrueLiteralExpr(Token) : LiteralExpr<bool>(token, true) {}

    FalseLiteralExpr::FalseLiteralExpr(Token) : LiteralExpr<bool>(token, false) {}

    FloatExpr::FloatExpr(Token token, float value) : LiteralExpr<float>(token, value) {}

    IntegerExpr::IntegerExpr(Token token, int value) : LiteralExpr<int>(token, value) {}

    ByteExpr::ByteExpr(Token token, unsigned char value) : LiteralExpr<unsigned char>(token, value) {}

    StringExpr::StringExpr(Token token, string value) : LiteralExpr<string>(token, value) {}

    CharacterExpr::CharacterExpr(Token token, const char value) : LiteralExpr<char>(token, value) {}

    UnitExpr::UnitExpr(Token) : LiteralExpr<nullptr_t>(token, nullptr) {}

    TupleExpr::TupleExpr(Token token, const vector<ExprNode>& values) :
        ValueExpr(token), values(nodes_with_parent(values, this))
    {
    }

    DictExpr::DictExpr(Token token, const vector<pair<ExprNode, ExprNode>>& values) :
        ValueExpr(token), values(nodes_with_parent(values, this))
    {
    }

    ValuesSequenceExpr::ValuesSequenceExpr(Token token, const vector<ExprNode>& values) :
        SequenceExpr(token), values(nodes_with_parent(values, this))
    {
    }

    RangeSequenceExpr::RangeSequenceExpr(Token token, ExprNode start, ExprNode end, ExprNode step) :
        SequenceExpr(token), start(*start.with_parent<ExprNode>(this)), end(*end.with_parent<ExprNode>(this)),
        step(*step.with_parent<ExprNode>(this))
    {
    }

    SetExpr::SetExpr(Token token, const vector<ExprNode>& values) :
        ValueExpr(token), values(nodes_with_parent(values, this))
    {
    }

    SymbolExpr::SymbolExpr(Token token, string value) : ValueExpr(token), value(std::move(value)) {}

    PackageNameExpr::PackageNameExpr(Token token, const vector<NameExpr>& parts) :
        ValueExpr(token), parts(nodes_with_parent(parts, this))
    {
    }

    FqnExpr::FqnExpr(Token token, PackageNameExpr packageName, NameExpr moduleName) :
        ValueExpr(token), packageName(*packageName.with_parent<PackageNameExpr>(this)),
        moduleName(*moduleName.with_parent<NameExpr>(this))
    {
    }

    FunctionExpr::FunctionExpr(Token token, string name, const vector<PatternNode>& patterns,
                               const vector<FunctionBody>& bodies) :
        ExprNode(token), name(std::move(name)), patterns(nodes_with_parent(patterns, this)),
        bodies(nodes_with_parent(bodies, this))
    {
    }

    ModuleExpr::ModuleExpr(Token token, FqnExpr fqn, const vector<string>& exports, const vector<RecordNode>& records,
                           const vector<FunctionExpr>& functions) :
        ValueExpr(token), fqn(*fqn.with_parent<FqnExpr>(this)), exports(exports),
        records(nodes_with_parent(records, this)), functions(nodes_with_parent(functions, this))
    {
    }

    RecordInstanceExpr::RecordInstanceExpr(Token token, NameExpr recordType,
                                           const vector<pair<NameExpr, ExprNode>>& items) :
        ValueExpr(token), recordType(*recordType.with_parent<NameExpr>(this)), items(nodes_with_parent(items, this))
    {
    }

    BodyWithGuards::BodyWithGuards(Token token, ExprNode guard, const vector<ExprNode>& expr) :
        FunctionBody(token), guard(*guard.with_parent<ExprNode>(this)), exprs(nodes_with_parent(expr, this))
    {
    }

    BodyWithoutGuards::BodyWithoutGuards(Token token, ExprNode expr) :
        FunctionBody(token), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    LogicalNotOpExpr::LogicalNotOpExpr(Token token, ExprNode expr) :
        OpExpr(token), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    BinaryNotOpExpr::BinaryNotOpExpr(Token token, ExprNode expr) :
        OpExpr(token), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    PowerExpr::PowerExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    MultiplyExpr::MultiplyExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    DivideExpr::DivideExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    ModuloExpr::ModuloExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    AddExpr::AddExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    SubtractExpr::SubtractExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    LeftShiftExpr::LeftShiftExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    RightShiftExpr::RightShiftExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    ZerofillRightShiftExpr::ZerofillRightShiftExpr(Token token, ExprNode left, ExprNode right) :
        BinaryOpExpr(token, left, right)
    {
    }

    GteExpr::GteExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    LteExpr::LteExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    GtExpr::GtExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    LtExpr::LtExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    EqExpr::EqExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    NeqExpr::NeqExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    ConsLeftExpr::ConsLeftExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    ConsRightExpr::ConsRightExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    JoinExpr::JoinExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    BitwiseAndExpr::BitwiseAndExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    BitwiseXorExpr::BitwiseXorExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    BitwiseOrExpr::BitwiseOrExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    LogicalAndExpr::LogicalAndExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    LogicalOrExpr::LogicalOrExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    InExpr::InExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    PipeLeftExpr::PipeLeftExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    PipeRightExpr::PipeRightExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    LetExpr::LetExpr(Token token, const vector<AliasExpr>& aliases, ExprNode expr) :
        ExprNode(token), aliases(nodes_with_parent(aliases, this)), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    IfExpr::IfExpr(Token token, ExprNode condition, ExprNode thenExpr, const optional<ExprNode>& elseExpr) :
        ExprNode(token), condition(*condition.with_parent<ExprNode>(this)),
        thenExpr(*thenExpr.with_parent<ExprNode>(this)), elseExpr(node_with_parent(elseExpr, this))
    {
    }

    ApplyExpr::ApplyExpr(Token token, CallExpr call, const vector<variant<ExprNode, ValueExpr>>& args) :
        ExprNode(token), call(*call.with_parent<CallExpr>(this)), args(nodes_with_parent(args, this))
    {
    }

    DoExpr::DoExpr(Token token, const vector<variant<AliasExpr, ExprNode>>& steps) : ExprNode(token), steps(steps) {}

    ImportExpr::ImportExpr(Token token, const vector<ImportClauseExpr>& clauses, ExprNode expr) :
        ExprNode(token), clauses(nodes_with_parent(clauses, this)), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    RaiseExpr::RaiseExpr(Token token, SymbolExpr symbol, LiteralExpr<string> message) :
        ExprNode(token), symbol(*symbol.with_parent<SymbolExpr>(this)),
        message(*message.with_parent<LiteralExpr<string>>(this))
    {
    }

    WithExpr::WithExpr(Token token, ExprNode contextExpr, const optional<NameExpr>& name, ExprNode bodyExpr) :
        ExprNode(token), contextExpr(*contextExpr.with_parent<ExprNode>(this)), name(node_with_parent(name, this)),
        bodyExpr(*bodyExpr.with_parent<ExprNode>(this))
    {
    }

    FieldAccessExpr::FieldAccessExpr(Token token, IdentifierExpr identifier, NameExpr name) :
        ExprNode(token), identifier(*identifier.with_parent<IdentifierExpr>(this)),
        name(*name.with_parent<NameExpr>(this))
    {
    }

    FieldUpdateExpr::FieldUpdateExpr(Token token, IdentifierExpr identifier,
                                     const vector<pair<NameExpr, ExprNode>>& updates) :
        ExprNode(token), identifier(*identifier.with_parent<IdentifierExpr>(this)),
        updates(nodes_with_parent(updates, this))
    {
    }

    LambdaAlias::LambdaAlias(Token token, NameExpr name, FunctionExpr lambda) :
        AliasExpr(token), name(*name.with_parent<NameExpr>(this)), lambda(*lambda.with_parent<FunctionExpr>(this))
    {
    }

    ModuleAlias::ModuleAlias(Token token, NameExpr name, ModuleExpr module) :
        AliasExpr(token), name(*name.with_parent<NameExpr>(this)), module(*module.with_parent<ModuleExpr>(this))
    {
    }

    ValueAlias::ValueAlias(Token token, IdentifierExpr identifier, ExprNode expr) :
        AliasExpr(token), identifier(*identifier.with_parent<IdentifierExpr>(this)),
        expr(*expr.with_parent<ExprNode>(this))
    {
    }

    PatternAlias::PatternAlias(Token token, PatternNode pattern, ExprNode expr) :
        AliasExpr(token), pattern(*pattern.with_parent<PatternNode>(this)), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    FqnAlias::FqnAlias(Token token, NameExpr name, FqnExpr fqn) :
        AliasExpr(token), name(*name.with_parent<NameExpr>(this)), fqn(*fqn.with_parent<FqnExpr>(this))
    {
    }

    FunctionAlias::FunctionAlias(Token token, NameExpr name, NameExpr alias) :
        AliasExpr(token), name(*name.with_parent<NameExpr>(this)), alias(*alias.with_parent<NameExpr>(this))
    {
    }

    AliasCall::AliasCall(Token token, NameExpr alias, NameExpr funName) :
        CallExpr(token), alias(*alias.with_parent<NameExpr>(this)), funName(*funName.with_parent<NameExpr>(this))
    {
    }

    NameCall::NameCall(Token token, NameExpr name) : CallExpr(token), name(*name.with_parent<NameExpr>(this)) {}

    ModuleCall::ModuleCall(Token token, const variant<FqnExpr, ExprNode>& fqn, NameExpr funName) :
        CallExpr(token), fqn(node_with_parent(fqn, this)), funName(*funName.with_parent<NameExpr>(this))
    {
    }

    ModuleImport::ModuleImport(Token token, FqnExpr fqn, NameExpr name) :
        ImportClauseExpr(token), fqn(*fqn.with_parent<FqnExpr>(this)), name(*name.with_parent<NameExpr>(this))
    {
    }

    FunctionsImport::FunctionsImport(Token token, const vector<FunctionAlias>& aliases, FqnExpr fromFqn) :
        ImportClauseExpr(token), aliases(nodes_with_parent(aliases, this)), fromFqn(*fromFqn.with_parent<FqnExpr>(this))
    {
    }

    SeqGeneratorExpr::SeqGeneratorExpr(Token token, ExprNode reducerExpr, CollectionExtractorExpr collectionExtractor,
                                       ExprNode stepExpression) :
        GeneratorExpr(token), reducerExpr(*reducerExpr.with_parent<ExprNode>(this)),
        collectionExtractor(*collectionExtractor.with_parent<CollectionExtractorExpr>(this)),
        stepExpression(*stepExpression.with_parent<ExprNode>(this))
    {
    }

    SetGeneratorExpr::SetGeneratorExpr(Token token, ExprNode reducerExpr, CollectionExtractorExpr collectionExtractor,
                                       ExprNode stepExpression) :
        GeneratorExpr(token), reducerExpr(*reducerExpr.with_parent<ExprNode>(this)),
        collectionExtractor(*collectionExtractor.with_parent<CollectionExtractorExpr>(this)),
        stepExpression(*stepExpression.with_parent<ExprNode>(this))
    {
    }

    DictGeneratorReducer::DictGeneratorReducer(Token token, ExprNode key, ExprNode value) :
        ExprNode(token), key(*key.with_parent<ExprNode>(this)), value(*value.with_parent<ExprNode>(this))
    {
    }

    DictGeneratorExpr::DictGeneratorExpr(Token token, DictGeneratorReducer reducerExpr,
                                         CollectionExtractorExpr collectionExtractor, ExprNode stepExpression) :
        GeneratorExpr(token), reducerExpr(*reducerExpr.with_parent<DictGeneratorReducer>(this)),
        collectionExtractor(*collectionExtractor.with_parent<CollectionExtractorExpr>(this)),
        stepExpression(*stepExpression.with_parent<ExprNode>(this))
    {
    }

    ValueCollectionExtractorExpr::ValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore expr) :
        CollectionExtractorExpr(token), expr(node_with_parent(expr, this))
    {
    }

    KeyValueCollectionExtractorExpr::KeyValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore keyExpr,
                                                                     IdentifierOrUnderscore valueExpr) :
        CollectionExtractorExpr(token), keyExpr(node_with_parent(keyExpr, this)),
        valueExpr(node_with_parent(valueExpr, this))
    {
    }

    PatternWithGuards::PatternWithGuards(Token token, ExprNode guard, ExprNode exprNode) :
        PatternNode(token), guard(*guard.with_parent<ExprNode>(this)),
        exprNode(*exprNode.with_parent<ExprNode>(this)) {};

    PatternWithoutGuards::PatternWithoutGuards(Token token, ExprNode exprNode) :
        PatternNode(token), exprNode(*exprNode.with_parent<ExprNode>(this))
    {
    }

    PatternExpr::PatternExpr(Token token,
                             const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>>& patternExpr) :
        ExprNode(token), patternExpr(patternExpr)
    {
        // std::visit({ [this](Pattern& arg) { arg.with_parent<Pattern>(this); },
        //              [this](PatternWithoutGuards& arg) { arg.with_parent<PatternWithGuards>(this); },
        //              [this](vector<PatternWithGuards>& arg) { nodes_with_parent(arg, this); } },
        //            patternExpr); // TODO
    }

    CatchPatternExpr::CatchPatternExpr(Token token, Pattern matchPattern,
                                       const variant<PatternWithoutGuards, vector<PatternWithGuards>>& pattern) :
        ExprNode(token), matchPattern(*matchPattern.with_parent<Pattern>(this)), pattern(pattern)
    {
        // std::visit({ [this](PatternWithoutGuards& arg) { arg.with_parent<PatternWithGuards>(this); },
        //              [this](vector<PatternWithGuards>& arg) { nodes_with_parent(arg, this); } },
        //            pattern); // TODO
    }

    CatchExpr::CatchExpr(Token token, const vector<CatchPatternExpr>& patterns) :
        ExprNode(token), patterns(nodes_with_parent(patterns, this))
    {
    }

    TryCatchExpr::TryCatchExpr(Token token, ExprNode tryExpr, CatchExpr catchExpr) :
        ExprNode(token), tryExpr(*tryExpr.with_parent<ExprNode>(this)),
        catchExpr(*catchExpr.with_parent<CatchExpr>(this))
    {
    }

    PatternValue::PatternValue(Token token,
                               const variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr>& expr) :
        PatternNode(token), expr(node_with_parent(expr, this))
    {
    }

    AsDataStructurePattern::AsDataStructurePattern(Token token, IdentifierExpr identifier,
                                                   DataStructurePattern pattern) :
        PatternNode(token), identifier(*identifier.with_parent<IdentifierExpr>(this)),
        pattern(*pattern.with_parent<DataStructurePattern>(this))
    {
    }

    TuplePattern::TuplePattern(Token token, const vector<Pattern>& patterns) :
        PatternNode(token), patterns(nodes_with_parent(patterns, this))
    {
    }

    SeqPattern::SeqPattern(Token token, const vector<Pattern>& patterns) :
        PatternNode(token), patterns(nodes_with_parent(patterns, this))
    {
    }

    HeadTailsPattern::HeadTailsPattern(Token token, const vector<PatternWithoutSequence>& heads, TailPattern tail) :
        PatternNode(token), heads(nodes_with_parent(heads, this)), tail(*tail.with_parent<TailPattern>(this))
    {
    }

    TailsHeadPattern::TailsHeadPattern(Token token, TailPattern tail, const vector<PatternWithoutSequence>& heads) :
        PatternNode(token), tail(*tail.with_parent<TailPattern>(this)), heads(nodes_with_parent(heads, this))
    {
    }

    HeadTailsHeadPattern::HeadTailsHeadPattern(Token token, const vector<PatternWithoutSequence>& left,
                                               TailPattern tail, const vector<PatternWithoutSequence>& right) :
        PatternNode(token), left(nodes_with_parent(left, this)), tail(*tail.with_parent<TailPattern>(this)),
        right(nodes_with_parent(right, this))
    {
    }

    DictPattern::DictPattern(Token token, const vector<pair<PatternValue, Pattern>>& keyValuePairs) :
        PatternNode(token), keyValuePairs(nodes_with_parent(keyValuePairs, this))
    {
    }

    RecordPattern::RecordPattern(Token token, string recordType, const vector<pair<NameExpr, Pattern>>& items) :
        PatternNode(token), recordType(std::move(recordType)), items(nodes_with_parent(items, this))
    {
    }

    CaseExpr::CaseExpr(Token token, ExprNode expr, const vector<PatternExpr>& patterns) :
        ExprNode(token), expr(*expr.with_parent<ExprNode>(this)), patterns(nodes_with_parent(patterns, this))
    {
    }
}
