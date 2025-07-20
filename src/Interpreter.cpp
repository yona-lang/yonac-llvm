//
// Created by akovari on 17.11.24.
//

#include <numeric>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>
#include <ranges>

#include "Interpreter.h"
#include "common.h"
#include "utils.h"
#include "Parser.h"
#include "ast_visitor_impl.h"


using namespace yona::compiler::types;
using namespace yona::typechecker;

#define BINARY_OP_EXTRACTION(ROT, VT, start, offset, op)                                                                                             \
  map_value<ROT, VT>({node->left, node->right},                                                                                                      \
                     [](vector<VT> values) { return reduce(values.begin() + (offset), values.end(), (offset) ? values[0] : (start), op()); })

#define CHECK_EXCEPTION_RETURN()                                                                                                                     \
  if (IS.has_exception) return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr))

#define VISIT_WITH_EXCEPTION_CHECK(body)                                                                                                            \
  CHECK_EXCEPTION_RETURN();                                                                                                                          \
  body

#define BINARY_OP(T, err, ...)                                                                                                                       \
  InterpreterResult Interpreter::visit(T *node) const {                                                                                                            \
    CHECK_EXCEPTION_RETURN();                                                                                                                        \
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

#define COMPARISON_OP(T, op)                                                                                                                         \
  InterpreterResult Interpreter::visit(T *node) const {                                                                                                            \
    CHECK_EXCEPTION_RETURN();                                                                                                                        \
    const auto left = (node->left->accept(*this).value);                                                                                \
    CHECK_EXCEPTION_RETURN();                                                                                                                        \
    const auto right = (node->right->accept(*this).value);                                                                              \
    CHECK_EXCEPTION_RETURN();                                                                                                                        \
                                                                                                                                                     \
    if (left->type == Int && right->type == Int) {                                                                                                  \
      return InterpreterResult(make_shared<RuntimeObject>(Bool, left->get<int>() op right->get<int>()));                                                                \
    } else if (left->type == Float && right->type == Float) {                                                                                       \
      return InterpreterResult(make_shared<RuntimeObject>(Bool, left->get<double>() op right->get<double>()));                                                          \
    } else if (left->type == Int && right->type == Float) {                                                                                         \
      return InterpreterResult(make_shared<RuntimeObject>(Bool, left->get<int>() op right->get<double>()));                                                             \
    } else if (left->type == Float && right->type == Int) {                                                                                         \
      return InterpreterResult(make_shared<RuntimeObject>(Bool, left->get<double>() op right->get<int>()));                                                             \
    } else if (left->type == Byte && right->type == Byte) {                                                                                         \
      return InterpreterResult(make_shared<RuntimeObject>(Bool, static_cast<uint8_t>(left->get<std::byte>()) op static_cast<uint8_t>(right->get<std::byte>())));       \
    } else if (left->type == Byte && right->type == Int) {                                                                                          \
      return InterpreterResult(make_shared<RuntimeObject>(Bool, static_cast<int>(static_cast<uint8_t>(left->get<std::byte>())) op right->get<int>()));                  \
    } else if (left->type == Int && right->type == Byte) {                                                                                          \
      return InterpreterResult(make_shared<RuntimeObject>(Bool, left->get<int>() op static_cast<int>(static_cast<uint8_t>(right->get<std::byte>()))));                  \
    }                                                                                                                                                \
                                                                                                                                                     \
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected numeric types for comparison");                         \
  }

namespace yona::interp {
using namespace std::placeholders;

template <RuntimeObjectType ROT, typename VT> optional<VT> Interpreter::get_value(AstNode *node) const {
  const auto runtime_object = (node->accept(*this).value);

  // Check for exception after visit
  if (IS.has_exception) {
    return nullopt;
  }

  if (runtime_object->type == ROT) {
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
optional<InterpreterResult> Interpreter::map_value(const initializer_list<AstNode *> nodes, function<VT(vector<VT>)> cb) const {
  vector<VT> values;

  for (const auto node : nodes) {
    if (const auto value = get_value<ROT, VT>(node)) {
      values.push_back(value.value());
    } else {
      return nullopt;
    }
  }

  return InterpreterResult(make_shared<RuntimeObject>(ROT, cb(values)));
}

template <RuntimeObjectType actual, RuntimeObjectType... expected> void Interpreter::type_error(AstNode *node) {
  static constexpr auto expected_types = {expected...};
  if (!expected_types.contains(actual)) {
    throw yona_error(node->source_context, yona_error::Type::TYPE,
                     "Type mismatch: expected " + std::string(expected_types) + ", got " + std::string(RuntimeObjectTypes[actual]));
  }
}

bool Interpreter::match_fun_args(const vector<PatternNode *> &patterns, const vector<RuntimeObjectPtr> &args) const {
  if (patterns.size() != args.size()) {
    return false;
  }

  IS.push_frame();

  for (size_t i = 0; i < patterns.size(); i++) {
    if (!match_pattern(patterns[i], args[i])) {
      IS.pop_frame();
      return false;
    }
  }

  // Frame is kept with bindings - merge to parent
  IS.merge_frame_to_parent();
  return true;
}


RuntimeObjectPtr Interpreter::make_exception(const RuntimeObjectPtr& symbol, const RuntimeObjectPtr& message) const {
  // Create a tuple containing the exception symbol and message
  vector<RuntimeObjectPtr> fields;
  fields.push_back(symbol);
  fields.push_back(message);
  return make_shared<RuntimeObject>(Tuple, make_shared<TupleValue>(fields));
}

bool Interpreter::match_pattern(PatternNode *pattern, const RuntimeObjectPtr& value) const {
  // Pattern matching implementation
  // Returns true if the pattern matches the value, false otherwise
  // Side effect: binds variables in the current frame

  // Check the specific pattern type
  if (auto underscore = dynamic_cast<UnderscoreNode*>(pattern)) {
    // Underscore always matches
    return true;
  }

  if (auto underscore = dynamic_cast<UnderscorePattern*>(pattern)) {
    // Underscore always matches
    return true;
  }

  if (auto pattern_value = dynamic_cast<PatternValue*>(pattern)) {
    return match_pattern_value(pattern_value, value);
  }

  if (auto tuple_pattern = dynamic_cast<TuplePattern*>(pattern)) {
    return match_tuple_pattern(tuple_pattern, value);
  }

  if (auto seq_pattern = dynamic_cast<SeqPattern*>(pattern)) {
    return match_seq_pattern(seq_pattern, value);
  }

  if (auto dict_pattern = dynamic_cast<DictPattern*>(pattern)) {
    return match_dict_pattern(dict_pattern, value);
  }

  if (auto record_pattern = dynamic_cast<RecordPattern*>(pattern)) {
    return match_record_pattern(record_pattern, value);
  }

  if (auto or_pattern = dynamic_cast<OrPattern*>(pattern)) {
    return match_or_pattern(or_pattern, value);
  }

  if (auto as_pattern = dynamic_cast<AsDataStructurePattern*>(pattern)) {
    return match_as_pattern(as_pattern, value);
  }

  if (auto head_tails_pattern = dynamic_cast<HeadTailsPattern*>(pattern)) {
    return match_head_tails_pattern(head_tails_pattern, value);
  }

  if (auto tails_head_pattern = dynamic_cast<TailsHeadPattern*>(pattern)) {
    return match_tails_head_pattern(tails_head_pattern, value);
  }

  if (auto head_tails_head_pattern = dynamic_cast<HeadTailsHeadPattern*>(pattern)) {
    return match_head_tails_head_pattern(head_tails_head_pattern, value);
  }

  // Unknown pattern type
  return false;
}

bool Interpreter::match_pattern_value(PatternValue *pattern, const RuntimeObjectPtr& value) const {
  // PatternValue can contain: literal, symbol, or identifier
  return std::visit([this, &value](auto&& arg) -> bool {
    using T = decay_t<decltype(arg)>;

    if constexpr (is_same_v<T, LiteralExpr<nullptr_t>*>) {
      // Unit literal - this would be UnitExpr
      return value->type == Unit;
    } else if constexpr (is_same_v<T, LiteralExpr<void*>*>) {
      // HACK: We're using void* to smuggle in IntegerExpr and ByteExpr
      // Cast to base AstNode to check the type
      auto node = reinterpret_cast<AstNode*>(arg);

      if (node->get_type() == AST_INTEGER_EXPR) {
        auto int_expr = static_cast<IntegerExpr*>(node);
        if (value->type != Int) return false;
        return value->get<int>() == int_expr->value;
      }
      else if (node->get_type() == AST_BYTE_EXPR) {
        auto byte_expr = static_cast<ByteExpr*>(node);
        if (value->type != Byte) return false;
        return static_cast<uint8_t>(value->get<std::byte>()) == byte_expr->value;
      }
      // Handle other expression types that get evaluated
      else {
        // For other literal types, evaluate them and compare
        auto expr_node = static_cast<ExprNode*>(node);
        auto pattern_result = (expr_node->accept(*this).value);
        return *pattern_result == *value;
      }
      return false;
    } else if constexpr (is_same_v<T, SymbolExpr*>) {
      // Symbol literal
      if (value->type != Symbol) return false;
      auto symbol_result = (arg->accept(*this).value);
      return *symbol_result == *value;
    } else if constexpr (is_same_v<T, IdentifierExpr*>) {
      // Identifier - bind the value
      IS.frame->write(arg->name->value, value);
      return true;
    }
    return false;
  }, pattern->expr);
}

bool Interpreter::match_tuple_pattern(TuplePattern *pattern, const RuntimeObjectPtr& value) const {
  if (value->type != Tuple) return false;

  auto tuple_value = value->get<shared_ptr<TupleValue>>();
  if (pattern->patterns.size() != tuple_value->fields.size()) return false;

  // Match each element
  for (size_t i = 0; i < pattern->patterns.size(); i++) {
    if (!match_pattern(pattern->patterns[i], tuple_value->fields[i])) {
      return false;
    }
  }

  return true;
}

bool Interpreter::match_seq_pattern(SeqPattern *pattern, const RuntimeObjectPtr& value) const {
  if (value->type != Seq) return false;

  auto seq_value = value->get<shared_ptr<SeqValue>>();
  if (pattern->patterns.size() != seq_value->fields.size()) return false;

  // Match each element
  for (size_t i = 0; i < pattern->patterns.size(); i++) {
    if (!match_pattern(pattern->patterns[i], seq_value->fields[i])) {
      return false;
    }
  }

  return true;
}

bool Interpreter::match_dict_pattern(DictPattern *pattern, const RuntimeObjectPtr& value) const {
  if (value->type != Dict) return false;

  auto dict_value = value->get<shared_ptr<DictValue>>();

  // For each pattern key-value pair, find matching entry in dict
  for (const auto& [key_pattern, value_pattern] : pattern->keyValuePairs) {
    // Evaluate the key pattern
    auto key_result = (key_pattern->accept(*this).value);

    // Find matching key in dict
    bool found = false;
    for (const auto& [dict_key, dict_val] : dict_value->fields) {
      if (*dict_key == *key_result) {
        // Found matching key, now match the value pattern
        if (!match_pattern(value_pattern, dict_val)) {
          return false;
        }
        found = true;
        break;
      }
    }

    if (!found) {
      return false;
    }
  }

  return true;
}

bool Interpreter::match_record_pattern(RecordPattern *pattern, const RuntimeObjectPtr& value) const {
  if (value->type != Record) return false;

  auto record_value = value->get<shared_ptr<RecordValue>>();

  // Check if the record type matches
  if (record_value->type_name != pattern->recordType) {
    return false;
  }

  // Match each field pattern
  for (const auto& [field_name_expr, field_pattern] : pattern->items) {
    const string& field_name = field_name_expr->value;

    // Find the field value in the record
    auto field_value = record_value->get_field(field_name);
    if (!field_value) {
      // Field not found in record
      return false;
    }

    // Match the field pattern against the field value
    if (!match_pattern(field_pattern, field_value)) {
      return false;
    }
  }

  return true;
}

bool Interpreter::match_or_pattern(OrPattern *pattern, const RuntimeObjectPtr& value) const {
  // Try to match against each sub-pattern
  for (const auto& sub_pattern : pattern->patterns) {
    // For OR patterns, we need to be careful about variable bindings
    // Each alternative should bind the same variables
    if (match_pattern(sub_pattern.get(), value)) {
      // Successfully matched one of the patterns
      return true;
    }
  }

  return false;
}

bool Interpreter::match_as_pattern(AsDataStructurePattern *pattern, const RuntimeObjectPtr& value) const {
  // First bind the whole value to the identifier
  IS.frame->write(pattern->identifier->name->value, value);

  // Then match the inner pattern
  return match_pattern(pattern->pattern, value);
}

bool Interpreter::match_head_tails_pattern(HeadTailsPattern *pattern, const RuntimeObjectPtr& value) const {
  if (value->type != Seq) return false;

  auto seq_value = value->get<shared_ptr<SeqValue>>();

  // Check if we have enough elements for heads
  if (seq_value->fields.size() < pattern->heads.size()) return false;

  // Match head elements
  for (size_t i = 0; i < pattern->heads.size(); i++) {
    if (!match_pattern(pattern->heads[i], seq_value->fields[i])) {
      return false;
    }
  }

  // Match tail (remaining elements)
  vector<RuntimeObjectPtr> tail_elements;
  for (size_t i = pattern->heads.size(); i < seq_value->fields.size(); i++) {
    tail_elements.push_back(seq_value->fields[i]);
  }

  auto tail_seq = make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(tail_elements));
  return match_tail_pattern(pattern->tail, tail_seq);
}

bool Interpreter::match_tails_head_pattern(TailsHeadPattern *pattern, const RuntimeObjectPtr& value) const {
  if (value->type != Seq) return false;

  auto seq_value = value->get<shared_ptr<SeqValue>>();

  // Check if we have enough elements for heads
  if (seq_value->fields.size() < pattern->heads.size()) return false;

  size_t tail_end = seq_value->fields.size() - pattern->heads.size();

  // Match tail (initial elements)
  vector<RuntimeObjectPtr> tail_elements;
  for (size_t i = 0; i < tail_end; i++) {
    tail_elements.push_back(seq_value->fields[i]);
  }

  auto tail_seq = make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(tail_elements));
  if (!match_tail_pattern(pattern->tail, tail_seq)) {
    return false;
  }

  // Match head elements
  for (size_t i = 0; i < pattern->heads.size(); i++) {
    if (!match_pattern(pattern->heads[i], seq_value->fields[tail_end + i])) {
      return false;
    }
  }

  return true;
}

bool Interpreter::match_head_tails_head_pattern(HeadTailsHeadPattern *pattern, const RuntimeObjectPtr& value) const {
  if (value->type != Seq) return false;

  auto seq_value = value->get<shared_ptr<SeqValue>>();

  // Check if we have enough elements
  size_t min_size = pattern->left.size() + pattern->right.size();
  if (seq_value->fields.size() < min_size) return false;

  // Match left heads
  for (size_t i = 0; i < pattern->left.size(); i++) {
    if (!match_pattern(pattern->left[i], seq_value->fields[i])) {
      return false;
    }
  }

  // Match right heads
  size_t right_start = seq_value->fields.size() - pattern->right.size();
  for (size_t i = 0; i < pattern->right.size(); i++) {
    if (!match_pattern(pattern->right[i], seq_value->fields[right_start + i])) {
      return false;
    }
  }

  // Match tail (middle elements)
  vector<RuntimeObjectPtr> tail_elements;
  for (size_t i = pattern->left.size(); i < right_start; i++) {
    tail_elements.push_back(seq_value->fields[i]);
  }

  auto tail_seq = make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(tail_elements));
  return match_tail_pattern(pattern->tail, tail_seq);
}

bool Interpreter::match_tail_pattern(TailPattern *pattern, const RuntimeObjectPtr& value) const {
  // TailPattern can be: PatternValue (with IdentifierExpr), SequenceExpr, UnderscoreNode, StringExpr
  // BOOST_LOG_TRIVIAL(debug) << "match_tail_pattern: pattern type = " << pattern->get_type();

  // Check if it's a PatternValue containing an identifier
  if (auto pattern_value = dynamic_cast<PatternValue*>(pattern)) {
    // Check if the PatternValue contains an IdentifierExpr
    return std::visit([this, &value](auto&& expr) -> bool {
      using T = decay_t<decltype(expr)>;
      if constexpr (is_same_v<T, IdentifierExpr*>) {
        // Bind the tail value
        IS.frame->write(expr->name->value, value);
        return true;
      } else {
        // Other pattern value types - evaluate and compare
        auto pattern_result = (expr->accept(*this).value);
        return *pattern_result == *value;
      }
    }, pattern_value->expr);
  }

  if (auto identifier = dynamic_cast<IdentifierExpr*>(pattern)) {
    // Direct identifier (shouldn't happen in patterns, but handle it)
    IS.frame->write(identifier->name->value, value);
    return true;
  }

  if (dynamic_cast<UnderscoreNode*>(pattern)) {
    // Underscore always matches
    return true;
  }

  // For other cases, try to match as a pattern
  return match_pattern(pattern, value);
}

InterpreterResult Interpreter::visit(AddExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  // Handle byte promotion to int
  if (left->type == Byte || right->type == Byte) {
    int left_val = (left->type == Byte) ? static_cast<int>(static_cast<uint8_t>(left->get<std::byte>())) :
                   (left->type == Int) ? left->get<int>() :
                   throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    int right_val = (right->type == Byte) ? static_cast<int>(static_cast<uint8_t>(right->get<std::byte>())) :
                    (right->type == Int) ? right->get<int>() :
                    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    return InterpreterResult(make_shared<RuntimeObject>(Int, left_val + right_val));
  }

  // Original int/float handling
  if (auto result = first_defined_optional({
    BINARY_OP_EXTRACTION(Int, int, 0, 0, plus<>),
    BINARY_OP_EXTRACTION(Float, double, 0.0, 0, plus<>)
  }); result.has_value()) {
    return result.value();
  } else {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Float");
  }
}
InterpreterResult Interpreter::visit(MultiplyExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  // Handle byte promotion to int
  if (left->type == Byte || right->type == Byte) {
    int left_val = (left->type == Byte) ? static_cast<int>(static_cast<uint8_t>(left->get<std::byte>())) :
                   (left->type == Int) ? left->get<int>() :
                   throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    int right_val = (right->type == Byte) ? static_cast<int>(static_cast<uint8_t>(right->get<std::byte>())) :
                    (right->type == Int) ? right->get<int>() :
                    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    return InterpreterResult(make_shared<RuntimeObject>(Int, left_val * right_val));
  }

  // Original int/float handling
  if (auto result = first_defined_optional({
    BINARY_OP_EXTRACTION(Int, int, 1, 0, multiplies<>),
    BINARY_OP_EXTRACTION(Float, double, 1.0, 0, multiplies<>)
  }); result.has_value()) {
    return result.value();
  } else {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Float");
  }
}
InterpreterResult Interpreter::visit(SubtractExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  // Handle byte promotion to int
  if (left->type == Byte || right->type == Byte) {
    int left_val = (left->type == Byte) ? static_cast<int>(static_cast<uint8_t>(left->get<std::byte>())) :
                   (left->type == Int) ? left->get<int>() :
                   throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    int right_val = (right->type == Byte) ? static_cast<int>(static_cast<uint8_t>(right->get<std::byte>())) :
                    (right->type == Int) ? right->get<int>() :
                    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    return InterpreterResult(make_shared<RuntimeObject>(Int, left_val - right_val));
  }

  // Original int/float handling
  if (auto result = first_defined_optional({
    BINARY_OP_EXTRACTION(Int, int, 0, 1, minus<>),
    BINARY_OP_EXTRACTION(Float, double, 0.0, 1, minus<>)
  }); result.has_value()) {
    return result.value();
  } else {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Float");
  }
}
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

InterpreterResult Interpreter::visit(LogicalNotOpExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto operand = (node->expr->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  if (operand->type != Bool) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Bool for logical not operation");
  }

  return InterpreterResult(make_shared<RuntimeObject>(Bool, !operand->get<bool>()));
}
InterpreterResult Interpreter::visit(BinaryNotOpExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto operand = (node->expr->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  if (operand->type != Int) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int for binary not operation");
  }

  return InterpreterResult(make_shared<RuntimeObject>(Int, ~operand->get<int>()));
}

InterpreterResult Interpreter::visit(AliasCall *node) const {
  CHECK_EXCEPTION_RETURN();

  // AliasCall represents calling a function through an alias: alias.funName(args)
  // First resolve the alias to get the module or value
  auto alias_result = node->alias->accept(*this).value;
  CHECK_EXCEPTION_RETURN();

  if (alias_result->type == Module) {
    auto module_value = alias_result->get<shared_ptr<ModuleValue>>();
    string fun_name = node->funName->value;

    // Look up the function in the module's exports
    auto it = module_value->exports.find(fun_name);
    if (it == module_value->exports.end()) {
      auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"function_not_found"}));
      auto message = make_shared<RuntimeObject>(String, string("Function '" + fun_name + "' not found in module"));
      auto exception = make_exception(symbol, message);
      IS.raise_exception(exception, node->source_context);
      return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
    }

    return InterpreterResult(make_shared<RuntimeObject>(Function, it->second));
  } else {
    // Not a module - raise error
    auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"type_error"}));
    auto message = make_shared<RuntimeObject>(String, string("Cannot call function on non-module value"));
    auto exception = make_exception(symbol, message);
    IS.raise_exception(exception, node->source_context);
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }
}
InterpreterResult Interpreter::visit(ApplyExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "ApplyExpr: Starting function application";

  // First visit the call expression to get the function
  // BOOST_LOG_TRIVIAL(debug) << "ApplyExpr: About to visit call expression of type: " << node->call->get_type();
  // BOOST_LOG_TRIVIAL(debug) << "ApplyExpr: Call expr is " << typeid(*node->call).name();
  auto call_result = node->call->accept(*this);
  CHECK_EXCEPTION_RETURN();

  // BOOST_LOG_TRIVIAL(debug) << "ApplyExpr: Call expression visited, result type: " << (call_result).value->type;

  // Check if it's a function
  auto func_obj = (call_result).value;
  if (func_obj->type != Function) {
    std::cerr << "ApplyExpr: Call expression is not a function, type=" << RuntimeObjectTypes[func_obj->type] << std::endl;
    throw yona_error(node->source_context, yona_error::Type::RUNTIME, "Invalid function call - not a function");
  }

  auto func_val = func_obj->get<shared_ptr<FunctionValue>>();
  // BOOST_LOG_TRIVIAL(debug) << "ApplyExpr: Function arity=" << func_val->arity
  //                          << ", partial_args=" << func_val->partial_args.size()
  //                          << ", new_args=" << node->args.size();

  // Evaluate arguments
  vector<RuntimeObjectPtr> new_args;
  new_args.reserve(node->args.size());

  for (const auto arg : node->args) {
    if (holds_alternative<ValueExpr *>(arg)) {
      new_args.push_back(get<ValueExpr *>(arg)->accept(*this).value);
    } else {
      new_args.push_back(get<ExprNode *>(arg)->accept(*this).value);
    }
    CHECK_EXCEPTION_RETURN();
  }

  // Handle named arguments for record constructors
  if (node->named_args.has_value() && !node->named_args->empty()) {
    // Check if this is a record constructor call
    if (auto name_call = dynamic_cast<NameCall*>(node->call)) {
      string func_name = name_call->name->value;

      // Check if this is a record constructor
      auto record_it = IS.record_types.find(func_name);
      if (record_it != IS.record_types.end()) {
        auto record_info = record_it->second;

        // Create a map from field names to values
        unordered_map<string, RuntimeObjectPtr> named_values;
        for (const auto& [name, arg] : *node->named_args) {
          RuntimeObjectPtr value;
          if (holds_alternative<ValueExpr *>(arg)) {
            value = get<ValueExpr *>(arg)->accept(*this).value;
          } else {
            value = get<ExprNode *>(arg)->accept(*this).value;
          }
          CHECK_EXCEPTION_RETURN();
          named_values[name] = value;
        }

        // Reorder arguments based on field order
        new_args.clear();
        for (const auto& field_name : record_info->field_names) {
          auto it = named_values.find(field_name);
          if (it == named_values.end()) {
            throw yona_error(node->source_context, yona_error::RUNTIME,
              "Missing field '" + field_name + "' in record construction");
          }
          new_args.push_back(it->second);
        }

        // Check for extra fields
        if (named_values.size() > record_info->field_names.size()) {
          throw yona_error(node->source_context, yona_error::RUNTIME,
            "Too many fields provided for record " + func_name);
        }
      }
    }
  }

  // Combine with any partial args
  vector<RuntimeObjectPtr> all_args;
  all_args.reserve(func_val->partial_args.size() + new_args.size());
  all_args.insert(all_args.end(), func_val->partial_args.begin(), func_val->partial_args.end());
  all_args.insert(all_args.end(), new_args.begin(), new_args.end());

  // Check if we have enough arguments
  if (all_args.size() < func_val->arity) {
    // BOOST_LOG_TRIVIAL(debug) << "ApplyExpr: Creating partial application - have "
    //                          << all_args.size() << " args, need " << func_val->arity;
    // Create a partially applied function
    auto partial_func = make_shared<FunctionValue>();
    partial_func->fqn = func_val->fqn;
    partial_func->arity = func_val->arity - all_args.size();  // Remaining arity!
    partial_func->partial_args = {};  // Don't store args here since we'll capture them in the closure
    partial_func->type = func_val->type;  // Preserve type information

    // Create a new code function that will apply the remaining args
    auto original_code = func_val->code;
    partial_func->code = [original_code, all_args](const vector<RuntimeObjectPtr>& more_args) -> RuntimeObjectPtr {
      // BOOST_LOG_TRIVIAL(debug) << "Partial function: Executing with " << more_args.size()
      //                          << " more args (already have " << all_args.size() << ")";

      // Combine stored args with new args
      vector<RuntimeObjectPtr> combined_args;
      combined_args.reserve(all_args.size() + more_args.size());
      combined_args.insert(combined_args.end(), all_args.begin(), all_args.end());
      combined_args.insert(combined_args.end(), more_args.begin(), more_args.end());

      return original_code(combined_args);
    };

    return InterpreterResult(make_typed_object(Function, partial_func, node));
  } else if (all_args.size() == func_val->arity) {
    // Exact number of arguments - apply the function

    // Perform runtime type checking if types are available
    if (func_val->type.has_value()) {
      // Get the function type
      auto func_type_variant = func_val->type.value();

      // Check if it's a function type
      if (holds_alternative<shared_ptr<compiler::types::FunctionType>>(func_type_variant)) {
        auto func_type = get<shared_ptr<compiler::types::FunctionType>>(func_type_variant);

        // For now, do basic runtime type validation
        // In a full implementation, we would check that each argument matches the expected type
        // This would require:
        // 1. Decomposing the argument type (which may be a product type for multiple args)
        // 2. Checking each runtime value against its expected type
        // 3. Supporting type inference and polymorphic types

        // Basic validation: ensure we have the right number of arguments
        if (all_args.size() != func_val->arity) {
          throw yona_error(node->source_context, yona_error::Type::TYPE,
                          "Wrong number of arguments: expected " +
                          to_string(func_val->arity) + ", got " +
                          to_string(all_args.size()));
        }
      }
    }

    return InterpreterResult(func_val->code(all_args));
  } else {
    // Too many arguments - apply function and then apply result to remaining args
    // First, take only the needed arguments
    vector<RuntimeObjectPtr> needed_args(all_args.begin(), all_args.begin() + func_val->arity);
    auto result = func_val->code(needed_args);

    // If there are remaining arguments, try to apply them to the result
    if (all_args.size() > func_val->arity) {
      vector<RuntimeObjectPtr> remaining_args(all_args.begin() + func_val->arity, all_args.end());

      // Create a new ApplyExpr node to apply remaining args
      // For now, throw an error as we'd need to create AST nodes dynamically
      throw yona_error(node->source_context, yona_error::Type::RUNTIME,
                      "Too many arguments provided: expected " + to_string(func_val->arity) +
                      ", got " + to_string(all_args.size()));
    }

    return InterpreterResult(result);
  }
}
InterpreterResult Interpreter::visit(AsDataStructurePattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // AsDataStructurePattern is used in pattern matching contexts, not for evaluation
  // It binds a value to an identifier and then matches against a pattern
  // This should not be visited directly
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}

