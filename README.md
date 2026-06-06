# ricc — Rust-Inspired Compiler

A **modern optimizing compiler** for a Rust-inspired statically typed systems
language, featuring ownership analysis, three-address code IR, five optimization
passes, and native x86-64 code generation via NASM.

Built entirely in **C++23** with no external compiler-construction libraries.

---

## What it compiles

```rust
fn factorial(n: i32) -> i32 {
    if n <= 1 {
        1
    } else {
        n * factorial(n - 1)
    }
}

fn main() -> i32 {
    factorial(10)     // returns 3628800
}
```

---

## Features

### Language
- Primitive types: `i32`, `i64`, `bool`, `char`, `()`
- Immutable and mutable bindings (`let` / `let mut`)
- Arithmetic, comparison, and boolean operators
- `if` / `else` expressions (value-producing)
- `while`, `loop`, `break` (with value), `continue`
- Functions with parameters and return types
- Recursion and mutual recursion
- Block expressions with tail values
- Simplified ownership checker (move tracking + mutability enforcement)

### Compiler stages
| Stage | Implementation |
|-------|---------------|
| Lexer | Hand-written, O(n), tracks line/column/offset |
| Parser | Recursive descent with Pratt precedence climbing |
| Semantic analysis | Scoped symbol table, type inference, ownership |
| IR | Three-address code with CFG (SSA-ready) |
| Optimizer | Constant folding · Algebraic simplification · Strength reduction · DCE · CF simplification |
| Code generation | x86-64 NASM, System V ABI, stack-based allocation |

### Diagnostics
Error messages styled after `rustc`, with source snippets and caret underlining:

```
error[E0208]: type mismatch: expected `i32`, found `bool`
 --> main.ric:3:5
  |
3 |     if true { 1 } else { false }
  |                          ^^^^^
  = note: expected `i32`
  = note: found `bool`
```

---

## Build

**Requirements**
- CMake ≥ 3.25
- Clang 17+ or GCC 13+ (C++23 support required)
- NASM (for assembling output)
- GNU ld (for linking)
- Internet access on first CMake configure (fetches Catch2 for tests)

```bash
git clone https://github.com/yourname/ric-compiler
cd ric-compiler
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The compiler binary is at `build/compiler/ricc`.

---

## Usage

```bash
# Compile to executable
./ricc examples/factorial.ric -o factorial
./factorial; echo $?    # prints 3628800 (mod 256 due to exit code)

# Inspect the pipeline
./ricc examples/fibonacci.ric --emit-tokens   # token stream
./ricc examples/fibonacci.ric --emit-ir       # IR before optimization
./ricc examples/fibonacci.ric --emit-opt-ir   # IR after optimization
./ricc examples/fibonacci.ric --emit-asm      # NASM assembly

# Disable optimizations
./ricc examples/factorial.ric -O0 --emit-asm

# Compile to object only
./ricc examples/factorial.ric --no-link
```

---

## Run tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

Or run directly:

```bash
./build/tests/ricc_tests
./build/tests/ricc_tests --reporter compact   # Catch2 compact format
./build/tests/ricc_tests "[lexer]"            # filter by tag
```

---

## Project layout

```
ric-compiler/
├── compiler/
│   ├── include/
│   │   ├── diagnostics/    source_location.hpp  diagnostics.hpp
│   │   ├── lexer/          token.hpp  lexer.hpp
│   │   ├── ast/            types.hpp  ast.hpp  visitor.hpp
│   │   ├── parser/         parser.hpp
│   │   ├── semantic/       symbol_table.hpp  semantic_analyzer.hpp
│   │   ├── ir/             ir.hpp  ir_builder.hpp
│   │   ├── optimizer/      passes.hpp
│   │   ├── codegen/        codegen.hpp
│   │   └── driver/         driver.hpp
│   └── src/                (implementations)
├── tests/                  Catch2 unit tests
├── examples/               factorial · fibonacci · collatz
├── docs/
│   ├── Architecture.md
│   └── IRDesign.md
└── CMakeLists.txt
```

---

## Generated assembly (annotated)

For `fn add(a: i32, b: i32) -> i32 { a + b }`:

```nasm
global add
add:
    push rbp                    ; save frame pointer
    mov  rbp, rsp
    sub  rsp, 24                ; allocate frame (2 params + 1 temp, aligned)

    mov  qword [rbp - 8], rdi   ; store param a
    mov  qword [rbp - 16], rsi  ; store param b

.add_entry_0:
    mov  rax, qword [rbp - 8]   ; LoadLocal a → %0
    mov  qword [rbp - 24], rax
    mov  rax, qword [rbp - 16]  ; LoadLocal b → %1
    mov  qword [rbp - 32], rax  ; ... (frame grows as needed)
    mov  rax, qword [rbp - 24]  ; BinOp add
    mov  rcx, qword [rbp - 32]
    add  rax, rcx
    mov  qword [rbp - 40], rax
    mov  rax, qword [rbp - 40]  ; Return
    leave
    ret
```

---

## Roadmap

- [ ] `match` expressions and `enum` types
- [ ] `struct` definitions and field access
- [ ] Generic functions (monomorphization)
- [ ] Trait definitions and `impl` blocks
- [ ] Full borrow checker with lifetime regions
- [ ] SSA construction (dominance frontiers, Cytron renaming)
- [ ] Graph-coloring register allocation (Chaitin-Briggs)
- [ ] Dead branch elimination (requires SSA)
- [ ] Inline expansion pass
- [ ] LLVM IR backend (optional, replaces NASM)
- [ ] Windows x64 ABI support

---

## Design decisions

See [`docs/Architecture.md`](docs/Architecture.md) for a full discussion of every
major design decision — AST representation, IR structure, ownership model,
register allocation algorithm, and ABI compliance.

See [`docs/IRDesign.md`](docs/IRDesign.md) for the complete IR specification and
the SSA extension plan.
