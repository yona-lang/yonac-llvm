#pragma once

#include "yona_export.h"
#include <stdexcept>

// Disable MSVC warning C4251 about STL types in exported class interfaces
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

namespace yona::ast {

// Forward declarations
class AddExpr;
class AliasCall;
class ApplyExpr;
class AsDataStructurePattern;
class BinaryNotOpExpr;
class BitwiseAndExpr;
class BitwiseOrExpr;
class BitwiseXorExpr;
class BodyWithGuards;
class BodyWithoutGuards;
class ByteExpr;
class CaseClause;
class CaseExpr;
class CatchExpr;
class CatchPatternExpr;
class CharacterExpr;
class ConsLeftExpr;
class ConsRightExpr;
class DictExpr;
class DictGeneratorExpr;
class DictGeneratorReducer;
class DictPattern;
class DivideExpr;
class DoExpr;
class EqExpr;
class FalseLiteralExpr;
class FieldAccessExpr;
class FieldUpdateExpr;
class FloatExpr;
class FqnAlias;
class FqnExpr;
class FunctionAlias;
class FunctionDeclaration;
class FunctionExpr;
class FunctionsImport;
class GtExpr;
class GteExpr;
class HeadTailsHeadPattern;
class HeadTailsPattern;
class IfExpr;
class ImportClauseExpr;
class ImportExpr;
class InExpr;
class IntegerExpr;
class JoinExpr;
class KeyValueCollectionExtractorExpr;
class LambdaAlias;
class LeftShiftExpr;
class LetExpr;
class LogicalAndExpr;
class LogicalNotOpExpr;
class LogicalOrExpr;
class LtExpr;
class LteExpr;
class ModuloExpr;
class ModuleAlias;
class ModuleCall;
class ExprCall;
class ModuleExpr;
class ModuleImport;
class MultiplyExpr;
class NameCall;
class NameExpr;
class NeqExpr;
class PackageNameExpr;
class PipeLeftExpr;
class PipeRightExpr;
class PatternAlias;
class PatternExpr;
class PatternValue;
class PatternWithGuards;
class PatternWithoutGuards;
class PowerExpr;
class RaiseExpr;
class RangeSequenceExpr;
class RecordInstanceExpr;
class RecordNode;
class RecordPattern;
class RightShiftExpr;
class SeqGeneratorExpr;
class SeqPattern;
class SetExpr;
class SetGeneratorExpr;
class StringExpr;
class SubtractExpr;
class SymbolExpr;
class TailsHeadPattern;
class TrueLiteralExpr;
class TryCatchExpr;
class TupleExpr;
class TuplePattern;
class TypeDeclaration;
class TypeDefinition;
class TypeNode;
class TypeInstance;
class UnderscoreNode;
class UnitExpr;
class ValueAlias;
class ValueCollectionExtractorExpr;
class ValuesSequenceExpr;
class WithExpr;
class ZerofillRightShiftExpr;
class MainNode;
class IdentifierExpr;
class BuiltinTypeNode;
class UserDefinedTypeNode;
class ExprNode;
class AstNode;
class ScopedNode;
class PatternNode;
class ValueExpr;
class SequenceExpr;
class FunctionBody;
class AliasExpr;
class OpExpr;
class BinaryOpExpr;
class TypeNameNode;
class CallExpr;
class GeneratorExpr;
class CollectionExtractorExpr;

// Templated visitor base class that allows each visitor to specify its own return type
template<typename ResultType>
class AstVisitor {
protected:
  // Helper method to dispatch based on runtime type when we have a base pointer
  // This avoids infinite recursion when accept() is called on base class pointers
  ResultType dispatchVisit(AstNode *node) const {
    // First try concrete expression types
    if (auto* n = dynamic_cast<AddExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<SubtractExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<MultiplyExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<DivideExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ModuloExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<PowerExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<EqExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<NeqExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<LtExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<LteExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<GtExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<GteExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<LogicalAndExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<LogicalOrExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<LogicalNotOpExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<BitwiseAndExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<BitwiseOrExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<BitwiseXorExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<BinaryNotOpExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<LeftShiftExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<RightShiftExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ZerofillRightShiftExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<IntegerExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FloatExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<StringExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<CharacterExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ByteExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<TrueLiteralExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FalseLiteralExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<UnitExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<SymbolExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<LetExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<IfExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<CaseExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<CaseClause*>(node)) return visit(n);
    if (auto* n = dynamic_cast<DoExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<WithExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<RaiseExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<TryCatchExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<CatchExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<CatchPatternExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FunctionDeclaration*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FunctionExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ApplyExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<NameCall*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ModuleCall*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ExprCall*>(node)) return visit(n);
    if (auto* n = dynamic_cast<TupleExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<SetExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<SeqGeneratorExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<SetGeneratorExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<DictExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<DictGeneratorExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<DictGeneratorReducer*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ConsLeftExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ConsRightExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<RangeSequenceExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ValuesSequenceExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<InExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<JoinExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<PatternValue*>(node)) return visit(n);
    if (auto* n = dynamic_cast<PatternWithoutGuards*>(node)) return visit(n);
    if (auto* n = dynamic_cast<PatternWithGuards*>(node)) return visit(n);
    if (auto* n = dynamic_cast<TuplePattern*>(node)) return visit(n);
    if (auto* n = dynamic_cast<SeqPattern*>(node)) return visit(n);
    if (auto* n = dynamic_cast<DictPattern*>(node)) return visit(n);
    if (auto* n = dynamic_cast<RecordPattern*>(node)) return visit(n);
    if (auto* n = dynamic_cast<AsDataStructurePattern*>(node)) return visit(n);
    if (auto* n = dynamic_cast<HeadTailsPattern*>(node)) return visit(n);
    if (auto* n = dynamic_cast<TailsHeadPattern*>(node)) return visit(n);
    if (auto* n = dynamic_cast<HeadTailsHeadPattern*>(node)) return visit(n);
    if (auto* n = dynamic_cast<UnderscoreNode*>(node)) return visit(n);
    if (auto* n = dynamic_cast<NameExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FqnExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FieldAccessExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FieldUpdateExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<RecordNode*>(node)) return visit(n);
    if (auto* n = dynamic_cast<RecordInstanceExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ModuleExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ImportExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ImportClauseExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ModuleImport*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FunctionsImport*>(node)) return visit(n);
    if (auto* n = dynamic_cast<TypeDeclaration*>(node)) return visit(n);
    if (auto* n = dynamic_cast<TypeDefinition*>(node)) return visit(n);
    if (auto* n = dynamic_cast<TypeInstance*>(node)) return visit(n);
    if (auto* n = dynamic_cast<MainNode*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FunctionAlias*>(node)) return visit(n);
    if (auto* n = dynamic_cast<PatternAlias*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ValueAlias*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ModuleAlias*>(node)) return visit(n);
    if (auto* n = dynamic_cast<LambdaAlias*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FqnAlias*>(node)) return visit(n);
    if (auto* n = dynamic_cast<AliasCall*>(node)) return visit(n);
    if (auto* n = dynamic_cast<ValueCollectionExtractorExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<KeyValueCollectionExtractorExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<PipeLeftExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<PipeRightExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<BodyWithoutGuards*>(node)) return visit(n);
    if (auto* n = dynamic_cast<BodyWithGuards*>(node)) return visit(n);
    if (auto* n = dynamic_cast<IdentifierExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<BuiltinTypeNode*>(node)) return visit(n);
    if (auto* n = dynamic_cast<UserDefinedTypeNode*>(node)) return visit(n);
    if (auto* n = dynamic_cast<FunctionBody*>(node)) return visit(n);
    if (auto* n = dynamic_cast<TypeNameNode*>(node)) return visit(n);
    if (auto* n = dynamic_cast<TypeNode*>(node)) return visit(n);
    if (auto* n = dynamic_cast<PatternExpr*>(node)) return visit(n);
    if (auto* n = dynamic_cast<PackageNameExpr*>(node)) return visit(n);

    // If we get here, it's an unknown node type
    throw std::runtime_error("AstVisitor::dispatchVisit: Unknown node type");
  }

public:
  virtual ~AstVisitor() = default;
  virtual ResultType visit(AddExpr *node) const = 0;
  virtual ResultType visit(AliasCall *node) const = 0;
  virtual ResultType visit(ApplyExpr *node) const = 0;
  virtual ResultType visit(AsDataStructurePattern *node) const = 0;
  virtual ResultType visit(BinaryNotOpExpr *node) const = 0;
  virtual ResultType visit(BitwiseAndExpr *node) const = 0;
  virtual ResultType visit(BitwiseOrExpr *node) const = 0;
  virtual ResultType visit(BitwiseXorExpr *node) const = 0;
  virtual ResultType visit(BodyWithGuards *node) const = 0;
  virtual ResultType visit(BodyWithoutGuards *node) const = 0;
  virtual ResultType visit(ByteExpr *node) const = 0;
  virtual ResultType visit(CaseClause *node) const = 0;
  virtual ResultType visit(CaseExpr *node) const = 0;
  virtual ResultType visit(CatchExpr *node) const = 0;
  virtual ResultType visit(CatchPatternExpr *node) const = 0;
  virtual ResultType visit(CharacterExpr *node) const = 0;
  virtual ResultType visit(ConsLeftExpr *node) const = 0;
  virtual ResultType visit(ConsRightExpr *node) const = 0;
  virtual ResultType visit(DictExpr *node) const = 0;
  virtual ResultType visit(DictGeneratorExpr *node) const = 0;
  virtual ResultType visit(DictGeneratorReducer *node) const = 0;
  virtual ResultType visit(DictPattern *node) const = 0;
  virtual ResultType visit(DivideExpr *node) const = 0;
  virtual ResultType visit(DoExpr *node) const = 0;
  virtual ResultType visit(EqExpr *node) const = 0;
  virtual ResultType visit(FalseLiteralExpr *node) const = 0;
  virtual ResultType visit(FieldAccessExpr *node) const = 0;
  virtual ResultType visit(FieldUpdateExpr *node) const = 0;
  virtual ResultType visit(FloatExpr *node) const = 0;
  virtual ResultType visit(FqnAlias *node) const = 0;
  virtual ResultType visit(FqnExpr *node) const = 0;
  virtual ResultType visit(FunctionAlias *node) const = 0;
  virtual ResultType visit(FunctionDeclaration *node) const = 0;
  virtual ResultType visit(FunctionExpr *node) const = 0;
  virtual ResultType visit(FunctionsImport *node) const = 0;
  virtual ResultType visit(GtExpr *node) const = 0;
  virtual ResultType visit(GteExpr *node) const = 0;
  virtual ResultType visit(HeadTailsHeadPattern *node) const = 0;
  virtual ResultType visit(HeadTailsPattern *node) const = 0;
  virtual ResultType visit(IfExpr *node) const = 0;
  virtual ResultType visit(ImportClauseExpr *node) const = 0;
  virtual ResultType visit(ImportExpr *node) const = 0;
  virtual ResultType visit(InExpr *node) const = 0;
  virtual ResultType visit(IntegerExpr *node) const = 0;
  virtual ResultType visit(JoinExpr *node) const = 0;
  virtual ResultType visit(KeyValueCollectionExtractorExpr *node) const = 0;
  virtual ResultType visit(LambdaAlias *node) const = 0;
  virtual ResultType visit(LeftShiftExpr *node) const = 0;
  virtual ResultType visit(LetExpr *node) const = 0;
  virtual ResultType visit(LogicalAndExpr *node) const = 0;
  virtual ResultType visit(LogicalNotOpExpr *node) const = 0;
  virtual ResultType visit(LogicalOrExpr *node) const = 0;
  virtual ResultType visit(LtExpr *node) const = 0;
  virtual ResultType visit(LteExpr *node) const = 0;
  virtual ResultType visit(ModuloExpr *node) const = 0;
  virtual ResultType visit(ModuleAlias *node) const = 0;
  virtual ResultType visit(ModuleCall *node) const = 0;
  virtual ResultType visit(ExprCall *node) const = 0;
  virtual ResultType visit(ModuleExpr *node) const = 0;
  virtual ResultType visit(ModuleImport *node) const = 0;
  virtual ResultType visit(MultiplyExpr *node) const = 0;
  virtual ResultType visit(NameCall *node) const = 0;
  virtual ResultType visit(NameExpr *node) const = 0;
  virtual ResultType visit(NeqExpr *node) const = 0;
  virtual ResultType visit(PackageNameExpr *node) const = 0;
  virtual ResultType visit(PipeLeftExpr *node) const = 0;
  virtual ResultType visit(PipeRightExpr *node) const = 0;
  virtual ResultType visit(PatternAlias *node) const = 0;
  virtual ResultType visit(PatternExpr *node) const = 0;
  virtual ResultType visit(PatternValue *node) const = 0;
  virtual ResultType visit(PatternWithGuards *node) const = 0;
  virtual ResultType visit(PatternWithoutGuards *node) const = 0;
  virtual ResultType visit(PowerExpr *node) const = 0;
  virtual ResultType visit(RaiseExpr *node) const = 0;
  virtual ResultType visit(RangeSequenceExpr *node) const = 0;
  virtual ResultType visit(RecordInstanceExpr *node) const = 0;
  virtual ResultType visit(RecordNode *node) const = 0;
  virtual ResultType visit(RecordPattern *node) const = 0;
  virtual ResultType visit(RightShiftExpr *node) const = 0;
  virtual ResultType visit(SeqGeneratorExpr *node) const = 0;
  virtual ResultType visit(SeqPattern *node) const = 0;
  virtual ResultType visit(SetExpr *node) const = 0;
  virtual ResultType visit(SetGeneratorExpr *node) const = 0;
  virtual ResultType visit(StringExpr *node) const = 0;
  virtual ResultType visit(SubtractExpr *node) const = 0;
  virtual ResultType visit(SymbolExpr *node) const = 0;
  virtual ResultType visit(TailsHeadPattern *node) const = 0;
  virtual ResultType visit(TrueLiteralExpr *node) const = 0;
  virtual ResultType visit(TryCatchExpr *node) const = 0;
  virtual ResultType visit(TupleExpr *node) const = 0;
  virtual ResultType visit(TuplePattern *node) const = 0;
  virtual ResultType visit(TypeDeclaration *node) const = 0;
  virtual ResultType visit(TypeDefinition *node) const = 0;
  virtual ResultType visit(TypeNode *node) const = 0;
  virtual ResultType visit(TypeInstance *node) const = 0;
  virtual ResultType visit(UnderscoreNode *node) const = 0;
  virtual ResultType visit(UnitExpr *node) const = 0;
  virtual ResultType visit(ValueAlias *node) const = 0;
  virtual ResultType visit(ValueCollectionExtractorExpr *node) const = 0;
  virtual ResultType visit(ValuesSequenceExpr *node) const = 0;
  virtual ResultType visit(WithExpr *node) const = 0;
  virtual ResultType visit(ZerofillRightShiftExpr *node) const = 0;
  virtual ResultType visit(MainNode *node) const = 0;
  virtual ResultType visit(IdentifierExpr *node) const = 0;
  virtual ResultType visit(BuiltinTypeNode *node) const = 0;
  virtual ResultType visit(UserDefinedTypeNode *node) const = 0;
  virtual ResultType visit(ScopedNode *node) const = 0;

  // Default implementations for intermediate classes that dispatch to concrete types
  virtual ResultType visit(AstNode *node) const {
    // Base case - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(ExprNode *node) const {
    // ExprNode is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(PatternNode *node) const {
    // PatternNode is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(ValueExpr *node) const {
    // ValueExpr is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(SequenceExpr *node) const {
    // SequenceExpr is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(FunctionBody *node) const {
    // FunctionBody is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(AliasExpr *node) const {
    // AliasExpr is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(OpExpr *node) const {
    // OpExpr is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(BinaryOpExpr *node) const {
    // BinaryOpExpr is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(TypeNameNode *node) const {
    // TypeNameNode is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(CallExpr *node) const {
    // CallExpr is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(GeneratorExpr *node) const {
    // GeneratorExpr is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }

  virtual ResultType visit(CollectionExtractorExpr *node) const {
    // CollectionExtractorExpr is intermediate - dispatch to concrete type
    return dispatchVisit(node);
  }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace yona::ast
