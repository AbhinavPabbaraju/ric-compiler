# Architecture

## Overview

`ricc` is a multi-stage optimizing compiler for a statically typed, Rust-inspired systems language.
It is built entirely in C++23 with no external compiler-construction libraries.
The pipeline is structured as a classic separation between front end, middle end, and back end.

```
Source Text
    │
    ▼
 Lexer          → Vec<Token>
    │
    ▼
 Parser         → AST (Program)
    │
    ▼
 Semantic       → Type-annotated AST, diagnostics
 Analyzer
    │
    ▼
 IR Builder     → ir::Module (TAC + CFG)
    │
    ▼
 Pass Manager   → Optimized ir::Module
    │
    ▼
 Code Generator → NASM x86-64 text
    │
    ▼
 NASM + ld      → ELF executable
```

---

## Front End

### Lexer (`compiler/src/lexer/lexer.cpp`)

Single-pass character-level scanner.  Produces a flat `std::vector<Token>` in one
forward sweep over the source.  No backtracking; the only lookahead needed is one
character (`peek(1)`) for two-character operators (`==`, `!=`, `<=`, `>=`, `->`,
`&&`, `||`).

Line/column numbers are tracked incrementally on every `advance()` call.
Block comments support nesting depth (`/* /* */ */`), which is a known correctness
improvement over many reference compilers.

**Tradeoff**: tokens are heap-allocated strings. An arena-based lexer that stores
slices (string_views) into the source buffer would be faster but adds lifetime
complexity. For a compiler of this scope the allocation cost is negligible.

### Parser (`compiler/src/parser/parser.cpp`)

Recursive-descent with Pratt-style precedence climbing for expressions.  The
grammar is deliberately stratified into `or_expr → and_expr → eq_expr →
rel_expr → add_expr → mul_expr → unary_expr → primary_expr` so that precedence
is encoded in the call structure rather than a table.

**Error recovery**: on encountering an unexpected token the parser emits a
diagnostic and calls `synchronize_to_stmt_boundary()`, which skips tokens until
it finds a `;` or `}`.  This allows the parser to continue and report multiple
errors in one compilation.

**Design decision — AST ownership**: all AST nodes are heap-allocated and owned
by `unique_ptr`.  Parent nodes own their children.  The `Program` owns its
`FnDecl`s, each `FnDecl` owns its `BlockExpr`, and so on.  There are no raw
owning pointers anywhere in the tree.

### Type System (`compiler/include/ast/types.hpp`)

Primitive types are interned singletons (returned by static `get()` methods),
so type comparison is an `equals()` virtual call rather than a pointer compare.
`FunctionType` stores parameter and return types and is compared structurally.
`UnknownType` is a sentinel used during error recovery so that the analyzer can
continue after a type error without cascading false positives.

### Semantic Analyzer (`compiler/src/semantic/semantic_analyzer.cpp`)

Two phases:

1. **Signature collection**: walks the program once to register all function
   names in the global symbol table.  This enables mutual recursion without
   forward declarations.

2. **Full analysis**: walks the type-annotated AST via the visitor pattern,
   resolving identifiers, inferring and checking types, enforcing mutability,
   and running the simplified ownership checker.

**Symbol table**: a `std::vector<Scope>` where each `Scope` is an
`unordered_map<string, Symbol>`.  `lookup()` walks the vector from the top
(most recently pushed scope) backwards.  `push_scope` / `pop_scope` are called
around every `{}` block, every function body, and every loop.

**Simplified ownership checker**: all primitive types (`i32`, `i64`, `bool`,
`char`) are `Copy`, so they are never moved.  The checker tracks the `is_moved`
flag on `Symbol` structs; using a moved value is an error.  A full borrow
checker with regions and lifetimes is out of scope for this release but the
infrastructure (mutability flags, initialization tracking) is in place.

---

## Middle End

### Intermediate Representation (`compiler/include/ir/ir.hpp`)

Three-Address Code (TAC) organized as a Control Flow Graph (CFG).

```
ir::Module
  └─ ir::Function[]
       ├─ locals: (name, type, is_mutable)[]
       ├─ params: (name, type)[]
       └─ blocks: ir::BasicBlock[]
            ├─ id:           LabelId
            ├─ label:        string
            ├─ instructions: ir::Instruction[]
            ├─ predecessors: LabelId[]
            └─ successors:   LabelId[]
```

Instructions are a `std::variant` over:
```
Assign    | BinOp   | UnOp    | Call
Branch    | Jump    | Return
LoadLocal | StoreLocal
```

**Why variant over virtual dispatch?** The IR instruction set is *closed*; the
compiler defines it and users cannot extend it.  `std::visit` gives exhaustive
compile-time checking and zero vtable overhead.  Adding a new instruction
requires touching the variant definition, which is a deliberate forcing function
to audit all existing visitors.

