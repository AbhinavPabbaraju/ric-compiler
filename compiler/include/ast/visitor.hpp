#pragma once

namespace ric {

class BinaryExpr;
class UnaryExpr;
class IntLiteralExpr;
class BoolLiteralExpr;
class CharLiteralExpr;
class UnitLiteralExpr;
class IdentExpr;
class CallExpr;
class AssignExpr;
class BlockExpr;
class IfExpr;
class WhileExpr;
class LoopExpr;
class BreakExpr;
class ContinueExpr;
class ReturnExpr;
class LetStmt;
class ExprStmt;
class FnDecl;
class Program;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(BinaryExpr&)    = 0;
    virtual void visit(UnaryExpr&)     = 0;
    virtual void visit(IntLiteralExpr&)= 0;
    virtual void visit(BoolLiteralExpr&)=0;
    virtual void visit(CharLiteralExpr&)=0;
    virtual void visit(UnitLiteralExpr&)=0;
    virtual void visit(IdentExpr&)     = 0;
    virtual void visit(CallExpr&)      = 0;
    virtual void visit(AssignExpr&)    = 0;
    virtual void visit(BlockExpr&)     = 0;
    virtual void visit(IfExpr&)        = 0;
    virtual void visit(WhileExpr&)     = 0;
    virtual void visit(LoopExpr&)      = 0;
    virtual void visit(BreakExpr&)     = 0;
    virtual void visit(ContinueExpr&)  = 0;
    virtual void visit(ReturnExpr&)    = 0;
    virtual void visit(LetStmt&)       = 0;
    virtual void visit(ExprStmt&)      = 0;
    virtual void visit(FnDecl&)        = 0;
    virtual void visit(Program&)       = 0;
};

class ASTVisitorBase : public ASTVisitor {
public:
    void visit(BinaryExpr&)     override {}
    void visit(UnaryExpr&)      override {}
    void visit(IntLiteralExpr&) override {}
    void visit(BoolLiteralExpr&)override {}
    void visit(CharLiteralExpr&)override {}
    void visit(UnitLiteralExpr&)override {}
    void visit(IdentExpr&)      override {}
    void visit(CallExpr&)       override {}
    void visit(AssignExpr&)     override {}
    void visit(BlockExpr&)      override {}
    void visit(IfExpr&)         override {}
    void visit(WhileExpr&)      override {}
    void visit(LoopExpr&)       override {}
    void visit(BreakExpr&)      override {}
    void visit(ContinueExpr&)   override {}
    void visit(ReturnExpr&)     override {}
    void visit(LetStmt&)        override {}
    void visit(ExprStmt&)       override {}
    void visit(FnDecl&)         override {}
    void visit(Program&)        override {}
};

}
