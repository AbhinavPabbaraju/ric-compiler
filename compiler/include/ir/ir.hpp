#pragma once

#include "ast/types.hpp"
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace ric::ir {

using TempId  = std::uint32_t;
using LabelId = std::uint32_t;

static constexpr TempId  k_invalid_temp  = ~TempId{0};
static constexpr LabelId k_invalid_label = ~LabelId{0};

struct TempRef   { TempId  id; };
struct ImmI64    { std::int64_t value; };
struct ImmBool   { bool value; };
struct ImmChar   { char32_t value; };
struct ImmUnit   {};

using Value = std::variant<TempRef, ImmI64, ImmBool, ImmChar, ImmUnit>;

enum class BinOpKind { Add, Sub, Mul, Div, Rem, Eq, Ne, Lt, Le, Gt, Ge, And, Or };
enum class UnOpKind  { Neg, Not };

struct Assign    { TempId dst; Value src; };
struct BinOp     { TempId dst; BinOpKind op; Value lhs; Value rhs; };
struct UnOp      { TempId dst; UnOpKind  op; Value src; };
struct Call      { std::optional<TempId> dst; std::string func; std::vector<Value> args; };
struct Branch    { Value cond; LabelId true_label; LabelId false_label; };
struct Jump      { LabelId target; };
struct Return    { std::optional<Value> value; };
struct LoadLocal { TempId dst; std::string var_name; };
struct StoreLocal{ std::string var_name; Value src; };

using Instruction = std::variant<
    Assign, BinOp, UnOp, Call,
    Branch, Jump, Return,
    LoadLocal, StoreLocal
>;

struct Temp {
    TempId      id;
    TypePtr     type;
    std::string debug_name;
};

struct BasicBlock {
    LabelId                    id;
    std::string                label;
    std::vector<Instruction>   instructions;
    std::vector<LabelId>       predecessors;
    std::vector<LabelId>       successors;

    [[nodiscard]] auto is_terminated() const noexcept -> bool;
};

struct Local {
    std::string name;
    TypePtr     type;
    bool        is_mutable;
};

struct Function {
    std::string                              name;
    std::vector<std::pair<std::string, TypePtr>> params;
    TypePtr                                  return_type;
    std::vector<Local>                       locals;
    std::vector<BasicBlock>                  blocks;
    LabelId                                  entry_block_id{0};
    TempId                                   next_temp_id{0};
    LabelId                                  next_label_id{0};

    [[nodiscard]] auto alloc_temp(TypePtr type, std::string debug_name = "") -> TempId;
    [[nodiscard]] auto alloc_label(std::string label_hint) -> LabelId;
    [[nodiscard]] auto current_block() -> BasicBlock&;
    [[nodiscard]] auto block(LabelId id) -> BasicBlock&;
    [[nodiscard]] auto block(LabelId id) const -> const BasicBlock&;
    auto add_block(std::string label_hint) -> LabelId;
    void emit(Instruction instr);
    void seal_block(LabelId pred, LabelId succ);
    void set_current_block(LabelId id);

private:
    LabelId current_block_id_{0};
};

struct Module {
    std::string            source_file;
    std::vector<Function>  functions;

    [[nodiscard]] auto find_function(const std::string& name) const -> const Function*;
    void print(std::ostream& out) const;
};

void print_value(const Value& val, std::ostream& out);
void print_instruction(const Instruction& instr, std::ostream& out);
[[nodiscard]] auto bin_op_symbol(BinOpKind op) noexcept -> std::string_view;

}
