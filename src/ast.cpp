#include "ast.h"

#include <variant>

namespace yona::ast
{
    template <typename T>
    LiteralExpr<T>::LiteralExpr(Token token, T value) : ValueExpr(token), value(std::move(value))
    {
    }

    template <typename T>
    any LiteralExpr<T>::accept(const AstVisitor& visitor)
    {
        return ValueExpr::accept(visitor);
    }

    any AstNode::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type AstNode::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    ScopedNode* ScopedNode::getParentScopedNode() const
    {
        if (parent == nullptr)
        {
            return nullptr;
        }
        else
        {
            AstNode* tmp = parent;
            do
            {
                if (const auto result = dynamic_cast<ScopedNode*>(tmp); result != nullptr)
                {
                    return result;
                }
                tmp = tmp->parent;
            }
            while (tmp != nullptr);
            return nullptr;
        }
    }

    any ScopedNode::accept(const AstVisitor& visitor) { return AstNode::accept(visitor); }

    any OpExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }

    BinaryOpExpr::BinaryOpExpr(Token token, ExprNode* left, ExprNode* right) :
        OpExpr(token), left(left->with_parent<ExprNode>(this)), right(right->with_parent<ExprNode>(this))
    {
    }

    Type BinaryOpExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left->infer_type(ctx);
        const Type rightType = right->infer_type(ctx);

        if (holds_alternative<ValueType>(leftType) && get<ValueType>(leftType) != Int && get<ValueType>(leftType) != Float)
        {
            ctx.addError(TypeError(token, "Binary expression must be numeric type"));
        }

        if (holds_alternative<ValueType>(rightType) && get<ValueType>(rightType) != Int && get<ValueType>(rightType) != Float)
        {
            ctx.addError(TypeError(token, "Binary expression must be numeric type"));
        }

        return unordered_set{ leftType, rightType }.contains(Float) ? Float : Int;
    }

    any BinaryOpExpr::accept(const AstVisitor& visitor) { return OpExpr::accept(visitor); }
    BinaryOpExpr::~BinaryOpExpr()
    {
        delete left;
        delete right;
    }

    NameExpr::NameExpr(Token token, string value) : ExprNode(token), value(std::move(value)) {}

    any NameExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type NameExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    Type AliasExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    any AliasExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }

    Type CallExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    any CallExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }
    any ImportClauseExpr::accept(const AstVisitor& visitor) { return ScopedNode::accept(visitor); }

    any GeneratorExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }

    any CollectionExtractorExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }
    any SequenceExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }
    any FunctionBody::accept(const AstVisitor& visitor) { return AstNode::accept(visitor); }

    IdentifierExpr::IdentifierExpr(Token token, NameExpr* name) :
        ValueExpr(token), name(name->with_parent<NameExpr>(this))
    {
    }

    any IdentifierExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type IdentifierExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    IdentifierExpr::~IdentifierExpr() { delete name; }

    RecordNode::RecordNode(Token token, NameExpr* recordType, const vector<IdentifierExpr*>& identifiers) :
        AstNode(token), recordType(recordType->with_parent<NameExpr>(this)),
        identifiers(nodes_with_parent(identifiers, this))
    {
    }

    any RecordNode::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type RecordNode::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    RecordNode::~RecordNode()
    {
        delete recordType;
        for (auto p : identifiers)
            delete p;
    }

    TrueLiteralExpr::TrueLiteralExpr(Token) : LiteralExpr<bool>(token, true) {}

    any TrueLiteralExpr::accept(const ::yona::ast::AstVisitor& visitor) { return visitor.visit(this); }

    Type TrueLiteralExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    FalseLiteralExpr::FalseLiteralExpr(Token) : LiteralExpr<bool>(token, false) {}

    any FalseLiteralExpr::accept(const ::yona::ast::AstVisitor& visitor) { return visitor.visit(this); }

    Type FalseLiteralExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    FloatExpr::FloatExpr(Token token, float value) : LiteralExpr<float>(token, value) {}

    any FloatExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type FloatExpr::infer_type(TypeInferenceContext& ctx) const { return Float; }

    IntegerExpr::IntegerExpr(Token token, int value) : LiteralExpr<int>(token, value) {}

    any IntegerExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type IntegerExpr::infer_type(TypeInferenceContext& ctx) const { return Int; }

    any ExprNode::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    any PatternNode::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    any UnderscoreNode::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type UnderscoreNode::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    any ValueExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }

    ByteExpr::ByteExpr(Token token, unsigned char value) : LiteralExpr<unsigned char>(token, value) {}

    any ByteExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ByteExpr::infer_type(TypeInferenceContext& ctx) const { return Byte; }

    StringExpr::StringExpr(Token token, string value) : LiteralExpr<string>(token, std::move(value)) {}

    any StringExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type StringExpr::infer_type(TypeInferenceContext& ctx) const { return String; }

    CharacterExpr::CharacterExpr(Token token, const char value) : LiteralExpr<char>(token, value) {}

    any CharacterExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type CharacterExpr::infer_type(TypeInferenceContext& ctx) const { return Char; }

    UnitExpr::UnitExpr(Token) : LiteralExpr<nullptr_t>(token, nullptr) {}

    any UnitExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type UnitExpr::infer_type(TypeInferenceContext& ctx) const { return Unit; }

    TupleExpr::TupleExpr(Token token, const vector<ExprNode*>& values) :
        ValueExpr(token), values(nodes_with_parent(values, this))
    {
    }

    any TupleExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type TupleExpr::infer_type(TypeInferenceContext& ctx) const
    {
        vector<Type> fieldTypes;
        ranges::for_each(values, [&](const ExprNode* expr) { fieldTypes.push_back(expr->infer_type(ctx)); });
        return make_shared<TupleType>(fieldTypes);
    }

    TupleExpr::~TupleExpr()
    {
        for (auto p : values)
            delete p;
    }

    DictExpr::DictExpr(Token token, const vector<pair<ExprNode*, ExprNode*>>& values) :
        ValueExpr(token), values(nodes_with_parent(values, this))
    {
    }

    any DictExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type DictExpr::infer_type(TypeInferenceContext& ctx) const
    {
        unordered_set<Type> keyTypes;
        unordered_set<Type> valueTypes;
        ranges::for_each(values,
                         [&](pair<ExprNode*, ExprNode*> p)
                         {
                             keyTypes.insert(p.first->infer_type(ctx));
                             valueTypes.insert(p.second->infer_type(ctx));
                         });

        if (keyTypes.size() > 1 || valueTypes.size() > 1)
        {
            ctx.addError(TypeError(token, "Dictionary keys and values must have the same type"));
        }

        return make_shared<DictCollectionType>(values.front().first->infer_type(ctx),
                                               values.front().second->infer_type(ctx));
    }

    DictExpr::~DictExpr()
    {
        for (auto p : values)
        {
            delete p.first;
            delete p.second;
        }
    }

    ValuesSequenceExpr::ValuesSequenceExpr(Token token, const vector<ExprNode*>& values) :
        SequenceExpr(token), values(nodes_with_parent(values, this))
    {
    }

    any ValuesSequenceExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ValuesSequenceExpr::infer_type(TypeInferenceContext& ctx) const
    {
        unordered_set<Type> valueTypes;
        ranges::for_each(values, [&](const ExprNode* expr) { valueTypes.insert(expr->infer_type(ctx)); });

        if (valueTypes.size() > 1)
        {
            ctx.addError(TypeError(token, "Sequence values must have the same type"));
        }

        return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Seq, values.front()->infer_type(ctx));
    }

    ValuesSequenceExpr::~ValuesSequenceExpr()
    {
        for (auto p : values)
            delete p;
    }

    RangeSequenceExpr::RangeSequenceExpr(Token token, ExprNode* start, ExprNode* end, ExprNode* step) :
        SequenceExpr(token), start(start->with_parent<ExprNode>(this)), end(end->with_parent<ExprNode>(this)),
        step(step->with_parent<ExprNode>(this))
    {
    }

    any RangeSequenceExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type RangeSequenceExpr::infer_type(TypeInferenceContext& ctx) const
    {
        Type startExprType = start->infer_type(ctx);
        Type endExprType = start->infer_type(ctx);
        Type stepExprType = start->infer_type(ctx);

        if (!holds_alternative<ValueType>(startExprType) || get<ValueType>(startExprType) != Int)
        {
            ctx.addError(TypeError(start->token, "Sequence start expression must be integer"));
        }

        if (!holds_alternative<ValueType>(endExprType) || get<ValueType>(endExprType) != Int)
        {
            ctx.addError(TypeError(end->token, "Sequence end expression must be integer"));
        }

        if (!holds_alternative<ValueType>(stepExprType) || get<ValueType>(stepExprType) != Int)
        {
            ctx.addError(TypeError(step->token, "Sequence step expression must be integer"));
        }

        return nullptr;
    }

    RangeSequenceExpr::~RangeSequenceExpr()
    {
        delete start;
        delete end;
        delete step;
    }

    SetExpr::SetExpr(Token token, const vector<ExprNode*>& values) :
        ValueExpr(token), values(nodes_with_parent(values, this))
    {
    }

    any SetExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type SetExpr::infer_type(TypeInferenceContext& ctx) const
    {
        unordered_set<Type> valueTypes;
        ranges::for_each(values, [&](const ExprNode* expr) { valueTypes.insert(expr->infer_type(ctx)); });

        if (valueTypes.size() > 1)
        {
            ctx.addError(TypeError(token, "Set values must have the same type"));
        }

        return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Set, values.front()->infer_type(ctx));
    }

    SetExpr::~SetExpr()
    {
        for (auto p : values)
            delete p;
    }

    SymbolExpr::SymbolExpr(Token token, string value) : ValueExpr(token), value(std::move(value)) {}

    any SymbolExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type SymbolExpr::infer_type(TypeInferenceContext& ctx) const { return Symbol; }

    PackageNameExpr::PackageNameExpr(Token token, const vector<NameExpr*>& parts) :
        ValueExpr(token), parts(nodes_with_parent(parts, this))
    {
    }

    any PackageNameExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type PackageNameExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    PackageNameExpr::~PackageNameExpr()
    {
        for (auto p : parts)
            delete p;
    }

    FqnExpr::FqnExpr(Token token, PackageNameExpr* packageName, NameExpr* moduleName) :
        ValueExpr(token), packageName(packageName->with_parent<PackageNameExpr>(this)),
        moduleName(moduleName->with_parent<NameExpr>(this))
    {
    }

    any FqnExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type FqnExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    FqnExpr::~FqnExpr()
    {
        delete packageName;
        delete moduleName;
    }

    FunctionExpr::FunctionExpr(Token token, string name, const vector<PatternNode*>& patterns,
                               const vector<FunctionBody*>& bodies) :
        ScopedNode(token), name(std::move(name)), patterns(nodes_with_parent(patterns, this)),
        bodies(nodes_with_parent(bodies, this))
    {
    }

    any FunctionExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type FunctionExpr::infer_type(TypeInferenceContext& ctx) const
    {
        unordered_set<Type> bodyTypes;
        ranges::for_each(bodies, [&](const FunctionBody* body) { bodyTypes.insert(body->infer_type(ctx)); });

        return make_shared<SumType>(bodyTypes);
    }

    FunctionExpr::~FunctionExpr()
    {
        for (auto p : patterns)
            delete p;
        for (auto p : bodies)
            delete p;
    }

    BodyWithGuards::BodyWithGuards(Token token, ExprNode* guard, const vector<ExprNode*>& expr) :
        FunctionBody(token), guard(guard->with_parent<ExprNode>(this)), exprs(nodes_with_parent(expr, this))
    {
    }

    any BodyWithGuards::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type BodyWithGuards::infer_type(TypeInferenceContext& ctx) const
    {
        const Type guardType = guard->infer_type(ctx);

        if (!holds_alternative<ValueType>(guardType) || get<ValueType>(guardType) != Bool)
        {
            ctx.addError(TypeError(guard->token, "Guard expression must be boolean"));
        }

        return exprs.back()->infer_type(ctx);
    }
    BodyWithGuards::~BodyWithGuards()
    {
        delete guard;
        for (auto p : exprs)
            delete p;
    }

    BodyWithoutGuards::BodyWithoutGuards(Token token, ExprNode* expr) :
        FunctionBody(token), expr(expr->with_parent<ExprNode>(this))
    {
    }

    any BodyWithoutGuards::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type BodyWithoutGuards::infer_type(TypeInferenceContext& ctx) const { return expr->infer_type(ctx); }

    BodyWithoutGuards::~BodyWithoutGuards() { delete expr; }

    ModuleExpr::ModuleExpr(Token token, FqnExpr* fqn, const vector<string>& exports, const vector<RecordNode*>& records,
                           const vector<FunctionExpr*>& functions) :
        ValueExpr(token), fqn(fqn->with_parent<FqnExpr>(this)), exports(exports),
        records(nodes_with_parent(records, this)), functions(nodes_with_parent(functions, this))
    {
    }

    any ModuleExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ModuleExpr::infer_type(TypeInferenceContext& ctx) const { return Module; }

    ModuleExpr::~ModuleExpr()
    {
        delete fqn;
        for (auto p : records)
            delete p;
        for (auto p : functions)
            delete p;
    }

    RecordInstanceExpr::RecordInstanceExpr(Token token, NameExpr* recordType,
                                           const vector<pair<NameExpr*, ExprNode*>>& items) :
        ValueExpr(token), recordType(recordType->with_parent<NameExpr>(this)), items(nodes_with_parent(items, this))
    {
    }

    any RecordInstanceExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type RecordInstanceExpr::infer_type(TypeInferenceContext& ctx) const
    {
        vector<Type> itemTypes{ Symbol };
        ranges::for_each(items,
                         [&](const pair<NameExpr*, ExprNode*>& p) { itemTypes.push_back(p.second->infer_type(ctx)); });

        return make_shared<TupleType>(itemTypes);
    }

    RecordInstanceExpr::~RecordInstanceExpr()
    {
        delete recordType;
        for (auto p : items)
            delete p.second;
    }

    LogicalNotOpExpr::LogicalNotOpExpr(Token token, ExprNode* expr) :
        OpExpr(token), expr(expr->with_parent<ExprNode>(this))
    {
    }

    any LogicalNotOpExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type LogicalNotOpExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type exprType = expr->infer_type(ctx);

        if (!holds_alternative<ValueType>(exprType) || get<ValueType>(exprType) != Bool)
        {
            ctx.addError(TypeError(expr->token, "Expression for logical negation must be boolean"));
        }

        return Bool;
    }

    LogicalNotOpExpr::~LogicalNotOpExpr() { delete expr; }

    BinaryNotOpExpr::BinaryNotOpExpr(Token token, ExprNode* expr) :
        OpExpr(token), expr(expr->with_parent<ExprNode>(this))
    {
    }

    any BinaryNotOpExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type BinaryNotOpExpr::infer_type(TypeInferenceContext& ctx) const
    {
        Type exprType = expr->infer_type(ctx);

        if (!holds_alternative<ValueType>(exprType) || get<ValueType>(exprType) != Bool)
        {
            ctx.addError(TypeError(expr->token, "Expression for binary negation must be boolean"));
        }

        return Bool;
    }

    BinaryNotOpExpr::~BinaryNotOpExpr() { delete expr; }

    PowerExpr::PowerExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any PowerExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type PowerExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    MultiplyExpr::MultiplyExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any MultiplyExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type MultiplyExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    DivideExpr::DivideExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any DivideExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type DivideExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    ModuloExpr::ModuloExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any ModuloExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ModuloExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    AddExpr::AddExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any AddExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type AddExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    SubtractExpr::SubtractExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any SubtractExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type SubtractExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    LeftShiftExpr::LeftShiftExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any LeftShiftExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type LeftShiftExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left->infer_type(ctx);
        const Type rightType = right->infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for left shift must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for left shift must be integer"));
        }

        return Int;
    }

    RightShiftExpr::RightShiftExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any RightShiftExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type RightShiftExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left->infer_type(ctx);
        const Type rightType = right->infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for right shift must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for right shift must be integer"));
        }

        return Int;
    }

    ZerofillRightShiftExpr::ZerofillRightShiftExpr(Token token, ExprNode* left, ExprNode* right) :
        BinaryOpExpr(token, left, right)
    {
    }

    any ZerofillRightShiftExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ZerofillRightShiftExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left->infer_type(ctx);
        const Type rightType = right->infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for zerofill right shift must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for zerofill right shift must be integer"));
        }

        return Int;
    }

    GteExpr::GteExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any GteExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type GteExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    LteExpr::LteExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any LteExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type LteExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    GtExpr::GtExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any GtExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type GtExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    LtExpr::LtExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any LtExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type LtExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    EqExpr::EqExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any EqExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type EqExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    NeqExpr::NeqExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any NeqExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type NeqExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    ConsLeftExpr::ConsLeftExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any ConsLeftExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ConsLeftExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    ConsRightExpr::ConsRightExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any ConsRightExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ConsRightExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    JoinExpr::JoinExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any JoinExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type JoinExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left->infer_type(ctx);
        const Type rightType = right->infer_type(ctx);

        if (!holds_alternative<shared_ptr<SingleItemCollectionType>>(leftType) ||
            !holds_alternative<shared_ptr<DictCollectionType>>(leftType) ||
            !holds_alternative<shared_ptr<TupleType>>(leftType))
        {
            ctx.addError(TypeError(left->token, "Join expression can be used only for collections"));
        }

        if (!holds_alternative<shared_ptr<SingleItemCollectionType>>(rightType) ||
            !holds_alternative<shared_ptr<DictCollectionType>>(rightType) ||
            !holds_alternative<shared_ptr<TupleType>>(rightType))
        {
            ctx.addError(TypeError(right->token, "Join expression can be used only for collections"));
        }

        if (leftType != rightType)
        {
            ctx.addError(TypeError(token, "Join expression can be used only on same types"));
        }

        return leftType;
    }

    BitwiseAndExpr::BitwiseAndExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any BitwiseAndExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type BitwiseAndExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left->infer_type(ctx);
        const Type rightType = right->infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for bitwise AND must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for bitwise AND must be integer"));
        }

        return Bool;
    }

    BitwiseXorExpr::BitwiseXorExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any BitwiseXorExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type BitwiseXorExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left->infer_type(ctx);
        const Type rightType = right->infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for bitwise XOR must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for bitwise XOR must be integer"));
        }

        return Bool;
    }

    BitwiseOrExpr::BitwiseOrExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any BitwiseOrExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type BitwiseOrExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left->infer_type(ctx);
        const Type rightType = right->infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for bitwise OR must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left->token, "Expression for bitwise OR must be integer"));
        }

        return Bool;
    }

    LogicalAndExpr::LogicalAndExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any LogicalAndExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type LogicalAndExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left->infer_type(ctx);
        const Type rightType = right->infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Bool)
        {
            ctx.addError(TypeError(left->token, "Expression for logical AND must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Bool)
        {
            ctx.addError(TypeError(left->token, "Expression for logical AND must be integer"));
        }

        return Bool;
    }

    LogicalOrExpr::LogicalOrExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any LogicalOrExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type LogicalOrExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left->infer_type(ctx);
        const Type rightType = right->infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Bool)
        {
            ctx.addError(TypeError(left->token, "Expression for logical OR must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Bool)
        {
            ctx.addError(TypeError(left->token, "Expression for logical OR must be integer"));
        }

        return Bool;
    }

    InExpr::InExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any InExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type InExpr::infer_type(TypeInferenceContext& ctx) const
    {
        if (const Type rightType = right->infer_type(ctx);
            !holds_alternative<shared_ptr<SingleItemCollectionType>>(rightType) ||
            !holds_alternative<shared_ptr<DictCollectionType>>(rightType) ||
            !holds_alternative<shared_ptr<TupleType>>(rightType))
        {
            ctx.addError(TypeError(left->token, "Expression for IN must be a collection"));
        }

        return Bool;
    }

    PipeLeftExpr::PipeLeftExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any PipeLeftExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type PipeLeftExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    PipeRightExpr::PipeRightExpr(Token token, ExprNode* left, ExprNode* right) : BinaryOpExpr(token, left, right) {}

    any PipeRightExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type PipeRightExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    LetExpr::LetExpr(Token token, const vector<AliasExpr*>& aliases, ExprNode* expr) :
        ScopedNode(token), aliases(nodes_with_parent(aliases, this)), expr(expr->with_parent<ExprNode>(this))
    {
    }

    any LetExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type LetExpr::infer_type(TypeInferenceContext& ctx) const { return expr->infer_type(ctx); }

    IfExpr::IfExpr(Token token, ExprNode* condition, ExprNode* thenExpr, ExprNode* elseExpr) :
        ExprNode(token), condition(condition->with_parent<ExprNode>(this)),
        thenExpr(thenExpr->with_parent<ExprNode>(this)), elseExpr(elseExpr->with_parent<ExprNode>(this))
    {
    }

    LetExpr::~LetExpr()
    {
        for (auto p : aliases)
            delete p;
        delete expr;
    }

    any IfExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type IfExpr::infer_type(TypeInferenceContext& ctx) const
    {
        if (const Type conditionType = condition->infer_type(ctx);
            !holds_alternative<ValueType>(conditionType) || get<ValueType>(conditionType) != Bool)
        {
            ctx.addError(TypeError(condition->token, "If condition must be boolean"));
        }

        unordered_set returnTypes{ thenExpr->infer_type(ctx) };

        if (elseExpr != nullptr)
        {
            returnTypes.insert(elseExpr->infer_type(ctx));
        }

        return make_shared<SumType>(returnTypes);
    }

    IfExpr::~IfExpr()
    {
        delete condition;
        delete thenExpr;
        delete elseExpr;
    }

    ApplyExpr::ApplyExpr(Token token, CallExpr* call, const vector<variant<ExprNode*, ValueExpr*>>& args) :
        ExprNode(token), call(call->with_parent<CallExpr>(this)), args(nodes_with_parent(args, this))
    {
    }

    any ApplyExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ApplyExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    ApplyExpr::~ApplyExpr()
    {
        delete call;
        for (auto p : args)
        {
            if (holds_alternative<ExprNode*>(p))
            {
                delete get<ExprNode*>(p);
            }
            else
            {
                delete get<ValueExpr*>(p);
            }
        }
    }

    DoExpr::DoExpr(Token token, const vector<ExprNode*>& steps) : ExprNode(token), steps(steps) {}

    any DoExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type DoExpr::infer_type(TypeInferenceContext& ctx) const { return steps.back()->infer_type(ctx); }

    DoExpr::~DoExpr()
    {
        for (auto p : steps)
        {
            delete p;
        }
    }

    ImportExpr::ImportExpr(Token token, const vector<ImportClauseExpr*>& clauses, ExprNode* expr) :
        ScopedNode(token), clauses(nodes_with_parent(clauses, this)), expr(expr->with_parent<ExprNode>(this))
    {
    }

    any ImportExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ImportExpr::infer_type(TypeInferenceContext& ctx) const { return expr->infer_type(ctx); }

    ImportExpr::~ImportExpr()
    {
        for (auto p : clauses)
        {
            delete p;
        }
        delete expr;
    }

    RaiseExpr::RaiseExpr(Token token, SymbolExpr* symbol, StringExpr* message) :
        ExprNode(token), symbol(symbol->with_parent<SymbolExpr>(this)), message(message->with_parent<StringExpr>(this))
    {
    }

    any RaiseExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type RaiseExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    RaiseExpr::~RaiseExpr()
    {
        delete symbol;
        delete message;
    }

    WithExpr::WithExpr(Token token, ExprNode* contextExpr, NameExpr* name, ExprNode* bodyExpr) :
        ScopedNode(token), contextExpr(contextExpr->with_parent<ExprNode>(this)),
        name(name->with_parent<NameExpr>(this)), bodyExpr(bodyExpr->with_parent<ExprNode>(this))
    {
    }

    any WithExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type WithExpr::infer_type(TypeInferenceContext& ctx) const { return bodyExpr->infer_type(ctx); }

    WithExpr::~WithExpr()
    {
        delete contextExpr;
        delete name;
        delete bodyExpr;
    }

    FieldAccessExpr::FieldAccessExpr(Token token, IdentifierExpr* identifier, NameExpr* name) :
        ExprNode(token), identifier(identifier->with_parent<IdentifierExpr>(this)),
        name(name->with_parent<NameExpr>(this))
    {
    }

    any FieldAccessExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type FieldAccessExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    FieldUpdateExpr::FieldUpdateExpr(Token token, IdentifierExpr* identifier,
                                     const vector<pair<NameExpr*, ExprNode*>>& updates) :
        ExprNode(token), identifier(identifier->with_parent<IdentifierExpr>(this)),
        updates(nodes_with_parent(updates, this))
    {
    }

    FieldAccessExpr::~FieldAccessExpr()
    {
        delete identifier;
        delete name;
    }

    any FieldUpdateExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type FieldUpdateExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    FieldUpdateExpr::~FieldUpdateExpr()
    {
        delete identifier;
        for (auto p : updates)
        {
            delete p.first;
            delete p.second;
        }
    }

    LambdaAlias::LambdaAlias(Token token, NameExpr* name, FunctionExpr* lambda) :
        AliasExpr(token), name(name->with_parent<NameExpr>(this)), lambda(lambda->with_parent<FunctionExpr>(this))
    {
    }

    any LambdaAlias::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type LambdaAlias::infer_type(TypeInferenceContext& ctx) const { return lambda->infer_type(ctx); }

    LambdaAlias::~LambdaAlias()
    {
        delete name;
        delete lambda;
    }

    ModuleAlias::ModuleAlias(Token token, NameExpr* name, ModuleExpr* module) :
        AliasExpr(token), name(name->with_parent<NameExpr>(this)), module(module->with_parent<ModuleExpr>(this))
    {
    }

    any ModuleAlias::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ModuleAlias::infer_type(TypeInferenceContext& ctx) const { return module->infer_type(ctx); }

    ModuleAlias::~ModuleAlias()
    {
        delete name;
        delete module;
    }

    ValueAlias::ValueAlias(Token token, IdentifierExpr* identifier, ExprNode* expr) :
        AliasExpr(token), identifier(identifier->with_parent<IdentifierExpr>(this)),
        expr(expr->with_parent<ExprNode>(this))
    {
    }

    any ValueAlias::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ValueAlias::infer_type(TypeInferenceContext& ctx) const { return expr->infer_type(ctx); }

    ValueAlias::~ValueAlias()
    {
        delete identifier;
        delete expr;
    }

    PatternAlias::PatternAlias(Token token, PatternNode* pattern, ExprNode* expr) :
        AliasExpr(token), pattern(pattern->with_parent<PatternNode>(this)), expr(expr->with_parent<ExprNode>(this))
    {
    }

    any PatternAlias::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type PatternAlias::infer_type(TypeInferenceContext& ctx) const { return expr->infer_type(ctx); }

    PatternAlias::~PatternAlias()
    {
        delete pattern;
        delete expr;
    }

    FqnAlias::FqnAlias(Token token, NameExpr* name, FqnExpr* fqn) :
        AliasExpr(token), name(name->with_parent<NameExpr>(this)), fqn(fqn->with_parent<FqnExpr>(this))
    {
    }

    any FqnAlias::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type FqnAlias::infer_type(TypeInferenceContext& ctx) const { return fqn->infer_type(ctx); }

    FqnAlias::~FqnAlias()
    {
        delete name;
        delete fqn;
    }

    FunctionAlias::FunctionAlias(Token token, NameExpr* name, NameExpr* alias) :
        AliasExpr(token), name(name->with_parent<NameExpr>(this)), alias(alias->with_parent<NameExpr>(this))
    {
    }

    any FunctionAlias::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type FunctionAlias::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    FunctionAlias::~FunctionAlias()
    {
        delete name;
        delete alias;
    }

    AliasCall::AliasCall(Token token, NameExpr* alias, NameExpr* funName) :
        CallExpr(token), alias(alias->with_parent<NameExpr>(this)), funName(funName->with_parent<NameExpr>(this))
    {
    }

    any AliasCall::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type AliasCall::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    AliasCall::~AliasCall()
    {
        delete alias;
        delete funName;
    }

    NameCall::NameCall(Token token, NameExpr* name) : CallExpr(token), name(name->with_parent<NameExpr>(this)) {}

    any NameCall::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type NameCall::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    NameCall::~NameCall() { delete name; }

    ModuleCall::ModuleCall(Token token, const variant<FqnExpr*, ExprNode*>& fqn, NameExpr* funName) :
        CallExpr(token), fqn(node_with_parent(fqn, this)), funName(funName->with_parent<NameExpr>(this))
    {
    }

    any ModuleCall::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ModuleCall::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    ModuleCall::~ModuleCall()
    {
        if (holds_alternative<FqnExpr*>(fqn))
        {
            delete get<FqnExpr*>(fqn);
        }
        else
        {
            delete get<ExprNode*>(fqn);
        }
        delete funName;
    }

    ModuleImport::ModuleImport(Token token, FqnExpr* fqn, NameExpr* name) :
        ImportClauseExpr(token), fqn(fqn->with_parent<FqnExpr>(this)), name(name->with_parent<NameExpr>(this))
    {
    }

    any ModuleImport::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ModuleImport::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    ModuleImport::~ModuleImport()
    {
        delete fqn;
        delete name;
    }

    FunctionsImport::FunctionsImport(Token token, const vector<FunctionAlias*>& aliases, FqnExpr* fromFqn) :
        ImportClauseExpr(token), aliases(nodes_with_parent(aliases, this)), fromFqn(fromFqn->with_parent<FqnExpr>(this))
    {
    }

    any FunctionsImport::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type FunctionsImport::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    FunctionsImport::~FunctionsImport()
    {
        for (auto p : aliases)
        {
            delete p;
        }
        delete fromFqn;
    }

    SeqGeneratorExpr::SeqGeneratorExpr(Token token, ExprNode* reducerExpr, CollectionExtractorExpr* collectionExtractor,
                                       ExprNode* stepExpression) :
        GeneratorExpr(token), reducerExpr(reducerExpr->with_parent<ExprNode>(this)),
        collectionExtractor(collectionExtractor->with_parent<CollectionExtractorExpr>(this)),
        stepExpression(stepExpression->with_parent<ExprNode>(this))
    {
    }

    any SeqGeneratorExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type SeqGeneratorExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Seq, reducerExpr->infer_type(ctx));
    }

    SeqGeneratorExpr::~SeqGeneratorExpr()
    {
        delete reducerExpr;
        delete collectionExtractor;
        delete stepExpression;
    }

    SetGeneratorExpr::SetGeneratorExpr(Token token, ExprNode* reducerExpr, CollectionExtractorExpr* collectionExtractor,
                                       ExprNode* stepExpression) :
        GeneratorExpr(token), reducerExpr(reducerExpr->with_parent<ExprNode>(this)),
        collectionExtractor(collectionExtractor->with_parent<CollectionExtractorExpr>(this)),
        stepExpression(stepExpression->with_parent<ExprNode>(this))
    {
    }

    any SetGeneratorExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type SetGeneratorExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Set, reducerExpr->infer_type(ctx));
    }

    SetGeneratorExpr::~SetGeneratorExpr()
    {
        delete reducerExpr;
        delete collectionExtractor;
        delete stepExpression;
    }

    DictGeneratorReducer::DictGeneratorReducer(Token token, ExprNode* key, ExprNode* value) :
        ExprNode(token), key(key->with_parent<ExprNode>(this)), value(value->with_parent<ExprNode>(this))
    {
    }

    any DictGeneratorReducer::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type DictGeneratorReducer::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    DictGeneratorReducer::~DictGeneratorReducer()
    {
        delete key;
        delete value;
    }

    DictGeneratorExpr::DictGeneratorExpr(Token token, DictGeneratorReducer* reducerExpr,
                                         CollectionExtractorExpr* collectionExtractor, ExprNode* stepExpression) :
        GeneratorExpr(token), reducerExpr(reducerExpr->with_parent<DictGeneratorReducer>(this)),
        collectionExtractor(collectionExtractor->with_parent<CollectionExtractorExpr>(this)),
        stepExpression(stepExpression->with_parent<ExprNode>(this))
    {
    }

    any DictGeneratorExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type DictGeneratorExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return make_shared<DictCollectionType>(reducerExpr->key->infer_type(ctx), reducerExpr->value->infer_type(ctx));
    }

    DictGeneratorExpr::~DictGeneratorExpr()
    {
        delete reducerExpr;
        delete collectionExtractor;
        delete stepExpression;
    }

    ValueCollectionExtractorExpr::ValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore expr) :
        CollectionExtractorExpr(token), expr(node_with_parent(expr, this))
    {
    }

    any ValueCollectionExtractorExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type ValueCollectionExtractorExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    void release_identifier_or_underscore(IdentifierOrUnderscore expr)
    {
        if (holds_alternative<IdentifierExpr*>(expr))
        {
            delete get<IdentifierExpr*>(expr);
        }
        else
        {
            delete get<UnderscoreNode*>(expr);
        }
    }

    ValueCollectionExtractorExpr::~ValueCollectionExtractorExpr() { release_identifier_or_underscore(expr); }

    KeyValueCollectionExtractorExpr::KeyValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore keyExpr,
                                                                     IdentifierOrUnderscore valueExpr) :
        CollectionExtractorExpr(token), keyExpr(node_with_parent(keyExpr, this)),
        valueExpr(node_with_parent(valueExpr, this))
    {
    }

    any KeyValueCollectionExtractorExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type KeyValueCollectionExtractorExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    KeyValueCollectionExtractorExpr::~KeyValueCollectionExtractorExpr()
    {
        release_identifier_or_underscore(keyExpr);
        release_identifier_or_underscore(valueExpr);
    }

    PatternWithGuards::PatternWithGuards(Token token, ExprNode* guard, ExprNode* exprNode) :
        PatternNode(token), guard(guard->with_parent<ExprNode>(this)), expr(exprNode->with_parent<ExprNode>(this))
    {
    }

    any PatternWithGuards::accept(const AstVisitor& visitor) { return visitor.visit(this); };

    Type PatternWithGuards::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    PatternWithGuards::~PatternWithGuards()
    {
        delete guard;
        delete expr;
    }

    PatternWithoutGuards::PatternWithoutGuards(Token token, ExprNode* expr) :
        PatternNode(token), expr(expr->with_parent<ExprNode>(this))
    {
    }

    any PatternWithoutGuards::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type PatternWithoutGuards::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    PatternWithoutGuards::~PatternWithoutGuards() { delete expr; }

    PatternExpr::PatternExpr(Token token,
                             const variant<Pattern*, PatternWithoutGuards*, vector<PatternWithGuards*>>& patternExpr) :
        ExprNode(token), patternExpr(patternExpr) // TODO
    {
        // std::visit({ [this](Pattern& arg) { arg.with_parent<Pattern>(this); },
        //              [this](PatternWithoutGuards& arg) { arg.with_parent<PatternWithGuards>(this); },
        //              [this](vector<PatternWithGuards>& arg) { nodes_with_parent(arg, this); } },
        //            patternExpr); // TODO
    }
    any PatternExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }

    Type PatternExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    PatternExpr::~PatternExpr()
    {
        if (holds_alternative<Pattern*>(patternExpr))
        {
            delete get<Pattern*>(patternExpr);
        }
        else if (holds_alternative<PatternWithoutGuards*>(patternExpr))
        {
            delete get<PatternWithoutGuards*>(patternExpr);
        }
        else
        {
            for (auto p : get<vector<PatternWithGuards*>>(patternExpr))
            {
                delete p;
            }
        }
    }

    CatchPatternExpr::CatchPatternExpr(Token token, Pattern* matchPattern,
                                       const variant<PatternWithoutGuards*, vector<PatternWithGuards*>>& pattern) :
        ExprNode(token), matchPattern(matchPattern->with_parent<Pattern>(this)), pattern(pattern)
    {
        // std::visit({ [this](PatternWithoutGuards& arg) { arg.with_parent<PatternWithGuards>(this); },
        //              [this](vector<PatternWithGuards>& arg) { nodes_with_parent(arg, this); } },
        //            pattern); // TODO
    }

    any CatchPatternExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type CatchPatternExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    CatchPatternExpr::~CatchPatternExpr()
    {
        delete matchPattern;
        if (holds_alternative<PatternWithoutGuards*>(pattern))
        {
            delete get<PatternWithoutGuards*>(pattern);
        }
        else
        {
            for (auto p : get<vector<PatternWithGuards*>>(pattern))
            {
                delete p;
            }
        }
    }

    CatchExpr::CatchExpr(Token token, const vector<CatchPatternExpr*>& patterns) :
        ExprNode(token), patterns(nodes_with_parent(patterns, this))
    {
    }

    any CatchExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type CatchExpr::infer_type(TypeInferenceContext& ctx) const { return patterns.back()->infer_type(ctx); }

    CatchExpr::~CatchExpr()
    {
        for (auto p : patterns)
        {
            delete p;
        }
    }

    TryCatchExpr::TryCatchExpr(Token token, ExprNode* tryExpr, CatchExpr* catchExpr) :
        ExprNode(token), tryExpr(tryExpr->with_parent<ExprNode>(this)),
        catchExpr(catchExpr->with_parent<CatchExpr>(this))
    {
    }

    any TryCatchExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type TryCatchExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return make_shared<SumType>(unordered_set{ tryExpr->infer_type(ctx), catchExpr->infer_type(ctx) });
    }

    TryCatchExpr::~TryCatchExpr()
    {
        delete tryExpr;
        delete catchExpr;
    }

    PatternValue::PatternValue(
        Token token, const variant<LiteralExpr<nullptr_t>*, LiteralExpr<void*>*, SymbolExpr*, IdentifierExpr*>& expr) :
        PatternNode(token), expr(node_with_parent(expr, this))
    {
    }

    any PatternValue::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type PatternValue::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    PatternValue::~PatternValue()
    {
        if (holds_alternative<LiteralExpr<nullptr_t>*>(expr))
        {
            delete get<LiteralExpr<nullptr_t>*>(expr);
        }
        else if (holds_alternative<LiteralExpr<void*>*>(expr))
        {
            delete get<LiteralExpr<void*>*>(expr);
        }
        else if (holds_alternative<SymbolExpr*>(expr))
        {
            delete get<SymbolExpr*>(expr);
        }
    }

    AsDataStructurePattern::AsDataStructurePattern(Token token, IdentifierExpr* identifier,
                                                   DataStructurePattern* pattern) :
        PatternNode(token), identifier(identifier->with_parent<IdentifierExpr>(this)),
        pattern(pattern->with_parent<DataStructurePattern>(this))
    {
    }

    any AsDataStructurePattern::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type AsDataStructurePattern::infer_type(TypeInferenceContext& ctx) const { return pattern->infer_type(ctx); }

    AsDataStructurePattern::~AsDataStructurePattern()
    {
        delete identifier;
        delete pattern;
    }

    any UnderscorePattern::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type UnderscorePattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    TuplePattern::TuplePattern(Token token, const vector<Pattern*>& patterns) :
        PatternNode(token), patterns(nodes_with_parent(patterns, this))
    {
    }

    any TuplePattern::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type TuplePattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    TuplePattern::~TuplePattern()
    {
        for (auto p : patterns)
        {
            delete p;
        }
    }

    SeqPattern::SeqPattern(Token token, const vector<Pattern*>& patterns) :
        PatternNode(token), patterns(nodes_with_parent(patterns, this))
    {
    }

    any SeqPattern::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type SeqPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    SeqPattern::~SeqPattern()
    {
        for (auto p : patterns)
        {
            delete p;
        }
    }

    HeadTailsPattern::HeadTailsPattern(Token token, const vector<PatternWithoutSequence*>& heads, TailPattern* tail) :
        PatternNode(token), heads(nodes_with_parent(heads, this)), tail(tail->with_parent<TailPattern>(this))
    {
    }

    any HeadTailsPattern::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type HeadTailsPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    HeadTailsPattern::~HeadTailsPattern()
    {
        for (auto p : heads)
        {
            delete p;
        }
        delete tail;
    }

    TailsHeadPattern::TailsHeadPattern(Token token, TailPattern* tail, const vector<PatternWithoutSequence*>& heads) :
        PatternNode(token), tail(tail->with_parent<TailPattern>(this)), heads(nodes_with_parent(heads, this))
    {
    }

    any TailsHeadPattern::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type TailsHeadPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    TailsHeadPattern::~TailsHeadPattern()
    {
        delete tail;
        for (auto p : heads)
        {
            delete p;
        }
    }

    HeadTailsHeadPattern::HeadTailsHeadPattern(Token token, const vector<PatternWithoutSequence*>& left,
                                               TailPattern* tail, const vector<PatternWithoutSequence*>& right) :
        PatternNode(token), left(nodes_with_parent(left, this)), tail(tail->with_parent<TailPattern>(this)),
        right(nodes_with_parent(right, this))
    {
    }

    any HeadTailsHeadPattern::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type HeadTailsHeadPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    HeadTailsHeadPattern::~HeadTailsHeadPattern()
    {
        for (auto p : left)
        {
            delete p;
        }
        delete tail;
        for (auto p : right)
        {
            delete p;
        }
    }

    DictPattern::DictPattern(Token token, const vector<pair<PatternValue*, Pattern*>>& keyValuePairs) :
        PatternNode(token), keyValuePairs(nodes_with_parent(keyValuePairs, this))
    {
    }

    any DictPattern::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type DictPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    DictPattern::~DictPattern()
    {
        for (auto p : keyValuePairs)
        {
            delete p.first;
            delete p.second;
        }
    }

    RecordPattern::RecordPattern(Token token, string recordType, const vector<pair<NameExpr*, Pattern*>>& items) :
        PatternNode(token), recordType(std::move(recordType)), items(nodes_with_parent(items, this))
    {
    }

    any RecordPattern::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type RecordPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    RecordPattern::~RecordPattern()
    {
        for (auto p : items)
        {
            delete p.first;
            delete p.second;
        }
    }

    CaseExpr::CaseExpr(Token token, ExprNode* expr, const vector<PatternExpr*>& patterns) :
        ExprNode(token), expr(expr->with_parent<ExprNode>(this)), patterns(nodes_with_parent(patterns, this))
    {
    }

    any CaseExpr::accept(const AstVisitor& visitor) { return visitor.visit(this); }

    Type CaseExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    CaseExpr::~CaseExpr()
    {
        delete expr;
        for (auto p : patterns)
        {
            delete p;
        }
    }

    any AstVisitor::visit(ExprNode* node) const
    {
        if (auto derived = dynamic_cast<AliasExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<ApplyExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<CallExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<CaseExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<CatchExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<CatchPatternExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<CollectionExtractorExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<DictGeneratorReducer*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<DoExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<FieldAccessExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<FieldUpdateExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<GeneratorExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<IfExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<NameExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<OpExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<PatternExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<RaiseExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<SequenceExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<TryCatchExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<ValueExpr*>(node))
        {
            return visit(derived);
        }
        unreachable();
    }

    any AstVisitor::visit(AstNode* node) const
    {
        if (auto derived = dynamic_cast<ExprNode*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<PatternNode*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<ScopedNode*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<FunctionBody*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<RecordNode*>(node))
        {
            return visit(derived);
        }
        unreachable();
    }

    any AstVisitor::visit(ScopedNode* node) const
    {
        if (auto derived = dynamic_cast<ImportClauseExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<FunctionExpr*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<LetExpr*>(node))
        {
            return visit(derived);
        }
        unreachable();
    }

    any AstVisitor::visit(PatternNode* node) const
    {
        if (auto derived = dynamic_cast<UnderscoreNode*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<UnderscorePattern*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<PatternWithGuards*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<PatternWithoutGuards*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<PatternValue*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<AsDataStructurePattern*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<TuplePattern*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<SeqPattern*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<HeadTailsPattern*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<TailsHeadPattern*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<HeadTailsHeadPattern*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<DictPattern*>(node))
        {
            return visit(derived);
        }
        if (auto derived = dynamic_cast<RecordPattern*>(node))
        {
            return visit(derived);
        }
        unreachable();
    }
}
