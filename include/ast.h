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

    enum AstNodeType
    {
        AST_NODE,
        AST_EXPR_NODE,
        AST_PATTERN_NODE,
        AST_UNDERSCORE_NODE,
        AST_VALUE_EXPR,
        AST_SCOPED_NODE,
        AST_LITERAL_EXPR,
        AST_OP_EXPR,
        AST_BINARY_OP_EXPR,
        AST_ALIAS_EXPR,
        AST_CALL_EXPR,
        AST_IMPORT_CLAUSE_EXPR,
        AST_GENERATOR_EXPR,
        AST_COLLECTION_EXTRACTOR_EXPR,
        AST_SEQUENCE_EXPR,
        AST_FUNCTION_BODY,
        AST_NAME_EXPR,
        AST_IDENTIFIER_EXPR,
        AST_RECORD_NODE,
        AST_TRUE_LITERAL_EXPR,
        AST_FALSE_LITERAL_EXPR,
        AST_FLOAT_EXPR,
        AST_INTEGER_EXPR,
        AST_BYTE_EXPR,
        AST_STRING_EXPR,
        AST_CHARACTER_EXPR,
        AST_UNIT_EXPR,
        AST_TUPLE_EXPR,
        AST_DICT_EXPR,
        AST_VALUES_SEQUENCE_EXPR,
        AST_RANGE_SEQUENCE_EXPR,
        AST_SET_EXPR,
        AST_SYMBOL_EXPR,
        AST_PACKAGE_NAME_EXPR,
        AST_FQN_EXPR,
        AST_FUNCTION_EXPR,
        AST_MODULE_EXPR,
        AST_RECORD_INSTANCE_EXPR,
        AST_BODY_WITH_GUARDS,
        AST_BODY_WITHOUT_GUARDS,
        AST_LOGICAL_NOT_OP_EXPR,
        AST_BINARY_NOT_OP_EXPR,
        AST_POWER_EXPR,
        AST_MULTIPLY_EXPR,
        AST_DIVIDE_EXPR,
        AST_MODULO_EXPR,
        AST_ADD_EXPR,
        AST_SUBTRACT_EXPR,
        AST_LEFT_SHIFT_EXPR,
        AST_RIGHT_SHIFT_EXPR,
        AST_ZEROFILL_RIGHT_SHIFT_EXPR,
        AST_GTE_EXPR,
        AST_LTE_EXPR,
        AST_GT_EXPR,
        AST_LT_EXPR,
        AST_EQ_EXPR,
        AST_NEQ_EXPR,
        AST_CONS_LEFT_EXPR,
        AST_CONS_RIGHT_EXPR,
        AST_JOIN_EXPR,
        AST_BITWISE_AND_EXPR,
        AST_BITWISE_XOR_EXPR,
        AST_BITWISE_OR_EXPR,
        AST_LOGICAL_AND_EXPR,
        AST_LOGICAL_OR_EXPR,
        AST_IN_EXPR,
        AST_PIPE_LEFT_EXPR,
        AST_PIPE_RIGHT_EXPR,
        AST_LET_EXPR,
        AST_IF_EXPR,
        AST_APPLY_EXPR,
        AST_DO_EXPR,
        AST_IMPORT_EXPR,
        AST_RAISE_EXPR,
        AST_WITH_EXPR,
        AST_FIELD_ACCESS_EXPR,
        AST_FIELD_UPDATE_EXPR,
        AST_LAMBDA_ALIAS,
        AST_MODULE_ALIAS,
        AST_VALUE_ALIAS,
        AST_PATTERN_ALIAS,
        AST_FQN_ALIAS,
        AST_FUNCTION_ALIAS,
        AST_ALIAS_CALL,
        AST_NAME_CALL,
        AST_MODULE_CALL,
        AST_MODULE_IMPORT,
        AST_FUNCTIONS_IMPORT,
        AST_SEQ_GENERATOR_EXPR,
        AST_SET_GENERATOR_EXPR,
        AST_DICT_GENERATOR_REDUCER,
        AST_DICT_GENERATOR_EXPR,
        AST_UNDERSCORE_PATTERN,
        AST_VALUE_COLLECTION_EXTRACTOR_EXPR,
        AST_KEY_VALUE_COLLECTION_EXTRACTOR_EXPR,
        AST_PATTERN_WITH_GUARDS,
        AST_PATTERN_WITHOUT_GUARDS,
        AST_PATTERN_EXPR,
        AST_CATCH_PATTERN_EXPR,
        AST_CATCH_EXPR,
        AST_TRY_CATCH_EXPR,
        AST_PATTERN_VALUE,
        AST_AS_DATA_STRUCTURE_PATTERN,
        AST_TUPLE_PATTERN,
        AST_SEQ_PATTERN,
        AST_HEAD_TAILS_PATTERN,
        AST_TAILS_HEAD_PATTERN,
        AST_HEAD_TAILS_HEAD_PATTERN,
        AST_DICT_PATTERN,
        AST_RECORD_PATTERN,
        AST_CASE_EXPR
    };

    class AstNode
    {
    public:
        Token token;
        AstNode* parent = nullptr;
        explicit AstNode(Token token) : token(token) {};
        virtual ~AstNode() = default;
        virtual any accept(const AstVisitor& visitor);
        [[nodiscard]] virtual Type infer_type(TypeInferenceContext& ctx) const;
        [[nodiscard]] virtual AstNodeType get_type() const { return AST_NODE; };

        template <typename T>
            requires derived_from<T, AstNode>
        const T* with_parent(AstNode* parent)
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
        [[nodiscard]] AstNodeType get_type() const override { return AST_EXPR_NODE; };
    };

    class PatternNode : public AstNode
    {
    public:
        explicit PatternNode(Token token) : AstNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_NODE; };
    };

    class UnderscoreNode final : public PatternNode
    {
    public:
        explicit UnderscoreNode(Token token) : PatternNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_UNDERSCORE_NODE; };
    };

    class ValueExpr : public ExprNode
    {
    public:
        explicit ValueExpr(Token token) : ExprNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_VALUE_EXPR; };
    };

    class ScopedNode : public AstNode
    {
    public:
        explicit ScopedNode(Token token) : AstNode(token) {}
        ScopedNode* getParentScopedNode() const;
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SCOPED_NODE; };
    };

    template <typename T>
    class LiteralExpr : public ValueExpr
    {
    public:
        const T value;

        explicit LiteralExpr(Token token, T value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LITERAL_EXPR; };
    };

    class OpExpr : public ExprNode
    {
    public:
        explicit OpExpr(Token token) : ExprNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_OP_EXPR; };
    };

    class BinaryOpExpr : public OpExpr
    {
    public:
        const ExprNode* const left;
        const ExprNode* const right;

        explicit BinaryOpExpr(Token token, ExprNode* left, ExprNode* right);
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BINARY_OP_EXPR; };
    };

    class AliasExpr : public ExprNode
    {
    public:
        explicit AliasExpr(Token token) : ExprNode(token) {}
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_ALIAS_EXPR; };
    };

    class CallExpr : public ExprNode
    {
    public:
        explicit CallExpr(Token token) : ExprNode(token) {}
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CALL_EXPR; };
    };

    class ImportClauseExpr : public ScopedNode
    {
    public:
        explicit ImportClauseExpr(Token token) : ScopedNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_IMPORT_CLAUSE_EXPR; };
    };

    class GeneratorExpr : public ExprNode
    {
    public:
        explicit GeneratorExpr(Token token) : ExprNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_GENERATOR_EXPR; };
    };

    class CollectionExtractorExpr : public ExprNode
    {
    public:
        explicit CollectionExtractorExpr(Token token) : ExprNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_COLLECTION_EXTRACTOR_EXPR; };
    };

    class SequenceExpr : public ExprNode
    {
    public:
        explicit SequenceExpr(Token token) : ExprNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SEQUENCE_EXPR; };
    };

    class FunctionBody : public AstNode
    {
    public:
        explicit FunctionBody(Token token) : AstNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FUNCTION_BODY; };
    };

    class NameExpr final : public ExprNode
    {
    public:
        const string value;

        explicit NameExpr(Token token, string value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_NAME_EXPR; };
    };

    class IdentifierExpr final : public ValueExpr
    {
    public:
        const NameExpr* const name;

        explicit IdentifierExpr(Token token, NameExpr* name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_IDENTIFIER_EXPR; };
    };

    class RecordNode final : public AstNode
    {
    public:
        const NameExpr* const recordType;
        const vector<const IdentifierExpr* const> identifiers;

        explicit RecordNode(Token token, NameExpr* recordType, const vector<IdentifierExpr* const>& identifiers);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RECORD_NODE; };
    };

    class TrueLiteralExpr final : public LiteralExpr<bool>
    {
    public:
        explicit TrueLiteralExpr(Token token);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_TRUE_LITERAL_EXPR; };
    };

    class FalseLiteralExpr final : public LiteralExpr<bool>
    {
    public:
        explicit FalseLiteralExpr(Token token);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FALSE_LITERAL_EXPR; };
    };

    class FloatExpr final : public LiteralExpr<float>
    {
    public:
        explicit FloatExpr(Token token, float value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FLOAT_EXPR; };
    };

    class IntegerExpr final : public LiteralExpr<int>
    {
    public:
        explicit IntegerExpr(Token token, int value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_INTEGER_EXPR; };
    };

    class ByteExpr final : public LiteralExpr<unsigned char>
    {
    public:
        explicit ByteExpr(Token token, unsigned char value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BYTE_EXPR; };
    };

    class StringExpr final : public LiteralExpr<string>
    {
    public:
        explicit StringExpr(Token token, string value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_STRING_EXPR; };
    };

    class CharacterExpr final : public LiteralExpr<char>
    {
    public:
        explicit CharacterExpr(Token token, char value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CHARACTER_EXPR; };
    };

    class UnitExpr final : public LiteralExpr<nullptr_t>
    {
    public:
        explicit UnitExpr(Token token);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_UNIT_EXPR; };
    };

    class TupleExpr final : public ValueExpr
    {
    public:
        const vector<const ExprNode* const> values;

        explicit TupleExpr(Token token, const vector<const ExprNode* const>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_TUPLE_EXPR; };
    };

    class DictExpr final : public ValueExpr
    {
    public:
        const vector<pair<const ExprNode* const, const ExprNode* const>> values;

        explicit DictExpr(Token token, const vector<pair<ExprNode* const, ExprNode* const>>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DICT_EXPR; };
    };

    class ValuesSequenceExpr final : public SequenceExpr
    {
    public:
        const vector<const ExprNode* const> values;

        explicit ValuesSequenceExpr(Token token, const vector<ExprNode* const>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_VALUES_SEQUENCE_EXPR; };
    };

    class RangeSequenceExpr final : public SequenceExpr
    {
    public:
        // TODO make them optional
        const ExprNode* const start;
        const ExprNode* const end;
        const ExprNode* const step;

        explicit RangeSequenceExpr(Token token, ExprNode* start, ExprNode* end, ExprNode* step);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RANGE_SEQUENCE_EXPR; };
    };

    class SetExpr final : public ValueExpr
    {
    public:
        const vector<const ExprNode* const> values;

        explicit SetExpr(Token token, const vector<ExprNode* const>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SET_EXPR; };
    };

    class SymbolExpr final : public ValueExpr
    {
    public:
        const string value;

        explicit SymbolExpr(Token token, string value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SYMBOL_EXPR; };
    };

    class PackageNameExpr final : public ValueExpr
    {
    public:
        const vector<const NameExpr* const> parts;

        explicit PackageNameExpr(Token token, const vector<NameExpr* const>& parts);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PACKAGE_NAME_EXPR; };
    };

    class FqnExpr final : public ValueExpr
    {
    public:
        const PackageNameExpr* const packageName;
        const NameExpr* const moduleName;

        explicit FqnExpr(Token token, PackageNameExpr* packageName, NameExpr* moduleName);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FQN_EXPR; };
    };

    class FunctionExpr final : public ScopedNode
    {
    public:
        const string name;
        const vector<const PatternNode* const> patterns;
        const vector<const FunctionBody* const> bodies;

        explicit FunctionExpr(Token token, string name, const vector<PatternNode* const>& patterns,
                              const vector<FunctionBody* const>& bodies);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FUNCTION_EXPR; };
    };

    class ModuleExpr final : public ValueExpr
    {
    public:
        const FqnExpr* const fqn;
        const vector<string> exports;
        const vector<const RecordNode* const> records;
        const vector<const FunctionExpr* const> functions;

        explicit ModuleExpr(Token token, FqnExpr* fqn, const vector<string>& exports,
                            const vector<RecordNode* const>& records, const vector<FunctionExpr* const>& functions);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MODULE_EXPR; };
    };

    class RecordInstanceExpr final : public ValueExpr
    {
    public:
        const NameExpr* const recordType;
        const vector<pair<const NameExpr* const, const ExprNode* const>> items;

        explicit RecordInstanceExpr(Token token, NameExpr* recordType,
                                    const vector<pair<NameExpr* const, ExprNode* const>>& items);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RECORD_INSTANCE_EXPR; };
    };

    class BodyWithGuards final : public FunctionBody
    {
    public:
        const ExprNode* const guard;
        const vector<const ExprNode* const> exprs;

        explicit BodyWithGuards(Token token, ExprNode* guard, const vector<ExprNode*>& exprs);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BODY_WITH_GUARDS; };
    };

    class BodyWithoutGuards final : public FunctionBody
    {
    public:
        const ExprNode* const expr;

        explicit BodyWithoutGuards(Token token, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BODY_WITHOUT_GUARDS; };
    };

    class LogicalNotOpExpr final : public OpExpr
    {
    public:
        const ExprNode* const expr;

        explicit LogicalNotOpExpr(Token token, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LOGICAL_NOT_OP_EXPR; };
    };

    class BinaryNotOpExpr final : public OpExpr
    {
    public:
        const ExprNode* const expr;

        explicit BinaryNotOpExpr(Token token, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BINARY_NOT_OP_EXPR; };
    };

    class PowerExpr final : public BinaryOpExpr
    {
    public:
        explicit PowerExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_POWER_EXPR; };
    };

    class MultiplyExpr final : public BinaryOpExpr
    {
    public:
        explicit MultiplyExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MULTIPLY_EXPR; };
    };

    class DivideExpr final : public BinaryOpExpr
    {
    public:
        explicit DivideExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DIVIDE_EXPR; };
    };

    class ModuloExpr final : public BinaryOpExpr
    {
    public:
        explicit ModuloExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MODULO_EXPR; };
    };

    class AddExpr final : public BinaryOpExpr
    {
    public:
        explicit AddExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_ADD_EXPR; };
    };

    class SubtractExpr final : public BinaryOpExpr
    {
    public:
        explicit SubtractExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SUBTRACT_EXPR; };
    };

    class LeftShiftExpr final : public BinaryOpExpr
    {
    public:
        explicit LeftShiftExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LEFT_SHIFT_EXPR; };
    };

    class RightShiftExpr final : public BinaryOpExpr
    {
    public:
        explicit RightShiftExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RIGHT_SHIFT_EXPR; };
    };

    class ZerofillRightShiftExpr final : public BinaryOpExpr
    {
    public:
        explicit ZerofillRightShiftExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_ZEROFILL_RIGHT_SHIFT_EXPR; };
    };

    class GteExpr final : public BinaryOpExpr
    {
    public:
        explicit GteExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_GTE_EXPR; };
    };

    class LteExpr final : public BinaryOpExpr
    {
    public:
        explicit LteExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LTE_EXPR; };
    };

    class GtExpr final : public BinaryOpExpr
    {
    public:
        explicit GtExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_GT_EXPR; };
    };

    class LtExpr final : public BinaryOpExpr
    {
    public:
        explicit LtExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LT_EXPR; };
    };

    class EqExpr final : public BinaryOpExpr
    {
    public:
        explicit EqExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_EQ_EXPR; };
    };

    class NeqExpr final : public BinaryOpExpr
    {
    public:
        explicit NeqExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_NEQ_EXPR; };
    };

    class ConsLeftExpr final : public BinaryOpExpr
    {
    public:
        explicit ConsLeftExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CONS_LEFT_EXPR; };
    };

    class ConsRightExpr final : public BinaryOpExpr
    {
    public:
        explicit ConsRightExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CONS_RIGHT_EXPR; };
    };

    class JoinExpr final : public BinaryOpExpr
    {
    public:
        explicit JoinExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_JOIN_EXPR; };
    };

    class BitwiseAndExpr final : public BinaryOpExpr
    {
    public:
        explicit BitwiseAndExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BITWISE_AND_EXPR; };
    };

    class BitwiseXorExpr final : public BinaryOpExpr
    {
    public:
        explicit BitwiseXorExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BITWISE_XOR_EXPR; };
    };

    class BitwiseOrExpr final : public BinaryOpExpr
    {
    public:
        explicit BitwiseOrExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BITWISE_OR_EXPR; };
    };

    class LogicalAndExpr final : public BinaryOpExpr
    {
    public:
        explicit LogicalAndExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LOGICAL_AND_EXPR; };
    };

    class LogicalOrExpr final : public BinaryOpExpr
    {
    public:
        explicit LogicalOrExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LOGICAL_OR_EXPR; };
    };

    class InExpr final : public BinaryOpExpr
    {
    public:
        explicit InExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_IN_EXPR; };
    };

    class PipeLeftExpr final : public BinaryOpExpr
    {
    public:
        explicit PipeLeftExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PIPE_LEFT_EXPR; };
    };

    class PipeRightExpr final : public BinaryOpExpr
    {
    public:
        explicit PipeRightExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PIPE_RIGHT_EXPR; };
    };

    class LetExpr final : public ScopedNode
    {
    public:
        const vector<const AliasExpr* const> aliases;
        const ExprNode* const expr;

        explicit LetExpr(Token token, const vector<AliasExpr* const>& aliases, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LET_EXPR; };
    };

    class IfExpr final : public ExprNode
    {
    public:
        const ExprNode* const condition;
        const ExprNode* const thenExpr;
        const optional<const ExprNode* const> elseExpr;

        explicit IfExpr(Token token, ExprNode* condition, ExprNode* thenExpr, ExprNode* elseExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_IF_EXPR; };
    };

    class ApplyExpr final : public ExprNode
    {
    public:
        const CallExpr* const call;
        const vector<variant<const ExprNode* const, const ValueExpr* const>> args;

        explicit ApplyExpr(Token token, CallExpr* call, const vector<variant<ExprNode* const, ValueExpr* const>>& args);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_APPLY_EXPR; };
    };

    class DoExpr final : public ExprNode
    {
    public:
        const vector<const ExprNode* const> steps;

        explicit DoExpr(Token token, const vector<ExprNode* const>& steps);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DO_EXPR; };
    };

    class ImportExpr final : public ScopedNode
    {
    public:
        const vector<const ImportClauseExpr* const> clauses;
        const ExprNode* const expr;

        explicit ImportExpr(Token token, const vector<ImportClauseExpr*>& clauses, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_IMPORT_EXPR; };
    };

    class RaiseExpr final : public ExprNode
    {
    public:
        const SymbolExpr* const symbol;
        const LiteralExpr<string>* const message;

        explicit RaiseExpr(Token token, SymbolExpr* symbol, LiteralExpr<string>* message);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RAISE_EXPR; };
    };

    class WithExpr final : public ScopedNode
    {
    public:
        const ExprNode* const contextExpr;
        const optional<const NameExpr* const> name;
        const ExprNode* const bodyExpr;

        explicit WithExpr(Token token, ExprNode* contextExpr, NameExpr* name, ExprNode* bodyExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_WITH_EXPR; };
    };

    class FieldAccessExpr final : public ExprNode
    {
    public:
        const IdentifierExpr* const identifier;
        const NameExpr* const name;

        explicit FieldAccessExpr(Token token, IdentifierExpr* identifier, NameExpr* name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FIELD_ACCESS_EXPR; };
    };

    class FieldUpdateExpr final : public ExprNode
    {
    public:
        const IdentifierExpr* const identifier;
        const vector<pair<const NameExpr* const, const ExprNode* const>> updates;

        explicit FieldUpdateExpr(Token token, IdentifierExpr* identifier,
                                 const vector<pair<NameExpr* const, ExprNode* const>>& updates);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FIELD_UPDATE_EXPR; };
    };

    class LambdaAlias final : public AliasExpr
    {
    public:
        const NameExpr* const name;
        const FunctionExpr* const lambda;

        explicit LambdaAlias(Token token, NameExpr* name, FunctionExpr* lambda);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LAMBDA_ALIAS; };
    };

    class ModuleAlias final : public AliasExpr
    {
    public:
        const NameExpr* const name;
        const ModuleExpr* const module;

        explicit ModuleAlias(Token token, NameExpr* name, ModuleExpr* module);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MODULE_ALIAS; };
    };

    class ValueAlias final : public AliasExpr
    {
    public:
        const IdentifierExpr* const identifier;
        const ExprNode* const expr;

        explicit ValueAlias(Token token, IdentifierExpr* identifier, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_VALUE_ALIAS; };
    };

    class PatternAlias final : public AliasExpr
    {
    public:
        const PatternNode pattern;
        const ExprNode* const expr;

        explicit PatternAlias(Token token, PatternNode* pattern, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_ALIAS; };
    };

    class FqnAlias final : public AliasExpr
    {
    public:
        const NameExpr* const name;
        const FqnExpr* const fqn;

        explicit FqnAlias(Token token, NameExpr* name, FqnExpr* fqn);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FQN_ALIAS; };
    };

    class FunctionAlias final : public AliasExpr
    {
    public:
        const NameExpr* const name;
        const NameExpr* const alias;

        explicit FunctionAlias(Token token, NameExpr* name, NameExpr* alias);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FUNCTION_ALIAS; };
    };

    class AliasCall final : public CallExpr
    {
    public:
        const NameExpr* const alias;
        const NameExpr* const funName;

        explicit AliasCall(Token token, NameExpr* alias, NameExpr* funName);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_ALIAS_CALL; };
    };

    class NameCall final : public CallExpr
    {
    public:
        const NameExpr* const name;

        explicit NameCall(Token token, NameExpr* name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_NAME_CALL; };
    };

    class ModuleCall final : public CallExpr
    {
    public:
        const variant<const FqnExpr* const, const ExprNode* const> fqn;
        const NameExpr* const funName;

        explicit ModuleCall(Token token, const variant<FqnExpr* const, ExprNode* const>& fqn, const NameExpr* funName);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MODULE_CALL; };
    };

    class ModuleImport final : public ImportClauseExpr
    {
    public:
        const FqnExpr* const fqn;
        const NameExpr* const name;

        explicit ModuleImport(Token token, FqnExpr* fqn, NameExpr* name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MODULE_IMPORT; };
    };

    class FunctionsImport final : public ImportClauseExpr
    {
    public:
        const vector<const FunctionAlias* const> aliases;
        const FqnExpr* const fromFqn;

        explicit FunctionsImport(Token token, const vector<FunctionAlias* const>& aliases, FqnExpr* fromFqn);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FUNCTIONS_IMPORT; };
    };

    class SeqGeneratorExpr final : public GeneratorExpr
    {
    public:
        const ExprNode* const reducerExpr;
        const CollectionExtractorExpr* const collectionExtractor;
        const ExprNode* const stepExpression;

        explicit SeqGeneratorExpr(Token token, ExprNode* reducerExpr, CollectionExtractorExpr* collectionExtractor,
                                  ExprNode* stepExpression);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SEQ_GENERATOR_EXPR; };
    };

    class SetGeneratorExpr final : public GeneratorExpr
    {
    public:
        const ExprNode* const reducerExpr;
        const CollectionExtractorExpr* const collectionExtractor;
        const ExprNode* const stepExpression;

        explicit SetGeneratorExpr(Token token, ExprNode* reducerExpr, CollectionExtractorExpr* collectionExtractor,
                                  ExprNode* stepExpression);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SET_GENERATOR_EXPR; };
    };

    class DictGeneratorReducer final : public ExprNode
    {
    public:
        const ExprNode* const key;
        const ExprNode* const value;

        explicit DictGeneratorReducer(Token token, ExprNode* key, ExprNode* value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DICT_GENERATOR_REDUCER; };
    };

    class DictGeneratorExpr final : public GeneratorExpr
    {
    public:
        const DictGeneratorReducer* const reducerExpr;
        const CollectionExtractorExpr* const collectionExtractor;
        const ExprNode* const stepExpression;

        explicit DictGeneratorExpr(Token token, DictGeneratorReducer* reducerExpr,
                                   CollectionExtractorExpr* collectionExtractor, ExprNode* stepExpression);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DICT_GENERATOR_EXPR; };
    };

    class UnderscorePattern final : public PatternNode
    {
    public:
        UnderscorePattern(Token token) : PatternNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_UNDERSCORE_PATTERN; };
    };

    using IdentifierOrUnderscore = variant<IdentifierExpr* const, UnderscoreNode* const>;

    class ValueCollectionExtractorExpr final : public CollectionExtractorExpr
    {
    public:
        const IdentifierOrUnderscore expr;

        explicit ValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_VALUE_COLLECTION_EXTRACTOR_EXPR; };
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
        [[nodiscard]] AstNodeType get_type() const override { return AST_KEY_VALUE_COLLECTION_EXTRACTOR_EXPR; };
    };

    using PatternWithoutSequence =
        PatternNode* const; // variant<UnderscorePattern, PatternValue, TuplePattern, DictPattern>;
    using SequencePattern =
        PatternNode* const; // variant<SeqPattern, HeadTailsPattern, TailsHeadPattern, HeadTailsHeadPattern>;
    using DataStructurePattern =
        PatternNode* const; // variant<TuplePattern, SequencePattern, DictPattern, RecordPattern>;
    using Pattern = PatternNode* const; // variant<UnderscorePattern, PatternValue, DataStructurePattern,
                                        // AsDataStructurePattern>;
    using TailPattern = PatternNode* const; // variant<IdentifierExpr, SequenceExpr, UnderscoreNode, StringExpr>;

    class PatternWithGuards final : public PatternNode
    {
    public:
        const ExprNode* const guard;
        const ExprNode* const expr;

        explicit PatternWithGuards(Token token, ExprNode* guard, const ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_WITH_GUARDS; };
    };

    class PatternWithoutGuards final : public PatternNode
    {
    public:
        const ExprNode* const expr;

        explicit PatternWithoutGuards(Token token, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_WITHOUT_GUARDS; };
    };

    class PatternExpr : public ExprNode
    {
    public:
        const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>> patternExpr;

        explicit PatternExpr(Token token,
                             const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>>& patternExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_EXPR; };
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
        [[nodiscard]] AstNodeType get_type() const override { return AST_CATCH_PATTERN_EXPR; };
    };

    class CatchExpr final : public ExprNode
    {
    public:
        const vector<CatchPatternExpr> patterns;

        explicit CatchExpr(Token token, const vector<CatchPatternExpr>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CATCH_EXPR; };
    };

    class TryCatchExpr final : public ExprNode
    {
    public:
        const ExprNode* const tryExpr;
        const CatchExpr* const catchExpr;

        explicit TryCatchExpr(Token token, ExprNode* tryExpr, CatchExpr* catchExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_TRY_CATCH_EXPR; };
    };

    class PatternValue final : public PatternNode
    {
    public:
        const variant<const LiteralExpr<nullptr_t>* const, const LiteralExpr<void*>* const, const SymbolExpr* const,
                      const IdentifierExpr* const>
            expr;

        explicit PatternValue(Token token,
                              const variant<LiteralExpr<nullptr_t>* const, LiteralExpr<void*>* const, SymbolExpr* const,
                                            IdentifierExpr* const>& expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_VALUE; };
    };

    class AsDataStructurePattern final : public PatternNode
    {
    public:
        const IdentifierExpr* const identifier;
        const DataStructurePattern pattern;

        explicit AsDataStructurePattern(Token token, IdentifierExpr* identifier, DataStructurePattern pattern);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_AS_DATA_STRUCTURE_PATTERN; };
    };

    class TuplePattern final : public PatternNode
    {
    public:
        const vector<Pattern> patterns;

        explicit TuplePattern(Token token, const vector<Pattern>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_TUPLE_PATTERN; };
    };

    class SeqPattern final : public PatternNode
    {
    public:
        const vector<Pattern> patterns;

        explicit SeqPattern(Token token, const vector<Pattern>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SEQ_PATTERN; };
    };

    class HeadTailsPattern final : public PatternNode
    {
    public:
        const vector<PatternWithoutSequence> heads;
        const TailPattern tail;

        explicit HeadTailsPattern(Token token, const vector<PatternWithoutSequence>& heads, TailPattern tail);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_HEAD_TAILS_PATTERN; };
    };

    class TailsHeadPattern final : public PatternNode
    {
    public:
        const TailPattern tail;
        const vector<PatternWithoutSequence> heads;

        explicit TailsHeadPattern(Token token, TailPattern tail, const vector<PatternWithoutSequence>& heads);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_TAILS_HEAD_PATTERN; };
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
        [[nodiscard]] AstNodeType get_type() const override { return AST_HEAD_TAILS_HEAD_PATTERN; };
    };

    class DictPattern final : public PatternNode
    {
    public:
        const vector<pair<const PatternValue* const, Pattern>> keyValuePairs;

        explicit DictPattern(Token token, const vector<pair<PatternValue* const, Pattern>>& keyValuePairs);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DICT_PATTERN; };
    };

    class RecordPattern final : public PatternNode
    {
    public:
        const string recordType;
        const vector<pair<const NameExpr* const, Pattern>> items;

        explicit RecordPattern(Token token, string recordType, const vector<pair<NameExpr* const, Pattern>>& items);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RECORD_PATTERN; };
    };

    class CaseExpr final : public ExprNode
    {
    public:
        const ExprNode* const expr;
        const vector<const PatternExpr* const> patterns;

        explicit CaseExpr(Token token, ExprNode* expr, const vector<PatternExpr>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(TypeInferenceContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CASE_EXPR; };
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
        virtual any visit(const ExprNode* const& node) const;
        virtual any visit(const AstNode& node) const;
        virtual any visit(const ScopedNode& node) const;
        virtual any visit(const PatternNode& node) const;
    };
}
