//
// Created by akovari on 17.11.24.
//

#pragma once

#include "ast.h"

namespace yona::interp
{
    using namespace std;
    using namespace ast;

    using interp_result_t = any;
    using symbol_ref_t = interp_result_t;

    struct Symbol
    {
    private:
        string name;
        symbol_ref_t reference;
    };

    struct Frame
    {
    private:
        vector<symbol_ref_t> args_;
        unordered_map<string, Symbol> locals_;

        int64_t ria{0}, rib{0};
        double rda{0.0}, rdb{0.0};
        wchar_t rca{L'\0'}, rcb{L'\0'};
        bool rba{false}, rbb{false};
        byte rya{0}, ryb{0};
        string rsa, rsb;
    };

    class Interpreter : public AstVisitor<interp_result_t>
    {
    private:
        stack<Frame> stack_;
    public:
        explicit Interpreter();
        [[nodiscard]] interp_result_t  visit(const AddExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const AliasCall& node) const override;
        [[nodiscard]] interp_result_t  visit(const AliasExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ApplyExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const AsDataStructurePattern& node) const override;
        [[nodiscard]] interp_result_t  visit(const BinaryNotOpExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const BitwiseAndExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const BitwiseOrExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const BitwiseXorExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const BodyWithGuards& node) const override;
        [[nodiscard]] interp_result_t  visit(const BodyWithoutGuards& node) const override;
        [[nodiscard]] interp_result_t  visit(const ByteExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const CaseExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const CatchExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const CatchPatternExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const CharacterExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ConsLeftExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ConsRightExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const DictExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const DictGeneratorExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const DictGeneratorReducer& node) const override;
        [[nodiscard]] interp_result_t  visit(const DictPattern& node) const override;
        [[nodiscard]] interp_result_t  visit(const DivideExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const DoExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const EqExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const FalseLiteralExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const FieldAccessExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const FieldUpdateExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const FloatExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const FqnAlias& node) const override;
        [[nodiscard]] interp_result_t  visit(const FqnExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const FunctionAlias& node) const override;
        [[nodiscard]] interp_result_t  visit(const FunctionBody& node) const override;
        [[nodiscard]] interp_result_t  visit(const FunctionExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const FunctionsImport& node) const override;
        [[nodiscard]] interp_result_t  visit(const GtExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const GteExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const HeadTailsHeadPattern& node) const override;
        [[nodiscard]] interp_result_t  visit(const HeadTailsPattern& node) const override;
        [[nodiscard]] interp_result_t  visit(const IdentifierExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const IfExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ImportClauseExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ImportExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const InExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const IntegerExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const JoinExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const KeyValueCollectionExtractorExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const LambdaAlias& node) const override;
        [[nodiscard]] interp_result_t  visit(const LeftShiftExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const LetExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const LogicalAndExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const LogicalNotOpExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const LogicalOrExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const LtExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const LteExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ModuloExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ModuleAlias& node) const override;
        [[nodiscard]] interp_result_t  visit(const ModuleCall& node) const override;
        [[nodiscard]] interp_result_t  visit(const ModuleExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ModuleImport& node) const override;
        [[nodiscard]] interp_result_t  visit(const MultiplyExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const NameCall& node) const override;
        [[nodiscard]] interp_result_t  visit(const NameExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const NeqExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const OpExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const PackageNameExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const PatternAlias& node) const override;
        [[nodiscard]] interp_result_t  visit(const PatternExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const PatternValue& node) const override;
        [[nodiscard]] interp_result_t  visit(const PatternWithGuards& node) const override;
        [[nodiscard]] interp_result_t  visit(const PatternWithoutGuards& node) const override;
        [[nodiscard]] interp_result_t  visit(const PipeLeftExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const PipeRightExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const PowerExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const RaiseExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const RangeSequenceExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const RecordInstanceExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const RecordNode& node) const override;
        [[nodiscard]] interp_result_t  visit(const RecordPattern& node) const override;
        [[nodiscard]] interp_result_t  visit(const RightShiftExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const SeqGeneratorExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const SeqPattern& node) const override;
        [[nodiscard]] interp_result_t  visit(const SequenceExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const SetExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const SetGeneratorExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const StringExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const SubtractExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const SymbolExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const TailsHeadPattern& node) const override;
        [[nodiscard]] interp_result_t  visit(const TrueLiteralExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const TryCatchExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const TupleExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const TuplePattern& node) const override;
        [[nodiscard]] interp_result_t  visit(const UnitExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ValueAlias& node) const override;
        [[nodiscard]] interp_result_t  visit(const ValueCollectionExtractorExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ValueExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ValuesSequenceExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const WithExpr& node) const override;
        [[nodiscard]] interp_result_t  visit(const ZerofillRightShiftExpr& node) const override;
        ~Interpreter() override = default;
        [[nodiscard]] interp_result_t visit(const ExprNode& node) const override;
        [[nodiscard]] interp_result_t visit(const AstNode& node) const override;
        [[nodiscard]] interp_result_t visit(const ScopedNode& node) const override;
        [[nodiscard]] interp_result_t visit(const PatternNode& node) const override;
    };

} // yonac::interp
