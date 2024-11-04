#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>

namespace yonac::ast {
    using namespace std;

    class Token {};

    template <typename T>
    class AstVisitor {
    public:
        virtual T visit() = 0; // Placeholder for the actual implementation
    };

    class AstNode {
    public:
        Token token;
    };

    class ExprNode : public AstNode {};
    class PatternNode : public AstNode {};
    class UnderscoreNode : public PatternNode {};

    class ValueExpr : public ExprNode {};
    template <typename T>
    class LiteralExpr : public ValueExpr {
    public:
        T value;
        LiteralExpr(T value) : value(value) {}
    };
    class OpExpr : public ExprNode {};
    class BinaryOpExpr : public OpExpr {
    public:
        unique_ptr<ExprNode> left;
        unique_ptr<ExprNode> right;
        BinaryOpExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : left(left), right(right) {}
    };
    class AliasExpr : public ExprNode {};
    class CallExpr : public ExprNode {};
    class ImportClauseExpr : public ExprNode {};
    class GeneratorExpr : public ExprNode {};
    class CollectionExtractorExpr : public ExprNode {};
    class SequenceExpr : public ExprNode {};

    class FunctionBody : public AstNode {};

    class NameExpr : public ExprNode {
    public:
        string value;
        NameExpr(string value) : value(value) {}
    };

    class IdentifierExpr : public ValueExpr {
    public:
        NameExpr name;
        IdentifierExpr(NameExpr name) : name(name) {}
    };

    class RecordNode : public AstNode {
    public:
        string recordType;
        vector<unique_ptr<IdentifierExpr>> identifiers;
        RecordNode(string recordType, vector<unique_ptr<IdentifierExpr>> identifiers)
            : recordType(recordType), identifiers(identifiers) {}
    };

    class TrueLiteralExpr : public LiteralExpr<bool> {
    public:
        TrueLiteralExpr() : LiteralExpr<bool>(true) {}
    };
    class FalseLiteralExpr : public LiteralExpr<bool> {
    public:
        FalseLiteralExpr() : LiteralExpr<bool>(false) {}
    };
    class FloatExpr : public LiteralExpr<float> {
    public:
        FloatExpr(float value) : LiteralExpr<float>(value) {}
    };
    class IntegerExpr : public LiteralExpr<int> {
    public:
        IntegerExpr(int value) : LiteralExpr<int>(value) {}
    };
    class ByteExpr : public LiteralExpr<unsigned char> {
    public:
        ByteExpr(unsigned char value) : LiteralExpr<unsigned char>(value) {}
    };
    class StringExpr : public LiteralExpr<string> {
    public:
        StringExpr(string value) : LiteralExpr<string>(value) {}
    };
    class CharacterExpr : public LiteralExpr<char> {
    public:
        CharacterExpr(char value) : LiteralExpr<char>(value) {}
    };