InterpreterResult Interpreter::visit(BodyWithGuards *node) const {
  CHECK_EXCEPTION_RETURN();

  // BodyWithGuards represents a function body with a guard condition
  // First evaluate the guard expression
  auto guard_result = node->guard->accept(*this).value;
  CHECK_EXCEPTION_RETURN();

  // Check if the guard evaluates to true
  if (guard_result->type != Bool) {
    auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"type_error"}));
    auto message = make_shared<RuntimeObject>(String, string("Guard expression must evaluate to boolean"));
    auto exception = make_exception(symbol, message);
    IS.raise_exception(exception, node->source_context);
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }

  bool guard_value = guard_result->get<bool>();
  if (guard_value) {
    // Guard passed, evaluate the body expression
    return node->expr->accept(*this);
  } else {
    // Guard failed, return a special value indicating guard failure
    auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"guard_failed"}));
    auto message = make_shared<RuntimeObject>(String, string("Function guard condition failed"));
    auto exception = make_exception(symbol, message);
    IS.raise_exception(exception, node->source_context);
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }
}
InterpreterResult Interpreter::visit(BodyWithoutGuards *node) const {
  CHECK_EXCEPTION_RETURN();
  return node->expr->accept(*this);
}
InterpreterResult Interpreter::visit(ByteExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_typed_object(Byte, static_cast<std::byte>(node->value), node));
}
InterpreterResult Interpreter::visit(CaseExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // Evaluate the expression to match against
  auto match_value = (node->expr->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  // Try each clause in order
  for (auto* clause : node->clauses) {
    // Create a new frame for pattern bindings
    IS.push_frame();

    // Try to match the pattern
    if (match_pattern(clause->pattern, match_value)) {
      // Pattern matched - evaluate the body and return
      auto result = clause->body->accept(*this);
      IS.merge_frame_to_parent();
      return result;
    } else {
      // Pattern didn't match - discard frame and try next
      IS.pop_frame();
    }

    // Check for exceptions
    CHECK_EXCEPTION_RETURN();
  }

  // No pattern matched - raise :nomatch exception
  auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"nomatch"}));
  auto message = make_shared<RuntimeObject>(String, string("No pattern matched in case expression"));
  auto exception = make_exception(symbol, message);
  IS.raise_exception(exception, node->source_context);
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}

