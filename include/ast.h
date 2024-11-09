#pragma once

#include <any>
#include <string>
#include <utility>
#include <vector>
#include <variant>
#include <optional>
#include <memory>

namespace yonac::ast {
    class IdentifierExpr;
    class SequenceExpr;
    class StringExpr;
    class CatchPatternExpr;
    class CatchExpr;
    class DictGeneratorReducer;
    class AsDataStructurePattern;
    class PatternValue;
    class RecordPattern;
    class DictPattern;
    class TuplePattern;
    class SeqPattern;
    class HeadTailsPattern;
    class TailsHeadPattern;
    class HeadTailsHeadPattern;
    class PatternExpr;

    using namespace std;

    class Token {};

    template <typename T>
    class AstVisitor {
    public:
        virtual ~AstVisitor() = default;
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

    using PatternWithoutSequence = variant<UnderscoreNode, PatternValue, TuplePattern, DictPattern>;
    using SequencePattern = variant<SeqPattern, HeadTailsPattern, TailsHeadPattern, HeadTailsHeadPattern>;
    using DataStructurePattern = variant<TuplePattern, SequencePattern, DictPattern, RecordPattern>;
    using Pattern = variant<UnderscoreNode, PatternValue, DataStructurePattern, AsDataStructurePattern>;
    using TailPattern = variant<IdentifierExpr, SequenceExpr, UnderscoreNode, StringExpr>;

    template <typename T>
    class LiteralExpr : public ValueExpr {
    protected:
        unique_ptr<T> value;

    public:
        explicit LiteralExpr(T value) : value(move(value)) {}
    };
    class OpExpr : public ExprNode {};
    class BinaryOpExpr : public OpExpr {
    protected:
        unique_ptr<ExprNode> left;
        unique_ptr<ExprNode> right;

    public:
        explicit BinaryOpExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : left(move(left)), right(move(right)) {}
    };
    class AliasExpr : public ExprNode {};
    class CallExpr : public ExprNode {};
    class ImportClauseExpr : public ExprNode {};
    class GeneratorExpr : public ExprNode {};
    class CollectionExtractorExpr : public ExprNode {};
    class SequenceExpr : public ExprNode {};

    class FunctionBody : public AstNode {};

    class NameExpr : public ExprNode {
    protected:
        string value;

    public:
        explicit NameExpr(string value) : value(std::move(value)) {}
    };

    class IdentifierExpr : public ValueExpr {
    protected:
        unique_ptr<NameExpr> name;

    public:
        explicit IdentifierExpr(unique_ptr<NameExpr> name) : name(move(name)) {}
    };

    class RecordNode : public AstNode {
    protected:
        string recordType;
        vector<unique_ptr<IdentifierExpr>> identifiers;

    public:
        explicit RecordNode(string recordType, vector<unique_ptr<IdentifierExpr>> identifiers)
            : AstNode(), recordType(move(recordType)), identifiers(move(identifiers)) {}
    };

    class TrueLiteralExpr : public LiteralExpr<bool> {
    public:
        explicit TrueLiteralExpr() : LiteralExpr<bool>(true) {}
    };
    class FalseLiteralExpr : public LiteralExpr<bool> {
    public:
        explicit FalseLiteralExpr() : LiteralExpr<bool>(false) {}
    };
    class FloatExpr : public LiteralExpr<float> {
    public:
        explicit FloatExpr(const float value) : LiteralExpr<float>(value) {}
    };
    class IntegerExpr : public LiteralExpr<int> {
    public:
        explicit IntegerExpr(const int value) : LiteralExpr<int>(value) {}
    };
    class ByteExpr : public LiteralExpr<unsigned char> {
    public:
        explicit ByteExpr(unsigned const char value) : LiteralExpr<unsigned char>(value) {}
    };
    class StringExpr : public LiteralExpr<string> {
    public:
        explicit StringExpr(string value) : LiteralExpr<string>(move(value)) {}
    };
    class CharacterExpr : public LiteralExpr<char> {
    public:
        explicit CharacterExpr(const char value) : LiteralExpr<char>(value) {}
    };

