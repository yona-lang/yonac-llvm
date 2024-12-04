//
// Created by akovari on 17.11.24.
//

#include "Interpreter.h"
#include <boost/log/trivial.hpp>

namespace yona::interp
{
    Interpreter::Interpreter()
    {
        stack_ = stack<Frame>();
        stack_.push(Frame());
    }
    interp_result_t Interpreter::visit(const AddExpr& node) const
    {
    }
    interp_result_t Interpreter::visit(const AliasCall& node) const {}
    interp_result_t Interpreter::visit(const AliasExpr& node) const {}
    interp_result_t Interpreter::visit(const ApplyExpr& node) const {}
    interp_result_t Interpreter::visit(const AsDataStructurePattern& node) const {}
    interp_result_t Interpreter::visit(const BinaryNotOpExpr& node) const {}
    interp_result_t Interpreter::visit(const BitwiseAndExpr& node) const {}
    interp_result_t Interpreter::visit(const BitwiseOrExpr& node) const {}
    interp_result_t Interpreter::visit(const BitwiseXorExpr& node) const {}
    interp_result_t Interpreter::visit(const BodyWithGuards& node) const {}
    interp_result_t Interpreter::visit(const BodyWithoutGuards& node) const {}
    interp_result_t Interpreter::visit(const ByteExpr& node) const {}
    interp_result_t Interpreter::visit(const CaseExpr& node) const {}
    interp_result_t Interpreter::visit(const CatchExpr& node) const {}
    interp_result_t Interpreter::visit(const CatchPatternExpr& node) const {}
    interp_result_t Interpreter::visit(const CharacterExpr& node) const {}
    interp_result_t Interpreter::visit(const ConsLeftExpr& node) const {}
    interp_result_t Interpreter::visit(const ConsRightExpr& node) const {}
    interp_result_t Interpreter::visit(const DictExpr& node) const {}
    interp_result_t Interpreter::visit(const DictGeneratorExpr& node) const {}
    interp_result_t Interpreter::visit(const DictGeneratorReducer& node) const {}
    interp_result_t Interpreter::visit(const DictPattern& node) const {}
    interp_result_t Interpreter::visit(const DivideExpr& node) const {}
    interp_result_t Interpreter::visit(const DoExpr& node) const {}
    interp_result_t Interpreter::visit(const EqExpr& node) const {}
    interp_result_t Interpreter::visit(const FalseLiteralExpr& node) const {}
    interp_result_t Interpreter::visit(const FieldAccessExpr& node) const {}
    interp_result_t Interpreter::visit(const FieldUpdateExpr& node) const {}
    interp_result_t Interpreter::visit(const FloatExpr& node) const {}
    interp_result_t Interpreter::visit(const FqnAlias& node) const {}
    interp_result_t Interpreter::visit(const FqnExpr& node) const {}
    interp_result_t Interpreter::visit(const FunctionAlias& node) const {}
    interp_result_t Interpreter::visit(const FunctionBody& node) const {}
    interp_result_t Interpreter::visit(const FunctionExpr& node) const {}
    interp_result_t Interpreter::visit(const FunctionsImport& node) const {}
    interp_result_t Interpreter::visit(const GtExpr& node) const {}
    interp_result_t Interpreter::visit(const GteExpr& node) const {}
    interp_result_t Interpreter::visit(const HeadTailsHeadPattern& node) const {}
    interp_result_t Interpreter::visit(const HeadTailsPattern& node) const {}
    interp_result_t Interpreter::visit(const IdentifierExpr& node) const {}
    interp_result_t Interpreter::visit(const IfExpr& node) const {}
    interp_result_t Interpreter::visit(const ImportClauseExpr& node) const {}
    interp_result_t Interpreter::visit(const ImportExpr& node) const {}
    interp_result_t Interpreter::visit(const InExpr& node) const {}
    interp_result_t Interpreter::visit(const IntegerExpr& node) const
    {
        return node.value;
    }
    interp_result_t Interpreter::visit(const JoinExpr& node) const {}
    interp_result_t Interpreter::visit(const KeyValueCollectionExtractorExpr& node) const {}
    interp_result_t Interpreter::visit(const LambdaAlias& node) const {}
    interp_result_t Interpreter::visit(const LeftShiftExpr& node) const {}
    interp_result_t Interpreter::visit(const LetExpr& node) const {}
    interp_result_t Interpreter::visit(const LogicalAndExpr& node) const {}
    interp_result_t Interpreter::visit(const LogicalNotOpExpr& node) const {}
    interp_result_t Interpreter::visit(const LogicalOrExpr& node) const {}
    interp_result_t Interpreter::visit(const LtExpr& node) const {}
    interp_result_t Interpreter::visit(const LteExpr& node) const {}
    interp_result_t Interpreter::visit(const ModuloExpr& node) const {}
    interp_result_t Interpreter::visit(const ModuleAlias& node) const {}
    interp_result_t Interpreter::visit(const ModuleCall& node) const {}
    interp_result_t Interpreter::visit(const ModuleExpr& node) const {}
    interp_result_t Interpreter::visit(const ModuleImport& node) const {}
    interp_result_t Interpreter::visit(const MultiplyExpr& node) const {}
    interp_result_t Interpreter::visit(const NameCall& node) const {}
    interp_result_t Interpreter::visit(const NameExpr& node) const {}
    interp_result_t Interpreter::visit(const NeqExpr& node) const {}
    interp_result_t Interpreter::visit(const OpExpr& node) const {}
    interp_result_t Interpreter::visit(const PackageNameExpr& node) const {}
    interp_result_t Interpreter::visit(const PatternAlias& node) const {}
    interp_result_t Interpreter::visit(const PatternExpr& node) const {}
    interp_result_t Interpreter::visit(const PatternValue& node) const {}
    interp_result_t Interpreter::visit(const PatternWithGuards& node) const {}
    interp_result_t Interpreter::visit(const PatternWithoutGuards& node) const {}
    interp_result_t Interpreter::visit(const PipeLeftExpr& node) const {}
    interp_result_t Interpreter::visit(const PipeRightExpr& node) const {}
    interp_result_t Interpreter::visit(const PowerExpr& node) const {}
    interp_result_t Interpreter::visit(const RaiseExpr& node) const {}
    interp_result_t Interpreter::visit(const RangeSequenceExpr& node) const {}
    interp_result_t Interpreter::visit(const RecordInstanceExpr& node) const {}
    interp_result_t Interpreter::visit(const RecordNode& node) const {}
    interp_result_t Interpreter::visit(const RecordPattern& node) const {}
    interp_result_t Interpreter::visit(const RightShiftExpr& node) const {}
    interp_result_t Interpreter::visit(const SeqGeneratorExpr& node) const {}
    interp_result_t Interpreter::visit(const SeqPattern& node) const {}
    interp_result_t Interpreter::visit(const SequenceExpr& node) const {}
    interp_result_t Interpreter::visit(const SetExpr& node) const {}
    interp_result_t Interpreter::visit(const SetGeneratorExpr& node) const {}
    interp_result_t Interpreter::visit(const StringExpr& node) const {}
    interp_result_t Interpreter::visit(const SubtractExpr& node) const {}
    interp_result_t Interpreter::visit(const SymbolExpr& node) const {}
    interp_result_t Interpreter::visit(const TailsHeadPattern& node) const {}
    interp_result_t Interpreter::visit(const TrueLiteralExpr& node) const {}
    interp_result_t Interpreter::visit(const TryCatchExpr& node) const {}
    interp_result_t Interpreter::visit(const TupleExpr& node) const {}
    interp_result_t Interpreter::visit(const TuplePattern& node) const {}
    interp_result_t Interpreter::visit(const UnitExpr& node) const {}
    interp_result_t Interpreter::visit(const ValueAlias& node) const {}
    interp_result_t Interpreter::visit(const ValueCollectionExtractorExpr& node) const {}
    interp_result_t Interpreter::visit(const ValueExpr& node) const {}
    interp_result_t Interpreter::visit(const ValuesSequenceExpr& node) const {}
    interp_result_t Interpreter::visit(const WithExpr& node) const {}
    interp_result_t Interpreter::visit(const ZerofillRightShiftExpr& node) const {}
    interp_result_t Interpreter::visit(const ExprNode& node) const { return AstVisitor::visit(node); }
    interp_result_t Interpreter::visit(const AstNode& node) const { return AstVisitor::visit(node); }
    interp_result_t Interpreter::visit(const ScopedNode& node) const { return AstVisitor::visit(node); }
    interp_result_t Interpreter::visit(const PatternNode& node) const { return AstVisitor::visit(node); }
} // yona::interp