InterpreterResult Interpreter::visit(CaseClause *node) const {
  // This method should not be called directly - CaseClause is handled within CaseExpr
  throw yona_error(node->source_context, yona_error::Type::RUNTIME, "CaseClause should not be visited directly");
}

InterpreterResult Interpreter::visit(CatchExpr *node) const {
  // Don't check for exceptions here - we're in a catch block
  // For now, execute the first catch pattern
  // In a full implementation, we would match patterns against the exception
  if (!node->patterns.empty()) {
    auto result = node->patterns[0]->accept(*this);
    // Don't check for exceptions - let them propagate
    return result;
  }
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(CatchPatternExpr *node) const {
  // This is called from CatchExpr, but the actual pattern matching
  // and execution is now handled in TryCatchExpr visitor
  // This method should not be called directly anymore
  throw yona_error(node->source_context, yona_error::Type::RUNTIME,
                  "CatchPatternExpr should not be visited directly");
}
InterpreterResult Interpreter::visit(CharacterExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_typed_object(Char, node->value, node));
}
InterpreterResult Interpreter::visit(ConsLeftExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  if (right->type != Seq) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Seq on right side of cons");
  }

  const auto seq = right->get<shared_ptr<SeqValue>>();
  vector<RuntimeObjectPtr> new_fields;
  new_fields.push_back(left);
  new_fields.insert(new_fields.end(), seq->fields.begin(), seq->fields.end());

  return InterpreterResult(make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(new_fields)));
}
InterpreterResult Interpreter::visit(ConsRightExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  const auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  const auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  if (left->type != Seq) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Seq on left side of cons");
  }

  auto seq = left->get<shared_ptr<SeqValue>>();
  vector<RuntimeObjectPtr> new_fields(seq->fields);
  new_fields.push_back(right);

  return InterpreterResult(make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(new_fields)));
}

