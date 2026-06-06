#pragma once

#include "ast/ast.hpp"
#include "ast/visitor.hpp"
#include "diagnostics/diagnostics.hpp"
#include "semantic/symbol_table.hpp"
#include <optional>
#include <string>
#include <vector>

namespace ric {

class SemanticAnalyzer final : public ASTVisitor {
public:
    explicit SemanticAnalyzer(DiagnosticEngine& diags);

    [[nodiscard]] auto analyze(Program& program) -> bool;

    void visit(BinaryExpr&)     override;
    void visit(UnaryExpr&)      override;
    void visit(IntLiteralExpr&) override;
    void visit(BoolLiteralExpr&)override;
    void visit(CharLiteralExpr&)override;
    void visit(UnitLiteralExpr&)override;
    void visit(IdentExpr&)      override;
    void visit(CallExpr&)       override;
    void visit(AssignExpr&)     override;
    void visit(BlockExpr&)      override;
    void visit(IfExpr&)         override;
    void visit(WhileExpr&)      override;
    void visit(LoopExpr&)       override;
    void visit(BreakExpr&)      override;
    void visit(ContinueExpr&)   override;
    void visit(ReturnExpr&)     override;
    void visit(LetStmt&)        override;
    void visit(ExprStmt&)       override;
    void visit(FnDecl&)         override;
    void visit(Program&)        override;

private:
    DiagnosticEngine&      diags_;
    SymbolTable            symbols_;
    TypePtr                current_return_type_;
    std::optional<TypePtr> current_loop_break_type_;
    TypePtr                last_expr_type_;

    void collect_fn_signatures(Program& program);
    void check_fn_decl(FnDecl& fn);

    void type_error(const std::string& code,
                    const std::string& expected,
                    const std::string& found,
                    SourceRange range);

    [[nodiscard]] auto check_arithmetic_op(BinaryOp op, const TypePtr& lhs_ty,
                                            const TypePtr& rhs_ty,
                                            SourceRange range) -> TypePtr;

    [[nodiscard]] auto check_comparison_op(BinaryOp op, const TypePtr& lhs_ty,
                                            const TypePtr& rhs_ty,
                                            SourceRange range) -> TypePtr;
};

}
