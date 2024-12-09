//
// Created by akovari on 17.11.24.
//

#pragma once

#include "ast.h"

namespace yona::compiler
{
    using namespace std;
    using namespace ast;

    class Optimizer final : public AstVisitor
    {
    public:
        [[nodiscard]] any visit(const AddExpr& node) const override;
        [[nodiscard]] any visit(const AliasCall& node) const override;
        [[nodiscard]] any visit(const AliasExpr& node) const override;
        [[nodiscard]] any visit(const ApplyExpr& node) const override;
        [[nodiscard]] any visit(const AsDataStructurePattern& node) const override;
        [[nodiscard]] any visit(const BinaryNotOpExpr& node) const override;
        [[nodiscard]] any visit(const BitwiseAndExpr& node) const override;
        [[nodiscard]] any visit(const BitwiseOrExpr& node) const override;
        [[nodiscard]] any visit(const BitwiseXorExpr& node) const override;
        [[nodiscard]] any visit(const BodyWithGuards& node) const override;
        [[nodiscard]] any visit(const BodyWithoutGuards& node) const override;
        [[nodiscard]] any visit(const ByteExpr& node) const override;
        [[nodiscard]] any visit(const CaseExpr& node) const override;
        [[nodiscard]] any visit(const CatchExpr& node) const override;
        [[nodiscard]] any visit(const CatchPatternExpr& node) const override;
        [[nodiscard]] any visit(const CharacterExpr& node) const override;
        [[nodiscard]] any visit(const ConsLeftExpr& node) const override;
        [[nodiscard]] any visit(const ConsRightExpr& node) const override;
        [[nodiscard]] any visit(const DictExpr& node) const override;
        [[nodiscard]] any visit(const DictGeneratorExpr& node) const override;
        [[nodiscard]] any visit(const DictGeneratorReducer& node) const override;
        [[nodiscard]] any visit(const DictPattern& node) const override;
        [[nodiscard]] any visit(const DivideExpr& node) const override;
        [[nodiscard]] any visit(const DoExpr& node) const override;
        [[nodiscard]] any visit(const EqExpr& node) const override;
        [[nodiscard]] any visit(const FalseLiteralExpr& node) const override;
        [[nodiscard]] any visit(const FieldAccessExpr& node) const override;
        [[nodiscard]] any visit(const FieldUpdateExpr& node) const override;
        [[nodiscard]] any visit(const FloatExpr& node) const override;
        [[nodiscard]] any visit(const FqnAlias& node) const override;
        [[nodiscard]] any visit(const FqnExpr& node) const override;
        [[nodiscard]] any visit(const FunctionAlias& node) const override;
        [[nodiscard]] any visit(const FunctionBody& node) const override;
        [[nodiscard]] any visit(const FunctionExpr& node) const override;
        [[nodiscard]] any visit(const FunctionsImport& node) const override;
        [[nodiscard]] any visit(const GtExpr& node) const override;
        [[nodiscard]] any visit(const GteExpr& node) const override;
        [[nodiscard]] any visit(const HeadTailsHeadPattern& node) const override;
        [[nodiscard]] any visit(const HeadTailsPattern& node) const override;
        [[nodiscard]] any visit(const IdentifierExpr& node) const override;
        [[nodiscard]] any visit(const IfExpr& node) const override;
        [[nodiscard]] any visit(const ImportClauseExpr& node) const override;
        [[nodiscard]] any visit(const ImportExpr& node) const override;
        [[nodiscard]] any visit(const InExpr& node) const override;
        [[nodiscard]] any visit(const IntegerExpr& node) const override;
        [[nodiscard]] any visit(const JoinExpr& node) const override;
        [[nodiscard]] any visit(const KeyValueCollectionExtractorExpr& node) const override;
        [[nodiscard]] any visit(const LambdaAlias& node) const override;
        [[nodiscard]] any visit(const LeftShiftExpr& node) const override;
        [[nodiscard]] any visit(const LetExpr& node) const override;
        [[nodiscard]] any visit(const LogicalAndExpr& node) const override;
        [[nodiscard]] any visit(const LogicalNotOpExpr& node) const override;
        [[nodiscard]] any visit(const LogicalOrExpr& node) const override;
        [[nodiscard]] any visit(const LtExpr& node) const override;
        [[nodiscard]] any visit(const LteExpr& node) const override;
        [[nodiscard]] any visit(const ModuloExpr& node) const override;
        [[nodiscard]] any visit(const ModuleAlias& node) const override;
        [[nodiscard]] any visit(const ModuleCall& node) const override;
        [[nodiscard]] any visit(const ModuleExpr& node) const override;
        [[nodiscard]] any visit(const ModuleImport& node) const override;
        [[nodiscard]] any visit(const MultiplyExpr& node) const override;
        [[nodiscard]] any visit(const NameCall& node) const override;
        [[nodiscard]] any visit(const NameExpr& node) const override;
        [[nodiscard]] any visit(const NeqExpr& node) const override;
        [[nodiscard]] any visit(const OpExpr& node) const override;
        [[nodiscard]] any visit(const PackageNameExpr& node) const override;
        [[nodiscard]] any visit(const PatternAlias& node) const override;
        [[nodiscard]] any visit(const PatternExpr& node) const override;
        [[nodiscard]] any visit(const PatternValue& node) const override;
        [[nodiscard]] any visit(const PatternWithGuards& node) const override;
        [[nodiscard]] any visit(const PatternWithoutGuards& node) const override;
        [[nodiscard]] any visit(const PipeLeftExpr& node) const override;
        [[nodiscard]] any visit(const PipeRightExpr& node) const override;
        [[nodiscard]] any visit(const PowerExpr& node) const override;
        [[nodiscard]] any visit(const RaiseExpr& node) const override;
        [[nodiscard]] any visit(const RangeSequenceExpr& node) const override;
        [[nodiscard]] any visit(const RecordInstanceExpr& node) const override;
        [[nodiscard]] any visit(const RecordNode& node) const override;
        [[nodiscard]] any visit(const RecordPattern& node) const override;
        [[nodiscard]] any visit(const RightShiftExpr& node) const override;
        [[nodiscard]] any visit(const SeqGeneratorExpr& node) const override;
        [[nodiscard]] any visit(const SeqPattern& node) const override;
        [[nodiscard]] any visit(const SequenceExpr& node) const override;
        [[nodiscard]] any visit(const SetExpr& node) const override;
        [[nodiscard]] any visit(const SetGeneratorExpr& node) const override;
        [[nodiscard]] any visit(const StringExpr& node) const override;
        [[nodiscard]] any visit(const SubtractExpr& node) const override;
        [[nodiscard]] any visit(const SymbolExpr& node) const override;
        [[nodiscard]] any visit(const TailsHeadPattern& node) const override;
        [[nodiscard]] any visit(const TrueLiteralExpr& node) const override;
        [[nodiscard]] any visit(const TryCatchExpr& node) const override;
        [[nodiscard]] any visit(const TupleExpr& node) const override;
        [[nodiscard]] any visit(const TuplePattern& node) const override;
        [[nodiscard]] any visit(const UnderscoreNode& node) const override;
        [[nodiscard]] any visit(const UnitExpr& node) const override;
        [[nodiscard]] any visit(const ValueAlias& node) const override;
        [[nodiscard]] any visit(const ValueCollectionExtractorExpr& node) const override;
        [[nodiscard]] any visit(const ValueExpr& node) const override;
        [[nodiscard]] any visit(const ValuesSequenceExpr& node) const override;
        [[nodiscard]] any visit(const WithExpr& node) const override;
        [[nodiscard]] any visit(const ZerofillRightShiftExpr& node) const override;
        ~Optimizer() override = default;
        [[nodiscard]] any visit(const ExprNode& node) const override;
        [[nodiscard]] any visit(const AstNode& node) const override;
        [[nodiscard]] any visit(const ScopedNode& node) const override;
        [[nodiscard]] any visit(const PatternNode& node) const override;
    };
}
