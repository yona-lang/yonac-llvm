#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace yona::ast
{
    class AsDataStructurePattern;
    class RecordPattern;
    class SeqPattern;
    class HeadTailsPattern;
    class TailsHeadPattern;
    class HeadTailsHeadPattern;
    class DictPattern;
    class TuplePattern;
    class PatternValue;

    using namespace std;

    class Token
    {
    };

    class AstNode
    {
    protected:
        Token token;

    public:
        AstNode() = default;
        using Ptr = unique_ptr<AstNode>;
        virtual ~AstNode() = default;
    };

    template <typename T>
    class AstVisitor
    {
    public:
        virtual ~AstVisitor() = default;
        virtual T visit(const AstNode& node) = 0; // Placeholder for the actual implementation
    };

    class ExprNode : public AstNode
    {
    };
    class PatternNode : public AstNode
    {
    };
    class UnderscoreNode : public PatternNode
    {
    };

    class ValueExpr : public ExprNode
    {
    };

    template <typename T>
    class LiteralExpr : public ValueExpr
    {
    protected:
        T value;

    public:
        explicit LiteralExpr(T value);
    };
    class OpExpr : public ExprNode
    {
    };
    class BinaryOpExpr : public OpExpr
    {
    protected:
        ExprNode left;
        ExprNode right;

    public:
        explicit BinaryOpExpr(const ExprNode& left, const ExprNode& right);
    };
    class AliasExpr : public ExprNode
    {
    };
    class CallExpr : public ExprNode
    {
    };
    class ImportClauseExpr : public ExprNode
    {
    };
    class GeneratorExpr : public ExprNode
    {
    };
    class CollectionExtractorExpr : public ExprNode
    {
    };
    class SequenceExpr : public ExprNode
    {
    };

    class FunctionBody : public AstNode
    {
    };

    class NameExpr : public ExprNode
    {
    protected:
        string value;

    public:
        explicit NameExpr(const string& value);
    };

    class IdentifierExpr : public ValueExpr
    {
    protected:
        NameExpr name;

    public:
        explicit IdentifierExpr(const NameExpr& name);
    };

    class RecordNode : public AstNode
    {
    protected:
        NameExpr recordType;
        vector<IdentifierExpr> identifiers;

    public:
        explicit RecordNode(const NameExpr& recordType, const vector<IdentifierExpr>& identifiers);
    };

    class TrueLiteralExpr : public LiteralExpr<bool>
    {
    public:
        explicit TrueLiteralExpr();
    };
    class FalseLiteralExpr : public LiteralExpr<bool>
    {
    public:
        explicit FalseLiteralExpr();
    };
    class FloatExpr : public LiteralExpr<float>
    {
    public:
        explicit FloatExpr(const float value);
    };
    class IntegerExpr : public LiteralExpr<int>
    {
    public:
        explicit IntegerExpr(const int value);
    };
    class ByteExpr : public LiteralExpr<unsigned char>
    {
    public:
        explicit ByteExpr(unsigned const char value);
    };
    class StringExpr : public LiteralExpr<string>
    {
    public:
        explicit StringExpr(const string& value);
    };
    class CharacterExpr : public LiteralExpr<char>
    {
    public:
        explicit CharacterExpr(const char value);
    };

    class UnitExpr : public LiteralExpr<nullptr_t>
    {
    public:
        UnitExpr();
    };
    class TupleExpr : public ValueExpr
    {
    protected:
        vector<ExprNode> values;

    public:
        explicit TupleExpr(const vector<ExprNode>& values);
    };
    class DictExpr : public ValueExpr
    {
    protected:
        vector<pair<ExprNode, ExprNode>> values;

    public:
        explicit DictExpr(const vector<pair<ExprNode, ExprNode>>& values);
    };
    class ValuesSequenceExpr : public SequenceExpr
    {
    protected:
        vector<ExprNode> values;

    public:
        explicit ValuesSequenceExpr(const vector<ExprNode>& values);
    };
    class RangeSequenceExpr : public SequenceExpr
    {
    protected:
        ExprNode start;
        ExprNode end;
        ExprNode step;

    public:
        explicit RangeSequenceExpr(const ExprNode& start, const ExprNode& end, const ExprNode& step);
    };
    class SetExpr : public ValueExpr
    {
    protected:
        vector<ExprNode> values;

    public:
        explicit SetExpr(const vector<ExprNode>& values);
    };
    class SymbolExpr : public ValueExpr
    {
    protected:
        string value;

    public:
        explicit SymbolExpr(const string& value);
    };
    class PackageNameExpr : public ValueExpr
    {
    protected:
        vector<NameExpr> parts;

    public:
        explicit PackageNameExpr(const vector<NameExpr>& parts);
    };
    class FqnExpr : public ValueExpr
    {
    protected:
        PackageNameExpr packageName;
        NameExpr moduleName;

    public:
        explicit FqnExpr(const PackageNameExpr& packageName, const NameExpr& moduleName);
    };
    class FunctionExpr : public ExprNode
    {
    protected:
        string name;
        vector<PatternNode> patterns;
        vector<FunctionBody> bodies;

    public:
        explicit FunctionExpr(const string& name, const vector<PatternNode>& patterns,
                              const vector<FunctionBody>& bodies);
    };

    class ModuleExpr : public ValueExpr
    {
    protected:
        FqnExpr fqn;
        vector<string> exports;
        vector<RecordNode> records;
        vector<FunctionExpr> functions;

    public:
        explicit ModuleExpr(const FqnExpr& fqn, const vector<string>& exports, const vector<RecordNode>& records,
                            const vector<FunctionExpr>& functions);
    };
    class RecordInstanceExpr : public ValueExpr
    {
    protected:
        NameExpr recordType;
        vector<pair<NameExpr, ExprNode>> items;

    public:
        explicit RecordInstanceExpr(const NameExpr& recordType, const vector<pair<NameExpr, ExprNode>>& items);
    };

    class BodyWithGuards : public FunctionBody
    {
    protected:
        ExprNode guard;
        vector<ExprNode> exprs;

    public:
        explicit BodyWithGuards(const ExprNode& guard, const vector<ExprNode>& exprs);
    };
    class BodyWithoutGuards : public FunctionBody
    {
    protected:
        ExprNode expr;

    public:
        explicit BodyWithoutGuards(const ExprNode& expr);
    };
    class LogicalNotOpExpr : public OpExpr
    {
    protected:
        ExprNode expr;

    public:
        explicit LogicalNotOpExpr(const ExprNode& expr);
    };
    class BinaryNotOpExpr : public OpExpr
    {
    protected:
        ExprNode expr;

    public:
        explicit BinaryNotOpExpr(const ExprNode& expr);
    };

    class PowerExpr : public BinaryOpExpr
    {
    public:
        explicit PowerExpr(const ExprNode& left, const ExprNode& right);
    };
    class MultiplyExpr : public BinaryOpExpr
    {
    public:
        explicit MultiplyExpr(const ExprNode& left, const ExprNode& right);
    };
    class DivideExpr : public BinaryOpExpr
    {
    public:
        explicit DivideExpr(const ExprNode& left, const ExprNode& right);
    };
    class ModuloExpr : public BinaryOpExpr
    {
    public:
        explicit ModuloExpr(const ExprNode& left, const ExprNode& right);
    };
    class AddExpr : public BinaryOpExpr
    {
    public:
        explicit AddExpr(const ExprNode& left, const ExprNode& right);
    };
    class SubtractExpr : public BinaryOpExpr
    {
    public:
        explicit SubtractExpr(const ExprNode& left, const ExprNode& right);
    };
    class LeftShiftExpr : public BinaryOpExpr
    {
    public:
        explicit LeftShiftExpr(const ExprNode& left, const ExprNode& right);
    };
    class RightShiftExpr : public BinaryOpExpr
    {
    public:
        explicit RightShiftExpr(const ExprNode& left, const ExprNode& right);
    };
    class ZerofillRightShiftExpr : public BinaryOpExpr
    {
    public:
        explicit ZerofillRightShiftExpr(const ExprNode& left, const ExprNode& right);
    };
    class GteExpr : public BinaryOpExpr
    {
    public:
        explicit GteExpr(const ExprNode& left, const ExprNode& right);
    };
    class LteExpr : public BinaryOpExpr
    {
    public:
        explicit LteExpr(const ExprNode& left, const ExprNode& right);
    };
    class GtExpr : public BinaryOpExpr
    {
    public:
        explicit GtExpr(const ExprNode& left, const ExprNode& right);
    };
    class LtExpr : public BinaryOpExpr
    {
    public:
        explicit LtExpr(const ExprNode& left, const ExprNode& right);
    };
    class EqExpr : public BinaryOpExpr
    {
    public:
        explicit EqExpr(const ExprNode& left, const ExprNode& right);
    };
    class NeqExpr : public BinaryOpExpr
    {
    public:
        explicit NeqExpr(const ExprNode& left, const ExprNode& right);
    };
    class ConsLeftExpr : public BinaryOpExpr
    {
    public:
        explicit ConsLeftExpr(const ExprNode& left, const ExprNode& right);
    };
    class ConsRightExpr : public BinaryOpExpr
    {
    public:
        explicit ConsRightExpr(const ExprNode& left, const ExprNode& right);
    };
    class JoinExpr : public BinaryOpExpr
    {
    public:
        explicit JoinExpr(const ExprNode& left, const ExprNode& right);
    };
    class BitwiseAndExpr : public BinaryOpExpr
    {
    public:
        explicit BitwiseAndExpr(const ExprNode& left, const ExprNode& right);
    };
    class BitwiseXorExpr : public BinaryOpExpr
    {
    public:
        explicit BitwiseXorExpr(const ExprNode& left, const ExprNode& right);
    };
    class BitwiseOrExpr : public BinaryOpExpr
    {
    public:
        explicit BitwiseOrExpr(const ExprNode& left, const ExprNode& right);
    };
    class LogicalAndExpr : public BinaryOpExpr
    {
    public:
        explicit LogicalAndExpr(const ExprNode& left, const ExprNode& right);
    };
    class LogicalOrExpr : public BinaryOpExpr
    {
    public:
        explicit LogicalOrExpr(const ExprNode& left, const ExprNode& right);
    };
    class InExpr : public BinaryOpExpr
    {
    public:
        explicit InExpr(const ExprNode& left, const ExprNode& right);
    };
    class PipeLeftExpr : public BinaryOpExpr
    {
    public:
        explicit PipeLeftExpr(const ExprNode& left, const ExprNode& right);
    };
    class PipeRightExpr : public BinaryOpExpr
    {
    public:
        explicit PipeRightExpr(const ExprNode& left, const ExprNode& right);
    };

    class LetExpr : public ExprNode
    {
    protected:
        vector<AliasExpr> aliases;
        ExprNode expr;

    public:
        explicit LetExpr(const vector<AliasExpr>& aliases, const ExprNode& expr);
    };
    class IfExpr : public ExprNode
    {
    protected:
        ExprNode condition;
        ExprNode thenExpr;
        optional<ExprNode> elseExpr;

    public:
        explicit IfExpr(const ExprNode& condition, const ExprNode& thenExpr, const optional<ExprNode>& elseExpr);
    };
    class ApplyExpr : public ExprNode
    {
    protected:
        CallExpr call;
        vector<variant<ExprNode, ValueExpr>> args;

    public:
        explicit ApplyExpr(const CallExpr& call, const vector<variant<ExprNode, ValueExpr>>& args);
    };
    class DoExpr : public ExprNode
    {
    protected:
        vector<variant<AliasExpr, ExprNode>> steps;

    public:
        explicit DoExpr(const vector<variant<AliasExpr, ExprNode>>& steps);
    };
    class ImportExpr : public ExprNode
    {
    protected:
        vector<ImportClauseExpr> clauses;
        ExprNode expr;

    public:
        explicit ImportExpr(const vector<ImportClauseExpr>& clauses, const ExprNode& expr);
    };
    class RaiseExpr : public ExprNode
    {
    protected:
        SymbolExpr symbol;
        LiteralExpr<string> message;

    public:
        explicit RaiseExpr(const SymbolExpr& symbol, const LiteralExpr<string>& message);
    };
    class WithExpr : public ExprNode
    {
    protected:
        ExprNode contextExpr;
        optional<NameExpr> name;
        ExprNode bodyExpr;

    public:
        explicit WithExpr(const ExprNode& contextExpr, const optional<NameExpr>& name, const ExprNode& bodyExpr);
    };
    class FieldAccessExpr : public ExprNode
    {
    protected:
        IdentifierExpr identifier;
        NameExpr name;

    public:
        explicit FieldAccessExpr(const IdentifierExpr& identifier, const NameExpr& name);
    };
    class FieldUpdateExpr : public ExprNode
    {
    protected:
        IdentifierExpr identifier;
        vector<pair<NameExpr, ExprNode>> updates;

    public:
        explicit FieldUpdateExpr(const IdentifierExpr& identifier, const vector<pair<NameExpr, ExprNode>>& updates);
    };

    class LambdaAlias : public AliasExpr
    {
    protected:
        NameExpr name;
        FunctionExpr lambda;

    public:
        explicit LambdaAlias(const NameExpr& name, const FunctionExpr& lambda);
    };
    class ModuleAlias : public AliasExpr
    {
    protected:
        NameExpr name;
        ModuleExpr module;

    public:
        explicit ModuleAlias(const NameExpr& name, const ModuleExpr& module);
    };
    class ValueAlias : public AliasExpr
    {
    protected:
        IdentifierExpr identifier;
        ExprNode expr;

    public:
        explicit ValueAlias(const IdentifierExpr& identifier, const ExprNode& expr);
    };
    class PatternAlias : public AliasExpr
    {
    protected:
        PatternNode pattern;
        ExprNode expr;

    public:
        explicit PatternAlias(const PatternNode& pattern, const ExprNode& expr);
    };
    class FqnAlias : public AliasExpr
    {
    protected:
        NameExpr name;
        FqnExpr fqn;

    public:
        explicit FqnAlias(const NameExpr& name, const FqnExpr& fqn);
    };
    class FunctionAlias : public AliasExpr
    {
    protected:
        NameExpr name;
        NameExpr alias;

    public:
        explicit FunctionAlias(const NameExpr& name, const NameExpr& alias);
    };

    class AliasCall : public CallExpr
    {
    protected:
        NameExpr alias;
        NameExpr funName;

    public:
        explicit AliasCall(const NameExpr& alias, const NameExpr& funName);
    };
    class NameCall : public CallExpr
    {
    protected:
        NameExpr name;

    public:
        explicit NameCall(const NameExpr& name);
    };
    class ModuleCall : public CallExpr
    {
    protected:
        variant<FqnExpr, ExprNode> fqn;
        NameExpr funName;

    public:
        explicit ModuleCall(const variant<FqnExpr, ExprNode>& fqn, const NameExpr& funName);
    };

    class ModuleImport : public ImportClauseExpr
    {
    protected:
        FqnExpr fqn;
        NameExpr name;

    public:
        explicit ModuleImport(const FqnExpr& fqn, const NameExpr& name);
    };
    class FunctionsImport : public ImportClauseExpr
    {
    protected:
        vector<FunctionAlias> aliases;
        FqnExpr fromFqn;

    public:
        explicit FunctionsImport(const vector<FunctionAlias>& aliases, const FqnExpr& fromFqn);
    };

    class SeqGeneratorExpr : public GeneratorExpr
    {
    protected:
        ExprNode reducerExpr;
        CollectionExtractorExpr collectionExtractor;
        ExprNode stepExpression;

    public:
        explicit SeqGeneratorExpr(const ExprNode& reducerExpr, const CollectionExtractorExpr& collectionExtractor,
                                  ExprNode stepExpression);
    };
    class SetGeneratorExpr : public GeneratorExpr
    {
    protected:
        ExprNode reducerExpr;
        CollectionExtractorExpr collectionExtractor;
        ExprNode stepExpression;

    public:
        explicit SetGeneratorExpr(const ExprNode& reducerExpr, const CollectionExtractorExpr& collectionExtractor,
                                  const ExprNode& stepExpression);
    };
    class DictGeneratorReducer : public ExprNode
    {
    protected:
        ExprNode key;
        ExprNode value;

    public:
        explicit DictGeneratorReducer(const ExprNode& key, const ExprNode& value);
    };
    class DictGeneratorExpr : public GeneratorExpr
    {
    protected:
        DictGeneratorReducer reducerExpr;
        CollectionExtractorExpr collectionExtractor;
        ExprNode stepExpression;

    public:
        explicit DictGeneratorExpr(const DictGeneratorReducer& reducerExpr,
                                   const CollectionExtractorExpr& collectionExtractor, const ExprNode& stepExpression);
    };
    class UnderscorePattern : public PatternNode
    {
    };

    using IdentifierOrUnderscore = variant<IdentifierExpr, UnderscoreNode>;

    class ValueCollectionExtractorExpr : public CollectionExtractorExpr
    {
    protected:
        IdentifierOrUnderscore expr;

    public:
        explicit ValueCollectionExtractorExpr(const IdentifierOrUnderscore& expr);
    };
    class KeyValueCollectionExtractorExpr : public CollectionExtractorExpr
    {
    protected:
        IdentifierOrUnderscore keyExpr;
        IdentifierOrUnderscore valueExpr;

    public:
        explicit KeyValueCollectionExtractorExpr(const IdentifierOrUnderscore& keyExpr,
                                                 const IdentifierOrUnderscore& valueExpr);
    };

    using PatternWithoutSequence = PatternNode; // variant<UnderscorePattern, PatternValue, TuplePattern, DictPattern>;
    using SequencePattern =
        PatternNode; // variant<SeqPattern, HeadTailsPattern, TailsHeadPattern, HeadTailsHeadPattern>;
    using DataStructurePattern = PatternNode; // variant<TuplePattern, SequencePattern, DictPattern, RecordPattern>;
    using Pattern =
        PatternNode; // variant<UnderscorePattern, PatternValue, DataStructurePattern, AsDataStructurePattern>;
    using TailPattern = PatternNode; // variant<IdentifierExpr, SequenceExpr, UnderscoreNode, StringExpr>;

    class PatternWithGuards : public PatternNode
    {
    protected:
        ExprNode guard;
        ExprNode exprNode;

    public:
        explicit PatternWithGuards(const ExprNode& guard, const ExprNode& exprNode);
    };
    class PatternWithoutGuards : public PatternNode
    {
    protected:
        ExprNode exprNode;

    public:
        PatternWithoutGuards() = default;
        explicit PatternWithoutGuards(const ExprNode& exprNode);
    };
    class PatternExpr : public ExprNode
    {
    protected:
        variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>> patternExpr;

    public:
        explicit PatternExpr(const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>>& patternExpr);
    };
    class CatchPatternExpr : public ExprNode
    {
    protected:
        Pattern matchPattern;
        variant<PatternWithoutGuards, vector<PatternWithGuards>> pattern;

    public:
        explicit CatchPatternExpr(const Pattern& matchPattern,
                                  const variant<PatternWithoutGuards, vector<PatternWithGuards>>& pattern);
    };
    class CatchExpr : public ExprNode
    {
    protected:
        vector<CatchPatternExpr> patterns;

    public:
        explicit CatchExpr(const vector<CatchPatternExpr>& patterns);
    };
    class TryCatchExpr : public ExprNode
    {
    protected:
        ExprNode tryExpr;
        CatchExpr catchExpr;

    public:
        explicit TryCatchExpr(const ExprNode& tryExpr, const CatchExpr& catchExpr);
    };

    class PatternValue : public PatternNode
    {
    protected:
        variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr> expr;

    public:
        explicit PatternValue(
            const variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr>& expr);
    };
    class AsDataStructurePattern : public PatternNode
    {
    protected:
        IdentifierExpr identifier;
        DataStructurePattern pattern;

    public:
        AsDataStructurePattern(const IdentifierExpr& identifier, const DataStructurePattern& pattern);
    };
    class TuplePattern : public PatternNode
    {
    protected:
        vector<Pattern> patterns;

    public:
        explicit TuplePattern(const vector<Pattern>& patterns);
    };

    class SeqPattern : public PatternNode
    {
    protected:
        vector<Pattern> patterns;

    public:
        explicit SeqPattern(const vector<Pattern>& patterns);
    };

    class HeadTailsPattern : public PatternNode
    {
    protected:
        vector<PatternWithoutSequence> heads;
        TailPattern tail;

    public:
        explicit HeadTailsPattern(const vector<PatternWithoutSequence>& heads, const TailPattern& tail);
    };

    class TailsHeadPattern : public PatternNode
    {
    protected:
        TailPattern tail;
        vector<PatternWithoutSequence> heads;

    public:
        explicit TailsHeadPattern(const TailPattern& tail, const vector<PatternWithoutSequence>& heads);
    };

    class HeadTailsHeadPattern : public PatternNode
    {
    protected:
        vector<PatternWithoutSequence> left;
        TailPattern tail;
        vector<PatternWithoutSequence> right;

    public:
        explicit HeadTailsHeadPattern(const vector<PatternWithoutSequence>& left, const TailPattern& tail,
                                      const vector<PatternWithoutSequence>& right);
    };

    class DictPattern : public PatternNode
    {
    protected:
        vector<pair<PatternValue, Pattern>> keyValuePairs;

    public:
        explicit DictPattern(const vector<pair<PatternValue, Pattern>>& keyValuePairs);
    };

    class RecordPattern : public PatternNode
    {
    protected:
        string recordType;
        vector<pair<NameExpr, Pattern>> items;

    public:
        explicit RecordPattern(const string& recordType, const vector<pair<NameExpr, Pattern>>& items);
    };

    class CaseExpr : public ExprNode
    {
    protected:
        ExprNode expr;
        vector<PatternExpr> patterns;

    public:
        explicit CaseExpr(const ExprNode& expr, const vector<PatternExpr>& patterns);
    };
}