    class UnitExpr : public LiteralExpr<nullptr_t> {
    public:
        UnitExpr() : LiteralExpr<nullptr_t>(nullptr) {}
    };
    class TupleExpr : public ValueExpr {
    public:
        vector<unique_ptr<ExprNode>> values;
        TupleExpr(vector<unique_ptr<ExprNode>> values) : values(values) {}
    };
    class DictExpr : public ValueExpr {
    public:
        vector<pair<unique_ptr<ExprNode>, unique_ptr<ExprNode>>> values;
        DictExpr(vector<pair<unique_ptr<ExprNode>, unique_ptr<ExprNode>>> values) : values(values) {}
    };
    class ValuesSequenceExpr : public SequenceExpr {
    public:
        vector<unique_ptr<ExprNode>> values;
        ValuesSequenceExpr(vector<unique_ptr<ExprNode>> values) : values(values) {}
    };
    class RangeSequenceExpr : public SequenceExpr {
    public:
        unique_ptr<ExprNode> start;
        unique_ptr<ExprNode> end;
        unique_ptr<ExprNode> step;
        RangeSequenceExpr(unique_ptr<ExprNode> start, unique_ptr<ExprNode> end, unique_ptr<ExprNode> step)
            : start(start), end(end), step(step) {}
    };
    class SetExpr : public ValueExpr {
    public:
        vector<unique_ptr<ExprNode>> values;
        SetExpr(vector<unique_ptr<ExprNode>> values) : values(values) {}
    };
    class SymbolExpr : public ValueExpr {
    public:
        string value;
        SymbolExpr(string value) : value(value) {}
    };
    class FqnExpr : public ValueExpr {
    public:
        unique_ptr<PackageNameExpr> packageName;
        unique_ptr<NameExpr> moduleName;
        FqnExpr(unique_ptr<PackageNameExpr> packageName, unique_ptr<NameExpr> moduleName)
            : packageName(packageName), moduleName(moduleName) {}
    };
    class PackageNameExpr : public ValueExpr {
    public:
        vector<NameExpr> parts;
        PackageNameExpr(vector<NameExpr> parts) : parts(parts) {}
    };
    class ModuleExpr : public ValueExpr {
    public:
        unique_ptr<FqnExpr> fqn;
        vector<string> exports;
        vector<RecordNode> records;
        vector<FunctionExpr> functions;
        ModuleExpr(unique_ptr<FqnExpr> fqn, vector<string> exports,
            vector<unique_ptr<RecordNode>> records, vector<unique_ptr<FunctionExpr>> functions)
            : fqn(fqn), exports(exports), records(records), functions(functions) {}
    };
    class RecordInstanceExpr : public ValueExpr {
    public:
        string recordType;
        vector<pair<unique_ptr<NameExpr>, unique_ptr<ExprNode>>> items;
        RecordInstanceExpr(string recordType, vector<pair<unique_ptr<NameExpr>, unique_ptr<ExprNode>>> items)
            : recordType(recordType), items(items) {}
    };

    class BodyWithGuards : public FunctionBody {
    public:
        unique_ptr<ExprNode> guard;
        unique_ptr<ExprNode> expr;
        BodyWithGuards(unique_ptr<ExprNode> guard, unique_ptr<ExprNode> expr) : guard(guard), expr(expr) {}
    };
    class BodyWithoutGuards : public FunctionBody {
    public:
        unique_ptr<ExprNode> expr;
        BodyWithoutGuards(unique_ptr<ExprNode> expr) : expr(expr) {}
    };
    class FunctionExpr : public ExprNode {
    public:
        string name;
        vector<unique_ptr<PatternNode>> patterns;
        vector<unique_ptr<FunctionBody>> bodies;
        FunctionExpr(string name, vector<unique_ptr<PatternNode>> patterns,
            vector<unique_ptr<FunctionBody>> bodies)
            : name(name), patterns(patterns), bodies(bodies) {}
    };

    class LogicalNotOpExpr : public OpExpr {
    public:
        unique_ptr<ExprNode> expr;
        LogicalNotOpExpr(unique_ptr<ExprNode> expr) : expr(expr) {}
    };
    class BinaryNotOpExpr : public OpExpr {
    public:
        unique_ptr<ExprNode> expr;
        BinaryNotOpExpr(unique_ptr<ExprNode> expr) : expr(expr) {}
    };

