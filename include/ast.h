#pragma once

#include <antlr4-runtime.h>
#include <concepts>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "types.h"

namespace yona::ast
{
    class AstNode;
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
    using namespace compiler;
    using Token = const antlr4::ParserRuleContext&;

    class AstNode
    {
    public:
        Token token;
        AstNode* parent = nullptr;
        explicit AstNode(Token token) : token(token) {};
        virtual ~AstNode() = default;
        Type infer_type();

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

    class ScopedNode : public AstNode
    {
    public:
        explicit ScopedNode(Token token) : AstNode(token) {}
        ScopedNode* getParentScopedNode() const;
    };

    template <typename T>
    class LiteralExpr : public ValueExpr
    {
    public:
        const T value;

        explicit LiteralExpr(Token token, T value);
    };
    class OpExpr : public ExprNode
    {
    public:
        explicit OpExpr(Token token) : ExprNode(token) {}
    };

    class BinaryOpExpr : public OpExpr
    {
    public:
        const ExprNode left;
        const ExprNode right;

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

    class ImportClauseExpr : public ScopedNode
    {
    public:
        explicit ImportClauseExpr(Token token) : ScopedNode(token) {}
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
    public:
        const string value;

        explicit NameExpr(Token token, string value);
    };

    class IdentifierExpr : public ValueExpr
    {
    public:
        const NameExpr name;

        explicit IdentifierExpr(Token token, NameExpr name);
    };

    class RecordNode : public AstNode
    {
    public:
        const NameExpr recordType;
        const vector<IdentifierExpr> identifiers;

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
    public:
        const vector<ExprNode> values;

        explicit TupleExpr(Token token, const vector<ExprNode>& values);
    };

    class DictExpr : public ValueExpr
    {
    public:
        const vector<pair<ExprNode, ExprNode>> values;

        explicit DictExpr(Token token, const vector<pair<ExprNode, ExprNode>>& values);
    };

    class ValuesSequenceExpr : public SequenceExpr
    {
    public:
        const vector<ExprNode> values;

        explicit ValuesSequenceExpr(Token token, const vector<ExprNode>& values);
    };

    class RangeSequenceExpr : public SequenceExpr
    {
    public:
        const ExprNode start;
        const ExprNode end;
        const ExprNode step;

        explicit RangeSequenceExpr(Token token, ExprNode start, ExprNode end, ExprNode step);
    };

    class SetExpr : public ValueExpr
    {
    public:
        const vector<ExprNode> values;

        explicit SetExpr(Token token, const vector<ExprNode>& values);
    };

    class SymbolExpr : public ValueExpr
    {
    public:
        const string value;

        explicit SymbolExpr(Token token, string value);
    };

    class PackageNameExpr : public ValueExpr
    {
    public:
        const vector<NameExpr> parts;

        explicit PackageNameExpr(Token token, const vector<NameExpr>& parts);
    };

    class FqnExpr : public ValueExpr
    {
    public:
        const PackageNameExpr packageName;
        const NameExpr moduleName;

        explicit FqnExpr(Token token, PackageNameExpr packageName, NameExpr moduleName);
    };

    class FunctionExpr : public ScopedNode
    {
    public:
        const string name;
        const vector<PatternNode> patterns;
        const vector<FunctionBody> bodies;

        explicit FunctionExpr(Token token, string name, const vector<PatternNode>& patterns,
                              const vector<FunctionBody>& bodies);
    };

    class ModuleExpr : public ValueExpr
    {
    public:
        const FqnExpr fqn;
        const vector<string> exports;
        const vector<RecordNode> records;
        const vector<FunctionExpr> functions;

        explicit ModuleExpr(Token token, FqnExpr fqn, const vector<string>& exports, const vector<RecordNode>& records,
                            const vector<FunctionExpr>& functions);
    };

    class RecordInstanceExpr : public ValueExpr
    {
    public:
        const NameExpr recordType;
        const vector<pair<NameExpr, ExprNode>> items;

        explicit RecordInstanceExpr(Token token, NameExpr recordType, const vector<pair<NameExpr, ExprNode>>& items);
    };

    class BodyWithGuards : public FunctionBody
    {
    public:
        const ExprNode guard;
        const vector<ExprNode> exprs;

        explicit BodyWithGuards(Token token, ExprNode guard, const vector<ExprNode>& exprs);
    };