    class UnitExpr : public LiteralExpr<nullptr_t> {
    public:
        UnitExpr() : LiteralExpr<nullptr_t>(nullptr) {}
    };
    class TupleExpr : public ValueExpr {
    protected:
        vector<unique_ptr<ExprNode>> values;

    public:
        explicit TupleExpr(vector<unique_ptr<ExprNode>> values) : values(move(values)) {}
    };
    class DictExpr : public ValueExpr {
    protected:
        vector<pair<unique_ptr<ExprNode>, unique_ptr<ExprNode>>> values;

    public:
        explicit DictExpr(vector<pair<unique_ptr<ExprNode>, unique_ptr<ExprNode>>> values) : values(move(values)) {}
    };
    class ValuesSequenceExpr : public SequenceExpr {
    protected:
        vector<unique_ptr<ExprNode>> values;

    public:
        explicit ValuesSequenceExpr(vector<unique_ptr<ExprNode>> values) : values(move(values)) {}
    };
    class RangeSequenceExpr : public SequenceExpr {
    protected:
        unique_ptr<ExprNode> start;
        unique_ptr<ExprNode> end;
        unique_ptr<ExprNode> step;

    public:
        explicit RangeSequenceExpr(unique_ptr<ExprNode> start, unique_ptr<ExprNode> end, unique_ptr<ExprNode> step)
            : start(move(start)), end(move(end)), step(move(step)) {}
    };
    class SetExpr : public ValueExpr {
    protected:
        vector<unique_ptr<ExprNode>> values;

    public:
        explicit SetExpr(vector<unique_ptr<ExprNode>> values) : values(move(values)) {}
    };
    class SymbolExpr : public ValueExpr {
    protected:
        string value;

    public:
        explicit SymbolExpr(string value) : value(move(value)) {}
    };
    class PackageNameExpr : public ValueExpr {
    protected:
        vector<NameExpr> parts;

    public:
        explicit PackageNameExpr(vector<NameExpr> parts) : parts(move(parts)) {}
    };
    class FqnExpr : public ValueExpr {
    protected:
        unique_ptr<PackageNameExpr> packageName;
        unique_ptr<NameExpr> moduleName;

    public:
        explicit FqnExpr(unique_ptr<PackageNameExpr> packageName, unique_ptr<NameExpr> moduleName)
            : packageName(move(packageName)), moduleName(move(moduleName)) {}
    };
    class FunctionExpr : public ExprNode {
    protected:
        string name;
        vector<unique_ptr<PatternNode>> patterns;
        vector<unique_ptr<FunctionBody>> bodies;

    public:
        explicit FunctionExpr(string name, vector<unique_ptr<PatternNode>> patterns,
            vector<unique_ptr<FunctionBody>> bodies)
            : name(move(name)), patterns(move(patterns)), bodies(move(bodies)) {}
    };

    class ModuleExpr : public ValueExpr {
    protected:
        unique_ptr<unique_ptr<FqnExpr>> fqn;
        vector<string> exports;
        vector<unique_ptr<RecordNode>> records;
        vector<unique_ptr<FunctionExpr>> functions;

    public:
        explicit ModuleExpr(unique_ptr<unique_ptr<FqnExpr>> fqn, vector<string> exports,
            vector<unique_ptr<RecordNode>> records, vector<unique_ptr<FunctionExpr>> functions)
            : fqn(move(fqn)), exports(move(exports)), records(move(records)), functions(move(functions)) {}
    };
    class RecordInstanceExpr : public ValueExpr {
    protected:
        string recordType;
        vector<pair<unique_ptr<NameExpr>, unique_ptr<ExprNode>>> items;

    public:
        explicit RecordInstanceExpr(string recordType, vector<pair<unique_ptr<NameExpr>, unique_ptr<ExprNode>>> items)
            : recordType(move(recordType)), items(move(items)) {}
    };

    class BodyWithGuards : public FunctionBody {
    protected:
        unique_ptr<ExprNode> guard;
        unique_ptr<ExprNode> expr;

