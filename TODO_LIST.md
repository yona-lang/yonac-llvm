# TODO List - Yona-LLVM

## Summary
- **Total TODOs**: 38 in source files
- **Unimplemented Interpreter Methods**: 18 (returning only expr_wrapper)
- **Critical Issues**: 1 (module import runtime)

## By File

### src/Interpreter.cpp (9 TODOs)
1. Line 296: Implement record pattern matching
2. Line 445: Implement zero-fill right shift operator (>>>)
3. Line 548: Implement type checking for function arguments
4. Line 1117: Store record type information in module->record_types
5. Line 1573: Use exception_context for better error reporting
6. Line 1578: Pass exception to catch block for pattern matching
7. Line 1842: Process exports list from ModuleExpr
8. Line 1886: Implement proper type unification/subtyping check
9. Line 1893: Implement proper type compatibility check

### src/Parser.cpp (8 TODOs) 
1. Line 775: Properly convert compiler::types::Type to AST TypeDefinition
2. Line 829: Parse the actual type expression after '='
3. Line 856: Create OR pattern node
4. Line 888: Update AST to support literal patterns
5. Line 1073: Fix to properly handle non-identifier functions
6. ~~Line 1322: BYTE token type doesn't exist in lexer~~ ✅ COMPLETED
7. ~~Line 1604: Implement actual cons operators :: and :>~~ ✅ COMPLETED
8. Line 1623: Fix FieldAccessExpr constructor call

### src/TypeChecker.cpp (14 TODOs)
1. Line 207: Implement proper instantiation of polymorphic types
2. Line 213: Implement proper generalization
3. Line 644: Extract bindings from pattern and add to environment
4. Line 646: Handle other alias types
5. Line 670: Implement function type inference
6. Line 697: Properly unify with function type
7. Line 756: Store full record type info
8. Line 767: Implement pattern type checking against scrutinee_type
9. Line 774: Unify result types from different clauses
10. Line 793: Implement pattern type inference
11. Line 815: Implement proper try-catch type checking
12. Line 847: Convert TypeDefinition to Type
13. Line 852: Get module name
14. Line 892: Type check pattern against expression type

### src/ast.cpp (4 TODOs)
1. Line 184: Character printing implementation
2. Line 1278: PatternExpr constructor
3. Line 1285: Implementation incomplete
4. Line 1326: Implementation incomplete

### src/Codegen.cpp
- Entire LLVM code generation is commented out/stubbed

## Priority Ranking

### Critical (Blocking Features)
1. Module import runtime binding
2. Record pattern matching
3. BYTE token type

### High (Core Language Features)
1. Cons operators (:: and :>)
2. OR patterns
3. Function argument type checking
4. FieldAccessExpr implementation

### Medium (Type System)
1. Type inference and unification
2. Pattern type checking
3. Record type information storage

### Low (Nice to Have)
1. Zero-fill right shift operator
2. Exception context improvements
3. LLVM code generation

## Unimplemented Interpreter Visitors
These all return `expr_wrapper(node)` with no actual implementation:

1. AliasCall
2. BodyWithGuards
3. FieldAccessExpr
4. FieldUpdateExpr
5. FqnAlias
6. FunctionAlias
7. ModuleAlias
8. ModuleCall
9. PackageNameExpr
10. RecordInstanceExpr
11. WithExpr
12. FunctionDeclaration
13. TypeDeclaration
14. TypeDefinition
15. TypeNode
16. TypeInstance
17. BuiltinTypeNode
18. UserDefinedTypeNode