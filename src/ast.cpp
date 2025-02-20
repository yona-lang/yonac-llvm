#include "ast.h"

#include <boost/algorithm/string/join.hpp>
#include <variant>

#include "utils.h"

namespace yona::ast {

using compiler::types::Type;

template <class T>
  requires derived_from<T, AstNode>
T *AstNode::parent_of_type() {
  if (parent == nullptr) {
    return nullptr;
  }
  AstNode *tmp = parent;
  do {
    if (const auto result = dynamic_cast<T *>(tmp); result != nullptr) {
      return result;
    }
    tmp = tmp->parent;
  } while (tmp != nullptr);
  return nullptr;
}

template <typename T> LiteralExpr<T>::LiteralExpr(SourceContext token, T value) : ValueExpr(token), value(std::move(value)) {}

template <typename T> any LiteralExpr<T>::accept(const AstVisitor &visitor) { return ValueExpr::accept(visitor); }

any AstNode::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type AstNode::infer_type(AstContext &ctx) const { unreachable(); }

any ScopedNode::accept(const AstVisitor &visitor) { return AstNode::accept(visitor); }

any OpExpr::accept(const AstVisitor &visitor) { return ExprNode::accept(visitor); }

BinaryOpExpr::BinaryOpExpr(SourceContext token, ExprNode *left, ExprNode *right)
    : OpExpr(token), left(left->with_parent<ExprNode>(this)), right(right->with_parent<ExprNode>(this)) {}

Type BinaryOpExpr::infer_type(AstContext &ctx) const {
  const Type lhs = left->infer_type(ctx);
  const Type rhs = right->infer_type(ctx);

  if (!is_numeric(lhs) || !is_numeric(rhs)) {
    ctx.addError(yona_error(source_context, yona_error::TYPE, "Binary expression must be numeric type"));
  }

  return derive_bin_op_result_type(lhs, rhs);
}

any BinaryOpExpr::accept(const AstVisitor &visitor) { return OpExpr::accept(visitor); }
BinaryOpExpr::~BinaryOpExpr() {
  delete left;
  delete right;
}

NameExpr::NameExpr(SourceContext token, string value) : ExprNode(token), value(std::move(value)) {}

any NameExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type NameExpr::infer_type(AstContext &ctx) const { unreachable(); }

Type AliasExpr::infer_type(AstContext &ctx) const { unreachable(); }

any AliasExpr::accept(const AstVisitor &visitor) { return ExprNode::accept(visitor); }

Type CallExpr::infer_type(AstContext &ctx) const { unreachable(); }

any CallExpr::accept(const AstVisitor &visitor) { return ExprNode::accept(visitor); }
any ImportClauseExpr::accept(const AstVisitor &visitor) { return ExprNode::accept(visitor); }

any GeneratorExpr::accept(const AstVisitor &visitor) { return ExprNode::accept(visitor); }

any CollectionExtractorExpr::accept(const AstVisitor &visitor) { return ExprNode::accept(visitor); }

any SequenceExpr::accept(const AstVisitor &visitor) { return ExprNode::accept(visitor); }

any FunctionBody::accept(const AstVisitor &visitor) { return AstNode::accept(visitor); }

void NameExpr::print(std::ostream &os) const { os << value; }

IdentifierExpr::IdentifierExpr(SourceContext token, NameExpr *name) : ValueExpr(token), name(name->with_parent<NameExpr>(this)) {}

any IdentifierExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type IdentifierExpr::infer_type(AstContext &ctx) const { return nullptr; }

IdentifierExpr::~IdentifierExpr() { delete name; }

void IdentifierExpr::print(std::ostream &os) const { os << *name; }

RecordNode::RecordNode(SourceContext token, NameExpr *recordType, vector<pair<IdentifierExpr *, TypeDefinition *>> identifiers)
    : AstNode(token), recordType(recordType->with_parent<NameExpr>(this)), identifiers(nodes_with_parent(std::move(identifiers), this)) {}

any RecordNode::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type RecordNode::infer_type(AstContext &ctx) const {
  return nullptr; // TODO
}

RecordNode::~RecordNode() {
  delete recordType;
  for (const auto [fst, snd] : identifiers) {
    delete fst;
    delete snd;
  }
}

void RecordNode::print(std::ostream &os) const {
  os << recordType << '(';
  size_t i = 0;
  for (const auto [ident, type_def] : identifiers) {
    os << *ident << ':' << *type_def;
    if (i++ < identifiers.size() - 1) {
      os << ", ";
    }
  }
  os << ')';
}

TrueLiteralExpr::TrueLiteralExpr(SourceContext token) : LiteralExpr<bool>(token, true) {}

any TrueLiteralExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type TrueLiteralExpr::infer_type(AstContext &ctx) const { return Bool; }

void TrueLiteralExpr::print(std::ostream &os) const { os << "true"; }

FalseLiteralExpr::FalseLiteralExpr(SourceContext token) : LiteralExpr<bool>(token, false) {}

any FalseLiteralExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type FalseLiteralExpr::infer_type(AstContext &ctx) const { return Bool; }

void FalseLiteralExpr::print(std::ostream &os) const { os << "false"; }

FloatExpr::FloatExpr(SourceContext token, const float value) : LiteralExpr<float>(token, value) {}

any FloatExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type FloatExpr::infer_type(AstContext &ctx) const { return Float32; }

void FloatExpr::print(std::ostream &os) const { os << value; }

IntegerExpr::IntegerExpr(SourceContext token, const int value) : LiteralExpr<int>(token, value) {}

any IntegerExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type IntegerExpr::infer_type(AstContext &ctx) const { return SignedInt32; }

void IntegerExpr::print(std::ostream &os) const { os << value; }

any ExprNode::accept(const AstVisitor &visitor) { return visitor.visit(this); }

any PatternNode::accept(const AstVisitor &visitor) { return visitor.visit(this); }

void UnderscoreNode::print(std::ostream &os) const { os << "_"; }

any UnderscoreNode::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type UnderscoreNode::infer_type(AstContext &ctx) const { unreachable(); }

any ValueExpr::accept(const AstVisitor &visitor) { return ExprNode::accept(visitor); }

ByteExpr::ByteExpr(SourceContext token, const unsigned char value) : LiteralExpr<unsigned char>(token, value) {}

any ByteExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ByteExpr::infer_type(AstContext &ctx) const { return Byte; }

void ByteExpr::print(std::ostream &os) const { os << value; }

StringExpr::StringExpr(SourceContext token, string value) : LiteralExpr<string>(token, std::move(value)) {}

any StringExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type StringExpr::infer_type(AstContext &ctx) const { return String; }

void StringExpr::print(std::ostream &os) const { os << value; }

CharacterExpr::CharacterExpr(SourceContext token, const wchar_t value) : LiteralExpr<wchar_t>(token, value) {}

any CharacterExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type CharacterExpr::infer_type(AstContext &ctx) const { return Char; }

void CharacterExpr::print(std::ostream &os) const {
  os << (char)value; // TODO
}

UnitExpr::UnitExpr(SourceContext token) : LiteralExpr<nullptr_t>(token, nullptr) {}

any UnitExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type UnitExpr::infer_type(AstContext &ctx) const { return Unit; }

void UnitExpr::print(std::ostream &os) const { os << "()"; }

TupleExpr::TupleExpr(SourceContext token, vector<ExprNode *> values) : ValueExpr(token), values(nodes_with_parent(std::move(values), this)) {}

any TupleExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type TupleExpr::infer_type(AstContext &ctx) const {
  vector<Type> fieldTypes;
  ranges::for_each(values, [&](const ExprNode *expr) { fieldTypes.push_back(expr->infer_type(ctx)); });
  return make_shared<ProductType>(fieldTypes);
}

TupleExpr::~TupleExpr() {
  for (const auto p : values)
    delete p;
}

void TupleExpr::print(std::ostream &os) const {
  os << '(';
  for (size_t i = 0; i < values.size(); i++) {
    os << *values[i];
    if (i < values.size() - 1) {
      os << ", ";
    }
  }
  os << ')';
}

DictExpr::DictExpr(SourceContext token, vector<pair<ExprNode *, ExprNode *>> values)
    : ValueExpr(token), values(nodes_with_parent(std::move(values), this)) {}

any DictExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type DictExpr::infer_type(AstContext &ctx) const {
  unordered_set<Type> keyTypes;
  unordered_set<Type> valueTypes;
  ranges::for_each(values, [&](const pair<ExprNode *, ExprNode *> &p) {
    keyTypes.insert(p.first->infer_type(ctx));
    valueTypes.insert(p.second->infer_type(ctx));
  });

  if (keyTypes.size() > 1 || valueTypes.size() > 1) {
    ctx.addError(yona_error(source_context, yona_error::TYPE, "Dictionary keys and values must have the same type"));
  }

  return make_shared<DictCollectionType>(values.front().first->infer_type(ctx), values.front().second->infer_type(ctx));
}

DictExpr::~DictExpr() {
  for (const auto [fst, snd] : values) {
    delete fst;
    delete snd;
  }
}

void DictExpr::print(std::ostream &os) const {
  os << '{';
  for (size_t i = 0; i < values.size(); i++) {
    os << values[i].first << ": " << values[i].second;
    if (i < values.size() - 1) {
      os << ", ";
    }
  }
  os << '}';
}

ValuesSequenceExpr::ValuesSequenceExpr(SourceContext token, vector<ExprNode *> values)
    : SequenceExpr(token), values(nodes_with_parent(std::move(values), this)) {}

any ValuesSequenceExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ValuesSequenceExpr::infer_type(AstContext &ctx) const {
  unordered_set<Type> valueTypes;
  ranges::for_each(values, [&](const ExprNode *expr) { valueTypes.insert(expr->infer_type(ctx)); });

  if (valueTypes.size() > 1) {
    ctx.addError(yona_error(source_context, yona_error::TYPE, "Sequence values must have the same type"));
  }

  return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Seq, values.front()->infer_type(ctx));
}

