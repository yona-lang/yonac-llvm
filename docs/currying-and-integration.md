# Function Currying and Type System Integration

## Current State

### Function Currying

**Status: Not Implemented** ❌

The Yona language currently does not support function currying or partial application. Functions must be called with all required arguments at once.

#### Current Implementation

1. **Function Definition**:
   ```yona
   add(x, y) -> x + y
   ```
   This creates a function expecting exactly 2 arguments.

2. **Function Application**:
   ```yona
   add(1, 2)  # Works: returns 3
   add(1)     # Error: missing argument
   ```

3. **Runtime Behavior**:
   - `ApplyExpr` in the interpreter expects all arguments
   - No mechanism to return partially applied functions
   - Functions fail if argument count doesn't match pattern count

### Type System Integration

**Status: Not Integrated** ❌

The type checker and interpreter operate independently with no integration.

#### Current State

1. **Type Checker**:
   - Standalone implementation in `TypeChecker.cpp`
   - Can infer types for expressions
   - Not invoked during interpretation

2. **Interpreter**:
   - No type information used during execution
   - No runtime type checking
   - Type errors only caught when operations fail

3. **Missing Integration Points**:
   - No type checking before interpretation
   - No type information stored in runtime values
   - No type-based optimizations

## Proposed Implementation

### Function Currying

To implement currying in Yona, we need to:

#### 1. Update Function Representation

```cpp
// Current (in runtime.h)
struct FunctionValue {
  shared_ptr<FqnValue> fqn;
  function<RuntimeObjectPtr(const vector<RuntimeObjectPtr>&)> code;
};

// Proposed addition
struct PartiallyAppliedFunction : FunctionValue {
  vector<RuntimeObjectPtr> applied_args;
  size_t remaining_args;
};
```

#### 2. Modify Function Application

```cpp
// In Interpreter::visit(ApplyExpr*)
any Interpreter::visit(ApplyExpr *node) const {
  auto func = evaluate_function(node);
  auto provided_args = evaluate_arguments(node);
  
  if (func->arity() > provided_args.size()) {
    // Return partially applied function
    return make_partial_application(func, provided_args);
  } else if (func->arity() == provided_args.size()) {
    // Full application
    return apply_function(func, provided_args);
  } else {
    // Too many arguments - apply and continue with result
    auto result = apply_function(func, take_n_args(provided_args, func->arity()));
    return apply_remaining_args(result, drop_n_args(provided_args, func->arity()));
  }
}
```

#### 3. Automatic Currying

Transform multi-parameter functions into curried form:

```yona
# User writes:
add(x, y) -> x + y

# Internally becomes:
add(x) -> \\y -> x + y
```

#### 4. Type System Support

Update `FunctionType` to properly represent curried functions:

```cpp
// For add : Int -> Int -> Int
auto add_type = make_shared<FunctionType>();
add_type->argumentType = Type(SignedInt64);
add_type->returnType = Type(make_shared<FunctionType>(
  SignedInt64,  // argument
  SignedInt64   // return
));
```

### Type System Integration

To integrate the type checker with the interpreter:

#### 1. Add Type Checking Phase

```cpp
// In Parser or main execution flow
ParseResult Parser::parse_and_check(istream& stream) {
  // Parse
  auto parse_result = parse_input(stream);
  if (!parse_result.success) return parse_result;
  
  // Type check
  TypeInferenceContext ctx;
  TypeChecker checker(ctx);
  Type inferred_type = checker.check(parse_result.node.get());
  
  if (ctx.has_errors()) {
    // Convert type errors to parse errors
    parse_result.success = false;
    for (const auto& error : ctx.get_errors()) {
      parse_result.ast_ctx.addError(error);
    }
  }
  
  parse_result.type = inferred_type;
  return parse_result;
}
```

#### 2. Store Type Information in Runtime

```cpp
struct RuntimeObject {
  RuntimeObjectType type;
  RuntimeObjectData data;
  optional<compiler::types::Type> static_type; // Add this
  
  // Constructor with type
  RuntimeObject(RuntimeObjectType t, RuntimeObjectData d, compiler::types::Type st)
    : type(t), data(std::move(d)), static_type(st) {}
};
```

#### 3. Runtime Type Checking

