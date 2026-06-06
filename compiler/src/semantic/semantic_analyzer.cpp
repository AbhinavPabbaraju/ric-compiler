#include "semantic/semantic_analyzer.hpp"
#include <format>

namespace ric {

SemanticAnalyzer::SemanticAnalyzer(DiagnosticEngine& diags)
    : diags_(diags) {}

auto SemanticAnalyzer::analyze(Program& program) -> bool {
    program.accept(*this);
    return !diags_.has_errors();
}

void SemanticAnalyzer::type_error(const std::string& code,
                                   const std::string& expected,
                                   const std::string& found,
                                   SourceRange range) {
    auto& diag = diags_.emit_error(code,
        std::format("type mismatch: expected `{}`, found `{}`", expected, found), range);
    diags_.add_note(diag, std::format("expected `{}`", expected));
    diags_.add_note(diag, std::format("found `{}`", found));
}

void SemanticAnalyzer::collect_fn_signatures(Program& program) {
    for (const auto& fn : program.declarations()) {
        std::vector<TypePtr> param_types;
        for (const auto& p : fn->params())
            param_types.push_back(p.type);

        auto fn_type = types::make_fn(std::move(param_types), fn->return_type());
        Symbol sym{
            .name                 = fn->name(),
            .type                 = std::move(fn_type),
            .kind                 = SymbolKind::Function,
            .is_mutable           = false,
            .is_initialized       = true,
            .declaration_location = fn->location(),
        };

        if (!symbols_.declare(std::move(sym))) {
            diags_.emit_error("E0200",
                std::format("function `{}` is defined multiple times", fn->name()),
                SourceRange::at(fn->location()));
        }
    }
}

void SemanticAnalyzer::visit(Program& node) {
    symbols_.push_scope(false);
    collect_fn_signatures(node);
    for (auto& fn : node.declarations_mut())
        fn->accept(*this);
    symbols_.pop_scope();
}

void SemanticAnalyzer::visit(FnDecl& node) {
    check_fn_decl(node);
}

void SemanticAnalyzer::check_fn_decl(FnDecl& fn) {
    const auto saved_return_type = current_return_type_;
    current_return_type_ = fn.return_type();

    symbols_.push_scope(true);

    for (const auto& param : fn.params()) {
        Symbol sym{
            .name                 = param.name,
            .type                 = param.type,
            .kind                 = SymbolKind::Parameter,
            .is_mutable           = false,
            .is_initialized       = true,
            .declaration_location = param.location,
        };
        if (!symbols_.declare(std::move(sym))) {
            diags_.emit_error("E0201",
                std::format("duplicate parameter name `{}`", param.name),
                SourceRange::at(param.location));
        }
    }

    fn.body_mut().accept(*this);

    if (!fn.return_type()->is_unit() && !fn.return_type()->is_unknown()) {
        const auto& body_type = last_expr_type_;
        if (body_type && !body_type->is_unknown()
            && !types::compatible(body_type, fn.return_type())) {
            type_error("E0202",
                fn.return_type()->name(),
                body_type->name(),
                fn.body().range());
        }
    }

    symbols_.pop_scope();
    current_return_type_ = saved_return_type;
}

void SemanticAnalyzer::visit(BlockExpr& node) {
    symbols_.push_scope();

    for (auto& stmt : node.stmts_mut())
        stmt->accept(*this);

    if (node.tail_expr()) {
        (*node.tail_expr())->accept(*this);
        node.resolved_type = last_expr_type_;
    } else {
        node.resolved_type = types::unit();
    }

    last_expr_type_ = node.resolved_type;
    symbols_.pop_scope();
}

void SemanticAnalyzer::visit(LetStmt& node) {
    node.initializer_mut().accept(*this);
    const auto init_type = last_expr_type_;

    TypePtr var_type;
    if (node.annotation()) {
        var_type = *node.annotation();
        if (!init_type->is_unknown() && !types::compatible(init_type, var_type)) {
            type_error("E0203",
                var_type->name(),
                init_type->name(),
                node.initializer().range());
        }
    } else {
        var_type = init_type;
    }

    Symbol sym{
        .name                 = node.name(),
        .type                 = var_type,
        .kind                 = SymbolKind::Variable,
        .is_mutable           = node.is_mutable(),
        .is_initialized       = true,
        .declaration_location = node.location(),
    };

    if (!symbols_.declare(std::move(sym))) {
        diags_.emit_error("E0204",
            std::format("`{}` is already declared in this scope", node.name()),
            SourceRange::at(node.location()));
    }
}

void SemanticAnalyzer::visit(ExprStmt& node) {
    node.expr_mut().accept(*this);
}

void SemanticAnalyzer::visit(IdentExpr& node) {
    const auto* sym = symbols_.lookup(node.name());
    if (!sym) {
        auto& diag = diags_.emit_error("E0205",
            std::format("cannot find value `{}` in this scope", node.name()),
            SourceRange::at(node.location()));
        diags_.add_suggestion(diag,
            std::format("consider declaring `let {} = ...;`", node.name()));
        node.resolved_type = types::unknown();
        last_expr_type_    = types::unknown();
        return;
    }

    if (!sym->is_usable()) {
        if (sym->is_moved) {
            diags_.emit_error("E0206",
                std::format("use of moved value `{}`", node.name()),
                SourceRange::at(node.location()));
        } else {
            diags_.emit_error("E0207",
                std::format("`{}` used before initialization", node.name()),
                SourceRange::at(node.location()));
        }
        node.resolved_type = types::unknown();
        last_expr_type_    = types::unknown();
        return;
    }

    if (!sym->type->is_copy()) {
        symbols_.mark_moved(node.name());
    }

    node.resolved_type = sym->type;
    last_expr_type_    = sym->type;
}

void SemanticAnalyzer::visit(AssignExpr& node) {
    node.value_mut().accept(*this);
    const auto rhs_type = last_expr_type_;

    const auto* sym = symbols_.lookup(node.target());
    if (!sym) {
        diags_.emit_error("E0205",
            std::format("cannot find value `{}` in this scope", node.target()),
            SourceRange::at(node.location()));
        node.resolved_type = types::unknown();
        last_expr_type_    = types::unknown();
        return;
    }

    if (!sym->is_mutable) {
        auto& diag = diags_.emit_error("E0208",
            std::format("cannot assign twice to immutable variable `{}`", node.target()),
            SourceRange::at(node.location()));
        diags_.add_suggestion(diag,
            std::format("consider changing to `let mut {}`", node.target()));
        node.resolved_type = types::unknown();
        last_expr_type_    = types::unknown();
        return;
    }

    if (!rhs_type->is_unknown() && !sym->type->is_unknown()
        && !types::compatible(rhs_type, sym->type)) {
        type_error("E0209", sym->type->name(), rhs_type->name(), node.value().range());
    }

    node.resolved_type = sym->type;
    last_expr_type_    = sym->type;
}

void SemanticAnalyzer::visit(BinaryExpr& node) {
    node.lhs_mut().accept(*this);
    const auto lhs_type = last_expr_type_;

    node.rhs_mut().accept(*this);
    const auto rhs_type = last_expr_type_;

    TypePtr result;
    const auto range = node.range();

    switch (node.op()) {
        case BinaryOp::Add:
        case BinaryOp::Sub:
        case BinaryOp::Mul:
        case BinaryOp::Div:
        case BinaryOp::Rem:
            result = check_arithmetic_op(node.op(), lhs_type, rhs_type, range);
            break;

        case BinaryOp::Eq:
        case BinaryOp::Ne:
        case BinaryOp::Lt:
        case BinaryOp::Le:
        case BinaryOp::Gt:
        case BinaryOp::Ge:
            result = check_comparison_op(node.op(), lhs_type, rhs_type, range);
            break;

        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr:
            if (!lhs_type->is_unknown() && !lhs_type->is_bool()) {
                type_error("E0210", "bool", lhs_type->name(), node.lhs().range());
            }
            if (!rhs_type->is_unknown() && !rhs_type->is_bool()) {
                type_error("E0210", "bool", rhs_type->name(), node.rhs().range());
            }
            result = types::boolean();
            break;
    }

    node.resolved_type = result;
    last_expr_type_    = result;
}

auto SemanticAnalyzer::check_arithmetic_op(BinaryOp op,
                                            const TypePtr& lhs,
                                            const TypePtr& rhs,
                                            SourceRange range) -> TypePtr {
    if (lhs->is_unknown() || rhs->is_unknown()) return types::unknown();

    if (!lhs->is_numeric()) {
        type_error("E0211", "numeric type", lhs->name(), range);
        return types::unknown();
    }
    if (!rhs->is_numeric()) {
        type_error("E0211", "numeric type", rhs->name(), range);
        return types::unknown();
    }
    if (!types::compatible(lhs, rhs)) {
        type_error("E0212", lhs->name(), rhs->name(), range);
        return types::unknown();
    }
    return lhs;
}

auto SemanticAnalyzer::check_comparison_op(BinaryOp op,
                                             const TypePtr& lhs,
                                             const TypePtr& rhs,
                                             SourceRange range) -> TypePtr {
    if (lhs->is_unknown() || rhs->is_unknown()) return types::boolean();

    if (!types::compatible(lhs, rhs)) {
        type_error("E0213", lhs->name(), rhs->name(), range);
        return types::boolean();
    }
    return types::boolean();
}

void SemanticAnalyzer::visit(UnaryExpr& node) {
    node.operand_mut().accept(*this);
    const auto operand_type = last_expr_type_;

    TypePtr result;
    switch (node.op()) {
        case UnaryOp::Neg:
            if (!operand_type->is_unknown() && !operand_type->is_numeric()) {
                type_error("E0214", "numeric type", operand_type->name(), node.operand().range());
                result = types::unknown();
            } else {
                result = operand_type;
            }
            break;
        case UnaryOp::Not:
            if (!operand_type->is_unknown() && !operand_type->is_bool()) {
                type_error("E0214", "bool", operand_type->name(), node.operand().range());
                result = types::unknown();
            } else {
                result = types::boolean();
            }
            break;
    }

    node.resolved_type = result;
    last_expr_type_    = result;
}

void SemanticAnalyzer::visit(IntLiteralExpr& node) {
    node.resolved_type = node.is_i64() ? types::i64() : types::i32();
    last_expr_type_    = node.resolved_type;
}

void SemanticAnalyzer::visit(BoolLiteralExpr& node) {
    node.resolved_type = types::boolean();
    last_expr_type_    = types::boolean();
}

void SemanticAnalyzer::visit(CharLiteralExpr& node) {
    node.resolved_type = types::ch();
    last_expr_type_    = types::ch();
}

void SemanticAnalyzer::visit(UnitLiteralExpr& node) {
    node.resolved_type = types::unit();
    last_expr_type_    = types::unit();
}

void SemanticAnalyzer::visit(CallExpr& node) {
    const auto* sym = symbols_.lookup(node.callee());
    if (!sym) {
        diags_.emit_error("E0215",
            std::format("cannot find function `{}` in this scope", node.callee()),
            SourceRange::at(node.location()));
        node.resolved_type = types::unknown();
        last_expr_type_    = types::unknown();
        return;
    }

    if (!sym->is_function()) {
        diags_.emit_error("E0216",
            std::format("`{}` is not a function", node.callee()),
            SourceRange::at(node.location()));
        node.resolved_type = types::unknown();
        last_expr_type_    = types::unknown();
        return;
    }

    const auto& fn_type = static_cast<const FunctionType&>(*sym->type);
    const auto& params  = fn_type.param_types();

    if (node.args().size() != params.size()) {
        diags_.emit_error("E0217",
            std::format("function `{}` expects {} argument(s), got {}",
                node.callee(), params.size(), node.args().size()),
            SourceRange::at(node.location()));
    }

    for (std::size_t i = 0; i < node.args_mut().size(); ++i) {
        node.args_mut()[i]->accept(*this);
        const auto arg_type = last_expr_type_;

        if (i < params.size() && !arg_type->is_unknown()
            && !types::compatible(arg_type, params[i])) {
            type_error("E0218", params[i]->name(), arg_type->name(),
                node.args()[i]->range());
        }
    }

    node.resolved_type = fn_type.return_type();
    last_expr_type_    = fn_type.return_type();
}

void SemanticAnalyzer::visit(IfExpr& node) {
    node.condition_mut().accept(*this);
    const auto cond_type = last_expr_type_;

    if (!cond_type->is_unknown() && !cond_type->is_bool()) {
        type_error("E0219", "bool", cond_type->name(), node.condition().range());
    }

    node.then_branch_mut().accept(*this);
    const auto then_type = last_expr_type_;

    if (node.else_branch()) {
        (*node.else_branch_mut())->accept(*this);
        const auto else_type = last_expr_type_;

        if (!then_type->is_unknown() && !else_type->is_unknown()
            && !types::compatible(then_type, else_type)) {
            type_error("E0220",
                then_type->name(), else_type->name(),
                node.else_branch().has_value()
                    ? (*node.else_branch())->range()
                    : node.range());
        }
        node.resolved_type = then_type->is_unknown() ? else_type : then_type;
    } else {
        node.resolved_type = types::unit();
    }

    last_expr_type_ = node.resolved_type;
}

void SemanticAnalyzer::visit(WhileExpr& node) {
    node.condition_mut().accept(*this);
    const auto cond_type = last_expr_type_;

    if (!cond_type->is_unknown() && !cond_type->is_bool()) {
        type_error("E0221", "bool", cond_type->name(), node.condition().range());
    }

    const auto saved_loop_type = current_loop_break_type_;
    current_loop_break_type_   = types::unit();

    symbols_.push_scope(false, true);
    node.body_mut().accept(*this);
    symbols_.pop_scope();

    current_loop_break_type_ = saved_loop_type;
    node.resolved_type = types::unit();
    last_expr_type_    = types::unit();
}

void SemanticAnalyzer::visit(LoopExpr& node) {
    const auto saved_loop_type = current_loop_break_type_;
    current_loop_break_type_   = std::nullopt;

    symbols_.push_scope(false, true);
    node.body_mut().accept(*this);
    symbols_.pop_scope();

    node.resolved_type = current_loop_break_type_.value_or(types::unit());
    last_expr_type_    = node.resolved_type;
    current_loop_break_type_ = saved_loop_type;
}

void SemanticAnalyzer::visit(BreakExpr& node) {
    if (!symbols_.in_loop_scope()) {
        diags_.emit_error("E0222", "`break` outside of a loop", SourceRange::at(node.location()));
        node.resolved_type = types::unknown();
        last_expr_type_    = types::unknown();
        return;
    }

    TypePtr break_type = types::unit();
    if (node.value()) {
        (*node.value_mut())->accept(*this);
        break_type = last_expr_type_;
    }

    if (current_loop_break_type_.has_value()) {
        const auto& existing = *current_loop_break_type_;
        if (!break_type->is_unknown() && !existing->is_unknown()
            && !types::compatible(break_type, existing)) {
            type_error("E0223", existing->name(), break_type->name(), node.range());
        }
    } else {
        current_loop_break_type_ = break_type;
    }

    node.resolved_type = types::unknown();
    last_expr_type_    = types::unknown();
}

void SemanticAnalyzer::visit(ContinueExpr& node) {
    if (!symbols_.in_loop_scope()) {
        diags_.emit_error("E0224", "`continue` outside of a loop",
            SourceRange::at(node.location()));
    }
    node.resolved_type = types::unknown();
    last_expr_type_    = types::unknown();
}

void SemanticAnalyzer::visit(ReturnExpr& node) {
    TypePtr ret_type = types::unit();
    if (node.value()) {
        (*node.value_mut())->accept(*this);
        ret_type = last_expr_type_;
    }

    if (!ret_type->is_unknown() && !current_return_type_->is_unknown()
        && !types::compatible(ret_type, current_return_type_)) {
        type_error("E0225",
            current_return_type_->name(), ret_type->name(), node.range());
    }

    node.resolved_type = types::unknown();
    last_expr_type_    = types::unknown();
}

SymbolTable::SymbolTable() {
    scopes_.reserve(16);
}

void SymbolTable::push_scope(bool is_function_scope, bool is_loop_scope) {
    scopes_.push_back(Scope{
        .is_function_scope = is_function_scope,
        .is_loop_scope     = is_loop_scope,
    });
}

void SymbolTable::pop_scope() {
    if (!scopes_.empty()) scopes_.pop_back();
}

auto SymbolTable::declare(Symbol sym) -> bool {
    if (scopes_.empty()) return false;
    auto& top = scopes_.back().symbols;
    if (top.contains(sym.name)) return false;
    top.emplace(sym.name, std::move(sym));
    return true;
}

auto SymbolTable::lookup(const std::string& name) -> Symbol* {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->symbols.find(name);
        if (found != it->symbols.end()) return &found->second;
    }
    return nullptr;
}

auto SymbolTable::lookup(const std::string& name) const -> const Symbol* {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->symbols.find(name);
        if (found != it->symbols.end()) return &found->second;
    }
    return nullptr;
}

auto SymbolTable::lookup_current_scope(const std::string& name) -> Symbol* {
    if (scopes_.empty()) return nullptr;
    auto found = scopes_.back().symbols.find(name);
    return found != scopes_.back().symbols.end() ? &found->second : nullptr;
}

auto SymbolTable::in_loop_scope() const noexcept -> bool {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->is_loop_scope)     return true;
        if (it->is_function_scope) return false;
    }
    return false;
}

auto SymbolTable::in_function_scope() const noexcept -> bool {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
        if (it->is_function_scope) return true;
    return false;
}

auto SymbolTable::scope_depth() const noexcept -> std::size_t {
    return scopes_.size();
}

void SymbolTable::mark_moved(const std::string& name) {
    if (auto* sym = lookup(name)) sym->is_moved = true;
}

}