    class BodyWithoutGuards : public FunctionBody
    {
    public:
        const ExprNode expr;

        explicit BodyWithoutGuards(Token token, ExprNode expr);
    };

    class LogicalNotOpExpr : public OpExpr
    {
    public:
        const ExprNode expr;

        explicit LogicalNotOpExpr(Token token, ExprNode expr);
    };

    class BinaryNotOpExpr : public OpExpr
    {
    public:
        const ExprNode expr;

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

    class LetExpr : public ScopedNode
    {
    public:
        const vector<AliasExpr> aliases;
        const ExprNode expr;

        explicit LetExpr(Token token, const vector<AliasExpr>& aliases, ExprNode expr);
    };

    class IfExpr : public ExprNode
    {
    public:
        const ExprNode condition;
        const ExprNode thenExpr;
        const optional<ExprNode> elseExpr;

        explicit IfExpr(Token token, ExprNode condition, ExprNode thenExpr, const optional<ExprNode>& elseExpr);
    };

    class ApplyExpr : public ExprNode
    {
    public:
        const CallExpr call;
        const vector<variant<ExprNode, ValueExpr>> args;

        explicit ApplyExpr(Token token, CallExpr call, const vector<variant<ExprNode, ValueExpr>>& args);
    };

    class DoExpr : public ExprNode
    {
    public:
        const vector<variant<AliasExpr, ExprNode>> steps;

        explicit DoExpr(Token token, const vector<variant<AliasExpr, ExprNode>>& steps);
    };

    class ImportExpr : public ScopedNode
    {
    public:
        const vector<ImportClauseExpr> clauses;
        const ExprNode expr;

        explicit ImportExpr(Token token, const vector<ImportClauseExpr>& clauses, ExprNode expr);
    };

    class RaiseExpr : public ExprNode
    {
    public:
        const SymbolExpr symbol;
        const LiteralExpr<string> message;

        explicit RaiseExpr(Token token, SymbolExpr symbol, LiteralExpr<string> message);
    };

    class WithExpr : public ScopedNode
    {
    public:
        const ExprNode contextExpr;
        const optional<NameExpr> name;
        const ExprNode bodyExpr;

        explicit WithExpr(Token token, ExprNode contextExpr, const optional<NameExpr>& name, ExprNode bodyExpr);
    };

    class FieldAccessExpr : public ExprNode
    {
    public:
        const IdentifierExpr identifier;
        const NameExpr name;

        explicit FieldAccessExpr(Token token, IdentifierExpr identifier, NameExpr name);
    };

    class FieldUpdateExpr : public ExprNode
    {
    public:
        const IdentifierExpr identifier;
        const vector<pair<NameExpr, ExprNode>> updates;

        explicit FieldUpdateExpr(Token token, IdentifierExpr identifier,
                                 const vector<pair<NameExpr, ExprNode>>& updates);
    };

    class LambdaAlias : public AliasExpr
    {
    public:
        const NameExpr name;
        const FunctionExpr lambda;

        explicit LambdaAlias(Token token, NameExpr name, FunctionExpr lambda);
    };

    class ModuleAlias : public AliasExpr
    {
    public:
        const NameExpr name;
        const ModuleExpr module;

        explicit ModuleAlias(Token token, NameExpr name, ModuleExpr module);
    };

    class ValueAlias : public AliasExpr
    {
    public:
        const IdentifierExpr identifier;
        const ExprNode expr;

        explicit ValueAlias(Token token, IdentifierExpr identifier, ExprNode expr);
    };

    class PatternAlias : public AliasExpr
    {
    public:
        const PatternNode pattern;
        const ExprNode expr;

        explicit PatternAlias(Token token, PatternNode pattern, ExprNode expr);
    };

    class FqnAlias : public AliasExpr
    {
    public:
        const NameExpr name;
        const FqnExpr fqn;

        explicit FqnAlias(Token token, NameExpr name, FqnExpr fqn);
    };

    class FunctionAlias : public AliasExpr
    {
    public:
        const NameExpr name;
        const NameExpr alias;

        explicit FunctionAlias(Token token, NameExpr name, NameExpr alias);
    };

    class AliasCall : public CallExpr
    {
    public:
        const NameExpr alias;
        const NameExpr funName;

        explicit AliasCall(Token token, NameExpr alias, NameExpr funName);
    };

