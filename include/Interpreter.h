//
// Created by akovari on 17.11.24.
//

#pragma once

#include <stack>
#include <unordered_map>
#include <cstdlib>
#include <optional>

#include "yona_export.h"
#include "ast.h"
#include "runtime.h"
#include "TypeChecker.h"

namespace yona::interp {
using namespace std;
using namespace ast;
using namespace runtime;

using symbol_ref_t = shared_ptr<RuntimeObject>;
using InterepterFrame = Frame<symbol_ref_t>;
using ModuleItem = pair<shared_ptr<FqnValue>, shared_ptr<ModuleValue>>;

struct InterpreterState {
  shared_ptr<InterepterFrame> frame;
  stack<ModuleItem> module_stack;
  RuntimeObjectPtr generator_current_element;  // Current element for generator expressions
  RuntimeObjectPtr generator_current_key;      // Current key for dict generator expressions

  // Exception handling state
  bool has_exception = false;
  RuntimeObjectPtr exception_value;  // The exception object (symbol + message)
  SourceContext exception_context;   // Where the exception was raised

  // Module cache: maps FQN to loaded module
  unordered_map<string, shared_ptr<ModuleValue>> module_cache;

  // Module search paths (initialized from YONA_PATH environment variable)
  vector<string> module_paths;

  InterpreterState() : frame(make_shared<InterepterFrame>(nullptr)) {
    // Initialize module paths from YONA_PATH environment variable
    const char* yona_path = std::getenv("YONA_PATH");
    if (yona_path) {
      string path_str(yona_path);
      size_t pos = 0;
      string delimiter = ":";  // Use : on Unix, ; on Windows
      #ifdef _WIN32
        delimiter = ";";
      #endif

      while ((pos = path_str.find(delimiter)) != string::npos) {
        module_paths.push_back(path_str.substr(0, pos));
        path_str.erase(0, pos + delimiter.length());
      }
      if (!path_str.empty()) {
        module_paths.push_back(path_str);
      }
    }

    // Always include current directory
    module_paths.insert(module_paths.begin(), ".");
  }

  void push_frame() { frame = make_shared<InterepterFrame>(frame); }
  void pop_frame() { frame = frame->parent; }
  void merge_frame_to_parent() {
    frame->parent->merge(*frame);
    pop_frame();
  }

  void raise_exception(RuntimeObjectPtr exc, SourceContext ctx) {
    has_exception = true;
    exception_value = exc;
    exception_context = ctx;
  }

  void clear_exception() {
    has_exception = false;
    exception_value = nullptr;
    exception_context = EMPTY_SOURCE_LOCATION;
  }
};

class YONA_API Interpreter final : public AstVisitor {
private:
  mutable InterpreterState IS;  // mutable because visitor methods are const
  mutable optional<unique_ptr<typechecker::TypeChecker>> type_checker;  // Optional type checker
  mutable typechecker::TypeInferenceContext type_context;  // Type inference context
  mutable unordered_map<AstNode*, compiler::types::Type> type_annotations;  // Store inferred types

  template <RuntimeObjectType ROT, typename VT> optional<VT> get_value(AstNode *node) const;
  template <RuntimeObjectType ROT, typename VT, class T>
    requires derived_from<T, AstNode>
  optional<vector<VT>> get_value(const vector<T *> &nodes) const;
  template <RuntimeObjectType ROT, typename VT> optional<any> map_value(initializer_list<AstNode *> nodes, function<VT(vector<VT>)> cb) const;
  template <RuntimeObjectType actual, RuntimeObjectType... expected> static void type_error(AstNode *node);
  [[nodiscard]] bool match_fun_args(const vector<PatternNode *> &patterns, const vector<RuntimeObjectPtr> &args) const;
  RuntimeObjectPtr call(CallExpr *call_expr, vector<RuntimeObjectPtr> args) const;

  // Create an exception runtime object
  RuntimeObjectPtr make_exception(const RuntimeObjectPtr& symbol, const RuntimeObjectPtr& message) const;

