#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostics.hpp"
#include "lexer/token.hpp"
#include <memory>
#include <span>
#include <vector>

namespace ric {

class Parser {
public:
    explicit Parser(std::span<const Token> tokens, DiagnosticEngine& diags);

    [[nodiscard]] auto parse() -> std::unique_ptr<Program>;

private:
    std::span<const Token> tokens_;
    DiagnosticEngine&      diags_;
    std::size_t            pos_{0};

    [[nodiscard]] auto peek(std::size_t offset = 0) const noexcept -> const Token&;
    [[nodiscard]] auto peek_kind(std::size_t offset = 0) const noexcept -> TokenKind;
    [[nodiscard]] auto at_end()   const noexcept -> bool;
    [[nodiscard]] auto check(TokenKind kind) const noexcept -> bool;

    auto advance()                             -> const Token&;
    auto consume(TokenKind kind, std::string_view msg) -> const Token&;
    auto match(TokenKind kind)                 -> bool;

    void synchronize_to_stmt_boundary();
    void synchronize_to_decl_boundary();

    [[nodiscard]] auto parse_fn_decl()  -> FnDeclPtr;
    [[nodiscard]] auto parse_param_list() -> std::vector<FnParam>;
    [[nodiscard]] auto parse_type()       -> TypePtr;
    [[nodiscard]] auto parse_block()      -> std::unique_ptr<BlockExpr>;

    [[nodiscard]] auto parse_stmt()        -> StmtPtr;
    [[nodiscard]] auto parse_let_stmt()    -> StmtPtr;
    [[nodiscard]] auto parse_expr_stmt()   -> StmtPtr;

    [[nodiscard]] auto parse_expr()        -> ExprPtr;
    [[nodiscard]] auto parse_assign_expr() -> ExprPtr;
    [[nodiscard]] auto parse_or_expr()     -> ExprPtr;
    [[nodiscard]] auto parse_and_expr()    -> ExprPtr;
    [[nodiscard]] auto parse_eq_expr()     -> ExprPtr;
    [[nodiscard]] auto parse_rel_expr()    -> ExprPtr;
    [[nodiscard]] auto parse_add_expr()    -> ExprPtr;
    [[nodiscard]] auto parse_mul_expr()    -> ExprPtr;
    [[nodiscard]] auto parse_unary_expr()  -> ExprPtr;
    [[nodiscard]] auto parse_primary_expr()-> ExprPtr;
    [[nodiscard]] auto parse_call_args()   -> std::vector<ExprPtr>;

    [[nodiscard]] auto parse_if_expr()      -> ExprPtr;
    [[nodiscard]] auto parse_while_expr()   -> ExprPtr;
    [[nodiscard]] auto parse_loop_expr()    -> ExprPtr;
    [[nodiscard]] auto parse_return_expr()  -> ExprPtr;
    [[nodiscard]] auto parse_break_expr()   -> ExprPtr;
    [[nodiscard]] auto parse_continue_expr()-> ExprPtr;

    [[nodiscard]] auto make_error_expr(SourceLocation loc) -> ExprPtr;
    [[nodiscard]] auto current_range()   const noexcept -> SourceRange;
    [[nodiscard]] auto location_of(std::size_t idx) const noexcept -> SourceLocation;
};

}
