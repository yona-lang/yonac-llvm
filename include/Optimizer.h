//
// Created by akovari on 17.11.24.
//

#pragma once

#include "ast.h"


namespace yona::compiler
{
    using namespace std;
    using namespace ast;

    class Optimizer : public AstVisitor<const AstNode>
    {
    public:
        [[nodiscard]] const AstNode visit(const AddExpr& node) const override;
        [[nodiscard]] const AstNode visit(const AliasCall& node) const override;
        [[nodiscard]] const AstNode visit(const AliasExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ApplyExpr& node) const override;
        [[nodiscard]] const AstNode visit(const AsDataStructurePattern& node) const override;
        [[nodiscard]] const AstNode visit(const BinaryNotOpExpr& node) const override;
        [[nodiscard]] const AstNode visit(const BitwiseAndExpr& node) const override;
        [[nodiscard]] const AstNode visit(const BitwiseOrExpr& node) const override;
        [[nodiscard]] const AstNode visit(const BitwiseXorExpr& node) const override;
        [[nodiscard]] const AstNode visit(const BodyWithGuards& node) const override;
        [[nodiscard]] const AstNode visit(const BodyWithoutGuards& node) const override;
        [[nodiscard]] const AstNode visit(const ByteExpr& node) const override;
        [[nodiscard]] const AstNode visit(const CaseExpr& node) const override;
        [[nodiscard]] const AstNode visit(const CatchExpr& node) const override;
        [[nodiscard]] const AstNode visit(const CatchPatternExpr& node) const override;
        [[nodiscard]] const AstNode visit(const CharacterExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ConsLeftExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ConsRightExpr& node) const override;
        [[nodiscard]] const AstNode visit(const DictExpr& node) const override;
        [[nodiscard]] const AstNode visit(const DictGeneratorExpr& node) const override;
        [[nodiscard]] const AstNode visit(const DictGeneratorReducer& node) const override;
        [[nodiscard]] const AstNode visit(const DictPattern& node) const override;
        [[nodiscard]] const AstNode visit(const DivideExpr& node) const override;
        [[nodiscard]] const AstNode visit(const DoExpr& node) const override;
        [[nodiscard]] const AstNode visit(const EqExpr& node) const override;
        [[nodiscard]] const AstNode visit(const FalseLiteralExpr& node) const override;
        [[nodiscard]] const AstNode visit(const FieldAccessExpr& node) const override;
        [[nodiscard]] const AstNode visit(const FieldUpdateExpr& node) const override;
        [[nodiscard]] const AstNode visit(const FloatExpr& node) const override;
        [[nodiscard]] const AstNode visit(const FqnAlias& node) const override;
        [[nodiscard]] const AstNode visit(const FqnExpr& node) const override;
        [[nodiscard]] const AstNode visit(const FunctionAlias& node) const override;
        [[nodiscard]] const AstNode visit(const FunctionBody& node) const override;
        [[nodiscard]] const AstNode visit(const FunctionExpr& node) const override;
        [[nodiscard]] const AstNode visit(const FunctionsImport& node) const override;
        [[nodiscard]] const AstNode visit(const GtExpr& node) const override;
        [[nodiscard]] const AstNode visit(const GteExpr& node) const override;
        [[nodiscard]] const AstNode visit(const HeadTailsHeadPattern& node) const override;
        [[nodiscard]] const AstNode visit(const HeadTailsPattern& node) const override;
        [[nodiscard]] const AstNode visit(const IdentifierExpr& node) const override;
        [[nodiscard]] const AstNode visit(const IfExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ImportClauseExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ImportExpr& node) const override;
        [[nodiscard]] const AstNode visit(const InExpr& node) const override;
        [[nodiscard]] const AstNode visit(const IntegerExpr& node) const override;
        [[nodiscard]] const AstNode visit(const JoinExpr& node) const override;
        [[nodiscard]] const AstNode visit(const KeyValueCollectionExtractorExpr& node) const override;
        [[nodiscard]] const AstNode visit(const LambdaAlias& node) const override;
        [[nodiscard]] const AstNode visit(const LeftShiftExpr& node) const override;
        [[nodiscard]] const AstNode visit(const LetExpr& node) const override;
        [[nodiscard]] const AstNode visit(const LogicalAndExpr& node) const override;
        [[nodiscard]] const AstNode visit(const LogicalNotOpExpr& node) const override;
        [[nodiscard]] const AstNode visit(const LogicalOrExpr& node) const override;
        [[nodiscard]] const AstNode visit(const LtExpr& node) const override;
        [[nodiscard]] const AstNode visit(const LteExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ModuloExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ModuleAlias& node) const override;
        [[nodiscard]] const AstNode visit(const ModuleCall& node) const override;
        [[nodiscard]] const AstNode visit(const ModuleExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ModuleImport& node) const override;
        [[nodiscard]] const AstNode visit(const MultiplyExpr& node) const override;
        [[nodiscard]] const AstNode visit(const NameCall& node) const override;
        [[nodiscard]] const AstNode visit(const NameExpr& node) const override;
        [[nodiscard]] const AstNode visit(const NeqExpr& node) const override;
        [[nodiscard]] const AstNode visit(const OpExpr& node) const override;
        [[nodiscard]] const AstNode visit(const PackageNameExpr& node) const override;
        [[nodiscard]] const AstNode visit(const PatternAlias& node) const override;
        [[nodiscard]] const AstNode visit(const PatternExpr& node) const override;
        [[nodiscard]] const AstNode visit(const PatternValue& node) const override;
        [[nodiscard]] const AstNode visit(const PatternWithGuards& node) const override;
        [[nodiscard]] const AstNode visit(const PatternWithoutGuards& node) const override;
        [[nodiscard]] const AstNode visit(const PipeLeftExpr& node) const override;
        [[nodiscard]] const AstNode visit(const PipeRightExpr& node) const override;
        [[nodiscard]] const AstNode visit(const PowerExpr& node) const override;
        [[nodiscard]] const AstNode visit(const RaiseExpr& node) const override;
        [[nodiscard]] const AstNode visit(const RangeSequenceExpr& node) const override;
        [[nodiscard]] const AstNode visit(const RecordInstanceExpr& node) const override;
        [[nodiscard]] const AstNode visit(const RecordNode& node) const override;
        [[nodiscard]] const AstNode visit(const RecordPattern& node) const override;
        [[nodiscard]] const AstNode visit(const RightShiftExpr& node) const override;
        [[nodiscard]] const AstNode visit(const SeqGeneratorExpr& node) const override;
        [[nodiscard]] const AstNode visit(const SeqPattern& node) const override;
        [[nodiscard]] const AstNode visit(const SequenceExpr& node) const override;
        [[nodiscard]] const AstNode visit(const SetExpr& node) const override;
        [[nodiscard]] const AstNode visit(const SetGeneratorExpr& node) const override;
        [[nodiscard]] const AstNode visit(const StringExpr& node) const override;
        [[nodiscard]] const AstNode visit(const SubtractExpr& node) const override;
        [[nodiscard]] const AstNode visit(const SymbolExpr& node) const override;
        [[nodiscard]] const AstNode visit(const TailsHeadPattern& node) const override;
        [[nodiscard]] const AstNode visit(const TrueLiteralExpr& node) const override;
        [[nodiscard]] const AstNode visit(const TryCatchExpr& node) const override;
        [[nodiscard]] const AstNode visit(const TupleExpr& node) const override;
        [[nodiscard]] const AstNode visit(const TuplePattern& node) const override;
        [[nodiscard]] const AstNode visit(const UnitExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ValueAlias& node) const override;
        [[nodiscard]] const AstNode visit(const ValueCollectionExtractorExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ValueExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ValuesSequenceExpr& node) const override;
        [[nodiscard]] const AstNode visit(const WithExpr& node) const override;
        [[nodiscard]] const AstNode visit(const ZerofillRightShiftExpr& node) const override;
        ~Optimizer() override = default;
        [[nodiscard]] const AstNode visit(const ExprNode& node) const override;
        [[nodiscard]] const AstNode visit(const AstNode& node) const override;
        [[nodiscard]] const AstNode visit(const ScopedNode& node) const override;
        [[nodiscard]] const AstNode visit(const PatternNode& node) const override;
    };
}
