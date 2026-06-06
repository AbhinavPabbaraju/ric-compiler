#include "ir/ir_builder.hpp"
#include <format>
#include <stdexcept>

namespace ric {

auto ir::BasicBlock::is_terminated() const noexcept -> bool {
    if (instructions.empty()) return false;
    return std::holds_alternative<ir::Branch>(instructions.back())
        || std::holds_alternative<ir::Jump>(instructions.back())
        || std::holds_alternative<ir::Return>(instructions.back());
}

auto ir::Function::alloc_temp(TypePtr type, std::string debug_name) -> TempId {
    return next_temp_id++;
}

auto ir::Function::alloc_label(std::string hint) -> LabelId {
    auto id    = next_label_id++;
    auto label = hint.empty() ? std::format("bb{}", id) : std::format("{}_{}", hint, id);
    return id;
}

auto ir::Function::current_block() -> BasicBlock& {
    return block(current_block_id_);
}

auto ir::Function::block(LabelId id) -> BasicBlock& {
    for (auto& b : blocks) if (b.id == id) return b;
    throw std::logic_error("block not found");
}

auto ir::Function::block(LabelId id) const -> const BasicBlock& {
    for (const auto& b : blocks) if (b.id == id) return b;
    throw std::logic_error("block not found");
}

auto ir::Function::add_block(std::string hint) -> LabelId {
    const auto id    = next_label_id++;
    const auto label = hint.empty() ? std::format("bb{}", id)
                                    : std::format("{}_{}", hint, id);
    blocks.push_back(BasicBlock{.id = id, .label = label});
    return id;
}

void ir::Function::emit(Instruction instr) {
    current_block().instructions.push_back(std::move(instr));
}

void ir::Function::seal_block(LabelId pred, LabelId succ) {
    block(pred).successors.push_back(succ);
    block(succ).predecessors.push_back(pred);
}

void ir::Function::set_current_block(LabelId id) {
    current_block_id_ = id;
}

auto ir::Module::find_function(const std::string& name) const -> const Function* {
    for (const auto& fn : functions)
        if (fn.name == name) return &fn;
    return nullptr;
}

auto ir::bin_op_symbol(BinOpKind op) noexcept -> std::string_view {
    switch (op) {
        case BinOpKind::Add: return "+";
        case BinOpKind::Sub: return "-";
        case BinOpKind::Mul: return "*";
        case BinOpKind::Div: return "/";
        case BinOpKind::Rem: return "%";
        case BinOpKind::Eq:  return "==";
        case BinOpKind::Ne:  return "!=";
        case BinOpKind::Lt:  return "<";
        case BinOpKind::Le:  return "<=";
        case BinOpKind::Gt:  return ">";
        case BinOpKind::Ge:  return ">=";
        case BinOpKind::And: return "&&";
        case BinOpKind::Or:  return "||";
    }
    return "?";
}

void ir::print_value(const Value& val, std::ostream& out) {
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, TempRef>)  out << std::format("%{}", v.id);
        else if constexpr (std::is_same_v<T, ImmI64>)  out << v.value;
        else if constexpr (std::is_same_v<T, ImmBool>) out << (v.value ? "true" : "false");
        else if constexpr (std::is_same_v<T, ImmChar>) out << std::format("'{}'", (char)v.value);
        else if constexpr (std::is_same_v<T, ImmUnit>) out << "()";
    }, val);
}

void ir::print_instruction(const Instruction& instr, std::ostream& out) {
    std::visit([&](const auto& i) {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, Assign>) {
            out << std::format("  %{} = ", i.dst);
            print_value(i.src, out);
            out << "\n";
        } else if constexpr (std::is_same_v<T, BinOp>) {
            out << std::format("  %{} = ", i.dst);
            print_value(i.lhs, out);
            out << std::format(" {} ", bin_op_symbol(i.op));
            print_value(i.rhs, out);
            out << "\n";
        } else if constexpr (std::is_same_v<T, UnOp>) {
            out << std::format("  %{} = {}", i.dst,
                i.op == UnOpKind::Neg ? "-" : "!");
            print_value(i.src, out);
            out << "\n";
        } else if constexpr (std::is_same_v<T, Call>) {
            if (i.dst) out << std::format("  %{} = ", *i.dst);
            else       out << "  ";
            out << std::format("call {}(", i.func);
            for (std::size_t a = 0; a < i.args.size(); ++a) {
                if (a > 0) out << ", ";
                print_value(i.args[a], out);
            }
            out << ")\n";
        } else if constexpr (std::is_same_v<T, Branch>) {
            out << "  br ";
            print_value(i.cond, out);
            out << std::format(", bb{}, bb{}\n", i.true_label, i.false_label);
        } else if constexpr (std::is_same_v<T, Jump>) {
            out << std::format("  jmp bb{}\n", i.target);
        } else if constexpr (std::is_same_v<T, Return>) {
            if (i.value) {
                out << "  ret ";
                print_value(*i.value, out);
                out << "\n";
            } else {
                out << "  ret\n";
            }
        } else if constexpr (std::is_same_v<T, LoadLocal>) {
            out << std::format("  %{} = load {}\n", i.dst, i.var_name);
        } else if constexpr (std::is_same_v<T, StoreLocal>) {
            out << std::format("  store {}, ", i.var_name);
            print_value(i.src, out);
            out << "\n";
        }
    }, instr);
}

