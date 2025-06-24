//
// Created by akovari on 17.11.24.
//

#include "Interpreter.h"

#include <print>
#include <numeric>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "common.h"
#include "utils.h"
#include "Parser.h"

using namespace yona::compiler::types;
using namespace yona::typechecker;

#define BINARY_OP_EXTRACTION(ROT, VT, start, offset, op)                                                                                             \
  map_value<ROT, VT>({node->left, node->right},                                                                                                      \
                     [](vector<VT> values) { return reduce(values.begin() + (offset), values.end(), (offset) ? values[0] : (start), op()); })

#define CHECK_EXCEPTION_RETURN()                                                                                                                     \
  if (IS.has_exception) return make_shared<RuntimeObject>(Unit, nullptr)

#define VISIT_WITH_EXCEPTION_CHECK(body)                                                                                                            \
  CHECK_EXCEPTION_RETURN();                                                                                                                          \
  body

#define BINARY_OP(T, err, ...)                                                                                                                       \
  any Interpreter::visit(T *node) const {                                                                                                            \
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
  any Interpreter::visit(T *node) const {                                                                                                            \
    CHECK_EXCEPTION_RETURN();                                                                                                                        \
    const auto left = any_cast<RuntimeObjectPtr>(visit(node->left));                                                                                \
    CHECK_EXCEPTION_RETURN();                                                                                                                        \
    const auto right = any_cast<RuntimeObjectPtr>(visit(node->right));                                                                              \
    CHECK_EXCEPTION_RETURN();                                                                                                                        \
                                                                                                                                                     \
    if (left->type == Int && right->type == Int) {                                                                                                  \
      return make_shared<RuntimeObject>(Bool, left->get<int>() op right->get<int>());                                                                \
    } else if (left->type == Float && right->type == Float) {                                                                                       \
      return make_shared<RuntimeObject>(Bool, left->get<double>() op right->get<double>());                                                          \
    } else if (left->type == Int && right->type == Float) {                                                                                         \
      return make_shared<RuntimeObject>(Bool, left->get<int>() op right->get<double>());                                                             \
    } else if (left->type == Float && right->type == Int) {                                                                                         \
      return make_shared<RuntimeObject>(Bool, left->get<double>() op right->get<int>());                                                             \
    } else if (left->type == Byte && right->type == Byte) {                                                                                         \
      return make_shared<RuntimeObject>(Bool, static_cast<uint8_t>(left->get<std::byte>()) op static_cast<uint8_t>(right->get<std::byte>()));       \
    } else if (left->type == Byte && right->type == Int) {                                                                                          \
      return make_shared<RuntimeObject>(Bool, static_cast<int>(static_cast<uint8_t>(left->get<std::byte>())) op right->get<int>());                  \
    } else if (left->type == Int && right->type == Byte) {                                                                                          \
      return make_shared<RuntimeObject>(Bool, left->get<int>() op static_cast<int>(static_cast<uint8_t>(right->get<std::byte>())));                  \
    }                                                                                                                                                \
                                                                                                                                                     \
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected numeric types for comparison");                         \
  }

namespace yona::interp {
using namespace std::placeholders;

template <RuntimeObjectType ROT, typename VT> optional<VT> Interpreter::get_value(AstNode *node) const {
  const auto runtime_object = any_cast<RuntimeObjectPtr>(visit(node));

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
      return false;
    } else if constexpr (is_same_v<T, SymbolExpr*>) {
      // Symbol literal
      if (value->type != Symbol) return false;
      auto symbol_result = any_cast<RuntimeObjectPtr>(this->visit(arg));
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
    auto key_result = any_cast<RuntimeObjectPtr>(visit(key_pattern));

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
  // TODO: Implement record pattern matching
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
        auto pattern_result = any_cast<RuntimeObjectPtr>(visit(expr));
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

any Interpreter::visit(AddExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  
  auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();
  
  // Handle byte promotion to int
  if (left->type == Byte || right->type == Byte) {
    int left_val = (left->type == Byte) ? static_cast<int>(static_cast<uint8_t>(left->get<std::byte>())) : 
                   (left->type == Int) ? left->get<int>() : 
                   throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    int right_val = (right->type == Byte) ? static_cast<int>(static_cast<uint8_t>(right->get<std::byte>())) : 
                    (right->type == Int) ? right->get<int>() :
                    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    return make_shared<RuntimeObject>(Int, left_val + right_val);
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
any Interpreter::visit(MultiplyExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  
  auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();
  
  // Handle byte promotion to int
  if (left->type == Byte || right->type == Byte) {
    int left_val = (left->type == Byte) ? static_cast<int>(static_cast<uint8_t>(left->get<std::byte>())) : 
                   (left->type == Int) ? left->get<int>() : 
                   throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    int right_val = (right->type == Byte) ? static_cast<int>(static_cast<uint8_t>(right->get<std::byte>())) : 
                    (right->type == Int) ? right->get<int>() :
                    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    return make_shared<RuntimeObject>(Int, left_val * right_val);
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
any Interpreter::visit(SubtractExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  
  auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();
  
  // Handle byte promotion to int
  if (left->type == Byte || right->type == Byte) {
    int left_val = (left->type == Byte) ? static_cast<int>(static_cast<uint8_t>(left->get<std::byte>())) : 
                   (left->type == Int) ? left->get<int>() : 
                   throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    int right_val = (right->type == Byte) ? static_cast<int>(static_cast<uint8_t>(right->get<std::byte>())) : 
                    (right->type == Int) ? right->get<int>() :
                    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int or Byte");
    return make_shared<RuntimeObject>(Int, left_val - right_val);
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

any Interpreter::visit(LogicalNotOpExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto operand = any_cast<RuntimeObjectPtr>(visit(node->expr));
  CHECK_EXCEPTION_RETURN();

  if (operand->type != Bool) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Bool for logical not operation");
  }

  return make_shared<RuntimeObject>(Bool, !operand->get<bool>());
}
any Interpreter::visit(BinaryNotOpExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto operand = any_cast<RuntimeObjectPtr>(visit(node->expr));
  CHECK_EXCEPTION_RETURN();

  if (operand->type != Int) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int for binary not operation");
  }

  return make_shared<RuntimeObject>(Int, ~operand->get<int>());
}

any Interpreter::visit(AliasCall *node) const { return expr_wrapper(node); }
any Interpreter::visit(ApplyExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "ApplyExpr: Starting function application";

  // First visit the call expression to get the function
  // BOOST_LOG_TRIVIAL(debug) << "ApplyExpr: About to visit call expression of type: " << node->call->get_type();
  // BOOST_LOG_TRIVIAL(debug) << "ApplyExpr: Call expr is " << typeid(*node->call).name();
  auto call_result = node->call->accept(*this);
  CHECK_EXCEPTION_RETURN();

  // BOOST_LOG_TRIVIAL(debug) << "ApplyExpr: Call expression visited, result type: " << any_cast<RuntimeObjectPtr>(call_result)->type;

  // Check if it's a function
  auto func_obj = any_cast<RuntimeObjectPtr>(call_result);
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
      new_args.push_back(any_cast<RuntimeObjectPtr>(visit(get<ValueExpr *>(arg))));
    } else {
      new_args.push_back(any_cast<RuntimeObjectPtr>(visit(get<ExprNode *>(arg))));
    }
    CHECK_EXCEPTION_RETURN();
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

    return make_typed_object(Function, partial_func, node);
  } else if (all_args.size() == func_val->arity) {
    // Exact number of arguments - apply the function

    // Perform runtime type checking if types are available
    if (type_checker.has_value() && func_val->type.has_value()) {
      // TODO: Implement type checking for function arguments
      // For now, just apply without checking
    }

    return func_val->code(all_args);
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

    return result;
  }
}
any Interpreter::visit(AsDataStructurePattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // AsDataStructurePattern is used in pattern matching contexts, not for evaluation
  // It binds a value to an identifier and then matches against a pattern
  // This should not be visited directly
  return make_shared<RuntimeObject>(Unit, nullptr);
}

any Interpreter::visit(BodyWithGuards *node) const { return expr_wrapper(node); }
any Interpreter::visit(BodyWithoutGuards *node) const {
  CHECK_EXCEPTION_RETURN();
  return visit(node->expr);
}
any Interpreter::visit(ByteExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_typed_object(Byte, static_cast<std::byte>(node->value), node);
}
any Interpreter::visit(CaseExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // Evaluate the expression to match against
  auto match_value = any_cast<RuntimeObjectPtr>(visit(node->expr));
  CHECK_EXCEPTION_RETURN();

  // Try each clause in order
  for (auto* clause : node->clauses) {
    // Create a new frame for pattern bindings
    IS.push_frame();

    // Try to match the pattern
    if (match_pattern(clause->pattern, match_value)) {
      // Pattern matched - evaluate the body and return
      auto result = visit(clause->body);
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
  auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>("nomatch"));
  auto message = make_shared<RuntimeObject>(String, string("No pattern matched in case expression"));
  auto exception = make_exception(symbol, message);
  IS.raise_exception(exception, node->source_context);
  return make_shared<RuntimeObject>(Unit, nullptr);
}

any Interpreter::visit(CaseClause *node) const {
  // This method should not be called directly - CaseClause is handled within CaseExpr
  throw yona_error(node->source_context, yona_error::Type::RUNTIME, "CaseClause should not be visited directly");
}

any Interpreter::visit(CatchExpr *node) const {
  // Don't check for exceptions here - we're in a catch block
  // For now, execute the first catch pattern
  // In a full implementation, we would match patterns against the exception
  if (!node->patterns.empty()) {
    auto result = visit(node->patterns[0]);
    // Don't check for exceptions - let them propagate
    return result;
  }
  return make_shared<RuntimeObject>(Unit, nullptr);
}
any Interpreter::visit(CatchPatternExpr *node) const {
  // Don't check for exceptions here - we're in a catch block
  // For now, always execute the pattern body
  // In a full implementation, we would check if the pattern matches the exception
  if (holds_alternative<PatternWithoutGuards*>(node->pattern)) {
    auto pattern = get<PatternWithoutGuards*>(node->pattern);
    if (pattern) {
      return visit(pattern);
    }
  } else {
    auto patterns = get<vector<PatternWithGuards*>>(node->pattern);
    if (!patterns.empty() && patterns[0]) {
      return visit(patterns[0]);
    }
  }
  return make_shared<RuntimeObject>(Unit, nullptr);
}
any Interpreter::visit(CharacterExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_typed_object(Char, node->value, node);
}
any Interpreter::visit(ConsLeftExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();

  if (right->type != Seq) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Seq on right side of cons");
  }

  const auto seq = right->get<shared_ptr<SeqValue>>();
  vector<RuntimeObjectPtr> new_fields;
  new_fields.push_back(left);
  new_fields.insert(new_fields.end(), seq->fields.begin(), seq->fields.end());

  return make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(new_fields));
}
any Interpreter::visit(ConsRightExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  const auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  const auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();

  if (left->type != Seq) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Seq on left side of cons");
  }

  auto seq = left->get<shared_ptr<SeqValue>>();
  vector<RuntimeObjectPtr> new_fields(seq->fields);
  new_fields.push_back(right);

  return make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(new_fields));
}

any Interpreter::visit(DictExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  vector<pair<RuntimeObjectPtr, RuntimeObjectPtr>> fields;

  fields.reserve(node->values.size());
  for (const auto [fst, snd] : node->values) {
    auto key = any_cast<RuntimeObjectPtr>(visit(fst));
    CHECK_EXCEPTION_RETURN();
    auto value = any_cast<RuntimeObjectPtr>(visit(snd));
    CHECK_EXCEPTION_RETURN();
    fields.emplace_back(key, value);
  }

  return make_shared<RuntimeObject>(Dict, make_shared<DictValue>(fields));
}

any Interpreter::visit(DictGeneratorExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Evaluate the source collection
  auto source = any_cast<RuntimeObjectPtr>(visit(node->stepExpression));
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
      visit(node->collectionExtractor);
      CHECK_EXCEPTION_RETURN();
      // For dict generator, evaluate the reducer to get key and value
      auto key = any_cast<RuntimeObjectPtr>(visit(node->reducerExpr->key));
      CHECK_EXCEPTION_RETURN();
      auto value = any_cast<RuntimeObjectPtr>(visit(node->reducerExpr->value));
      CHECK_EXCEPTION_RETURN();
      result_fields.emplace_back(key, value);
      IS.pop_frame();
    }
  } else if (source->type == Set) {
    auto source_set = source->get<shared_ptr<SetValue>>();
    for (const auto& elem : source_set->fields) {
      IS.push_frame();
      IS.generator_current_element = elem;
      visit(node->collectionExtractor);
      CHECK_EXCEPTION_RETURN();
      auto key = any_cast<RuntimeObjectPtr>(visit(node->reducerExpr->key));
      CHECK_EXCEPTION_RETURN();
      auto value = any_cast<RuntimeObjectPtr>(visit(node->reducerExpr->value));
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
      visit(node->collectionExtractor);
      CHECK_EXCEPTION_RETURN();
      auto new_key = any_cast<RuntimeObjectPtr>(visit(node->reducerExpr->key));
      CHECK_EXCEPTION_RETURN();
      auto new_value = any_cast<RuntimeObjectPtr>(visit(node->reducerExpr->value));
      CHECK_EXCEPTION_RETURN();
      result_fields.emplace_back(new_key, new_value);
      IS.pop_frame();
    }
  }

  return make_shared<RuntimeObject>(Dict, make_shared<DictValue>(result_fields));
}
any Interpreter::visit(DictGeneratorReducer *node) const {
  // This node is not visited directly during generation
  // Instead, its key and value expressions are evaluated in DictGeneratorExpr
  throw yona_error(node->source_context, yona_error::Type::RUNTIME, "DictGeneratorReducer should not be visited directly");
}
any Interpreter::visit(DictPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // DictPattern is used in pattern matching contexts, not for evaluation
  return make_shared<RuntimeObject>(Unit, nullptr);
}

any Interpreter::visit(DoExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  any result;
  for (const auto child : node->steps) {
    result = visit(child);
    CHECK_EXCEPTION_RETURN();
  }
  return result;
}

any Interpreter::visit(EqExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  const auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  const auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();

  return make_shared<RuntimeObject>(Bool, *left == *right);
}
any Interpreter::visit(FalseLiteralExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_typed_object(Bool, false, node);
}
any Interpreter::visit(FieldAccessExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(FieldUpdateExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(FloatExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_typed_object(Float, node->value, node);
}
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
          auto result = visit(body);
          // BOOST_LOG_TRIVIAL(debug) << "FunctionExpr: Body execution complete";
          return any_cast<RuntimeObjectPtr>(result);
        }

        const auto body_with_guards = dynamic_cast<BodyWithGuards *>(body);
        if (optional<bool> guard_result = get_value<Bool, bool>(body_with_guards->guard); !guard_result.has_value() || !guard_result.value()) {
          continue;
        }

        return any_cast<RuntimeObjectPtr>(visit(body_with_guards->expr));
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

  return make_typed_object(Function, fun_value, node);
}

any Interpreter::visit(FunctionsImport *node) const {
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

  return make_shared<RuntimeObject>(Unit, nullptr);
}
COMPARISON_OP(GtExpr, >)
COMPARISON_OP(GteExpr, >=)
any Interpreter::visit(HeadTailsHeadPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // HeadTailsHeadPattern is used in pattern matching contexts, not for evaluation
  return make_shared<RuntimeObject>(Unit, nullptr);
}
any Interpreter::visit(HeadTailsPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // HeadTailsPattern is used in pattern matching contexts, not for evaluation
  return make_shared<RuntimeObject>(Unit, nullptr);
}
any Interpreter::visit(IfExpr *node) const {
  auto condition = any_cast<RuntimeObjectPtr>(visit(node->condition));

  if (condition->type != Bool) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Bool for if condition");
  }

  if (condition->get<bool>()) {
    return visit(node->thenExpr);
  } else {
    return visit(node->elseExpr);
  }
}
any Interpreter::visit(ImportClauseExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // ImportClauseExpr is a base class - the actual import is in derived classes
  // This will be called via dynamic dispatch to ModuleImport or FunctionsImport
  return make_shared<RuntimeObject>(Unit, nullptr);
}
any Interpreter::visit(ImportExpr *node) const {
  CHECK_EXCEPTION_RETURN();

  // Create a new frame for the imported bindings
  auto saved_frame = IS.frame;
  IS.frame = make_shared<InterepterFrame>(saved_frame);

  // Process all import clauses
  for (auto clause : node->clauses) {
    auto clause_result = any_cast<RuntimeObjectPtr>(visit(clause));
    CHECK_EXCEPTION_RETURN();
  }

  // Visit the expression that uses the imported functions
  any result = make_shared<RuntimeObject>(Unit, nullptr);
  if (node->expr) {
    result = visit(node->expr);
  }

  // Restore the previous frame
  IS.frame = saved_frame;
  
  return result;
}
any Interpreter::visit(InExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();

  if (right->type == Seq) {
    auto seq = right->get<shared_ptr<SeqValue>>();
    for (const auto& elem : seq->fields) {
      if (*elem == *left) {
        return make_shared<RuntimeObject>(Bool, true);
      }
    }
    return make_shared<RuntimeObject>(Bool, false);
  } else if (right->type == Set) {
    auto set = right->get<shared_ptr<SetValue>>();
    for (const auto& elem : set->fields) {
      if (*elem == *left) {
        return make_shared<RuntimeObject>(Bool, true);
      }
    }
    return make_shared<RuntimeObject>(Bool, false);
  } else if (right->type == Dict) {
    auto dict = right->get<shared_ptr<DictValue>>();
    for (const auto& [key, value] : dict->fields) {
      if (*key == *left) {
        return make_shared<RuntimeObject>(Bool, true);
      }
    }
    return make_shared<RuntimeObject>(Bool, false);
  }

  throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected collection type for 'in' operation");
}
any Interpreter::visit(IntegerExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_typed_object(Int, node->value, node);
}
any Interpreter::visit(JoinExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();

  if (left->type != Seq || right->type != Seq) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Seq for join operation");
  }

  auto left_seq = left->get<shared_ptr<SeqValue>>();
  auto right_seq = right->get<shared_ptr<SeqValue>>();

  vector<RuntimeObjectPtr> new_fields(left_seq->fields);
  new_fields.insert(new_fields.end(), right_seq->fields.begin(), right_seq->fields.end());

  return make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(new_fields));
}
any Interpreter::visit(KeyValueCollectionExtractorExpr *node) const {
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

  return value;
}
any Interpreter::visit(LambdaAlias *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "LambdaAlias: Creating lambda for name=" << node->name->value;

  // Evaluate the lambda expression
  auto lambda_result = visit(node->lambda);
  CHECK_EXCEPTION_RETURN();

  // Bind the function to the name in the current frame
  IS.frame->write(node->name->value, lambda_result);

  // BOOST_LOG_TRIVIAL(debug) << "LambdaAlias: Lambda bound to " << node->name->value;
  return lambda_result;
}