    class NameCall : public CallExpr
    {
    public:
        const NameExpr name;

        explicit NameCall(Token token, NameExpr name);
    };

    class ModuleCall : public CallExpr
    {
    public:
        const variant<FqnExpr, ExprNode> fqn;
        const NameExpr funName;

        explicit ModuleCall(Token token, const variant<FqnExpr, ExprNode>& fqn, NameExpr funName);
    };

    class ModuleImport : public ImportClauseExpr
    {
    public:
        const FqnExpr fqn;
        const NameExpr name;

        explicit ModuleImport(Token token, FqnExpr fqn, NameExpr name);
    };

    class FunctionsImport : public ImportClauseExpr
    {
    public:
        const vector<FunctionAlias> aliases;
        const FqnExpr fromFqn;

        explicit FunctionsImport(Token token, const vector<FunctionAlias>& aliases, FqnExpr fromFqn);
    };

    class SeqGeneratorExpr : public GeneratorExpr
    {
    public:
        const ExprNode reducerExpr;
        const CollectionExtractorExpr collectionExtractor;
        const ExprNode stepExpression;

        explicit SeqGeneratorExpr(Token token, ExprNode reducerExpr, CollectionExtractorExpr collectionExtractor,
                                  ExprNode stepExpression);
    };

    class SetGeneratorExpr : public GeneratorExpr
    {
    public:
        const ExprNode reducerExpr;
        const CollectionExtractorExpr collectionExtractor;
        const ExprNode stepExpression;

        explicit SetGeneratorExpr(Token token, ExprNode reducerExpr, CollectionExtractorExpr collectionExtractor,
                                  ExprNode stepExpression);
    };

    class DictGeneratorReducer : public ExprNode
    {
    public:
        const ExprNode key;
        const ExprNode value;

        explicit DictGeneratorReducer(Token token, ExprNode key, ExprNode value);
    };

    class DictGeneratorExpr : public GeneratorExpr
    {
    public:
        const DictGeneratorReducer reducerExpr;
        const CollectionExtractorExpr collectionExtractor;
        const ExprNode stepExpression;

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
    public:
        const IdentifierOrUnderscore expr;

        explicit ValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore expr);
    };

    class KeyValueCollectionExtractorExpr : public CollectionExtractorExpr
    {
    public:
        const IdentifierOrUnderscore keyExpr;
        const IdentifierOrUnderscore valueExpr;

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
    public:
        const ExprNode guard;
        const ExprNode exprNode;

        explicit PatternWithGuards(Token token, ExprNode guard, ExprNode exprNode);
    };

    class PatternWithoutGuards : public PatternNode
    {
    public:
        const ExprNode exprNode;

        explicit PatternWithoutGuards(Token token, ExprNode exprNode);
    };

    class PatternExpr : public ExprNode
    {
    public:
        const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>> patternExpr;

        explicit PatternExpr(Token token,
                             const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>>& patternExpr);
    };

    class CatchPatternExpr : public ExprNode
    {
    public:
        const Pattern matchPattern;
        const variant<PatternWithoutGuards, vector<PatternWithGuards>> pattern;

        explicit CatchPatternExpr(Token token, Pattern matchPattern,
                                  const variant<PatternWithoutGuards, vector<PatternWithGuards>>& pattern);
    };

    class CatchExpr : public ExprNode
    {
    public:
        const vector<CatchPatternExpr> patterns;

        explicit CatchExpr(Token token, const vector<CatchPatternExpr>& patterns);
    };

    class TryCatchExpr : public ExprNode
    {
    public:
        const ExprNode tryExpr;
        const CatchExpr catchExpr;

        explicit TryCatchExpr(Token token, ExprNode tryExpr, CatchExpr catchExpr);
    };

    class PatternValue : public PatternNode
    {
    public:
        const variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr> expr;

        explicit PatternValue(
            Token token, const variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr>& expr);
    };

    class AsDataStructurePattern : public PatternNode
    {
    public:
        const IdentifierExpr identifier;
        const DataStructurePattern pattern;

        explicit AsDataStructurePattern(Token token, IdentifierExpr identifier, DataStructurePattern pattern);
    };

    class TuplePattern : public PatternNode
    {
    public:
        const vector<Pattern> patterns;

