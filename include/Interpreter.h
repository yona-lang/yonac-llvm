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
#include "runtime_async.h"
#include "dependency_analyzer.h"

// Disable MSVC warnings about STL types in exported class interfaces and DLL interface inheritance
// This is safe for our use case as both the DLL and clients use the same STL and compiler
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#pragma warning(disable: 4275)
#endif

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

  // Record type information for pattern matching
  unordered_map<string, shared_ptr<RecordTypeInfo>> record_types;

  // Async execution context (TODO: implement async support)
  // shared_ptr<runtime::async::AsyncContext> async_ctx;
  bool is_async_context = false;

  // Current promise if executing async
  // shared_ptr<runtime::async::Promise> current_promise;

  InterpreterState() : frame(make_shared<InterepterFrame>(nullptr)) {
                       // async_ctx(runtime::async::get_global_async_context()) {
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

    // Add stdlib directory relative to executable
    // In production, this should be determined at build time
    module_paths.push_back("./stdlib");
    module_paths.push_back("../stdlib");
    module_paths.push_back("../../stdlib");

    // Add test module directory for test execution
    module_paths.push_back("./test/code");
    module_paths.push_back("../test/code");
    module_paths.push_back("../../test/code");
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

class YONA_API Interpreter final : public AstVisitor<InterpreterResult> {
private:
  mutable InterpreterState IS;  // mutable because visitor methods are const
  mutable optional<unique_ptr<typechecker::TypeChecker>> type_checker;  // Optional type checker
  mutable typechecker::TypeInferenceContext type_context;  // Type inference context
  mutable unordered_map<AstNode*, compiler::types::Type> type_annotations;  // Store inferred types

  template <RuntimeObjectType ROT, typename VT> optional<VT> get_value(AstNode *node) const;
  template <RuntimeObjectType ROT, typename VT, class T>
    requires derived_from<T, AstNode>
  optional<vector<VT>> get_value(const vector<T *> &nodes) const;
  template <RuntimeObjectType ROT, typename VT> optional<InterpreterResult> map_value(initializer_list<AstNode *> nodes, function<VT(vector<VT>)> cb) const;
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
  bool match_or_pattern(OrPattern *pattern, const RuntimeObjectPtr& value) const;
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
  Interpreter();

  // Enable/disable type checking
  void enable_type_checking(bool enable = true);

  // Type check an AST node before interpretation
  bool type_check(AstNode* node);

  // Get type errors from last type check
  const vector<shared_ptr<yona_error>>& get_type_errors() const {
    return type_context.get_errors();
  }
  InterpreterResult visit(AddExpr *node) const override;
  InterpreterResult visit(AliasCall *node) const override;
  InterpreterResult visit(ApplyExpr *node) const override;
  InterpreterResult visit(AsDataStructurePattern *node) const override;
  InterpreterResult visit(BinaryNotOpExpr *node) const override;
  InterpreterResult visit(BitwiseAndExpr *node) const override;
  InterpreterResult visit(BitwiseOrExpr *node) const override;
  InterpreterResult visit(BitwiseXorExpr *node) const override;
  InterpreterResult visit(BodyWithGuards *node) const override;
  InterpreterResult visit(BodyWithoutGuards *node) const override;
  InterpreterResult visit(ByteExpr *node) const override;
  InterpreterResult visit(CaseExpr *node) const override;
  InterpreterResult visit(CaseClause *node) const override;
  InterpreterResult visit(CatchExpr *node) const override;
  InterpreterResult visit(CatchPatternExpr *node) const override;
  InterpreterResult visit(CharacterExpr *node) const override;
  InterpreterResult visit(ConsLeftExpr *node) const override;
  InterpreterResult visit(ConsRightExpr *node) const override;
  InterpreterResult visit(DictExpr *node) const override;
  InterpreterResult visit(DictGeneratorExpr *node) const override;
  InterpreterResult visit(DictGeneratorReducer *node) const override;
  InterpreterResult visit(DictPattern *node) const override;
  InterpreterResult visit(DivideExpr *node) const override;
  InterpreterResult visit(DoExpr *node) const override;
  InterpreterResult visit(EqExpr *node) const override;
  InterpreterResult visit(FalseLiteralExpr *node) const override;
  InterpreterResult visit(FieldAccessExpr *node) const override;
  InterpreterResult visit(FieldUpdateExpr *node) const override;
  InterpreterResult visit(FloatExpr *node) const override;
  InterpreterResult visit(FqnAlias *node) const override;
  InterpreterResult visit(FqnExpr *node) const override;
  InterpreterResult visit(FunctionAlias *node) const override;
  InterpreterResult visit(FunctionExpr *node) const override;
  InterpreterResult visit(FunctionsImport *node) const override;
  InterpreterResult visit(GtExpr *node) const override;
  InterpreterResult visit(GteExpr *node) const override;
  InterpreterResult visit(HeadTailsHeadPattern *node) const override;
  InterpreterResult visit(HeadTailsPattern *node) const override;
  InterpreterResult visit(IfExpr *node) const override;
  InterpreterResult visit(ImportClauseExpr *node) const override;
  InterpreterResult visit(ImportExpr *node) const override;
  InterpreterResult visit(InExpr *node) const override;
  InterpreterResult visit(IntegerExpr *node) const override;
  InterpreterResult visit(JoinExpr *node) const override;
  InterpreterResult visit(KeyValueCollectionExtractorExpr *node) const override;
  InterpreterResult visit(LambdaAlias *node) const override;
  InterpreterResult visit(LeftShiftExpr *node) const override;
  InterpreterResult visit(LetExpr *node) const override;
  InterpreterResult visit(LogicalAndExpr *node) const override;
  InterpreterResult visit(LogicalNotOpExpr *node) const override;
  InterpreterResult visit(LogicalOrExpr *node) const override;
  InterpreterResult visit(LtExpr *node) const override;
  InterpreterResult visit(LteExpr *node) const override;
  InterpreterResult visit(ModuloExpr *node) const override;
  InterpreterResult visit(ModuleAlias *node) const override;
  InterpreterResult visit(ModuleCall *node) const override;
  InterpreterResult visit(ExprCall *node) const override;
  InterpreterResult visit(ModuleExpr *node) const override;
  InterpreterResult visit(ModuleImport *node) const override;
  InterpreterResult visit(MultiplyExpr *node) const override;
  InterpreterResult visit(NameCall *node) const override;
  InterpreterResult visit(NameExpr *node) const override;
  InterpreterResult visit(NeqExpr *node) const override;
  InterpreterResult visit(PackageNameExpr *node) const override;
  InterpreterResult visit(PipeLeftExpr *node) const override;
  InterpreterResult visit(PipeRightExpr *node) const override;
  InterpreterResult visit(PatternAlias *node) const override;
  InterpreterResult visit(PatternExpr *node) const override;
  InterpreterResult visit(PatternValue *node) const override;
  InterpreterResult visit(PatternWithGuards *node) const override;
  InterpreterResult visit(PatternWithoutGuards *node) const override;
  InterpreterResult visit(PowerExpr *node) const override;
  InterpreterResult visit(RaiseExpr *node) const override;
  InterpreterResult visit(RangeSequenceExpr *node) const override;
  InterpreterResult visit(RecordInstanceExpr *node) const override;
  InterpreterResult visit(RecordNode *node) const override;
  InterpreterResult visit(RecordPattern *node) const override;
  InterpreterResult visit(OrPattern *node) const override;
  InterpreterResult visit(RightShiftExpr *node) const override;
  InterpreterResult visit(SeqGeneratorExpr *node) const override;
  InterpreterResult visit(SeqPattern *node) const override;
  InterpreterResult visit(SetExpr *node) const override;
  InterpreterResult visit(SetGeneratorExpr *node) const override;
  InterpreterResult visit(StringExpr *node) const override;
  InterpreterResult visit(SubtractExpr *node) const override;
  InterpreterResult visit(SymbolExpr *node) const override;
  InterpreterResult visit(TailsHeadPattern *node) const override;
  InterpreterResult visit(TrueLiteralExpr *node) const override;
  InterpreterResult visit(TryCatchExpr *node) const override;
  InterpreterResult visit(TupleExpr *node) const override;
  InterpreterResult visit(TuplePattern *node) const override;
  InterpreterResult visit(UnderscoreNode *node) const override;
  InterpreterResult visit(UnitExpr *node) const override;
  InterpreterResult visit(ValueAlias *node) const override;
  InterpreterResult visit(ValueCollectionExtractorExpr *node) const override;
  InterpreterResult visit(ValuesSequenceExpr *node) const override;
  InterpreterResult visit(WithExpr *node) const override;
  InterpreterResult visit(ZerofillRightShiftExpr *node) const override;
  InterpreterResult visit(FunctionDeclaration *node) const override;
  InterpreterResult visit(TypeDeclaration *node) const override;
  InterpreterResult visit(TypeDefinition *node) const override;
  InterpreterResult visit(TypeNode *node) const override;
  InterpreterResult visit(TypeInstance *node) const override;
  ~Interpreter() override = default;
  InterpreterResult visit(ExprNode *node) const override;
  InterpreterResult visit(AstNode *node) const override;
  InterpreterResult visit(ScopedNode *node) const override;
  InterpreterResult visit(PatternNode *node) const override;
  InterpreterResult visit(ValueExpr *node) const override;
  InterpreterResult visit(SequenceExpr *node) const override;
  InterpreterResult visit(FunctionBody *node) const override;
  InterpreterResult visit(IdentifierExpr *node) const override;
  InterpreterResult visit(AliasExpr *node) const override;
  InterpreterResult visit(OpExpr *node) const override;
  InterpreterResult visit(BinaryOpExpr *node) const override;
  InterpreterResult visit(MainNode *node) const override;
  InterpreterResult visit(BuiltinTypeNode *node) const override;
  InterpreterResult visit(UserDefinedTypeNode *node) const override;
  InterpreterResult visit(TypeNameNode *node) const override;
  InterpreterResult visit(CallExpr *node) const override;
  InterpreterResult visit(GeneratorExpr *node) const override;
  InterpreterResult visit(CollectionExtractorExpr *node) const override;

  // Async evaluation methods (TODO: implement async support)
  // shared_ptr<runtime::async::Promise> eval_async(AstNode* node) const;
  // RuntimeObjectPtr await_if_promise(RuntimeObjectPtr obj) const;
  // InterpreterResult visit_parallel_let(LetExpr* node) const;
  // InterpreterResult call_async(FunctionValue* func, const vector<RuntimeObjectPtr>& args) const;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace yona::interp
