#pragma once

#include <antlr4-runtime.h>
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
    class AstVisitor;

    using namespace std;
    using namespace compiler::types;

    class AstNode
    {
    public:
        Token token;
        AstNode* parent = nullptr;
        explicit AstNode(Token token) : token(token) {};
        virtual ~AstNode() = default;
        virtual any accept(const AstVisitor& visitor);
        [[nodiscard]] virtual Type infer_type(TypeInferenceContext& ctx) const;

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
        any accept(const AstVisitor& visitor) override;
    };

    class PatternNode : public AstNode
    {
    public:
        explicit PatternNode(Token token) : AstNode(token) {}
        any accept(const AstVisitor& visitor) override;
    };

    class UnderscoreNode final : public PatternNode
    {
    public:
        explicit UnderscoreNode(Token token) : PatternNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ValueExpr : public ExprNode
    {
    public:
        explicit ValueExpr(Token token) : ExprNode(token) {}
        any accept(const AstVisitor& visitor) override;
    };

    class ScopedNode : public AstNode
    {
    public:
        explicit ScopedNode(Token token) : AstNode(token) {}
        ScopedNode* getParentScopedNode() const;
        any accept(const AstVisitor& visitor) override;
    };

    template <typename T>
    class LiteralExpr : public ValueExpr
    {
    public:
        const T value;

        explicit LiteralExpr(Token token, T value);
        any accept(const AstVisitor& visitor) override;
    };

    class OpExpr : public ExprNode
    {
    public:
        explicit OpExpr(Token token) : ExprNode(token) {}
        any accept(const AstVisitor& visitor) override;
    };

    class BinaryOpExpr : public OpExpr
    {
    public:
        const ExprNode left;
        const ExprNode right;

        explicit BinaryOpExpr(Token token, ExprNode left, ExprNode right);
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        any accept(const AstVisitor& visitor) override;
    };

    class AliasExpr : public ExprNode
    {
    public:
        explicit AliasExpr(Token token) : ExprNode(token) {}
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        any accept(const AstVisitor& visitor) override;
    };

    class CallExpr : public ExprNode
    {
    public:
        explicit CallExpr(Token token) : ExprNode(token) {}
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        any accept(const AstVisitor& visitor) override;
    };

    class ImportClauseExpr : public ScopedNode
    {
    public:
        explicit ImportClauseExpr(Token token) : ScopedNode(token) {}
        any accept(const AstVisitor& visitor) override;
    };

    class GeneratorExpr : public ExprNode
    {
    public:
        explicit GeneratorExpr(Token token) : ExprNode(token) {}
        any accept(const AstVisitor& visitor) override;
    };

    class CollectionExtractorExpr : public ExprNode
    {
    public:
        explicit CollectionExtractorExpr(Token token) : ExprNode(token) {}
        any accept(const AstVisitor& visitor) override;
    };

    class SequenceExpr : public ExprNode
    {
    public:
        explicit SequenceExpr(Token token) : ExprNode(token) {}
        any accept(const AstVisitor& visitor) override;
    };

    class FunctionBody : public AstNode
    {
    public:
        explicit FunctionBody(Token token) : AstNode(token) {}
        any accept(const AstVisitor& visitor) override;
    };

    class NameExpr final : public ExprNode
    {
    public:
        const string value;

        explicit NameExpr(Token token, string value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class IdentifierExpr final : public ValueExpr
    {
    public:
        const NameExpr name;

        explicit IdentifierExpr(Token token, NameExpr name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class RecordNode final : public AstNode
    {
    public:
        const NameExpr recordType;
        const vector<IdentifierExpr> identifiers;

        explicit RecordNode(Token token, NameExpr recordType, const vector<IdentifierExpr>& identifiers);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class TrueLiteralExpr final : public LiteralExpr<bool>
    {
    public:
        explicit TrueLiteralExpr(Token token);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class FalseLiteralExpr final : public LiteralExpr<bool>
    {
    public:
        explicit FalseLiteralExpr(Token token);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class FloatExpr final : public LiteralExpr<float>
    {
    public:
        explicit FloatExpr(Token token, float value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class IntegerExpr final : public LiteralExpr<int>
    {
    public:
        explicit IntegerExpr(Token token, int value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ByteExpr final : public LiteralExpr<unsigned char>
    {
    public:
        explicit ByteExpr(Token token, unsigned char value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class StringExpr final : public LiteralExpr<string>
    {
    public:
        explicit StringExpr(Token token, string value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class CharacterExpr final : public LiteralExpr<char>
    {
    public:
        explicit CharacterExpr(Token token, char value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class UnitExpr final : public LiteralExpr<nullptr_t>
    {
    public:
        explicit UnitExpr(Token token);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class TupleExpr final : public ValueExpr
    {
    public:
        const vector<ExprNode> values;

        explicit TupleExpr(Token token, const vector<ExprNode>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class DictExpr final : public ValueExpr
    {
    public:
        const vector<pair<ExprNode, ExprNode>> values;

        explicit DictExpr(Token token, const vector<pair<ExprNode, ExprNode>>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ValuesSequenceExpr final : public SequenceExpr
    {
    public:
        const vector<ExprNode> values;

        explicit ValuesSequenceExpr(Token token, const vector<ExprNode>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class RangeSequenceExpr final : public SequenceExpr
    {
    public:
        // TODO make them optional
        const ExprNode start;
        const ExprNode end;
        const ExprNode step;

        explicit RangeSequenceExpr(Token token, ExprNode start, ExprNode end, ExprNode step);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class SetExpr final : public ValueExpr
    {
    public:
        const vector<ExprNode> values;

        explicit SetExpr(Token token, const vector<ExprNode>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class SymbolExpr final : public ValueExpr
    {
    public:
        const string value;

        explicit SymbolExpr(Token token, string value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class PackageNameExpr final : public ValueExpr
    {
    public:
        const vector<NameExpr> parts;

        explicit PackageNameExpr(Token token, const vector<NameExpr>& parts);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class FqnExpr final : public ValueExpr
    {
    public:
        const PackageNameExpr packageName;
        const NameExpr moduleName;

        explicit FqnExpr(Token token, PackageNameExpr packageName, NameExpr moduleName);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class FunctionExpr final : public ScopedNode
    {
    public:
        const string name;
        const vector<PatternNode> patterns;
        const vector<FunctionBody> bodies;

        explicit FunctionExpr(Token token, string name, const vector<PatternNode>& patterns,
                              const vector<FunctionBody>& bodies);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ModuleExpr final : public ValueExpr
    {
    public:
        const FqnExpr fqn;
        const vector<string> exports;
        const vector<RecordNode> records;
        const vector<FunctionExpr> functions;

        explicit ModuleExpr(Token token, FqnExpr fqn, const vector<string>& exports, const vector<RecordNode>& records,
                            const vector<FunctionExpr>& functions);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class RecordInstanceExpr final : public ValueExpr
    {
    public:
        const NameExpr recordType;
        const vector<pair<NameExpr, ExprNode>> items;

        explicit RecordInstanceExpr(Token token, NameExpr recordType, const vector<pair<NameExpr, ExprNode>>& items);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class BodyWithGuards final : public FunctionBody
    {
    public:
        const ExprNode guard;
        const vector<ExprNode> exprs;

        explicit BodyWithGuards(Token token, ExprNode guard, const vector<ExprNode>& exprs);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class BodyWithoutGuards final : public FunctionBody
    {
    public:
        const ExprNode expr;

        explicit BodyWithoutGuards(Token token, ExprNode expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class LogicalNotOpExpr final : public OpExpr
    {
    public:
        const ExprNode expr;

        explicit LogicalNotOpExpr(Token token, ExprNode expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class BinaryNotOpExpr final : public OpExpr
    {
    public:
        const ExprNode expr;

        explicit BinaryNotOpExpr(Token token, ExprNode expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class PowerExpr final : public BinaryOpExpr
    {
    public:
        explicit PowerExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class MultiplyExpr final : public BinaryOpExpr
    {
    public:
        explicit MultiplyExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class DivideExpr final : public BinaryOpExpr
    {
    public:
        explicit DivideExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ModuloExpr final : public BinaryOpExpr
    {
    public:
        explicit ModuloExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class AddExpr final : public BinaryOpExpr
    {
    public:
        explicit AddExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class SubtractExpr final : public BinaryOpExpr
    {
    public:
        explicit SubtractExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class LeftShiftExpr final : public BinaryOpExpr
    {
    public:
        explicit LeftShiftExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class RightShiftExpr final : public BinaryOpExpr
    {
    public:
        explicit RightShiftExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ZerofillRightShiftExpr final : public BinaryOpExpr
    {
    public:
        explicit ZerofillRightShiftExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class GteExpr final : public BinaryOpExpr
    {
    public:
        explicit GteExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class LteExpr final : public BinaryOpExpr
    {
    public:
        explicit LteExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class GtExpr final : public BinaryOpExpr
    {
    public:
        explicit GtExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class LtExpr final : public BinaryOpExpr
    {
    public:
        explicit LtExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class EqExpr final : public BinaryOpExpr
    {
    public:
        explicit EqExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class NeqExpr final : public BinaryOpExpr
    {
    public:
        explicit NeqExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ConsLeftExpr final : public BinaryOpExpr
    {
    public:
        explicit ConsLeftExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ConsRightExpr final : public BinaryOpExpr
    {
    public:
        explicit ConsRightExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class JoinExpr final : public BinaryOpExpr
    {
    public:
        explicit JoinExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class BitwiseAndExpr final : public BinaryOpExpr
    {
    public:
        explicit BitwiseAndExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class BitwiseXorExpr final : public BinaryOpExpr
    {
    public:
        explicit BitwiseXorExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class BitwiseOrExpr final : public BinaryOpExpr
    {
    public:
        explicit BitwiseOrExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class LogicalAndExpr final : public BinaryOpExpr
    {
    public:
        explicit LogicalAndExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class LogicalOrExpr final : public BinaryOpExpr
    {
    public:
        explicit LogicalOrExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class InExpr final : public BinaryOpExpr
    {
    public:
        explicit InExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class PipeLeftExpr final : public BinaryOpExpr
    {
    public:
        explicit PipeLeftExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class PipeRightExpr final : public BinaryOpExpr
    {
    public:
        explicit PipeRightExpr(Token token, ExprNode left, ExprNode right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class LetExpr final : public ScopedNode
    {
    public:
        const vector<AliasExpr> aliases;
        const ExprNode expr;

        explicit LetExpr(Token token, const vector<AliasExpr>& aliases, ExprNode expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class IfExpr final : public ExprNode
    {
    public:
        const ExprNode condition;
        const ExprNode thenExpr;
        const optional<ExprNode> elseExpr;

        explicit IfExpr(Token token, ExprNode condition, ExprNode thenExpr, const optional<ExprNode>& elseExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ApplyExpr final : public ExprNode
    {
    public:
        const CallExpr call;
        const vector<variant<ExprNode, ValueExpr>> args;

        explicit ApplyExpr(Token token, CallExpr call, const vector<variant<ExprNode, ValueExpr>>& args);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class DoExpr final : public ExprNode
    {
    public:
        const vector<ExprNode> steps;

        explicit DoExpr(Token token, const vector<ExprNode>& steps);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ImportExpr final : public ScopedNode
    {
    public:
        const vector<ImportClauseExpr> clauses;
        const ExprNode expr;

        explicit ImportExpr(Token token, const vector<ImportClauseExpr>& clauses, ExprNode expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class RaiseExpr final : public ExprNode
    {
    public:
        const SymbolExpr symbol;
        const LiteralExpr<string> message;

        explicit RaiseExpr(Token token, SymbolExpr symbol, LiteralExpr<string> message);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class WithExpr final : public ScopedNode
    {
    public:
        const ExprNode contextExpr;
        const optional<NameExpr> name;
        const ExprNode bodyExpr;

        explicit WithExpr(Token token, ExprNode contextExpr, const optional<NameExpr>& name, ExprNode bodyExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class FieldAccessExpr final : public ExprNode
    {
    public:
        const IdentifierExpr identifier;
        const NameExpr name;

        explicit FieldAccessExpr(Token token, IdentifierExpr identifier, NameExpr name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class FieldUpdateExpr final : public ExprNode
    {
    public:
        const IdentifierExpr identifier;
        const vector<pair<NameExpr, ExprNode>> updates;

        explicit FieldUpdateExpr(Token token, IdentifierExpr identifier,
                                 const vector<pair<NameExpr, ExprNode>>& updates);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class LambdaAlias final : public AliasExpr
    {
    public:
        const NameExpr name;
        const FunctionExpr lambda;

        explicit LambdaAlias(Token token, NameExpr name, FunctionExpr lambda);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ModuleAlias final : public AliasExpr
    {
    public:
        const NameExpr name;
        const ModuleExpr module;

        explicit ModuleAlias(Token token, NameExpr name, ModuleExpr module);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ValueAlias final : public AliasExpr
    {
    public:
        const IdentifierExpr identifier;
        const ExprNode expr;

        explicit ValueAlias(Token token, IdentifierExpr identifier, ExprNode expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class PatternAlias final : public AliasExpr
    {
    public:
        const PatternNode pattern;
        const ExprNode expr;

        explicit PatternAlias(Token token, PatternNode pattern, ExprNode expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class FqnAlias final : public AliasExpr
    {
    public:
        const NameExpr name;
        const FqnExpr fqn;

        explicit FqnAlias(Token token, NameExpr name, FqnExpr fqn);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class FunctionAlias final : public AliasExpr
    {
    public:
        const NameExpr name;
        const NameExpr alias;

        explicit FunctionAlias(Token token, NameExpr name, NameExpr alias);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class AliasCall final : public CallExpr
    {
    public:
        const NameExpr alias;
        const NameExpr funName;

        explicit AliasCall(Token token, NameExpr alias, NameExpr funName);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class NameCall final : public CallExpr
    {
    public:
        const NameExpr name;

        explicit NameCall(Token token, NameExpr name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ModuleCall final : public CallExpr
    {
    public:
        const variant<FqnExpr, ExprNode> fqn;
        const NameExpr funName;

        explicit ModuleCall(Token token, const variant<FqnExpr, ExprNode>& fqn, NameExpr funName);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class ModuleImport final : public ImportClauseExpr
    {
    public:
        const FqnExpr fqn;
        const NameExpr name;

        explicit ModuleImport(Token token, FqnExpr fqn, NameExpr name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class FunctionsImport final : public ImportClauseExpr
    {
    public:
        const vector<FunctionAlias> aliases;
        const FqnExpr fromFqn;

        explicit FunctionsImport(Token token, const vector<FunctionAlias>& aliases, FqnExpr fromFqn);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class SeqGeneratorExpr final : public GeneratorExpr
    {
    public:
        const ExprNode reducerExpr;
        const CollectionExtractorExpr collectionExtractor;
        const ExprNode stepExpression;

        explicit SeqGeneratorExpr(Token token, ExprNode reducerExpr, CollectionExtractorExpr collectionExtractor,
                                  ExprNode stepExpression);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class SetGeneratorExpr final : public GeneratorExpr
    {
    public:
        const ExprNode reducerExpr;
        const CollectionExtractorExpr collectionExtractor;
        const ExprNode stepExpression;

        explicit SetGeneratorExpr(Token token, ExprNode reducerExpr, CollectionExtractorExpr collectionExtractor,
                                  ExprNode stepExpression);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class DictGeneratorReducer final : public ExprNode
    {
    public:
        const ExprNode key;
        const ExprNode value;

        explicit DictGeneratorReducer(Token token, ExprNode key, ExprNode value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class DictGeneratorExpr final : public GeneratorExpr
    {
    public:
        const DictGeneratorReducer reducerExpr;
        const CollectionExtractorExpr collectionExtractor;
        const ExprNode stepExpression;

        explicit DictGeneratorExpr(Token token, DictGeneratorReducer reducerExpr,
                                   CollectionExtractorExpr collectionExtractor, ExprNode stepExpression);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class UnderscorePattern final : public PatternNode
    {
    public:
        UnderscorePattern(Token token) : PatternNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    using IdentifierOrUnderscore = variant<IdentifierExpr, UnderscoreNode>;

    class ValueCollectionExtractorExpr final : public CollectionExtractorExpr
    {
    public:
        const IdentifierOrUnderscore expr;

        explicit ValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class KeyValueCollectionExtractorExpr final : public CollectionExtractorExpr
    {
    public:
        const IdentifierOrUnderscore keyExpr;
        const IdentifierOrUnderscore valueExpr;

        explicit KeyValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore keyExpr,
                                                 IdentifierOrUnderscore valueExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    using PatternWithoutSequence = PatternNode; // variant<UnderscorePattern, PatternValue, TuplePattern, DictPattern>;
    using SequencePattern =
        PatternNode; // variant<SeqPattern, HeadTailsPattern, TailsHeadPattern, HeadTailsHeadPattern>;
    using DataStructurePattern = PatternNode; // variant<TuplePattern, SequencePattern, DictPattern, RecordPattern>;
    using Pattern =
        PatternNode; // variant<UnderscorePattern, PatternValue, DataStructurePattern, AsDataStructurePattern>;
    using TailPattern = PatternNode; // variant<IdentifierExpr, SequenceExpr, UnderscoreNode, StringExpr>;

    class PatternWithGuards final : public PatternNode
    {
    public:
        const ExprNode guard;
        const ExprNode exprNode;

        explicit PatternWithGuards(Token token, ExprNode guard, ExprNode exprNode);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class PatternWithoutGuards final : public PatternNode
    {
    public:
        const ExprNode exprNode;

        explicit PatternWithoutGuards(Token token, ExprNode exprNode);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class PatternExpr : public ExprNode
    {
    public:
        const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>> patternExpr;

        explicit PatternExpr(Token token,
                             const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>>& patternExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class CatchPatternExpr final : public ExprNode
    {
    public:
        const Pattern matchPattern;
        const variant<PatternWithoutGuards, vector<PatternWithGuards>> pattern;

        explicit CatchPatternExpr(Token token, Pattern matchPattern,
                                  const variant<PatternWithoutGuards, vector<PatternWithGuards>>& pattern);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class CatchExpr final : public ExprNode
    {
    public:
        const vector<CatchPatternExpr> patterns;

        explicit CatchExpr(Token token, const vector<CatchPatternExpr>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class TryCatchExpr final : public ExprNode
    {
    public:
        const ExprNode tryExpr;
        const CatchExpr catchExpr;

        explicit TryCatchExpr(Token token, ExprNode tryExpr, CatchExpr catchExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class PatternValue final : public PatternNode
    {
    public:
        const variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr> expr;

        explicit PatternValue(
            Token token, const variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr>& expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class AsDataStructurePattern final : public PatternNode
    {
    public:
        const IdentifierExpr identifier;
        const DataStructurePattern pattern;

        explicit AsDataStructurePattern(Token token, IdentifierExpr identifier, DataStructurePattern pattern);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class TuplePattern final : public PatternNode
    {
    public:
        const vector<Pattern> patterns;

        explicit TuplePattern(Token token, const vector<Pattern>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class SeqPattern final : public PatternNode
    {
    public:
        const vector<Pattern> patterns;

        explicit SeqPattern(Token token, const vector<Pattern>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class HeadTailsPattern final : public PatternNode
    {
    public:
        const vector<PatternWithoutSequence> heads;
        const TailPattern tail;

        explicit HeadTailsPattern(Token token, const vector<PatternWithoutSequence>& heads, TailPattern tail);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class TailsHeadPattern final : public PatternNode
    {
    public:
        const TailPattern tail;
        const vector<PatternWithoutSequence> heads;

        explicit TailsHeadPattern(Token token, TailPattern tail, const vector<PatternWithoutSequence>& heads);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class HeadTailsHeadPattern final : public PatternNode
    {
    public:
        const vector<PatternWithoutSequence> left;
        const TailPattern tail;
        const vector<PatternWithoutSequence> right;

        explicit HeadTailsHeadPattern(Token token, const vector<PatternWithoutSequence>& left, TailPattern tail,
                                      const vector<PatternWithoutSequence>& right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class DictPattern final : public PatternNode
    {
    public:
        const vector<pair<PatternValue, Pattern>> keyValuePairs;

        explicit DictPattern(Token token, const vector<pair<PatternValue, Pattern>>& keyValuePairs);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class RecordPattern final : public PatternNode
    {
    public:
        const string recordType;
        const vector<pair<NameExpr, Pattern>> items;

        explicit RecordPattern(Token token, string recordType, const vector<pair<NameExpr, Pattern>>& items);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class CaseExpr final : public ExprNode
    {
    public:
        const ExprNode expr;
        const vector<PatternExpr> patterns;

        explicit CaseExpr(Token token, ExprNode expr, const vector<PatternExpr>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
    };

    class AstVisitor
    {
    public:
        virtual ~AstVisitor() = default;
        virtual any visit(const AddExpr& node) const = 0;
        virtual any visit(const AliasCall& node) const = 0;
        virtual any visit(const AliasExpr& node) const = 0;
        virtual any visit(const ApplyExpr& node) const = 0;
        virtual any visit(const AsDataStructurePattern& node) const = 0;
        virtual any visit(const BinaryNotOpExpr& node) const = 0;
        virtual any visit(const BitwiseAndExpr& node) const = 0;
        virtual any visit(const BitwiseOrExpr& node) const = 0;
        virtual any visit(const BitwiseXorExpr& node) const = 0;
        virtual any visit(const BodyWithGuards& node) const = 0;
        virtual any visit(const BodyWithoutGuards& node) const = 0;
        virtual any visit(const ByteExpr& node) const = 0;
        virtual any visit(const CaseExpr& node) const = 0;
        virtual any visit(const CatchExpr& node) const = 0;
        virtual any visit(const CatchPatternExpr& node) const = 0;
        virtual any visit(const CharacterExpr& node) const = 0;
        virtual any visit(const ConsLeftExpr& node) const = 0;
        virtual any visit(const ConsRightExpr& node) const = 0;
        virtual any visit(const DictExpr& node) const = 0;
        virtual any visit(const DictGeneratorExpr& node) const = 0;
        virtual any visit(const DictGeneratorReducer& node) const = 0;
        virtual any visit(const DictPattern& node) const = 0;
        virtual any visit(const DivideExpr& node) const = 0;
        virtual any visit(const DoExpr& node) const = 0;
        virtual any visit(const EqExpr& node) const = 0;
        virtual any visit(const FalseLiteralExpr& node) const = 0;
        virtual any visit(const FieldAccessExpr& node) const = 0;
        virtual any visit(const FieldUpdateExpr& node) const = 0;
        virtual any visit(const FloatExpr& node) const = 0;
        virtual any visit(const FqnAlias& node) const = 0;
        virtual any visit(const FqnExpr& node) const = 0;
        virtual any visit(const FunctionAlias& node) const = 0;
        virtual any visit(const FunctionBody& node) const = 0;
        virtual any visit(const FunctionExpr& node) const = 0;
        virtual any visit(const FunctionsImport& node) const = 0;
        virtual any visit(const GtExpr& node) const = 0;
        virtual any visit(const GteExpr& node) const = 0;
        virtual any visit(const HeadTailsHeadPattern& node) const = 0;
        virtual any visit(const HeadTailsPattern& node) const = 0;
        virtual any visit(const IdentifierExpr& node) const = 0;
        virtual any visit(const IfExpr& node) const = 0;
        virtual any visit(const ImportClauseExpr& node) const = 0;
        virtual any visit(const ImportExpr& node) const = 0;
        virtual any visit(const InExpr& node) const = 0;
        virtual any visit(const IntegerExpr& node) const = 0;
        virtual any visit(const JoinExpr& node) const = 0;
        virtual any visit(const KeyValueCollectionExtractorExpr& node) const = 0;
        virtual any visit(const LambdaAlias& node) const = 0;
        virtual any visit(const LeftShiftExpr& node) const = 0;
        virtual any visit(const LetExpr& node) const = 0;
        virtual any visit(const LogicalAndExpr& node) const = 0;
        virtual any visit(const LogicalNotOpExpr& node) const = 0;
        virtual any visit(const LogicalOrExpr& node) const = 0;
        virtual any visit(const LtExpr& node) const = 0;
        virtual any visit(const LteExpr& node) const = 0;
        virtual any visit(const ModuloExpr& node) const = 0;
        virtual any visit(const ModuleAlias& node) const = 0;
        virtual any visit(const ModuleCall& node) const = 0;
        virtual any visit(const ModuleExpr& node) const = 0;
        virtual any visit(const ModuleImport& node) const = 0;
        virtual any visit(const MultiplyExpr& node) const = 0;
        virtual any visit(const NameCall& node) const = 0;
        virtual any visit(const NameExpr& node) const = 0;
        virtual any visit(const NeqExpr& node) const = 0;
        virtual any visit(const OpExpr& node) const = 0;
        virtual any visit(const PackageNameExpr& node) const = 0;
        virtual any visit(const PatternAlias& node) const = 0;
        virtual any visit(const PatternExpr& node) const = 0;
        virtual any visit(const PatternValue& node) const = 0;
        virtual any visit(const PatternWithGuards& node) const = 0;
        virtual any visit(const PatternWithoutGuards& node) const = 0;
        virtual any visit(const PipeLeftExpr& node) const = 0;
        virtual any visit(const PipeRightExpr& node) const = 0;
        virtual any visit(const PowerExpr& node) const = 0;
        virtual any visit(const RaiseExpr& node) const = 0;
        virtual any visit(const RangeSequenceExpr& node) const = 0;
        virtual any visit(const RecordInstanceExpr& node) const = 0;
        virtual any visit(const RecordNode& node) const = 0;
        virtual any visit(const RecordPattern& node) const = 0;
        virtual any visit(const RightShiftExpr& node) const = 0;
        virtual any visit(const SeqGeneratorExpr& node) const = 0;
        virtual any visit(const SeqPattern& node) const = 0;
        virtual any visit(const SequenceExpr& node) const = 0;
        virtual any visit(const SetExpr& node) const = 0;
        virtual any visit(const SetGeneratorExpr& node) const = 0;
        virtual any visit(const StringExpr& node) const = 0;
        virtual any visit(const SubtractExpr& node) const = 0;
        virtual any visit(const SymbolExpr& node) const = 0;
        virtual any visit(const TailsHeadPattern& node) const = 0;
        virtual any visit(const TrueLiteralExpr& node) const = 0;
        virtual any visit(const TryCatchExpr& node) const = 0;
        virtual any visit(const TupleExpr& node) const = 0;
        virtual any visit(const TuplePattern& node) const = 0;
        virtual any visit(const UnderscoreNode& node) const = 0;
        virtual any visit(const UnitExpr& node) const = 0;
        virtual any visit(const ValueAlias& node) const = 0;
        virtual any visit(const ValueCollectionExtractorExpr& node) const = 0;
        virtual any visit(const ValueExpr& node) const = 0;
        virtual any visit(const ValuesSequenceExpr& node) const = 0;
        virtual any visit(const WithExpr& node) const = 0;
        virtual any visit(const ZerofillRightShiftExpr& node) const = 0;
        virtual any visit(const ExprNode& node) const;
        virtual any visit(const AstNode& node) const;
        virtual any visit(const ScopedNode& node) const;
        virtual any visit(const PatternNode& node) const;
    };
}
