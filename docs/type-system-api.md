# Type System API Documentation

## Overview

The Yona type system provides static type checking with type inference based on the Hindley-Milner type system. This document describes the API for working with types in Yona.

## Type Representation

### Basic Types (`compiler::types`)

```cpp
enum BuiltinType {
    Bool,           // Boolean values: true, false
    Byte,           // 8-bit unsigned integer
    SignedInt16,    // 16-bit signed integer
    SignedInt32,    // 32-bit signed integer
    SignedInt64,    // 64-bit signed integer (default Int)
    SignedInt128,   // 128-bit signed integer
    UnsignedInt16,  // 16-bit unsigned integer
    UnsignedInt32,  // 32-bit unsigned integer
    UnsignedInt64,  // 64-bit unsigned integer
    UnsignedInt128, // 128-bit unsigned integer
    Float32,        // 32-bit floating point
    Float64,        // 64-bit floating point (default Float)
    Float128,       // 128-bit floating point
    Char,           // Unicode character
    String,         // Unicode string
    Symbol,         // Symbolic atom
    Dict,           // Dictionary type constructor
    Set,            // Set type constructor
    Seq,            // Sequence type constructor
    Var,            // Mutable variable marker
    Unit            // Unit type ()
};
```

### Complex Types

```cpp
// Function type: argument -> return
struct FunctionType {
    Type argumentType;
    Type returnType;
};

// Collection type: [T] or {T}
struct SingleItemCollectionType {
    enum CollectionKind { Set, Seq } kind;
    Type valueType;
};

// Dictionary type: {K: V}
struct DictCollectionType {
    Type keyType;
    Type valueType;
};

// Sum type: A | B | C
struct SumType {
    unordered_set<Type> types;
};

// Product type: (A, B, C)
struct ProductType {
    vector<Type> types;
};

// Named type: type alias or user-defined
struct NamedType {
    string name;
    Type type;  // nullptr for unresolved
};
```

## Type Checking API

### TypeChecker Class

```cpp
class TypeChecker : public AstVisitor {
public:
    // Create a type checker with optional initial environment
    TypeChecker(TypeInferenceContext& ctx, 
                shared_ptr<TypeEnvironment> initial_env = nullptr);
    
    // Type check an AST node
    Type check(AstNode* node);
    
    // Import type information from modules
    void import_module_types(const string& module_name,
                           const unordered_map<string, RecordTypeInfo>& records,
                           const unordered_map<string, Type>& exports);
};
```

### TypeInferenceContext

```cpp
class TypeInferenceContext {
public:
    // Generate a fresh type variable
    shared_ptr<TypeVar> fresh_type_var();
    
    // Add a type error
    void add_error(const SourceLocation& loc, const string& message);
    
    // Check if there are any errors
    bool has_errors() const;
    
    // Get all errors
    const vector<shared_ptr<yona_error>>& get_errors() const;
};
```

### TypeEnvironment

```cpp
class TypeEnvironment {
public:
    // Bind a variable to a type
    void bind(const string& name, const Type& type);
    
    // Look up a variable's type
    optional<Type> lookup(const string& name) const;
    
    // Create a new child environment
    shared_ptr<TypeEnvironment> extend();
};
```

## Type Operations

### Type Checking Utilities

```cpp
// Check if a type is numeric
bool is_numeric(const Type& type);

// Check if a type is an integer
bool is_integer(const Type& type);

// Check if a type is a float
bool is_float(const Type& type);

// Check if a type is signed
bool is_signed(const Type& type);

// Check if a type is unsigned
bool is_unsigned(const Type& type);

// Derive result type for binary operations
Type derive_bin_op_result_type(const Type& lhs, const Type& rhs);
```

### Type Unification

```cpp
struct UnificationResult {
    bool success;
    TypeSubstitution substitution;
    optional<string> error_message;
};

// Unify two types
UnificationResult unify(const Type& t1, const Type& t2);
```

## Usage Examples

### Basic Type Checking

