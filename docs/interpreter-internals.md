# Yona Interpreter Internals

## Overview

The Yona interpreter is a tree-walking interpreter that directly executes the Abstract Syntax Tree (AST) produced by the parser. This document describes the internal architecture and implementation details.

## Architecture

### Core Components

1. **Interpreter Class** (`Interpreter.h/cpp`)
   - Implements the `AstVisitor` pattern
   - Maintains interpreter state
   - Handles runtime evaluation

2. **Runtime Objects** (`runtime.h`)
   - Represents values during execution
   - Supports multiple types: Int, Float, String, Bool, Function, Module, etc.
   - Uses `shared_ptr` for memory management

3. **Interpreter State** (`InterpreterState`)
   - Frame stack for lexical scoping
   - Module stack for module context
   - Exception handling state
   - Module cache for loaded modules

### Execution Model

#### Frame-Based Scoping

The interpreter uses a frame-based model for variable scoping:

```cpp
struct Frame {
  shared_ptr<Frame> parent;
  map<string, RuntimeObjectPtr> locals_;
  
  void write(const string& name, RuntimeObjectPtr value);
  RuntimeObjectPtr lookup(const string& name);
};
```

- Each scope (function, let expression, etc.) creates a new frame
- Frames are linked to parent frames for lexical scoping
- Variable lookup traverses the frame chain

#### Pattern Matching

Pattern matching is a core feature with support for:
- Identifier patterns: `x` 
- Tuple patterns: `(a, b, c)`
- Sequence patterns: `[head | tail]`
- Dictionary patterns: `{key: value, ...}`
- Record patterns: `Person{name: n, age: a}`
- Underscore (wildcard): `_`

Implementation:
```cpp
bool match_pattern(PatternNode *pattern, const RuntimeObjectPtr& value) {
  // Dispatch to specific pattern matchers
  if (auto tuple_pat = dynamic_cast<TuplePattern*>(pattern)) {
    return match_tuple_pattern(tuple_pat, value);
  }
  // ... other pattern types
}
```

#### Function Calls

Functions are first-class values represented by `FunctionValue`:

```cpp
struct FunctionValue {
  shared_ptr<FqnValue> fqn;
  function<RuntimeObjectPtr(const vector<RuntimeObjectPtr>&)> code;
};
```

Function application:
1. Evaluate function expression to get `FunctionValue`
2. Evaluate arguments
3. Match arguments against function patterns
4. Execute function body in new frame
5. Return result

### Module System Integration

#### Module Loading

1. **Path Resolution**: Convert FQN to file path
   ```
   Math/Basic -> Math/Basic.yona
   ```

2. **File Search**: Look in current directory and `YONA_PATH`

3. **Parsing**: Parse module file to AST

4. **Evaluation**: Visit ModuleExpr to create ModuleValue

5. **Caching**: Store in module cache for reuse

#### Import Mechanism

Import expressions work by:
1. Loading the target module
2. Extracting requested functions from exports
3. Adding them to current frame

```cpp
// Simplified import logic
auto module = get_or_load_module(fqn);
for (const auto& [name, func] : module->exports) {
  frame->write(name, make_shared<RuntimeObject>(Function, func));
}
```

### Exception Handling

The interpreter supports exception handling with:
- `raise` expressions to throw exceptions
- `try-catch` expressions to handle them
- Exception state propagation through the call stack

```cpp
struct InterpreterState {
  bool has_exception = false;
  RuntimeObjectPtr exception_value;
  SourceContext exception_context;
};
```

### Generator Support

Generators provide lazy evaluation for sequences:
- Sequence generators: `[x * 2 for x in list]`
- Set generators: `{x * 2 for x in list}`
- Dictionary generators: `{k: v * 2 for k, v in dict}`

Generator state is maintained in `InterpreterState`:
```cpp
RuntimeObjectPtr generator_current_element;
RuntimeObjectPtr generator_current_key;
```

## Visitor Pattern Implementation

Each AST node type has a corresponding visitor method:

```cpp
any visit(AddExpr *node) const override {
  auto left = any_cast<RuntimeObjectPtr>(visit(node->left));
  auto right = any_cast<RuntimeObjectPtr>(visit(node->right));
  
  if (left->type == Int && right->type == Int) {
    return make_shared<RuntimeObject>(Int, left->get<int>() + right->get<int>());
  }
  // ... handle other numeric types
}
```

### Expression Evaluation

1. **Literals**: Create RuntimeObject with appropriate type
2. **Binary Operations**: Evaluate operands, apply operation
3. **Function Calls**: Resolve function, evaluate arguments, apply
4. **Control Flow**: Conditional evaluation based on runtime values

### Type Checking

Runtime type checking ensures type safety:
- Operations verify operand types
- Pattern matching checks value types
- Type errors raise exceptions

## Performance Considerations

### Optimizations

1. **Module Caching**: Modules loaded once and reused
2. **Frame Pooling**: Potential optimization for frame allocation
3. **Direct Function Calls**: Function values store callable objects

### Limitations

As a tree-walking interpreter:
- No bytecode compilation
- Direct AST traversal overhead
- Limited optimization opportunities

## Debugging Support

### Source Location Tracking

All runtime objects maintain source location for error reporting:
```cpp
throw yona_error(node->source_context, yona_error::Type::TYPE, 
                 "Type mismatch: expected Int");
```

### Error Messages

Detailed error messages include:
- Error type (Type, Runtime, etc.)
- Source location (file:line:column)
- Descriptive message

## Future Improvements

1. **Bytecode VM**: Compile AST to bytecode for faster execution
2. **JIT Compilation**: Use LLVM JIT for hot code paths  
3. **Optimized Pattern Matching**: Decision trees for pattern dispatch
4. **Tail Call Optimization**: Proper tail recursion support
5. **Concurrent Execution**: Support for parallel evaluation