InterpreterResult Interpreter::visit(DictExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  vector<pair<RuntimeObjectPtr, RuntimeObjectPtr>> fields;

  fields.reserve(node->values.size());
  for (const auto [fst, snd] : node->values) {
    auto key = (fst->accept(*this).value);
    CHECK_EXCEPTION_RETURN();
    auto value = (snd->accept(*this).value);
    CHECK_EXCEPTION_RETURN();
    fields.emplace_back(key, value);
  }

  return InterpreterResult(make_shared<RuntimeObject>(Dict, make_shared<DictValue>(fields)));
}

InterpreterResult Interpreter::visit(DictGeneratorExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Evaluate the source collection
  auto source = (node->stepExpression->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  // Ensure source is a collection (Seq, Set, or Dict)
  if (source->type != Seq && source->type != Set && source->type != Dict) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected collection for generator source");
  }

  vector<pair<RuntimeObjectPtr, RuntimeObjectPtr>> result_fields;

  if (source->type == Seq) {
    auto source_seq = source->get<shared_ptr<SeqValue>>();
    for (const auto& elem : source_seq->fields) {
      IS.push_frame();
      IS.generator_current_element = elem;
      node->collectionExtractor->accept(*this);
      CHECK_EXCEPTION_RETURN();
      // For dict generator, evaluate the reducer to get key and value
      auto key = (node->reducerExpr->key->accept(*this).value);
      CHECK_EXCEPTION_RETURN();
      auto value = (node->reducerExpr->value->accept(*this).value);
      CHECK_EXCEPTION_RETURN();
      result_fields.emplace_back(key, value);
      IS.pop_frame();
    }
  } else if (source->type == Set) {
    auto source_set = source->get<shared_ptr<SetValue>>();
    for (const auto& elem : source_set->fields) {
      IS.push_frame();
      IS.generator_current_element = elem;
      node->collectionExtractor->accept(*this);
      CHECK_EXCEPTION_RETURN();
      auto key = (node->reducerExpr->key->accept(*this).value);
      CHECK_EXCEPTION_RETURN();
      auto value = (node->reducerExpr->value->accept(*this).value);
      CHECK_EXCEPTION_RETURN();
      result_fields.emplace_back(key, value);
      IS.pop_frame();
    }
  } else if (source->type == Dict) {
    auto source_dict = source->get<shared_ptr<DictValue>>();
    for (const auto& [key, value] : source_dict->fields) {
      IS.push_frame();
      // For dict source, we need to pass both key and value
      IS.generator_current_key = key;
      IS.generator_current_element = value;
      node->collectionExtractor->accept(*this);
      CHECK_EXCEPTION_RETURN();
      auto new_key = (node->reducerExpr->key->accept(*this).value);
      CHECK_EXCEPTION_RETURN();
      auto new_value = (node->reducerExpr->value->accept(*this).value);
      CHECK_EXCEPTION_RETURN();
      result_fields.emplace_back(new_key, new_value);
      IS.pop_frame();
    }
  }

  return InterpreterResult(make_shared<RuntimeObject>(Dict, make_shared<DictValue>(result_fields)));
}
InterpreterResult Interpreter::visit(DictGeneratorReducer *node) const {
  // This node is not visited directly during generation
  // Instead, its key and value expressions are evaluated in DictGeneratorExpr
  throw yona_error(node->source_context, yona_error::Type::RUNTIME, "DictGeneratorReducer should not be visited directly");
}
InterpreterResult Interpreter::visit(DictPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // DictPattern is used in pattern matching contexts, not for evaluation
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}

InterpreterResult Interpreter::visit(DoExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  InterpreterResult result;
  for (const auto child : node->steps) {
    result = child->accept(*this);
    CHECK_EXCEPTION_RETURN();
  }
  return result;
}

InterpreterResult Interpreter::visit(EqExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  const auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  const auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  return InterpreterResult(make_shared<RuntimeObject>(Bool, *left == *right));
}
InterpreterResult Interpreter::visit(FalseLiteralExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_typed_object(Bool, false, node));
}
InterpreterResult Interpreter::visit(FieldAccessExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // First evaluate the identifier expression to get the record
  auto record_result = node->identifier->accept(*this);
  CHECK_EXCEPTION_RETURN();

  auto record_obj = record_result.value;
  if (!record_obj || record_obj->type != Record) {
    throw yona_error(node->source_context, yona_error::Type::TYPE,
                    "Field access on non-record value");
  }

  // Get the record value
  auto record_value = record_obj->get<shared_ptr<RecordValue>>();
  if (!record_value) {
    throw yona_error(node->source_context, yona_error::Type::TYPE,
                    "Invalid record value");
  }

  // Get the field name
  const string& field_name = node->name->value;

  // Get the field value
  auto field_value = record_value->get_field(field_name);
  if (!field_value) {
    throw yona_error(node->source_context, yona_error::Type::RUNTIME,
                    "Field '" + field_name + "' not found in record");
  }

  return InterpreterResult(field_value);
}
InterpreterResult Interpreter::visit(FieldUpdateExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // FieldUpdateExpr updates fields in a record: record{field1: new_value, field2: new_value}
  // First evaluate the identifier to get the original record
  auto record_result = node->identifier->accept(*this).value;
  CHECK_EXCEPTION_RETURN();

  if (record_result->type != Record) {
    auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"type_error"}));
    auto message = make_shared<RuntimeObject>(String, string("Cannot update fields on non-record value"));
    auto exception = make_exception(symbol, message);
    IS.raise_exception(exception, node->source_context);
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }

  // Get the original record value
  auto original_record = record_result->get<shared_ptr<RecordValue>>();

  // Create a copy of the record for updating
  auto updated_record = make_shared<RecordValue>(*original_record);

  // Apply each field update
  for (const auto& update : node->updates) {
    string field_name = update.first->value;
    auto new_value = update.second->accept(*this).value;
    CHECK_EXCEPTION_RETURN();

    // Update the field in the record
    if (!updated_record->set_field(field_name, new_value)) {
      auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"field_not_found"}));
      auto message = make_shared<RuntimeObject>(String, string("Field '" + field_name + "' not found in record"));
      auto exception = make_exception(symbol, message);
      IS.raise_exception(exception, node->source_context);
      return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
    }
  }

  // Return the updated record
  return InterpreterResult(make_shared<RuntimeObject>(Record, updated_record));
}
InterpreterResult Interpreter::visit(FloatExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_typed_object(Float, node->value, node));
}
InterpreterResult Interpreter::visit(FqnAlias *node) const {
  CHECK_EXCEPTION_RETURN();

  // FqnAlias binds a fully-qualified name to a local name: name = Pkg\Module
  // Evaluate the FQN to get a module
  auto fqn_result = node->fqn->accept(*this).value;
  CHECK_EXCEPTION_RETURN();

  // Bind the result to the local name in the current frame
  string alias_name = node->name->value;
  IS.frame->write(alias_name, fqn_result);

  // Return the bound value
  return InterpreterResult(fqn_result);
}

InterpreterResult Interpreter::visit(FqnExpr *node) const {
  vector<string> fqn;
  if (node->packageName.has_value()) {
    for (const auto name : node->packageName.value()->parts) {
      fqn.push_back(name->value);
    }
  }
  fqn.push_back(node->moduleName->value);

  return InterpreterResult(make_shared<RuntimeObject>(FQN, make_shared<FqnValue>(fqn)));
}

InterpreterResult Interpreter::visit(FunctionAlias *node) const {
  CHECK_EXCEPTION_RETURN();

  // FunctionAlias creates a local alias for a function: name = alias
  // Look up the alias function in the current frame
  RuntimeObjectPtr alias_result;
  try {
    alias_result = IS.frame->lookup(node->source_context, node->alias->value);
  } catch (const yona_error& e) {
    // Variable not found
    auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"undefined_variable"}));
    auto message = make_shared<RuntimeObject>(String, string("Undefined variable: " + node->alias->value));
    auto exception = make_exception(symbol, message);
    IS.raise_exception(exception, node->source_context);
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }

  // Bind the function to the new name
  string new_name = node->name->value;
  IS.frame->write(new_name, alias_result);

  // Return the aliased function
  return InterpreterResult(alias_result);
}

InterpreterResult Interpreter::visit(FunctionExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Determine arity from patterns
  size_t arity = node->patterns.size();
  // BOOST_LOG_TRIVIAL(debug) << "FunctionExpr: Creating function with arity=" << arity
  //                          << ", name=" << (node->name.empty() ? "<lambda>" : node->name);

  function code = [this, node](const vector<RuntimeObjectPtr> &args) -> RuntimeObjectPtr {
    // BOOST_LOG_TRIVIAL(debug) << "FunctionExpr: Executing function with " << args.size() << " args";

    if (match_fun_args(node->patterns, args)) {
      // BOOST_LOG_TRIVIAL(debug) << "FunctionExpr: Arguments matched patterns";
      // match_fun_args pushes a frame and merges it if successful

      for (const auto body : node->bodies) {
        // BOOST_LOG_TRIVIAL(debug) << "FunctionExpr: Processing body";
        if (dynamic_cast<BodyWithoutGuards *>(body)) {
          // BOOST_LOG_TRIVIAL(debug) << "FunctionExpr: Executing body without guards";
          auto result = body->accept(*this);
          // BOOST_LOG_TRIVIAL(debug) << "FunctionExpr: Body execution complete";
          return (result).value;
        }

        const auto body_with_guards = dynamic_cast<BodyWithGuards *>(body);
        if (optional<bool> guard_result = get_value<Bool, bool>(body_with_guards->guard); !guard_result.has_value() || !guard_result.value()) {
          continue;
        }

        return (body_with_guards->expr->accept(*this).value);
      }
    } else {
      // BOOST_LOG_TRIVIAL(debug) << "FunctionExpr: Arguments did not match patterns";
    }

    return nullptr;
  };

  auto fun_value = make_shared<FunctionValue>();
  // For anonymous functions (lambdas), use a placeholder name
  if (node->name.empty()) {
    fun_value->fqn = make_shared<FqnValue>(vector{string("<lambda>")});
  } else {
    fun_value->fqn = make_shared<FqnValue>(vector{node->name});
  }
  fun_value->code = code;
  fun_value->arity = arity;
  fun_value->partial_args = {};  // No partial args for new function

  return InterpreterResult(make_typed_object(Function, fun_value, node));
}

