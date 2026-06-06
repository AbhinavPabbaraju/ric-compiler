#pragma once

#include "ast/types.hpp"
#include "diagnostics/source_location.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ric {

class ASTVisitor;

class ASTNode {
public:
    explicit ASTNode(SourceRange range) noexcept : range_(std::move(range)) {}
    virtual ~ASTNode() = default;

    ASTNode(const ASTNode&)            = delete;
    ASTNode& operator=(const ASTNode&) = delete;
    ASTNode(ASTNode&&)                 = default;
    ASTNode& operator=(ASTNode&&)      = default;

    [[nodiscard]] auto range()    const noexcept -> const SourceRange&    { return range_; }
    [[nodiscard]] auto location() const noexcept -> const SourceLocation& { return range_.begin; }

    virtual void accept(ASTVisitor& visitor) = 0;

protected:
    SourceRange range_;
};

class Expr;
class Stmt;
class FnDecl;

using ExprPtr   = std::unique_ptr<Expr>;
using StmtPtr   = std::unique_ptr<Stmt>;
using FnDeclPtr = std::unique_ptr<FnDecl>;

enum class BinaryOp {
    Add, Sub, Mul, Div, Rem,
    Eq, Ne, Lt, Le, Gt, Ge,
    LogicalAnd, LogicalOr,
};

enum class UnaryOp { Neg, Not };

[[nodiscard]] auto binary_op_symbol(BinaryOp op) noexcept -> std::string_view;
[[nodiscard]] auto unary_op_symbol(UnaryOp op) noexcept   -> std::string_view;

class Expr : public ASTNode {
public:
    explicit Expr(SourceRange range) noexcept : ASTNode(std::move(range)) {}
    TypePtr resolved_type;
};

class BinaryExpr final : public Expr {
public:
    BinaryExpr(SourceRange range, BinaryOp op, ExprPtr lhs, ExprPtr rhs)
        : Expr(std::move(range))
        , op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

    [[nodiscard]] auto op()      const noexcept -> BinaryOp  { return op_; }
    [[nodiscard]] auto lhs()     const noexcept -> const Expr& { return *lhs_; }
    [[nodiscard]] auto rhs()     const noexcept -> const Expr& { return *rhs_; }
    [[nodiscard]] auto lhs_mut() noexcept       -> Expr&       { return *lhs_; }
    [[nodiscard]] auto rhs_mut() noexcept       -> Expr&       { return *rhs_; }

    void accept(ASTVisitor& v) override;

private:
    BinaryOp op_;
    ExprPtr  lhs_, rhs_;
};

class UnaryExpr final : public Expr {
public:
    UnaryExpr(SourceRange range, UnaryOp op, ExprPtr operand)
        : Expr(std::move(range)), op_(op), operand_(std::move(operand)) {}

    [[nodiscard]] auto op()          const noexcept -> UnaryOp   { return op_; }
    [[nodiscard]] auto operand()     const noexcept -> const Expr& { return *operand_; }
    [[nodiscard]] auto operand_mut() noexcept       -> Expr&       { return *operand_; }

    void accept(ASTVisitor& v) override;

private:
    UnaryOp op_;
    ExprPtr operand_;
};

class IntLiteralExpr final : public Expr {
public:
    IntLiteralExpr(SourceRange range, std::int64_t value, bool is_i64 = false)
        : Expr(std::move(range)), value_(value), is_i64_(is_i64) {}

    [[nodiscard]] auto value()  const noexcept -> std::int64_t { return value_; }
    [[nodiscard]] auto is_i64() const noexcept -> bool         { return is_i64_; }

    void accept(ASTVisitor& v) override;

private:
    std::int64_t value_;
    bool         is_i64_;
};

class BoolLiteralExpr final : public Expr {
public:
    BoolLiteralExpr(SourceRange range, bool value)
        : Expr(std::move(range)), value_(value) {}

    [[nodiscard]] auto value() const noexcept -> bool { return value_; }

    void accept(ASTVisitor& v) override;

private:
    bool value_;
};

class CharLiteralExpr final : public Expr {
public:
    CharLiteralExpr(SourceRange range, char32_t value)
        : Expr(std::move(range)), value_(value) {}

    [[nodiscard]] auto value() const noexcept -> char32_t { return value_; }

    void accept(ASTVisitor& v) override;

private:
    char32_t value_;
};

class UnitLiteralExpr final : public Expr {
public:
    explicit UnitLiteralExpr(SourceRange range) : Expr(std::move(range)) {}
    void accept(ASTVisitor& v) override;
};

class IdentExpr final : public Expr {
public:
    IdentExpr(SourceRange range, std::string name)
        : Expr(std::move(range)), name_(std::move(name)) {}