ValuesSequenceExpr::~ValuesSequenceExpr() {
  for (const auto p : values)
    delete p;
}

void ValuesSequenceExpr::print(std::ostream &os) const {
  os << '[';
  for (size_t i = 0; i < values.size(); i++) {
    os << values[i];
    if (i < values.size() - 1) {
      os << ", ";
    }
  }
  os << ']';
}

RangeSequenceExpr::RangeSequenceExpr(SourceContext token, ExprNode *start, ExprNode *end, ExprNode *step)
    : SequenceExpr(token), start(start->with_parent<ExprNode>(this)), end(end->with_parent<ExprNode>(this)), step(step->with_parent<ExprNode>(this)) {
}

any RangeSequenceExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type RangeSequenceExpr::infer_type(AstContext &ctx) const {
  Type startExprType = start->infer_type(ctx);
  Type endExprType = start->infer_type(ctx);
  Type stepExprType = start->infer_type(ctx);

  if (!is_signed(startExprType)) {
    ctx.addError(yona_error(start->source_context, yona_error::TYPE, "Sequence start expression must be integer"));
  }

  if (!is_signed(endExprType)) {
    ctx.addError(yona_error(end->source_context, yona_error::TYPE, "Sequence end expression must be integer"));
  }

  if (!is_signed(stepExprType)) {
    ctx.addError(yona_error(step->source_context, yona_error::TYPE, "Sequence step expression must be integer"));
  }

  return nullptr;
}

RangeSequenceExpr::~RangeSequenceExpr() {
  delete start;
  delete end;
  delete step;
}

void RangeSequenceExpr::print(std::ostream &os) const {
  os << '[';
  if (step) {
    os << *step << ", ";
  }
  os << *start << ".." << *end << ']';
}

SetExpr::SetExpr(SourceContext token, vector<ExprNode *> values) : ValueExpr(token), values(nodes_with_parent(std::move(values), this)) {}

any SetExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type SetExpr::infer_type(AstContext &ctx) const {
  unordered_set<Type> valueTypes;
  ranges::for_each(values, [&](const ExprNode *expr) { valueTypes.insert(expr->infer_type(ctx)); });

  if (valueTypes.size() > 1) {
    ctx.addError(yona_error(source_context, yona_error::TYPE, "Set values must have the same type"));
  }

  return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Set, values.front()->infer_type(ctx));
}

SetExpr::~SetExpr() {
  for (const auto p : values)
    delete p;
}

void SetExpr::print(std::ostream &os) const {
  os << '{';
  for (size_t i = 0; i < values.size(); i++) {
    os << *values[i];
    if (i < values.size() - 1) {
      os << ", ";
    }
  }
  os << '}';
}

SymbolExpr::SymbolExpr(SourceContext token, string value) : ValueExpr(token), value(std::move(value)) {}

any SymbolExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type SymbolExpr::infer_type(AstContext &ctx) const { return Symbol; }

void SymbolExpr::print(std::ostream &os) const { os << ':' << value; }

PackageNameExpr::PackageNameExpr(SourceContext token, vector<NameExpr *> parts)
    : ValueExpr(token), parts(nodes_with_parent(std::move(parts), this)) {}

any PackageNameExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type PackageNameExpr::infer_type(AstContext &ctx) const { unreachable(); }

PackageNameExpr::~PackageNameExpr() {
  for (const auto p : parts)
    delete p;
}

string PackageNameExpr::to_string() const {
  vector<string> names;
  names.reserve(parts.size());
  for (const auto part : parts) {
    names.push_back(part->value);
  }

  return boost::algorithm::join(names, PACKAGE_DELIMITER);
}

void PackageNameExpr::print(std::ostream &os) const { os << to_string(); }

FqnExpr::FqnExpr(SourceContext token, optional<PackageNameExpr *> packageName, NameExpr *moduleName)
    : ValueExpr(token), packageName(node_with_parent<PackageNameExpr>(packageName, this)), moduleName(moduleName->with_parent<NameExpr>(this)) {}

any FqnExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type FqnExpr::infer_type(AstContext &ctx) const { unreachable(); }

FqnExpr::~FqnExpr() {
  if (packageName.has_value()) {
    delete packageName.value();
  }
  delete moduleName;
}

string FqnExpr::to_string() const {
  string result = packageName.transform([](auto val) { return val->to_string(); }).value_or("");
  if (!result.empty()) {
    result += PACKAGE_DELIMITER;
  }
  result += moduleName->value;
  return result;
}

void FqnExpr::print(std::ostream &os) const { os << to_string(); }

FunctionExpr::FunctionExpr(SourceContext token, string name, vector<PatternNode *> patterns, vector<FunctionBody *> bodies)
    : ScopedNode(token), name(std::move(name)), patterns(nodes_with_parent(std::move(patterns), this)),
      bodies(nodes_with_parent(std::move(bodies), this)) {}

any FunctionExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type FunctionExpr::infer_type(AstContext &ctx) const {
  unordered_set<Type> bodyTypes;
  ranges::for_each(bodies, [&](const FunctionBody *body) { bodyTypes.insert(body->infer_type(ctx)); });

  return make_shared<SumType>(bodyTypes);
}

FunctionExpr::~FunctionExpr() {
  for (const auto p : patterns)
    delete p;
  for (const auto p : bodies)
    delete p;
}

void FunctionExpr::print(std::ostream &os) const {
  os << name << " ";
  size_t i = 0;
  for (const auto p : patterns) {
    os << *p;
    if (i < patterns.size() - 1) {
      os << ' ';
    }
  }
  for (const auto p : bodies) {
    os << *p << endl;
  }
}

BodyWithGuards::BodyWithGuards(SourceContext token, ExprNode *guard, ExprNode *expr)
    : FunctionBody(token), guard(guard->with_parent<ExprNode>(this)), expr(expr->with_parent<ExprNode>(this)) {}

any BodyWithGuards::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type BodyWithGuards::infer_type(AstContext &ctx) const {
  if (const Type guardType = guard->infer_type(ctx); !holds_alternative<BuiltinType>(guardType) || get<BuiltinType>(guardType) != Bool) {
    ctx.addError(yona_error(guard->source_context, yona_error::TYPE, "Guard expression must be boolean"));
  }

  return expr->infer_type(ctx);
}

void BodyWithGuards::print(std::ostream &os) const { os << " | " << *guard << " = " << *expr; }

BodyWithGuards::~BodyWithGuards() {
  delete guard;
  delete expr;
}

BodyWithoutGuards::BodyWithoutGuards(SourceContext token, ExprNode *expr) : FunctionBody(token), expr(expr->with_parent<ExprNode>(this)) {}

any BodyWithoutGuards::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type BodyWithoutGuards::infer_type(AstContext &ctx) const { return expr->infer_type(ctx); }

BodyWithoutGuards::~BodyWithoutGuards() { delete expr; }

void BodyWithoutGuards::print(std::ostream &os) const { os << " = " << *expr; }

ModuleExpr::ModuleExpr(SourceContext token, FqnExpr *fqn, const vector<string> &exports, const vector<RecordNode *> &records,
                       const vector<FunctionExpr *> &functions, const vector<FunctionDeclaration *> &function_declarations)
    : ValueExpr(token), fqn(fqn->with_parent<FqnExpr>(this)), exports(exports), records(nodes_with_parent(std::move(records), this)),
      functions(nodes_with_parent(std::move(functions), this)), functionDeclarations(nodes_with_parent(std::move(function_declarations), this)) {}

any ModuleExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ModuleExpr::infer_type(AstContext &ctx) const {
  unordered_set<Type> functionTypes;
  ranges::for_each(functions, [&](const auto *item) { functionTypes.insert(item->infer_type(ctx)); });

  return make_shared<NamedType>(fqn->to_string(), make_shared<SumType>(functionTypes));
}

ModuleExpr::~ModuleExpr() {
  delete fqn;
  for (const auto p : records)
    delete p;
  for (const auto p : functions)
    delete p;
  for (const auto p : functionDeclarations)
    delete p;
}

void ModuleExpr::print(std::ostream &os) const {
  os << "module " << *fqn << " exports ";

  for (size_t i = 0; i < exports.size(); i++) {
    os << exports[i];
    if (i < exports.size() - 1) {
      os << ", ";
    }
  }

  os << " as" << endl;

  for (const auto p : records) {
    os << *p << endl;
  }

  for (const auto p : functionDeclarations) {
    os << *p << endl;
  }

  for (const auto p : functions) {
    os << *p << endl;
  }

  os << "end";
}

