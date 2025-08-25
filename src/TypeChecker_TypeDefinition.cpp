#include "TypeChecker.h"
#include "ast.h"

namespace yona::typechecker {

Type TypeChecker::visit(TypeDefinition *node) const {
    // TypeDefinition represents a type alias or sum type definition
    // e.g., type MyInt = Int or type Option = None | Some

    if (!node->name) {
        return Type(nullptr);
    }

    // Get the type name
    string type_name;
    if (auto user_defined = dynamic_cast<UserDefinedTypeNode*>(node->name)) {
        type_name = user_defined->name->value;
    } else {
        return Type(nullptr);
    }

    // Process the type definitions (alternatives for sum types)
    if (node->definitions.empty()) {
        return Type(nullptr);
    }

    if (node->definitions.size() == 1) {
        // Simple type alias
        return type_node_to_type(node->definitions[0]);
    } else {
        // Sum type with multiple alternatives
        auto sum_type = make_shared<SumType>();
        sum_type->name = type_name;

        for (auto* type_node : node->definitions) {
            Type alt_type = type_node_to_type(type_node);
            sum_type->types.push_back(alt_type);
        }

        return Type(sum_type);
    }
}

} // namespace yona::typechecker