    [[nodiscard]] auto name() const noexcept -> const std::string& { return name_; }

    void accept(ASTVisitor& v) override;

private:
    std::string name_;
};

class CallExpr final : public Expr {
public:
    CallExpr(SourceRange range, std::string callee, std::vector<ExprPtr> args)
        : Expr(std::move(range)), callee_(std::move(callee)), args_(std::move(args)) {}

    [[nodiscard]] auto callee()    const noexcept -> const std::string&        { return callee_; }
    [[nodiscard]] auto args()      const noexcept -> const std::vector<ExprPtr>& { return args_; }
    [[nodiscard]] auto args_mut()  noexcept       -> std::vector<ExprPtr>&       { return args_; }

    void accept(ASTVisitor& v) override;

private:
    std::string          callee_;
    std::vector<ExprPtr> args_;
};

class AssignExpr final : public Expr {
public:
    AssignExpr(SourceRange range, std::string target, ExprPtr value)
        : Expr(std::move(range)), target_(std::move(target)), value_(std::move(value)) {}

    [[nodiscard]] auto target()    const noexcept -> const std::string& { return target_; }
    [[nodiscard]] auto value()     const noexcept -> const Expr&        { return *value_; }
    [[nodiscard]] auto value_mut() noexcept       -> Expr&              { return *value_; }

    void accept(ASTVisitor& v) override;

private:
    std::string target_;
    ExprPtr     value_;
};

class BlockExpr final : public Expr {
public:
    BlockExpr(SourceRange range, std::vector<StmtPtr> stmts,
              std::optional<ExprPtr> tail)
        : Expr(std::move(range))
        , stmts_(std::move(stmts))
        , tail_expr_(std::move(tail)) {}

    [[nodiscard]] auto stmts()         const noexcept -> const std::vector<StmtPtr>&  { return stmts_; }
    [[nodiscard]] auto stmts_mut()     noexcept       -> std::vector<StmtPtr>&         { return stmts_; }
    [[nodiscard]] auto tail_expr()     const noexcept -> const std::optional<ExprPtr>& { return tail_expr_; }
    [[nodiscard]] auto tail_expr_mut() noexcept       -> std::optional<ExprPtr>&       { return tail_expr_; }

    void accept(ASTVisitor& v) override;

private:
    std::vector<StmtPtr>    stmts_;
    std::optional<ExprPtr>  tail_expr_;
};

class IfExpr final : public Expr {
public:
    IfExpr(SourceRange range, ExprPtr condition,
           ExprPtr then_branch, std::optional<ExprPtr> else_branch)
        : Expr(std::move(range))
        , condition_(std::move(condition))
        , then_branch_(std::move(then_branch))
        , else_branch_(std::move(else_branch)) {}

    [[nodiscard]] auto condition()      const noexcept -> const Expr&                { return *condition_; }
    [[nodiscard]] auto then_branch()    const noexcept -> const Expr&                { return *then_branch_; }
    [[nodiscard]] auto else_branch()    const noexcept -> const std::optional<ExprPtr>& { return else_branch_; }
    [[nodiscard]] auto condition_mut()  noexcept       -> Expr&                      { return *condition_; }
    [[nodiscard]] auto then_branch_mut()noexcept       -> Expr&                      { return *then_branch_; }
    [[nodiscard]] auto else_branch_mut()noexcept       -> std::optional<ExprPtr>&    { return else_branch_; }

    void accept(ASTVisitor& v) override;

private:
    ExprPtr                condition_;
    ExprPtr                then_branch_;
    std::optional<ExprPtr> else_branch_;
};

class WhileExpr final : public Expr {
public:
    WhileExpr(SourceRange range, ExprPtr condition, ExprPtr body)
        : Expr(std::move(range))
        , condition_(std::move(condition))
        , body_(std::move(body)) {}

    [[nodiscard]] auto condition()     const noexcept -> const Expr& { return *condition_; }
    [[nodiscard]] auto body()          const noexcept -> const Expr& { return *body_; }
    [[nodiscard]] auto condition_mut() noexcept       -> Expr&       { return *condition_; }
    [[nodiscard]] auto body_mut()      noexcept       -> Expr&       { return *body_; }

    void accept(ASTVisitor& v) override;

private:
    ExprPtr condition_, body_;
};

class LoopExpr final : public Expr {
public:
    LoopExpr(SourceRange range, ExprPtr body)
        : Expr(std::move(range)), body_(std::move(body)) {}

    [[nodiscard]] auto body()     const noexcept -> const Expr& { return *body_; }
    [[nodiscard]] auto body_mut() noexcept       -> Expr&       { return *body_; }

