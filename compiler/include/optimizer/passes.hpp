#pragma once

#include "ir/ir.hpp"
#include <string>

namespace ric {

class OptimizationPass {
public:
    virtual ~OptimizationPass() = default;

    [[nodiscard]] virtual auto name() const noexcept -> std::string_view = 0;
    virtual auto run(ir::Module& module) -> bool = 0;
};

class ConstantFoldingPass final : public OptimizationPass {
public:
    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return "constant-folding";
    }
    auto run(ir::Module& module) -> bool override;

private:
    [[nodiscard]] static auto fold_binop(ir::BinOpKind op,
                                          std::int64_t lhs,
                                          std::int64_t rhs) -> std::optional<ir::Value>;
    [[nodiscard]] static auto as_i64(const ir::Value& v) -> std::optional<std::int64_t>;
    [[nodiscard]] static auto as_bool(const ir::Value& v)-> std::optional<bool>;
};

class AlgebraicSimplificationPass final : public OptimizationPass {
public:
    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return "algebraic-simplification";
    }
    auto run(ir::Module& module) -> bool override;

private:
    [[nodiscard]] static auto simplify(const ir::BinOp& instr)
        -> std::optional<ir::Instruction>;
};

class StrengthReductionPass final : public OptimizationPass {
public:
    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return "strength-reduction";
    }
    auto run(ir::Module& module) -> bool override;
};

class DeadCodeEliminationPass final : public OptimizationPass {
public:
    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return "dead-code-elimination";
    }
    auto run(ir::Module& module) -> bool override;

private:
    static auto run_on_function(ir::Function& fn) -> bool;
};

class ControlFlowSimplificationPass final : public OptimizationPass {
public:
    [[nodiscard]] auto name() const noexcept -> std::string_view override {
        return "cf-simplification";
    }
    auto run(ir::Module& module) -> bool override;

private:
    static auto run_on_function(ir::Function& fn) -> bool;
};

class PassManager {
public:
    void add_pass(std::unique_ptr<OptimizationPass> pass);
    auto run(ir::Module& module, std::ostream& log) -> std::size_t;

    [[nodiscard]] static auto default_pipeline() -> PassManager;

private:
    std::vector<std::unique_ptr<OptimizationPass>> passes_;
};

}