any Interpreter::visit(LetExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  for (const auto alias : node->aliases) {
    visit(alias);
    CHECK_EXCEPTION_RETURN();
  }

  return visit(node->expr);
}

COMPARISON_OP(LtExpr, <)
COMPARISON_OP(LteExpr, <=)
any Interpreter::visit(ModuloExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();

  if (left->type == Int && right->type == Int) {
    int divisor = right->get<int>();
    if (divisor == 0) {
      throw yona_error(node->source_context, yona_error::Type::RUNTIME, "Division by zero");
    }
    return make_shared<RuntimeObject>(Int, left->get<int>() % divisor);
  }

  throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Int for modulo operation");
}
any Interpreter::visit(ModuleAlias *node) const { return expr_wrapper(node); }
any Interpreter::visit(ModuleCall *node) const { return expr_wrapper(node); }
any Interpreter::visit(ExprCall *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "ExprCall: Evaluating expression for call";
  // BOOST_LOG_TRIVIAL(debug) << "ExprCall: Expression type = " << node->expr->get_type();

  // ExprCall handles general expression calls (e.g., (lambda)(args) or curried(args))
  // Simply evaluate the expression - it should return a function
  auto result = visit(node->expr);
  // BOOST_LOG_TRIVIAL(debug) << "ExprCall: Expression evaluated, type="
  //                          << (result.has_value() ? to_string(any_cast<RuntimeObjectPtr>(result)->type) : "no value");
  return result;
}

