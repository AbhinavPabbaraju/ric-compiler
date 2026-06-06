#pragma once

#include "ast/ast.hpp"
#include "ast/visitor.hpp"
#include "ir/ir.hpp"
#include <optional>
#include <stack>
#include <string>

namespace ric {

class IRBuilder final : public ASTVisitor {
public:
    explicit IRBuilder();

    [[nodiscard]] auto lower(Program& program) -> ir::Module;

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
    ir::Module    module_;
    ir::Function* current_fn_{nullptr};

    std::optional<ir::Value> last_value_;

    struct LoopContext {
        ir::LabelId header_label;
        ir::LabelId exit_label;
        std::optional<ir::TempId> break_slot;
    };

    std::vector<LoopContext> loop_stack_;

    void emit(ir::Instruction instr);
    [[nodiscard]] auto alloc_temp(TypePtr type, std::string name = "") -> ir::TempId;
    [[nodiscard]] auto alloc_label(std::string hint)                   -> ir::LabelId;
    [[nodiscard]] auto current_block()                                  -> ir::BasicBlock&;
    void switch_to(ir::LabelId label);
    void connect_blocks(ir::LabelId from, ir::LabelId to);

    void lower_fn(FnDecl& fn);

    [[nodiscard]] auto take_value()  -> ir::Value;
    void               set_value(ir::Value val);
};

}