InterpreterResult Interpreter::visit(FunctionsImport *node) const {
  CHECK_EXCEPTION_RETURN();

  // Get the FQN of the module to import from
  auto fqn = get_value<FQN, shared_ptr<FqnValue>>(node->fromFqn);
  CHECK_EXCEPTION_RETURN();

  if (!fqn.has_value()) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Invalid module FQN");
  }

  // Load or get the module
  auto module = get_or_load_module(fqn.value());

  // Import specific functions
  for (const auto& alias : node->aliases) {
    const string& func_name = alias->name->value;
    auto func = module->get_export(func_name);
    if (!func) {
      throw yona_error(node->source_context, yona_error::Type::RUNTIME,
                       "Function '" + func_name + "' not exported from module");
    }

    // Use alias if provided, otherwise use original name
    const string& import_name = alias->alias ? alias->alias->value : func_name;
    IS.frame->write(import_name, make_shared<RuntimeObject>(Function, func));

    // Debug: Verify the function was written
    // std::cout << "Imported function '" << import_name << "' to frame" << std::endl;
  }

  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
COMPARISON_OP(GtExpr, >)
COMPARISON_OP(GteExpr, >=)
InterpreterResult Interpreter::visit(HeadTailsHeadPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // HeadTailsHeadPattern is used in pattern matching contexts, not for evaluation
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(HeadTailsPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // HeadTailsPattern is used in pattern matching contexts, not for evaluation
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(IfExpr *node) const {
  auto condition = (node->condition->accept(*this).value);

  if (condition->type != Bool) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Bool for if condition");
  }

  if (condition->get<bool>()) {
    return node->thenExpr->accept(*this);
  } else {
    return node->elseExpr->accept(*this);
  }
}
InterpreterResult Interpreter::visit(ImportClauseExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // ImportClauseExpr is a base class - the actual import is in derived classes
  // This will be called via dynamic dispatch to ModuleImport or FunctionsImport
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(ImportExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // Create a new frame for the imported bindings
  auto saved_frame = IS.frame;
  IS.frame = make_shared<InterepterFrame>(saved_frame);

  // Process all import clauses
  for (auto clause : node->clauses) {
    clause->accept(*this);
    CHECK_EXCEPTION_RETURN();
  }

  // Visit the expression that uses the imported functions
  InterpreterResult result(make_shared<RuntimeObject>(Unit, nullptr));
  if (node->expr) {
    result = node->expr->accept(*this);
  }

  // Restore the previous frame
  IS.frame = saved_frame;

  return result;
}
InterpreterResult Interpreter::visit(InExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  if (right->type == Seq) {
    auto seq = right->get<shared_ptr<SeqValue>>();
    for (const auto& elem : seq->fields) {
      if (*elem == *left) {
        return InterpreterResult(make_shared<RuntimeObject>(Bool, true));
      }
    }
    return InterpreterResult(make_shared<RuntimeObject>(Bool, false));
  } else if (right->type == Set) {
    auto set = right->get<shared_ptr<SetValue>>();
    for (const auto& elem : set->fields) {
      if (*elem == *left) {
        return InterpreterResult(make_shared<RuntimeObject>(Bool, true));
      }
    }
    return InterpreterResult(make_shared<RuntimeObject>(Bool, false));
  } else if (right->type == Dict) {
    auto dict = right->get<shared_ptr<DictValue>>();
    for (const auto& [key, value] : dict->fields) {
      if (*key == *left) {
        return InterpreterResult(make_shared<RuntimeObject>(Bool, true));
      }
    }
    return InterpreterResult(make_shared<RuntimeObject>(Bool, false));
  }

  throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected collection type for 'in' operation");
}
InterpreterResult Interpreter::visit(IntegerExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_typed_object(Int, node->value, node));
}
InterpreterResult Interpreter::visit(JoinExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  if (left->type != Seq || right->type != Seq) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Seq for join operation");
  }

  auto left_seq = left->get<shared_ptr<SeqValue>>();
  auto right_seq = right->get<shared_ptr<SeqValue>>();

  vector<RuntimeObjectPtr> new_fields(left_seq->fields);
  new_fields.insert(new_fields.end(), right_seq->fields.begin(), right_seq->fields.end());

  return InterpreterResult(make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(new_fields)));
}
InterpreterResult Interpreter::visit(KeyValueCollectionExtractorExpr *node) const {
  // Get the current key and value from the generator context
  auto key = IS.generator_current_key;
  auto value = IS.generator_current_element;

  if (holds_alternative<IdentifierExpr*>(node->keyExpr)) {
    // Bind the key to the identifier
    auto identifier = get<IdentifierExpr*>(node->keyExpr);
    IS.frame->write(identifier->name->value, key);
  }
  // If it's an underscore, we don't bind anything

  if (holds_alternative<IdentifierExpr*>(node->valueExpr)) {
    // Bind the value to the identifier
    auto identifier = get<IdentifierExpr*>(node->valueExpr);
    IS.frame->write(identifier->name->value, value);
  }
  // If it's an underscore, we don't bind anything

  return InterpreterResult(value);
}
InterpreterResult Interpreter::visit(LambdaAlias *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "LambdaAlias: Creating lambda for name=" << node->name->value;

  // Evaluate the lambda expression
  auto lambda_result = node->lambda->accept(*this);
  CHECK_EXCEPTION_RETURN();

  // Bind the function to the name in the current frame
  IS.frame->write(node->name->value, lambda_result.value);

  // BOOST_LOG_TRIVIAL(debug) << "LambdaAlias: Lambda bound to " << node->name->value;
  return lambda_result;
}

InterpreterResult Interpreter::visit(LetExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  for (const auto alias : node->aliases) {
    alias->accept(*this);
    CHECK_EXCEPTION_RETURN();
  }

  return node->expr->accept(*this);
}

COMPARISON_OP(LtExpr, <)
COMPARISON_OP(LteExpr, <=)
InterpreterResult Interpreter::visit(ModuloExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  if (left->type == Int && right->type == Int) {
    int divisor = right->get<int>();
    if (divisor == 0) {
      throw yona_error(node->source_context, yona_error::Type::RUNTIME, "Division by zero");
    }
    return InterpreterResult(make_shared<RuntimeObject>(Int, left->get<int>() % divisor));
  }

  throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int for modulo operation");
}
InterpreterResult Interpreter::visit(ModuleAlias *node) const {
  CHECK_EXCEPTION_RETURN();

  // ModuleAlias binds a module to a local name: name = module
  // Evaluate the module expression
  auto module_result = node->module->accept(*this).value;
  CHECK_EXCEPTION_RETURN();

  // Bind the module to the local name
  string alias_name = node->name->value;
  IS.frame->write(alias_name, module_result);

  // Return the module
  return InterpreterResult(module_result);
}
InterpreterResult Interpreter::visit(ModuleCall *node) const {
  CHECK_EXCEPTION_RETURN();

  // ModuleCall calls a function from a module: Module\Function(args)
  // First resolve the module from the FQN or expression
  RuntimeObjectPtr module_result;

  if (holds_alternative<FqnExpr*>(node->fqn)) {
    // FQN case: direct module reference
    auto fqn_expr = get<FqnExpr*>(node->fqn);
    module_result = fqn_expr->accept(*this).value;
  } else {
    // Expression case: evaluate expression to get module
    auto expr = get<ExprNode*>(node->fqn);
    module_result = expr->accept(*this).value;
  }

  CHECK_EXCEPTION_RETURN();

  if (module_result->type != Module) {
    auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"type_error"}));
    auto message = make_shared<RuntimeObject>(String, string("Cannot call function on non-module value"));
    auto exception = make_exception(symbol, message);
    IS.raise_exception(exception, node->source_context);
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }

  // Get the module and look up the function
  auto module_value = module_result->get<shared_ptr<ModuleValue>>();
  string fun_name = node->funName->value;

  auto it = module_value->exports.find(fun_name);
  if (it == module_value->exports.end()) {
    auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"function_not_found"}));
    auto message = make_shared<RuntimeObject>(String, string("Function '" + fun_name + "' not found in module"));
    auto exception = make_exception(symbol, message);
    IS.raise_exception(exception, node->source_context);
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }

  // Return the function for later application
  return InterpreterResult(make_shared<RuntimeObject>(Function, it->second));
}
InterpreterResult Interpreter::visit(ExprCall *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "ExprCall: Evaluating expression for call";
  // BOOST_LOG_TRIVIAL(debug) << "ExprCall: Expression type = " << node->expr->get_type();

  // ExprCall handles general expression calls (e.g., (lambda)(args) or curried(args))
  // Simply evaluate the expression - it should return a function
  auto result = node->expr->accept(*this);
  // BOOST_LOG_TRIVIAL(debug) << "ExprCall: Expression evaluated, type="
  //                          << (result.has_value() ? to_string((result).value->type) : "no value");
  return result;
}

InterpreterResult Interpreter::visit(ModuleExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto fqn = get_value<FQN, shared_ptr<FqnValue>>(node->fqn);
  CHECK_EXCEPTION_RETURN();
  if (!fqn.has_value()) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Invalid module FQN");
  }
  IS.module_stack.emplace(fqn.value(), nullptr);

  // Create a new module
  auto module = make_shared<ModuleValue>();
  module->fqn = fqn.value();

  // Process records
  for (auto record : node->records) {
    auto record_result = (record->accept(*this).value);
    CHECK_EXCEPTION_RETURN();
    if (record_result->type == Tuple) {
      // TODO: Store record type information in module->record_types
      // For now, we're not storing runtime record instances in the module
    }
  }

  // Process functions and populate exports
  for (auto func : node->functions) {
    auto func_result = (func->accept(*this).value);
    CHECK_EXCEPTION_RETURN();
    if (func_result->type == Function) {
      auto func_value = func_result->get<shared_ptr<FunctionValue>>();

      // Add to exports if it's in the export list
      const string& func_name = func->name;
      if (find(node->exports.begin(), node->exports.end(), func_name) != node->exports.end()) {
        module->exports[func_name] = func_value;
      }
    }
  }

  IS.module_stack.top().second = module;
  return InterpreterResult(make_shared<RuntimeObject>(Module, module));
}

