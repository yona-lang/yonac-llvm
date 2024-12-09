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

    any AstNode::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

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
                else
                {
                    tmp = tmp->parent;
                }
            }
            while (tmp != nullptr);
            return nullptr;
        }
    }

    any ScopedNode::accept(const AstVisitor& visitor) { return AstNode::accept(visitor); }

    any OpExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }

    BinaryOpExpr::BinaryOpExpr(Token token, ExprNode left, ExprNode right) :
        OpExpr(token), left(*left.with_parent<ExprNode>(this)), right(*right.with_parent<ExprNode>(this))
    {
    }

    Type BinaryOpExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left.infer_type(ctx);
        const Type rightType = left.infer_type(ctx);

        if (holds_alternative<ValueType>(leftType) != Int && holds_alternative<ValueType>(leftType) != Float)
        {
            ctx.addError(TypeError(token, "Binary expression must be numeric type"));
        }

        if (holds_alternative<ValueType>(rightType) != Int && holds_alternative<ValueType>(rightType) != Float)
        {
            ctx.addError(TypeError(token, "Binary expression must be numeric type"));
        }

        return unordered_set{ leftType, rightType }.contains(Float) ? Float : Int;
    }

    any BinaryOpExpr::accept(const AstVisitor& visitor) { return OpExpr::accept(visitor); }

    NameExpr::NameExpr(Token token, string value) : ExprNode(token), value(std::move(value)) {}

    any NameExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

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

    IdentifierExpr::IdentifierExpr(Token token, NameExpr name) :
        ValueExpr(token), name(*name.with_parent<NameExpr>(this))
    {
    }

    any IdentifierExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type IdentifierExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    RecordNode::RecordNode(Token token, NameExpr recordType, const vector<IdentifierExpr>& identifiers) :
        AstNode(token), recordType(*recordType.with_parent<NameExpr>(this)),
        identifiers(nodes_with_parent(identifiers, this))
    {
    }

    any RecordNode::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type RecordNode::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    TrueLiteralExpr::TrueLiteralExpr(Token) : LiteralExpr<bool>(token, true) {}

    any TrueLiteralExpr::accept(const ::yona::ast::AstVisitor& visitor) { return visitor.visit(*this); }

    Type TrueLiteralExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    FalseLiteralExpr::FalseLiteralExpr(Token) : LiteralExpr<bool>(token, false) {}

    any FalseLiteralExpr::accept(const ::yona::ast::AstVisitor& visitor) { return visitor.visit(*this); }

    Type FalseLiteralExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    FloatExpr::FloatExpr(Token token, float value) : LiteralExpr<float>(token, value) {}

    any FloatExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type FloatExpr::infer_type(TypeInferenceContext& ctx) const { return Float; }

    IntegerExpr::IntegerExpr(Token token, int value) : LiteralExpr<int>(token, value) {}

    any IntegerExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type IntegerExpr::infer_type(TypeInferenceContext& ctx) const { return Int; }

    any ExprNode::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    any PatternNode::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    any UnderscoreNode::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type UnderscoreNode::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    any ValueExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }

    ByteExpr::ByteExpr(Token token, unsigned char value) : LiteralExpr<unsigned char>(token, value) {}

    any ByteExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ByteExpr::infer_type(TypeInferenceContext& ctx) const { return Byte; }

    StringExpr::StringExpr(Token token, string value) : LiteralExpr<string>(token, value) {}

    any StringExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type StringExpr::infer_type(TypeInferenceContext& ctx) const { return String; }

    CharacterExpr::CharacterExpr(Token token, const char value) : LiteralExpr<char>(token, value) {}

    any CharacterExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type CharacterExpr::infer_type(TypeInferenceContext& ctx) const { return Char; }

    UnitExpr::UnitExpr(Token) : LiteralExpr<nullptr_t>(token, nullptr) {}

    any UnitExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type UnitExpr::infer_type(TypeInferenceContext& ctx) const { return Unit; }

    TupleExpr::TupleExpr(Token token, const vector<ExprNode>& values) :
        ValueExpr(token), values(nodes_with_parent(values, this))
    {
    }

    any TupleExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type TupleExpr::infer_type(TypeInferenceContext& ctx) const
    {
        vector<Type> fieldTypes;
        ranges::for_each(values, [&](const ExprNode& expr) { fieldTypes.push_back(expr.infer_type(ctx)); });
        return make_shared<TupleType>(fieldTypes);
    }

    DictExpr::DictExpr(Token token, const vector<pair<ExprNode, ExprNode>>& values) :
        ValueExpr(token), values(nodes_with_parent(values, this))
    {
    }

    any DictExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type DictExpr::infer_type(TypeInferenceContext& ctx) const
    {
        unordered_set<Type> keyTypes;
        unordered_set<Type> valueTypes;
        ranges::for_each(values,
                         [&](pair<ExprNode, ExprNode> p)
                         {
                             keyTypes.insert(p.first.infer_type(ctx));
                             valueTypes.insert(p.second.infer_type(ctx));
                         });

        if (keyTypes.size() > 1 || valueTypes.size() > 1)
        {
            ctx.addError(TypeError(token, "Dictionary keys and values must have the same type"));
        }

        return make_shared<DictCollectionType>(values.front().first.infer_type(ctx),
                                               values.front().second.infer_type(ctx));
    }

    ValuesSequenceExpr::ValuesSequenceExpr(Token token, const vector<ExprNode>& values) :
        SequenceExpr(token), values(nodes_with_parent(values, this))
    {
    }

    any ValuesSequenceExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ValuesSequenceExpr::infer_type(TypeInferenceContext& ctx) const
    {
        unordered_set<Type> valueTypes;
        ranges::for_each(values, [&](const ExprNode& expr) { valueTypes.insert(expr.infer_type(ctx)); });

        if (valueTypes.size() > 1)
        {
            ctx.addError(TypeError(token, "Sequence values must have the same type"));
        }

        return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Seq, values.front().infer_type(ctx));
    }

    RangeSequenceExpr::RangeSequenceExpr(Token token, ExprNode start, ExprNode end, ExprNode step) :
        SequenceExpr(token), start(*start.with_parent<ExprNode>(this)), end(*end.with_parent<ExprNode>(this)),
        step(*step.with_parent<ExprNode>(this))
    {
    }

    any RangeSequenceExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type RangeSequenceExpr::infer_type(TypeInferenceContext& ctx) const
    {
        Type startExprType = start.infer_type(ctx);
        Type endExprType = start.infer_type(ctx);
        Type stepExprType = start.infer_type(ctx);

        if (!holds_alternative<ValueType>(startExprType) || get<ValueType>(startExprType) != Int)
        {
            ctx.addError(TypeError(start.token, "Sequence start expression must be integer"));
        }

        if (!holds_alternative<ValueType>(endExprType) || get<ValueType>(endExprType) != Int)
        {
            ctx.addError(TypeError(end.token, "Sequence end expression must be integer"));
        }

        if (!holds_alternative<ValueType>(stepExprType) || get<ValueType>(stepExprType) != Int)
        {
            ctx.addError(TypeError(step.token, "Sequence step expression must be integer"));
        }

        return nullptr;
    }

    SetExpr::SetExpr(Token token, const vector<ExprNode>& values) :
        ValueExpr(token), values(nodes_with_parent(values, this))
    {
    }

    any SetExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type SetExpr::infer_type(TypeInferenceContext& ctx) const
    {
        unordered_set<Type> valueTypes;
        ranges::for_each(values, [&](const ExprNode& expr) { valueTypes.insert(expr.infer_type(ctx)); });

        if (valueTypes.size() > 1)
        {
            ctx.addError(TypeError(token, "Set values must have the same type"));
        }

        return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Set, values.front().infer_type(ctx));
    }

    SymbolExpr::SymbolExpr(Token token, string value) : ValueExpr(token), value(std::move(value)) {}

    any SymbolExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type SymbolExpr::infer_type(TypeInferenceContext& ctx) const { return Symbol; }

    PackageNameExpr::PackageNameExpr(Token token, const vector<NameExpr>& parts) :
        ValueExpr(token), parts(nodes_with_parent(parts, this))
    {
    }

    any PackageNameExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type PackageNameExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    FqnExpr::FqnExpr(Token token, PackageNameExpr packageName, NameExpr moduleName) :
        ValueExpr(token), packageName(*packageName.with_parent<PackageNameExpr>(this)),
        moduleName(*moduleName.with_parent<NameExpr>(this))
    {
    }

    any FqnExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type FqnExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    FunctionExpr::FunctionExpr(Token token, string name, const vector<PatternNode>& patterns,
                               const vector<FunctionBody>& bodies) :
        ScopedNode(token), name(std::move(name)), patterns(nodes_with_parent(patterns, this)),
        bodies(nodes_with_parent(bodies, this))
    {
    }

    any FunctionExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type FunctionExpr::infer_type(TypeInferenceContext& ctx) const
    {
        unordered_set<Type> bodyTypes;
        ranges::for_each(bodies, [&](const FunctionBody& body) { bodyTypes.insert(body.infer_type(ctx)); });

        return make_shared<SumType>(bodyTypes);
    }

    BodyWithGuards::BodyWithGuards(Token token, ExprNode guard, const vector<ExprNode>& expr) :
        FunctionBody(token), guard(*guard.with_parent<ExprNode>(this)), exprs(nodes_with_parent(expr, this))
    {
    }

    any BodyWithGuards::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type BodyWithGuards::infer_type(TypeInferenceContext& ctx) const
    {
        const Type guardType = guard.infer_type(ctx);

        if (!holds_alternative<ValueType>(guardType) || get<ValueType>(guardType) != Bool)
        {
            ctx.addError(TypeError(guard.token, "Guard expression must be boolean"));
        }

        return exprs.back().infer_type(ctx);
    }

    BodyWithoutGuards::BodyWithoutGuards(Token token, ExprNode expr) :
        FunctionBody(token), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    any BodyWithoutGuards::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type BodyWithoutGuards::infer_type(TypeInferenceContext& ctx) const { return expr.infer_type(ctx); }

    ModuleExpr::ModuleExpr(Token token, FqnExpr fqn, const vector<string>& exports, const vector<RecordNode>& records,
                           const vector<FunctionExpr>& functions) :
        ValueExpr(token), fqn(*fqn.with_parent<FqnExpr>(this)), exports(exports),
        records(nodes_with_parent(records, this)), functions(nodes_with_parent(functions, this))
    {
    }

    any ModuleExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ModuleExpr::infer_type(TypeInferenceContext& ctx) const { return Module; }

    RecordInstanceExpr::RecordInstanceExpr(Token token, NameExpr recordType,
                                           const vector<pair<NameExpr, ExprNode>>& items) :
        ValueExpr(token), recordType(*recordType.with_parent<NameExpr>(this)), items(nodes_with_parent(items, this))
    {
    }

    any RecordInstanceExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type RecordInstanceExpr::infer_type(TypeInferenceContext& ctx) const
    {
        vector<Type> itemTypes{ Symbol };
        ranges::for_each(items,
                         [&](const pair<NameExpr, ExprNode>& p) { itemTypes.push_back(p.second.infer_type(ctx)); });

        return make_shared<TupleType>(itemTypes);
    }

    LogicalNotOpExpr::LogicalNotOpExpr(Token token, ExprNode expr) :
        OpExpr(token), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    any LogicalNotOpExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type LogicalNotOpExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type exprType = expr.infer_type(ctx);

        if (!holds_alternative<ValueType>(exprType) || get<ValueType>(exprType) != Bool)
        {
            ctx.addError(TypeError(expr.token, "Expression for logical negation must be boolean"));
        }

        return Bool;
    }

    BinaryNotOpExpr::BinaryNotOpExpr(Token token, ExprNode expr) :
        OpExpr(token), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    any BinaryNotOpExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type BinaryNotOpExpr::infer_type(TypeInferenceContext& ctx) const
    {
        Type exprType = expr.infer_type(ctx);

        if (!holds_alternative<ValueType>(exprType) || get<ValueType>(exprType) != Bool)
        {
            ctx.addError(TypeError(expr.token, "Expression for binary negation must be boolean"));
        }

        return Bool;
    }

    PowerExpr::PowerExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any PowerExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type PowerExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    MultiplyExpr::MultiplyExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any MultiplyExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type MultiplyExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    DivideExpr::DivideExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any DivideExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type DivideExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    ModuloExpr::ModuloExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any ModuloExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ModuloExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    AddExpr::AddExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any AddExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type AddExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    SubtractExpr::SubtractExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any SubtractExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type SubtractExpr::infer_type(TypeInferenceContext& ctx) const { return BinaryOpExpr::infer_type(ctx); }

    LeftShiftExpr::LeftShiftExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any LeftShiftExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type LeftShiftExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left.infer_type(ctx);
        const Type rightType = right.infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for left shift must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for left shift must be integer"));
        }

        return Int;
    }

    RightShiftExpr::RightShiftExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any RightShiftExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type RightShiftExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left.infer_type(ctx);
        const Type rightType = right.infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for right shift must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for right shift must be integer"));
        }

        return Int;
    }

    ZerofillRightShiftExpr::ZerofillRightShiftExpr(Token token, ExprNode left, ExprNode right) :
        BinaryOpExpr(token, left, right)
    {
    }

    any ZerofillRightShiftExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ZerofillRightShiftExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left.infer_type(ctx);
        const Type rightType = right.infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for zerofill right shift must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for zerofill right shift must be integer"));
        }

        return Int;
    }

    GteExpr::GteExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any GteExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type GteExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    LteExpr::LteExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any LteExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type LteExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    GtExpr::GtExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any GtExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type GtExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    LtExpr::LtExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any LtExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type LtExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    EqExpr::EqExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any EqExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type EqExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    NeqExpr::NeqExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any NeqExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type NeqExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    ConsLeftExpr::ConsLeftExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any ConsLeftExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ConsLeftExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    ConsRightExpr::ConsRightExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any ConsRightExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ConsRightExpr::infer_type(TypeInferenceContext& ctx) const { return Bool; }

    JoinExpr::JoinExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any JoinExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type JoinExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left.infer_type(ctx);
        const Type rightType = right.infer_type(ctx);

        if (!holds_alternative<shared_ptr<SingleItemCollectionType>>(leftType) ||
            !holds_alternative<shared_ptr<DictCollectionType>>(leftType) ||
            !holds_alternative<shared_ptr<TupleType>>(leftType))
        {
            ctx.addError(TypeError(left.token, "Join expression can be used only for collections"));
        }

        if (!holds_alternative<shared_ptr<SingleItemCollectionType>>(rightType) ||
            !holds_alternative<shared_ptr<DictCollectionType>>(rightType) ||
            !holds_alternative<shared_ptr<TupleType>>(rightType))
        {
            ctx.addError(TypeError(right.token, "Join expression can be used only for collections"));
        }

        if (leftType != rightType)
        {
            ctx.addError(TypeError(token, "Join expression can be used only on same types"));
        }

        return leftType;
    }

    BitwiseAndExpr::BitwiseAndExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any BitwiseAndExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type BitwiseAndExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left.infer_type(ctx);
        const Type rightType = right.infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for bitwise AND must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for bitwise AND must be integer"));
        }

        return Bool;
    }

    BitwiseXorExpr::BitwiseXorExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any BitwiseXorExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type BitwiseXorExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left.infer_type(ctx);
        const Type rightType = right.infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for bitwise XOR must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for bitwise XOR must be integer"));
        }

        return Bool;
    }

    BitwiseOrExpr::BitwiseOrExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any BitwiseOrExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type BitwiseOrExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left.infer_type(ctx);
        const Type rightType = right.infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for bitwise OR must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Int)
        {
            ctx.addError(TypeError(left.token, "Expression for bitwise OR must be integer"));
        }

        return Bool;
    }

    LogicalAndExpr::LogicalAndExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any LogicalAndExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type LogicalAndExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left.infer_type(ctx);
        const Type rightType = right.infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Bool)
        {
            ctx.addError(TypeError(left.token, "Expression for logical AND must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Bool)
        {
            ctx.addError(TypeError(left.token, "Expression for logical AND must be integer"));
        }

        return Bool;
    }

    LogicalOrExpr::LogicalOrExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any LogicalOrExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type LogicalOrExpr::infer_type(TypeInferenceContext& ctx) const
    {
        const Type leftType = left.infer_type(ctx);
        const Type rightType = right.infer_type(ctx);

        if (!holds_alternative<ValueType>(leftType) || get<ValueType>(leftType) != Bool)
        {
            ctx.addError(TypeError(left.token, "Expression for logical OR must be integer"));
        }

        if (!holds_alternative<ValueType>(rightType) || get<ValueType>(rightType) != Bool)
        {
            ctx.addError(TypeError(left.token, "Expression for logical OR must be integer"));
        }

        return Bool;
    }

    InExpr::InExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any InExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type InExpr::infer_type(TypeInferenceContext& ctx) const
    {
        if (const Type rightType = right.infer_type(ctx);
            !holds_alternative<shared_ptr<SingleItemCollectionType>>(rightType) ||
            !holds_alternative<shared_ptr<DictCollectionType>>(rightType) ||
            !holds_alternative<shared_ptr<TupleType>>(rightType))
        {
            ctx.addError(TypeError(left.token, "Expression for IN must be a collection"));
        }

        return Bool;
    }

    PipeLeftExpr::PipeLeftExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any PipeLeftExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type PipeLeftExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    PipeRightExpr::PipeRightExpr(Token token, ExprNode left, ExprNode right) : BinaryOpExpr(token, left, right) {}

    any PipeRightExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type PipeRightExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    LetExpr::LetExpr(Token token, const vector<AliasExpr>& aliases, ExprNode expr) :
        ScopedNode(token), aliases(nodes_with_parent(aliases, this)), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    any LetExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type LetExpr::infer_type(TypeInferenceContext& ctx) const { return expr.infer_type(ctx); }

    IfExpr::IfExpr(Token token, ExprNode condition, ExprNode thenExpr, const optional<ExprNode>& elseExpr) :
        ExprNode(token), condition(*condition.with_parent<ExprNode>(this)),
        thenExpr(*thenExpr.with_parent<ExprNode>(this)), elseExpr(node_with_parent(elseExpr, this))
    {
    }

    any IfExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type IfExpr::infer_type(TypeInferenceContext& ctx) const
    {
        if (const Type conditionType = condition.infer_type(ctx);
            !holds_alternative<ValueType>(conditionType) || get<ValueType>(conditionType) != Bool)
        {
            ctx.addError(TypeError(condition.token, "If condition must be boolean"));
        }

        unordered_set returnTypes{ thenExpr.infer_type(ctx) };

        if (elseExpr.has_value())
        {
            returnTypes.insert(elseExpr.value().infer_type(ctx));
        }

        return make_shared<SumType>(returnTypes);
    }

    ApplyExpr::ApplyExpr(Token token, CallExpr call, const vector<variant<ExprNode, ValueExpr>>& args) :
        ExprNode(token), call(*call.with_parent<CallExpr>(this)), args(nodes_with_parent(args, this))
    {
    }

    any ApplyExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ApplyExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    DoExpr::DoExpr(Token token, const vector<ExprNode>& steps) : ExprNode(token), steps(steps) {}

    any DoExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type DoExpr::infer_type(TypeInferenceContext& ctx) const { return steps.back().infer_type(ctx); }

    ImportExpr::ImportExpr(Token token, const vector<ImportClauseExpr>& clauses, ExprNode expr) :
        ScopedNode(token), clauses(nodes_with_parent(clauses, this)), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    any ImportExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ImportExpr::infer_type(TypeInferenceContext& ctx) const { return expr.infer_type(ctx); }

    RaiseExpr::RaiseExpr(Token token, SymbolExpr symbol, LiteralExpr<string> message) :
        ExprNode(token), symbol(*symbol.with_parent<SymbolExpr>(this)),
        message(*message.with_parent<LiteralExpr<string>>(this))
    {
    }

    any RaiseExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type RaiseExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    WithExpr::WithExpr(Token token, ExprNode contextExpr, const optional<NameExpr>& name, ExprNode bodyExpr) :
        ScopedNode(token), contextExpr(*contextExpr.with_parent<ExprNode>(this)), name(node_with_parent(name, this)),
        bodyExpr(*bodyExpr.with_parent<ExprNode>(this))
    {
    }

    any WithExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type WithExpr::infer_type(TypeInferenceContext& ctx) const { return bodyExpr.infer_type(ctx); }

    FieldAccessExpr::FieldAccessExpr(Token token, IdentifierExpr identifier, NameExpr name) :
        ExprNode(token), identifier(*identifier.with_parent<IdentifierExpr>(this)),
        name(*name.with_parent<NameExpr>(this))
    {
    }

    any FieldAccessExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type FieldAccessExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    FieldUpdateExpr::FieldUpdateExpr(Token token, IdentifierExpr identifier,
                                     const vector<pair<NameExpr, ExprNode>>& updates) :
        ExprNode(token), identifier(*identifier.with_parent<IdentifierExpr>(this)),
        updates(nodes_with_parent(updates, this))
    {
    }

    any FieldUpdateExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type FieldUpdateExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    LambdaAlias::LambdaAlias(Token token, NameExpr name, FunctionExpr lambda) :
        AliasExpr(token), name(*name.with_parent<NameExpr>(this)), lambda(*lambda.with_parent<FunctionExpr>(this))
    {
    }

    any LambdaAlias::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type LambdaAlias::infer_type(TypeInferenceContext& ctx) const { return lambda.infer_type(ctx); }

    ModuleAlias::ModuleAlias(Token token, NameExpr name, ModuleExpr module) :
        AliasExpr(token), name(*name.with_parent<NameExpr>(this)), module(*module.with_parent<ModuleExpr>(this))
    {
    }

    any ModuleAlias::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ModuleAlias::infer_type(TypeInferenceContext& ctx) const { return module.infer_type(ctx); }

    ValueAlias::ValueAlias(Token token, IdentifierExpr identifier, ExprNode expr) :
        AliasExpr(token), identifier(*identifier.with_parent<IdentifierExpr>(this)),
        expr(*expr.with_parent<ExprNode>(this))
    {
    }

    any ValueAlias::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ValueAlias::infer_type(TypeInferenceContext& ctx) const { return expr.infer_type(ctx); }

    PatternAlias::PatternAlias(Token token, PatternNode pattern, ExprNode expr) :
        AliasExpr(token), pattern(*pattern.with_parent<PatternNode>(this)), expr(*expr.with_parent<ExprNode>(this))
    {
    }

    any PatternAlias::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type PatternAlias::infer_type(TypeInferenceContext& ctx) const { return expr.infer_type(ctx); }

    FqnAlias::FqnAlias(Token token, NameExpr name, FqnExpr fqn) :
        AliasExpr(token), name(*name.with_parent<NameExpr>(this)), fqn(*fqn.with_parent<FqnExpr>(this))
    {
    }

    any FqnAlias::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type FqnAlias::infer_type(TypeInferenceContext& ctx) const { return fqn.infer_type(ctx); }

    FunctionAlias::FunctionAlias(Token token, NameExpr name, NameExpr alias) :
        AliasExpr(token), name(*name.with_parent<NameExpr>(this)), alias(*alias.with_parent<NameExpr>(this))
    {
    }

    any FunctionAlias::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type FunctionAlias::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    AliasCall::AliasCall(Token token, NameExpr alias, NameExpr funName) :
        CallExpr(token), alias(*alias.with_parent<NameExpr>(this)), funName(*funName.with_parent<NameExpr>(this))
    {
    }

    any AliasCall::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type AliasCall::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    NameCall::NameCall(Token token, NameExpr name) : CallExpr(token), name(*name.with_parent<NameExpr>(this)) {}

    any NameCall::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type NameCall::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    ModuleCall::ModuleCall(Token token, const variant<FqnExpr, ExprNode>& fqn, NameExpr funName) :
        CallExpr(token), fqn(node_with_parent(fqn, this)), funName(*funName.with_parent<NameExpr>(this))
    {
    }

    any ModuleCall::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ModuleCall::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    ModuleImport::ModuleImport(Token token, FqnExpr fqn, NameExpr name) :
        ImportClauseExpr(token), fqn(*fqn.with_parent<FqnExpr>(this)), name(*name.with_parent<NameExpr>(this))
    {
    }

    any ModuleImport::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ModuleImport::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    FunctionsImport::FunctionsImport(Token token, const vector<FunctionAlias>& aliases, FqnExpr fromFqn) :
        ImportClauseExpr(token), aliases(nodes_with_parent(aliases, this)), fromFqn(*fromFqn.with_parent<FqnExpr>(this))
    {
    }

    any FunctionsImport::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type FunctionsImport::infer_type(TypeInferenceContext& ctx) const
    {
        return nullptr; // TODO
    }

    SeqGeneratorExpr::SeqGeneratorExpr(Token token, ExprNode reducerExpr, CollectionExtractorExpr collectionExtractor,
                                       ExprNode stepExpression) :
        GeneratorExpr(token), reducerExpr(*reducerExpr.with_parent<ExprNode>(this)),
        collectionExtractor(*collectionExtractor.with_parent<CollectionExtractorExpr>(this)),
        stepExpression(*stepExpression.with_parent<ExprNode>(this))
    {
    }

    any SeqGeneratorExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type SeqGeneratorExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Seq, reducerExpr.infer_type(ctx));
    }

    SetGeneratorExpr::SetGeneratorExpr(Token token, ExprNode reducerExpr, CollectionExtractorExpr collectionExtractor,
                                       ExprNode stepExpression) :
        GeneratorExpr(token), reducerExpr(*reducerExpr.with_parent<ExprNode>(this)),
        collectionExtractor(*collectionExtractor.with_parent<CollectionExtractorExpr>(this)),
        stepExpression(*stepExpression.with_parent<ExprNode>(this))
    {
    }

    any SetGeneratorExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type SetGeneratorExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return make_shared<SingleItemCollectionType>(SingleItemCollectionType::Set, reducerExpr.infer_type(ctx));
    }

    DictGeneratorReducer::DictGeneratorReducer(Token token, ExprNode key, ExprNode value) :
        ExprNode(token), key(*key.with_parent<ExprNode>(this)), value(*value.with_parent<ExprNode>(this))
    {
    }

    any DictGeneratorReducer::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type DictGeneratorReducer::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    DictGeneratorExpr::DictGeneratorExpr(Token token, DictGeneratorReducer reducerExpr,
                                         CollectionExtractorExpr collectionExtractor, ExprNode stepExpression) :
        GeneratorExpr(token), reducerExpr(*reducerExpr.with_parent<DictGeneratorReducer>(this)),
        collectionExtractor(*collectionExtractor.with_parent<CollectionExtractorExpr>(this)),
        stepExpression(*stepExpression.with_parent<ExprNode>(this))
    {
    }

    any DictGeneratorExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type DictGeneratorExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return make_shared<DictCollectionType>(reducerExpr.key.infer_type(ctx), reducerExpr.value.infer_type(ctx));
    }

    ValueCollectionExtractorExpr::ValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore expr) :
        CollectionExtractorExpr(token), expr(node_with_parent(expr, this))
    {
    }

    any ValueCollectionExtractorExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type ValueCollectionExtractorExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    KeyValueCollectionExtractorExpr::KeyValueCollectionExtractorExpr(Token token, IdentifierOrUnderscore keyExpr,
                                                                     IdentifierOrUnderscore valueExpr) :
        CollectionExtractorExpr(token), keyExpr(node_with_parent(keyExpr, this)),
        valueExpr(node_with_parent(valueExpr, this))
    {
    }

    any KeyValueCollectionExtractorExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type KeyValueCollectionExtractorExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    PatternWithGuards::PatternWithGuards(Token token, ExprNode guard, ExprNode exprNode) :
        PatternNode(token), guard(*guard.with_parent<ExprNode>(this)), exprNode(*exprNode.with_parent<ExprNode>(this))
    {
    }

    any PatternWithGuards::accept(const AstVisitor& visitor) { return visitor.visit(*this); };

    Type PatternWithGuards::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    PatternWithoutGuards::PatternWithoutGuards(Token token, ExprNode exprNode) :
        PatternNode(token), exprNode(*exprNode.with_parent<ExprNode>(this))
    {
    }

    any PatternWithoutGuards::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type PatternWithoutGuards::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    PatternExpr::PatternExpr(Token token,
                             const variant<Pattern, PatternWithoutGuards, vector<PatternWithGuards>>& patternExpr) :
        ExprNode(token), patternExpr(patternExpr)
    {
        // std::visit({ [this](Pattern& arg) { arg.with_parent<Pattern>(this); },
        //              [this](PatternWithoutGuards& arg) { arg.with_parent<PatternWithGuards>(this); },
        //              [this](vector<PatternWithGuards>& arg) { nodes_with_parent(arg, this); } },
        //            patternExpr); // TODO
    }
    any PatternExpr::accept(const AstVisitor& visitor) { return ExprNode::accept(visitor); }

    Type PatternExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    CatchPatternExpr::CatchPatternExpr(Token token, Pattern matchPattern,
                                       const variant<PatternWithoutGuards, vector<PatternWithGuards>>& pattern) :
        ExprNode(token), matchPattern(*matchPattern.with_parent<Pattern>(this)), pattern(pattern)
    {
        // std::visit({ [this](PatternWithoutGuards& arg) { arg.with_parent<PatternWithGuards>(this); },
        //              [this](vector<PatternWithGuards>& arg) { nodes_with_parent(arg, this); } },
        //            pattern); // TODO
    }

    any CatchPatternExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type CatchPatternExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    CatchExpr::CatchExpr(Token token, const vector<CatchPatternExpr>& patterns) :
        ExprNode(token), patterns(nodes_with_parent(patterns, this))
    {
    }

    any CatchExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type CatchExpr::infer_type(TypeInferenceContext& ctx) const { return patterns.back().infer_type(ctx); }

    TryCatchExpr::TryCatchExpr(Token token, ExprNode tryExpr, CatchExpr catchExpr) :
        ExprNode(token), tryExpr(*tryExpr.with_parent<ExprNode>(this)),
        catchExpr(*catchExpr.with_parent<CatchExpr>(this))
    {
    }

    any TryCatchExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type TryCatchExpr::infer_type(TypeInferenceContext& ctx) const
    {
        return make_shared<SumType>(unordered_set{ tryExpr.infer_type(ctx), catchExpr.infer_type(ctx) });
    }

    PatternValue::PatternValue(
        Token token, const variant<LiteralExpr<nullptr_t>, LiteralExpr<void*>, SymbolExpr, IdentifierExpr>& expr) :
        PatternNode(token), expr(node_with_parent(expr, this))
    {
    }

    any PatternValue::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type PatternValue::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    AsDataStructurePattern::AsDataStructurePattern(Token token, IdentifierExpr identifier,
                                                   DataStructurePattern pattern) :
        PatternNode(token), identifier(*identifier.with_parent<IdentifierExpr>(this)),
        pattern(*pattern.with_parent<DataStructurePattern>(this))
    {
    }

    any AsDataStructurePattern::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type AsDataStructurePattern::infer_type(TypeInferenceContext& ctx) const { return pattern.infer_type(ctx); }

    any UnderscorePattern::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type UnderscorePattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    TuplePattern::TuplePattern(Token token, const vector<Pattern>& patterns) :
        PatternNode(token), patterns(nodes_with_parent(patterns, this))
    {
    }

    any TuplePattern::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type TuplePattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    SeqPattern::SeqPattern(Token token, const vector<Pattern>& patterns) :
        PatternNode(token), patterns(nodes_with_parent(patterns, this))
    {
    }

    any SeqPattern::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type SeqPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    HeadTailsPattern::HeadTailsPattern(Token token, const vector<PatternWithoutSequence>& heads, TailPattern tail) :
        PatternNode(token), heads(nodes_with_parent(heads, this)), tail(*tail.with_parent<TailPattern>(this))
    {
    }

    any HeadTailsPattern::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type HeadTailsPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    TailsHeadPattern::TailsHeadPattern(Token token, TailPattern tail, const vector<PatternWithoutSequence>& heads) :
        PatternNode(token), tail(*tail.with_parent<TailPattern>(this)), heads(nodes_with_parent(heads, this))
    {
    }

    any TailsHeadPattern::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type TailsHeadPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    HeadTailsHeadPattern::HeadTailsHeadPattern(Token token, const vector<PatternWithoutSequence>& left,
                                               TailPattern tail, const vector<PatternWithoutSequence>& right) :
        PatternNode(token), left(nodes_with_parent(left, this)), tail(*tail.with_parent<TailPattern>(this)),
        right(nodes_with_parent(right, this))
    {
    }

    any HeadTailsHeadPattern::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type HeadTailsHeadPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    DictPattern::DictPattern(Token token, const vector<pair<PatternValue, Pattern>>& keyValuePairs) :
        PatternNode(token), keyValuePairs(nodes_with_parent(keyValuePairs, this))
    {
    }

    any DictPattern::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type DictPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    RecordPattern::RecordPattern(Token token, string recordType, const vector<pair<NameExpr, Pattern>>& items) :
        PatternNode(token), recordType(std::move(recordType)), items(nodes_with_parent(items, this))
    {
    }

    any RecordPattern::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type RecordPattern::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    CaseExpr::CaseExpr(Token token, ExprNode expr, const vector<PatternExpr>& patterns) :
        ExprNode(token), expr(*expr.with_parent<ExprNode>(this)), patterns(nodes_with_parent(patterns, this))
    {
    }

    any CaseExpr::accept(const AstVisitor& visitor) { return visitor.visit(*this); }

    Type CaseExpr::infer_type(TypeInferenceContext& ctx) const { return nullptr; }

    any AstVisitor::visit(const ExprNode& node) const
    {
        if (const AliasExpr* derived = dynamic_cast<const AliasExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const ApplyExpr* derived = dynamic_cast<const ApplyExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const CallExpr* derived = dynamic_cast<const CallExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const CaseExpr* derived = dynamic_cast<const CaseExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const CatchExpr* derived = dynamic_cast<const CatchExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const CatchPatternExpr* derived = dynamic_cast<const CatchPatternExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const CollectionExtractorExpr* derived = dynamic_cast<const CollectionExtractorExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const DictGeneratorReducer* derived = dynamic_cast<const DictGeneratorReducer*>(&node))
        {
            return visit(*derived);
        }
        if (const DoExpr* derived = dynamic_cast<const DoExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const FieldAccessExpr* derived = dynamic_cast<const FieldAccessExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const FieldUpdateExpr* derived = dynamic_cast<const FieldUpdateExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const GeneratorExpr* derived = dynamic_cast<const GeneratorExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const IfExpr* derived = dynamic_cast<const IfExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const NameExpr* derived = dynamic_cast<const NameExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const OpExpr* derived = dynamic_cast<const OpExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const PatternExpr* derived = dynamic_cast<const PatternExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const RaiseExpr* derived = dynamic_cast<const RaiseExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const SequenceExpr* derived = dynamic_cast<const SequenceExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const TryCatchExpr* derived = dynamic_cast<const TryCatchExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const ValueExpr* derived = dynamic_cast<const ValueExpr*>(&node))
        {
            return visit(*derived);
        }
        unreachable();
    }

    any AstVisitor::visit(const AstNode& node) const
    {
        if (const ExprNode* derived = dynamic_cast<const ExprNode*>(&node))
        {
            return visit(*derived);
        }
        if (const PatternNode* derived = dynamic_cast<const PatternNode*>(&node))
        {
            return visit(*derived);
        }
        if (const ScopedNode* derived = dynamic_cast<const ScopedNode*>(&node))
        {
            return visit(*derived);
        }
        if (const FunctionBody* derived = dynamic_cast<const FunctionBody*>(&node))
        {
            return visit(*derived);
        }
        if (const RecordNode* derived = dynamic_cast<const RecordNode*>(&node))
        {
            return visit(*derived);
        }
        unreachable();
    }

    any AstVisitor::visit(const ScopedNode& node) const
    {
        if (const ImportClauseExpr* derived = dynamic_cast<const ImportClauseExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const FunctionExpr* derived = dynamic_cast<const FunctionExpr*>(&node))
        {
            return visit(*derived);
        }
        if (const LetExpr* derived = dynamic_cast<const LetExpr*>(&node))
        {
            return visit(*derived);
        }
        unreachable();
    }

    any AstVisitor::visit(const PatternNode& node) const
    {
        if (const UnderscoreNode* derived = dynamic_cast<const UnderscoreNode*>(&node))
        {
            return visit(*derived);
        }
        if (const UnderscorePattern* derived = dynamic_cast<const UnderscorePattern*>(&node))
        {
            return visit(*derived);
        }
        if (const PatternWithGuards* derived = dynamic_cast<const PatternWithGuards*>(&node))
        {
            return visit(*derived);
        }
        if (const PatternWithoutGuards* derived = dynamic_cast<const PatternWithoutGuards*>(&node))
        {
            return visit(*derived);
        }
        if (const PatternValue* derived = dynamic_cast<const PatternValue*>(&node))
        {
            return visit(*derived);
        }
        if (const AsDataStructurePattern* derived = dynamic_cast<const AsDataStructurePattern*>(&node))
        {
            return visit(*derived);
        }
        if (const TuplePattern* derived = dynamic_cast<const TuplePattern*>(&node))
        {
            return visit(*derived);
        }
        if (const SeqPattern* derived = dynamic_cast<const SeqPattern*>(&node))
        {
            return visit(*derived);
        }
        if (const HeadTailsPattern* derived = dynamic_cast<const HeadTailsPattern*>(&node))
        {
            return visit(*derived);
        }
        if (const TailsHeadPattern* derived = dynamic_cast<const TailsHeadPattern*>(&node))
        {
            return visit(*derived);
        }
        if (const HeadTailsHeadPattern* derived = dynamic_cast<const HeadTailsHeadPattern*>(&node))
        {
            return visit(*derived);
        }
        if (const DictPattern* derived = dynamic_cast<const DictPattern*>(&node))
        {
            return visit(*derived);
        }
        if (const RecordPattern* derived = dynamic_cast<const RecordPattern*>(&node))
        {
            return visit(*derived);
        }
        unreachable();
    }
}
