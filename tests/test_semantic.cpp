#include <catch2/catch_test_macros.hpp>
#include "diagnostics/diagnostics.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "semantic/semantic_analyzer.hpp"

using namespace ric;

static auto analyze(std::string_view src) -> std::pair<bool, std::size_t> {
    DiagnosticEngine diags{"<test>", src};
    Lexer lexer{src, "<test>", diags};
    auto tokens = lexer.tokenize();
    Parser parser{tokens, diags};
    auto prog = parser.parse();
    if (diags.has_errors()) return {false, diags.error_count()};
    SemanticAnalyzer analyzer{diags};
    analyzer.analyze(*prog);
    return {!diags.has_errors(), diags.error_count()};
}

static auto analyze_ok(std::string_view src) -> bool {
    auto [ok, _] = analyze(src);
    return ok;
}

static auto analyze_err(std::string_view src) -> std::size_t {
    auto [_, n] = analyze(src);
    return n;
}

TEST_CASE("Semantic: simple well-typed program passes", "[semantic]") {
    REQUIRE(analyze_ok("fn add(a: i32, b: i32) -> i32 { a + b }"));
}

TEST_CASE("Semantic: integer literal has i32 type", "[semantic]") {
    REQUIRE(analyze_ok("fn f() -> i32 { 42 }"));
}

TEST_CASE("Semantic: bool literal has bool type", "[semantic]") {
    REQUIRE(analyze_ok("fn f() -> bool { true }"));
}

TEST_CASE("Semantic: char literal has char type", "[semantic]") {
    REQUIRE(analyze_ok("fn f() -> char { 'x' }"));
}

TEST_CASE("Semantic: unit return type inferred from empty block", "[semantic]") {
    REQUIRE(analyze_ok("fn f() {}"));
}

TEST_CASE("Semantic: return type mismatch is an error", "[semantic]") {
    REQUIRE(analyze_err("fn f() -> i32 { true }") > 0);
}

TEST_CASE("Semantic: undeclared variable is an error", "[semantic]") {
    REQUIRE(analyze_err("fn f() -> i32 { x }") > 0);
}

TEST_CASE("Semantic: let binding resolves identifier", "[semantic]") {
    REQUIRE(analyze_ok("fn f() -> i32 { let x = 5; x }"));
}

TEST_CASE("Semantic: immutable variable assignment is an error", "[semantic]") {
    REQUIRE(analyze_err("fn f() { let x = 5; x = 6; }") > 0);
}

TEST_CASE("Semantic: mutable variable assignment is allowed", "[semantic]") {
    REQUIRE(analyze_ok("fn f() { let mut x = 5; x = 6; }"));
}

TEST_CASE("Semantic: arithmetic on non-numeric type is an error", "[semantic]") {
    REQUIRE(analyze_err("fn f() -> bool { true + false }") > 0);
}

TEST_CASE("Semantic: mixed numeric types is an error", "[semantic]") {
    REQUIRE(analyze_err("fn add(a: i32, b: i64) -> i32 { a + b }") > 0);
}

TEST_CASE("Semantic: comparison produces bool", "[semantic]") {
    REQUIRE(analyze_ok("fn f(a: i32, b: i32) -> bool { a < b }"));
}

TEST_CASE("Semantic: logical ops require bool operands", "[semantic]") {
    REQUIRE(analyze_err("fn f(a: i32) -> bool { a && true }") > 0);
}

TEST_CASE("Semantic: logical ops on bools are fine", "[semantic]") {
    REQUIRE(analyze_ok("fn f(a: bool, b: bool) -> bool { a && b }"));
}

TEST_CASE("Semantic: if condition must be bool", "[semantic]") {
    REQUIRE(analyze_err("fn f() -> i32 { if 1 { 2 } else { 3 } }") > 0);
}

TEST_CASE("Semantic: if-else branches must agree on type", "[semantic]") {
    REQUIRE(analyze_err("fn f() -> i32 { if true { 1 } else { false } }") > 0);
}

TEST_CASE("Semantic: if-else with matching types is fine", "[semantic]") {
    REQUIRE(analyze_ok("fn f() -> i32 { if true { 1 } else { 2 } }"));
}

TEST_CASE("Semantic: while condition must be bool", "[semantic]") {
    REQUIRE(analyze_err("fn f() { while 42 {} }") > 0);
}

TEST_CASE("Semantic: while with bool condition is fine", "[semantic]") {
    REQUIRE(analyze_ok("fn f() { while true {} }"));
}

TEST_CASE("Semantic: break outside loop is an error", "[semantic]") {
    REQUIRE(analyze_err("fn f() { break; }") > 0);
}

TEST_CASE("Semantic: continue outside loop is an error", "[semantic]") {
    REQUIRE(analyze_err("fn f() { continue; }") > 0);
}

TEST_CASE("Semantic: break inside loop is fine", "[semantic]") {
    REQUIRE(analyze_ok("fn f() { loop { break; } }"));
}

TEST_CASE("Semantic: function call with wrong arity is an error", "[semantic]") {
    REQUIRE(analyze_err("fn f(a: i32) -> i32 { a } fn g() -> i32 { f(1, 2) }") > 0);
}

TEST_CASE("Semantic: function call with wrong type is an error", "[semantic]") {
    REQUIRE(analyze_err("fn f(a: i32) -> i32 { a } fn g() -> i32 { f(true) }") > 0);
}

TEST_CASE("Semantic: recursive call typechecks correctly", "[semantic]") {
    REQUIRE(analyze_ok(
        "fn factorial(n: i32) -> i32 { "
        "  if n <= 1 { 1 } else { n * factorial(n - 1) } "
        "}"
    ));
}

TEST_CASE("Semantic: mutual recursion fails if forward declaration missing", "[semantic]") {
    REQUIRE(analyze_err(
        "fn odd(n: i32) -> bool { if n == 0 { false } else { even(n - 1) } } "
        "fn even(n: i32) -> bool { if n == 0 { true } else { odd(n - 1) } }"
    ) > 0);
}

TEST_CASE("Semantic: mutual recursion succeeds when both declared", "[semantic]") {
    REQUIRE(analyze_ok(
        "fn even(n: i32) -> bool { if n == 0 { true } else { odd(n - 1) } } "
        "fn odd(n: i32) -> bool { if n == 0 { false } else { even(n - 1) } }"
    ));
}

TEST_CASE("Semantic: nested scopes shadow correctly", "[semantic]") {
    REQUIRE(analyze_ok(
        "fn f() -> i32 { "
        "  let x = 1; "
        "  { let x = 2; x } "
        "}"
    ));
}

TEST_CASE("Semantic: type annotation override is checked", "[semantic]") {
    REQUIRE(analyze_err("fn f() { let x: i32 = true; }") > 0);
}

TEST_CASE("Semantic: negation on bool is an error", "[semantic]") {
    REQUIRE(analyze_err("fn f() -> i32 { -true }") > 0);
}

TEST_CASE("Semantic: logical not on integer is an error", "[semantic]") {
    REQUIRE(analyze_err("fn f() -> bool { !42 }") > 0);
}

TEST_CASE("Semantic: duplicate function names are errors", "[semantic]") {
    REQUIRE(analyze_err("fn f() {} fn f() {}") > 0);
}
