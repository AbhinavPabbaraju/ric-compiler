# IR Design

## Philosophy

The `ricc` IR sits between a naive AST-to-assembly lowering and a full
production IR like LLVM.  It is designed to be:

- **Simple enough** to implement completely in one pass from the AST
- **Structured enough** to support multiple optimization passes
- **Extensible** toward full SSA form without breaking existing code

---

## Structure

```
Module
  source_file: string
  functions: Function[]

Function
  name: string
  params: (string, TypePtr)[]
  return_type: TypePtr
  locals: Local[]
  blocks: BasicBlock[]
  entry_block_id: LabelId

BasicBlock
  id: LabelId
  label: string            -- human-readable hint
  instructions: Instruction[]
  predecessors: LabelId[]
  successors: LabelId[]

Local
  name: string
  type: TypePtr
  is_mutable: bool
```

---

## Value Grammar

```
Value ::=
  | TempRef(TempId)         -- reference to a computed temporary
  | ImmI64(int64)           -- integer constant (i32 and i64 both use this)
  | ImmBool(bool)           -- boolean constant
  | ImmChar(char32_t)       -- character constant
  | ImmUnit                 -- unit value ()
```

Temporaries are identified by a `uint32_t` ID that is unique within a function.
They are write-once: each `TempId` appears as `dst` in exactly one instruction.

---

## Instruction Reference

### Data Movement

| Instruction | Semantics |
|-------------|-----------|
| `Assign { dst, src }` | `dst = src` |
| `LoadLocal { dst, var_name }` | `dst = locals[var_name]` |
| `StoreLocal { var_name, src }` | `locals[var_name] = src` |

### Arithmetic & Logic

| Instruction | Semantics |
|-------------|-----------|
| `BinOp { dst, op, lhs, rhs }` | `dst = lhs op rhs` |
| `UnOp { dst, op, src }` | `dst = op src` |

BinOp kinds: `Add Sub Mul Div Rem Eq Ne Lt Le Gt Ge And Or`  
UnOp kinds: `Neg Not`

### Control Flow

| Instruction | Semantics |
|-------------|-----------|
| `Branch { cond, true_label, false_label }` | `if cond goto true else false` |
| `Jump { target }` | `goto target` |
| `Return { value? }` | return from function |

### Calls

| Instruction | Semantics |
|-------------|-----------|
| `Call { dst?, func, args }` | `[dst =] func(args…)` |

---

## Named Locals vs. Temporaries

The IR distinguishes two kinds of storage:

**Named locals** are source-level variables (`let x = …`) and function
parameters.  They are allocated with `StoreLocal` and read with `LoadLocal`.
They are addressable by name across basic blocks.

**Temporaries** (`TempId`) are the result of computations: arithmetic,
comparisons, calls.  Each temporary is defined exactly once (write-once
semantics) but may be read many times.

This design avoids the need for SSA `Phi` nodes for simple programs while
keeping the representation honest enough that adding `Phi` nodes later
(for full SSA) is an additive change.

---

## Control Flow Patterns

### `if expr { A } else { B }`

```
   current_block:
     %cond = ...
     br %cond, if_then, if_else

   if_then:
     ... (result stored to __if_result_N)
     jmp if_merge

   if_else:
     ... (result stored to __if_result_N)
     jmp if_merge

   if_merge:
     %result = load __if_result_N
```

### `while cond { body }`

```
   entry → while_header
   while_header:
     %cond = ...
     br %cond, while_body, while_exit
   while_body:
     ...
     jmp while_header
   while_exit:
     ... (continue here)
```

### `loop { body }`

```
   entry → loop_start
   loop_start:
     ... (break stores to __loop_result_N)
     jmp loop_start
   loop_exit:
     %result = load __loop_result_N
```

---

## IR Text Format

Printed by `ir::Module::print(ostream&)`:

```
fn factorial(n: i32) -> i32 {
bb0 (entry_0):
  %0 = load n
  %1 = load n
  %2 = %1 <= 1
  br %2, bb1, bb2

bb1 (if_then_1):
  store __if_result_3, 1
  jmp bb3

bb2 (if_else_2):
  %4 = load n
  %5 = load n
  %6 = %5 - 1
  %7 = call factorial(%6)
  %8 = %4 * %7
  store __if_result_3, %8
  jmp bb3

bb3 (if_merge_3):
  %9 = load __if_result_3
  ret %9
}
```

---

## Future: SSA Extension

To convert the current IR to SSA form:

1. Add `Phi { dst: TempId, operands: Vec<(Value, LabelId)> }` to the
   `Instruction` variant.
2. Compute the dominance tree and dominance frontiers of the CFG.
3. Insert `Phi` nodes at dominance frontiers for each variable that has
   multiple definitions.
4. Rename variables using the standard Cytron et al. algorithm.
5. Update all optimization passes to handle `Phi` nodes.

The CFG edges (`predecessors` / `successors`) are already populated by the IR
builder, so step 2 can begin immediately.