        explicit TuplePattern(Token token, const vector<Pattern>& patterns);
    };

    class SeqPattern : public PatternNode
    {
    public:
        const vector<Pattern> patterns;

        explicit SeqPattern(Token token, const vector<Pattern>& patterns);
    };

    class HeadTailsPattern : public PatternNode
    {
    protected:
        const vector<PatternWithoutSequence> heads;
        const TailPattern tail;

    public:
        explicit HeadTailsPattern(Token token, const vector<PatternWithoutSequence>& heads, TailPattern tail);
    };

    class TailsHeadPattern : public PatternNode
    {
    public:
        const TailPattern tail;
        const vector<PatternWithoutSequence> heads;

        explicit TailsHeadPattern(Token token, TailPattern tail, const vector<PatternWithoutSequence>& heads);
    };

    class HeadTailsHeadPattern : public PatternNode
    {
    public:
        const vector<PatternWithoutSequence> left;
        const TailPattern tail;
        const vector<PatternWithoutSequence> right;

        explicit HeadTailsHeadPattern(Token token, const vector<PatternWithoutSequence>& left, TailPattern tail,
                                      const vector<PatternWithoutSequence>& right);
    };

    class DictPattern : public PatternNode
    {
    public:
        const vector<pair<PatternValue, Pattern>> keyValuePairs;

        explicit DictPattern(Token token, const vector<pair<PatternValue, Pattern>>& keyValuePairs);
    };

    class RecordPattern : public PatternNode
    {
    public:
        const string recordType;
        const vector<pair<NameExpr, Pattern>> items;

        explicit RecordPattern(Token token, string recordType, const vector<pair<NameExpr, Pattern>>& items);
    };

    class CaseExpr : public ExprNode
    {
    public:
        const ExprNode expr;
        const vector<PatternExpr> patterns;

        explicit CaseExpr(Token token, ExprNode expr, const vector<PatternExpr>& patterns);
    };

