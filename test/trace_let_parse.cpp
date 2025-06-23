#include <gtest/gtest.h>
#include "Parser.h"
#include "ast.h"
#include <iostream>
#include <set>

using namespace yona;
using namespace yona::ast;
using namespace yona::parser;
using namespace std;

void trace_ast(AstNode* node, int depth = 0, set<AstNode*>* visited = nullptr) {
    set<AstNode*> local_visited;
    if (!visited) visited = &local_visited;
    
    if (visited->count(node) > 0) {
        cout << string(depth * 2, ' ') << "CIRCULAR REFERENCE DETECTED!" << endl;
        return;
    }
    visited->insert(node);
    
    string indent(depth * 2, ' ');
    cout << indent << "Node type: " << node->get_type() << " at " << node << endl;
    
    if (auto let_expr = dynamic_cast<LetExpr*>(node)) {
        cout << indent << "LetExpr with " << let_expr->aliases.size() << " aliases" << endl;
        for (auto alias : let_expr->aliases) {
            trace_ast(alias, depth + 1, visited);
        }
        cout << indent << "Body:" << endl;
        trace_ast(let_expr->expr, depth + 1, visited);
    } else if (auto lambda_alias = dynamic_cast<LambdaAlias*>(node)) {
        cout << indent << "LambdaAlias: " << lambda_alias->name->value << endl;
        trace_ast(lambda_alias->lambda, depth + 1, visited);
    } else if (auto func_expr = dynamic_cast<FunctionExpr*>(node)) {
        cout << indent << "FunctionExpr: " << func_expr->name << " with " << func_expr->patterns.size() << " params" << endl;
        for (auto body : func_expr->bodies) {
            trace_ast(body, depth + 1, visited);
        }
    } else if (auto body = dynamic_cast<BodyWithoutGuards*>(node)) {
        cout << indent << "BodyWithoutGuards:" << endl;
        trace_ast(body->expr, depth + 1, visited);
    } else if (auto main_node = dynamic_cast<MainNode*>(node)) {
        cout << indent << "MainNode:" << endl;
        trace_ast(main_node->node, depth + 1, visited);
    } else if (auto id_expr = dynamic_cast<IdentifierExpr*>(node)) {
        cout << indent << "IdentifierExpr: " << id_expr->name->value << endl;
    } else if (auto apply_expr = dynamic_cast<ApplyExpr*>(node)) {
        cout << indent << "ApplyExpr with " << apply_expr->args.size() << " args" << endl;
        cout << indent << "Call:" << endl;
        trace_ast(apply_expr->call, depth + 1, visited);
        cout << indent << "Args:" << endl;
        for (auto& arg : apply_expr->args) {
            if (auto expr = get_if<ExprNode*>(&arg)) {
                trace_ast(*expr, depth + 1, visited);
            } else if (auto val = get_if<ValueExpr*>(&arg)) {
                trace_ast(*val, depth + 1, visited);
            }
        }
    } else if (auto name_call = dynamic_cast<NameCall*>(node)) {
        cout << indent << "NameCall: " << (name_call->name ? name_call->name->value : "NULL") << endl;
    } else if (auto expr_call = dynamic_cast<ExprCall*>(node)) {
        cout << indent << "ExprCall:" << endl;
        trace_ast(expr_call->expr, depth + 1, visited);
    } else if (auto int_expr = dynamic_cast<IntegerExpr*>(node)) {
        cout << indent << "IntegerExpr: " << int_expr->value << endl;
    }
}

TEST(TraceLetParse, LetWithLambda) {
    Parser parser;
    
    stringstream ss("let f = \\(x) -> x in f(42)");
    cout << "Parsing: " << ss.str() << endl;
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.success);
    
    cout << "\nTracing AST structure:" << endl;
    trace_ast(parse_result.node.get());
}