void ir::Module::print(std::ostream& out) const {
    for (const auto& fn : functions) {
        out << std::format("fn {}(", fn.name);
        for (std::size_t i = 0; i < fn.params.size(); ++i) {
            if (i > 0) out << ", ";
            out << std::format("{}: {}", fn.params[i].first, fn.params[i].second->name());
        }
        out << std::format(") -> {} {{\n", fn.return_type->name());

        for (const auto& blk : fn.blocks) {
            out << std::format("bb{} ({}):\n", blk.id, blk.label);
            for (const auto& instr : blk.instructions)
                print_instruction(instr, out);
        }
        out << "}\n\n";
    }
}

IRBuilder::IRBuilder() = default;

auto IRBuilder::lower(Program& program) -> ir::Module {
    program.accept(*this);
    return std::move(module_);
}

void IRBuilder::emit(ir::Instruction instr) {
    current_fn_->emit(std::move(instr));
}

auto IRBuilder::alloc_temp(TypePtr type, std::string name) -> ir::TempId {
    return current_fn_->alloc_temp(std::move(type), std::move(name));
}

auto IRBuilder::alloc_label(std::string hint) -> ir::LabelId {
    return current_fn_->add_block(std::move(hint));
}

auto IRBuilder::current_block() -> ir::BasicBlock& {
    return current_fn_->current_block();
}

void IRBuilder::switch_to(ir::LabelId label) {
    current_fn_->set_current_block(label);
}

void IRBuilder::connect_blocks(ir::LabelId from, ir::LabelId to) {
    current_fn_->seal_block(from, to);
}

auto IRBuilder::take_value() -> ir::Value {
    return last_value_.value_or(ir::ImmUnit{});
}

void IRBuilder::set_value(ir::Value val) {
    last_value_ = std::move(val);
}

void IRBuilder::visit(Program& node) {
    module_.source_file = node.location().filename;
    for (auto& fn : node.declarations_mut())
        fn->accept(*this);
}

void IRBuilder::visit(FnDecl& node) {
    lower_fn(node);
}

void IRBuilder::lower_fn(FnDecl& fn) {
    module_.functions.emplace_back();
    current_fn_ = &module_.functions.back();
    current_fn_->name        = fn.name();
    current_fn_->return_type = fn.return_type();

    for (const auto& param : fn.params()) {
        current_fn_->params.emplace_back(param.name, param.type);
        current_fn_->locals.push_back({param.name, param.type, false});
    }

    const auto entry = current_fn_->add_block("entry");
    current_fn_->entry_block_id = entry;
    switch_to(entry);

    for (const auto& param : fn.params()) {
        const auto tmp = alloc_temp(param.type, param.name);
        emit(ir::LoadLocal{.dst = tmp, .var_name = param.name});
    }

    fn.body_mut().accept(*this);

    if (!current_block().is_terminated()) {
        if (fn.return_type()->is_unit()) {
            emit(ir::Return{.value = std::nullopt});
        } else {
            emit(ir::Return{.value = take_value()});
        }
    }

    current_fn_ = nullptr;
}

void IRBuilder::visit(BlockExpr& node) {
    for (auto& stmt : node.stmts_mut())
        stmt->accept(*this);

    if (node.tail_expr()) {
        (*node.tail_expr_mut())->accept(*this);
    } else {
        set_value(ir::ImmUnit{});
    }
}

void IRBuilder::visit(LetStmt& node) {
    node.initializer_mut().accept(*this);
    const auto val = take_value();

    const auto type = node.initializer().resolved_type
        ? node.initializer().resolved_type
        : types::unknown();

    current_fn_->locals.push_back({node.name(), type, node.is_mutable()});
    emit(ir::StoreLocal{.var_name = node.name(), .src = val});
}

void IRBuilder::visit(ExprStmt& node) {
    node.expr_mut().accept(*this);
    last_value_.reset();
}

void IRBuilder::visit(AssignExpr& node) {
    node.value_mut().accept(*this);
    const auto val = take_value();
    emit(ir::StoreLocal{.var_name = node.target(), .src = val});
    set_value(val);
}