```cpp
// Create a type checking context
TypeInferenceContext ctx;
TypeChecker checker(ctx);

// Parse some code
Parser parser;
auto ast = parser.parse_expression("let x = 42 in x + 1");

// Type check the AST
Type result_type = checker.check(ast.get());

// Check for errors
if (ctx.has_errors()) {
    for (const auto& error : ctx.get_errors()) {
        cerr << error->format() << endl;
    }
}
```

### Working with Types

```cpp
// Create a function type: Int -> String -> Bool
auto func_type = make_shared<FunctionType>();
func_type->argumentType = Type(SignedInt64);
func_type->returnType = Type(make_shared<FunctionType>(
    Type(String),  // argument
    Type(Bool)     // return
));

// Create a list type: [Int]
auto list_type = make_shared<SingleItemCollectionType>();
list_type->kind = SingleItemCollectionType::Seq;
list_type->valueType = Type(SignedInt64);

// Create a tuple type: (Int, String)
auto tuple_type = make_shared<ProductType>();
tuple_type->types.push_back(Type(SignedInt64));
tuple_type->types.push_back(Type(String));
```

### Type Annotations in Yona

```yona
# Function type annotation
add : Int -> Int -> Int
add(x, y) -> x + y

# Let binding with type
let x : String = "hello" in x

# Record with typed fields
record Person(name: String, age: Int)

# Type alias (future feature)
type UserId = Int
type Point2D = (Float, Float)
```

## Type Inference Rules

### Literals

- Integer literals → `SignedInt64`
- Float literals → `Float64`
- String literals → `String`
- Character literals → `Char`
- Boolean literals → `Bool`
- Unit literal `()` → `Unit`
- Symbol literals → `Symbol`

### Collections

- Empty list `[]` → `[a]` where `a` is fresh type variable
- Non-empty list → `[T]` where `T` is unified element type
- Empty set `{}` → `{a}` where `a` is fresh type variable
- Dictionary → `{K: V}` where `K` and `V` are unified types

### Expressions

- Arithmetic operators → numeric types with promotion
- Comparison operators → numeric arguments, `Bool` result
- Logical operators → `Bool` arguments and result
- If expression → condition must be `Bool`, branches unified
- Let expression → polymorphic binding with generalization

### Functions

- Function definition → infers type from body
- Function application → unifies argument with parameter
- Partial application → returns function type (when implemented)

## Error Messages

Type errors include:

1. **Type Mismatch**:
   ```
   Type error: expected Int, got String
   at line 10, column 5
   ```

2. **Undefined Variable**:
   ```
   Type error: undefined variable 'x'
   at line 5, column 10
   ```

3. **Wrong Number of Arguments**:
   ```
   Type error: function expects 2 arguments, got 1
   at line 15, column 3
   ```

4. **Incompatible Types**:
   ```
   Type error: cannot unify Int with String
   at line 20, column 8
   ```

## Extending the Type System

### Adding a New Built-in Type

1. Add to `BuiltinType` enum in `types.h`
2. Add string representation to `BuiltinTypeStrings`
3. Update type checking utilities if needed
4. Add parser support in `parse_primary_type()`

### Adding a New Type Constructor

1. Define new struct inheriting from base type
2. Add to `Type` variant in `types.h`
3. Update `TypeSubstitution::apply()`
4. Update `TypeChecker::unify()`
5. Add visitor methods if needed

### Custom Type Rules

Override visitor methods in `TypeChecker`:

```cpp
any TypeChecker::visit(CustomExpr* node) override {
    // Custom type checking logic
    Type result = /* infer type */;
    return result;
}
```

## Best Practices

1. **Let Polymorphism**: Use `let` for generic functions
2. **Type Annotations**: Add for documentation and verification
3. **Explicit Types**: Use when inference is ambiguous
4. **Error Handling**: Always check `TypeInferenceContext` for errors
5. **Module Types**: Export type information with modules

## Future Enhancements

- **Type Classes**: Ad-hoc polymorphism
- **Row Polymorphism**: Extensible records
- **Dependent Types**: Types depending on values
- **Linear Types**: Resource management
- **Effect Types**: Track side effects
- **Gradual Typing**: Mix static and dynamic