any Interpreter::visit(ModuleExpr *node) const {
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
    auto record_result = any_cast<RuntimeObjectPtr>(visit(record));
    CHECK_EXCEPTION_RETURN();
    if (record_result->type == Tuple) {
      // TODO: Store record type information in module->record_types
      // For now, we're not storing runtime record instances in the module
    }
  }

  // Process functions and populate exports
  for (auto func : node->functions) {
    auto func_result = any_cast<RuntimeObjectPtr>(visit(func));
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
  return make_shared<RuntimeObject>(Module, module);
}

any Interpreter::visit(ModuleImport *node) const {
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

  return make_shared<RuntimeObject>(Unit, nullptr);
}
any Interpreter::visit(NameCall *node) const {
  CHECK_EXCEPTION_RETURN();
  auto expr = IS.frame->lookup(node->source_context, node->name->value);

  if (expr->type != Function) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Expected a function, got " + RuntimeObjectTypes[expr->type]);
  }

  return expr;
}
any Interpreter::visit(NameExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_shared<RuntimeObject>(FQN, make_shared<FqnValue>(vector{node->value}));
}
any Interpreter::visit(NeqExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();

  return make_shared<RuntimeObject>(Bool, *left != *right);
}
any Interpreter::visit(PackageNameExpr *node) const { return expr_wrapper(node); }

