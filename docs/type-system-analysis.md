# Type System Analysis and Implementation Plan

## Current State Analysis

### 1. Type System Infrastructure (types.h)

The type system is well-defined with:

- **Built-in Types**: Bool, Byte, various integers (signed/unsigned 16/32/64/128-bit), floats (32/64/128-bit), Char, String, Symbol, Dict, Set, Seq, Var, Unit
- **Complex Types**:
  - `SingleItemCollectionType`: For Set and Seq with element type
  - `DictCollectionType`: For dictionaries with key and value types
  - `FunctionType`: Return type and argument type
  - `SumType`: Union of multiple types
  - `ProductType`: Tuple/product of types in order
  - `NamedType`: Type aliases
- **Type Utilities**: Functions for checking numeric types, deriving binary operation result types

### 2. AST Type Support

The AST has comprehensive type node definitions:
- `TypeDefinition`: Base type definition with name and type parameters
- `TypeNameNode`, `BuiltinTypeNode`, `UserDefinedTypeNode`: Type name representations
- `TypeDeclaration`: Type declaration with optional type variables
- `TypeNode`: Combines declaration and definitions
- `TypeInstance`: Type instantiation with expressions

Key integration points:
- `RecordNode`: Contains field type definitions
- `FunctionDeclaration`: Contains type annotations
- Every AST node has `infer_type(AstContext &ctx)` method for type inference

### 3. Parser State

Type parsing is largely **unimplemented**:
- `parse_type()` and `parse_type_definition()` are TODO stubs
- Type annotations in functions and let bindings are recognized but skipped
- Record parsing is incomplete
- Lexer has TYPE and RECORD tokens defined

### 4. Runtime Representation

Records are currently represented as `TupleValue` at runtime:
- No distinction between tuples and records in runtime
- No type information retained at runtime
- Module system stores `vector<shared_ptr<TupleValue>> records`

### 5. Missing Components

1. **Type Parser Implementation**
2. **Type Checker/Inference Engine**
3. **Record Type System**:
   - Record type definitions in modules
   - Record instantiation with type checking
   - Field access type checking
4. **Function Type Annotations**
5. **Module Type Exports**
6. **Type Error Reporting**

## Implementation Plan

### Phase 1: Type Parsing (Priority: High)

1. **Implement `parse_type()` method**:
   ```cpp
   // Parse type expressions like:
   // Int
   // String -> Int
   // [Int]
   // {String: Bool}
   // Person | Animal
   // (Int, String)
   ```

2. **Implement `parse_type_definition()` method**:
   ```cpp
   // Parse type definitions like:
   // type Maybe a = Just a | Nothing
   // type Tree a = Leaf | Node a (Tree a) (Tree a)
   ```

3. **Parse function type annotations**:
   ```cpp
   // add: Int -> Int -> Int
   // add(x, y) -> x + y
   ```

4. **Parse record definitions**:
   ```cpp
   // record Person(name: String, age: Int)
   ```

### Phase 2: Record Implementation (Priority: High)

1. **Enhance Record Runtime Representation**:
   - Create `RecordValue` struct with field names and types
   - Distinguish from `TupleValue`
   - Store record metadata (field names, types)

2. **Record Creation and Field Access**:
   - Parse record instantiation: `Person{name: "Alice", age: 30}`
   - Type-check field assignments
   - Implement field access with type checking

3. **Record Pattern Matching**:
   - Complete record pattern parsing
   - Type-check patterns against record types

### Phase 3: Type Inference Engine (Priority: High)

1. **Create TypeInference Visitor**:
   - Implement Hindley-Milner style type inference
   - Handle let-polymorphism
   - Support recursive types

2. **Implement `infer_type()` for all AST nodes**:
   - Literals: Return their concrete type
   - Variables: Look up in type environment
   - Function calls: Unify argument and parameter types
   - Binary operations: Use `derive_bin_op_result_type()`

3. **Type Environment Management**:
   - Track variable types in scopes
   - Handle type variables and generics
   - Support let-polymorphism

### Phase 4: Type Checking Visitor (Priority: High)

1. **Create TypeChecker Visitor**:
   - Traverse AST after parsing
   - Verify type consistency
   - Report type errors with locations

2. **Type Checking Rules**:
   - Function application type checking
   - Pattern matching exhaustiveness
   - Record field type checking
   - Module export type checking

### Phase 5: Module System Integration (Priority: Medium)

1. **Module Type Exports**:
   - Export type definitions from modules
   - Import types from other modules
   - Type-check cross-module function calls

2. **Module Interface Files**:
   - Generate .yonai interface files
   - Store type signatures for exported functions
   - Enable separate compilation

### Phase 6: Error Reporting (Priority: Medium)

1. **Type Error Messages**:
   - Clear error messages with expected vs actual types
   - Source location information
   - Suggestions for common mistakes

2. **Type Error Recovery**:
   - Continue type checking after errors
   - Report multiple errors in one pass

## Technical Considerations

### 1. Type Representation

- Use the existing `Type` variant from types.h
- Extend with record types and type variables
- Consider adding location information to types for error reporting

### 2. Type Variables

- Implement fresh type variable generation
- Support type variable constraints
- Handle polymorphic recursion carefully

### 3. Performance

- Cache type information to avoid repeated inference
- Consider lazy type checking for large modules
- Optimize type unification algorithm

### 4. Compatibility

- Ensure interpreted code can gradually adopt types
- Support mixed typed/untyped modules
- Plan for future LLVM code generation needs

## Next Steps

1. Start with Phase 1: Implement type parsing
2. Create comprehensive test suite for type parser
3. Move to Phase 2: Complete record implementation
4. Phases 3-4 can be developed in parallel
5. Integrate with module system
6. Add comprehensive error reporting

## Estimated Timeline

- Phase 1 (Type Parsing): 2-3 days
- Phase 2 (Records): 2-3 days
- Phase 3 (Type Inference): 4-5 days
- Phase 4 (Type Checking): 3-4 days
- Phase 5 (Module Integration): 2-3 days
- Phase 6 (Error Reporting): 2 days

Total: ~3 weeks for full implementation
