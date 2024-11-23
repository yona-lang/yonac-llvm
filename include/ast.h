#pragma once

#include <antlr4-runtime.h>
#include <concepts>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <Interpreter.h>
#include <SymbolTable.h>

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
    using Token = const antlr4::ParserRuleContext&;

    class AstNode
    {
    public:
        Token token;
        AstNode* parent;
        explicit AstNode(Token token);
        virtual ~AstNode() = default;

        template <typename T>
            requires derived_from<T, AstNode>
        T* with_parent(AstNode* parent)
        {
            this->parent = parent;
            return dynamic_cast<T*>(this);
        }
    };

    template <typename T>
        requires derived_from<T, AstNode>
    vector<T> nodes_with_parent(vector<T> nodes, AstNode* parent)
    {
        for (auto node : nodes)
        {
            node.parent = parent;
        }
        return nodes;
    }

    template <typename T1, typename T2>
        requires derived_from<T1, AstNode> && derived_from<T2, AstNode>
    vector<pair<T1, T2>> nodes_with_parent(vector<pair<T1, T2>> nodes, AstNode* parent)
    {
        for (auto node : nodes)
        {
            node.first.parent = parent;
            node.second.parent = parent;
        }
        return nodes;
    }

    template <typename... Types>
        requires(derived_from<Types, AstNode> && ...)
    variant<Types...> node_with_parent(variant<Types...> node, AstNode* parent)
    {
        visit([parent](auto arg) { arg.parent = parent; }, node);
        return node;
    }

    template <typename T>
        requires derived_from<T, AstNode>
    optional<T> node_with_parent(optional<T> node, AstNode* parent)
    {
        if (node.has_value())
        {
            node.value().parent = parent;
        }
        return node;
    }

    template <typename... Types>
        requires(derived_from<Types, AstNode> && ...)
    vector<variant<Types...>> nodes_with_parent(vector<variant<Types...>> nodes, AstNode* parent)
    {
        for (auto node : nodes)
        {
            node_with_parent(node, parent);
        }
        return nodes;
    }

    template <typename T>
    class AstVisitor
    {
    public:
        virtual ~AstVisitor() = default;
        virtual T visit(const AstNode node) = 0; // Placeholder for the actual implementation
    };

    class ExprNode : public AstNode
    {
    public:
        explicit ExprNode(Token token) : AstNode(token) {}
    };

    class PatternNode : public AstNode
    {
    public:
        explicit PatternNode(Token token) : AstNode(token) {}
    };

    class UnderscoreNode final : public PatternNode
    {
    public:
        explicit UnderscoreNode(Token token) : PatternNode(token) {}
    };

    class ValueExpr : public ExprNode
    {
    public:
        explicit ValueExpr(Token token) : ExprNode(token) {}
    };

    class ScopedNode final : public AstNode
    {
    protected:
        interp::SymbolTable symbol_table;

    public:
        explicit ScopedNode(Token token) : AstNode(token) {}
        ScopedNode* getParentScopedNode() const;
    };

    template <typename T>
    class LiteralExpr : public ValueExpr
    {
    protected:
        T value;

    public:
        explicit LiteralExpr(Token token, T value);
    };
    class OpExpr : public ExprNode
    {
    public:
        explicit OpExpr(Token token) : ExprNode(token) {}
    };

    class BinaryOpExpr : public OpExpr
    {
    protected:
        ExprNode left;
        ExprNode right;

    public:
        explicit BinaryOpExpr(Token token, ExprNode left, ExprNode right);
    };

    class AliasExpr : public ExprNode
    {
    public:
        explicit AliasExpr(Token token) : ExprNode(token) {}
    };

    class CallExpr : public ExprNode
    {
    public:
        explicit CallExpr(Token token) : ExprNode(token) {}
    };

    class ImportClauseExpr : public ExprNode
    {
    public:
        explicit ImportClauseExpr(Token token) : ExprNode(token) {}
    };

    class GeneratorExpr : public ExprNode
    {
    public:
        explicit GeneratorExpr(Token token) : ExprNode(token) {}
    };

    class CollectionExtractorExpr : public ExprNode
    {
    public:
        explicit CollectionExtractorExpr(Token token) : ExprNode(token) {}
    };

    class SequenceExpr : public ExprNode
    {
    public:
        explicit SequenceExpr(Token token) : ExprNode(token) {}
    };

    class FunctionBody : public AstNode
    {
    public:
        explicit FunctionBody(Token token) : AstNode(token) {}
    };

    class NameExpr : public ExprNode
    {
    protected:
        string value;

    public:
        explicit NameExpr(Token token, string value);
    };

    class IdentifierExpr : public ValueExpr
    {
    protected:
        NameExpr name;

    public:
        explicit IdentifierExpr(Token token, NameExpr name);
    };

    class RecordNode : public AstNode
    {
    protected:
        NameExpr recordType;
        vector<IdentifierExpr> identifiers;

    public:
        explicit RecordNode(Token token, NameExpr recordType, const vector<IdentifierExpr>& identifiers);
    };

    class TrueLiteralExpr : public LiteralExpr<bool>
    {
    public:
        explicit TrueLiteralExpr(Token token);
    };

    class FalseLiteralExpr : public LiteralExpr<bool>
    {
    public:
        explicit FalseLiteralExpr(Token token);
    };

    class FloatExpr : public LiteralExpr<float>
    {
    public:
        explicit FloatExpr(Token token, float value);
    };

    class IntegerExpr : public LiteralExpr<int>
    {
    public:
        explicit IntegerExpr(Token token, int value);
    };

    class ByteExpr : public LiteralExpr<unsigned char>
    {
    public:
        explicit ByteExpr(Token token, unsigned char value);
    };

    class StringExpr : public LiteralExpr<string>
    {
    public:
        explicit StringExpr(Token token, string value);
    };

    class CharacterExpr : public LiteralExpr<char>
    {
    public:
        explicit CharacterExpr(Token token, char value);
    };

    class UnitExpr : public LiteralExpr<nullptr_t>
    {
    public:
        explicit UnitExpr(Token token);
    };

    class TupleExpr : public ValueExpr
    {
    protected:
        vector<ExprNode> values;

    public:
        explicit TupleExpr(Token token, const vector<ExprNode>& values);
    };

    class DictExpr : public ValueExpr
    {
    protected:
        vector<pair<ExprNode, ExprNode>> values;

    public:
        explicit DictExpr(Token token, const vector<pair<ExprNode, ExprNode>>& values);
    };

    class ValuesSequenceExpr : public SequenceExpr
    {
    protected:
        vector<ExprNode> values;

    public:
        explicit ValuesSequenceExpr(Token token, const vector<ExprNode>& values);
    };

    class RangeSequenceExpr : public SequenceExpr
    {
    protected:
        ExprNode start;
        ExprNode end;
        ExprNode step;

    public:
        explicit RangeSequenceExpr(Token token, ExprNode start, ExprNode end, ExprNode step);
    };

    class SetExpr : public ValueExpr
    {
    protected:
        vector<ExprNode> values;

    public:
        explicit SetExpr(Token token, const vector<ExprNode>& values);
    };

    class SymbolExpr : public ValueExpr
    {
    protected:
        string value;

    public:
        explicit SymbolExpr(Token token, string value);
    };

    class PackageNameExpr : public ValueExpr
    {
    protected:
        vector<NameExpr> parts;

    public:
        explicit PackageNameExpr(Token token, const vector<NameExpr>& parts);
    };

    class FqnExpr : public ValueExpr
    {
    protected:
        PackageNameExpr packageName;
        NameExpr moduleName;

    public:
        explicit FqnExpr(Token token, PackageNameExpr packageName, NameExpr moduleName);
    };

    class FunctionExpr : public ExprNode
    {
    protected:
        string name;
        vector<PatternNode> patterns;
        vector<FunctionBody> bodies;

    public:
        explicit FunctionExpr(Token token, string name, const vector<PatternNode>& patterns,
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
        explicit ModuleExpr(Token token, FqnExpr fqn, const vector<string>& exports, const vector<RecordNode>& records,
                            const vector<FunctionExpr>& functions);
    };

    class RecordInstanceExpr : public ValueExpr
    {
    protected:
        NameExpr recordType;
        vector<pair<NameExpr, ExprNode>> items;

    public:
        explicit RecordInstanceExpr(Token token, NameExpr recordType, const vector<pair<NameExpr, ExprNode>>& items);
    };

    class BodyWithGuards : public FunctionBody
    {
    protected:
        ExprNode guard;
        vector<ExprNode> exprs;

    public:
        explicit BodyWithGuards(Token token, ExprNode guard, const vector<ExprNode>& exprs);
    };

    class BodyWithoutGuards : public FunctionBody
    {
    protected:
        ExprNode expr;

    public:
        explicit BodyWithoutGuards(Token token, ExprNode expr);
    };

    class LogicalNotOpExpr : public OpExpr
    {
    protected:
        ExprNode expr;

    public:
        explicit LogicalNotOpExpr(Token token, ExprNode expr);
    };

    class BinaryNotOpExpr : public OpExpr
    {
    protected:
        ExprNode expr;

    public:
        explicit BinaryNotOpExpr(Token token, ExprNode expr);
    };

    class PowerExpr : public BinaryOpExpr
    {
    public:
        explicit PowerExpr(Token token, ExprNode left, ExprNode right);
    };

    class MultiplyExpr : public BinaryOpExpr
    {
    public:
        explicit MultiplyExpr(Token token, ExprNode left, ExprNode right);
    };

    class DivideExpr : public BinaryOpExpr
    {
    public:
        explicit DivideExpr(Token token, ExprNode left, ExprNode right);
    };

    class ModuloExpr : public BinaryOpExpr
    {
    public:
        explicit ModuloExpr(Token token, ExprNode left, ExprNode right);
    };

    class AddExpr : public BinaryOpExpr
    {
    public:
        explicit AddExpr(Token token, ExprNode left, ExprNode right);
    };

    class SubtractExpr : public BinaryOpExpr
    {
    public:
        explicit SubtractExpr(Token token, ExprNode left, ExprNode right);
    };

    class LeftShiftExpr : public BinaryOpExpr
    {
    public:
        explicit LeftShiftExpr(Token token, ExprNode left, ExprNode right);
    };

    class RightShiftExpr : public BinaryOpExpr
    {
    public:
        explicit RightShiftExpr(Token token, ExprNode left, ExprNode right);
    };

    class ZerofillRightShiftExpr : public BinaryOpExpr
    {
    public:
        explicit ZerofillRightShiftExpr(Token token, ExprNode left, ExprNode right);
    };

    class GteExpr : public BinaryOpExpr
    {
    public:
        explicit GteExpr(Token token, ExprNode left, ExprNode right);
    };

    class LteExpr : public BinaryOpExpr
    {
    public:
        explicit LteExpr(Token token, ExprNode left, ExprNode right);
    };

    class GtExpr : public BinaryOpExpr
    {
    public:
        explicit GtExpr(Token token, ExprNode left, ExprNode right);
    };

    class LtExpr : public BinaryOpExpr
    {
    public:
        explicit LtExpr(Token token, ExprNode left, ExprNode right);
    };

    class EqExpr : public BinaryOpExpr
    {
    public:
        explicit EqExpr(Token token, ExprNode left, ExprNode right);
    };

    class NeqExpr : public BinaryOpExpr
    {
    public:
        explicit NeqExpr(Token token, ExprNode left, ExprNode right);
    };

    class ConsLeftExpr : public BinaryOpExpr
    {
    public:
        explicit ConsLeftExpr(Token token, ExprNode left, ExprNode right);
    };

    class ConsRightExpr : public BinaryOpExpr
    {
    public:
        explicit ConsRightExpr(Token token, ExprNode left, ExprNode right);
    };

    class JoinExpr : public BinaryOpExpr
    {
    public:
        explicit JoinExpr(Token token, ExprNode left, ExprNode right);
    };

    class BitwiseAndExpr : public BinaryOpExpr
    {
    public:
        explicit BitwiseAndExpr(Token token, ExprNode left, ExprNode right);
    };

    class BitwiseXorExpr : public BinaryOpExpr
    {
    public:
        explicit BitwiseXorExpr(Token token, ExprNode left, ExprNode right);
    };

    class BitwiseOrExpr : public BinaryOpExpr
    {
    public:
        explicit BitwiseOrExpr(Token token, ExprNode left, ExprNode right);
    };

    class LogicalAndExpr : public BinaryOpExpr
    {
    public:
        explicit LogicalAndExpr(Token token, ExprNode left, ExprNode right);
    };

    class LogicalOrExpr : public BinaryOpExpr
    {
    public:
        explicit LogicalOrExpr(Token token, ExprNode left, ExprNode right);
    };

    class InExpr : public BinaryOpExpr
    {
    public:
        explicit InExpr(Token token, ExprNode left, ExprNode right);
    };

    class PipeLeftExpr : public BinaryOpExpr
    {
    public:
        explicit PipeLeftExpr(Token token, ExprNode left, ExprNode right);
    };

    class PipeRightExpr : public BinaryOpExpr
    {
    public:
        explicit PipeRightExpr(Token token, ExprNode left, ExprNode right);
    };

    class LetExpr : public ExprNode
    {
    protected:
        vector<AliasExpr> aliases;
        ExprNode expr;

    public:
        explicit LetExpr(Token token, const vector<AliasExpr>& aliases, ExprNode expr);
    };

    class IfExpr : public ExprNode
    {
    protected:
        ExprNode condition;
        ExprNode thenExpr;
        optional<ExprNode> elseExpr;

    public:
        explicit IfExpr(Token token, ExprNode condition, ExprNode thenExpr, const optional<ExprNode>& elseExpr);
    };

    class ApplyExpr : public ExprNode
    {
    protected:
        CallExpr call;
        vector<variant<ExprNode, ValueExpr>> args;

    public:
        explicit ApplyExpr(Token token, CallExpr call, const vector<variant<ExprNode, ValueExpr>>& args);
    };

    class DoExpr : public ExprNode
    {
    protected:
        vector<variant<AliasExpr, ExprNode>> steps;

    public:
        explicit DoExpr(Token token, const vector<variant<AliasExpr, ExprNode>>& steps);
    };

    class ImportExpr : public ExprNode
    {
    protected:
        vector<ImportClauseExpr> clauses;
        ExprNode expr;

    public:
        explicit ImportExpr(Token token, const vector<ImportClauseExpr>& clauses, ExprNode expr);
    };

    class RaiseExpr : public ExprNode
    {
    protected:
        SymbolExpr symbol;
        LiteralExpr<string> message;

    public:
        explicit RaiseExpr(Token token, SymbolExpr symbol, LiteralExpr<string> message);
    };

    class WithExpr : public ExprNode
    {
    protected:
        ExprNode contextExpr;
        optional<NameExpr> name;
        ExprNode bodyExpr;

    public:
        explicit WithExpr(Token token, ExprNode contextExpr, const optional<NameExpr>& name, ExprNode bodyExpr);
    };

    class FieldAccessExpr : public ExprNode
    {
    protected:
        IdentifierExpr identifier;
        NameExpr name;

    public:
        explicit FieldAccessExpr(Token token, IdentifierExpr identifier, NameExpr name);
    };

    class FieldUpdateExpr : public ExprNode
    {
    protected:
        IdentifierExpr identifier;
        vector<pair<NameExpr, ExprNode>> updates;

    public:
        explicit FieldUpdateExpr(Token token, IdentifierExpr identifier,
                                 const vector<pair<NameExpr, ExprNode>>& updates);
    };

    class LambdaAlias : public AliasExpr
    {
    protected:
        NameExpr name;
        FunctionExpr lambda;

    public:
        explicit LambdaAlias(Token token, NameExpr name, FunctionExpr lambda);
    };

    class ModuleAlias : public AliasExpr
    {
    protected:
        NameExpr name;
        ModuleExpr module;

    public:
        explicit ModuleAlias(Token token, NameExpr name, ModuleExpr module);
    };

    class ValueAlias : public AliasExpr
    {
    protected:
        IdentifierExpr identifier;
        ExprNode expr;

    public:
        explicit ValueAlias(Token token, IdentifierExpr identifier, ExprNode expr);
    };

    class PatternAlias : public AliasExpr
    {
    protected:
        PatternNode pattern;
        ExprNode expr;

    public:
        explicit PatternAlias(Token token, PatternNode pattern, ExprNode expr);
    };

    class FqnAlias : public AliasExpr
    {
    protected:
        NameExpr name;
        FqnExpr fqn;

    public:
        explicit FqnAlias(Token token, NameExpr name, FqnExpr fqn);
    };

    class FunctionAlias : public AliasExpr
    {
    protected:
        NameExpr name;
        NameExpr alias;

    public:
        explicit FunctionAlias(Token token, NameExpr name, NameExpr alias);
    };

    class AliasCall : public CallExpr
    {
    protected:
        NameExpr alias;
        NameExpr funName;

    public:
        explicit AliasCall(Token token, NameExpr alias, NameExpr funName);
    };

    class NameCall : public CallExpr
    {
    protected:
        NameExpr name;

    public:
        explicit NameCall(Token token, NameExpr name);
    };

    class ModuleCall : public CallExpr
    {
    protected:
        variant<FqnExpr, ExprNode> fqn;
        NameExpr funName;

    public:
        explicit ModuleCall(Token token, const variant<FqnExpr, ExprNode>& fqn, NameExpr funName);
    };

    class ModuleImport : public ImportClauseExpr
    {
    protected:
        FqnExpr fqn;
        NameExpr name;

    public:
        explicit ModuleImport(Token token, FqnExpr fqn, NameExpr name);
    };

    class FunctionsImport : public ImportClauseExpr
    {
    protected:
        vector<FunctionAlias> aliases;
        FqnExpr fromFqn;

    public:
        explicit FunctionsImport(Token token, const vector<FunctionAlias>& aliases, FqnExpr fromFqn);
    };

    class SeqGeneratorExpr : public GeneratorExpr
    {
    protected:
        ExprNode reducerExpr;
        CollectionExtractorExpr collectionExtractor;
        ExprNode stepExpression;

    public:
        explicit SeqGeneratorExpr(Token token, ExprNode reducerExpr, CollectionExtractorExpr collectionExtractor,
                                  ExprNode stepExpression);
    };

    class SetGeneratorExpr : public GeneratorExpr
    {
    protected:
        ExprNode reducerExpr;
        CollectionExtractorExpr collectionExtractor;
        ExprNode stepExpression;

    public:
        explicit SetGeneratorExpr(Token token, ExprNode reducerExpr, CollectionExtractorExpr collectionExtractor,
                                  ExprNode stepExpression);
    };

    class DictGeneratorReducer : public ExprNode
    {
    protected:
        ExprNode key;
        ExprNode value;

    public:
        explicit DictGeneratorReducer(Token token, ExprNode key, ExprNode value);
    };

    class DictGeneratorExpr : public GeneratorExpr
    {
    protected:
        DictGeneratorReducer reducerExpr;
        CollectionExtractorExpr collectionExtractor;
        ExprNode stepExpression;

    public:
        explicit DictGeneratorExpr(Token token, DictGeneratorReducer reducerExpr,
                                   CollectionExtractorExpr collectionExtractor, ExprNode stepExpression);
    };
    class UnderscorePattern final : public PatternNode
    {
    public:
        UnderscorePattern(Token token) : PatternNode(token) {}
    };

    using IdentifierOrUnderscore = variant<IdentifierExpr, UnderscoreNode>;

    class ValueCollectionExtractorExpr : public CollectionExtractorExpr
    {
    protected:
        IdentifierOrUnderscore expr;

    public:
        explicit ValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore expr);
    };

    class KeyValueCollectionExtractorExpr : public CollectionExtractorExpr
    {
    protected:
        IdentifierOrUnderscore keyExpr;
        IdentifierOrUnderscore valueExpr;

    public:
        explicit KeyValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore keyExpr,
                                                 IdentifierOrUnderscore valueExpr);
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
        explicit PatternWithGuards(Token token, ExprNode guard, ExprNode exprNode);
    };

    class PatternWithoutGuards : public PatternNode
    {
    protected:
        ExprNode exprNode;

    public:
        explicit PatternWithoutGuards(Token token, ExprNode exprNode);
    };

    class PatternExpr : public ExprNode
    {
    protected:
        variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>> patternExpr;

    public:
        explicit PatternExpr(Token token,
                             const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>>& patternExpr);
    };

    class CatchPatternExpr : public ExprNode
    {
    protected:
        Pattern matchPattern;
        variant<PatternWithoutGuards, vector<PatternWithGuards>> pattern;

    public:
        explicit CatchPatternExpr(Token token, Pattern matchPattern,
                                  const variant<PatternWithoutGuards, vector<PatternWithGuards>>& pattern);
    };

    class CatchExpr : public ExprNode
    {
    protected:
        vector<CatchPatternExpr> patterns;

    public:
        explicit CatchExpr(Token token, const vector<CatchPatternExpr>& patterns);
    };

    class TryCatchExpr : public ExprNode
    {
    protected:
        ExprNode tryExpr;
        CatchExpr catchExpr;

    public:
        explicit TryCatchExpr(Token token, ExprNode tryExpr, CatchExpr catchExpr);
    };

    class PatternValue : public PatternNode
    {
    protected:
        variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr> expr;

    public:
        explicit PatternValue(Token token,
                              const variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr>& expr);
    };

    class AsDataStructurePattern : public PatternNode
    {
    protected:
        IdentifierExpr identifier;
        DataStructurePattern pattern;

    public:
        explicit AsDataStructurePattern(Token token, IdentifierExpr identifier, DataStructurePattern pattern);
    };

    class TuplePattern : public PatternNode
    {
    protected:
        vector<Pattern> patterns;

    public:
        explicit TuplePattern(Token token, const vector<Pattern>& patterns);
    };

    class SeqPattern : public PatternNode
    {
    protected:
        vector<Pattern> patterns;

    public:
        explicit SeqPattern(Token token, const vector<Pattern>& patterns);
    };

    class HeadTailsPattern : public PatternNode
    {
    protected:
        vector<PatternWithoutSequence> heads;
        TailPattern tail;

    public:
        explicit HeadTailsPattern(Token token, const vector<PatternWithoutSequence>& heads, TailPattern tail);
    };

    class TailsHeadPattern : public PatternNode
    {
    protected:
        TailPattern tail;
        vector<PatternWithoutSequence> heads;

    public:
        explicit TailsHeadPattern(Token token, TailPattern tail, const vector<PatternWithoutSequence>& heads);
    };

    class HeadTailsHeadPattern : public PatternNode
    {
    protected:
        vector<PatternWithoutSequence> left;
        TailPattern tail;
        vector<PatternWithoutSequence> right;

    public:
        explicit HeadTailsHeadPattern(Token token, const vector<PatternWithoutSequence>& left, TailPattern tail,
                                      const vector<PatternWithoutSequence>& right);
    };

    class DictPattern : public PatternNode
    {
    protected:
        vector<pair<PatternValue, Pattern>> keyValuePairs;

    public:
        explicit DictPattern(Token token, const vector<pair<PatternValue, Pattern>>& keyValuePairs);
    };

    class RecordPattern : public PatternNode
    {
    protected:
        string recordType;
        vector<pair<NameExpr, Pattern>> items;

    public:
        explicit RecordPattern(Token token, string recordType, const vector<pair<NameExpr, Pattern>>& items);
    };

    class CaseExpr : public ExprNode
    {
    protected:
        ExprNode expr;
        vector<PatternExpr> patterns;

    public:
        explicit CaseExpr(Token token, ExprNode expr, const vector<PatternExpr>& patterns);
    };
}