any Interpreter::visit(PipeLeftExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Pipe left: right <| left
  // Apply left (function) to right (argument)
  auto right_val = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();
  auto left_val = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();

  if (left_val->type != Function) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Function for pipe left operator");
  }

  auto func = left_val->get<shared_ptr<FunctionValue>>();

  // Call the function with the argument
  vector<RuntimeObjectPtr> args = {right_val};
  return func->code(args);
}

any Interpreter::visit(PipeRightExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Pipe right: left |> right
  // Apply right (function) to left (argument)
  auto left_val = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  auto right_val = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();

  if (right_val->type != Function) {
    throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected Function for pipe right operator");
  }

  auto func = right_val->get<shared_ptr<FunctionValue>>();

  // Call the function with the argument
  vector<RuntimeObjectPtr> args = {left_val};
  return func->code(args);
}

any Interpreter::visit(PatternAlias *node) const {
  CHECK_EXCEPTION_RETURN();
  // Evaluate the expression
  auto expr_result = any_cast<RuntimeObjectPtr>(visit(node->expr));
  CHECK_EXCEPTION_RETURN();

  // Try to match the pattern against the result
  // Create a new frame for pattern bindings
  IS.push_frame();

  if (!match_pattern(node->pattern, expr_result)) {
    // Pattern didn't match - raise :nomatch exception
    IS.pop_frame();
    auto symbol = make_shared<RuntimeObject>(Symbol, make_shared<SymbolValue>("nomatch"));
    auto message = make_shared<RuntimeObject>(String, string("Pattern match failed"));
    auto exception = make_exception(symbol, message);
    IS.raise_exception(exception, node->source_context);
    return make_shared<RuntimeObject>(Unit, nullptr);
  }

  // Pattern matched - merge bindings to parent frame
  IS.merge_frame_to_parent();
  return expr_result;
}
any Interpreter::visit(PatternExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // PatternExpr is used in case expressions
  // It contains pattern + optional guards + expression to evaluate
  return std::visit([this](auto&& arg) -> any {
    using T = decay_t<decltype(arg)>;

    if constexpr (is_same_v<T, Pattern*>) {
      // Just a pattern - used in context where pattern is evaluated
      return visit(arg);
    } else if constexpr (is_same_v<T, PatternWithoutGuards*>) {
      return visit(arg);
    } else if constexpr (is_same_v<T, vector<PatternWithGuards*>>) {
      // Multiple patterns with guards - evaluate first matching one
      for (auto* pattern_with_guard : arg) {
        auto result = visit(pattern_with_guard);
        // If pattern matched (didn't return Unit), return the result
        auto result_obj = any_cast<RuntimeObjectPtr>(result);
        // Check if we got a non-unit result (pattern matched and expression evaluated)
        if (result_obj->type != Unit) {
          return result;
        }
      }
      return make_shared<RuntimeObject>(Unit, nullptr);
    }
    return make_shared<RuntimeObject>(Unit, nullptr);
  }, node->patternExpr);
}
any Interpreter::visit(PatternValue *node) const {
  CHECK_EXCEPTION_RETURN();
  // PatternValue evaluates the contained expression
  return std::visit([this](auto&& arg) -> any {
    return visit(arg);
  }, node->expr);
}
any Interpreter::visit(PatternWithGuards *node) const {
  CHECK_EXCEPTION_RETURN();
  // Evaluate the guard expression
  auto guard_result = any_cast<RuntimeObjectPtr>(visit(node->guard));
  CHECK_EXCEPTION_RETURN();

  // Check if guard is true
  if (guard_result->type != Bool || !guard_result->get<bool>()) {
    // Guard failed
    return make_shared<RuntimeObject>(Unit, nullptr);
  }

  // Guard passed - evaluate the expression
  return visit(node->expr);
}
any Interpreter::visit(PatternWithoutGuards *node) const { return visit(node->expr); }
any Interpreter::visit(PowerExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  CHECK_EXCEPTION_RETURN();
  auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  CHECK_EXCEPTION_RETURN();

  if (left->type == Int && right->type == Int) {
    return make_shared<RuntimeObject>(Float, pow(left->get<int>(), right->get<int>()));
  } else if (left->type == Float && right->type == Float) {
    return make_shared<RuntimeObject>(Float, pow(left->get<double>(), right->get<double>()));
  } else if (left->type == Int && right->type == Float) {
    return make_shared<RuntimeObject>(Float, pow(left->get<int>(), right->get<double>()));
  } else if (left->type == Float && right->type == Int) {
    return make_shared<RuntimeObject>(Float, pow(left->get<double>(), right->get<int>()));
  }

  throw yona_error(node->source_context, yona_error::Type::TYPE, "Type mismatch: expected numeric types for power operation");
}
any Interpreter::visit(RaiseExpr *node) const {
  // Create an exception with symbol and message
  auto symbol = any_cast<RuntimeObjectPtr>(visit(node->symbol));
  auto message = any_cast<RuntimeObjectPtr>(visit(node->message));

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
  return make_shared<RuntimeObject>(Unit, nullptr);
}
any Interpreter::visit(RangeSequenceExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  
  // Evaluate start, end, and step expressions
  auto start_obj = any_cast<RuntimeObjectPtr>(visit(node->start));
  CHECK_EXCEPTION_RETURN();
  
  auto end_obj = any_cast<RuntimeObjectPtr>(visit(node->end));
  CHECK_EXCEPTION_RETURN();
  
  // Evaluate step if provided
  RuntimeObjectPtr step_obj;
  if (node->step) {
    step_obj = any_cast<RuntimeObjectPtr>(visit(node->step));
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
  
  return make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(result_fields));
}
any Interpreter::visit(RecordInstanceExpr *node) const { return expr_wrapper(node); }

