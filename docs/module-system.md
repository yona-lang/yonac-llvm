# Yona Module System Documentation

## Overview

The Yona module system provides a way to organize code into reusable components. Modules are first-class values in Yona, allowing them to be imported, passed as arguments, and manipulated like any other value.

## Module Definition

Modules are defined using the `module` keyword followed by a fully-qualified name (FQN), export list, and module body:

```yona
module Package\ModuleName exports function1, function2 as
  # Record definitions
  record Person(name: String, age: Int)
  
  # Function definitions  
  function1(x) -> x + 1
  function2(a, b) -> a * b
  
  # Private functions (not exported)
  helper(x) -> x * 2
end
```

### Module Structure

- **Module Name**: Uses backslashes to separate package components (e.g., `Data\List`, `Utils\String`)
- **Exports**: Explicitly lists which functions are available to importers
- **Body**: Contains record definitions, type definitions, and function implementations

## Import Expressions

The `import` expression brings functions from other modules into scope:

```yona
import function1, function2 from Package\ModuleName in
  # Use imported functions
  function1(10) + function2(5, 6)
```

### Import Syntax Options

1. **Import specific functions**:
   ```yona
   import add, multiply from Math\Basic in
     add(1, 2)
   ```

2. **Import with aliases**:
   ```yona
   import 
     add as plus,
     multiply as mult
   from Math\Basic in
     plus(1, 2) + mult(3, 4)
   ```

3. **Import entire module** (imports all exported functions):
   ```yona
   import Math\Basic in
     # All exported functions are available
     add(1, 2)
   ```

4. **Import module with alias**:
   ```yona
   import Math\Basic as M in
     # Functions accessed via module alias
     M.add(1, 2)
   ```

5. **Multiple import clauses**:
   ```yona
   import
     add from Math\Basic,
     concat from Data\String
   in
     concat(toString(add(1, 2)), "!")
   ```

## Module Resolution

### Search Paths

Modules are resolved using the `YONA_PATH` environment variable, which contains a colon-separated list of directories (semicolon-separated on Windows):

```bash
export YONA_PATH=/usr/local/lib/yona:/home/user/mylibs
```

The module loader searches for modules in the following order:
1. Current directory
2. Directories listed in `YONA_PATH`

### File Naming Convention

Module names map to file paths by:
1. Replacing backslashes with directory separators
2. Appending `.yona` extension

Examples:
- `Math\Basic` → `Math\Basic.yona`
- `Data\Collections\List` → `Data\Collections\List.yona`

## Implementation Details

### Module Caching

Modules are cached after first load to improve performance:
- Each module is loaded and evaluated only once per interpreter session
- Subsequent imports reuse the cached module
- Cache key is the fully-qualified module name

### Export Mechanism

Only functions listed in the `exports` clause are accessible to importers:
- Exported functions are stored in the module's export table
- Non-exported functions remain private to the module
- Attempting to import a non-exported function raises a runtime error

### Runtime Representation

Modules are represented as runtime values with:
- **FQN**: Fully-qualified name
- **Exports**: Map of exported function names to function values
- **Records**: List of record types defined in the module
- **Source Path**: File path for debugging

## Best Practices

1. **Module Organization**:
   - Group related functionality into modules
   - Use meaningful package hierarchies (e.g., `Data\`, `Utils\`, `IO\`)
   - Keep modules focused on a single responsibility

2. **Export Design**:
   - Export only the public API
   - Use helper functions internally without exporting
   - Consider providing a clean, minimal interface

3. **Import Style**:
   - Import only what you need
   - Use aliases to avoid naming conflicts
   - Group related imports together

## Examples

### Basic Math Module

```yona
# File: Math/Basic.yona
module Math\Basic exports add, subtract, multiply, divide as
  add(a, b) -> a + b
  subtract(a, b) -> a - b
  multiply(a, b) -> a * b
  divide(a, b) -> if b != 0 then a / b else raise :division_by_zero
end
```

### Using the Math Module

```yona
# Import specific functions
import add, multiply from Math\Basic in
  let result = add(10, multiply(5, 6)) in
    result  # Returns 40

# Import with aliases
import 
  add as plus,
  subtract as minus
from Math\Basic in
  plus(10, minus(20, 5))  # Returns 25
```

### Data Structure Module

```yona
# File: Data/Stack.yona
module Data\Stack exports empty, push, pop, peek, isEmpty as
  # Stack is represented as a list
  
  empty() -> []
  
  push(stack, item) -> [item | stack]
  
  pop(stack) -> 
    case stack of
      [] -> raise :empty_stack
      [head | tail] -> (head, tail)  # Returns (item, new_stack)
    end
  
  peek(stack) ->
    case stack of
      [] -> raise :empty_stack
      [head | _] -> head
    end
  
  isEmpty(stack) -> stack == []
end
```

## Error Handling

Common module-related errors:

1. **Module not found**: Module file doesn't exist in any search path
   ```
   Runtime error: Module not found: Math\Advanced.yona
   ```

2. **Function not exported**: Attempting to import a private function
   ```
   Runtime error: Function 'helper' not exported from module
   ```

3. **Parse error in module**: Module file contains syntax errors
   ```
   Syntax error at Math\Basic.yona:5:10: Expected '->' after function parameters
   ```

## Future Enhancements

The following features are planned for future releases:

1. **Module Compilation**: Compile modules to LLVM IR for improved performance
2. **Foreign Function Interface (FFI)**: Import functions from C/C++ libraries
3. **Module Versioning**: Support for multiple versions of the same module
4. **Hot Reloading**: Reload modules during development without restarting
5. **Package Manager**: Automated module installation and dependency management