  // Module loading and resolution
  string fqn_to_path(const shared_ptr<FqnValue>& fqn) const;
  string find_module_file(const string& relative_path) const;
  shared_ptr<ModuleValue> load_module(const shared_ptr<FqnValue>& fqn) const;
  shared_ptr<ModuleValue> get_or_load_module(const shared_ptr<FqnValue>& fqn) const;

  // Pattern matching helpers
  bool match_pattern(PatternNode *pattern, const RuntimeObjectPtr& value) const;
  bool match_pattern_value(PatternValue *pattern, const RuntimeObjectPtr& value) const;
  bool match_tuple_pattern(TuplePattern *pattern, const RuntimeObjectPtr& value) const;
  bool match_seq_pattern(SeqPattern *pattern, const RuntimeObjectPtr& value) const;
  bool match_dict_pattern(DictPattern *pattern, const RuntimeObjectPtr& value) const;
  bool match_record_pattern(RecordPattern *pattern, const RuntimeObjectPtr& value) const;
  bool match_as_pattern(AsDataStructurePattern *pattern, const RuntimeObjectPtr& value) const;
  bool match_head_tails_pattern(HeadTailsPattern *pattern, const RuntimeObjectPtr& value) const;
  bool match_tails_head_pattern(TailsHeadPattern *pattern, const RuntimeObjectPtr& value) const;
  bool match_head_tails_head_pattern(HeadTailsHeadPattern *pattern, const RuntimeObjectPtr& value) const;
  bool match_tail_pattern(TailPattern *pattern, const RuntimeObjectPtr& value) const;

  // Helper to create runtime objects with type information
  RuntimeObjectPtr make_typed_object(RuntimeObjectType type, RuntimeObjectData data, AstNode* node = nullptr) const;

  // Runtime type checking helpers
  bool check_runtime_type(const RuntimeObjectPtr& value, const compiler::types::Type& expected_type) const;
  compiler::types::Type runtime_type_to_static_type(RuntimeObjectType type) const;

public:
  Interpreter() = default;

  // Enable/disable type checking
  void enable_type_checking(bool enable = true);

  // Type check an AST node before interpretation
  bool type_check(AstNode* node);

