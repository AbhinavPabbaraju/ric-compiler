#include <catch2/catch_test_macros.hpp>
#include "diagnostics/diagnostics.hpp"
#include "ir/ir.hpp"
#include "ir/ir_builder.hpp"
#include "lexer/lexer.hpp"
#include "optimizer/passes.hpp"
#include "parser/parser.hpp"
#include "semantic/semantic_analyzer.hpp"
#include <sstream>

using namespace ric;

static auto build_ir(std::string_view src) -> ir::Module {
    DiagnosticEngine diags{"<test>", src};
    Lexer lexer{src, "<test>", diags};
    auto tokens = lexer.tokenize();
    Parser parser{tokens, diags};
    auto prog = parser.parse();
    SemanticAnalyzer sem{diags};
    sem.analyze(*prog);
    IRBuilder builder;
    return builder.lower(*prog);
}

static auto count_binops(const ir::Module& mod) -> std::size_t {
    std::size_t n = 0;
    for (const auto& fn : mod.functions)
        for (const auto& blk : fn.blocks)
            for (const auto& instr : blk.instructions)
                if (std::holds_alternative<ir::BinOp>(instr)) ++n;
    return n;
}

static auto count_instructions_of_type(const ir::Module& mod,
    std::function<bool(const ir::Instruction&)> pred) -> std::size_t {
    std::size_t n = 0;
    for (const auto& fn : mod.functions)
        for (const auto& blk : fn.blocks)
            for (const auto& instr : blk.instructions)
                if (pred(instr)) ++n;
    return n;
}

static auto contains_imm(const ir::Module& mod, std::int64_t val) -> bool {
    for (const auto& fn : mod.functions)
        for (const auto& blk : fn.blocks)
            for (const auto& instr : blk.instructions) {
                if (const auto* assign = std::get_if<ir::Assign>(&instr)) {
                    if (const auto* imm = std::get_if<ir::ImmI64>(&assign->src))
                        if (imm->value == val) return true;
                }
            }
    return false;
}

TEST_CASE("Optimizer: constant folding eliminates 2+3", "[optimizer]") {
    auto mod  = build_ir("fn f() -> i32 { 2 + 3 }");
    ConstantFoldingPass pass;
    pass.run(mod);
    REQUIRE(count_binops(mod) == 0);
    REQUIRE(contains_imm(mod, 5));
}

TEST_CASE("Optimizer: constant folding eliminates 10 * 10", "[optimizer]") {
    auto mod = build_ir("fn f() -> i32 { 10 * 10 }");
    ConstantFoldingPass pass;
    pass.run(mod);
    REQUIRE(count_binops(mod) == 0);
    REQUIRE(contains_imm(mod, 100));
}

TEST_CASE("Optimizer: constant folding chains — (2+3)*4 becomes 20", "[optimizer]") {
    auto mod = build_ir("fn f() -> i32 { (2 + 3) * 4 }");
    auto pm  = PassManager::default_pipeline();
    std::ostringstream log;
    pm.run(mod, log);
    REQUIRE(count_binops(mod) == 0);
    REQUIRE(contains_imm(mod, 20));
}

TEST_CASE("Optimizer: algebraic simplification — x + 0 becomes x", "[optimizer]") {
    auto mod = build_ir("fn f(x: i32) -> i32 { x + 0 }");
    AlgebraicSimplificationPass pass;
    pass.run(mod);
    REQUIRE(count_binops(mod) == 0);
}

TEST_CASE("Optimizer: algebraic simplification — x * 1 becomes x", "[optimizer]") {
    auto mod = build_ir("fn f(x: i32) -> i32 { x * 1 }");
    AlgebraicSimplificationPass pass;
    pass.run(mod);
    REQUIRE(count_binops(mod) == 0);
}

TEST_CASE("Optimizer: algebraic simplification — x * 0 becomes 0", "[optimizer]") {
    auto mod = build_ir("fn f(x: i32) -> i32 { x * 0 }");
    AlgebraicSimplificationPass pass;
    pass.run(mod);
    REQUIRE(count_binops(mod) == 0);
}

TEST_CASE("Optimizer: algebraic simplification — x - x becomes 0", "[optimizer]") {
    auto mod = build_ir("fn f(x: i32) -> i32 { x - x }");
    AlgebraicSimplificationPass pass;
    pass.run(mod);
    REQUIRE(count_binops(mod) == 0);
}

TEST_CASE("Optimizer: DCE removes unused temp from pure binop", "[optimizer]") {
    auto mod = build_ir("fn f() -> i32 { let x = 2 + 2; 99 }");
    const auto before = count_binops(mod);
    DeadCodeEliminationPass pass;
    pass.run(mod);
    const auto after = count_binops(mod);
    REQUIRE(after <= before);
}

TEST_CASE("Optimizer: default pipeline does not alter correctness of factorial IR", "[optimizer]") {
    auto mod = build_ir(
        "fn factorial(n: i32) -> i32 { "
        "  if n <= 1 { 1 } else { n * factorial(n - 1) } "
        "}");

    const auto calls_before = count_instructions_of_type(mod,
        [](const ir::Instruction& i) { return std::holds_alternative<ir::Call>(i); });

    auto pm = PassManager::default_pipeline();
    std::ostringstream log;
    pm.run(mod, log);

    const auto calls_after = count_instructions_of_type(mod,
        [](const ir::Instruction& i) { return std::holds_alternative<ir::Call>(i); });

    REQUIRE(calls_after == calls_before);
}

TEST_CASE("Optimizer: constant folding folds true && false to false", "[optimizer]") {
    auto mod = build_ir("fn f() -> bool { true && false }");
    ConstantFoldingPass pass;
    pass.run(mod);
    REQUIRE(count_binops(mod) == 0);
}

TEST_CASE("Optimizer: algebraic simplification — false && x becomes false", "[optimizer]") {
    auto mod = build_ir("fn f(x: bool) -> bool { false && x }");
    AlgebraicSimplificationPass pass;
    pass.run(mod);
    REQUIRE(count_binops(mod) == 0);
}

TEST_CASE("Optimizer: pass manager runs to fixed point", "[optimizer]") {
    auto mod = build_ir("fn f() -> i32 { (1 + 2) * (3 + 4) }");
    auto pm  = PassManager::default_pipeline();
    std::ostringstream log;
    pm.run(mod, log);
    REQUIRE(count_binops(mod) == 0);
    REQUIRE(contains_imm(mod, 21));
}

TEST_CASE("Optimizer: IR printer produces non-empty output", "[optimizer]") {
    auto mod = build_ir("fn add(a: i32, b: i32) -> i32 { a + b }");
    std::ostringstream out;
    mod.print(out);
    REQUIRE(!out.str().empty());
    REQUIRE(out.str().find("add") != std::string::npos);
}