    template <typename T>
    class AstVisitor
    {
    public:
        virtual ~AstVisitor() = default;
        virtual T visit(const AddExpr& node) const = 0;
        virtual T visit(const AliasCall& node) const = 0;
        virtual T visit(const AliasExpr& node) const = 0;
        virtual T visit(const ApplyExpr& node) const = 0;
        virtual T visit(const AsDataStructurePattern& node) const = 0;
        virtual T visit(const BinaryNotOpExpr& node) const = 0;
        virtual T visit(const BitwiseAndExpr& node) const = 0;
        virtual T visit(const BitwiseOrExpr& node) const = 0;
        virtual T visit(const BitwiseXorExpr& node) const = 0;
        virtual T visit(const BodyWithGuards& node) const = 0;
        virtual T visit(const BodyWithoutGuards& node) const = 0;
        virtual T visit(const ByteExpr& node) const = 0;
        virtual T visit(const CaseExpr& node) const = 0;
        virtual T visit(const CatchExpr& node) const = 0;
        virtual T visit(const CatchPatternExpr& node) const = 0;
        virtual T visit(const CharacterExpr& node) const = 0;
        virtual T visit(const ConsLeftExpr& node) const = 0;
        virtual T visit(const ConsRightExpr& node) const = 0;
        virtual T visit(const DictExpr& node) const = 0;
        virtual T visit(const DictGeneratorExpr& node) const = 0;
        virtual T visit(const DictGeneratorReducer& node) const = 0;
        virtual T visit(const DictPattern& node) const = 0;
        virtual T visit(const DivideExpr& node) const = 0;
        virtual T visit(const DoExpr& node) const = 0;
        virtual T visit(const EqExpr& node) const = 0;
        virtual T visit(const FalseLiteralExpr& node) const = 0;
        virtual T visit(const FieldAccessExpr& node) const = 0;
        virtual T visit(const FieldUpdateExpr& node) const = 0;
        virtual T visit(const FloatExpr& node) const = 0;
        virtual T visit(const FqnAlias& node) const = 0;
        virtual T visit(const FqnExpr& node) const = 0;
        virtual T visit(const FunctionAlias& node) const = 0;
        virtual T visit(const FunctionBody& node) const = 0;
        virtual T visit(const FunctionExpr& node) const = 0;
        virtual T visit(const FunctionsImport& node) const = 0;
        virtual T visit(const GtExpr& node) const = 0;
        virtual T visit(const GteExpr& node) const = 0;
        virtual T visit(const HeadTailsHeadPattern& node) const = 0;
        virtual T visit(const HeadTailsPattern& node) const = 0;
        virtual T visit(const IdentifierExpr& node) const = 0;
        virtual T visit(const IfExpr& node) const = 0;
        virtual T visit(const ImportClauseExpr& node) const = 0;
        virtual T visit(const ImportExpr& node) const = 0;
        virtual T visit(const InExpr& node) const = 0;
        virtual T visit(const IntegerExpr& node) const = 0;
        virtual T visit(const JoinExpr& node) const = 0;
        virtual T visit(const KeyValueCollectionExtractorExpr& node) const = 0;
        virtual T visit(const LambdaAlias& node) const = 0;
        virtual T visit(const LeftShiftExpr& node) const = 0;
        virtual T visit(const LetExpr& node) const = 0;
        virtual T visit(const LogicalAndExpr& node) const = 0;
        virtual T visit(const LogicalNotOpExpr& node) const = 0;
        virtual T visit(const LogicalOrExpr& node) const = 0;
        virtual T visit(const LtExpr& node) const = 0;
        virtual T visit(const LteExpr& node) const = 0;
        virtual T visit(const ModuloExpr& node) const = 0;
        virtual T visit(const ModuleAlias& node) const = 0;
        virtual T visit(const ModuleCall& node) const = 0;
        virtual T visit(const ModuleExpr& node) const = 0;
        virtual T visit(const ModuleImport& node) const = 0;
        virtual T visit(const MultiplyExpr& node) const = 0;
        virtual T visit(const NameCall& node) const = 0;
        virtual T visit(const NameExpr& node) const = 0;
        virtual T visit(const NeqExpr& node) const = 0;
        virtual T visit(const OpExpr& node) const = 0;
        virtual T visit(const PackageNameExpr& node) const = 0;
        virtual T visit(const PatternAlias& node) const = 0;
        virtual T visit(const PatternExpr& node) const = 0;
        virtual T visit(const PatternValue& node) const = 0;
        virtual T visit(const PatternWithGuards& node) const = 0;
        virtual T visit(const PatternWithoutGuards& node) const = 0;
        virtual T visit(const PipeLeftExpr& node) const = 0;
        virtual T visit(const PipeRightExpr& node) const = 0;
        virtual T visit(const PowerExpr& node) const = 0;
        virtual T visit(const RaiseExpr& node) const = 0;
        virtual T visit(const RangeSequenceExpr& node) const = 0;
        virtual T visit(const RecordInstanceExpr& node) const = 0;
        virtual T visit(const RecordNode& node) const = 0;
        virtual T visit(const RecordPattern& node) const = 0;
        virtual T visit(const RightShiftExpr& node) const = 0;
        virtual T visit(const SeqGeneratorExpr& node) const = 0;
        virtual T visit(const SeqPattern& node) const = 0;
        virtual T visit(const SequenceExpr& node) const = 0;
        virtual T visit(const SetExpr& node) const = 0;
        virtual T visit(const SetGeneratorExpr& node) const = 0;
        virtual T visit(const StringExpr& node) const = 0;
        virtual T visit(const SubtractExpr& node) const = 0;
        virtual T visit(const SymbolExpr& node) const = 0;
        virtual T visit(const TailsHeadPattern& node) const = 0;
        virtual T visit(const TrueLiteralExpr& node) const = 0;
        virtual T visit(const TryCatchExpr& node) const = 0;
        virtual T visit(const TupleExpr& node) const = 0;
        virtual T visit(const TuplePattern& node) const = 0;
        virtual T visit(const UnitExpr& node) const = 0;
        virtual T visit(const ValueAlias& node) const = 0;
        virtual T visit(const ValueCollectionExtractorExpr& node) const = 0;
        virtual T visit(const ValueExpr& node) const = 0;
        virtual T visit(const ValuesSequenceExpr& node) const = 0;
        virtual T visit(const WithExpr& node) const = 0;
        virtual T visit(const ZerofillRightShiftExpr& node) const = 0;
        virtual T visit(const ExprNode& node) const;
        virtual T visit(const AstNode& node) const;
        virtual T visit(const ScopedNode& node) const;
        virtual T visit(const PatternNode& node) const;
    };
}