    void accept(ASTVisitor& v) override;

private:
    ExprPtr body_;
};

class BreakExpr final : public Expr {
public:
    BreakExpr(SourceRange range, std::optional<ExprPtr> value)
        : Expr(std::move(range)), value_(std::move(value)) {}

    [[nodiscard]] auto value()     const noexcept -> const std::optional<ExprPtr>& { return value_; }
    [[nodiscard]] auto value_mut() noexcept       -> std::optional<ExprPtr>&       { return value_; }

    void accept(ASTVisitor& v) override;

private:
    std::optional<ExprPtr> value_;
};

class ContinueExpr final : public Expr {
public:
    explicit ContinueExpr(SourceRange range) : Expr(std::move(range)) {}
    void accept(ASTVisitor& v) override;
};

class ReturnExpr final : public Expr {
public:
    ReturnExpr(SourceRange range, std::optional<ExprPtr> value)
        : Expr(std::move(range)), value_(std::move(value)) {}

    [[nodiscard]] auto value()     const noexcept -> const std::optional<ExprPtr>& { return value_; }
    [[nodiscard]] auto value_mut() noexcept       -> std::optional<ExprPtr>&       { return value_; }

    void accept(ASTVisitor& v) override;

private:
    std::optional<ExprPtr> value_;
};

class Stmt : public ASTNode {
public:
    explicit Stmt(SourceRange range) noexcept : ASTNode(std::move(range)) {}
};

class LetStmt final : public Stmt {
public:
    LetStmt(SourceRange range, std::string name, bool is_mutable,
            std::optional<TypePtr> annotation, ExprPtr initializer)
        : Stmt(std::move(range))
        , name_(std::move(name))
        , is_mutable_(is_mutable)
        , annotation_(std::move(annotation))
        , initializer_(std::move(initializer)) {}

    [[nodiscard]] auto name()            const noexcept -> const std::string&       { return name_; }
    [[nodiscard]] auto is_mutable()      const noexcept -> bool                     { return is_mutable_; }
    [[nodiscard]] auto annotation()      const noexcept -> const std::optional<TypePtr>& { return annotation_; }
    [[nodiscard]] auto initializer()     const noexcept -> const Expr&              { return *initializer_; }
    [[nodiscard]] auto initializer_mut() noexcept       -> Expr&                    { return *initializer_; }

    void accept(ASTVisitor& v) override;

private:
    std::string            name_;
    bool                   is_mutable_;
    std::optional<TypePtr> annotation_;
    ExprPtr                initializer_;
};

class ExprStmt final : public Stmt {
public:
    ExprStmt(SourceRange range, ExprPtr expr)
        : Stmt(std::move(range)), expr_(std::move(expr)) {}

    [[nodiscard]] auto expr()     const noexcept -> const Expr& { return *expr_; }
    [[nodiscard]] auto expr_mut() noexcept       -> Expr&       { return *expr_; }

    void accept(ASTVisitor& v) override;

private:
    ExprPtr expr_;
};

struct FnParam {
    std::string    name;
    TypePtr        type;
    SourceLocation location;
};

class FnDecl final : public ASTNode {
public:
    FnDecl(SourceRange range, std::string name, std::vector<FnParam> params,
           TypePtr return_type, std::unique_ptr<BlockExpr> body)
        : ASTNode(std::move(range))
        , name_(std::move(name))
        , params_(std::move(params))
        , return_type_(std::move(return_type))
        , body_(std::move(body)) {}

    [[nodiscard]] auto name()        const noexcept -> const std::string&        { return name_; }
    [[nodiscard]] auto params()      const noexcept -> const std::vector<FnParam>& { return params_; }
    [[nodiscard]] auto return_type() const noexcept -> const TypePtr&            { return return_type_; }
    [[nodiscard]] auto body()        const noexcept -> const BlockExpr&          { return *body_; }
    [[nodiscard]] auto body_mut()    noexcept       -> BlockExpr&                { return *body_; }

    void accept(ASTVisitor& v) override;

private:
    std::string                  name_;
    std::vector<FnParam>         params_;
    TypePtr                      return_type_;
    std::unique_ptr<BlockExpr>   body_;
};

class Program final : public ASTNode {
public:
    Program(SourceRange range, std::vector<FnDeclPtr> decls)
        : ASTNode(std::move(range)), declarations_(std::move(decls)) {}

    [[nodiscard]] auto declarations()     const noexcept -> const std::vector<FnDeclPtr>& { return declarations_; }
    [[nodiscard]] auto declarations_mut() noexcept       -> std::vector<FnDeclPtr>&       { return declarations_; }

    void accept(ASTVisitor& v) override;

private:
    std::vector<FnDeclPtr> declarations_;
};

}