void IRBuilder::visit(IdentExpr& node) {
    const auto type = node.resolved_type ? node.resolved_type : types::unknown();
    const auto tmp  = alloc_temp(type, node.name());
    emit(ir::LoadLocal{.dst = tmp, .var_name = node.name()});
    set_value(ir::TempRef{tmp});
}

void IRBuilder::visit(IntLiteralExpr& node) {
    set_value(ir::ImmI64{node.value()});
}

void IRBuilder::visit(BoolLiteralExpr& node) {
    set_value(ir::ImmBool{node.value()});
}

void IRBuilder::visit(CharLiteralExpr& node) {
    set_value(ir::ImmChar{node.value()});
}

void IRBuilder::visit(UnitLiteralExpr&) {
    set_value(ir::ImmUnit{});
}

void IRBuilder::visit(BinaryExpr& node) {
    node.lhs_mut().accept(*this);
    const auto lhs = take_value();

    node.rhs_mut().accept(*this);
    const auto rhs = take_value();

    const auto type = node.resolved_type ? node.resolved_type : types::unknown();

    ir::BinOpKind op;
    switch (node.op()) {
        case BinaryOp::Add:        op = ir::BinOpKind::Add; break;
        case BinaryOp::Sub:        op = ir::BinOpKind::Sub; break;
        case BinaryOp::Mul:        op = ir::BinOpKind::Mul; break;
        case BinaryOp::Div:        op = ir::BinOpKind::Div; break;
        case BinaryOp::Rem:        op = ir::BinOpKind::Rem; break;
        case BinaryOp::Eq:         op = ir::BinOpKind::Eq;  break;
        case BinaryOp::Ne:         op = ir::BinOpKind::Ne;  break;
        case BinaryOp::Lt:         op = ir::BinOpKind::Lt;  break;
        case BinaryOp::Le:         op = ir::BinOpKind::Le;  break;
        case BinaryOp::Gt:         op = ir::BinOpKind::Gt;  break;
        case BinaryOp::Ge:         op = ir::BinOpKind::Ge;  break;
        case BinaryOp::LogicalAnd: op = ir::BinOpKind::And; break;
        case BinaryOp::LogicalOr:  op = ir::BinOpKind::Or;  break;
    }

    const auto dst = alloc_temp(type);
    emit(ir::BinOp{.dst = dst, .op = op, .lhs = lhs, .rhs = rhs});
    set_value(ir::TempRef{dst});
}

void IRBuilder::visit(UnaryExpr& node) {
    node.operand_mut().accept(*this);
    const auto src  = take_value();
    const auto type = node.resolved_type ? node.resolved_type : types::unknown();

    const auto op  = node.op() == UnaryOp::Neg ? ir::UnOpKind::Neg : ir::UnOpKind::Not;
    const auto dst = alloc_temp(type);
    emit(ir::UnOp{.dst = dst, .op = op, .src = src});
    set_value(ir::TempRef{dst});
}

void IRBuilder::visit(CallExpr& node) {
    std::vector<ir::Value> args;
    for (auto& arg : node.args_mut()) {
        arg->accept(*this);
        args.push_back(take_value());
    }

    const auto type = node.resolved_type ? node.resolved_type : types::unknown();

    if (type->is_unit()) {
        emit(ir::Call{.dst = std::nullopt, .func = node.callee(), .args = std::move(args)});
        set_value(ir::ImmUnit{});
    } else {
        const auto dst = alloc_temp(type);
        emit(ir::Call{.dst = dst, .func = node.callee(), .args = std::move(args)});
        set_value(ir::TempRef{dst});
    }
}

void IRBuilder::visit(IfExpr& node) {
    node.condition_mut().accept(*this);
    const auto cond = take_value();

    const auto then_label  = alloc_label("if_then");
    const auto merge_label = alloc_label("if_merge");
    const auto type = node.resolved_type ? node.resolved_type : types::unit();

    ir::LabelId else_label = merge_label;
    if (node.else_branch()) {
        else_label = alloc_label("if_else");
    }

    const auto from_block = current_block().id;
    emit(ir::Branch{.cond = cond, .true_label = then_label, .false_label = else_label});
    connect_blocks(from_block, then_label);
    connect_blocks(from_block, else_label);

    ir::TempId result_slot = ir::k_invalid_temp;
    if (!type->is_unit() && !type->is_unknown()) {
        result_slot = current_fn_->alloc_temp(type, "if_result");
    }

    switch_to(then_label);
    node.then_branch_mut().accept(*this);
    const auto then_val   = take_value();
    const auto then_block = current_block().id;

    if (result_slot != ir::k_invalid_temp && !current_block().is_terminated()) {
        emit(ir::StoreLocal{.var_name = std::format("__if_result_{}", result_slot), .src = then_val});
    }
    if (!current_block().is_terminated()) {
        emit(ir::Jump{.target = merge_label});
        connect_blocks(then_block, merge_label);
    }

    if (node.else_branch()) {
        switch_to(else_label);
        (*node.else_branch_mut())->accept(*this);
        const auto else_val   = take_value();
        const auto else_block = current_block().id;

        if (result_slot != ir::k_invalid_temp && !current_block().is_terminated()) {
            emit(ir::StoreLocal{.var_name = std::format("__if_result_{}", result_slot), .src = else_val});
        }
        if (!current_block().is_terminated()) {
            emit(ir::Jump{.target = merge_label});
            connect_blocks(else_block, merge_label);
        }
    }

    switch_to(merge_label);

    if (result_slot != ir::k_invalid_temp) {
        const auto load_dst = alloc_temp(type);
        emit(ir::LoadLocal{.dst = load_dst,
                           .var_name = std::format("__if_result_{}", result_slot)});
        set_value(ir::TempRef{load_dst});
    } else {
        set_value(ir::ImmUnit{});
    }
}

