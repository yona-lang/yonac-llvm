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
    any Interpreter::visit(const AddExpr& node) const {}
    any Interpreter::visit(const AliasCall& node) const {}
    any Interpreter::visit(const AliasExpr& node) const {}
    any Interpreter::visit(const ApplyExpr& node) const {}
    any Interpreter::visit(const AsDataStructurePattern& node) const {}
    any Interpreter::visit(const BinaryNotOpExpr& node) const {}
    any Interpreter::visit(const BitwiseAndExpr& node) const {}
    any Interpreter::visit(const BitwiseOrExpr& node) const {}
    any Interpreter::visit(const BitwiseXorExpr& node) const {}
    any Interpreter::visit(const BodyWithGuards& node) const {}
    any Interpreter::visit(const BodyWithoutGuards& node) const {}
    any Interpreter::visit(const ByteExpr& node) const {}
    any Interpreter::visit(const CaseExpr& node) const {}
    any Interpreter::visit(const CatchExpr& node) const {}
    any Interpreter::visit(const CatchPatternExpr& node) const {}
    any Interpreter::visit(const CharacterExpr& node) const {}
    any Interpreter::visit(const ConsLeftExpr& node) const {}
    any Interpreter::visit(const ConsRightExpr& node) const {}
    any Interpreter::visit(const DictExpr& node) const {}
    any Interpreter::visit(const DictGeneratorExpr& node) const {}
    any Interpreter::visit(const DictGeneratorReducer& node) const {}
    any Interpreter::visit(const DictPattern& node) const {}
    any Interpreter::visit(const DivideExpr& node) const {}
    any Interpreter::visit(const DoExpr& node) const {}
    any Interpreter::visit(const EqExpr& node) const {}
    any Interpreter::visit(const FalseLiteralExpr& node) const {}
    any Interpreter::visit(const FieldAccessExpr& node) const {}
    any Interpreter::visit(const FieldUpdateExpr& node) const {}
    any Interpreter::visit(const FloatExpr& node) const {}
    any Interpreter::visit(const FqnAlias& node) const {}
    any Interpreter::visit(const FqnExpr& node) const {}
    any Interpreter::visit(const FunctionAlias& node) const {}
    any Interpreter::visit(const FunctionBody& node) const {}
    any Interpreter::visit(const FunctionExpr& node) const {}
    any Interpreter::visit(const FunctionsImport& node) const {}
    any Interpreter::visit(const GtExpr& node) const {}
    any Interpreter::visit(const GteExpr& node) const {}
    any Interpreter::visit(const HeadTailsHeadPattern& node) const {}
    any Interpreter::visit(const HeadTailsPattern& node) const {}
    any Interpreter::visit(const IdentifierExpr& node) const {}
    any Interpreter::visit(const IfExpr& node) const {}
    any Interpreter::visit(const ImportClauseExpr& node) const {}
    any Interpreter::visit(const ImportExpr& node) const {}
    any Interpreter::visit(const InExpr& node) const {}
    any Interpreter::visit(const IntegerExpr& node) const { return node.value; }
    any Interpreter::visit(const JoinExpr& node) const {}
    any Interpreter::visit(const KeyValueCollectionExtractorExpr& node) const {}
    any Interpreter::visit(const LambdaAlias& node) const {}
    any Interpreter::visit(const LeftShiftExpr& node) const {}
    any Interpreter::visit(const LetExpr& node) const {}
    any Interpreter::visit(const LogicalAndExpr& node) const {}
    any Interpreter::visit(const LogicalNotOpExpr& node) const {}
    any Interpreter::visit(const LogicalOrExpr& node) const {}
    any Interpreter::visit(const LtExpr& node) const {}
    any Interpreter::visit(const LteExpr& node) const {}
    any Interpreter::visit(const ModuloExpr& node) const {}
    any Interpreter::visit(const ModuleAlias& node) const {}
    any Interpreter::visit(const ModuleCall& node) const {}
    any Interpreter::visit(const ModuleExpr& node) const {}
    any Interpreter::visit(const ModuleImport& node) const {}
    any Interpreter::visit(const MultiplyExpr& node) const {}
    any Interpreter::visit(const NameCall& node) const {}
    any Interpreter::visit(const NameExpr& node) const {}
    any Interpreter::visit(const NeqExpr& node) const {}
    any Interpreter::visit(const OpExpr& node) const {}
    any Interpreter::visit(const PackageNameExpr& node) const {}
    any Interpreter::visit(const PatternAlias& node) const {}
    any Interpreter::visit(const PatternExpr& node) const {}
    any Interpreter::visit(const PatternValue& node) const {}
    any Interpreter::visit(const PatternWithGuards& node) const {}
    any Interpreter::visit(const PatternWithoutGuards& node) const {}
    any Interpreter::visit(const PipeLeftExpr& node) const {}
    any Interpreter::visit(const PipeRightExpr& node) const {}
    any Interpreter::visit(const PowerExpr& node) const {}
    any Interpreter::visit(const RaiseExpr& node) const {}
    any Interpreter::visit(const RangeSequenceExpr& node) const {}
    any Interpreter::visit(const RecordInstanceExpr& node) const {}
    any Interpreter::visit(const RecordNode& node) const {}
    any Interpreter::visit(const RecordPattern& node) const {}
    any Interpreter::visit(const RightShiftExpr& node) const {}
    any Interpreter::visit(const SeqGeneratorExpr& node) const {}
    any Interpreter::visit(const SeqPattern& node) const {}
    any Interpreter::visit(const SequenceExpr& node) const {}
    any Interpreter::visit(const SetExpr& node) const {}
    any Interpreter::visit(const SetGeneratorExpr& node) const {}
    any Interpreter::visit(const StringExpr& node) const {}
    any Interpreter::visit(const SubtractExpr& node) const {}
    any Interpreter::visit(const SymbolExpr& node) const {}
    any Interpreter::visit(const TailsHeadPattern& node) const {}
    any Interpreter::visit(const TrueLiteralExpr& node) const {}
    any Interpreter::visit(const TryCatchExpr& node) const {}
    any Interpreter::visit(const TupleExpr& node) const {}
    any Interpreter::visit(const TuplePattern& node) const {}
    any Interpreter::visit(const UnderscoreNode& node) const {}
    any Interpreter::visit(const UnitExpr& node) const {}
    any Interpreter::visit(const ValueAlias& node) const {}
    any Interpreter::visit(const ValueCollectionExtractorExpr& node) const {}
    any Interpreter::visit(const ValueExpr& node) const {}
    any Interpreter::visit(const ValuesSequenceExpr& node) const {}
    any Interpreter::visit(const WithExpr& node) const {}
    any Interpreter::visit(const ZerofillRightShiftExpr& node) const {}
    any Interpreter::visit(const ExprNode& node) const { return AstVisitor::visit(node); }
    any Interpreter::visit(const AstNode& node) const { return AstVisitor::visit(node); }
    any Interpreter::visit(const ScopedNode& node) const { return AstVisitor::visit(node); }
    any Interpreter::visit(const PatternNode& node) const { return AstVisitor::visit(node); }
} // yona::interp