InterpreterResult Interpreter::visit(ModuleImport *node) const {
  CHECK_EXCEPTION_RETURN();

  // Get the FQN of the module to import
  auto fqn = get_value<FQN, shared_ptr<FqnValue>>(node->fqn);
  CHECK_EXCEPTION_RETURN();

  if (!fqn.has_value()) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Invalid module FQN");
  }

  // Load or get the module
  auto module = get_or_load_module(fqn.value());

  // Import all exported functions from the module
  for (const auto& [name, func] : module->exports) {
    IS.frame->write(name, make_shared<RuntimeObject>(Function, func));
  }

  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(NameCall *node) const {
  CHECK_EXCEPTION_RETURN();
  auto expr = IS.frame->lookup(node->source_context, node->name->value);

  if (expr->type != Function) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Expected a function, got " + RuntimeObjectTypes[expr->type]);
  }

  return InterpreterResult(expr);
}
InterpreterResult Interpreter::visit(NameExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_shared<RuntimeObject>(FQN, make_shared<FqnValue>(vector{node->value})));
}
InterpreterResult Interpreter::visit(NeqExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  return InterpreterResult(make_shared<RuntimeObject>(Bool, *left != *right));
}
InterpreterResult Interpreter::visit(PackageNameExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // PackageNameExpr represents a package name like Package\SubPackage
  // Build the fully qualified name from the parts
  string fqn_str = "";
  for (size_t i = 0; i < node->parts.size(); ++i) {
    if (i > 0) fqn_str += "\\";
    fqn_str += node->parts[i]->value;
  }

  // Create an FQN value
  auto fqn_value = make_shared<FqnValue>();
  // Split the FQN string by backslashes to create parts
  vector<string> parts;
  string current_part;
  for (char c : fqn_str) {
    if (c == '\\') {
      if (!current_part.empty()) {
        parts.push_back(current_part);
        current_part.clear();
      }
    } else {
      current_part += c;
    }
  }
  if (!current_part.empty()) {
    parts.push_back(current_part);
  }
  fqn_value->parts = parts;

  // Try to load the module at this package path
  try {
    auto module = get_or_load_module(fqn_value);
    return InterpreterResult(make_shared<RuntimeObject>(Module, module));
  } catch (const yona_error& e) {
    // Module not found or failed to load
    auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"module_not_found"}));
    auto message = make_shared<RuntimeObject>(String, string("Module not found: " + fqn_str));
    auto exception = make_exception(symbol, message);
    IS.raise_exception(exception, node->source_context);
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }
}

InterpreterResult Interpreter::visit(PipeLeftExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Pipe left: right <| left
  // Apply left (function) to right (argument)
  auto right_val = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto left_val = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  if (left_val->type != Function) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Function for pipe left operator");
  }

  auto func = left_val->get<shared_ptr<FunctionValue>>();

  // Call the function with the argument
  vector<RuntimeObjectPtr> args = {right_val};
  return InterpreterResult(func->code(args));
}

InterpreterResult Interpreter::visit(PipeRightExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Pipe right: left |> right
  // Apply right (function) to left (argument)
  auto left_val = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto right_val = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  if (right_val->type != Function) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Function for pipe right operator");
  }

  auto func = right_val->get<shared_ptr<FunctionValue>>();

  // Call the function with the argument
  vector<RuntimeObjectPtr> args = {left_val};
  return InterpreterResult(func->code(args));
}

