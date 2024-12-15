#pragma once

#include <antlr4-runtime.h>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "common.h"
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

    struct expr_wrapper
    {
    private:
        AstNode* node;

    public:
        explicit expr_wrapper(AstNode* node) : node(node) {}

        template <typename T>
            requires derived_from<T, AstNode>
        T* get_node()
        {
            return static_cast<T*>(node);
        }
    };

    class AstNode
    {
    public:
        Token token;
        AstNode* parent = nullptr;
        explicit AstNode(Token token) : token(token) {};
        virtual ~AstNode() = default;
        virtual any accept(const AstVisitor& visitor);
        [[nodiscard]] virtual Type infer_type(AstContext& ctx) const;
        [[nodiscard]] virtual AstNodeType get_type() const { return AST_NODE; };

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
    vector<T*> nodes_with_parent(vector<T*> nodes, AstNode* parent)
    {
        for (auto node : nodes)
        {
            node->parent = parent;
        }
        return nodes;
    }

    template <typename T1, typename T2>
        requires derived_from<T1, AstNode> && derived_from<T2, AstNode>
    vector<pair<T1*, T2*>> nodes_with_parent(vector<pair<T1*, T2*>> nodes, AstNode* parent)
    {
        for (auto node : nodes)
        {
            node.first->parent = parent;
            node.second->parent = parent;
        }
        return nodes;
    }

    template <typename... Types>
        requires(derived_from<Types, AstNode> && ...)
    variant<Types*...> node_with_parent(variant<Types*...> node, AstNode* parent)
    {
        visit([parent](auto arg) { arg->parent = parent; }, node);
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
    vector<variant<Types*...>> nodes_with_parent(vector<variant<Types*...>> nodes, AstNode* parent)
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
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
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
        ExprNode* left;
        ExprNode* right;

        explicit BinaryOpExpr(Token token, ExprNode* left, ExprNode* right);
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BINARY_OP_EXPR; };
        ~BinaryOpExpr() override;
    };

    class AliasExpr : public ExprNode
    {
    public:
        explicit AliasExpr(Token token) : ExprNode(token) {}
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_ALIAS_EXPR; };
    };

    class CallExpr : public ExprNode
    {
    public:
        explicit CallExpr(Token token) : ExprNode(token) {}
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
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
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_NAME_EXPR; };
    };

    class IdentifierExpr final : public ValueExpr
    {
    public:
        NameExpr* name;

        explicit IdentifierExpr(Token token, NameExpr* name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_IDENTIFIER_EXPR; };
        ~IdentifierExpr() override;
    };

    class RecordNode final : public AstNode
    {
    public:
        NameExpr* recordType;
        vector<IdentifierExpr*> identifiers;

        explicit RecordNode(Token token, NameExpr* recordType, const vector<IdentifierExpr*>& identifiers);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RECORD_NODE; };
        ~RecordNode() override;
    };

    class TrueLiteralExpr final : public LiteralExpr<bool>
    {
    public:
        explicit TrueLiteralExpr(Token token);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_TRUE_LITERAL_EXPR; };
    };

    class FalseLiteralExpr final : public LiteralExpr<bool>
    {
    public:
        explicit FalseLiteralExpr(Token token);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FALSE_LITERAL_EXPR; };
    };

    class FloatExpr final : public LiteralExpr<float>
    {
    public:
        explicit FloatExpr(Token token, float value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FLOAT_EXPR; };
    };

    class IntegerExpr final : public LiteralExpr<int>
    {
    public:
        explicit IntegerExpr(Token token, int value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_INTEGER_EXPR; };
    };

    class ByteExpr final : public LiteralExpr<unsigned char>
    {
    public:
        explicit ByteExpr(Token token, unsigned char value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BYTE_EXPR; };
    };

    class StringExpr final : public LiteralExpr<string>
    {
    public:
        explicit StringExpr(Token token, string value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_STRING_EXPR; };
    };

    class CharacterExpr final : public LiteralExpr<char>
    {
    public:
        explicit CharacterExpr(Token token, char value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CHARACTER_EXPR; };
    };

    class UnitExpr final : public LiteralExpr<nullptr_t>
    {
    public:
        explicit UnitExpr(Token token);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_UNIT_EXPR; };
    };

    class TupleExpr final : public ValueExpr
    {
    public:
        vector<ExprNode*> values;

        explicit TupleExpr(Token token, const vector<ExprNode*>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_TUPLE_EXPR; };
        ~TupleExpr() override;
    };

    class DictExpr final : public ValueExpr
    {
    public:
        vector<pair<ExprNode*, ExprNode*>> values;

        explicit DictExpr(Token token, const vector<pair<ExprNode*, ExprNode*>>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DICT_EXPR; };
        ~DictExpr() override;
    };

    class ValuesSequenceExpr final : public SequenceExpr
    {
    public:
        vector<ExprNode*> values;

        explicit ValuesSequenceExpr(Token token, const vector<ExprNode*>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_VALUES_SEQUENCE_EXPR; };
        ~ValuesSequenceExpr() override;
    };

    class RangeSequenceExpr final : public SequenceExpr
    {
    public:
        // TODO make them optional
        ExprNode* start;
        ExprNode* end;
        ExprNode* step;

        explicit RangeSequenceExpr(Token token, ExprNode* start, ExprNode* end, ExprNode* step);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RANGE_SEQUENCE_EXPR; };
        ~RangeSequenceExpr() override;
    };

    class SetExpr final : public ValueExpr
    {
    public:
        vector<ExprNode*> values;

        explicit SetExpr(Token token, const vector<ExprNode*>& values);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SET_EXPR; };
        ~SetExpr() override;
    };

    class SymbolExpr final : public ValueExpr
    {
    public:
        string value;

        explicit SymbolExpr(Token token, string value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SYMBOL_EXPR; };
    };

    class PackageNameExpr final : public ValueExpr
    {
    public:
        vector<NameExpr*> parts;

        explicit PackageNameExpr(Token token, const vector<NameExpr*>& parts);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PACKAGE_NAME_EXPR; };
        ~PackageNameExpr() override;
    };

    class FqnExpr final : public ValueExpr
    {
    public:
        PackageNameExpr* packageName;
        NameExpr* moduleName;

        explicit FqnExpr(Token token, PackageNameExpr* packageName, NameExpr* moduleName);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FQN_EXPR; };
        ~FqnExpr() override;
    };

    class FunctionExpr final : public ScopedNode
    {
    public:
        const string name;
        vector<PatternNode*> patterns;
        vector<FunctionBody*> bodies;

        explicit FunctionExpr(Token token, string name, const vector<PatternNode*>& patterns,
                              const vector<FunctionBody*>& bodies);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FUNCTION_EXPR; };
        ~FunctionExpr() override;
    };

    class ModuleExpr final : public ValueExpr
    {
    public:
        FqnExpr* fqn;
        vector<string> exports;
        vector<RecordNode*> records;
        vector<FunctionExpr*> functions;

        explicit ModuleExpr(Token token, FqnExpr* fqn, const vector<string>& exports,
                            const vector<RecordNode*>& records, const vector<FunctionExpr*>& functions);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MODULE_EXPR; };
        ~ModuleExpr() override;
    };

    class RecordInstanceExpr final : public ValueExpr
    {
    public:
        NameExpr* recordType;
        vector<pair<NameExpr*, ExprNode*>> items;

        explicit RecordInstanceExpr(Token token, NameExpr* recordType, const vector<pair<NameExpr*, ExprNode*>>& items);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RECORD_INSTANCE_EXPR; };
        ~RecordInstanceExpr() override;
    };

    class BodyWithGuards final : public FunctionBody
    {
    public:
        ExprNode* guard;
        vector<ExprNode*> exprs;

        explicit BodyWithGuards(Token token, ExprNode* guard, const vector<ExprNode*>& exprs);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BODY_WITH_GUARDS; };
        ~BodyWithGuards() override;
    };

    class BodyWithoutGuards final : public FunctionBody
    {
    public:
        ExprNode* expr;

        explicit BodyWithoutGuards(Token token, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BODY_WITHOUT_GUARDS; };
        ~BodyWithoutGuards() override;
    };

    class LogicalNotOpExpr final : public OpExpr
    {
    public:
        ExprNode* expr;

        explicit LogicalNotOpExpr(Token token, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LOGICAL_NOT_OP_EXPR; };
        ~LogicalNotOpExpr() override;
    };

    class BinaryNotOpExpr final : public OpExpr
    {
    public:
        ExprNode* expr;

        explicit BinaryNotOpExpr(Token token, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BINARY_NOT_OP_EXPR; };
        ~BinaryNotOpExpr() override;
    };

    class PowerExpr final : public BinaryOpExpr
    {
    public:
        explicit PowerExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_POWER_EXPR; };
    };

    class MultiplyExpr final : public BinaryOpExpr
    {
    public:
        explicit MultiplyExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MULTIPLY_EXPR; };
    };

    class DivideExpr final : public BinaryOpExpr
    {
    public:
        explicit DivideExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DIVIDE_EXPR; };
    };

    class ModuloExpr final : public BinaryOpExpr
    {
    public:
        explicit ModuloExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MODULO_EXPR; };
    };

    class AddExpr final : public BinaryOpExpr
    {
    public:
        explicit AddExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_ADD_EXPR; };
    };

    class SubtractExpr final : public BinaryOpExpr
    {
    public:
        explicit SubtractExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SUBTRACT_EXPR; };
    };

    class LeftShiftExpr final : public BinaryOpExpr
    {
    public:
        explicit LeftShiftExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LEFT_SHIFT_EXPR; };
    };

    class RightShiftExpr final : public BinaryOpExpr
    {
    public:
        explicit RightShiftExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RIGHT_SHIFT_EXPR; };
    };

    class ZerofillRightShiftExpr final : public BinaryOpExpr
    {
    public:
        explicit ZerofillRightShiftExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_ZEROFILL_RIGHT_SHIFT_EXPR; };
    };

    class GteExpr final : public BinaryOpExpr
    {
    public:
        explicit GteExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_GTE_EXPR; };
    };

    class LteExpr final : public BinaryOpExpr
    {
    public:
        explicit LteExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LTE_EXPR; };
    };

    class GtExpr final : public BinaryOpExpr
    {
    public:
        explicit GtExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_GT_EXPR; };
    };

    class LtExpr final : public BinaryOpExpr
    {
    public:
        explicit LtExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LT_EXPR; };
    };

    class EqExpr final : public BinaryOpExpr
    {
    public:
        explicit EqExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_EQ_EXPR; };
    };

    class NeqExpr final : public BinaryOpExpr
    {
    public:
        explicit NeqExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_NEQ_EXPR; };
    };

    class ConsLeftExpr final : public BinaryOpExpr
    {
    public:
        explicit ConsLeftExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CONS_LEFT_EXPR; };
    };

    class ConsRightExpr final : public BinaryOpExpr
    {
    public:
        explicit ConsRightExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CONS_RIGHT_EXPR; };
    };

    class JoinExpr final : public BinaryOpExpr
    {
    public:
        explicit JoinExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_JOIN_EXPR; };
    };

    class BitwiseAndExpr final : public BinaryOpExpr
    {
    public:
        explicit BitwiseAndExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BITWISE_AND_EXPR; };
    };

    class BitwiseXorExpr final : public BinaryOpExpr
    {
    public:
        explicit BitwiseXorExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BITWISE_XOR_EXPR; };
    };

    class BitwiseOrExpr final : public BinaryOpExpr
    {
    public:
        explicit BitwiseOrExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_BITWISE_OR_EXPR; };
    };

    class LogicalAndExpr final : public BinaryOpExpr
    {
    public:
        explicit LogicalAndExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LOGICAL_AND_EXPR; };
    };

    class LogicalOrExpr final : public BinaryOpExpr
    {
    public:
        explicit LogicalOrExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LOGICAL_OR_EXPR; };
    };

    class InExpr final : public BinaryOpExpr
    {
    public:
        explicit InExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_IN_EXPR; };
    };

    class PipeLeftExpr final : public BinaryOpExpr
    {
    public:
        explicit PipeLeftExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PIPE_LEFT_EXPR; };
    };

    class PipeRightExpr final : public BinaryOpExpr
    {
    public:
        explicit PipeRightExpr(Token token, ExprNode* left, ExprNode* right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PIPE_RIGHT_EXPR; };
    };

    class LetExpr final : public ScopedNode
    {
    public:
        vector<AliasExpr*> aliases;
        ExprNode* expr;

        explicit LetExpr(Token token, const vector<AliasExpr*>& aliases, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LET_EXPR; };
        ~LetExpr() override;
    };

    class IfExpr final : public ExprNode
    {
    public:
        ExprNode* condition;
        ExprNode* thenExpr;
        ExprNode* elseExpr;

        explicit IfExpr(Token token, ExprNode* condition, ExprNode* thenExpr, ExprNode* elseExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_IF_EXPR; };
        ~IfExpr() override;
    };

    class ApplyExpr final : public ExprNode
    {
    public:
        CallExpr* call;
        vector<variant<ExprNode*, ValueExpr*>> args;

        explicit ApplyExpr(Token token, CallExpr* call, const vector<variant<ExprNode*, ValueExpr*>>& args);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_APPLY_EXPR; };
        ~ApplyExpr() override;
    };

    class DoExpr final : public ExprNode
    {
    public:
        vector<ExprNode*> steps;

        explicit DoExpr(Token token, const vector<ExprNode*>& steps);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DO_EXPR; };
        ~DoExpr() override;
    };

    class ImportExpr final : public ScopedNode
    {
    public:
        vector<ImportClauseExpr*> clauses;
        ExprNode* expr;

        explicit ImportExpr(Token token, const vector<ImportClauseExpr*>& clauses, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_IMPORT_EXPR; };
        ~ImportExpr() override;
    };

    class RaiseExpr final : public ExprNode
    {
    public:
        SymbolExpr* symbol;
        StringExpr* message;

        explicit RaiseExpr(Token token, SymbolExpr* symbol, StringExpr* message);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RAISE_EXPR; };
        ~RaiseExpr() override;
    };

    class WithExpr final : public ScopedNode
    {
    public:
        ExprNode* contextExpr;
        NameExpr* name;
        ExprNode* bodyExpr;

        explicit WithExpr(Token token, ExprNode* contextExpr, NameExpr* name, ExprNode* bodyExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_WITH_EXPR; };
        ~WithExpr() override;
    };

    class FieldAccessExpr final : public ExprNode
    {
    public:
        IdentifierExpr* identifier;
        NameExpr* name;

        explicit FieldAccessExpr(Token token, IdentifierExpr* identifier, NameExpr* name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FIELD_ACCESS_EXPR; };
        ~FieldAccessExpr() override;
    };

    class FieldUpdateExpr final : public ExprNode
    {
    public:
        IdentifierExpr* identifier;
        vector<pair<NameExpr*, ExprNode*>> updates;

        explicit FieldUpdateExpr(Token token, IdentifierExpr* identifier,
                                 const vector<pair<NameExpr*, ExprNode*>>& updates);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FIELD_UPDATE_EXPR; };
        ~FieldUpdateExpr() override;
    };

    class LambdaAlias final : public AliasExpr
    {
    public:
        NameExpr* name;
        FunctionExpr* lambda;

        explicit LambdaAlias(Token token, NameExpr* name, FunctionExpr* lambda);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_LAMBDA_ALIAS; };
        ~LambdaAlias() override;
    };

    class ModuleAlias final : public AliasExpr
    {
    public:
        NameExpr* name;
        ModuleExpr* module;

        explicit ModuleAlias(Token token, NameExpr* name, ModuleExpr* module);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MODULE_ALIAS; };
        ~ModuleAlias() override;
    };

    class ValueAlias final : public AliasExpr
    {
    public:
        IdentifierExpr* identifier;
        ExprNode* expr;

        explicit ValueAlias(Token token, IdentifierExpr* identifier, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_VALUE_ALIAS; };
        ~ValueAlias() override;
    };

    class PatternAlias final : public AliasExpr
    {
    public:
        PatternNode* pattern;
        ExprNode* expr;

        explicit PatternAlias(Token token, PatternNode* pattern, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_ALIAS; };
        ~PatternAlias() override;
    };

    class FqnAlias final : public AliasExpr
    {
    public:
        NameExpr* name;
        FqnExpr* fqn;

        explicit FqnAlias(Token token, NameExpr* name, FqnExpr* fqn);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FQN_ALIAS; };
        ~FqnAlias() override;
    };

    class FunctionAlias final : public AliasExpr
    {
    public:
        NameExpr* name;
        NameExpr* alias;

        explicit FunctionAlias(Token token, NameExpr* name, NameExpr* alias);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FUNCTION_ALIAS; };
        ~FunctionAlias() override;
    };

    class AliasCall final : public CallExpr
    {
    public:
        NameExpr* alias;
        NameExpr* funName;

        explicit AliasCall(Token token, NameExpr* alias, NameExpr* funName);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_ALIAS_CALL; };
        ~AliasCall() override;
    };

    class NameCall final : public CallExpr
    {
    public:
        NameExpr* name;

        explicit NameCall(Token token, NameExpr* name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_NAME_CALL; };
        ~NameCall() override;
    };

    class ModuleCall final : public CallExpr
    {
    public:
        variant<FqnExpr*, ExprNode*> fqn;
        NameExpr* funName;

        explicit ModuleCall(Token token, const variant<FqnExpr*, ExprNode*>& fqn, NameExpr* funName);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MODULE_CALL; };
        ~ModuleCall() override;
    };

    class ModuleImport final : public ImportClauseExpr
    {
    public:
        FqnExpr* fqn;
        NameExpr* name;

        explicit ModuleImport(Token token, FqnExpr* fqn, NameExpr* name);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_MODULE_IMPORT; };
        ~ModuleImport() override;
    };

    class FunctionsImport final : public ImportClauseExpr
    {
    public:
        vector<FunctionAlias*> aliases;
        FqnExpr* fromFqn;

        explicit FunctionsImport(Token token, const vector<FunctionAlias*>& aliases, FqnExpr* fromFqn);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_FUNCTIONS_IMPORT; };
        ~FunctionsImport() override;
    };

    class SeqGeneratorExpr final : public GeneratorExpr
    {
    public:
        ExprNode* reducerExpr;
        CollectionExtractorExpr* collectionExtractor;
        ExprNode* stepExpression;

        explicit SeqGeneratorExpr(Token token, ExprNode* reducerExpr, CollectionExtractorExpr* collectionExtractor,
                                  ExprNode* stepExpression);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SEQ_GENERATOR_EXPR; };
        ~SeqGeneratorExpr() override;
    };

    class SetGeneratorExpr final : public GeneratorExpr
    {
    public:
        ExprNode* reducerExpr;
        CollectionExtractorExpr* collectionExtractor;
        ExprNode* stepExpression;

        explicit SetGeneratorExpr(Token token, ExprNode* reducerExpr, CollectionExtractorExpr* collectionExtractor,
                                  ExprNode* stepExpression);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SET_GENERATOR_EXPR; };
        ~SetGeneratorExpr() override;
    };

    class DictGeneratorReducer final : public ExprNode
    {
    public:
        ExprNode* key;
        ExprNode* value;

        explicit DictGeneratorReducer(Token token, ExprNode* key, ExprNode* value);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DICT_GENERATOR_REDUCER; };
        ~DictGeneratorReducer() override;
    };

    class DictGeneratorExpr final : public GeneratorExpr
    {
    public:
        DictGeneratorReducer* reducerExpr;
        CollectionExtractorExpr* collectionExtractor;
        ExprNode* stepExpression;

        explicit DictGeneratorExpr(Token token, DictGeneratorReducer* reducerExpr,
                                   CollectionExtractorExpr* collectionExtractor, ExprNode* stepExpression);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DICT_GENERATOR_EXPR; };
        ~DictGeneratorExpr() override;
    };

    class UnderscorePattern final : public PatternNode
    {
    public:
        UnderscorePattern(Token token) : PatternNode(token) {}
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_UNDERSCORE_PATTERN; };
    };

    using IdentifierOrUnderscore = variant<IdentifierExpr*, UnderscoreNode*>;

    class ValueCollectionExtractorExpr final : public CollectionExtractorExpr
    {
    public:
        IdentifierOrUnderscore expr;

        explicit ValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_VALUE_COLLECTION_EXTRACTOR_EXPR; };
        ~ValueCollectionExtractorExpr() override;
    };

    class KeyValueCollectionExtractorExpr final : public CollectionExtractorExpr
    {
    public:
        IdentifierOrUnderscore keyExpr;
        IdentifierOrUnderscore valueExpr;

        explicit KeyValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore keyExpr,
                                                 IdentifierOrUnderscore valueExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_KEY_VALUE_COLLECTION_EXTRACTOR_EXPR; };
        ~KeyValueCollectionExtractorExpr() override;
    };

    using PatternWithoutSequence = PatternNode; // variant<UnderscorePattern, PatternValue, TuplePattern, DictPattern>;
    using SequencePattern =
        PatternNode; // variant<SeqPattern, HeadTailsPattern, TailsHeadPattern, HeadTailsHeadPattern>;
    using DataStructurePattern = PatternNode; // variant<TuplePattern, SequencePattern, DictPattern, RecordPattern>;
    using Pattern = PatternNode; // variant<UnderscorePattern, PatternValue, DataStructurePattern,
                                 // AsDataStructurePattern>;
    using TailPattern = PatternNode; // variant<IdentifierExpr, SequenceExpr, UnderscoreNode, StringExpr>;

    class PatternWithGuards final : public PatternNode
    {
    public:
        ExprNode* guard;
        ExprNode* expr;

        explicit PatternWithGuards(Token token, ExprNode* guard, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_WITH_GUARDS; };
        ~PatternWithGuards() override;
    };

    class PatternWithoutGuards final : public PatternNode
    {
    public:
        ExprNode* expr;

        explicit PatternWithoutGuards(Token token, ExprNode* expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_WITHOUT_GUARDS; };
        ~PatternWithoutGuards() override;
    };

    class PatternExpr : public ExprNode
    {
    public:
        variant<Pattern*, PatternWithoutGuards*, vector<PatternWithGuards*>> patternExpr;

        explicit PatternExpr(Token token,
                             const variant<Pattern*, PatternWithoutGuards*, vector<PatternWithGuards*>>& patternExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_EXPR; };
        ~PatternExpr() override;
    };

    class CatchPatternExpr final : public ExprNode
    {
    public:
        Pattern* matchPattern;
        variant<PatternWithoutGuards*, vector<PatternWithGuards*>> pattern;

        explicit CatchPatternExpr(Token token, Pattern* matchPattern,
                                  const variant<PatternWithoutGuards*, vector<PatternWithGuards*>>& pattern);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CATCH_PATTERN_EXPR; };
        ~CatchPatternExpr() override;
    };

    class CatchExpr final : public ExprNode
    {
    public:
        vector<CatchPatternExpr*> patterns;

        explicit CatchExpr(Token token, const vector<CatchPatternExpr*>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CATCH_EXPR; };
        ~CatchExpr() override;
    };

    class TryCatchExpr final : public ExprNode
    {
    public:
        ExprNode* tryExpr;
        CatchExpr* catchExpr;

        explicit TryCatchExpr(Token token, ExprNode* tryExpr, CatchExpr* catchExpr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_TRY_CATCH_EXPR; };
        ~TryCatchExpr() override;
    };

    class PatternValue final : public PatternNode
    {
    public:
        variant<LiteralExpr<nullptr_t>*, LiteralExpr<void*>*, SymbolExpr*, IdentifierExpr*> expr;

        explicit PatternValue(
            Token token,
            const variant<LiteralExpr<nullptr_t>*, LiteralExpr<void*>*, SymbolExpr*, IdentifierExpr*>& expr);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_PATTERN_VALUE; };
        ~PatternValue() override;
    };

    class AsDataStructurePattern final : public PatternNode
    {
    public:
        IdentifierExpr* identifier;
        DataStructurePattern* pattern;

        explicit AsDataStructurePattern(Token token, IdentifierExpr* identifier, DataStructurePattern* pattern);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_AS_DATA_STRUCTURE_PATTERN; };
        ~AsDataStructurePattern() override;
    };

    class TuplePattern final : public PatternNode
    {
    public:
        vector<Pattern*> patterns;

        explicit TuplePattern(Token token, const vector<Pattern*>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_TUPLE_PATTERN; };
        ~TuplePattern() override;
    };

    class SeqPattern final : public PatternNode
    {
    public:
        vector<Pattern*> patterns;

        explicit SeqPattern(Token token, const vector<Pattern*>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_SEQ_PATTERN; };
        ~SeqPattern() override;
    };

    class HeadTailsPattern final : public PatternNode
    {
    public:
        vector<PatternWithoutSequence*> heads;
        TailPattern* tail;

        explicit HeadTailsPattern(Token token, const vector<PatternWithoutSequence*>& heads, TailPattern* tail);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_HEAD_TAILS_PATTERN; };
        ~HeadTailsPattern() override;
    };

    class TailsHeadPattern final : public PatternNode
    {
    public:
        TailPattern* tail;
        vector<PatternWithoutSequence*> heads;

        explicit TailsHeadPattern(Token token, TailPattern* tail, const vector<PatternWithoutSequence*>& heads);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_TAILS_HEAD_PATTERN; };
        ~TailsHeadPattern() override;
    };

    class HeadTailsHeadPattern final : public PatternNode
    {
    public:
        vector<PatternWithoutSequence*> left;
        TailPattern* tail;
        vector<PatternWithoutSequence*> right;

        explicit HeadTailsHeadPattern(Token token, const vector<PatternWithoutSequence*>& left, TailPattern* tail,
                                      const vector<PatternWithoutSequence*>& right);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_HEAD_TAILS_HEAD_PATTERN; };
        ~HeadTailsHeadPattern() override;
    };

    class DictPattern final : public PatternNode
    {
    public:
        vector<pair<PatternValue*, Pattern*>> keyValuePairs;

        explicit DictPattern(Token token, const vector<pair<PatternValue*, Pattern*>>& keyValuePairs);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_DICT_PATTERN; };
        ~DictPattern() override;
    };

    class RecordPattern final : public PatternNode
    {
    public:
        const string recordType;
        vector<pair<NameExpr*, Pattern*>> items;

        explicit RecordPattern(Token token, string recordType, const vector<pair<NameExpr*, Pattern*>>& items);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_RECORD_PATTERN; };
        ~RecordPattern() override;
    };

    class CaseExpr final : public ExprNode
    {
    public:
        ExprNode* expr;
        vector<PatternExpr*> patterns;

        explicit CaseExpr(Token token, ExprNode* expr, const vector<PatternExpr*>& patterns);
        any accept(const AstVisitor& visitor) override;
        [[nodiscard]] Type infer_type(AstContext& ctx) const override;
        [[nodiscard]] AstNodeType get_type() const override { return AST_CASE_EXPR; };
        ~CaseExpr() override;
    };

    class AstVisitor
    {
    public:
        virtual ~AstVisitor() = default;
        virtual any visit(AddExpr* node) const = 0;
        virtual any visit(AliasCall* node) const = 0;
        virtual any visit(AliasExpr* node) const = 0;
        virtual any visit(ApplyExpr* node) const = 0;
        virtual any visit(AsDataStructurePattern* node) const = 0;
        virtual any visit(BinaryNotOpExpr* node) const = 0;
        virtual any visit(BitwiseAndExpr* node) const = 0;
        virtual any visit(BitwiseOrExpr* node) const = 0;
        virtual any visit(BitwiseXorExpr* node) const = 0;
        virtual any visit(BodyWithGuards* node) const = 0;
        virtual any visit(BodyWithoutGuards* node) const = 0;
        virtual any visit(ByteExpr* node) const = 0;
        virtual any visit(CaseExpr* node) const = 0;
        virtual any visit(CatchExpr* node) const = 0;
        virtual any visit(CatchPatternExpr* node) const = 0;
        virtual any visit(CharacterExpr* node) const = 0;
        virtual any visit(ConsLeftExpr* node) const = 0;
        virtual any visit(ConsRightExpr* node) const = 0;
        virtual any visit(DictExpr* node) const = 0;
        virtual any visit(DictGeneratorExpr* node) const = 0;
        virtual any visit(DictGeneratorReducer* node) const = 0;
        virtual any visit(DictPattern* node) const = 0;
        virtual any visit(DivideExpr* node) const = 0;
        virtual any visit(DoExpr* node) const = 0;
        virtual any visit(EqExpr* node) const = 0;
        virtual any visit(FalseLiteralExpr* node) const = 0;
        virtual any visit(FieldAccessExpr* node) const = 0;
        virtual any visit(FieldUpdateExpr* node) const = 0;
        virtual any visit(FloatExpr* node) const = 0;
        virtual any visit(FqnAlias* node) const = 0;
        virtual any visit(FqnExpr* node) const = 0;
        virtual any visit(FunctionAlias* node) const = 0;
        virtual any visit(FunctionBody* node) const = 0;
        virtual any visit(FunctionExpr* node) const = 0;
        virtual any visit(FunctionsImport* node) const = 0;
        virtual any visit(GtExpr* node) const = 0;
        virtual any visit(GteExpr* node) const = 0;
        virtual any visit(HeadTailsHeadPattern* node) const = 0;
        virtual any visit(HeadTailsPattern* node) const = 0;
        virtual any visit(IdentifierExpr* node) const = 0;
        virtual any visit(IfExpr* node) const = 0;
        virtual any visit(ImportClauseExpr* node) const = 0;
        virtual any visit(ImportExpr* node) const = 0;
        virtual any visit(InExpr* node) const = 0;
        virtual any visit(IntegerExpr* node) const = 0;
        virtual any visit(JoinExpr* node) const = 0;
        virtual any visit(KeyValueCollectionExtractorExpr* node) const = 0;
        virtual any visit(LambdaAlias* node) const = 0;
        virtual any visit(LeftShiftExpr* node) const = 0;
        virtual any visit(LetExpr* node) const = 0;
        virtual any visit(LogicalAndExpr* node) const = 0;
        virtual any visit(LogicalNotOpExpr* node) const = 0;
        virtual any visit(LogicalOrExpr* node) const = 0;
        virtual any visit(LtExpr* node) const = 0;
        virtual any visit(LteExpr* node) const = 0;
        virtual any visit(ModuloExpr* node) const = 0;
        virtual any visit(ModuleAlias* node) const = 0;
        virtual any visit(ModuleCall* node) const = 0;
        virtual any visit(ModuleExpr* node) const = 0;
        virtual any visit(ModuleImport* node) const = 0;
        virtual any visit(MultiplyExpr* node) const = 0;
        virtual any visit(NameCall* node) const = 0;
        virtual any visit(NameExpr* node) const = 0;
        virtual any visit(NeqExpr* node) const = 0;
        virtual any visit(OpExpr* node) const = 0;
        virtual any visit(PackageNameExpr* node) const = 0;
        virtual any visit(PatternAlias* node) const = 0;
        virtual any visit(PatternExpr* node) const = 0;
        virtual any visit(PatternValue* node) const = 0;
        virtual any visit(PatternWithGuards* node) const = 0;
        virtual any visit(PatternWithoutGuards* node) const = 0;
        virtual any visit(PipeLeftExpr* node) const = 0;
        virtual any visit(PipeRightExpr* node) const = 0;
        virtual any visit(PowerExpr* node) const = 0;
        virtual any visit(RaiseExpr* node) const = 0;
        virtual any visit(RangeSequenceExpr* node) const = 0;
        virtual any visit(RecordInstanceExpr* node) const = 0;
        virtual any visit(RecordNode* node) const = 0;
        virtual any visit(RecordPattern* node) const = 0;
        virtual any visit(RightShiftExpr* node) const = 0;
        virtual any visit(SeqGeneratorExpr* node) const = 0;
        virtual any visit(SeqPattern* node) const = 0;
        virtual any visit(SequenceExpr* node) const = 0;
        virtual any visit(SetExpr* node) const = 0;
        virtual any visit(SetGeneratorExpr* node) const = 0;
        virtual any visit(StringExpr* node) const = 0;
        virtual any visit(SubtractExpr* node) const = 0;
        virtual any visit(SymbolExpr* node) const = 0;
        virtual any visit(TailsHeadPattern* node) const = 0;
        virtual any visit(TrueLiteralExpr* node) const = 0;
        virtual any visit(TryCatchExpr* node) const = 0;
        virtual any visit(TupleExpr* node) const = 0;
        virtual any visit(TuplePattern* node) const = 0;
        virtual any visit(UnderscoreNode* node) const = 0;
        virtual any visit(UnitExpr* node) const = 0;
        virtual any visit(ValueAlias* node) const = 0;
        virtual any visit(ValueCollectionExtractorExpr* node) const = 0;
        virtual any visit(ValueExpr* node) const = 0;
        virtual any visit(ValuesSequenceExpr* node) const = 0;
        virtual any visit(WithExpr* node) const = 0;
        virtual any visit(ZerofillRightShiftExpr* node) const = 0;
        virtual any visit(ExprNode* node) const;
        virtual any visit(AstNode* node) const;
        virtual any visit(ScopedNode* node) const;
        virtual any visit(PatternNode* node) const;
    };
}
