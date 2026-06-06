#include <catch2/catch_test_macros.hpp>
#include "diagnostics/diagnostics.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

using namespace ric;

static auto parse(std::string_view src) -> std::unique_ptr<Program> {
    DiagnosticEngine diags{"<test>", src};
    Lexer lexer{src, "<test>", diags};
    auto tokens = lexer.tokenize();
    Parser parser{tokens, diags};
    return parser.parse();
}

static auto parse_ok(std::string_view src) -> std::unique_ptr<Program> {
    DiagnosticEngine diags{"<test>", src};
    Lexer lexer{src, "<test>", diags};
    auto tokens = lexer.tokenize();
    Parser parser{tokens, diags};
    auto prog = parser.parse();
    REQUIRE_FALSE(diags.has_errors());
    return prog;
}

TEST_CASE("Parser: empty program produces zero declarations", "[parser]") {
    const auto prog = parse_ok("");
    REQUIRE(prog->declarations().empty());
}

TEST_CASE("Parser: single function with no params and unit return", "[parser]") {
    const auto prog = parse_ok("fn nothing() {}");
    REQUIRE(prog->declarations().size() == 1);
    const auto& fn = *prog->declarations()[0];
    REQUIRE(fn.name() == "nothing");
    REQUIRE(fn.params().empty());
    REQUIRE(fn.return_type()->kind() == TypeKind::Unit);
}

TEST_CASE("Parser: function with i32 parameters and return type", "[parser]") {
    const auto prog = parse_ok("fn add(a: i32, b: i32) -> i32 { a + b }");
    REQUIRE(prog->declarations().size() == 1);
    const auto& fn = *prog->declarations()[0];
    REQUIRE(fn.name() == "add");
    REQUIRE(fn.params().size() == 2);
    REQUIRE(fn.params()[0].name == "a");
    REQUIRE(fn.params()[0].type->kind() == TypeKind::I32);
    REQUIRE(fn.params()[1].name == "b");
    REQUIRE(fn.return_type()->kind() == TypeKind::I32);
}

TEST_CASE("Parser: let binding with initializer", "[parser]") {
    const auto prog = parse_ok("fn f() { let x = 42; }");
    const auto& body = prog->declarations()[0]->body();
    REQUIRE(body.stmts().size() == 1);
    const auto* let = dynamic_cast<const LetStmt*>(body.stmts()[0].get());
    REQUIRE(let != nullptr);
    REQUIRE(let->name() == "x");
    REQUIRE_FALSE(let->is_mutable());
}

TEST_CASE("Parser: mutable let binding", "[parser]") {
    const auto prog = parse_ok("fn f() { let mut y = 0; }");
    const auto& body = prog->declarations()[0]->body();
    const auto* let  = dynamic_cast<const LetStmt*>(body.stmts()[0].get());
    REQUIRE(let != nullptr);
    REQUIRE(let->is_mutable());
    REQUIRE(let->name() == "y");
}

TEST_CASE("Parser: binary expression precedence — mul before add", "[parser]") {
    const auto prog = parse_ok("fn f() -> i32 { 1 + 2 * 3 }");
    const auto& tail = *prog->declarations()[0]->body().tail_expr();
    const auto* add  = dynamic_cast<const BinaryExpr*>(tail.get());
    REQUIRE(add != nullptr);
    REQUIRE(add->op() == BinaryOp::Add);
    const auto* mul  = dynamic_cast<const BinaryExpr*>(&add->rhs());
    REQUIRE(mul != nullptr);
    REQUIRE(mul->op() == BinaryOp::Mul);
}

TEST_CASE("Parser: if expression with else", "[parser]") {
    const auto prog = parse_ok(
        "fn f(b: bool) -> i32 { if b { 1 } else { 2 } }");
    const auto& tail = *prog->declarations()[0]->body().tail_expr();
    const auto* ife  = dynamic_cast<const IfExpr*>(tail.get());
    REQUIRE(ife != nullptr);
    REQUIRE(ife->else_branch().has_value());
}

TEST_CASE("Parser: while loop", "[parser]") {
    const auto prog = parse_ok("fn f() { while true { } }");
    const auto& body = prog->declarations()[0]->body();
    REQUIRE(body.stmts().size() == 1);
    const auto* es = dynamic_cast<const ExprStmt*>(body.stmts()[0].get());
    REQUIRE(es != nullptr);
    const auto* w  = dynamic_cast<const WhileExpr*>(&es->expr());
    REQUIRE(w != nullptr);
}

TEST_CASE("Parser: loop with break value", "[parser]") {
    const auto prog = parse_ok("fn f() -> i32 { loop { break 42; } }");
    const auto& tail = *prog->declarations()[0]->body().tail_expr();
    const auto* lp   = dynamic_cast<const LoopExpr*>(tail.get());
    REQUIRE(lp != nullptr);
}

TEST_CASE("Parser: return statement", "[parser]") {
    const auto prog = parse_ok("fn f() -> i32 { return 0; }");
    const auto& body = prog->declarations()[0]->body();
    const auto* es   = dynamic_cast<const ExprStmt*>(body.stmts()[0].get());
    REQUIRE(es != nullptr);
    const auto* ret  = dynamic_cast<const ReturnExpr*>(&es->expr());
    REQUIRE(ret != nullptr);
    REQUIRE(ret->value().has_value());
}

TEST_CASE("Parser: recursive call in expression", "[parser]") {
    const auto prog = parse_ok(
        "fn fib(n: i32) -> i32 { fib(n - 1) + fib(n - 2) }");
    REQUIRE_FALSE(prog->declarations().empty());
    const auto& tail = *prog->declarations()[0]->body().tail_expr();
    const auto* add  = dynamic_cast<const BinaryExpr*>(tail.get());
    REQUIRE(add != nullptr);
    REQUIRE(add->op() == BinaryOp::Add);
    const auto* c1 = dynamic_cast<const CallExpr*>(&add->lhs());
    REQUIRE(c1 != nullptr);
    REQUIRE(c1->callee() == "fib");
}

TEST_CASE("Parser: multiple top-level functions", "[parser]") {
    const auto prog = parse_ok("fn a() {} fn b() {} fn c() {}");
    REQUIRE(prog->declarations().size() == 3);
}

TEST_CASE("Parser: error recovery on bad statement", "[parser]") {
    DiagnosticEngine diags{"<test>", "fn f() { @@@ let x = 1; }"};
    Lexer lexer{"fn f() { @@@ let x = 1; }", "<test>", diags};
    auto tokens = lexer.tokenize();
    Parser parser{tokens, diags};
    auto prog = parser.parse();
    REQUIRE(diags.has_errors());
    REQUIRE(prog != nullptr);
}