    public:
        explicit BodyWithGuards(unique_ptr<ExprNode> guard, unique_ptr<ExprNode> expr) : guard(move(guard)), expr(move(expr)) {}
    };
    class BodyWithoutGuards : public FunctionBody {
    protected:
        unique_ptr<ExprNode> expr;

    public:
        explicit BodyWithoutGuards(unique_ptr<ExprNode> expr) : expr(move(expr)) {}
    };
    class LogicalNotOpExpr : public OpExpr {
    protected:
        unique_ptr<ExprNode> expr;

    public:
        explicit LogicalNotOpExpr(unique_ptr<ExprNode> expr) : expr(move(expr)) {}
    };
    class BinaryNotOpExpr : public OpExpr {
    protected:
        unique_ptr<ExprNode> expr;

    public:
        explicit BinaryNotOpExpr(unique_ptr<ExprNode> expr) : expr(move(expr)) {}
    };

    class PowerExpr : public BinaryOpExpr {
    public:
        explicit PowerExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class MultiplyExpr : public BinaryOpExpr {
    public:
        explicit MultiplyExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class DivideExpr : public BinaryOpExpr {
    public:
        explicit DivideExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class ModuloExpr : public BinaryOpExpr {
    public:
        explicit ModuloExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class AddExpr : public BinaryOpExpr {
    public:
        explicit AddExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class SubtractExpr : public BinaryOpExpr {
    public:
        explicit SubtractExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class LeftShiftExpr : public BinaryOpExpr {
    public:
        explicit LeftShiftExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class RightShiftExpr : public BinaryOpExpr {
    public:
        explicit RightShiftExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class ZerofillRightShiftExpr : public BinaryOpExpr {
    public:
        explicit ZerofillRightShiftExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class GteExpr : public BinaryOpExpr {
    public:
        explicit GteExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class LteExpr : public BinaryOpExpr {
    public:
        explicit LteExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class GtExpr : public BinaryOpExpr {
    public:
        explicit GtExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class LtExpr : public BinaryOpExpr {
    public:
        explicit LtExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class EqExpr : public BinaryOpExpr {
    public:
        explicit EqExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class NeqExpr : public BinaryOpExpr {
    public:
        explicit NeqExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class ConsLeftExpr : public BinaryOpExpr {
    public:
        explicit ConsLeftExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class ConsRightExpr : public BinaryOpExpr {
    public:
        explicit ConsRightExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class JoinExpr : public BinaryOpExpr {
    public:
        explicit JoinExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class BitwiseAndExpr : public BinaryOpExpr {
    public:
        explicit BitwiseAndExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class BitwiseXorExpr : public BinaryOpExpr {
    public:
        explicit BitwiseXorExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class BitwiseOrExpr : public BinaryOpExpr {
    public:
        explicit BitwiseOrExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class LogicalAndExpr : public BinaryOpExpr {
    public:
        explicit LogicalAndExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class LogicalOrExpr : public BinaryOpExpr {
    public:
        explicit LogicalOrExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class InExpr : public BinaryOpExpr {
    public:
        explicit InExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class PipeLeftExpr : public BinaryOpExpr {
    public:
        explicit PipeLeftExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };
    class PipeRightExpr : public BinaryOpExpr {
    public:
        explicit PipeRightExpr(unique_ptr<ExprNode> left, unique_ptr<ExprNode> right) : BinaryOpExpr(move(left), move(right)) {}
    };

    class LetExpr : public ExprNode {
    protected:
        vector<unique_ptr<AliasExpr>> aliases;
        unique_ptr<ExprNode> expr;

    public:
        explicit LetExpr(vector<unique_ptr<AliasExpr>> aliases, unique_ptr<ExprNode> expr)
            : aliases(move(aliases)), expr(move(expr)) {}
    };
    class IfExpr : public ExprNode {
    protected:
        unique_ptr<ExprNode> condition;
        unique_ptr<ExprNode> thenExpr;
        optional<unique_ptr<ExprNode>> elseExpr;

    public:
        explicit IfExpr(unique_ptr<ExprNode> condition, unique_ptr<ExprNode> thenExpr, optional<unique_ptr<ExprNode>> elseExpr)
            : condition(move(condition)), thenExpr(move(thenExpr)), elseExpr(move(elseExpr)) {}
    };
    class ApplyExpr : public ExprNode {
    protected:
        unique_ptr<CallExpr> call;
        vector<variant<unique_ptr<ExprNode>, unique_ptr<ValueExpr>>> args;

    public:
        explicit ApplyExpr(unique_ptr<CallExpr> call, vector<variant<unique_ptr<ExprNode>, unique_ptr<ValueExpr>>> args)
            : call(move(call)), args(move(args)) {}
    };
    class CaseExpr : public ExprNode {
    protected:
        unique_ptr<ExprNode> expr;
        vector<unique_ptr<PatternExpr>> patterns;

    public:
        explicit CaseExpr(unique_ptr<ExprNode> expr, vector<unique_ptr<PatternExpr>> patterns)
            : expr(move(expr)), patterns(move(patterns)) {}
    };
    class DoExpr : public ExprNode {
    protected:
        vector<variant<unique_ptr<AliasExpr>, unique_ptr<ExprNode>>> steps;

    public:
        explicit DoExpr(vector<variant<unique_ptr<AliasExpr>, unique_ptr<ExprNode>>> steps) : steps(steps) {}
    };
    class ImportExpr : public ExprNode {
    protected:
        vector<unique_ptr<ImportClauseExpr>> clauses;
        unique_ptr<ExprNode> expr;

    public:
        explicit ImportExpr(vector<unique_ptr<ImportClauseExpr>> clauses, unique_ptr<ExprNode> expr)
            : clauses(move(clauses)), expr(move(expr)) {}
    };
    class TryCatchExpr : public ExprNode {
    protected:
        unique_ptr<ExprNode> tryExpr;
        unique_ptr<CatchExpr> catchExpr;

    public:
        explicit TryCatchExpr(unique_ptr<ExprNode> tryExpr, unique_ptr<CatchExpr> catchExpr)
            : tryExpr(move(tryExpr)), catchExpr(move(catchExpr)) {}
    };
    class CatchExpr : public ExprNode {
    protected:
        vector<unique_ptr<CatchPatternExpr>> patterns;

    public:
        explicit CatchExpr(vector<unique_ptr<CatchPatternExpr>> patterns) : patterns(patterns) {}
    };
    class RaiseExpr : public ExprNode {
    protected:
        unique_ptr<SymbolExpr> symbol;
        unique_ptr<StringExpr> message;

    public:
        explicit RaiseExpr(unique_ptr<SymbolExpr> symbol, unique_ptr<StringExpr> message)
            : symbol(move(symbol)), message(move(message)) {}
    };
    class WithExpr : public ExprNode {
    protected:
        unique_ptr<ExprNode> contextExpr;
        optional<unique_ptr<NameExpr>> name;
        unique_ptr<ExprNode> bodyExpr;

    public:
        explicit WithExpr(unique_ptr<ExprNode> contextExpr, optional<unique_ptr<NameExpr>> name, unique_ptr<ExprNode> bodyExpr)
            : contextExpr(move(contextExpr)), name(move(name)), bodyExpr(move(bodyExpr)) {}
    };
    class FieldAccessExpr : public ExprNode {
    protected:
        unique_ptr<IdentifierExpr> identifier;
        unique_ptr<NameExpr> name;

    public:
        explicit FieldAccessExpr(unique_ptr<IdentifierExpr> identifier, unique_ptr<NameExpr> name)
            : identifier(move(identifier)), name(move(name)) {}
    };
    class FieldUpdateExpr : public ExprNode {
    protected:
        unique_ptr<IdentifierExpr> identifier;
        vector<pair<unique_ptr<NameExpr>, unique_ptr<ExprNode>>> updates;

    public:
        explicit FieldUpdateExpr(unique_ptr<IdentifierExpr> identifier, vector<pair<unique_ptr<NameExpr>, unique_ptr<ExprNode>>> updates)
            : identifier(move(identifier)), updates(move(updates)) {}
    };

    class LambdaAlias : public AliasExpr {
    protected:
        unique_ptr<NameExpr> name;
        unique_ptr<FunctionExpr> lambda;

    public:
        explicit LambdaAlias(unique_ptr<NameExpr> name, unique_ptr<FunctionExpr> lambda) : name(move(name)), lambda(move(lambda)) {}
    };
    class ModuleAlias : public AliasExpr {
    protected:
        unique_ptr<NameExpr> name;
        unique_ptr<ModuleExpr> module;

    public:
        explicit ModuleAlias(unique_ptr<NameExpr> name, unique_ptr<ModuleExpr> module) : name(move(name)), module(move(module)) {}
    };
    class ValueAlias : public AliasExpr {
    protected:
        unique_ptr<IdentifierExpr> identifier;
        unique_ptr<ExprNode> expr;

    public:
        explicit ValueAlias(unique_ptr<IdentifierExpr> identifier, unique_ptr<ExprNode> expr)
            : identifier(move(identifier)), expr(move(expr)) {}
    };
    class PatternAlias : public AliasExpr {
    protected:
        unique_ptr<PatternNode> pattern;
        unique_ptr<ExprNode> expr;

    public:
        explicit PatternAlias(unique_ptr<PatternNode> pattern, unique_ptr<ExprNode> expr)
            : pattern(move(pattern)), expr(move(expr)) {}
    };
    class FqnAlias : public AliasExpr {
    protected:
        unique_ptr<NameExpr> name;
        unique_ptr<FqnExpr> fqn;

    public:
        explicit FqnAlias(unique_ptr<NameExpr> name, unique_ptr<FqnExpr> fqn) : name(move(name)), fqn(move(fqn)) {}
    };
    class FunctionAlias : public AliasExpr {
    protected:
        unique_ptr<NameExpr> name;
        unique_ptr<NameExpr> alias;

    public:
        explicit FunctionAlias(unique_ptr<NameExpr> name, unique_ptr<NameExpr> alias) : name(move(name)), alias(move(alias)) {}
    };

    class AliasCall : public CallExpr {
    protected:
        unique_ptr<NameExpr> alias;
        unique_ptr<NameExpr> funName;

    public:
        explicit AliasCall(unique_ptr<NameExpr> alias, unique_ptr<NameExpr> funName)
            : alias(move(alias)), funName(move(funName)) {}
    };
    class NameCall : public CallExpr {
    protected:
        unique_ptr<NameExpr> name;

    public:
        explicit NameCall(unique_ptr<NameExpr> name) : name(move(name)) {}
    };
    class ModuleCall : public CallExpr {
    protected:
        variant<unique_ptr<FqnExpr>, unique_ptr<ExprNode>> fqn;
        unique_ptr<NameExpr> funName;

    public:
        explicit ModuleCall(variant<unique_ptr<FqnExpr>, unique_ptr<ExprNode>> fqn, unique_ptr<NameExpr> funName)
            : fqn(move(fqn)), funName(move(funName)) {}
    };

    class ModuleImport : public ImportClauseExpr {
    protected:
        unique_ptr<FqnExpr> fqn;
        unique_ptr<NameExpr> name;

    public:
        explicit ModuleImport(unique_ptr<FqnExpr> fqn, unique_ptr<NameExpr> name) : fqn(move(fqn)), name(move(name)) {}
    };
    class FunctionsImport : public ImportClauseExpr {
    protected:
        vector<unique_ptr<FunctionAlias>> aliases;
        unique_ptr<FqnExpr> fromFqn;

    public:
        explicit FunctionsImport(vector<unique_ptr<FunctionAlias>> aliases, unique_ptr<FqnExpr> fromFqn)
            : aliases(aliases), fromFqn(move(fromFqn)) {}
    };

    class SeqGeneratorExpr : public GeneratorExpr {
    protected:
        unique_ptr<ExprNode> reducerExpr;
        unique_ptr<CollectionExtractorExpr> collectionExtractor;
        unique_ptr<ExprNode> stepExpression;

    public:
        explicit SeqGeneratorExpr(unique_ptr<ExprNode> reducerExpr, unique_ptr<CollectionExtractorExpr> collectionExtractor,
            unique_ptr<ExprNode> stepExpression)
            : reducerExpr(move(reducerExpr)), collectionExtractor(move(collectionExtractor)), stepExpression(move(stepExpression)) {}
    };
    class SetGeneratorExpr : public GeneratorExpr {
    protected:
        unique_ptr<ExprNode> reducerExpr;
        unique_ptr<CollectionExtractorExpr> collectionExtractor;
        unique_ptr<ExprNode> stepExpression;

    public:
        explicit SetGeneratorExpr(unique_ptr<ExprNode> reducerExpr, unique_ptr<CollectionExtractorExpr> collectionExtractor,
            unique_ptr<ExprNode> stepExpression)
            : reducerExpr(move(reducerExpr)), collectionExtractor(move(collectionExtractor)), stepExpression(move(stepExpression)) {}
    };
    class DictGeneratorExpr : public GeneratorExpr {
    protected:
        unique_ptr<DictGeneratorReducer> reducerExpr;
        unique_ptr<CollectionExtractorExpr> collectionExtractor;
        unique_ptr<ExprNode> stepExpression;

    public:
        explicit DictGeneratorExpr(unique_ptr<DictGeneratorReducer> reducerExpr, unique_ptr<CollectionExtractorExpr> collectionExtractor,
            unique_ptr<ExprNode> stepExpression)
            : reducerExpr(move(reducerExpr)), collectionExtractor(move(collectionExtractor)), stepExpression(move(stepExpression)) {}
    };
    class DictGeneratorReducer : public ExprNode {
    protected:
        unique_ptr<ExprNode> key;
        unique_ptr<ExprNode> value;

    public:
        explicit DictGeneratorReducer(unique_ptr<ExprNode> key, unique_ptr<ExprNode> value) : key(move(key)), value(move(value)) {}
    };

    class UnderscorePattern : public UnderscoreNode {};

    using IdentifierOrUnderscore = variant<unique_ptr<IdentifierExpr>, unique_ptr<UnderscoreNode>>;

    class ValueCollectionExtractorExpr : public CollectionExtractorExpr {
    protected:
        unique_ptr<IdentifierOrUnderscore> expr;

    public:
        explicit ValueCollectionExtractorExpr(unique_ptr<IdentifierOrUnderscore> expr) : expr(move(expr)) {}
    };
    class KeyValueCollectionExtractorExpr : public CollectionExtractorExpr {
    protected:
        unique_ptr<IdentifierOrUnderscore> keyExpr;
        unique_ptr<IdentifierOrUnderscore> valueExpr;

    public:
        explicit KeyValueCollectionExtractorExpr(unique_ptr<IdentifierOrUnderscore> keyExpr, unique_ptr<IdentifierOrUnderscore> valueExpr)
            : keyExpr(move(keyExpr)), valueExpr(move(valueExpr)) {}
    };

    class PatternWithGuards : public AstNode {
    protected:
        unique_ptr<ExprNode> guard;
        unique_ptr<ExprNode> exprNode;

    public:
        explicit PatternWithGuards(unique_ptr<ExprNode> guard, unique_ptr<ExprNode> exprNode)
            : guard(move(guard)), exprNode(move(exprNode)) {}
    };
    class PatternWithoutGuards : public AstNode {
    protected:
        unique_ptr<ExprNode> exprNode;

    public:
        explicit PatternWithoutGuards(unique_ptr<ExprNode> exprNode) : exprNode(move(exprNode)) {}
    };
    class PatternExpr : public ExprNode {
    protected:
        variant<unique_ptr<Pattern>, unique_ptr<PatternWithoutGuards>, vector<unique_ptr<PatternWithGuards>>> patternExpr;

    public:
        explicit PatternExpr(variant<unique_ptr<Pattern>, unique_ptr<PatternWithoutGuards>, vector<unique_ptr<PatternWithGuards>>> patternExpr)
            : patternExpr(move(patternExpr)) {}
    };
    class CatchPatternExpr : public ExprNode {
    protected:
        unique_ptr<Pattern> matchPattern;
        variant<unique_ptr<PatternWithoutGuards>, vector<unique_ptr<PatternWithGuards>>> pattern;

    public:
        explicit CatchPatternExpr(unique_ptr<Pattern> matchPattern, variant<unique_ptr<PatternWithoutGuards>, vector<unique_ptr<PatternWithGuards>>> pattern)
            : matchPattern(move(matchPattern)), pattern(move(pattern)) {}
    };

    class PatternValue : public PatternNode {
    protected:
        variant<unique_ptr<LiteralExpr<nullptr_t>>, unique_ptr<LiteralExpr<any>>, unique_ptr<SymbolExpr>, unique_ptr<IdentifierExpr>> expr;

    public:
        explicit PatternValue(variant<unique_ptr<LiteralExpr<nullptr_t>>, unique_ptr<LiteralExpr<any>>, unique_ptr<SymbolExpr>, unique_ptr<IdentifierExpr>> expr)
            : expr(move(expr)) {}
    };
    class AsDataStructurePattern : public PatternNode {
    protected:
        unique_ptr<IdentifierExpr> identifier;
        unique_ptr<DataStructurePattern> pattern;

    public:
        AsDataStructurePattern(unique_ptr<IdentifierExpr> identifier, unique_ptr<DataStructurePattern> pattern)
            : identifier(move(identifier)), pattern(move(pattern)) {}
    };
    class TuplePattern : public PatternNode {
    protected:
        vector<unique_ptr<Pattern>> patterns;

    public:
        explicit TuplePattern(vector<unique_ptr<Pattern>> patterns) : patterns(move(patterns)) {}
    };
    class SeqPattern : public PatternNode {
    protected:
        vector<unique_ptr<Pattern>> patterns;

    public:
        explicit SeqPattern(vector<unique_ptr<Pattern>> patterns) : patterns(move(patterns)) {}
    };
    class HeadTailsPattern : public PatternNode {
    protected:
        vector<unique_ptr<PatternWithoutSequence>> heads;
        unique_ptr<TailPattern> tail;

    public:
        explicit HeadTailsPattern(vector<unique_ptr<PatternWithoutSequence>> heads, unique_ptr<TailPattern> tail)
            : heads(move(heads)), tail(move(tail)) {}
    };
    class TailsHeadPattern : public PatternNode {
    protected:
        unique_ptr<TailPattern> tail;
        vector<unique_ptr<PatternWithoutSequence>> heads;

    public:
        explicit TailsHeadPattern(unique_ptr<TailPattern> tail, vector<unique_ptr<PatternWithoutSequence>> heads)
            : tail(move(tail)), heads(move(heads)) {}
    };
    class HeadTailsHeadPattern : public PatternNode {
    protected:
        vector<unique_ptr<PatternWithoutSequence>> left;
        unique_ptr<TailPattern> tail;
        vector<unique_ptr<PatternWithoutSequence>> right;

    public:
        explicit HeadTailsHeadPattern(vector<unique_ptr<PatternWithoutSequence>> left, unique_ptr<TailPattern> tail,
            vector<unique_ptr<PatternWithoutSequence>> right)
            : left(move(left)), tail(move(tail)), right(move(right)) {}
    };
    class DictPattern : public PatternNode {
    protected:
        vector<pair<unique_ptr<PatternValue>, unique_ptr<Pattern>>> keyValuePairs{};

    public:
        explicit DictPattern(vector<pair<unique_ptr<PatternValue>, unique_ptr<Pattern>>> keyValuePairs)
            : keyValuePairs(move(keyValuePairs)) {}
    };
    class RecordPattern : public PatternNode {
    protected:
        string recordType;
        vector<pair<unique_ptr<NameExpr>, unique_ptr<Pattern>>> items{};

    public:
        explicit RecordPattern(string recordType, vector<pair<unique_ptr<NameExpr>, unique_ptr<Pattern>>> items)
            : recordType(move(recordType)), items(items) {}
    };
}