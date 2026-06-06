#include "optimizer/passes.hpp"
#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

namespace ric {

static auto is_power_of_two(std::int64_t n) -> bool {
    return n > 0 && (n & (n - 1)) == 0;
}

static auto log2_of(std::int64_t n) -> int {
    int count = 0;
    while (n > 1) { n >>= 1; ++count; }
    return count;
}

auto ConstantFoldingPass::as_i64(const ir::Value& v) -> std::optional<std::int64_t> {
    if (const auto* imm = std::get_if<ir::ImmI64>(&v)) return imm->value;
    if (const auto* imm = std::get_if<ir::ImmBool>(&v)) return imm->value ? 1 : 0;
    return std::nullopt;
}

auto ConstantFoldingPass::as_bool(const ir::Value& v) -> std::optional<bool> {
    if (const auto* imm = std::get_if<ir::ImmBool>(&v)) return imm->value;
    if (const auto* imm = std::get_if<ir::ImmI64>(&v))  return imm->value != 0;
    return std::nullopt;
}

auto ConstantFoldingPass::fold_binop(ir::BinOpKind op,
                                      std::int64_t lhs,
                                      std::int64_t rhs) -> std::optional<ir::Value> {
    switch (op) {
        case ir::BinOpKind::Add: return ir::ImmI64{lhs + rhs};
        case ir::BinOpKind::Sub: return ir::ImmI64{lhs - rhs};
        case ir::BinOpKind::Mul: return ir::ImmI64{lhs * rhs};
        case ir::BinOpKind::Div:
            if (rhs == 0) return std::nullopt;
            return ir::ImmI64{lhs / rhs};
        case ir::BinOpKind::Rem:
            if (rhs == 0) return std::nullopt;
            return ir::ImmI64{lhs % rhs};
        case ir::BinOpKind::Eq:  return ir::ImmBool{lhs == rhs};
        case ir::BinOpKind::Ne:  return ir::ImmBool{lhs != rhs};
        case ir::BinOpKind::Lt:  return ir::ImmBool{lhs < rhs};
        case ir::BinOpKind::Le:  return ir::ImmBool{lhs <= rhs};
        case ir::BinOpKind::Gt:  return ir::ImmBool{lhs > rhs};
        case ir::BinOpKind::Ge:  return ir::ImmBool{lhs >= rhs};
        case ir::BinOpKind::And: return ir::ImmBool{lhs != 0 && rhs != 0};
        case ir::BinOpKind::Or:  return ir::ImmBool{lhs != 0 || rhs != 0};
    }
    return std::nullopt;
}

auto ConstantFoldingPass::run(ir::Module& module) -> bool {
    bool changed = false;

    for (auto& fn : module.functions) {
        std::unordered_map<ir::TempId, ir::Value> const_map;

        for (auto& blk : fn.blocks) {
            for (auto& instr : blk.instructions) {
                auto resolve = [&](const ir::Value& v) -> ir::Value {
                    if (const auto* ref = std::get_if<ir::TempRef>(&v)) {
                        auto it = const_map.find(ref->id);
                        if (it != const_map.end()) return it->second;
                    }
                    return v;
                };

                if (auto* assign = std::get_if<ir::Assign>(&instr)) {
                    const auto resolved = resolve(assign->src);
                    if (!std::holds_alternative<ir::TempRef>(resolved)) {
                        const_map[assign->dst] = resolved;
                        instr   = ir::Assign{.dst = assign->dst, .src = resolved};
                        changed = true;
                    }
                } else if (auto* binop = std::get_if<ir::BinOp>(&instr)) {
                    const auto l = resolve(binop->lhs);
                    const auto r = resolve(binop->rhs);

                    const auto li = as_i64(l);
                    const auto ri = as_i64(r);

                    if (li && ri) {
                        if (auto folded = fold_binop(binop->op, *li, *ri)) {
                            const_map[binop->dst] = *folded;
                            instr   = ir::Assign{.dst = binop->dst, .src = *folded};
                            changed = true;
                            continue;
                        }
                    }

                    binop->lhs = l;
                    binop->rhs = r;
                } else if (auto* unop = std::get_if<ir::UnOp>(&instr)) {
                    const auto s = resolve(unop->src);
                    unop->src   = s;

                    if (auto si = as_i64(s)) {
                        ir::Value folded;
                        if (unop->op == ir::UnOpKind::Neg) {
                            folded = ir::ImmI64{-(*si)};
                        } else {
                            folded = ir::ImmBool{*si == 0};
                        }
                        const_map[unop->dst] = folded;
                        instr   = ir::Assign{.dst = unop->dst, .src = folded};
                        changed = true;
                    }
                } else if (auto* br = std::get_if<ir::Branch>(&instr)) {
                    br->cond = resolve(br->cond);
                    if (auto bv = as_bool(br->cond)) {
                        instr   = ir::Jump{.target = *bv ? br->true_label : br->false_label};
                        changed = true;
                    }
                } else if (auto* ret = std::get_if<ir::Return>(&instr)) {
                    if (ret->value) ret->value = resolve(*ret->value);
                } else if (auto* st = std::get_if<ir::StoreLocal>(&instr)) {
                    st->src = resolve(st->src);
                }
            }
        }
    }

    return changed;
}

auto AlgebraicSimplificationPass::simplify(const ir::BinOp& b) -> std::optional<ir::Instruction> {
    const auto* li = std::get_if<ir::ImmI64>(&b.lhs);
    const auto* ri = std::get_if<ir::ImmI64>(&b.rhs);

    switch (b.op) {
        case ir::BinOpKind::Add:
            if (ri && ri->value == 0) return ir::Assign{b.dst, b.lhs};
            if (li && li->value == 0) return ir::Assign{b.dst, b.rhs};
            break;
        case ir::BinOpKind::Sub:
            if (ri && ri->value == 0) return ir::Assign{b.dst, b.lhs};
            if (const auto* lr = std::get_if<ir::TempRef>(&b.lhs)) {
                if (const auto* rr = std::get_if<ir::TempRef>(&b.rhs)) {
                    if (lr->id == rr->id) return ir::Assign{b.dst, ir::ImmI64{0}};
                }
            }
            break;
        case ir::BinOpKind::Mul:
            if (li && li->value == 1) return ir::Assign{b.dst, b.rhs};
            if (ri && ri->value == 1) return ir::Assign{b.dst, b.lhs};
            if ((li && li->value == 0) || (ri && ri->value == 0))
                return ir::Assign{b.dst, ir::ImmI64{0}};
            break;
        case ir::BinOpKind::Div:
            if (ri && ri->value == 1) return ir::Assign{b.dst, b.lhs};
            break;
        case ir::BinOpKind::And:
            if (const auto* lb = std::get_if<ir::ImmBool>(&b.lhs)) {
                if (!lb->value) return ir::Assign{b.dst, ir::ImmBool{false}};
                return ir::Assign{b.dst, b.rhs};
            }
            if (const auto* rb = std::get_if<ir::ImmBool>(&b.rhs)) {
                if (!rb->value) return ir::Assign{b.dst, ir::ImmBool{false}};
                return ir::Assign{b.dst, b.lhs};
            }
            break;
        case ir::BinOpKind::Or:
            if (const auto* lb = std::get_if<ir::ImmBool>(&b.lhs)) {
                if (lb->value) return ir::Assign{b.dst, ir::ImmBool{true}};
                return ir::Assign{b.dst, b.rhs};
            }
            if (const auto* rb = std::get_if<ir::ImmBool>(&b.rhs)) {
                if (rb->value) return ir::Assign{b.dst, ir::ImmBool{true}};
                return ir::Assign{b.dst, b.lhs};
            }
            break;
        default: break;
    }
    return std::nullopt;
}

auto AlgebraicSimplificationPass::run(ir::Module& module) -> bool {
    bool changed = false;
    for (auto& fn : module.functions) {
        for (auto& blk : fn.blocks) {
            for (auto& instr : blk.instructions) {
                if (const auto* binop = std::get_if<ir::BinOp>(&instr)) {
                    if (auto simplified = simplify(*binop)) {
                        instr   = std::move(*simplified);
                        changed = true;
                    }
                }
            }
        }
    }
    return changed;
}

auto StrengthReductionPass::run(ir::Module& module) -> bool {
    bool changed = false;
    for (auto& fn : module.functions) {
        for (auto& blk : fn.blocks) {
            for (auto& instr : blk.instructions) {
                if (auto* binop = std::get_if<ir::BinOp>(&instr)) {
                    if (binop->op == ir::BinOpKind::Mul) {
                        const auto* ri = std::get_if<ir::ImmI64>(&binop->rhs);
                        const auto* li = std::get_if<ir::ImmI64>(&binop->lhs);
                        const auto* const_side = ri ? ri : li;
                        const auto& var_side   = ri ? binop->lhs : binop->rhs;

                        if (const_side && is_power_of_two(const_side->value)) {
                            const auto shift = log2_of(const_side->value);
                            instr   = ir::BinOp{
                                .dst = binop->dst,
                                .op  = ir::BinOpKind::Add,
                                .lhs = var_side,
                                .rhs = ir::ImmI64{shift},
                            };
                            changed = true;
                        }
                    }
                }
            }
        }
    }
    return changed;
}

auto DeadCodeEliminationPass::run_on_function(ir::Function& fn) -> bool {
    std::unordered_set<ir::TempId> used_temps;

    auto mark_used = [&](const ir::Value& v) {
        if (const auto* ref = std::get_if<ir::TempRef>(&v)) {
            used_temps.insert(ref->id);
        }
    };

    for (const auto& blk : fn.blocks) {
        for (const auto& instr : blk.instructions) {
            std::visit([&](const auto& i) {
                using T = std::decay_t<decltype(i)>;
                if constexpr (std::is_same_v<T, ir::BinOp>) {
                    mark_used(i.lhs); mark_used(i.rhs);
                } else if constexpr (std::is_same_v<T, ir::UnOp>) {
                    mark_used(i.src);
                } else if constexpr (std::is_same_v<T, ir::Assign>) {
                    mark_used(i.src);
                } else if constexpr (std::is_same_v<T, ir::Call>) {
                    for (const auto& a : i.args) mark_used(a);
                } else if constexpr (std::is_same_v<T, ir::Branch>) {
                    mark_used(i.cond);
                } else if constexpr (std::is_same_v<T, ir::Return>) {
                    if (i.value) mark_used(*i.value);
                } else if constexpr (std::is_same_v<T, ir::StoreLocal>) {
                    mark_used(i.src);
                }
            }, instr);
        }
    }

    bool changed = false;
    for (auto& blk : fn.blocks) {
        auto new_instrs = std::vector<ir::Instruction>{};
        new_instrs.reserve(blk.instructions.size());

        for (auto& instr : blk.instructions) {
            bool keep = true;
            std::visit([&](const auto& i) {
                using T = std::decay_t<decltype(i)>;
                if constexpr (std::is_same_v<T, ir::Assign>
                           || std::is_same_v<T, ir::BinOp>
                           || std::is_same_v<T, ir::UnOp>) {
                    if (!used_temps.count(i.dst)) {
                        keep    = false;
                        changed = true;
                    }
                }
            }, instr);

            if (keep) new_instrs.push_back(std::move(instr));
        }

        blk.instructions = std::move(new_instrs);
    }

    return changed;
}

auto DeadCodeEliminationPass::run(ir::Module& module) -> bool {
    bool changed = false;
    for (auto& fn : module.functions)
        changed |= run_on_function(fn);
    return changed;
}

auto ControlFlowSimplificationPass::run_on_function(ir::Function& fn) -> bool {
    bool changed = false;

    for (auto& blk : fn.blocks) {
        if (blk.instructions.size() != 1) continue;

        const auto* jmp = std::get_if<ir::Jump>(&blk.instructions.back());
        if (!jmp) continue;
        if (jmp->target == blk.id) continue;

        const auto bypass_target = jmp->target;
        for (auto& other : fn.blocks) {
            if (other.id == blk.id) continue;
            for (auto& other_instr : other.instructions) {
                if (auto* other_br = std::get_if<ir::Branch>(&other_instr)) {
                    if (other_br->true_label  == blk.id) {
                        other_br->true_label  = bypass_target; changed = true;
                    }
                    if (other_br->false_label == blk.id) {
                        other_br->false_label = bypass_target; changed = true;
                    }
                } else if (auto* other_jmp = std::get_if<ir::Jump>(&other_instr)) {
                    if (other_jmp->target == blk.id) {
                        other_jmp->target = bypass_target; changed = true;
                    }
                }
            }
        }
    }

    return changed;
}

auto ControlFlowSimplificationPass::run(ir::Module& module) -> bool {
    bool changed = false;
    for (auto& fn : module.functions)
        changed |= run_on_function(fn);
    return changed;
}

void PassManager::add_pass(std::unique_ptr<OptimizationPass> pass) {
    passes_.push_back(std::move(pass));
}

auto PassManager::run(ir::Module& module, std::ostream& log) -> std::size_t {
    std::size_t total_iterations = 0;

    bool any_changed = true;
    while (any_changed) {
        any_changed = false;
        for (auto& pass : passes_) {
            const bool changed = pass->run(module);
            if (changed) {
                log << "[opt] " << pass->name() << " made changes\n";
                any_changed = true;
                ++total_iterations;
            }
        }
    }

    return total_iterations;
}

auto PassManager::default_pipeline() -> PassManager {
    PassManager pm;
    pm.add_pass(std::make_unique<ConstantFoldingPass>());
    pm.add_pass(std::make_unique<AlgebraicSimplificationPass>());
    pm.add_pass(std::make_unique<StrengthReductionPass>());
    pm.add_pass(std::make_unique<DeadCodeEliminationPass>());
    pm.add_pass(std::make_unique<ControlFlowSimplificationPass>());
    return pm;
}

}