```cpp
// Add to Interpreter
RuntimeObjectPtr Interpreter::apply_with_type_check(
    RuntimeObjectPtr func,
    const vector<RuntimeObjectPtr>& args) const {
  
  if (func->static_type.has_value()) {
    // Check argument types match function signature
    auto func_type = func->static_type.value();
    // ... perform type checking ...
  }
  
  // Proceed with application
  return apply_function(func, args);
}
```

#### 4. Type-Directed Optimizations

```cpp
// Example: Optimize numeric operations based on types
any Interpreter::visit(AddExpr *node) const {
  auto left_type = type_of(node->left);
  auto right_type = type_of(node->right);
  
  if (is_integer(left_type) && is_integer(right_type)) {
    // Use integer addition
    return optimized_int_add(node->left, node->right);
  } else if (is_float(left_type) || is_float(right_type)) {
    // Use float addition
    return optimized_float_add(node->left, node->right);
  } else {
    // Fall back to generic addition
    return generic_add(node->left, node->right);
  }
}
```

## Implementation Roadmap

### Phase 1: Basic Currying (1-2 weeks)
1. ✅ Define `PartiallyAppliedFunction` structure
2. ✅ Implement partial application in `ApplyExpr`
3. ✅ Add tests for currying behavior
4. ✅ Update function type representation

### Phase 2: Automatic Currying (1 week)
1. ✅ Transform multi-parameter functions during parsing
2. ✅ Update pattern matching for curried functions
3. ✅ Ensure backward compatibility

### Phase 3: Type Checker Integration (2-3 weeks)
1. ✅ Add type checking phase to execution pipeline
2. ✅ Store type information in runtime values
3. ✅ Implement runtime type checking
4. ✅ Add comprehensive error reporting

### Phase 4: Optimizations (1-2 weeks)
1. ✅ Type-directed dispatch for operators
2. ✅ Monomorphization for generic functions
3. ✅ Inline small functions based on types

## Usage Examples

### With Currying

```yona
# Define a curried function
add : Int -> Int -> Int
add(x, y) -> x + y

# Partial application
let add5 = add(5) in
  [add5(1), add5(2), add5(3)]  # [6, 7, 8]

# Higher-order functions
map : (a -> b) -> [a] -> [b]
map(f, list) -> [f(x) for x in list]

# Partial application with map
let doubles = map(\\x -> x * 2) in
  doubles([1, 2, 3])  # [2, 4, 6]
```

### With Type Integration

```yona
# Type errors caught before runtime
let x : Int = "not an int"  # Type error: expected Int, got String

# Type-safe function application
add : Int -> Int -> Int
add("1", "2")  # Type error: expected Int, got String

# Type inference
let numbers = [1, 2, 3] in  # Inferred: [Int]
  map(toString, numbers)     # Inferred: [String]
```

## Testing Strategy

### Currying Tests

1. **Basic Currying**:
   - Single argument application
   - Multiple partial applications
   - Full application at once

2. **Edge Cases**:
   - Zero-argument functions
   - Variadic functions
   - Recursive curried functions

3. **Integration**:
   - Currying with pattern matching
   - Currying with type annotations
   - Currying in module exports

### Type Integration Tests

1. **Type Checking**:
   - Catch type errors before runtime
   - Verify type inference accuracy
   - Test polymorphic functions

2. **Runtime Behavior**:
   - Type information preserved
   - Runtime type checks work
   - Performance improvements

3. **Error Messages**:
   - Clear type error reporting
   - Source location accuracy
   - Helpful suggestions

## Performance Considerations

1. **Currying Overhead**:
   - Small runtime cost for partial application
   - Can be optimized away in many cases
   - JIT compilation can eliminate overhead

2. **Type Checking Cost**:
   - One-time cost at parse time
   - Runtime checks can be disabled in production
   - Type-directed optimizations offset the cost

3. **Memory Usage**:
   - Partial applications store captured arguments
   - Type information adds small overhead per value
   - Can be significant for large data structures

## Conclusion

Implementing currying and type system integration will:
- Make Yona more functional and composable
- Catch errors earlier with better messages
- Enable performance optimizations
- Improve developer experience

The implementation is straightforward but requires careful attention to maintain backward compatibility and performance.