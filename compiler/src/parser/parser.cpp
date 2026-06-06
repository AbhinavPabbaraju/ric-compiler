#include "parser/parser.hpp"
#include "ast/visitor.hpp"
#include <format>

namespace ric {

namespace {

auto type_from_token(TokenKind kind) -> TypePtr {
    switch (kind) {
        case TokenKind::KwI32:  return types::i32();
        case TokenKind::KwI64:  return types::i64();
        case TokenKind::KwBool: return types::boolean();
        case TokenKind::KwChar: return types::ch();
        default:                return nullptr;
    }
}

}

void BinaryExpr::accept(ASTVisitor& v)     { v.visit(*this); }
void UnaryExpr::accept(ASTVisitor& v)      { v.visit(*this); }
void IntLiteralExpr::accept(ASTVisitor& v) { v.visit(*this); }
void BoolLiteralExpr::accept(ASTVisitor& v){ v.visit(*this); }
void CharLiteralExpr::accept(ASTVisitor& v){ v.visit(*this); }
void UnitLiteralExpr::accept(ASTVisitor& v){ v.visit(*this); }
void IdentExpr::accept(ASTVisitor& v)      { v.visit(*this); }
void CallExpr::accept(ASTVisitor& v)       { v.visit(*this); }
void AssignExpr::accept(ASTVisitor& v)     { v.visit(*this); }
void BlockExpr::accept(ASTVisitor& v)      { v.visit(*this); }
void IfExpr::accept(ASTVisitor& v)         { v.visit(*this); }
void WhileExpr::accept(ASTVisitor& v)      { v.visit(*this); }
void LoopExpr::accept(ASTVisitor& v)       { v.visit(*this); }
void BreakExpr::accept(ASTVisitor& v)      { v.visit(*this); }
void ContinueExpr::accept(ASTVisitor& v)   { v.visit(*this); }
void ReturnExpr::accept(ASTVisitor& v)     { v.visit(*this); }
void LetStmt::accept(ASTVisitor& v)        { v.visit(*this); }
void ExprStmt::accept(ASTVisitor& v)       { v.visit(*this); }
void FnDecl::accept(ASTVisitor& v)         { v.visit(*this); }
void Program::accept(ASTVisitor& v)        { v.visit(*this); }

auto binary_op_symbol(BinaryOp op) noexcept -> std::string_view {
    switch (op) {
        case BinaryOp::Add:        return "+";
        case BinaryOp::Sub:        return "-";
        case BinaryOp::Mul:        return "*";
        case BinaryOp::Div:        return "/";
        case BinaryOp::Rem:        return "%";
        case BinaryOp::Eq:         return "==";
        case BinaryOp::Ne:         return "!=";
        case BinaryOp::Lt:         return "<";
        case BinaryOp::Le:         return "<=";
        case BinaryOp::Gt:         return ">";
        case BinaryOp::Ge:         return ">=";
        case BinaryOp::LogicalAnd: return "&&";
        case BinaryOp::LogicalOr:  return "||";
    }
    return "?";
}

auto unary_op_symbol(UnaryOp op) noexcept -> std::string_view {
    switch (op) {
        case UnaryOp::Neg: return "-";
        case UnaryOp::Not: return "!";
    }
    return "?";
}

auto I32Type::get() -> TypePtr { static auto s = std::make_shared<I32Type>(); return s; }
auto I64Type::get() -> TypePtr { static auto s = std::make_shared<I64Type>(); return s; }
auto BoolType::get()-> TypePtr { static auto s = std::make_shared<BoolType>();return s; }
auto CharType::get()-> TypePtr { static auto s = std::make_shared<CharType>();return s; }
auto UnitType::get()-> TypePtr { static auto s = std::make_shared<UnitType>();return s; }
auto UnknownType::get()->TypePtr{static auto s = std::make_shared<UnknownType>();return s;}

auto FunctionType::name() const -> std::string {
    std::string s = "fn(";
    for (std::size_t i = 0; i < params_.size(); ++i) {
        if (i > 0) s += ", ";
        s += params_[i]->name();
    }
    s += ") -> ";
    s += return_type_->name();
    return s;
}

auto FunctionType::equals(const Type& o) const noexcept -> bool {
    if (o.kind() != TypeKind::Function) return false;
    const auto& fn = static_cast<const FunctionType&>(o);
    if (!return_type_->equals(*fn.return_type_)) return false;
    if (params_.size() != fn.params_.size()) return false;
    for (std::size_t i = 0; i < params_.size(); ++i)
        if (!params_[i]->equals(*fn.params_[i])) return false;
    return true;
}

auto types::make_fn(std::vector<TypePtr> params, TypePtr ret) -> TypePtr {
    return std::make_shared<FunctionType>(std::move(params), std::move(ret));
}

auto types::compatible(const TypePtr& a, const TypePtr& b) noexcept -> bool {
    if (!a || !b) return false;
    if (a->is_unknown() || b->is_unknown()) return true;
    return a->equals(*b);
}

Parser::Parser(std::span<const Token> tokens, DiagnosticEngine& diags)
    : tokens_(tokens), diags_(diags) {}

auto Parser::peek(std::size_t offset) const noexcept -> const Token& {
    const auto idx = pos_ + offset;
    return idx < tokens_.size() ? tokens_[idx] : tokens_.back();
}

auto Parser::peek_kind(std::size_t offset) const noexcept -> TokenKind {
    return peek(offset).kind;
}

auto Parser::at_end() const noexcept -> bool {
    return peek_kind() == TokenKind::Eof;
}

auto Parser::check(TokenKind kind) const noexcept -> bool {
    return peek_kind() == kind;
}

auto Parser::advance() -> const Token& {
    const auto& tok = tokens_[pos_];
    if (!at_end()) ++pos_;
    return tok;
}

auto Parser::consume(TokenKind kind, std::string_view msg) -> const Token& {
    if (check(kind)) return advance();
    auto& diag = diags_.emit_error(
        "E0100",
        std::string(msg),
        SourceRange::at(peek().location));
    diags_.add_note(diag, std::string("found ") + std::string(token_kind_name(peek_kind())));
    return peek();
}

auto Parser::match(TokenKind kind) -> bool {
    if (check(kind)) { advance(); return true; }
    return false;
}

auto Parser::current_range() const noexcept -> SourceRange {
    return SourceRange::at(peek().location);
}

auto Parser::location_of(std::size_t idx) const noexcept -> SourceLocation {
    return idx < tokens_.size() ? tokens_[idx].location : tokens_.back().location;
}

void Parser::synchronize_to_stmt_boundary() {
    while (!at_end()) {
        if (peek_kind() == TokenKind::Semicolon) { advance(); return; }
        if (peek_kind() == TokenKind::RBrace)    return;
        if (peek_kind() == TokenKind::KwFn)      return;
        if (peek_kind() == TokenKind::KwLet)     return;
        if (peek_kind() == TokenKind::KwReturn)  return;
        advance();
    }
}

void Parser::synchronize_to_decl_boundary() {
    while (!at_end()) {
        if (peek_kind() == TokenKind::KwFn) return;
        advance();
    }
}

auto Parser::parse() -> std::unique_ptr<Program> {
    const auto start = location_of(0);
    std::vector<FnDeclPtr> decls;

    while (!at_end()) {
        if (check(TokenKind::KwFn)) {
            if (auto fn = parse_fn_decl()) {
                decls.push_back(std::move(fn));
            } else {
                synchronize_to_decl_boundary();
            }
        } else {
            diags_.emit_error("E0101",
                "expected `fn` at top level",
                SourceRange::at(peek().location));
            synchronize_to_decl_boundary();
        }
    }

    const auto end = location_of(pos_);
    return std::make_unique<Program>(
        SourceRange::spanning(start, end), std::move(decls));
}

auto Parser::parse_fn_decl() -> FnDeclPtr {
    const auto start = peek().location;
    consume(TokenKind::KwFn, "expected `fn`");

    const auto& name_tok = consume(TokenKind::Identifier, "expected function name after `fn`");
    const auto fn_name   = name_tok.lexeme;

    consume(TokenKind::LParen, "expected `(` after function name");
    auto params = parse_param_list();
    consume(TokenKind::RParen, "expected `)` after parameter list");

    TypePtr ret_type = types::unit();
    if (match(TokenKind::Arrow)) {
        ret_type = parse_type();
    }

    auto body = parse_block();
    if (!body) return nullptr;

    const auto end = body->range().end;
    return std::make_unique<FnDecl>(
        SourceRange::spanning(start, end),
        fn_name, std::move(params), std::move(ret_type), std::move(body));
}

auto Parser::parse_param_list() -> std::vector<FnParam> {
    std::vector<FnParam> params;
    if (check(TokenKind::RParen)) return params;

    do {
        const auto& name_tok = consume(TokenKind::Identifier, "expected parameter name");
        consume(TokenKind::Colon, "expected `:` after parameter name");
        auto ty = parse_type();
        params.push_back({.name = name_tok.lexeme, .type = std::move(ty),
                          .location = name_tok.location});
    } while (match(TokenKind::Comma) && !check(TokenKind::RParen));

    return params;
}

auto Parser::parse_type() -> TypePtr {
    const auto& tok = peek();
    if (auto ty = type_from_token(tok.kind)) {
        advance();
        return ty;
    }
    if (check(TokenKind::LParen)) {
        advance();
        consume(TokenKind::RParen, "expected `)` for unit type `()`");
        return types::unit();
    }
    diags_.emit_error("E0102", "expected a type", SourceRange::at(tok.location));
    return types::unknown();
}

auto Parser::parse_block() -> std::unique_ptr<BlockExpr> {
    const auto start = peek().location;
    consume(TokenKind::LBrace, "expected `{`");

    std::vector<StmtPtr>   stmts;
    std::optional<ExprPtr> tail;

    auto is_block_like = [](const Expr& e) -> bool {
        return dynamic_cast<const BlockExpr*>(&e)  != nullptr
            || dynamic_cast<const IfExpr*>(&e)     != nullptr
            || dynamic_cast<const WhileExpr*>(&e)  != nullptr
            || dynamic_cast<const LoopExpr*>(&e)   != nullptr;
    };

    while (!check(TokenKind::RBrace) && !at_end()) {
        if (check(TokenKind::KwLet)) {
            stmts.push_back(parse_let_stmt());
            continue;
        }

        auto expr = parse_expr();

        if (check(TokenKind::Semicolon)) {
            const auto semi_loc = peek().location;
            advance();
            stmts.push_back(std::make_unique<ExprStmt>(
                SourceRange::spanning(expr->location(), semi_loc), std::move(expr)));
        } else if (check(TokenKind::RBrace)) {
            tail = std::move(expr);
            break;
        } else if (is_block_like(*expr)) {
            const auto loc = expr->location();
            stmts.push_back(std::make_unique<ExprStmt>(
                SourceRange::at(loc), std::move(expr)));
        } else {
            diags_.emit_error("E0103",
                "expected `;` or `}` after expression",
                SourceRange::at(peek().location));
            synchronize_to_stmt_boundary();
        }
    }

    const auto end = peek().location;
    consume(TokenKind::RBrace, "expected `}` to close block");

    return std::make_unique<BlockExpr>(
        SourceRange::spanning(start, end), std::move(stmts), std::move(tail));
}

auto Parser::parse_stmt() -> StmtPtr {
    if (check(TokenKind::KwLet)) return parse_let_stmt();
    return parse_expr_stmt();
}

auto Parser::parse_let_stmt() -> StmtPtr {
    const auto start = peek().location;
    consume(TokenKind::KwLet, "expected `let`");

    const bool is_mut = match(TokenKind::KwMut);
    const auto& name_tok = consume(TokenKind::Identifier, "expected variable name");

    std::optional<TypePtr> annotation;
    if (match(TokenKind::Colon)) {
        annotation = parse_type();
    }

    consume(TokenKind::Eq, "expected `=` in let binding");
    auto init = parse_expr();

    const auto end = peek().location;
    consume(TokenKind::Semicolon, "expected `;` after let binding");

    return std::make_unique<LetStmt>(
        SourceRange::spanning(start, end),
        name_tok.lexeme, is_mut, std::move(annotation), std::move(init));
}

auto Parser::parse_expr_stmt() -> StmtPtr {
    auto expr = parse_expr();
    const auto end = peek().location;
    consume(TokenKind::Semicolon, "expected `;` after expression statement");
    return std::make_unique<ExprStmt>(
        SourceRange::spanning(expr->location(), end), std::move(expr));
}

auto Parser::parse_expr() -> ExprPtr { return parse_assign_expr(); }

auto Parser::parse_assign_expr() -> ExprPtr {
    auto lhs = parse_or_expr();

    if (check(TokenKind::Eq)) {
        const auto op_loc = peek().location;
        advance();
        auto rhs = parse_assign_expr();

        if (const auto* ident = dynamic_cast<const IdentExpr*>(lhs.get())) {
            return std::make_unique<AssignExpr>(
                SourceRange::spanning(lhs->location(), rhs->location()),
                ident->name(), std::move(rhs));
        }
        diags_.emit_error("E0104", "assignment target must be an identifier",
            SourceRange::at(op_loc));
        return make_error_expr(op_loc);
    }
    return lhs;
}

auto Parser::parse_or_expr() -> ExprPtr {
    auto lhs = parse_and_expr();
    while (check(TokenKind::PipePipe)) {
        advance();
        auto rhs = parse_and_expr();
        lhs = std::make_unique<BinaryExpr>(
            SourceRange::spanning(lhs->location(), rhs->location()),
            BinaryOp::LogicalOr, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

auto Parser::parse_and_expr() -> ExprPtr {
    auto lhs = parse_eq_expr();
    while (check(TokenKind::AmpAmp)) {
        advance();
        auto rhs = parse_eq_expr();
        lhs = std::make_unique<BinaryExpr>(
            SourceRange::spanning(lhs->location(), rhs->location()),
            BinaryOp::LogicalAnd, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

auto Parser::parse_eq_expr() -> ExprPtr {
    auto lhs = parse_rel_expr();
    while (check(TokenKind::EqEq) || check(TokenKind::BangEq)) {
        const auto op = peek_kind() == TokenKind::EqEq ? BinaryOp::Eq : BinaryOp::Ne;
        advance();
        auto rhs = parse_rel_expr();
        lhs = std::make_unique<BinaryExpr>(
            SourceRange::spanning(lhs->location(), rhs->location()),
            op, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

auto Parser::parse_rel_expr() -> ExprPtr {
    auto lhs = parse_add_expr();
    for (;;) {
        BinaryOp op;
        switch (peek_kind()) {
            case TokenKind::Lt:   op = BinaryOp::Lt; break;
            case TokenKind::Gt:   op = BinaryOp::Gt; break;
            case TokenKind::LtEq: op = BinaryOp::Le; break;
            case TokenKind::GtEq: op = BinaryOp::Ge; break;
            default: return lhs;
        }
        advance();
        auto rhs = parse_add_expr();
        lhs = std::make_unique<BinaryExpr>(
            SourceRange::spanning(lhs->location(), rhs->location()),
            op, std::move(lhs), std::move(rhs));
    }
}

auto Parser::parse_add_expr() -> ExprPtr {
    auto lhs = parse_mul_expr();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        const auto op = peek_kind() == TokenKind::Plus ? BinaryOp::Add : BinaryOp::Sub;
        advance();
        auto rhs = parse_mul_expr();
        lhs = std::make_unique<BinaryExpr>(
            SourceRange::spanning(lhs->location(), rhs->location()),
            op, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

auto Parser::parse_mul_expr() -> ExprPtr {
    auto lhs = parse_unary_expr();
    for (;;) {
        BinaryOp op;
        switch (peek_kind()) {
            case TokenKind::Star:    op = BinaryOp::Mul; break;
            case TokenKind::Slash:   op = BinaryOp::Div; break;
            case TokenKind::Percent: op = BinaryOp::Rem; break;
            default: return lhs;
        }
        advance();
        auto rhs = parse_unary_expr();
        lhs = std::make_unique<BinaryExpr>(
            SourceRange::spanning(lhs->location(), rhs->location()),
            op, std::move(lhs), std::move(rhs));
    }
}

auto Parser::parse_unary_expr() -> ExprPtr {
    const auto loc = peek().location;
    if (check(TokenKind::Minus)) {
        advance();
        auto operand = parse_unary_expr();
        return std::make_unique<UnaryExpr>(
            SourceRange::spanning(loc, operand->location()),
            UnaryOp::Neg, std::move(operand));
    }
    if (check(TokenKind::Bang)) {
        advance();
        auto operand = parse_unary_expr();
        return std::make_unique<UnaryExpr>(
            SourceRange::spanning(loc, operand->location()),
            UnaryOp::Not, std::move(operand));
    }
    return parse_primary_expr();
}

auto Parser::parse_primary_expr() -> ExprPtr {
    const auto& tok = peek();
    const auto  loc = tok.location;

    switch (tok.kind) {
        case TokenKind::IntLiteral: {
            advance();
            auto expr = std::make_unique<IntLiteralExpr>(
                SourceRange::at(loc), tok.literal.int_val);
            return expr;
        }
        case TokenKind::BoolLiteral: {
            advance();
            return std::make_unique<BoolLiteralExpr>(
                SourceRange::at(loc), tok.literal.bool_val);
        }
        case TokenKind::CharLiteral: {
            advance();
            return std::make_unique<CharLiteralExpr>(
                SourceRange::at(loc), tok.literal.char_val);
        }
        case TokenKind::Identifier: {
            advance();
            if (check(TokenKind::LParen)) {
                auto args = parse_call_args();
                const auto end = location_of(pos_ - 1);
                return std::make_unique<CallExpr>(
                    SourceRange::spanning(loc, end),
                    tok.lexeme, std::move(args));
            }
            return std::make_unique<IdentExpr>(SourceRange::at(loc), tok.lexeme);
        }
        case TokenKind::LParen: {
            advance();
            if (check(TokenKind::RParen)) {
                advance();
                return std::make_unique<UnitLiteralExpr>(SourceRange::at(loc));
            }
            auto inner = parse_expr();
            consume(TokenKind::RParen, "expected `)` to close parenthesised expression");
            return inner;
        }
        case TokenKind::LBrace:    return parse_block();
        case TokenKind::KwIf:      return parse_if_expr();
        case TokenKind::KwWhile:   return parse_while_expr();
        case TokenKind::KwLoop:    return parse_loop_expr();
        case TokenKind::KwReturn:  return parse_return_expr();
        case TokenKind::KwBreak:   return parse_break_expr();
        case TokenKind::KwContinue:return parse_continue_expr();

        default:
            diags_.emit_error("E0105",
                std::string("unexpected token ") + std::string(token_kind_name(tok.kind))
                    + " in expression",
                SourceRange::at(loc));
            advance();
            return make_error_expr(loc);
    }
}

auto Parser::parse_call_args() -> std::vector<ExprPtr> {
    consume(TokenKind::LParen, "expected `(`");
    std::vector<ExprPtr> args;

    if (!check(TokenKind::RParen)) {
        do {
            args.push_back(parse_expr());
        } while (match(TokenKind::Comma) && !check(TokenKind::RParen));
    }

    consume(TokenKind::RParen, "expected `)` after argument list");
    return args;
}

auto Parser::parse_if_expr() -> ExprPtr {
    const auto start = peek().location;
    consume(TokenKind::KwIf, "expected `if`");
    auto cond = parse_expr();
    auto then = parse_block();

    std::optional<ExprPtr> else_branch;
    if (match(TokenKind::KwElse)) {
        if (check(TokenKind::KwIf)) {
            else_branch = parse_if_expr();
        } else {
            else_branch = parse_block();
        }
    }

    const auto end = else_branch ? (*else_branch)->range().end : then->range().end;
    return std::make_unique<IfExpr>(
        SourceRange::spanning(start, end),
        std::move(cond), std::move(then), std::move(else_branch));
}

auto Parser::parse_while_expr() -> ExprPtr {
    const auto start = peek().location;
    consume(TokenKind::KwWhile, "expected `while`");
    auto cond = parse_expr();
    auto body = parse_block();
    const auto end = body->range().end;
    return std::make_unique<WhileExpr>(
        SourceRange::spanning(start, end), std::move(cond), std::move(body));
}

auto Parser::parse_loop_expr() -> ExprPtr {
    const auto start = peek().location;
    consume(TokenKind::KwLoop, "expected `loop`");
    auto body = parse_block();
    const auto end = body->range().end;
    return std::make_unique<LoopExpr>(SourceRange::spanning(start, end), std::move(body));
}

auto Parser::parse_return_expr() -> ExprPtr {
    const auto start = peek().location;
    consume(TokenKind::KwReturn, "expected `return`");

    std::optional<ExprPtr> value;
    if (!check(TokenKind::Semicolon) && !check(TokenKind::RBrace)) {
        value = parse_expr();
    }

    const auto end = value ? (*value)->range().end : start;
    return std::make_unique<ReturnExpr>(SourceRange::spanning(start, end), std::move(value));
}

auto Parser::parse_break_expr() -> ExprPtr {
    const auto start = peek().location;
    consume(TokenKind::KwBreak, "expected `break`");

    std::optional<ExprPtr> value;
    if (!check(TokenKind::Semicolon) && !check(TokenKind::RBrace)) {
        value = parse_expr();
    }

    const auto end = value ? (*value)->range().end : start;
    return std::make_unique<BreakExpr>(SourceRange::spanning(start, end), std::move(value));
}

auto Parser::parse_continue_expr() -> ExprPtr {
    const auto loc = peek().location;
    consume(TokenKind::KwContinue, "expected `continue`");
    return std::make_unique<ContinueExpr>(SourceRange::at(loc));
}

auto Parser::make_error_expr(SourceLocation loc) -> ExprPtr {
    auto expr = std::make_unique<IntLiteralExpr>(SourceRange::at(loc), 0);
    expr->resolved_type = types::unknown();
    return expr;
}

}
