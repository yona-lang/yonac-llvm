//
// Created by akovari on 17.11.24.
//

#include "Interpreter.h"

#include <boost/log/trivial.hpp>
#include <numeric>

#include "common.h"
#include "utils.h"

#define BINARY_OP_EXTRACTION(ROT, VT, start, offset, op)                                                                                             \
  map_value<ROT, VT>({node->left, node->right},                                                                                                      \
                     [](vector<VT> values) { return reduce(values.begin() + (offset), values.end(), (offset) ? values[0] : (start), op()); })

#define BINARY_OP(T, err, ...)                                                                                                                       \
  any Interpreter::visit(T *node) const {                                                                                                            \
    if (auto result = first_defined_optional({__VA_ARGS__}); result.has_value()) {                                                                   \
      return result.value();                                                                                                                         \
    } else {                                                                                                                                         \
      throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected " + string(err));                                      \
    }                                                                                                                                                \
  }

#define BITWISE_OP(T, ROT, VT, op)                                                                                                                   \
  struct T##_op {                                                                                                                                    \
    int operator()(const int &x, const int &y) const { return x op y; }                                                                              \
  };                                                                                                                                                 \
  BINARY_OP(T, #ROT, BINARY_OP_EXTRACTION(ROT, VT, 0, 1, T##_op))

namespace yona::interp {
using namespace std::placeholders;

template <RuntimeObjectType ROT, typename VT> optional<VT> Interpreter::get_value(AstNode *node) const {
  if (const auto runtime_object = any_cast<RuntimeObjectPtr>(visit(node)); runtime_object->type == ROT) {
    return make_optional(runtime_object->get<VT>());
  }
  return nullopt;
}

template <RuntimeObjectType ROT, typename VT, class T>
  requires derived_from<T, AstNode>
optional<vector<VT>> Interpreter::get_value(const vector<T *> &nodes) const {
  vector<VT> result;
  for (auto node : nodes) {
    if (auto node_res = get_value<ROT, VT>(node); node_res.has_value()) {
      result.push_back(node_res.value());
    } else {
      return nullopt;
    }
  }
  return optional(result);
}

template <RuntimeObjectType ROT, typename VT>
optional<any> Interpreter::map_value(const initializer_list<AstNode *> nodes, function<VT(vector<VT>)> cb) const {
  vector<VT> values;

  for (const auto node : nodes) {
    if (const auto value = get_value<ROT, VT>(node)) {
      values.push_back(value.value());
    } else {
      return nullopt;
    }
  }

  return make_shared<RuntimeObject>(ROT, cb(values));
}

template <RuntimeObjectType actual, RuntimeObjectType... expected> void Interpreter::type_error(AstNode *node) {
  static constexpr auto expected_types = {expected...};
  if (!expected_types.contains(actual)) {
    throw yona_error(node->source_context, yona_error::Type::TYPE,
                     "Type mismatch: expected " + std::string(expected_types) + ", got " + std::string(RuntimeObjectTypes[actual]));
  }
}

bool Interpreter::match_fun_args(const vector<PatternNode *> &patterns, const vector<RuntimeObjectPtr> &args) const {
  if (patterns.size() == args.size()) {
    IS.push_frame();

    for (size_t i = 0; i < patterns.size(); i++) {
      auto pattern_value = any_cast<RuntimeObjectPtr>(visit(patterns[i]));
      if (pattern_value != args[i]) {
        IS.pop_frame();
        return false;
      }
    }

    IS.merge_frame_to_parent();
    return true;
  }

  return false;
}

BINARY_OP(AddExpr, "Int or Float", BINARY_OP_EXTRACTION(Int, int, 0, 0, plus<>), BINARY_OP_EXTRACTION(Float, double, 0.0, 0, plus<>))
BINARY_OP(MultiplyExpr, "Int or Float", BINARY_OP_EXTRACTION(Int, int, 1, 0, multiplies<>), BINARY_OP_EXTRACTION(Float, double, 1.0, 0, multiplies<>))
BINARY_OP(SubtractExpr, "Int or Float", BINARY_OP_EXTRACTION(Int, int, 0, 1, minus<>), BINARY_OP_EXTRACTION(Float, double, 0.0, 1, minus<>))
BINARY_OP(DivideExpr, "Int or Float", BINARY_OP_EXTRACTION(Int, int, 0, 1, divides<>), BINARY_OP_EXTRACTION(Float, double, 0.0, 1, divides<>))

BITWISE_OP(BitwiseAndExpr, Int, int, &)
BITWISE_OP(BitwiseOrExpr, Int, int, |)
BITWISE_OP(BitwiseXorExpr, Int, int, ^)

BITWISE_OP(LeftShiftExpr, Int, int, <<)
BITWISE_OP(RightShiftExpr, Int, int, >>)
BITWISE_OP(ZerofillRightShiftExpr, Int, int,
           >>) // TODO https://stackoverflow.com/questions/8422424/can-you-control-what-a-bitwise-right-shift-will-fill-in-c
BITWISE_OP(LogicalAndExpr, Bool, bool, &&)
BITWISE_OP(LogicalOrExpr, Bool, bool, ||)

any Interpreter::visit(LogicalNotOpExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(BinaryNotOpExpr *node) const { return expr_wrapper(node); }

any Interpreter::visit(AliasCall *node) const { return expr_wrapper(node); }
any Interpreter::visit(ApplyExpr *node) const {
  auto func = get_value<Function, shared_ptr<FunctionValue>>(node->call);

  if (!func.has_value()) {
    throw yona_error(node->source_context, yona_error::Type::RUNTIME, "Invalid function call");
  }

  vector<RuntimeObjectPtr> args;
  args.reserve(node->args.size());

  for (const auto arg : node->args) {
    if (holds_alternative<ValueExpr *>(arg)) {
      args.push_back(any_cast<RuntimeObjectPtr>(visit(get<ValueExpr *>(arg))));
    } else {
      args.push_back(any_cast<RuntimeObjectPtr>(visit(get<ExprNode *>(arg))));
    }
  }

  return func->get()->code(args);
}
any Interpreter::visit(AsDataStructurePattern *node) const { return expr_wrapper(node); }

any Interpreter::visit(BodyWithGuards *node) const { return expr_wrapper(node); }
any Interpreter::visit(BodyWithoutGuards *node) const { return visit(node->expr); }
any Interpreter::visit(ByteExpr *node) const { return make_shared<RuntimeObject>(Byte, node->value); }
any Interpreter::visit(CaseExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(CatchExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(CatchPatternExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(CharacterExpr *node) const { return make_shared<RuntimeObject>(Char, node->value); }
any Interpreter::visit(ConsLeftExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(ConsRightExpr *node) const { return expr_wrapper(node); }

any Interpreter::visit(DictExpr *node) const {
  vector<pair<RuntimeObjectPtr, RuntimeObjectPtr>> fields;

  fields.reserve(node->values.size());
  for (const auto [fst, snd] : node->values) {
    fields.emplace_back(any_cast<RuntimeObjectPtr>(visit(fst)), any_cast<RuntimeObjectPtr>(visit(snd)));
  }

  return make_shared<RuntimeObject>(Dict, make_shared<DictValue>(fields));
}

any Interpreter::visit(DictGeneratorExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(DictGeneratorReducer *node) const { return expr_wrapper(node); }
any Interpreter::visit(DictPattern *node) const { return expr_wrapper(node); }

any Interpreter::visit(DoExpr *node) const {
  any result;
  for (const auto child : node->steps) {
    result = visit(child);
  }
  return result;
}

any Interpreter::visit(EqExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(FalseLiteralExpr *node) const { return make_shared<RuntimeObject>(Bool, false); }
any Interpreter::visit(FieldAccessExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(FieldUpdateExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(FloatExpr *node) const { return make_shared<RuntimeObject>(Float, node->value); }
any Interpreter::visit(FqnAlias *node) const { return expr_wrapper(node); }

any Interpreter::visit(FqnExpr *node) const {
  vector<string> fqn;
  if (node->packageName.has_value()) {
    for (const auto name : node->packageName.value()->parts) {
      fqn.push_back(name->value);
    }
  }
  fqn.push_back(node->moduleName->value);

  return make_shared<RuntimeObject>(FQN, make_shared<FqnValue>(fqn));
}

any Interpreter::visit(FunctionAlias *node) const { return expr_wrapper(node); }

any Interpreter::visit(FunctionExpr *node) const {
  function code = [this, node](const vector<RuntimeObjectPtr> &args) -> RuntimeObjectPtr {
    if (match_fun_args(node->patterns, args)) {
      IS.merge_frame_to_parent();

      for (const auto body : node->bodies) {
        if (dynamic_cast<BodyWithoutGuards *>(body)) {
          return any_cast<RuntimeObjectPtr>(visit(body));
        }

        const auto body_with_guards = dynamic_cast<BodyWithGuards *>(body);
        if (optional<bool> result = get_value<Bool, bool>(body_with_guards->guard); !result.has_value() || !result.value()) {
          continue;
        }

        return any_cast<RuntimeObjectPtr>(visit(body_with_guards->expr));
      }
    }

    return nullptr;
  };

  auto fun_value = make_shared<FunctionValue>(make_shared<FqnValue>(vector{node->name}), code);
  return make_shared<RuntimeObject>(Function, fun_value);
}

any Interpreter::visit(FunctionsImport *node) const { return expr_wrapper(node); }
any Interpreter::visit(GtExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(GteExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(HeadTailsHeadPattern *node) const { return expr_wrapper(node); }
any Interpreter::visit(HeadTailsPattern *node) const { return expr_wrapper(node); }
any Interpreter::visit(IfExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(ImportClauseExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(ImportExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(InExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(IntegerExpr *node) const { return make_shared<RuntimeObject>(Int, node->value); }
any Interpreter::visit(JoinExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(KeyValueCollectionExtractorExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(LambdaAlias *node) const { return expr_wrapper(node); }

any Interpreter::visit(LetExpr *node) const {
  for (const auto alias : node->aliases) {
    visit(alias);
  }

  return visit(node->expr);
}

any Interpreter::visit(LtExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(LteExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(ModuloExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(ModuleAlias *node) const { return expr_wrapper(node); }
any Interpreter::visit(ModuleCall *node) const { return expr_wrapper(node); }

any Interpreter::visit(ModuleExpr *node) const {
  auto fqn = get_value<FQN, shared_ptr<FqnValue>>(node->fqn);
  IS.module_stack.emplace(fqn.value(), nullptr);
  auto records = get_value<Tuple, shared_ptr<TupleValue>>(node->records);
  auto functions = get_value<Function, shared_ptr<FunctionValue>>(node->functions);

  auto module = make_shared<ModuleValue>(fqn.value(), functions.value(), records.value());

  for (const auto& function : functions.value()) {
    module->functions.push_back(function);
  }

  IS.module_stack.top().second = module;
  return make_shared<RuntimeObject>(Module, module);
}

any Interpreter::visit(ModuleImport *node) const { return expr_wrapper(node); }
any Interpreter::visit(NameCall *node) const {
  auto expr = IS.frame->lookup(node->source_context, node->name->value);

  if (expr->type != Function) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Expected a function, got " + RuntimeObjectTypes[expr->type]);
  }

  return expr;
}
any Interpreter::visit(NameExpr *node) const { return make_shared<RuntimeObject>(FQN, make_shared<FqnValue>(vector{node->value})); }
any Interpreter::visit(NeqExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(PackageNameExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(PatternAlias *node) const { return expr_wrapper(node); }
any Interpreter::visit(PatternExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(PatternValue *node) const { return expr_wrapper(node); }
any Interpreter::visit(PatternWithGuards *node) const { return expr_wrapper(node); }
any Interpreter::visit(PatternWithoutGuards *node) const { return expr_wrapper(node); }
any Interpreter::visit(PowerExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(RaiseExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(RangeSequenceExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(RecordInstanceExpr *node) const { return expr_wrapper(node); }

any Interpreter::visit(RecordNode *node) const {
  vector<RuntimeObjectPtr> fields;
  fields.reserve(node->identifiers.size());
  for (const auto [identifier, type_def] : node->identifiers) {
    fields.push_back(any_cast<RuntimeObjectPtr>(visit(identifier)));
  }
  return make_shared<RuntimeObject>(Tuple, make_shared<TupleValue>(fields));
}

any Interpreter::visit(RecordPattern *node) const { return expr_wrapper(node); }
any Interpreter::visit(SeqGeneratorExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(SeqPattern *node) const { return expr_wrapper(node); }

any Interpreter::visit(SetExpr *node) const {
  vector<RuntimeObjectPtr> fields;

  fields.reserve(node->values.size());
  for (const auto value : node->values) {
    fields.push_back(any_cast<RuntimeObjectPtr>(visit(value)));
  }

  return make_shared<RuntimeObject>(Set, make_shared<SetValue>(fields));
}

any Interpreter::visit(SetGeneratorExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(StringExpr *node) const { return make_shared<RuntimeObject>(String, node->value); }
any Interpreter::visit(SymbolExpr *node) const { return make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(node->value)); }
any Interpreter::visit(TailsHeadPattern *node) const { return expr_wrapper(node); }
any Interpreter::visit(TrueLiteralExpr *node) const { return make_shared<RuntimeObject>(Bool, true); }
any Interpreter::visit(TryCatchExpr *node) const { return expr_wrapper(node); }

any Interpreter::visit(TupleExpr *node) const {
  vector<RuntimeObjectPtr> fields;

  fields.reserve(node->values.size());
  for (const auto value : node->values) {
    fields.push_back(any_cast<RuntimeObjectPtr>(visit(value)));
  }

  return make_shared<RuntimeObject>(Tuple, make_shared<TupleValue>(fields));
}

any Interpreter::visit(TuplePattern *node) const { return expr_wrapper(node); }
any Interpreter::visit(UnderscoreNode *node) const { return make_shared<RuntimeObject>(Bool, true); }
any Interpreter::visit(UnitExpr *node) const { return make_shared<RuntimeObject>(Unit, nullptr); }

any Interpreter::visit(ValueAlias *node) const {
  auto result = visit(node->expr);
  IS.frame->write(node->identifier->name->value, result);
  return result;
}

any Interpreter::visit(ValueCollectionExtractorExpr *node) const { return expr_wrapper(node); }

any Interpreter::visit(ValuesSequenceExpr *node) const {
  vector<RuntimeObjectPtr> fields;

  fields.reserve(node->values.size());
  for (const auto value : node->values) {
    fields.push_back(any_cast<RuntimeObjectPtr>(visit(value)));
  }

  return make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(fields));
}

any Interpreter::visit(WithExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(FunctionDeclaration *node) const { return expr_wrapper(node); }
any Interpreter::visit(TypeDeclaration *node) const { return expr_wrapper(node); }
any Interpreter::visit(TypeDefinition *node) const { return expr_wrapper(node); }
any Interpreter::visit(TypeNode *node) const { return expr_wrapper(node); }
any Interpreter::visit(TypeInstance *node) const { return expr_wrapper(node); }
any Interpreter::visit(IdentifierExpr *node) const { return IS.frame->lookup(node->source_context, node->name->value); }

any Interpreter::visit(ScopedNode *node) const {
  IS.push_frame();
  auto result = AstVisitor::visit(node);
  IS.pop_frame();
  return result;
}

any Interpreter::visit(ExprNode *node) const { return AstVisitor::visit(node); }
any Interpreter::visit(AstNode *node) const { return AstVisitor::visit(node); }
any Interpreter::visit(PatternNode *node) const { return AstVisitor::visit(node); }
any Interpreter::visit(ValueExpr *node) const { return AstVisitor::visit(node); }
any Interpreter::visit(SequenceExpr *node) const { return AstVisitor::visit(node); }
any Interpreter::visit(FunctionBody *node) const { return AstVisitor::visit(node); }
any Interpreter::visit(AliasExpr *node) const { return AstVisitor::visit(node); }
any Interpreter::visit(OpExpr *node) const { return AstVisitor::visit(node); }
any Interpreter::visit(BinaryOpExpr *node) const { return AstVisitor::visit(node); }
any Interpreter::visit(TypeNameNode *node) const { return AstVisitor::visit(node); }

any Interpreter::visit(MainNode *node) const {
  IS.push_frame();
  auto result = visit(node->node);
  IS.pop_frame();
  return result;
}
any Interpreter::visit(BuiltinTypeNode *node) const { return expr_wrapper(node); }
any Interpreter::visit(UserDefinedTypeNode *node) const { return expr_wrapper(node); }

} // namespace yona::interp