any Interpreter::visit(RecordNode *node) const {
  CHECK_EXCEPTION_RETURN();
  vector<RuntimeObjectPtr> fields;
  fields.reserve(node->identifiers.size());
  for (const auto [identifier, type_def] : node->identifiers) {
    fields.push_back(any_cast<RuntimeObjectPtr>(visit(identifier)));
    CHECK_EXCEPTION_RETURN();
  }
  return make_shared<RuntimeObject>(Tuple, make_shared<TupleValue>(fields));
}

any Interpreter::visit(RecordPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // RecordPattern is used in pattern matching contexts, not for evaluation
  return make_shared<RuntimeObject>(Unit, nullptr);
}
any Interpreter::visit(SeqGeneratorExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Evaluate the source collection
  auto source = any_cast<RuntimeObjectPtr>(visit(node->stepExpression));
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
    visit(node->collectionExtractor);
    CHECK_EXCEPTION_RETURN();

    // Evaluate the reducer expression with the bound variable
    auto reduced = any_cast<RuntimeObjectPtr>(visit(node->reducerExpr));
    CHECK_EXCEPTION_RETURN();
    result_fields.push_back(reduced);

    // Pop the frame
    IS.pop_frame();
  }

  return make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(result_fields));
}
any Interpreter::visit(SeqPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // SeqPattern is used in pattern matching contexts, not for evaluation
  return make_shared<RuntimeObject>(Unit, nullptr);
}