void IRBuilder::visit(WhileExpr& node) {
    const auto header_label = alloc_label("while_header");
    const auto body_label   = alloc_label("while_body");
    const auto exit_label   = alloc_label("while_exit");

    const auto entry_block = current_block().id;
    emit(ir::Jump{.target = header_label});
    connect_blocks(entry_block, header_label);

    switch_to(header_label);
    node.condition_mut().accept(*this);
    const auto cond       = take_value();
    const auto hdr_block  = current_block().id;
    emit(ir::Branch{.cond = cond, .true_label = body_label, .false_label = exit_label});
    connect_blocks(hdr_block, body_label);
    connect_blocks(hdr_block, exit_label);

    loop_stack_.push_back({.header_label = header_label, .exit_label = exit_label});

    switch_to(body_label);
    node.body_mut().accept(*this);
    const auto body_block = current_block().id;
    if (!current_block().is_terminated()) {
        emit(ir::Jump{.target = header_label});
        connect_blocks(body_block, header_label);
    }

    loop_stack_.pop_back();
    switch_to(exit_label);
    set_value(ir::ImmUnit{});
}

void IRBuilder::visit(LoopExpr& node) {
    const auto start_label = alloc_label("loop_start");
    const auto exit_label  = alloc_label("loop_exit");

    const auto entry_block = current_block().id;
    emit(ir::Jump{.target = start_label});
    connect_blocks(entry_block, start_label);

    const auto type = node.resolved_type ? node.resolved_type : types::unit();
    ir::TempId break_slot = ir::k_invalid_temp;
    if (!type->is_unit() && !type->is_unknown()) {
        break_slot = current_fn_->alloc_temp(type, "loop_result");
    }

    loop_stack_.push_back({
        .header_label = start_label,
        .exit_label   = exit_label,
        .break_slot   = break_slot != ir::k_invalid_temp ? std::optional{break_slot} : std::nullopt,
    });

    switch_to(start_label);
    node.body_mut().accept(*this);
    const auto loop_body_block = current_block().id;
    if (!current_block().is_terminated()) {
        emit(ir::Jump{.target = start_label});
        connect_blocks(loop_body_block, start_label);
    }

    loop_stack_.pop_back();
    switch_to(exit_label);

    if (break_slot != ir::k_invalid_temp) {
        const auto dst = alloc_temp(type);
        emit(ir::LoadLocal{.dst = dst, .var_name = std::format("__loop_result_{}", break_slot)});
        set_value(ir::TempRef{dst});
    } else {
        set_value(ir::ImmUnit{});
    }
}

void IRBuilder::visit(BreakExpr& node) {
    if (loop_stack_.empty()) return;

    const auto& ctx = loop_stack_.back();

    if (node.value() && ctx.break_slot) {
        (*node.value_mut())->accept(*this);
        const auto val = take_value();
        emit(ir::StoreLocal{
            .var_name = std::format("__loop_result_{}", *ctx.break_slot),
            .src      = val,
        });
    }

    const auto from = current_block().id;
    emit(ir::Jump{.target = ctx.exit_label});
    connect_blocks(from, ctx.exit_label);
    set_value(ir::ImmUnit{});
}

void IRBuilder::visit(ContinueExpr&) {
    if (loop_stack_.empty()) return;
    const auto& ctx = loop_stack_.back();
    const auto from = current_block().id;
    emit(ir::Jump{.target = ctx.header_label});
    connect_blocks(from, ctx.header_label);
    set_value(ir::ImmUnit{});
}

void IRBuilder::visit(ReturnExpr& node) {
    std::optional<ir::Value> ret_val;
    if (node.value()) {
        (*node.value_mut())->accept(*this);
        ret_val = take_value();
    }
    emit(ir::Return{.value = ret_val});
    set_value(ir::ImmUnit{});
}

}