**Named locals vs SSA temporaries**: the IR uses named stack slots (`LoadLocal` /
`StoreLocal`) for source-level variables and numeric `TempId`s for computed
values.  This is a middle ground between raw stack-machine bytecode and full SSA.
The design is intentionally SSA-ready: adding `Phi` nodes to the `BasicBlock`
structure and running a proper dominance-frontier algorithm would convert the
current IR to full SSA form without breaking any existing code.

### IR Builder (`compiler/src/ir/ir_builder.cpp`)

Visits the type-annotated AST and emits IR instructions.  Control flow is
handled by maintaining a "current block" pointer and creating new blocks at
branch points.  The loop stack (`std::vector<LoopContext>`) tracks the header
and exit labels for nested loops so that `break` and `continue` can jump to the
right target regardless of nesting depth.

### Optimization Passes (`compiler/src/optimizer/passes.cpp`)

The `PassManager` runs passes in a fixed order until no pass makes progress
(fixed-point iteration):

| Pass | What it does |
|------|-------------|
| `ConstantFoldingPass` | Evaluates constant sub-expressions at compile time.  Propagates values through the `const_map` within each basic block. |
| `AlgebraicSimplificationPass` | Eliminates identities: `x+0→x`, `x*1→x`, `x*0→0`, `x-x→0`, `true&&x→x`, etc. |
| `StrengthReductionPass` | Replaces `x * 2^k` with a shift (encoded as an add with a shift count in the current IR). |
| `DeadCodeEliminationPass` | Computes the set of live temporaries and removes instructions whose results are never used. |
| `ControlFlowSimplificationPass` | Rewrites branch targets that pass through unconditional-jump-only blocks, collapsing the CFG. |

All passes return `bool` indicating whether any change was made.  The pass
manager iterates until all passes return `false`.

---

## Back End

### Code Generator (`compiler/src/codegen/codegen.cpp`)

**Stack frame layout** (System V AMD64 ABI):

```
[rbp + 16] ...    ← incoming args > 6 (not used by current ABI support)
[rbp +  8]        ← return address (pushed by CALL)
[rbp +  0]        ← saved rbp  (pushed by prologue)
[rbp -  8]        ← first named local / parameter slot
[rbp - 16]        ← second named local
      ...
[rbp - N]         ← last temporary
[rsp]             ← end of frame
```

Frame size is computed as the sum of all variable and temporary slots (8 bytes
each), then adjusted to satisfy `(8 + frame_size) % 16 == 0` — the System V
requirement that `rsp` must be 16-byte aligned *before* a `CALL` instruction.

**Value emission strategy**: all values are materialized through `rax`.  The
right-hand side of binary operations is loaded into `rcx`.  This "register pair"
model is simpler than a full register allocator and produces correct (if
suboptimal) code.  The `LinearScanAllocator` class implements the full linear
scan algorithm and can be wired into the pipeline as a drop-in replacement.

**Calling convention**: the first six integer/pointer arguments are passed in
`rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`.  Return value is in `rax`.  Functions
with more than six parameters are not currently supported; the ABI extension
(stack-passed arguments) is straightforward to add.

**Division and remainder**: `idiv rcx` divides `rdx:rax` by `rcx`.  The
`cqo` instruction sign-extends `rax` into `rdx:rax` before every division,
matching the semantics of signed Euclidean division.

### Register Allocator (`compiler/include/codegen/codegen.hpp`)

`LinearScanAllocator` implements the classic Poletto & Sarkar (1999) algorithm:

1. Sort live intervals by start point.
2. For each interval, expire all active intervals whose end point is before
   the current start.
3. If a free register is available, assign it; otherwise spill the interval
   with the latest end point (spill to a stack slot at `[rbp - N]`).

The allocatable set is `{rbx, r12, r13, r14, r15, r10, r11}` — all
callee-saved registers come first so that functions with few live values avoid
save/restore overhead.

---

## Diagnostics

Diagnostics are structured objects (`Diagnostic`) collected by
`DiagnosticEngine`.  All compiler stages receive a reference to the engine and
call `emit_error` / `emit_warning` rather than printing directly.  The engine
renders diagnostics to any `std::ostream` with ANSI color, source snippets, and
caret underlining, styled after `rustc`'s output.

---

## Extension Points

| Feature | Where to extend |
|---------|----------------|
| Structs / Enums | Add `StructDecl` / `EnumDecl` to `ast.hpp`; add `StructType` / `EnumType` to `types.hpp` |
| Pattern matching | Add `MatchExpr` to `ast.hpp`; extend semantic and codegen |
| Generics | Add `TypeParam` to `FnDecl`; add a monomorphization pass between semantic analysis and IR lowering |
| Borrow checking | Add region annotations to `TypePtr`; add a `BorrowCheckPass` after semantic analysis |
| Full SSA | Add `Phi` variant to `ir::Instruction`; run dominance-frontier computation in IR builder |
| Graph-coloring allocator | Replace `LinearScanAllocator` with interference-graph construction + Chaitin-Briggs coloring |
| LLVM backend | Replace `CodeGenerator` with an LLVM IR emitter; retain all front-end and middle-end code |