    class PowerExpr : public BinaryOpExpr {
    public:
        PowerExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class MultiplyExpr : public BinaryOpExpr {
    public:
        MultiplyExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class DivideExpr : public BinaryOpExpr {
    public:
        DivideExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class ModuloExpr : public BinaryOpExpr {
    public:
        ModuloExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class AddExpr : public BinaryOpExpr {
    public:
        AddExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class SubtractExpr : public BinaryOpExpr {
    public:
        SubtractExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class LeftShiftExpr : public BinaryOpExpr {
    public:
        LeftShiftExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class RightShiftExpr : public BinaryOpExpr {
    public:
        RightShiftExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class ZerofillRightShiftExpr : public BinaryOpExpr {
    public:
        ZerofillRightShiftExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class GteExpr : public BinaryOpExpr {
    public:
        GteExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class LteExpr : public BinaryOpExpr {
    public:
        LteExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class GtExpr : public BinaryOpExpr {
    public:
        GtExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class LtExpr : public BinaryOpExpr {
    public:
        LtExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class EqExpr : public BinaryOpExpr {
    public:
        EqExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class NeqExpr : public BinaryOpExpr {
    public:
        NeqExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class ConsLeftExpr : public BinaryOpExpr {
    public:
        ConsLeftExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class ConsRightExpr : public BinaryOpExpr {
    public:
        ConsRightExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class JoinExpr : public BinaryOpExpr {
    public:
        JoinExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class BitwiseAndExpr : public BinaryOpExpr {
    public:
        BitwiseAndExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class BitwiseXorExpr : public BinaryOpExpr {
    public:
        BitwiseXorExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class BitwiseOrExpr : public BinaryOpExpr {
    public:
        BitwiseOrExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class LogicalAndExpr : public BinaryOpExpr {
    public:
        LogicalAndExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class LogicalOrExpr : public BinaryOpExpr {
    public:
        LogicalOrExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class InExpr : public BinaryOpExpr {
    public:
        InExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class PipeLeftExpr : public BinaryOpExpr {
    public:
        PipeLeftExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };
    class PipeRightExpr : public BinaryOpExpr {
    public:
        PipeRightExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(left, right) {}
    };

    class LetExpr : public ExprNode {
    public:
        vector<unique_ptr<AliasExpr>> aliases;
        unique_ptr<ExprNode> expr;
        LetExpr(vector<unique_ptr<AliasExpr>> aliases, unique_ptr<ExprNode> expr)
            : aliases(aliases), expr(expr) {}
    };
    class IfExpr : public ExprNode {
    public:
        unique_ptr<ExprNode> condition;
        unique_ptr<ExprNode> thenExpr;
        optional<unique_ptr<ExprNode>> elseExpr;
        IfExpr(unique_ptr<ExprNode> condition, unique_ptr<ExprNode> thenExpr, optional<unique_ptr<ExprNode>> elseExpr)
            : condition(condition), thenExpr(thenExpr), elseExpr(elseExpr) {}
    };
    class ApplyExpr : public ExprNode {
    public:
        CallExpr* call;
        vector<variant<unique_ptr<ExprNode>, unique_ptr<ValueExpr>>> args;
        ApplyExpr(unique_ptr<CallExpr> call, vector<variant<unique_ptr<ExprNode>, unique_ptr<ValueExpr>>> args)
            : call(call), args(args) {}
    };
    class CaseExpr : public ExprNode {
    public:
        unique_ptr<ExprNode> expr;
        vector<unique_ptr<PatternExpr>> patterns;
        CaseExpr(unique_ptr<ExprNode> expr, vector<unique_ptr<PatternExpr>> patterns)
            : expr(expr), patterns(patterns) {}
    };
    class DoExpr : public ExprNode {
    public:
        vector<variant<unique_ptr<AliasExpr>, unique_ptr<ExprNode>>> steps;
        DoExpr(vector<variant<unique_ptr<AliasExpr>, unique_ptr<ExprNode>>> steps) : steps(steps) {}
    };
    class ImportExpr : public ExprNode {
    public:
        vector<unique_ptr<ImportClauseExpr>> clauses;
        unique_ptr<ExprNode> expr;
        ImportExpr(vector<unique_ptr<ImportClauseExpr>> clauses, unique_ptr<ExprNode> expr)
            : clauses(clauses), expr(expr) {}
    };
    class TryCatchExpr : public ExprNode {
    public:
        unique_ptr<ExprNode> tryExpr;
        CatchExpr catchExpr;
        TryCatchExpr(unique_ptr<ExprNode> tryExpr, CatchExpr catchExpr)
            : tryExpr(tryExpr), catchExpr(catchExpr) {}
    };
    class CatchExpr : public ExprNode {
    public:
        vector<unique_ptr<CatchPatternExpr>> patterns;
        CatchExpr(vector<unique_ptr<CatchPatternExpr>> patterns) : patterns(patterns) {}
    };
    class RaiseExpr : public ExprNode {
    public:
        SymbolExpr symbol;
        StringExpr message;
        RaiseExpr(SymbolExpr symbol, StringExpr message)
            : symbol(symbol), message(message) {}
    };
    class WithExpr : public ExprNode {
    public:
        unique_ptr<ExprNode> contextExpr;
        optional<unique_ptr<NameExpr>> name;
        unique_ptr<ExprNode> bodyExpr;
        WithExpr(unique_ptr<ExprNode> contextExpr, optional<NameExpr> name, unique_ptr<ExprNode> bodyExpr)
            : contextExpr(contextExpr), name(name), bodyExpr(bodyExpr) {}
    };
    class FieldAccessExpr : public ExprNode {
    public:
        unique_ptr<IdentifierExpr> identifier;
        unique_ptr<NameExpr> name;
        FieldAccessExpr(unique_ptr<IdentifierExpr> identifier, unique_ptr<NameExpr> name)
            : identifier(identifier), name(name) {}
    };
    class FieldUpdateExpr : public ExprNode {
    public:
        unique_ptr<IdentifierExpr> identifier;
        vector<pair<unique_ptr<NameExpr>, unique_ptr<ExprNode>>> updates;
        FieldUpdateExpr(unique_ptr<IdentifierExpr> identifier, vector<pair<NameExpr, unique_ptr<ExprNode>>> updates)
            : identifier(identifier), updates(updates) {}
    };

    class LambdaAlias : public AliasExpr {
    public:
        unique_ptr<NameExpr> name;
        unique_ptr<FunctionExpr> lambda;
        LambdaAlias(unique_ptr<NameExpr> name, unique_ptr<FunctionExpr> lambda) : name(name), lambda(lambda) {}
    };
    class ModuleAlias : public AliasExpr {
    public:
        unique_ptr<NameExpr> name;
        unique_ptr<ModuleExpr> module;
        ModuleAlias(unique_ptr<NameExpr> name, unique_ptr<ModuleExpr> module) : name(name), module(module) {}
    };
    class ValueAlias : public AliasExpr {
    public:
        unique_ptr<IdentifierExpr> identifier;
        unique_ptr<ExprNode> expr;
        ValueAlias(unique_ptr<IdentifierExpr> identifier, unique_ptr<ExprNode> expr)
            : identifier(identifier), expr(expr) {}
    };
    class PatternAlias : public AliasExpr {
    public:
        unique_ptr<PatternNode> pattern;
        unique_ptr<ExprNode> expr;
        PatternAlias(unique_ptr<PatternNode> pattern, unique_ptr<ExprNode> expr)
            : pattern(pattern), expr(expr) {}
    };
    class FqnAlias : public AliasExpr {
    public:
        unique_ptr<NameExpr> name;
        unique_ptr<FqnExpr> fqn;
        FqnAlias(unique_ptr<NameExpr> name, unique_ptr<FqnExpr> fqn) : name(name), fqn(fqn) {}
    };
    class FunctionAlias : public AliasExpr {
    public:
        unique_ptr<NameExpr> name;
        unique_ptr<NameExpr> alias;
        FunctionAlias(unique_ptr<NameExpr> name, unique_ptr<NameExpr> alias) : name(name), alias(alias) {}
    };

    class AliasCall : public CallExpr {
    public:
        unique_ptr<NameExpr> alias;
        unique_ptr<NameExpr> funName;
        AliasCall(unique_ptr<NameExpr> alias, unique_ptr<NameExpr> funName)
            : alias(alias), funName(funName) {}
    };
    class NameCall : public CallExpr {
    public:
        unique_ptr<NameExpr> name;
        NameCall(unique_ptr<NameExpr> name) : name(name) {}
    };
    class ModuleCall : public CallExpr {
    public:
        variant<unique_ptr<FqnExpr>, unique_ptr<ExprNode>> fqn;
        unique_ptr<NameExpr> funName;
        ModuleCall(variant<unique_ptr<FqnExpr>, unique_ptr<ExprNode>> fqn, unique_ptr<NameExpr> funName)
            : fqn(fqn), funName(funName) {}
    };

    class ModuleImport : public ImportClauseExpr {
    public:
        unique_ptr<FqnExpr> fqn;
        unique_ptr<NameExpr> name;
        ModuleImport(unique_ptr<FqnExpr> fqn, unique_ptr<NameExpr> name) : fqn(fqn), name(name) {}
    };
    class FunctionsImport : public ImportClauseExpr {
    public:
        vector<FunctionAlias> aliases;
        unique_ptr<FqnExpr> fromFqn;
        FunctionsImport(vector<FunctionAlias> aliases, unique_ptr<FqnExpr> fromFqn)
            : aliases(aliases), fromFqn(fromFqn) {}
    };

    class SeqGeneratorExpr : public GeneratorExpr {
    public:
        unique_ptr<ExprNode> reducerExpr;
        unique_ptr<CollectionExtractorExpr> collectionExtractor;
        unique_ptr<ExprNode> stepExpression;
        SeqGeneratorExpr(unique_ptr<ExprNode> reducerExpr, unique_ptr<CollectionExtractorExpr> collectionExtractor,
            unique_ptr<ExprNode> stepExpression)
            : reducerExpr(reducerExpr), collectionExtractor(collectionExtractor), stepExpression(stepExpression) {}
    };
    class SetGeneratorExpr : public GeneratorExpr {
    public:
        unique_ptr<ExprNode> reducerExpr;
        unique_ptr<CollectionExtractorExpr> collectionExtractor;
        unique_ptr<ExprNode> stepExpression;
        SetGeneratorExpr(unique_ptr<ExprNode> reducerExpr, unique_ptr<CollectionExtractorExpr> collectionExtractor,
            unique_ptr<ExprNode> stepExpression)
            : reducerExpr(reducerExpr), collectionExtractor(collectionExtractor), stepExpression(stepExpression) {}
    };
    class DictGeneratorExpr : public GeneratorExpr {
    public:
        unique_ptr<DictGeneratorReducer> reducerExpr;
        unique_ptr<CollectionExtractorExpr> collectionExtractor;
        unique_ptr<ExprNode> stepExpression;
        DictGeneratorExpr(unique_ptr<DictGeneratorReducer> reducerExpr, unique_ptr<CollectionExtractorExpr> collectionExtractor,
            unique_ptr<ExprNode> stepExpression)
            : reducerExpr(reducerExpr), collectionExtractor(collectionExtractor), stepExpression(stepExpression) {}
    };
    class DictGeneratorReducer : public ExprNode {
    public:
        unique_ptr<ExprNode> key;
        unique_ptr<ExprNode> value;
        DictGeneratorReducer(unique_ptr<ExprNode> key, unique_ptr<ExprNode> value) : key(key), value(value) {}
    };

    class UnderscorePattern : public UnderscoreNode {};

    using IdentifierOrUnderscore = variant<unique_ptr<IdentifierExpr>, unique_ptr<UnderscoreNode>>;

    class ValueCollectionExtractorExpr : public CollectionExtractorExpr {
    public:
        unique_ptr<IdentifierOrUnderscore> expr;
        ValueCollectionExtractorExpr(IdentifierOrUnderscore expr) : expr(expr) {}
    };
    class KeyValueCollectionExtractorExpr : public CollectionExtractorExpr {
    public:
        unique_ptr<IdentifierOrUnderscore> keyExpr;
        unique_ptr<IdentifierOrUnderscore> valueExpr;
        KeyValueCollectionExtractorExpr(unique_ptr<IdentifierOrUnderscore> keyExpr, unique_ptr<IdentifierOrUnderscore> valueExpr)
            : keyExpr(keyExpr), valueExpr(valueExpr) {}
    };

    class PatternWithGuards : public AstNode {
    public:
        unique_ptr<ExprNode> guard;
        unique_ptr<ExprNode> exprNode;
        PatternWithGuards(unique_ptr<ExprNode> guard, unique_ptr<ExprNode> exprNode)
            : guard(guard), exprNode(exprNode) {}
    };
    class PatternWithoutGuards : public AstNode {
    public:
        unique_ptr<ExprNode> exprNode;
        PatternWithoutGuards(unique_ptr<ExprNode> exprNode) : exprNode(exprNode) {}
    };
    class PatternExpr : public ExprNode {
    public:
        variant<unique_ptr<Pattern>, unique_ptr<PatternWithoutGuards>, vector<unique_ptr<PatternWithGuards>>> patternExpr;
        PatternExpr(variant<unique_ptr<Pattern>, unique_ptr<PatternWithoutGuards>, vector<unique_ptr<PatternWithGuards>>> patternExpr)
            : patternExpr(patternExpr) {}
    };
    class CatchPatternExpr : public ExprNode {
    public:
        unique_ptr<Pattern> matchPattern;
        variant<unique_ptr<PatternWithoutGuards>, vector<unique_ptr<PatternWithGuards>>> pattern;
        CatchPatternExpr(unique_ptr<Pattern> matchPattern, variant<unique_ptr<PatternWithoutGuards>, vector<unique_ptr<PatternWithGuards>>> pattern)
            : matchPattern(matchPattern), pattern(pattern) {}
    };

    class PatternValue : public PatternNode {
    public:
        variant<unique_ptr<LiteralExpr<nullptr_t>>, unique_ptr<LiteralExpr<any>>, unique_ptr<SymbolExpr>, unique_ptr<IdentifierExpr>> expr;
        PatternValue(variant<unique_ptr<LiteralExpr<nullptr_t>>, unique_ptr<LiteralExpr<void*>>, unique_ptr<SymbolExpr>, unique_ptr<IdentifierExpr>> expr)
            : expr(expr) {}
    };
    using Pattern = variant<unique_ptr<UnderscoreNode>, unique_ptr<PatternValue>, unique_ptr<DataStructurePattern>, unique_ptr<AsDataStructurePattern>>;
    using PatternWithoutSequence = variant<unique_ptr<UnderscoreNode>, unique_ptr<PatternValue>, unique_ptr<TuplePattern>, unique_ptr<DictPattern>>;
    using DataStructurePattern = variant<unique_ptr<TuplePattern>, unique_ptr<SequencePattern>, unique_ptr<DictPattern>, unique_ptr<RecordPattern>>;

    class AsDataStructurePattern : public PatternNode {
    public:
        unique_ptr<IdentifierExpr> identifier;
        unique_ptr<DataStructurePattern> pattern;
        AsDataStructurePattern(unique_ptr<IdentifierExpr> identifier, unique_ptr<DataStructurePattern> pattern)
            : identifier(identifier), pattern(pattern) {}
    };
    class TuplePattern : public PatternNode {
    public:
        vector<unique_ptr<Pattern>> patterns;
        TuplePattern(vector<unique_ptr<Pattern>> patterns) : patterns(patterns) {}
    };
    using SequencePattern = variant<SeqPattern, HeadTailsPattern, TailsHeadPattern, HeadTailsHeadPattern>;
    class SeqPattern : public PatternNode {
    public:
        vector<unique_ptr<Pattern>> patterns;
        SeqPattern(vector<unique_ptr<Pattern>> patterns) : patterns(patterns) {}
    };
    class HeadTailsPattern : public PatternNode {
    public:
        vector<unique_ptr<PatternWithoutSequence>> heads;
        unique_ptr<TailPattern> tail;
        HeadTailsPattern(vector<unique_ptr<PatternWithoutSequence>> heads, unique_ptr<TailPattern> tail)
            : heads(heads), tail(tail) {}
    };
    class TailsHeadPattern : public PatternNode {
    public:
        unique_ptr<TailPattern> tail;
        vector<unique_ptr<PatternWithoutSequence>> heads;
        TailsHeadPattern(unique_ptr<TailPattern> tail, vector<unique_ptr<PatternWithoutSequence>> heads)
            : tail(tail), heads(heads) {}
    };
    using TailPattern = variant<unique_ptr<IdentifierExpr>, unique_ptr<SequenceExpr>, unique_ptr<UnderscoreNode>, unique_ptr<StringExpr>>;
    class HeadTailsHeadPattern : public PatternNode {
    public:
        vector<unique_ptr<PatternWithoutSequence>> left;
        unique_ptr<TailPattern> tail;
        vector<unique_ptr<PatternWithoutSequence>> right;
        HeadTailsHeadPattern(vector<unique_ptr<PatternWithoutSequence>> left, unique_ptr<TailPattern> tail,
            vector<unique_ptr<PatternWithoutSequence>> right)
            : left(left), tail(tail), right(right) {}
    };
    class DictPattern : public PatternNode {
    public:
        vector<pair<unique_ptr<PatternValue>, unique_ptr<Pattern>>> keyValuePairs;
        DictPattern(vector<pair<unique_ptr<PatternValue>, unique_ptr<Pattern>>> keyValuePairs)
            : keyValuePairs(keyValuePairs) {}
    };
    class RecordPattern : public PatternNode {
    public:
        string recordType;
        vector<pair<unique_ptr<NameExpr>, unique_ptr<Pattern>>> items;
        RecordPattern(string recordType, vector<pair<unique_ptr<NameExpr>, unique_ptr<Pattern>>> items)
            : recordType(recordType), items(items) {}
    };
}