RecordInstanceExpr::RecordInstanceExpr(SourceContext token, NameExpr *recordType, vector<pair<NameExpr *, ExprNode *>> items)
    : ValueExpr(token), recordType(recordType->with_parent<NameExpr>(this)), items(nodes_with_parent(std::move(items), this)) {}

any RecordInstanceExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type RecordInstanceExpr::infer_type(AstContext &ctx) const {
  vector<Type> itemTypes;

  for (const auto &[name, expr] : items) {
    itemTypes.push_back(expr->infer_type(ctx));
  }

  return make_shared<NamedType>(recordType->value, make_shared<ProductType>(itemTypes));
}

RecordInstanceExpr::~RecordInstanceExpr() {
  delete recordType;
  for (const auto [fst, snd] : items) {
    delete fst;
    delete snd;
  }
}

void RecordInstanceExpr::print(std::ostream &os) const {
  os << *recordType << '(';

  for (size_t i = 0; i < items.size(); i++) {
    os << *items[i].first << " = " << *items[i].second;
    if (i < items.size() - 1) {
      os << ", ";
    }
  }

  os << ')';
}

LogicalNotOpExpr::LogicalNotOpExpr(SourceContext token, ExprNode *expr) : OpExpr(token), expr(expr->with_parent<ExprNode>(this)) {}

any LogicalNotOpExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type LogicalNotOpExpr::infer_type(AstContext &ctx) const {
  if (const Type exprType = expr->infer_type(ctx); !holds_alternative<BuiltinType>(exprType) || get<BuiltinType>(exprType) != Bool) {
    ctx.addError(yona_error(expr->source_context, yona_error::TYPE, "Expression for logical negation must be boolean"));
  }

  return Bool;
}

LogicalNotOpExpr::~LogicalNotOpExpr() { delete expr; }

void LogicalNotOpExpr::print(std::ostream &os) const { os << '!' << *expr; }

BinaryNotOpExpr::BinaryNotOpExpr(SourceContext token, ExprNode *expr) : OpExpr(token), expr(expr->with_parent<ExprNode>(this)) {}

any BinaryNotOpExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type BinaryNotOpExpr::infer_type(AstContext &ctx) const {
  if (const Type exprType = expr->infer_type(ctx); !holds_alternative<BuiltinType>(exprType) || get<BuiltinType>(exprType) != Bool) {
    ctx.addError(yona_error(expr->source_context, yona_error::TYPE, "Expression for binary negation must be boolean"));
  }

  return Bool;
}

BinaryNotOpExpr::~BinaryNotOpExpr() { delete expr; }

void BinaryNotOpExpr::print(std::ostream &os) const { os << '~' << *expr; }

PowerExpr::PowerExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any PowerExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type PowerExpr::infer_type(AstContext &ctx) const { return BinaryOpExpr::infer_type(ctx); }

void PowerExpr::print(std::ostream &os) const { os << *left << "**" << *right; }

MultiplyExpr::MultiplyExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any MultiplyExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type MultiplyExpr::infer_type(AstContext &ctx) const { return BinaryOpExpr::infer_type(ctx); }

void MultiplyExpr::print(std::ostream &os) const { os << *left << "*" << *right; }

DivideExpr::DivideExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any DivideExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type DivideExpr::infer_type(AstContext &ctx) const { return BinaryOpExpr::infer_type(ctx); }

void DivideExpr::print(std::ostream &os) const { os << *left << "/" << *right; }

ModuloExpr::ModuloExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any ModuloExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ModuloExpr::infer_type(AstContext &ctx) const { return BinaryOpExpr::infer_type(ctx); }

void ModuloExpr::print(std::ostream &os) const { os << *left << "%" << *right; }

AddExpr::AddExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any AddExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type AddExpr::infer_type(AstContext &ctx) const { return BinaryOpExpr::infer_type(ctx); }

void AddExpr::print(std::ostream &os) const { os << *left << "+" << *right; }

SubtractExpr::SubtractExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any SubtractExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type SubtractExpr::infer_type(AstContext &ctx) const { return BinaryOpExpr::infer_type(ctx); }

void SubtractExpr::print(std::ostream &os) const { os << *left << "-" << *right; }