InterpreterResult Interpreter::visit(PatternAlias *node) const {
  CHECK_EXCEPTION_RETURN();
  // Evaluate the expression
  auto expr_result = (node->expr->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  // Try to match the pattern against the result
  // Create a new frame for pattern bindings
  IS.push_frame();

  if (!match_pattern(node->pattern, expr_result)) {
    // Pattern didn't match - raise :nomatch exception
    IS.pop_frame();
    auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>(SymbolValue{"nomatch"}));
    auto message = make_shared<RuntimeObject>(String, string("Pattern match failed"));
    auto exception = make_exception(symbol, message);
    IS.raise_exception(exception, node->source_context);
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }

  // Pattern matched - merge bindings to parent frame
  IS.merge_frame_to_parent();
  return InterpreterResult(expr_result);
}
InterpreterResult Interpreter::visit(PatternExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // PatternExpr is used in case expressions
  // It contains pattern + optional guards + expression to evaluate
  return std::visit([this](auto&& arg) -> InterpreterResult {
    using T = decay_t<decltype(arg)>;

    if constexpr (is_same_v<T, Pattern*>) {
      // Just a pattern - used in context where pattern is evaluated
      return arg->accept(*this);
    } else if constexpr (is_same_v<T, PatternWithoutGuards*>) {
      return arg->accept(*this);
    } else if constexpr (is_same_v<T, vector<PatternWithGuards*>>) {
      // Multiple patterns with guards - evaluate first matching one
      for (auto* pattern_with_guard : arg) {
        auto result = pattern_with_guard->accept(*this);
        // If pattern matched (didn't return Unit), return the result
        auto result_obj = result.value;
        // Check if we got a non-unit result (pattern matched and expression evaluated)
        if (result_obj->type != Unit) {
          return result;
        }
      }
      return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
    }
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }, node->patternExpr);
}
InterpreterResult Interpreter::visit(PatternValue *node) const {
  CHECK_EXCEPTION_RETURN();
  // PatternValue evaluates the contained expression
  return std::visit([this](auto&& arg) -> InterpreterResult {
    return arg->accept(*this);
  }, node->expr);
}
InterpreterResult Interpreter::visit(PatternWithGuards *node) const {
  CHECK_EXCEPTION_RETURN();
  // Evaluate the guard expression
  auto guard_result = (node->guard->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  // Check if guard is true
  if (guard_result->type != Bool || !guard_result->get<bool>()) {
    // Guard failed
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }

  // Guard passed - evaluate the expression
  return node->expr->accept(*this);
}
InterpreterResult Interpreter::visit(PatternWithoutGuards *node) const { return node->expr->accept(*this); }
InterpreterResult Interpreter::visit(PowerExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = (node->left->accept(*this).value);
  CHECK_EXCEPTION_RETURN();
  auto right = (node->right->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  if (left->type == Int && right->type == Int) {
    return InterpreterResult(make_shared<RuntimeObject>(Float, pow(left->get<int>(), right->get<int>())));
  } else if (left->type == Float && right->type == Float) {
    return InterpreterResult(make_shared<RuntimeObject>(Float, pow(left->get<double>(), right->get<double>())));
  } else if (left->type == Int && right->type == Float) {
    return InterpreterResult(make_shared<RuntimeObject>(Float, pow(left->get<int>(), right->get<double>())));
  } else if (left->type == Float && right->type == Int) {
    return InterpreterResult(make_shared<RuntimeObject>(Float, pow(left->get<double>(), right->get<int>())));
  }

  throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected numeric types for power operation");
}
InterpreterResult Interpreter::visit(RaiseExpr *node) const {
  // Create an exception with symbol and message
  auto symbol = (node->symbol->accept(*this).value);
  auto message = (node->message->accept(*this).value);

  if (symbol->type != Symbol) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Symbol for exception type");
  }
  if (message->type != String) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected String for exception message");
  }

  // Set the exception in interpreter state
  auto exception = make_exception(symbol, message);
  IS.raise_exception(exception, node->source_context);

  // Return unit value (exception is in the state)
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(RangeSequenceExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // Evaluate start, end, and step expressions
  auto start_obj = (node->start->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  auto end_obj = (node->end->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  // Evaluate step if provided
  RuntimeObjectPtr step_obj;
  if (node->step) {
    step_obj = (node->step->accept(*this).value);
    CHECK_EXCEPTION_RETURN();
  }

  vector<RuntimeObjectPtr> result_fields;

  // Handle integer ranges
  if (start_obj->type == Int && end_obj->type == Int && (!step_obj || step_obj->type == Int)) {
    int start = start_obj->get<int>();
    int end = end_obj->get<int>();
    // If no step provided, determine direction automatically
    int step = step_obj ? step_obj->get<int>() : (start <= end ? 1 : -1);

    if (step == 0) {
      throw yona_error(node->source_context, yona_error::Type::RUNTIME, "Range step cannot be zero");
    }

    // Handle forward and reverse ranges
    if ((step > 0 && start <= end) || (step < 0 && start >= end)) {
      for (int i = start; (step > 0 ? i <= end : i >= end); i += step) {
        result_fields.push_back(make_shared<RuntimeObject>(Int, i));
      }
    }
    // Empty range if direction doesn't match
  }
  // Handle float ranges
  else if ((start_obj->type == Float || start_obj->type == Int) &&
           (end_obj->type == Float || end_obj->type == Int) &&
           (!step_obj || step_obj->type == Float || step_obj->type == Int)) {
    double start = (start_obj->type == Float) ? start_obj->get<double>() : start_obj->get<int>();
    double end = (end_obj->type == Float) ? end_obj->get<double>() : end_obj->get<int>();
    // If no step provided, determine direction automatically
    double step = step_obj ?
      ((step_obj->type == Float) ? step_obj->get<double>() : step_obj->get<int>()) :
      (start <= end ? 1.0 : -1.0);

    if (step == 0.0) {
      throw yona_error(node->source_context, yona_error::Type::RUNTIME, "Range step cannot be zero");
    }

    // Handle forward and reverse ranges with floating point comparison tolerance
    const double epsilon = 1e-10;
    if ((step > 0 && start <= end + epsilon) || (step < 0 && start >= end - epsilon)) {
      for (double i = start; (step > 0 ? i <= end + epsilon : i >= end - epsilon); i += step) {
        result_fields.push_back(make_shared<RuntimeObject>(Float, i));
      }
    }
    // Empty range if direction doesn't match
  } else {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Range bounds and step must be numeric types");
  }

  return InterpreterResult(make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(result_fields)));
}
InterpreterResult Interpreter::visit(RecordInstanceExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // RecordInstanceExpr creates a record instance: RecordType{field1: value1, field2: value2}
  string record_type_name = node->recordType->value;

  // Create field names and values vectors
  vector<string> field_names;
  vector<RuntimeObjectPtr> field_values;

  // Evaluate each field assignment
  for (const auto& item : node->items) {
    string field_name = item.first->value;
    auto field_value = item.second->accept(*this).value;
    CHECK_EXCEPTION_RETURN();

    field_names.push_back(field_name);
    field_values.push_back(field_value);
  }

  // Create the record value
  auto record_value = make_shared<RecordValue>();
  record_value->type_name = record_type_name;
  record_value->field_names = field_names;
  record_value->field_values = field_values;

  // Store record type information in current module if we're in a module context
  if (!IS.module_stack.empty()) {
    auto current_module = IS.module_stack.top();
    // TODO: Store record type information in module->record_types
  }

  return InterpreterResult(make_shared<RuntimeObject>(Record, record_value));
}

InterpreterResult Interpreter::visit(RecordNode *node) const {
  CHECK_EXCEPTION_RETURN();

  // Get the record name
  string record_name = node->recordType->value;

  // Create a list of field names
  vector<string> field_names;
  field_names.reserve(node->identifiers.size());
  for (const auto& [name_expr, _] : node->identifiers) {
    // name_expr is an IdentifierExpr, which contains a NameExpr
    field_names.push_back(name_expr->name->value);
  }

  // Create a constructor function for the record
  // The constructor will take N arguments and create a RecordValue
  auto constructor_func = make_shared<FunctionValue>();
  constructor_func->fqn = make_shared<FqnValue>(vector{record_name});
  constructor_func->arity = field_names.size();
  constructor_func->partial_args = {};

  // Capture the record name and field names in the lambda
  constructor_func->code = [record_name, field_names](const vector<RuntimeObjectPtr>& args) -> RuntimeObjectPtr {
    // Check arity
    if (args.size() != field_names.size()) {
      throw yona_error(SourceLocation{}, yona_error::RUNTIME,
        "Record " + record_name + " expects " + to_string(field_names.size()) +
        " arguments but got " + to_string(args.size()));
    }

    // Create field values map
    vector<RuntimeObjectPtr> field_values;
    field_values.reserve(args.size());
    for (const auto& arg : args) {
      field_values.push_back(arg);
    }

    // Create the record value
    auto record_value = make_shared<RecordValue>(record_name, field_names, field_values);
    return make_shared<RuntimeObject>(Record, record_value);
  };

  auto constructor = make_shared<RuntimeObject>(Function, constructor_func);

  // Store the constructor in the current frame
  IS.frame->locals_[record_name] = constructor;

  // Also store record type information for pattern matching
  RecordTypeInfo type_info;
  type_info.name = record_name;
  type_info.field_names = field_names;
  // For now, leave field_types empty as Yona records are dynamically typed
  IS.record_types[record_name] = make_shared<RecordTypeInfo>(type_info);

  // Return the constructor function
  return InterpreterResult(constructor);
}

InterpreterResult Interpreter::visit(RecordPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // RecordPattern is used in pattern matching contexts, not for evaluation
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}

InterpreterResult Interpreter::visit(OrPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // OrPattern is used in pattern matching contexts, not for evaluation
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}

InterpreterResult Interpreter::visit(SeqGeneratorExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Evaluate the source collection
  auto source = (node->stepExpression->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  // Ensure source is a sequence
  if (source->type != Seq) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Seq for generator source");
  }

  auto source_seq = source->get<shared_ptr<SeqValue>>();
  vector<RuntimeObjectPtr> result_fields;

  // Process each element in the source
  for (const auto& elem : source_seq->fields) {
    // Push a new frame for this iteration
    IS.push_frame();

    // Bind the element to the extractor variable(s)
    // The collectionExtractor will be a ValueCollectionExtractorExpr that binds the element
    // We need to pass the element to the extractor so it can bind it
    IS.generator_current_element = elem;
    node->collectionExtractor->accept(*this);
    CHECK_EXCEPTION_RETURN();

    // Evaluate the reducer expression with the bound variable
    auto reduced = (node->reducerExpr->accept(*this).value);
    CHECK_EXCEPTION_RETURN();
    result_fields.push_back(reduced);

    // Pop the frame
    IS.pop_frame();
  }

  return InterpreterResult(make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(result_fields)));
}
InterpreterResult Interpreter::visit(SeqPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // SeqPattern is used in pattern matching contexts, not for evaluation
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}

InterpreterResult Interpreter::visit(SetExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  vector<RuntimeObjectPtr> fields;

  fields.reserve(node->values.size());
  for (const auto value : node->values) {
    fields.push_back((value->accept(*this).value));
    CHECK_EXCEPTION_RETURN();
  }

  return InterpreterResult(make_shared<RuntimeObject>(Set, make_shared<SetValue>(fields)));
}

InterpreterResult Interpreter::visit(SetGeneratorExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Evaluate the source collection
  auto source = (node->stepExpression->accept(*this).value);
  CHECK_EXCEPTION_RETURN();

  // Ensure source is a collection (Seq, Set, or Dict)
  if (source->type != Seq && source->type != Set && source->type != Dict) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected collection for generator source");
  }

  vector<RuntimeObjectPtr> result_fields;

  if (source->type == Seq) {
    auto source_seq = source->get<shared_ptr<SeqValue>>();
    for (const auto& elem : source_seq->fields) {
      IS.push_frame();
      IS.generator_current_element = elem;
      node->collectionExtractor->accept(*this);
      CHECK_EXCEPTION_RETURN();
      auto reduced = (node->reducerExpr->accept(*this).value);
      CHECK_EXCEPTION_RETURN();
      result_fields.push_back(reduced);
      IS.pop_frame();
    }
  } else if (source->type == Set) {
    auto source_set = source->get<shared_ptr<SetValue>>();
    for (const auto& elem : source_set->fields) {
      IS.push_frame();
      IS.generator_current_element = elem;
      node->collectionExtractor->accept(*this);
      CHECK_EXCEPTION_RETURN();
      auto reduced = (node->reducerExpr->accept(*this).value);
      CHECK_EXCEPTION_RETURN();
      result_fields.push_back(reduced);
      IS.pop_frame();
    }
  } else if (source->type == Dict) {
    auto source_dict = source->get<shared_ptr<DictValue>>();
    for (const auto& [key, value] : source_dict->fields) {
      IS.push_frame();
      // For dict, we need to handle key-value pairs differently
      // This would use KeyValueCollectionExtractorExpr
      IS.generator_current_element = value; // For now, just use value
      node->collectionExtractor->accept(*this);
      CHECK_EXCEPTION_RETURN();
      auto reduced = (node->reducerExpr->accept(*this).value);
      CHECK_EXCEPTION_RETURN();
      result_fields.push_back(reduced);
      IS.pop_frame();
    }
  }

  return InterpreterResult(make_shared<RuntimeObject>(Set, make_shared<SetValue>(result_fields)));
}
InterpreterResult Interpreter::visit(StringExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_typed_object(String, node->value, node));
}
InterpreterResult Interpreter::visit(SymbolExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_typed_object(Symbol, make_shared<SymbolValue>(SymbolValue{node->value}), node));
}
InterpreterResult Interpreter::visit(TailsHeadPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // TailsHeadPattern is used in pattern matching contexts, not for evaluation
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(TrueLiteralExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_typed_object(Bool, true, node));
}
InterpreterResult Interpreter::visit(TryCatchExpr *node) const {
  // Clear any existing exception before try block
  IS.clear_exception();

  // Execute the try expression
  InterpreterResult result;
  try {
    result = node->tryExpr->accept(*this);
  } catch (...) {
    // If an exception was thrown during visit, just re-throw
    throw;
  }

  // If no exception was raised, return the result
  if (!IS.has_exception) {
    return result;
  }

  // Save the exception for pattern matching
  auto exception = IS.exception_value;
  // TODO: Use exception_context for better error reporting

  // Clear the exception state before executing catch block
  IS.clear_exception();

  // Pass exception to catch block for pattern matching
  // Try each catch pattern in order
  for (auto catch_pattern : node->catchExpr->patterns) {
    if (catch_pattern->matchPattern) {
      // Create a new frame for pattern bindings
      IS.push_frame();

      // Check if the pattern matches the exception
      if (match_pattern(catch_pattern->matchPattern, exception)) {
        // Pattern matched - execute the body
        InterpreterResult catch_result(make_shared<RuntimeObject>(Unit, nullptr));

        if (holds_alternative<PatternWithoutGuards*>(catch_pattern->pattern)) {
          auto pattern_body = get<PatternWithoutGuards*>(catch_pattern->pattern);
          if (pattern_body) {
            catch_result = pattern_body->accept(*this);
          }
        } else {
          auto patterns = get<vector<PatternWithGuards*>>(catch_pattern->pattern);
          if (!patterns.empty() && patterns[0]) {
            catch_result = patterns[0]->accept(*this);
          }
        }

        // Merge pattern bindings and return the result
        IS.merge_frame_to_parent();
        return catch_result;
      } else {
        // Pattern didn't match - discard frame and try next
        IS.pop_frame();
      }
    }
  }

  // No pattern matched - re-raise the exception
  IS.raise_exception(exception, IS.exception_context);
  throw yona_error(IS.exception_context, yona_error::Type::RUNTIME, "Unhandled exception");
}

InterpreterResult Interpreter::visit(TupleExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  vector<RuntimeObjectPtr> fields;

  fields.reserve(node->values.size());
  for (const auto value : node->values) {
    fields.push_back((value->accept(*this).value));
    CHECK_EXCEPTION_RETURN();
  }

  return InterpreterResult(make_typed_object(Tuple, make_shared<TupleValue>(fields), node));
}

InterpreterResult Interpreter::visit(TuplePattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // TuplePattern is used in pattern matching contexts, not for evaluation
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(UnderscoreNode *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_shared<RuntimeObject>(Bool, true));
}
InterpreterResult Interpreter::visit(UnitExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return InterpreterResult(make_typed_object(Unit, nullptr, node));
}

InterpreterResult Interpreter::visit(ValueAlias *node) const {
  CHECK_EXCEPTION_RETURN();
  auto result = node->expr->accept(*this);
  CHECK_EXCEPTION_RETURN();
  IS.frame->write(node->identifier->name->value, (result).value);
  return result;
}

InterpreterResult Interpreter::visit(ValueCollectionExtractorExpr *node) const {
  // Get the current element from the generator context
  auto elem = IS.generator_current_element;

  if (holds_alternative<IdentifierExpr*>(node->expr)) {
    // Bind the element to the identifier
    auto identifier = get<IdentifierExpr*>(node->expr);
    IS.frame->write(identifier->name->value, elem);
  }
  // If it's an underscore, we don't bind anything

  return InterpreterResult(elem);
}

InterpreterResult Interpreter::visit(ValuesSequenceExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  vector<RuntimeObjectPtr> fields;

  fields.reserve(node->values.size());
  for (const auto value : node->values) {
    fields.push_back((value->accept(*this).value));
    CHECK_EXCEPTION_RETURN();
  }

  return InterpreterResult(make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(fields)));
}