  // Get type errors from last type check
  const vector<shared_ptr<yona_error>>& get_type_errors() const {
    return type_context.get_errors();
  }
  any visit(AddExpr *node) const override;
  any visit(AliasCall *node) const override;
  any visit(ApplyExpr *node) const override;
  any visit(AsDataStructurePattern *node) const override;
  any visit(BinaryNotOpExpr *node) const override;
  any visit(BitwiseAndExpr *node) const override;
  any visit(BitwiseOrExpr *node) const override;
  any visit(BitwiseXorExpr *node) const override;
  any visit(BodyWithGuards *node) const override;
  any visit(BodyWithoutGuards *node) const override;
  any visit(ByteExpr *node) const override;
  any visit(CaseExpr *node) const override;
  any visit(CaseClause *node) const override;
  any visit(CatchExpr *node) const override;
  any visit(CatchPatternExpr *node) const override;
  any visit(CharacterExpr *node) const override;
  any visit(ConsLeftExpr *node) const override;
  any visit(ConsRightExpr *node) const override;
  any visit(DictExpr *node) const override;
  any visit(DictGeneratorExpr *node) const override;
  any visit(DictGeneratorReducer *node) const override;
  any visit(DictPattern *node) const override;
  any visit(DivideExpr *node) const override;
  any visit(DoExpr *node) const override;
  any visit(EqExpr *node) const override;
  any visit(FalseLiteralExpr *node) const override;
  any visit(FieldAccessExpr *node) const override;
  any visit(FieldUpdateExpr *node) const override;
  any visit(FloatExpr *node) const override;
  any visit(FqnAlias *node) const override;
  any visit(FqnExpr *node) const override;
  any visit(FunctionAlias *node) const override;
  any visit(FunctionExpr *node) const override;
  any visit(FunctionsImport *node) const override;
  any visit(GtExpr *node) const override;
  any visit(GteExpr *node) const override;
  any visit(HeadTailsHeadPattern *node) const override;
  any visit(HeadTailsPattern *node) const override;
  any visit(IfExpr *node) const override;
  any visit(ImportClauseExpr *node) const override;
  any visit(ImportExpr *node) const override;
  any visit(InExpr *node) const override;
  any visit(IntegerExpr *node) const override;
  any visit(JoinExpr *node) const override;
  any visit(KeyValueCollectionExtractorExpr *node) const override;
  any visit(LambdaAlias *node) const override;
  any visit(LeftShiftExpr *node) const override;
  any visit(LetExpr *node) const override;
  any visit(LogicalAndExpr *node) const override;
  any visit(LogicalNotOpExpr *node) const override;
  any visit(LogicalOrExpr *node) const override;
  any visit(LtExpr *node) const override;
  any visit(LteExpr *node) const override;
  any visit(ModuloExpr *node) const override;
  any visit(ModuleAlias *node) const override;
  any visit(ModuleCall *node) const override;
  any visit(ExprCall *node) const override;
  any visit(ModuleExpr *node) const override;
  any visit(ModuleImport *node) const override;
  any visit(MultiplyExpr *node) const override;
  any visit(NameCall *node) const override;
  any visit(NameExpr *node) const override;
  any visit(NeqExpr *node) const override;
  any visit(PackageNameExpr *node) const override;
  any visit(PipeLeftExpr *node) const override;
  any visit(PipeRightExpr *node) const override;
  any visit(PatternAlias *node) const override;
  any visit(PatternExpr *node) const override;
  any visit(PatternValue *node) const override;
  any visit(PatternWithGuards *node) const override;
  any visit(PatternWithoutGuards *node) const override;
  any visit(PowerExpr *node) const override;
  any visit(RaiseExpr *node) const override;
  any visit(RangeSequenceExpr *node) const override;
  any visit(RecordInstanceExpr *node) const override;
  any visit(RecordNode *node) const override;
  any visit(RecordPattern *node) const override;
  any visit(RightShiftExpr *node) const override;
  any visit(SeqGeneratorExpr *node) const override;
  any visit(SeqPattern *node) const override;
  any visit(SetExpr *node) const override;
  any visit(SetGeneratorExpr *node) const override;
  any visit(StringExpr *node) const override;
  any visit(SubtractExpr *node) const override;
  any visit(SymbolExpr *node) const override;
  any visit(TailsHeadPattern *node) const override;
  any visit(TrueLiteralExpr *node) const override;
  any visit(TryCatchExpr *node) const override;
  any visit(TupleExpr *node) const override;
  any visit(TuplePattern *node) const override;
  any visit(UnderscoreNode *node) const override;
  any visit(UnitExpr *node) const override;
  any visit(ValueAlias *node) const override;
  any visit(ValueCollectionExtractorExpr *node) const override;
  any visit(ValuesSequenceExpr *node) const override;
  any visit(WithExpr *node) const override;
  any visit(ZerofillRightShiftExpr *node) const override;
  any visit(FunctionDeclaration *node) const override;
  any visit(TypeDeclaration *node) const override;
  any visit(TypeDefinition *node) const override;
  any visit(TypeNode *node) const override;
  any visit(TypeInstance *node) const override;
  ~Interpreter() override = default;
  any visit(ExprNode *node) const override;
  any visit(AstNode *node) const override;
  any visit(ScopedNode *node) const override;
  any visit(PatternNode *node) const override;
  any visit(ValueExpr *node) const override;
  any visit(SequenceExpr *node) const override;
  any visit(FunctionBody *node) const override;
  any visit(IdentifierExpr *node) const override;
  any visit(AliasExpr *node) const override;
  any visit(OpExpr *node) const override;
  any visit(BinaryOpExpr *node) const override;
  any visit(MainNode *node) const override;
  any visit(BuiltinTypeNode *node) const override;
  any visit(UserDefinedTypeNode *node) const override;
  any visit(TypeNameNode *node) const override;
  any visit(CallExpr *node) const override;
  any visit(GeneratorExpr *node) const override;
  any visit(CollectionExtractorExpr *node) const override;
};
} // namespace yona::interp