LeftShiftExpr::LeftShiftExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any LeftShiftExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type LeftShiftExpr::infer_type(AstContext &ctx) const {
  const Type lhs = left->infer_type(ctx);
  const Type rhs = right->infer_type(ctx);

  if (!is_integer(lhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for left shift must be integer"));
  }

  if (!is_integer(rhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for left shift must be integer"));
  }

  return derive_bin_op_result_type(lhs, rhs);
}

void LeftShiftExpr::print(std::ostream &os) const { os << *left << "<<" << *right; }

RightShiftExpr::RightShiftExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any RightShiftExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type RightShiftExpr::infer_type(AstContext &ctx) const {
  const Type lhs = left->infer_type(ctx);
  const Type rhs = right->infer_type(ctx);

  if (!is_integer(lhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for right shift must be integer"));
  }

  if (!is_integer(rhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for right shift must be integer"));
  }

  return derive_bin_op_result_type(lhs, rhs);
}

void RightShiftExpr::print(std::ostream &os) const { os << *left << ">>" << *right; }

ZerofillRightShiftExpr::ZerofillRightShiftExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any ZerofillRightShiftExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ZerofillRightShiftExpr::infer_type(AstContext &ctx) const {
  const Type lhs = left->infer_type(ctx);
  const Type rhs = right->infer_type(ctx);

  if (!is_integer(lhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for zerofill right shift must be integer"));
  }

  if (!is_integer(rhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for zerofill right shift must be integer"));
  }

  return derive_bin_op_result_type(lhs, rhs);
}

void ZerofillRightShiftExpr::print(std::ostream &os) const { os << *left << ">>>" << *right; }

GteExpr::GteExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any GteExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type GteExpr::infer_type(AstContext &ctx) const { return Bool; }

void GteExpr::print(std::ostream &os) const { os << *left << ">=" << *right; }

LteExpr::LteExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any LteExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type LteExpr::infer_type(AstContext &ctx) const { return Bool; }

void LteExpr::print(std::ostream &os) const { os << *left << "<=" << *right; }

GtExpr::GtExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any GtExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type GtExpr::infer_type(AstContext &ctx) const { return Bool; }

void GtExpr::print(std::ostream &os) const { os << *left << ">" << *right; }

LtExpr::LtExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any LtExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type LtExpr::infer_type(AstContext &ctx) const { return Bool; }

void LtExpr::print(std::ostream &os) const { os << *left << "<" << *right; }

EqExpr::EqExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any EqExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type EqExpr::infer_type(AstContext &ctx) const { return Bool; }

void EqExpr::print(std::ostream &os) const { os << *left << "==" << *right; }

NeqExpr::NeqExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any NeqExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type NeqExpr::infer_type(AstContext &ctx) const { return Bool; }

void NeqExpr::print(std::ostream &os) const { os << *left << "!=" << *right; }

ConsLeftExpr::ConsLeftExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any ConsLeftExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ConsLeftExpr::infer_type(AstContext &ctx) const { return Bool; }

void ConsLeftExpr::print(std::ostream &os) const { os << *left << "-|" << *right; }

ConsRightExpr::ConsRightExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any ConsRightExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ConsRightExpr::infer_type(AstContext &ctx) const { return Bool; }

void ConsRightExpr::print(std::ostream &os) const { os << *left << "|-" << *right; }

JoinExpr::JoinExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any JoinExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type JoinExpr::infer_type(AstContext &ctx) const {
  const Type lhs = left->infer_type(ctx);
  const Type rhs = right->infer_type(ctx);

  if (!holds_alternative<shared_ptr<SingleItemCollectionType>>(lhs) || !holds_alternative<shared_ptr<DictCollectionType>>(lhs) ||
      !holds_alternative<shared_ptr<ProductType>>(lhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Join expression can be used only for collections"));
  }

  if (!holds_alternative<shared_ptr<SingleItemCollectionType>>(rhs) || !holds_alternative<shared_ptr<DictCollectionType>>(rhs) ||
      !holds_alternative<shared_ptr<ProductType>>(rhs)) {
    ctx.addError(yona_error(right->source_context, yona_error::TYPE, "Join expression can be used only for collections"));
  }

  if (lhs != rhs) {
    ctx.addError(yona_error(source_context, yona_error::TYPE, "Join expression can be used only on same types"));
  }

  return lhs;
}

void JoinExpr::print(std::ostream &os) const { os << *left << "++" << *right; }

BitwiseAndExpr::BitwiseAndExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any BitwiseAndExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type BitwiseAndExpr::infer_type(AstContext &ctx) const {
  const Type lhs = left->infer_type(ctx);
  const Type rhs = right->infer_type(ctx);

  if (!is_integer(lhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for bitwise AND must be integer"));
  }

  if (!is_integer(rhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for bitwise AND must be integer"));
  }

  return derive_bin_op_result_type(lhs, rhs);
}

void BitwiseAndExpr::print(std::ostream &os) const { os << *left << "&" << *right; }

BitwiseXorExpr::BitwiseXorExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any BitwiseXorExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type BitwiseXorExpr::infer_type(AstContext &ctx) const {
  const Type lhs = left->infer_type(ctx);
  const Type rhs = right->infer_type(ctx);

  if (!is_integer(lhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for bitwise XOR must be integer"));
  }

  if (!is_integer(rhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for bitwise XOR must be integer"));
  }

  return Bool;
}

void BitwiseXorExpr::print(std::ostream &os) const { os << *left << "^" << *right; }

BitwiseOrExpr::BitwiseOrExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any BitwiseOrExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type BitwiseOrExpr::infer_type(AstContext &ctx) const {
  const Type lhs = left->infer_type(ctx);
  const Type rhs = right->infer_type(ctx);

  if (!is_integer(lhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for bitwise OR must be integer"));
  }

  if (!is_integer(rhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for bitwise OR must be integer"));
  }

  return Bool;
}

void BitwiseOrExpr::print(std::ostream &os) const { os << *left << "|" << *right; }

LogicalAndExpr::LogicalAndExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any LogicalAndExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type LogicalAndExpr::infer_type(AstContext &ctx) const {
  const Type lhs = left->infer_type(ctx);
  const Type rhs = right->infer_type(ctx);

  if (!holds_alternative<BuiltinType>(lhs) || get<BuiltinType>(lhs) != Bool) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for logical AND must be integer"));
  }

  if (!holds_alternative<BuiltinType>(rhs) || get<BuiltinType>(rhs) != Bool) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for logical AND must be integer"));
  }

  return Bool;
}

void LogicalAndExpr::print(std::ostream &os) const { os << *left << "&&" << *right; }

LogicalOrExpr::LogicalOrExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any LogicalOrExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type LogicalOrExpr::infer_type(AstContext &ctx) const {
  const Type lhs = left->infer_type(ctx);
  const Type rhs = right->infer_type(ctx);

  if (!holds_alternative<BuiltinType>(lhs) || get<BuiltinType>(lhs) != Bool) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for logical OR must be integer"));
  }

  if (!holds_alternative<BuiltinType>(rhs) || get<BuiltinType>(rhs) != Bool) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for logical OR must be integer"));
  }

  return Bool;
}

void LogicalOrExpr::print(std::ostream &os) const { os << *left << "||" << *right; }

InExpr::InExpr(SourceContext token, ExprNode *left, ExprNode *right) : BinaryOpExpr(token, left, right) {}

any InExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type InExpr::infer_type(AstContext &ctx) const {
  if (const Type rhs = right->infer_type(ctx); !holds_alternative<shared_ptr<SingleItemCollectionType>>(rhs) ||
                                               !holds_alternative<shared_ptr<DictCollectionType>>(rhs) ||
                                               !holds_alternative<shared_ptr<ProductType>>(rhs)) {
    ctx.addError(yona_error(left->source_context, yona_error::TYPE, "Expression for IN must be a collection"));
  }

  return Bool;
}

void InExpr::print(std::ostream &os) const { os << *left << " in " << *right; }

LetExpr::LetExpr(SourceContext token, vector<AliasExpr *> aliases, ExprNode *expr)
    : ScopedNode(token), aliases(nodes_with_parent(std::move(aliases), this)), expr(expr->with_parent<ExprNode>(this)) {}

any LetExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type LetExpr::infer_type(AstContext &ctx) const { return expr->infer_type(ctx); }

LetExpr::~LetExpr() {
  for (const auto p : aliases)
    delete p;
  delete expr;
}

void LetExpr::print(std::ostream &os) const {
  os << "let" << endl;
  for (const auto p : aliases) {
    os << "\t" << *p << endl;
  }
  os << "in" << endl << "\t" << *expr;
}

MainNode::MainNode(SourceContext token, AstNode *node) : ScopedNode(token), node(node->with_parent<AstNode>(this)) {}

any MainNode::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type MainNode::infer_type(AstContext &ctx) const { return node->infer_type(ctx); }

MainNode::~MainNode() { delete node; }

void MainNode::print(std::ostream &os) const { os << "main = " << *node; }

IfExpr::IfExpr(SourceContext token, ExprNode *condition, ExprNode *thenExpr, ExprNode *elseExpr)
    : ExprNode(token), condition(condition->with_parent<ExprNode>(this)), thenExpr(thenExpr->with_parent<ExprNode>(this)),
      elseExpr(elseExpr->with_parent<ExprNode>(this)) {}

any IfExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type IfExpr::infer_type(AstContext &ctx) const {
  if (const Type conditionType = condition->infer_type(ctx);
      !holds_alternative<BuiltinType>(conditionType) || get<BuiltinType>(conditionType) != Bool) {
    ctx.addError(yona_error(condition->source_context, yona_error::TYPE, "If condition must be boolean"));
  }

  unordered_set returnTypes{thenExpr->infer_type(ctx)};

  if (elseExpr != nullptr) {
    returnTypes.insert(elseExpr->infer_type(ctx));
  }

  return make_shared<SumType>(returnTypes);
}

IfExpr::~IfExpr() {
  delete condition;
  delete thenExpr;
  delete elseExpr;
}

void IfExpr::print(std::ostream &os) const {
  os << "if " << *condition << " then " << *thenExpr;
  if (elseExpr) {
    os << " else " << *elseExpr;
  }
}

ApplyExpr::ApplyExpr(SourceContext token, CallExpr *call, vector<variant<ExprNode *, ValueExpr *>> args)
    : ExprNode(token), call(call->with_parent<CallExpr>(this)), args(nodes_with_parent(std::move(args), this)) {}

any ApplyExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ApplyExpr::infer_type(AstContext &ctx) const {
  return nullptr; // TODO
}

ApplyExpr::~ApplyExpr() {
  delete call;
  for (const auto p : args) {
    if (holds_alternative<ExprNode *>(p)) {
      delete get<ExprNode *>(p);
    } else {
      delete get<ValueExpr *>(p);
    }
  }
}

void ApplyExpr::print(std::ostream &os) const {
  os << *call << ' ';
  size_t i = 0;
  for (const auto p : args) {
    if (holds_alternative<ExprNode *>(p)) {
      os << *get<ExprNode *>(p);
    } else {
      os << *get<ValueExpr *>(p);
    }
    if (i++ < args.size() - 1) {
      os << " ";
    }
  }
}

DoExpr::DoExpr(SourceContext token, vector<ExprNode *> steps) : ExprNode(token), steps(steps) {}

any DoExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type DoExpr::infer_type(AstContext &ctx) const { return steps.back()->infer_type(ctx); }

DoExpr::~DoExpr() {
  for (const auto p : steps) {
    delete p;
  }
}

void DoExpr::print(std::ostream &os) const {
  os << "do" << endl;
  for (const auto p : steps) {
    os << "  " << *p << endl;
  }
  os << "end";
}

ImportExpr::ImportExpr(SourceContext token, vector<ImportClauseExpr *> clauses, ExprNode *expr)
    : ScopedNode(token), clauses(nodes_with_parent(std::move(clauses), this)), expr(expr->with_parent<ExprNode>(this)) {}

any ImportExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ImportExpr::infer_type(AstContext &ctx) const { return expr->infer_type(ctx); }

ImportExpr::~ImportExpr() {
  for (const auto p : clauses) {
    delete p;
  }
  delete expr;
}

void ImportExpr::print(std::ostream &os) const {
  os << "import" << endl;
  for (const auto p : clauses) {
    os << "  " << *p << endl;
  }
  os << "in" << endl;
  os << "  " << *expr;
}

RaiseExpr::RaiseExpr(SourceContext token, SymbolExpr *symbol, StringExpr *message)
    : ExprNode(token), symbol(symbol->with_parent<SymbolExpr>(this)), message(message->with_parent<StringExpr>(this)) {}

any RaiseExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type RaiseExpr::infer_type(AstContext &ctx) const {
  return make_shared<NamedType>("exception", make_shared<ProductType>(vector{symbol->infer_type(ctx), message->infer_type(ctx)}));
}

RaiseExpr::~RaiseExpr() {
  delete symbol;
  delete message;
}

void RaiseExpr::print(std::ostream &os) const { os << "raise " << *symbol << " " << *message; }

WithExpr::WithExpr(SourceContext token, bool daemon, ExprNode *contextExpr, NameExpr *name, ExprNode *bodyExpr)
    : ScopedNode(token), daemon(daemon), contextExpr(contextExpr->with_parent<ExprNode>(this)), name(name->with_parent<NameExpr>(this)),
      bodyExpr(bodyExpr->with_parent<ExprNode>(this)) {}

any WithExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type WithExpr::infer_type(AstContext &ctx) const { return bodyExpr->infer_type(ctx); }

WithExpr::~WithExpr() {
  delete contextExpr;
  delete name;
  delete bodyExpr;
}

void WithExpr::print(std::ostream &os) const {
  os << "with ";
  if (daemon) {
    os << "daemon ";
  }
  os << *contextExpr;
  if (name) {
    os << " as " << *name;
  }
  os << endl << *bodyExpr << endl;
  os << "end";
}

FieldAccessExpr::FieldAccessExpr(SourceContext token, IdentifierExpr *identifier, NameExpr *name)
    : ExprNode(token), identifier(identifier->with_parent<IdentifierExpr>(this)), name(name->with_parent<NameExpr>(this)) {}

any FieldAccessExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type FieldAccessExpr::infer_type(AstContext &ctx) const {
  return nullptr; // TODO
}

FieldAccessExpr::~FieldAccessExpr() {
  delete identifier;
  delete name;
}

void FieldAccessExpr::print(std::ostream &os) const { os << *identifier << '.' << *name; }

FieldUpdateExpr::FieldUpdateExpr(SourceContext token, IdentifierExpr *identifier, vector<pair<NameExpr *, ExprNode *>> updates)
    : ExprNode(token), identifier(identifier->with_parent<IdentifierExpr>(this)), updates(nodes_with_parent(std::move(updates), this)) {}

any FieldUpdateExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type FieldUpdateExpr::infer_type(AstContext &ctx) const {
  return nullptr; // TODO
}

FieldUpdateExpr::~FieldUpdateExpr() {
  delete identifier;
  for (const auto [fst, snd] : updates) {
    delete fst;
    delete snd;
  }
}

void FieldUpdateExpr::print(std::ostream &os) const {
  os << *identifier << "(";
  size_t i = 0;
  for (const auto [fst, snd] : updates) {
    os << *fst << " = " << *snd;
    if (i++ < updates.size() - 1) {
      os << ", ";
    }
  }
  os << ")";
}

LambdaAlias::LambdaAlias(SourceContext token, NameExpr *name, FunctionExpr *lambda)
    : AliasExpr(token), name(name->with_parent<NameExpr>(this)), lambda(lambda->with_parent<FunctionExpr>(this)) {}

any LambdaAlias::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type LambdaAlias::infer_type(AstContext &ctx) const { return lambda->infer_type(ctx); }

LambdaAlias::~LambdaAlias() {
  delete name;
  delete lambda;
}

void LambdaAlias::print(std::ostream &os) const { os << *name << " = " << *lambda; }

ModuleAlias::ModuleAlias(SourceContext token, NameExpr *name, ModuleExpr *module)
    : AliasExpr(token), name(name->with_parent<NameExpr>(this)), module(module->with_parent<ModuleExpr>(this)) {}

any ModuleAlias::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ModuleAlias::infer_type(AstContext &ctx) const { return module->infer_type(ctx); }

ModuleAlias::~ModuleAlias() {
  delete name;
  delete module;
}

void ModuleAlias::print(std::ostream &os) const { os << *name << " = " << *module; }

ValueAlias::ValueAlias(SourceContext token, IdentifierExpr *identifier, ExprNode *expr)
    : AliasExpr(token), identifier(identifier->with_parent<IdentifierExpr>(this)), expr(expr->with_parent<ExprNode>(this)) {}

any ValueAlias::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ValueAlias::infer_type(AstContext &ctx) const { return expr->infer_type(ctx); }

ValueAlias::~ValueAlias() {
  delete identifier;
  delete expr;
}

void ValueAlias::print(std::ostream &os) const { os << *identifier << " = " << *expr; }

PatternAlias::PatternAlias(SourceContext token, PatternNode *pattern, ExprNode *expr)
    : AliasExpr(token), pattern(pattern->with_parent<PatternNode>(this)), expr(expr->with_parent<ExprNode>(this)) {}

any PatternAlias::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type PatternAlias::infer_type(AstContext &ctx) const { return expr->infer_type(ctx); }

PatternAlias::~PatternAlias() {
  delete pattern;
  delete expr;
}

void PatternAlias::print(std::ostream &os) const { os << *pattern << " = " << *expr; }

FqnAlias::FqnAlias(SourceContext token, NameExpr *name, FqnExpr *fqn)
    : AliasExpr(token), name(name->with_parent<NameExpr>(this)), fqn(fqn->with_parent<FqnExpr>(this)) {}

any FqnAlias::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type FqnAlias::infer_type(AstContext &ctx) const { return fqn->infer_type(ctx); }

FqnAlias::~FqnAlias() {
  delete name;
  delete fqn;
}

void FqnAlias::print(std::ostream &os) const { os << *name << " = " << *fqn; }

FunctionAlias::FunctionAlias(SourceContext token, NameExpr *name, NameExpr *alias)
    : AliasExpr(token), name(name->with_parent<NameExpr>(this)), alias(alias->with_parent<NameExpr>(this)) {}

any FunctionAlias::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type FunctionAlias::infer_type(AstContext &ctx) const { return alias->infer_type(ctx); }

FunctionAlias::~FunctionAlias() {
  delete name;
  delete alias;
}

void FunctionAlias::print(std::ostream &os) const { os << *name << " = " << *alias; }

AliasCall::AliasCall(SourceContext token, NameExpr *alias, NameExpr *funName)
    : CallExpr(token), alias(alias->with_parent<NameExpr>(this)), funName(funName->with_parent<NameExpr>(this)) {}

any AliasCall::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type AliasCall::infer_type(AstContext &ctx) const {
  return nullptr; // TODO
}

AliasCall::~AliasCall() {
  delete alias;
  delete funName;
}

void AliasCall::print(std::ostream &os) const { os << *alias << "::" << *funName; }

NameCall::NameCall(SourceContext token, NameExpr *name) : CallExpr(token), name(name->with_parent<NameExpr>(this)) {}

any NameCall::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type NameCall::infer_type(AstContext &ctx) const {
  return nullptr; // TODO
}

NameCall::~NameCall() { delete name; }

void NameCall::print(std::ostream &os) const { os << *name; }

ModuleCall::ModuleCall(SourceContext token, const variant<FqnExpr *, ExprNode *> &fqn, NameExpr *funName)
    : CallExpr(token), fqn(node_with_parent(fqn, this)), funName(funName->with_parent<NameExpr>(this)) {}

any ModuleCall::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ModuleCall::infer_type(AstContext &ctx) const {
  return nullptr; // TODO
}

ModuleCall::~ModuleCall() {
  if (holds_alternative<FqnExpr *>(fqn)) {
    delete get<FqnExpr *>(fqn);
  } else {
    delete get<ExprNode *>(fqn);
  }
  delete funName;
}

void ModuleCall::print(std::ostream &os) const {
  if (holds_alternative<FqnExpr *>(fqn)) {
    os << *get<FqnExpr *>(fqn);
  } else {
    os << *get<ExprNode *>(fqn);
  }
  os << "::" << *funName;
}

ModuleImport::ModuleImport(SourceContext token, FqnExpr *fqn, NameExpr *name)
    : ImportClauseExpr(token), fqn(fqn->with_parent<FqnExpr>(this)), name(name->with_parent<NameExpr>(this)) {}

any ModuleImport::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ModuleImport::infer_type(AstContext &ctx) const {
  return nullptr; // TODO
}

ModuleImport::~ModuleImport() {
  delete fqn;
  delete name;
}

void ModuleImport::print(std::ostream &os) const {
  os << *fqn;
  if (name) {
    os << " as " << *name;
  }
}

FunctionsImport::FunctionsImport(SourceContext token, vector<FunctionAlias *> aliases, FqnExpr *fromFqn)
    : ImportClauseExpr(token), aliases(nodes_with_parent(std::move(aliases), this)), fromFqn(fromFqn->with_parent<FqnExpr>(this)) {}

any FunctionsImport::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type FunctionsImport::infer_type(AstContext &ctx) const {
  return nullptr; // TODO
}

FunctionsImport::~FunctionsImport() {
  for (const auto p : aliases) {
    delete p;
  }
  delete fromFqn;
}

void FunctionsImport::print(std::ostream &os) const {
  size_t i = 0;
  for (const auto p : aliases) {
    os << *p;
    if (i++ < aliases.size() - 1) {
      os << ", ";
    }
  }
  os << " from " << *fromFqn;
}

SeqGeneratorExpr::SeqGeneratorExpr(SourceContext token, ExprNode *reducerExpr, CollectionExtractorExpr *collectionExtractor, ExprNode *stepExpression)
    : GeneratorExpr(token), reducerExpr(reducerExpr->with_parent<ExprNode>(this)),
      collectionExtractor(collectionExtractor->with_parent<CollectionExtractorExpr>(this)),
      stepExpression(stepExpression->with_parent<ExprNode>(this)) {}

any SeqGeneratorExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type SeqGeneratorExpr::infer_type(AstContext &ctx) const {
  return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Seq, reducerExpr->infer_type(ctx));
}

SeqGeneratorExpr::~SeqGeneratorExpr() {
  delete reducerExpr;
  delete collectionExtractor;
  delete stepExpression;
}

void SeqGeneratorExpr::print(std::ostream &os) const {
  os << "[" << *reducerExpr << " | " << *collectionExtractor << " <- " << *stepExpression << "]";
}

SetGeneratorExpr::SetGeneratorExpr(SourceContext token, ExprNode *reducerExpr, CollectionExtractorExpr *collectionExtractor, ExprNode *stepExpression)
    : GeneratorExpr(token), reducerExpr(reducerExpr->with_parent<ExprNode>(this)),
      collectionExtractor(collectionExtractor->with_parent<CollectionExtractorExpr>(this)),
      stepExpression(stepExpression->with_parent<ExprNode>(this)) {}

any SetGeneratorExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type SetGeneratorExpr::infer_type(AstContext &ctx) const {
  return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Set, reducerExpr->infer_type(ctx));
}

SetGeneratorExpr::~SetGeneratorExpr() {
  delete reducerExpr;
  delete collectionExtractor;
  delete stepExpression;
}

void SetGeneratorExpr::print(std::ostream &os) const {
  os << "{" << *reducerExpr << " | " << *collectionExtractor << " <- " << *stepExpression << "}";
}

DictGeneratorReducer::DictGeneratorReducer(SourceContext token, ExprNode *key, ExprNode *value)
    : ExprNode(token), key(key->with_parent<ExprNode>(this)), value(value->with_parent<ExprNode>(this)) {}

any DictGeneratorReducer::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type DictGeneratorReducer::infer_type(AstContext &ctx) const { unreachable(); }

DictGeneratorReducer::~DictGeneratorReducer() {
  delete key;
  delete value;
}

void DictGeneratorReducer::print(std::ostream &os) const { os << *key << " = " << *value; }

DictGeneratorExpr::DictGeneratorExpr(SourceContext token, DictGeneratorReducer *reducerExpr, CollectionExtractorExpr *collectionExtractor,
                                     ExprNode *stepExpression)
    : GeneratorExpr(token), reducerExpr(reducerExpr->with_parent<DictGeneratorReducer>(this)),
      collectionExtractor(collectionExtractor->with_parent<CollectionExtractorExpr>(this)),
      stepExpression(stepExpression->with_parent<ExprNode>(this)) {}

any DictGeneratorExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type DictGeneratorExpr::infer_type(AstContext &ctx) const {
  return make_shared<DictCollectionType>(reducerExpr->key->infer_type(ctx), reducerExpr->value->infer_type(ctx));
}

DictGeneratorExpr::~DictGeneratorExpr() {
  delete reducerExpr;
  delete collectionExtractor;
  delete stepExpression;
}

void DictGeneratorExpr::print(std::ostream &os) const {
  os << "{" << *reducerExpr << " | " << *collectionExtractor << " <- " << *stepExpression << "}";
}

ValueCollectionExtractorExpr::ValueCollectionExtractorExpr(SourceContext token, const IdentifierOrUnderscore identifier_or_underscore)
    : CollectionExtractorExpr(token), expr(node_with_parent(identifier_or_underscore, this)) {}

any ValueCollectionExtractorExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type ValueCollectionExtractorExpr::infer_type_identifier_or_underscore(AstContext &ctx) const {
  if (holds_alternative<IdentifierExpr *>(expr)) {
    return get<IdentifierExpr *>(expr)->infer_type(ctx);
  } else {
    return get<UnderscoreNode *>(expr)->infer_type(ctx);
  }
}

Type ValueCollectionExtractorExpr::infer_type(AstContext &ctx) const { return infer_type_identifier_or_underscore(ctx); }

void release_identifier_or_underscore(const IdentifierOrUnderscore expr) {
  if (holds_alternative<IdentifierExpr *>(expr)) {
    delete get<IdentifierExpr *>(expr);
  } else {
    delete get<UnderscoreNode *>(expr);
  }
}

ValueCollectionExtractorExpr::~ValueCollectionExtractorExpr() { release_identifier_or_underscore(expr); }

void ValueCollectionExtractorExpr::print(std::ostream &os) const {
  if (holds_alternative<IdentifierExpr *>(expr)) {
    os << *get<IdentifierExpr *>(expr);
  } else {
    os << *get<UnderscoreNode *>(expr);
  }
}

KeyValueCollectionExtractorExpr::KeyValueCollectionExtractorExpr(SourceContext token, const IdentifierOrUnderscore keyExpr,
                                                                 const IdentifierOrUnderscore valueExpr)
    : CollectionExtractorExpr(token), keyExpr(node_with_parent(keyExpr, this)), valueExpr(node_with_parent(valueExpr, this)) {}

any KeyValueCollectionExtractorExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type KeyValueCollectionExtractorExpr::infer_type(AstContext &ctx) const { unreachable(); }

KeyValueCollectionExtractorExpr::~KeyValueCollectionExtractorExpr() {
  release_identifier_or_underscore(keyExpr);
  release_identifier_or_underscore(valueExpr);
}

void KeyValueCollectionExtractorExpr::print(std::ostream &os) const {
  if (holds_alternative<IdentifierExpr *>(keyExpr)) {
    os << *get<IdentifierExpr *>(keyExpr);
  } else {
    os << *get<UnderscoreNode *>(keyExpr);
  }
  os << " = ";
  if (holds_alternative<IdentifierExpr *>(valueExpr)) {
    os << *get<IdentifierExpr *>(valueExpr);
  } else {
    os << *get<UnderscoreNode *>(valueExpr);
  }
}

PatternWithGuards::PatternWithGuards(SourceContext token, ExprNode *guard, ExprNode *expr)
    : PatternNode(token), guard(guard->with_parent<ExprNode>(this)), expr(expr->with_parent<ExprNode>(this)) {}

any PatternWithGuards::accept(const AstVisitor &visitor) { return visitor.visit(this); };

Type PatternWithGuards::infer_type(AstContext &ctx) const { unreachable(); }

PatternWithGuards::~PatternWithGuards() {
  delete guard;
  delete expr;
}

void PatternWithGuards::print(std::ostream &os) const { os << *expr << " if " << *guard; }

PatternWithoutGuards::PatternWithoutGuards(SourceContext token, ExprNode *expr) : PatternNode(token), expr(expr->with_parent<ExprNode>(this)) {}

any PatternWithoutGuards::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type PatternWithoutGuards::infer_type(AstContext &ctx) const { return expr->infer_type(ctx); }

PatternWithoutGuards::~PatternWithoutGuards() { delete expr; }

void PatternWithoutGuards::print(std::ostream &os) const { os << *expr; }

PatternExpr::PatternExpr(SourceContext token, const variant<Pattern *, PatternWithoutGuards *, vector<PatternWithGuards *>> &patternExpr)
    : ExprNode(token), patternExpr(patternExpr) // TODO
{
  // std::visit({ [this](Pattern& arg) { arg.with_parent<Pattern>(this); },
  //              [this](PatternWithoutGuards& arg) {
  //              arg.with_parent<PatternWithGuards>(this); },
  //              [this](vector<PatternWithGuards>& arg) {
  //              nodes_with_parent(std::move(arg), this); } },
  //            patternExpr); // TODO
}
any PatternExpr::accept(const AstVisitor &visitor) { return ExprNode::accept(visitor); }

Type PatternExpr::infer_type(AstContext &ctx) const { unreachable(); }

PatternExpr::~PatternExpr() {
  if (holds_alternative<Pattern *>(patternExpr)) {
    delete get<Pattern *>(patternExpr);
  } else if (holds_alternative<PatternWithoutGuards *>(patternExpr)) {
    delete get<PatternWithoutGuards *>(patternExpr);
  } else {
    for (const auto p : get<vector<PatternWithGuards *>>(patternExpr)) {
      delete p;
    }
  }
}

void PatternExpr::print(std::ostream &os) const {
  if (holds_alternative<Pattern *>(patternExpr)) {
    os << *get<Pattern *>(patternExpr);
  } else if (holds_alternative<PatternWithoutGuards *>(patternExpr)) {
    os << *get<PatternWithoutGuards *>(patternExpr);
  } else {
    size_t i = 0;
    for (const auto p : get<vector<PatternWithGuards *>>(patternExpr)) {
      os << *p;
      if (i++ < get<vector<PatternWithGuards *>>(patternExpr).size() - 1) {
        os << endl;
      }
    }
  }
}

CatchPatternExpr::CatchPatternExpr(SourceContext token, Pattern *matchPattern,
                                   const variant<PatternWithoutGuards *, vector<PatternWithGuards *>> &pattern)
    : ExprNode(token), matchPattern(matchPattern->with_parent<Pattern>(this)), pattern(pattern) {
  // std::visit({ [this](PatternWithoutGuards& arg) {
  // arg.with_parent<PatternWithGuards>(this); },
  //              [this](vector<PatternWithGuards>& arg) {
  //              nodes_with_parent(std::move(arg), this); } },
  //            pattern); // TODO
}

any CatchPatternExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type CatchPatternExpr::infer_type(AstContext &ctx) const { unreachable(); }

CatchPatternExpr::~CatchPatternExpr() {
  delete matchPattern;
  if (holds_alternative<PatternWithoutGuards *>(pattern)) {
    delete get<PatternWithoutGuards *>(pattern);
  } else {
    for (const auto p : get<vector<PatternWithGuards *>>(pattern)) {
      delete p;
    }
  }
}

void CatchPatternExpr::print(std::ostream &os) const {
  os << *matchPattern << " ";
  if (holds_alternative<PatternWithoutGuards *>(pattern)) {
    os << *get<PatternWithoutGuards *>(pattern);
  } else {
    size_t i = 0;
    for (const auto p : get<vector<PatternWithGuards *>>(pattern)) {
      os << *p;
      if (i++ < get<vector<PatternWithGuards *>>(pattern).size() - 1) {
        os << endl;
      }
    }
  }
}

CatchExpr::CatchExpr(SourceContext token, vector<CatchPatternExpr *> patterns)
    : ExprNode(token), patterns(nodes_with_parent(std::move(patterns), this)) {}

any CatchExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type CatchExpr::infer_type(AstContext &ctx) const { return patterns.back()->infer_type(ctx); }

CatchExpr::~CatchExpr() {
  for (const auto p : patterns) {
    delete p;
  }
}

void CatchExpr::print(std::ostream &os) const {
  size_t i = 0;
  for (const auto p : patterns) {
    os << *p;
    if (i++ < patterns.size() - 1) {
      os << endl;
    }
  }
}

TryCatchExpr::TryCatchExpr(SourceContext token, ExprNode *tryExpr, CatchExpr *catchExpr)
    : ExprNode(token), tryExpr(tryExpr->with_parent<ExprNode>(this)), catchExpr(catchExpr->with_parent<CatchExpr>(this)) {}

any TryCatchExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type TryCatchExpr::infer_type(AstContext &ctx) const {
  return make_shared<SumType>(unordered_set{tryExpr->infer_type(ctx), catchExpr->infer_type(ctx)});
}

TryCatchExpr::~TryCatchExpr() {
  delete tryExpr;
  delete catchExpr;
}

void TryCatchExpr::print(std::ostream &os) const { os << "try" << endl << "  " << *tryExpr << endl << "catch" << endl << "  " << *catchExpr << endl; }

PatternValue::PatternValue(SourceContext token, const variant<LiteralExpr<nullptr_t> *, LiteralExpr<void *> *, SymbolExpr *, IdentifierExpr *> &expr)
    : PatternNode(token), expr(node_with_parent(expr, this)) {}

any PatternValue::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type PatternValue::infer_type(AstContext &ctx) const { unreachable(); }

PatternValue::~PatternValue() {
  if (holds_alternative<LiteralExpr<nullptr_t> *>(expr)) {
    delete get<LiteralExpr<nullptr_t> *>(expr);
  } else if (holds_alternative<LiteralExpr<void *> *>(expr)) {
    delete get<LiteralExpr<void *> *>(expr);
  } else if (holds_alternative<SymbolExpr *>(expr)) {
    delete get<SymbolExpr *>(expr);
  }
}

void PatternValue::print(std::ostream &os) const {
  if (holds_alternative<LiteralExpr<nullptr_t> *>(expr)) {
    os << *get<LiteralExpr<nullptr_t> *>(expr);
  } else if (holds_alternative<LiteralExpr<void *> *>(expr)) {
    os << *get<LiteralExpr<void *> *>(expr);
  } else if (holds_alternative<SymbolExpr *>(expr)) {
    os << *get<SymbolExpr *>(expr);
  } else {
    os << *get<IdentifierExpr *>(expr);
  }
}

AsDataStructurePattern::AsDataStructurePattern(SourceContext token, IdentifierExpr *identifier, DataStructurePattern *pattern)
    : PatternNode(token), identifier(identifier->with_parent<IdentifierExpr>(this)), pattern(pattern->with_parent<DataStructurePattern>(this)) {}

any AsDataStructurePattern::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type AsDataStructurePattern::infer_type(AstContext &ctx) const { return pattern->infer_type(ctx); }

AsDataStructurePattern::~AsDataStructurePattern() {
  delete identifier;
  delete pattern;
}

void AsDataStructurePattern::print(std::ostream &os) const { os << *identifier << "@(" << *pattern << ")"; }

any UnderscorePattern::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type UnderscorePattern::infer_type(AstContext &ctx) const { unreachable(); }

void UnderscorePattern::print(std::ostream &os) const { os << '_'; }

TuplePattern::TuplePattern(SourceContext token, vector<Pattern *> patterns)
    : PatternNode(token), patterns(nodes_with_parent(std::move(patterns), this)) {}

any TuplePattern::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type TuplePattern::infer_type(AstContext &ctx) const { unreachable(); }

TuplePattern::~TuplePattern() {
  for (const auto p : patterns) {
    delete p;
  }
}

void TuplePattern::print(std::ostream &os) const {
  os << '(';
  size_t i = 0;
  for (const auto p : patterns) {
    os << *p;
    if (i++ < patterns.size() - 1) {
      os << ", ";
    }
  }
  os << ')';
}

SeqPattern::SeqPattern(SourceContext token, vector<Pattern *> patterns)
    : PatternNode(token), patterns(nodes_with_parent(std::move(patterns), this)) {}

any SeqPattern::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type SeqPattern::infer_type(AstContext &ctx) const { unreachable(); }

SeqPattern::~SeqPattern() {
  for (const auto p : patterns) {
    delete p;
  }
}

void SeqPattern::print(std::ostream &os) const {
  os << '[';
  size_t i = 0;
  for (const auto p : patterns) {
    os << *p;
    if (i++ < patterns.size() - 1) {
      os << ", ";
    }
  }
  os << ']';
}

HeadTailsPattern::HeadTailsPattern(SourceContext token, vector<PatternWithoutSequence *> heads, TailPattern *tail)
    : PatternNode(token), heads(nodes_with_parent(std::move(heads), this)), tail(tail->with_parent<TailPattern>(this)) {}

any HeadTailsPattern::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type HeadTailsPattern::infer_type(AstContext &ctx) const { unreachable(); }

HeadTailsPattern::~HeadTailsPattern() {
  for (const auto p : heads) {
    delete p;
  }
  delete tail;
}

void HeadTailsPattern::print(std::ostream &os) const {
  size_t i = 0;
  for (const auto p : heads) {
    os << *p;
    if (i++ < heads.size() - 1) {
      os << ", ";
    }
  }
  os << " -| ";
  os << *tail;
}

TailsHeadPattern::TailsHeadPattern(SourceContext token, TailPattern *tail, vector<PatternWithoutSequence *> heads)
    : PatternNode(token), tail(tail->with_parent<TailPattern>(this)), heads(nodes_with_parent(std::move(heads), this)) {}

any TailsHeadPattern::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type TailsHeadPattern::infer_type(AstContext &ctx) const { unreachable(); }

TailsHeadPattern::~TailsHeadPattern() {
  delete tail;
  for (const auto p : heads) {
    delete p;
  }
}

void TailsHeadPattern::print(std::ostream &os) const {
  os << *tail;
  os << " |- ";
  size_t i = 0;
  for (const auto p : heads) {
    os << *p;
    if (i++ < heads.size() - 1) {
      os << ", ";
    }
  }
}

HeadTailsHeadPattern::HeadTailsHeadPattern(SourceContext token, vector<PatternWithoutSequence *> left, TailPattern *tail,
                                           vector<PatternWithoutSequence *> right)
    : PatternNode(token), left(nodes_with_parent(std::move(left), this)), tail(tail->with_parent<TailPattern>(this)),
      right(nodes_with_parent(std::move(right), this)) {}

any HeadTailsHeadPattern::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type HeadTailsHeadPattern::infer_type(AstContext &ctx) const { unreachable(); }

HeadTailsHeadPattern::~HeadTailsHeadPattern() {
  for (const auto p : left) {
    delete p;
  }
  delete tail;
  for (const auto p : right) {
    delete p;
  }
}

void HeadTailsHeadPattern::print(std::ostream &os) const {
  size_t i = 0;
  for (const auto p : left) {
    os << *p;
    if (i++ < left.size() - 1) {
      os << ", ";
    }
  }
  os << " -| " << tail << " |- ";
  os << *tail;
  i = 0;
  for (const auto p : right) {
    os << *p;
    if (i++ < right.size() - 1) {
      os << ", ";
    }
  }
}

DictPattern::DictPattern(SourceContext token, vector<pair<PatternValue *, Pattern *>> keyValuePairs)
    : PatternNode(token), keyValuePairs(nodes_with_parent(std::move(keyValuePairs), this)) {}

any DictPattern::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type DictPattern::infer_type(AstContext &ctx) const { unreachable(); }

DictPattern::~DictPattern() {
  for (const auto [fst, snd] : keyValuePairs) {
    delete fst;
    delete snd;
  }
}

void DictPattern::print(std::ostream &os) const {
  os << '{';
  size_t i = 0;
  for (const auto [fst, snd] : keyValuePairs) {
    os << *fst << " = " << *snd;
    if (i++ < keyValuePairs.size() - 1) {
      os << ", ";
    }
  }
  os << '}';
}

RecordPattern::RecordPattern(SourceContext token, string recordType, vector<pair<NameExpr *, Pattern *>> items)
    : PatternNode(token), recordType(std::move(recordType)), items(nodes_with_parent(std::move(items), this)) {}

any RecordPattern::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type RecordPattern::infer_type(AstContext &ctx) const { unreachable(); }

RecordPattern::~RecordPattern() {
  for (const auto [fst, snd] : items) {
    delete fst;
    delete snd;
  }
}

void RecordPattern::print(std::ostream &os) const {
  os << recordType << " (";
  size_t i = 0;
  for (const auto [fst, snd] : items) {
    os << *fst << " = " << *snd;
    if (i++ < items.size() - 1) {
      os << ", ";
    }
  }
  os << ')';
}

CaseExpr::CaseExpr(SourceContext token, ExprNode *expr, vector<PatternExpr *> patterns)
    : ExprNode(token), expr(expr->with_parent<ExprNode>(this)), patterns(nodes_with_parent(std::move(patterns), this)) {}

any CaseExpr::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type CaseExpr::infer_type(AstContext &ctx) const {
  unordered_set<Type> types;
  for (const auto p : patterns) {
    types.insert(p->infer_type(ctx));
  }
  return make_shared<SumType>(types);
}

CaseExpr::~CaseExpr() {
  delete expr;
  for (const auto p : patterns) {
    delete p;
  }
}

void CaseExpr::print(std::ostream &os) const {
  os << "case " << *expr << " of" << endl;
  for (const auto p : patterns) {
    os << "  " << *p << endl;
  }
}

any TypeNameNode::accept(const AstVisitor &visitor) { return visitor.visit(this); }

void BuiltinTypeNode::print(std::ostream &os) const { os << BuiltinTypeStrings[type]; }

any BuiltinTypeNode::accept(const AstVisitor &visitor) { return visitor.visit(this); }

UserDefinedTypeNode::UserDefinedTypeNode(SourceContext token, NameExpr *name) : TypeNameNode(token), name(name->with_parent<NameExpr>(this)) {}

any UserDefinedTypeNode::accept(const AstVisitor &visitor) { return visitor.visit(this); }

UserDefinedTypeNode::~UserDefinedTypeNode() { delete name; }

void UserDefinedTypeNode::print(std::ostream &os) const { os << *name; }

TypeDeclaration::TypeDeclaration(SourceContext token, TypeNameNode *name, vector<NameExpr *> type_vars)
    : AstNode(token), name(name->with_parent<TypeNameNode>(this)), typeVars(nodes_with_parent(std::move(type_vars), this)) {}

any TypeDeclaration::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type TypeDeclaration::infer_type(AstContext &ctx) const { unreachable(); }

TypeDeclaration::~TypeDeclaration() {
  delete name;
  for (const auto p : typeVars) {
    delete p;
  }
}

void TypeDeclaration::print(std::ostream &os) const {
  os << *name << " ";
  size_t i = 0;
  for (const auto p : typeVars) {
    os << *p;
    if (i++ < typeVars.size() - 1) {
      os << " ";
    }
  }
}

TypeDefinition::TypeDefinition(SourceContext token, TypeNameNode *name, vector<TypeNameNode *> type_names)
    : AstNode(token), name(name->with_parent<TypeNameNode>(this)), typeNames(nodes_with_parent(std::move(type_names), this)) {}

any TypeDefinition::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type TypeDefinition::infer_type(AstContext &ctx) const {
  vector<Type> type_names;
  type_names.reserve(typeNames.size());
  for (const auto t : typeNames) {
    delete t;
  }
  // return make_shared<NamedType>(name->, make_shared<ProductType>(type_names)); // TODO
  return nullptr;
}

TypeDefinition::~TypeDefinition() {
  delete name;
  for (const auto p : typeNames) {
    delete p;
  }
}

void TypeDefinition::print(std::ostream &os) const {
  os << *name;
  size_t i = 0;
  for (const auto p : typeNames) {
    os << " " << *p;
    if (i++ < typeNames.size() - 1) {
      os << " ";
    }
  }
}

TypeNode::TypeNode(SourceContext token, TypeDeclaration *declaration, vector<TypeDeclaration *> definitions)
    : AstNode(token), declaration(declaration->with_parent<TypeDeclaration>(this)), definitions(nodes_with_parent(std::move(definitions), this)) {}

any TypeNode::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type TypeNode::infer_type(AstContext &ctx) const { unreachable(); }

TypeNode::~TypeNode() {
  delete declaration;
  for (const auto p : definitions) {
    delete p;
  }
}

void TypeNode::print(std::ostream &os) const {
  os << "type " << *declaration << " = ";
  size_t i = 0;
  for (const auto p : definitions) {
    os << *p;
    if (i++ < definitions.size() - 1) {
      os << "| " << endl;
    }
  }
}

TypeInstance::TypeInstance(SourceContext token, TypeNameNode *name, vector<ExprNode *> exprs)
    : AstNode(token), name(name->with_parent<TypeNameNode>(this)), exprs(nodes_with_parent(std::move(exprs), this)) {}

any TypeInstance::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type TypeInstance::infer_type(AstContext &ctx) const { unreachable(); }

TypeInstance::~TypeInstance() {
  delete name;
  for (const auto p : exprs) {
    delete p;
  }
}

void TypeInstance::print(std::ostream &os) const {
  os << *name << "(";
  size_t i = 0;
  for (const auto p : exprs) {
    os << *p;
    if (i++ < exprs.size() - 1) {
      os << ", ";
    }
  }
  os << ')';
}

FunctionDeclaration::FunctionDeclaration(SourceContext token, NameExpr *function_name, vector<TypeDefinition *> type_definitions)
    : AstNode(token), functionName(function_name->with_parent<NameExpr>(this)),
      typeDefinitions(nodes_with_parent(std::move(type_definitions), this)) {}

any FunctionDeclaration::accept(const AstVisitor &visitor) { return visitor.visit(this); }

Type FunctionDeclaration::infer_type(AstContext &ctx) const { unreachable(); }

FunctionDeclaration::~FunctionDeclaration() {
  delete functionName;
  for (const auto p : typeDefinitions) {
    delete p;
  }
}

void FunctionDeclaration::print(std::ostream &os) const {
  os << *functionName << " :: ";
  size_t i = 0;
  for (const auto p : typeDefinitions) {
    os << *p;
    if (i++ < typeDefinitions.size() - 1) {
      os << " -> ";
    }
  }
}

any AstVisitor::visit(ExprNode *node) const {
  if (const auto derived = dynamic_cast<AliasExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ApplyExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<CallExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<CaseExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<CatchExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<CatchPatternExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<CollectionExtractorExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<DictGeneratorReducer *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<DoExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<FieldAccessExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<FieldUpdateExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<GeneratorExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<IfExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<NameExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<OpExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<PatternExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<RaiseExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<SequenceExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<TryCatchExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ValueExpr *>(node)) {
    return visit(derived);
  }
  unreachable();
}

any AstVisitor::visit(AstNode *node) const {
  if (const auto derived = dynamic_cast<ExprNode *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<PatternNode *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ScopedNode *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<FunctionBody *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<RecordNode *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<TypeNode *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<TypeNameNode *>(node)) {
    return visit(derived);
  }
  unreachable();
}

any AstVisitor::visit(ScopedNode *node) const {
  if (const auto derived = dynamic_cast<ImportClauseExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<FunctionExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<LetExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<MainNode *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<WithExpr *>(node)) {
    return visit(derived);
  }
  unreachable();
}

any AstVisitor::visit(PatternNode *node) const {
  if (const auto derived = dynamic_cast<UnderscoreNode *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<UnderscorePattern *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<PatternWithGuards *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<PatternWithoutGuards *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<PatternValue *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<AsDataStructurePattern *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<TuplePattern *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<SeqPattern *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<HeadTailsPattern *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<TailsHeadPattern *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<HeadTailsHeadPattern *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<DictPattern *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<RecordPattern *>(node)) {
    return visit(derived);
  }
  unreachable();
}

any AstVisitor::visit(ValueExpr *node) const {
  if (const auto derived = dynamic_cast<DictExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<FqnExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<IdentifierExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ByteExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<CharacterExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<FalseLiteralExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<FloatExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<IntegerExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<StringExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<TrueLiteralExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<UnitExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ModuleExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<PackageNameExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<RecordInstanceExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<SetExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<SymbolExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<TupleExpr *>(node)) {
    return visit(derived);
  }
  unreachable();
}

any AstVisitor::visit(SequenceExpr *node) const {
  if (const auto derived = dynamic_cast<RangeSequenceExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ValuesSequenceExpr *>(node)) {
    return visit(derived);
  }
  unreachable();
}

any AstVisitor::visit(FunctionBody *node) const {
  if (const auto derived = dynamic_cast<BodyWithGuards *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<BodyWithoutGuards *>(node)) {
    return visit(derived);
  }
  unreachable();
}

any AstVisitor::visit(AliasExpr *node) const {
  if (const auto derived = dynamic_cast<ValueAlias *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<LambdaAlias *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ModuleAlias *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<PatternAlias *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<FqnAlias *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<FunctionAlias *>(node)) {
    return visit(derived);
  }
  unreachable();
}

any AstVisitor::visit(OpExpr *node) const {
  if (const auto derived = dynamic_cast<BinaryOpExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<LogicalNotOpExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<BinaryNotOpExpr *>(node)) {
    return visit(derived);
  }
  unreachable();
}

any AstVisitor::visit(BinaryOpExpr *node) const {
  if (const auto derived = dynamic_cast<PowerExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<MultiplyExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<DivideExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ModuloExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<AddExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<SubtractExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<LeftShiftExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<RightShiftExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ZerofillRightShiftExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<GteExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<LteExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<GtExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<LtExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<EqExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<NeqExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ConsLeftExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<ConsRightExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<JoinExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<BitwiseAndExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<BitwiseXorExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<BitwiseOrExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<LogicalAndExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<LogicalOrExpr *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<InExpr *>(node)) {
    return visit(derived);
  }
  unreachable();
}

any AstVisitor::visit(TypeNameNode *node) const {
  if (const auto derived = dynamic_cast<BuiltinTypeNode *>(node)) {
    return visit(derived);
  }
  if (const auto derived = dynamic_cast<UserDefinedTypeNode *>(node)) {
    return visit(derived);
  }
  unreachable();
}

std::ostream &operator<<(std::ostream &os, const AstNode &obj) {
  obj.print(os);
  return os;
}
} // namespace yona::ast