InterpreterResult Interpreter::visit(WithExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // WithExpr provides resource management: with resource_expr as name do body_expr end
  // First evaluate the context expression to get the resource
  auto context_result = node->contextExpr->accept(*this).value;
  CHECK_EXCEPTION_RETURN();

  // Create a new frame for the with expression
  IS.push_frame();

  // Bind the resource to the specified name
  string resource_name = node->name->value;
  IS.frame->write(resource_name, context_result);

  // Execute the body expression
  auto body_result = node->bodyExpr->accept(*this);

  // Pop the frame before returning
  IS.pop_frame();

  // Check if an exception occurred during body execution
  if (IS.has_exception) {
    // Still return the result, the exception is in the interpreter state
    return body_result;
  }

  // Note: In a full implementation, we would handle resource cleanup here
  // For now, we just return the body result
  return body_result;
}
InterpreterResult Interpreter::visit(FunctionDeclaration *node) const {
  CHECK_EXCEPTION_RETURN();

  // FunctionDeclaration represents a function type declaration
  // In the interpreter, we mainly just return Unit as this is for type checking
  // The actual function implementation should be handled separately
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(TypeDeclaration *node) const {
  CHECK_EXCEPTION_RETURN();

  // TypeDeclaration represents a type declaration in the AST
  // In the interpreter, this is mainly used for type checking
  // Return Unit as declarations don't produce runtime values
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(TypeDefinition *node) const {
  CHECK_EXCEPTION_RETURN();

  // TypeDefinition represents a type definition in the AST
  // In the interpreter, this is mainly used for type checking
  // Return Unit as definitions don't produce runtime values
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(TypeNode *node) const {
  CHECK_EXCEPTION_RETURN();

  // TypeNode represents a type node in the AST
  // In the interpreter, this is mainly used for type checking
  // Return Unit as type nodes don't produce runtime values
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(TypeInstance *node) const {
  CHECK_EXCEPTION_RETURN();

  // TypeInstance represents a type instance in the AST
  // In the interpreter, this is mainly used for type checking
  // Return Unit as type instances don't produce runtime values
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(IdentifierExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "IdentifierExpr: Looking up '" << node->name->value << "'";
  auto result = IS.frame->lookup(node->source_context, node->name->value);
  if (result->type == Function) {
    // BOOST_LOG_TRIVIAL(debug) << "IdentifierExpr: Found function for '" << node->name->value << "'";
  } else {
    // BOOST_LOG_TRIVIAL(debug) << "IdentifierExpr: Found non-function type=" << result->type << " for '" << node->name->value << "'";
  }
  return InterpreterResult(result);
}

InterpreterResult Interpreter::visit(ScopedNode *node) const {
  IS.push_frame();
  auto result = node->accept(*this);
  IS.pop_frame();
  return result;
}

// Override the base implementation to add exception handling logic
InterpreterResult Interpreter::visit(AstNode *node) const {
  // Don't check exceptions for certain node types that need to handle them
  if (dynamic_cast<TryCatchExpr*>(node) ||
      dynamic_cast<CatchExpr*>(node) ||
      dynamic_cast<CatchPatternExpr*>(node) ||
      dynamic_cast<PatternWithoutGuards*>(node)) {
    return dispatchVisit(node);
  }

  // Check for existing exception before visiting
  if (IS.has_exception) {
    return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
  }

  // Use our own dispatch to avoid issues with virtual calls from base template
  return dispatchVisit(node);
}
InterpreterResult Interpreter::visit(MainNode *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "MainNode: Starting visit";
  IS.push_frame();
  // Use accept() to ensure proper dynamic dispatch
  // BOOST_LOG_TRIVIAL(debug) << "MainNode: node type = " << (node->node ? node->node->get_type() : -1);
  auto result = node->node ? node->node->accept(*this) : InterpreterResult{};
  // BOOST_LOG_TRIVIAL(debug) << "MainNode: accept() returned";
  // Preserve exception state when popping frame
  auto had_exception = IS.has_exception;
  auto exception_value = IS.exception_value;
  auto exception_context = IS.exception_context;

  IS.pop_frame();

  // Restore exception state if there was one
  if (had_exception) {
    IS.has_exception = had_exception;
    IS.exception_value = exception_value;
    IS.exception_context = exception_context;
  }

  // BOOST_LOG_TRIVIAL(debug) << "MainNode: Visit complete";
  return result;
}
InterpreterResult Interpreter::visit(BuiltinTypeNode *node) const {
  CHECK_EXCEPTION_RETURN();

  // BuiltinTypeNode represents a builtin type in the AST
  // In the interpreter, this is mainly used for type checking
  // Return Unit as type nodes don't produce runtime values
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}
InterpreterResult Interpreter::visit(UserDefinedTypeNode *node) const {
  CHECK_EXCEPTION_RETURN();

  // UserDefinedTypeNode represents a user-defined type in the AST
  // In the interpreter, this is mainly used for type checking
  // Return Unit as type nodes don't produce runtime values
  return InterpreterResult(make_shared<RuntimeObject>(Unit, nullptr));
}

// Visitor methods for intermediate base classes
// These delegate to the base class implementation which uses dispatchVisit

InterpreterResult Interpreter::visit(ExprNode *node) const {
  CHECK_EXCEPTION_RETURN();
  // Use our own dispatchVisit to avoid calling virtual functions from base class template
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(PatternNode *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(ValueExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(SequenceExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(FunctionBody *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(AliasExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(OpExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(BinaryOpExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(TypeNameNode *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(CallExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(GeneratorExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}

InterpreterResult Interpreter::visit(CollectionExtractorExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return dispatchVisit(node);
}


// Module loading and resolution implementations
string Interpreter::fqn_to_path(const shared_ptr<FqnValue>& fqn) const {
  string path;
  for (size_t i = 0; i < fqn->parts.size(); ++i) {
    if (i > 0) {
      path += filesystem::path::preferred_separator;
    }
    path += fqn->parts[i];
  }
  path += ".yona";
  return path;
}

string Interpreter::find_module_file(const string& relative_path) const {
  // First check if it's an absolute path
  if (filesystem::exists(relative_path)) {
    return relative_path;
  }

  // Search in module paths
  for (const auto& base_path : IS.module_paths) {
    auto full_path = filesystem::path(base_path) / relative_path;
    if (filesystem::exists(full_path)) {
      return full_path.string();
    }
  }

  return "";  // Not found
}

shared_ptr<ModuleValue> Interpreter::load_module(const shared_ptr<FqnValue>& fqn) const {
  auto relative_path = fqn_to_path(fqn);
  auto module_path = find_module_file(relative_path);

  if (module_path.empty()) {
    throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
                     "Module not found: " + relative_path);
  }

  // Parse the module file
  parser::Parser parser;
  ifstream file(module_path);
  if (!file.is_open()) {
    throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
                     "Failed to open module file: " + module_path);
  }

  // Read file contents
  stringstream buffer;
  buffer << file.rdbuf();
  string file_contents = buffer.str();

  auto parse_result = parser.parse_module(file_contents, module_path);
  if (!parse_result.has_value()) {
    string error_msg = "Failed to parse module: " + module_path;
    auto& errors = parse_result.error();
    if (!errors.empty()) {
      error_msg = error_msg + " - " + errors.front().message;
    }
    throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, error_msg);
  }

  // Move the AST to a shared_ptr so we can keep it alive
  shared_ptr<ModuleExpr> module_ast(parse_result.value().release());

  // Save current frame and create a new one for module evaluation
  auto saved_frame = IS.frame;
  IS.frame = make_shared<InterepterFrame>(nullptr);  // Module gets its own top-level frame

  // Visit the module to evaluate it
  auto result = module_ast->accept(*this);

  // Restore the frame
  IS.frame = saved_frame;

  if (result.value->type != Module) {
    throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                     "Module file must evaluate to a module");
  }

  auto module = result.value->get<shared_ptr<ModuleValue>>();
  module->source_path = module_path;

  // Keep the AST alive as long as the module is alive
  module->ast_keeper = module_ast;

  // TODO: Process exports list from ModuleExpr

  return module;
}

shared_ptr<ModuleValue> Interpreter::get_or_load_module(const shared_ptr<FqnValue>& fqn) const {
  // Convert FQN to cache key
  string cache_key;
  for (size_t i = 0; i < fqn->parts.size(); ++i) {
    if (i > 0) cache_key += "/";
    cache_key += fqn->parts[i];
  }

  // Check cache first
  auto it = IS.module_cache.find(cache_key);
  if (it != IS.module_cache.end()) {
    return it->second;
  }

  // Load module
  auto module = load_module(fqn);

  // Cache it
  IS.module_cache[cache_key] = module;

  return module;
}

// Helper to create runtime objects with type information
RuntimeObjectPtr Interpreter::make_typed_object(RuntimeObjectType type, RuntimeObjectData data, AstNode* node) const {
  if (node && type_checker.has_value()) {
    // Check if we have type information for this node
    auto it = type_annotations.find(node);
    if (it != type_annotations.end()) {
      return make_shared<RuntimeObject>(type, std::move(data), it->second);
    }
  }
  return make_shared<RuntimeObject>(type, std::move(data));
}

// Runtime type checking helpers
bool Interpreter::check_runtime_type(const RuntimeObjectPtr& value, const Type& expected_type) const {
  // If value has static type information, use it
  if (value->static_type.has_value()) {
    // TODO: Implement proper type unification/subtyping check
    // For now, just check exact match
    return true;  // Placeholder
  }

  // Otherwise, check based on runtime type
  Type runtime_type = runtime_type_to_static_type(value->type);
  // TODO: Implement proper type compatibility check
  return true;  // Placeholder
}

Type Interpreter::runtime_type_to_static_type(RuntimeObjectType type) const {
  switch (type) {
    case Int: return Type(SignedInt64);
    case Float: return Type(Float64);
    case Bool: return Type(compiler::types::Bool);
    case Byte: return Type(compiler::types::Byte);
    case Char: return Type(compiler::types::Char);
    case String: return Type(compiler::types::String);
    case Symbol: return Type(compiler::types::Symbol);
    case Unit: return Type(compiler::types::Unit);
    case Tuple: return nullptr;  // Need more info
    case Seq: return nullptr;    // Need more info
    case Set: return nullptr;    // Need more info
    case Dict: return nullptr;   // Need more info
    case Record: return nullptr; // Need more info
    case Function: return nullptr; // Need more info
    default: return nullptr;
  }
}

// Type checking integration
void Interpreter::enable_type_checking(bool enable) {
  if (enable && !type_checker.has_value()) {
    type_checker = make_unique<TypeChecker>(type_context, nullptr);
  } else if (!enable) {
    type_checker.reset();
  }
}

bool Interpreter::type_check(AstNode* node) {
  if (!type_checker.has_value()) {
    enable_type_checking(true);
  }

  // Reset context for fresh type checking
  type_context = TypeInferenceContext();
  type_annotations.clear();

  // Perform type checking
  Type inferred_type = (*type_checker)->check(node);

  // Return whether type checking succeeded
  return !type_context.has_errors();
}

} // namespace yona::interp