any Interpreter::visit(SetExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  vector<RuntimeObjectPtr> fields;

  fields.reserve(node->values.size());
  for (const auto value : node->values) {
    fields.push_back(any_cast<RuntimeObjectPtr>(visit(value)));
    CHECK_EXCEPTION_RETURN();
  }

  return make_shared<RuntimeObject>(Set, make_shared<SetValue>(fields));
}

any Interpreter::visit(SetGeneratorExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // Evaluate the source collection
  auto source = any_cast<RuntimeObjectPtr>(visit(node->stepExpression));
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
      visit(node->collectionExtractor);
      CHECK_EXCEPTION_RETURN();
      auto reduced = any_cast<RuntimeObjectPtr>(visit(node->reducerExpr));
      CHECK_EXCEPTION_RETURN();
      result_fields.push_back(reduced);
      IS.pop_frame();
    }
  } else if (source->type == Set) {
    auto source_set = source->get<shared_ptr<SetValue>>();
    for (const auto& elem : source_set->fields) {
      IS.push_frame();
      IS.generator_current_element = elem;
      visit(node->collectionExtractor);
      CHECK_EXCEPTION_RETURN();
      auto reduced = any_cast<RuntimeObjectPtr>(visit(node->reducerExpr));
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
      visit(node->collectionExtractor);
      CHECK_EXCEPTION_RETURN();
      auto reduced = any_cast<RuntimeObjectPtr>(visit(node->reducerExpr));
      CHECK_EXCEPTION_RETURN();
      result_fields.push_back(reduced);
      IS.pop_frame();
    }
  }

  return make_shared<RuntimeObject>(Set, make_shared<SetValue>(result_fields));
}
any Interpreter::visit(StringExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_typed_object(String, node->value, node);
}
any Interpreter::visit(SymbolExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_typed_object(Symbol, make_shared<SymbolValue>(node->value), node);
}
any Interpreter::visit(TailsHeadPattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // TailsHeadPattern is used in pattern matching contexts, not for evaluation
  return make_shared<RuntimeObject>(Unit, nullptr);
}
any Interpreter::visit(TrueLiteralExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_typed_object(Bool, true, node);
}
any Interpreter::visit(TryCatchExpr *node) const {
  // Clear any existing exception before try block
  IS.clear_exception();

  // Execute the try expression
  any result;
  try {
    result = visit(node->tryExpr);
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

  // TODO: Pass exception to catch block for pattern matching
  // For now, just execute the catch expression
  return visit(node->catchExpr);
}

any Interpreter::visit(TupleExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  vector<RuntimeObjectPtr> fields;

  fields.reserve(node->values.size());
  for (const auto value : node->values) {
    fields.push_back(any_cast<RuntimeObjectPtr>(visit(value)));
    CHECK_EXCEPTION_RETURN();
  }

  return make_typed_object(Tuple, make_shared<TupleValue>(fields), node);
}

any Interpreter::visit(TuplePattern *node) const {
  CHECK_EXCEPTION_RETURN();
  // TuplePattern is used in pattern matching contexts, not for evaluation
  return make_shared<RuntimeObject>(Unit, nullptr);
}
any Interpreter::visit(UnderscoreNode *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_shared<RuntimeObject>(Bool, true);
}
any Interpreter::visit(UnitExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return make_typed_object(Unit, nullptr, node);
}

any Interpreter::visit(ValueAlias *node) const {
  CHECK_EXCEPTION_RETURN();
  auto result = visit(node->expr);
  CHECK_EXCEPTION_RETURN();
  IS.frame->write(node->identifier->name->value, result);
  return result;
}

any Interpreter::visit(ValueCollectionExtractorExpr *node) const {
  // Get the current element from the generator context
  auto elem = IS.generator_current_element;

  if (holds_alternative<IdentifierExpr*>(node->expr)) {
    // Bind the element to the identifier
    auto identifier = get<IdentifierExpr*>(node->expr);
    IS.frame->write(identifier->name->value, elem);
  }
  // If it's an underscore, we don't bind anything

  return elem;
}

any Interpreter::visit(ValuesSequenceExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  vector<RuntimeObjectPtr> fields;

  fields.reserve(node->values.size());
  for (const auto value : node->values) {
    fields.push_back(any_cast<RuntimeObjectPtr>(visit(value)));
    CHECK_EXCEPTION_RETURN();
  }

  return make_shared<RuntimeObject>(Seq, make_shared<SeqValue>(fields));
}

any Interpreter::visit(WithExpr *node) const { return expr_wrapper(node); }
any Interpreter::visit(FunctionDeclaration *node) const { return expr_wrapper(node); }
any Interpreter::visit(TypeDeclaration *node) const { return expr_wrapper(node); }
any Interpreter::visit(TypeDefinition *node) const { return expr_wrapper(node); }
any Interpreter::visit(TypeNode *node) const { return expr_wrapper(node); }
any Interpreter::visit(TypeInstance *node) const { return expr_wrapper(node); }
any Interpreter::visit(IdentifierExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "IdentifierExpr: Looking up '" << node->name->value << "'";
  auto result = IS.frame->lookup(node->source_context, node->name->value);
  if (result->type == Function) {
    // BOOST_LOG_TRIVIAL(debug) << "IdentifierExpr: Found function for '" << node->name->value << "'";
  } else {
    // BOOST_LOG_TRIVIAL(debug) << "IdentifierExpr: Found non-function type=" << result->type << " for '" << node->name->value << "'";
  }
  return result;
}

any Interpreter::visit(ScopedNode *node) const {
  IS.push_frame();
  auto result = AstVisitor::visit(node);
  IS.pop_frame();
  return result;
}

any Interpreter::visit(ExprNode *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "ExprNode: Visiting node type = " << node->get_type();
  auto result = AstVisitor::visit(node);
  // BOOST_LOG_TRIVIAL(debug) << "ExprNode: Visit complete for type = " << node->get_type();
  return result;
}
any Interpreter::visit(AstNode *node) const {
  // Don't check exceptions for certain node types that need to handle them
  if (dynamic_cast<TryCatchExpr*>(node) ||
      dynamic_cast<CatchExpr*>(node) ||
      dynamic_cast<CatchPatternExpr*>(node) ||
      dynamic_cast<PatternWithoutGuards*>(node)) {
    return Interpreter::visit(node);
  }

  // Check for existing exception before visiting
  if (IS.has_exception) {
    return make_shared<RuntimeObject>(Unit, nullptr);
  }

  // Visit the node using the base class implementation
  return AstVisitor::visit(node);
}
any Interpreter::visit(PatternNode *node) const {
  CHECK_EXCEPTION_RETURN();
  return AstVisitor::visit(node);
}
any Interpreter::visit(ValueExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return AstVisitor::visit(node);
}
any Interpreter::visit(SequenceExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return AstVisitor::visit(node);
}
any Interpreter::visit(FunctionBody *node) const {
  CHECK_EXCEPTION_RETURN();
  return AstVisitor::visit(node);
}
any Interpreter::visit(AliasExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return AstVisitor::visit(node);
}
any Interpreter::visit(OpExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return AstVisitor::visit(node);
}
any Interpreter::visit(BinaryOpExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return AstVisitor::visit(node);
}
any Interpreter::visit(TypeNameNode *node) const {
  CHECK_EXCEPTION_RETURN();
  return AstVisitor::visit(node);
}
any Interpreter::visit(MainNode *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "MainNode: Starting visit";
  IS.push_frame();
  // Use accept() to ensure proper dynamic dispatch
  // BOOST_LOG_TRIVIAL(debug) << "MainNode: node type = " << (node->node ? node->node->get_type() : -1);
  auto result = node->node ? node->node->accept(*this) : any{};
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
any Interpreter::visit(BuiltinTypeNode *node) const { return expr_wrapper(node); }
any Interpreter::visit(UserDefinedTypeNode *node) const { return expr_wrapper(node); }

// Visitor methods for intermediate base classes
any Interpreter::visit(CallExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  // BOOST_LOG_TRIVIAL(debug) << "CallExpr: Visiting call expression";
  // Let the base visitor dispatch to the concrete type
  return AstVisitor::visit(node);
}

any Interpreter::visit(GeneratorExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return AstVisitor::visit(node);
}

any Interpreter::visit(CollectionExtractorExpr *node) const {
  CHECK_EXCEPTION_RETURN();
  return AstVisitor::visit(node);
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

  // Visit the module to evaluate it
  auto result = any_cast<RuntimeObjectPtr>(visit(parse_result.value().get()));

  if (result->type != Module) {
    throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE,
                     "Module file must evaluate to a module");
  }

  auto module = result->get<shared_ptr<ModuleValue>>();
  module->source_path = module_path;

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
