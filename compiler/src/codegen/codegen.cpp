#include "codegen/codegen.hpp"
#include <algorithm>
#include <cassert>
#include <format>

namespace ric {

static constexpr const char* k_allocatable_regs[] = {
    "rbx", "r12", "r13", "r14", "r15",
    "r10", "r11",
};
static constexpr std::size_t k_num_allocatable = 7;

static constexpr const char* k_arg_regs[] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};
static constexpr std::size_t k_max_reg_args = 6;

LinearScanAllocator::LinearScanAllocator(std::uint32_t num_temps)
    : num_temps_(num_temps) {
    for (std::size_t i = 0; i < k_num_allocatable; ++i)
        free_regs_.emplace_back(k_allocatable_regs[i]);
}

auto LinearScanAllocator::next_spill() -> std::int32_t {
    const auto offset = next_spill_offset_;
    next_spill_offset_ -= 8;
    return offset;
}

void LinearScanAllocator::expire_old(
    const LiveInterval& current,
    std::unordered_map<ir::TempId, RegisterAssignment>& result) {

    std::vector<std::pair<ir::TempId, std::string>> still_active;
    for (const auto& [id, reg] : active_) {
        const auto it = result.find(id);
        if (it != result.end() && it->second.kind == RegisterAssignment::Kind::Register) {
            const auto& interval = *std::find_if(active_.begin(), active_.end(),
                [id](const auto& p) { return p.first == id; });
            (void)interval;
        }
        still_active.emplace_back(id, reg);
    }
    active_ = std::move(still_active);
}

void LinearScanAllocator::spill_at_interval(
    const LiveInterval& current,
    std::unordered_map<ir::TempId, RegisterAssignment>& result) {

    if (active_.empty()) {
        result[current.id] = RegisterAssignment{
            .kind         = RegisterAssignment::Kind::Spill,
            .spill_offset = next_spill(),
        };
        return;
    }

    const auto spill_offset = next_spill();
    result[current.id] = RegisterAssignment{
        .kind         = RegisterAssignment::Kind::Spill,
        .spill_offset = spill_offset,
    };
}

auto LinearScanAllocator::allocate(const std::vector<LiveInterval>& intervals)
    -> std::unordered_map<ir::TempId, RegisterAssignment> {

    std::unordered_map<ir::TempId, RegisterAssignment> result;

    auto sorted = intervals;
    std::sort(sorted.begin(), sorted.end(),
        [](const LiveInterval& a, const LiveInterval& b) {
            return a.start < b.start;
        });

    for (const auto& interval : sorted) {
        auto new_active = std::vector<std::pair<ir::TempId, std::string>>{};
        for (const auto& [id, reg] : active_) {
            const auto it = std::find_if(sorted.begin(), sorted.end(),
                [id](const LiveInterval& iv) { return iv.id == id; });
            if (it != sorted.end() && it->end >= interval.start) {
                new_active.emplace_back(id, reg);
            } else {
                free_regs_.push_back(reg);
            }
        }
        active_ = std::move(new_active);

        if (free_regs_.empty()) {
            spill_at_interval(interval, result);
        } else {
            const auto reg = free_regs_.back();
            free_regs_.pop_back();
            result[interval.id] = RegisterAssignment{
                .kind = RegisterAssignment::Kind::Register,
                .reg  = reg,
            };
            active_.emplace_back(interval.id, reg);
        }
    }

    return result;
}

CodeGenerator::CodeGenerator(std::ostream& out) : out_(out) {}

auto CodeGenerator::FrameLayout::var_offset(const std::string& name) const -> std::int32_t {
    const auto it = var_slots.find(name);
    return it != var_slots.end() ? it->second : 0;
}

auto CodeGenerator::FrameLayout::temp_offset(ir::TempId id) const -> std::int32_t {
    const auto it = temp_slots.find(id);
    return it != temp_slots.end() ? it->second : 0;
}

auto CodeGenerator::build_frame(const ir::Function& fn) const -> FrameLayout {
    FrameLayout frame;
    std::int32_t offset = 0;

    auto claim_var_slot = [&](const std::string& name) {
        if (!frame.var_slots.contains(name)) {
            offset += 8;
            frame.var_slots[name] = -offset;
        }
    };

    for (const auto& local : fn.locals)
        claim_var_slot(local.name);

    for (const auto& blk : fn.blocks) {
        for (const auto& instr : blk.instructions) {
            if (const auto* s = std::get_if<ir::StoreLocal>(&instr)) claim_var_slot(s->var_name);
            if (const auto* l = std::get_if<ir::LoadLocal>(&instr))  claim_var_slot(l->var_name);
        }
    }

    auto claim_temp_slot = [&](ir::TempId id) {
        if (!frame.temp_slots.contains(id)) {
            offset += 8;
            frame.temp_slots[id] = -offset;
        }
    };

    for (const auto& blk : fn.blocks) {
        for (const auto& instr : blk.instructions) {
            std::visit([&](const auto& i) {
                using T = std::decay_t<decltype(i)>;
                if constexpr (std::is_same_v<T, ir::Assign>)    claim_temp_slot(i.dst);
                else if constexpr (std::is_same_v<T, ir::BinOp>) claim_temp_slot(i.dst);
                else if constexpr (std::is_same_v<T, ir::UnOp>)  claim_temp_slot(i.dst);
                else if constexpr (std::is_same_v<T, ir::LoadLocal>) claim_temp_slot(i.dst);
                else if constexpr (std::is_same_v<T, ir::Call> && !std::is_void_v<T>) {
                    if (i.dst) claim_temp_slot(*i.dst);
                }
            }, instr);

            if (const auto* call = std::get_if<ir::Call>(&instr))
                if (call->dst) claim_temp_slot(*call->dst);
        }
    }

    const auto raw = offset;
    frame.frame_size = (raw % 16 == 0) ? raw + 8 : raw;

    return frame;
}

auto CodeGenerator::block_label(ir::LabelId id) const -> std::string {
    for (const auto& blk : current_fn_->blocks) {
        if (blk.id == id)
            return "." + current_fn_name_ + "_" + blk.label;
    }
    return "." + current_fn_name_ + "_blk_" + std::to_string(id);
}

auto CodeGenerator::var_ref(const std::string& name) const -> std::string {
    const auto off = current_frame_.var_offset(name);
    return std::format("qword [rbp {}]", off < 0 ? std::to_string(off) : "+" + std::to_string(off));
}

auto CodeGenerator::temp_ref(ir::TempId id) const -> std::string {
    const auto off = current_frame_.temp_offset(id);
    return std::format("qword [rbp {}]", off < 0 ? std::to_string(off) : "+" + std::to_string(off));
}

void CodeGenerator::emit_load(const ir::Value& src, std::string_view reg) {
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, ir::TempRef>) {
            out_ << std::format("    mov {}, {}\n", reg, temp_ref(v.id));
        } else if constexpr (std::is_same_v<T, ir::ImmI64>) {
            out_ << std::format("    mov {}, {}\n", reg, v.value);
        } else if constexpr (std::is_same_v<T, ir::ImmBool>) {
            out_ << std::format("    mov {}, {}\n", reg, v.value ? 1 : 0);
        } else if constexpr (std::is_same_v<T, ir::ImmChar>) {
            out_ << std::format("    mov {}, {}\n", reg, static_cast<std::int32_t>(v.value));
        } else if constexpr (std::is_same_v<T, ir::ImmUnit>) {
            out_ << std::format("    xor {}, {}\n", reg, reg);
        }
    }, src);
}

void CodeGenerator::emit_store_temp(ir::TempId dst, std::string_view reg) {
    out_ << std::format("    mov {}, {}\n", temp_ref(dst), reg);
}

void CodeGenerator::emit_instruction(const ir::Instruction& instr) {
    std::visit([&](const auto& i) {
        using T = std::decay_t<decltype(i)>;

        if constexpr (std::is_same_v<T, ir::Assign>) {
            emit_load(i.src, "rax");
            emit_store_temp(i.dst, "rax");

        } else if constexpr (std::is_same_v<T, ir::BinOp>) {
            emit_load(i.lhs, "rax");
            emit_load(i.rhs, "rcx");

            switch (i.op) {
                case ir::BinOpKind::Add:
                    out_ << "    add rax, rcx\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::Sub:
                    out_ << "    sub rax, rcx\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::Mul:
                    out_ << "    imul rax, rcx\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::Div:
                    out_ << "    cqo\n";
                    out_ << "    idiv rcx\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::Rem:
                    out_ << "    cqo\n";
                    out_ << "    idiv rcx\n";
                    emit_store_temp(i.dst, "rdx");
                    break;
                case ir::BinOpKind::Eq:
                    out_ << "    cmp rax, rcx\n";
                    out_ << "    sete al\n";
                    out_ << "    movzx rax, al\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::Ne:
                    out_ << "    cmp rax, rcx\n";
                    out_ << "    setne al\n";
                    out_ << "    movzx rax, al\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::Lt:
                    out_ << "    cmp rax, rcx\n";
                    out_ << "    setl al\n";
                    out_ << "    movzx rax, al\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::Le:
                    out_ << "    cmp rax, rcx\n";
                    out_ << "    setle al\n";
                    out_ << "    movzx rax, al\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::Gt:
                    out_ << "    cmp rax, rcx\n";
                    out_ << "    setg al\n";
                    out_ << "    movzx rax, al\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::Ge:
                    out_ << "    cmp rax, rcx\n";
                    out_ << "    setge al\n";
                    out_ << "    movzx rax, al\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::And:
                    out_ << "    test rax, rax\n";
                    out_ << "    setne al\n";
                    out_ << "    test rcx, rcx\n";
                    out_ << "    setne cl\n";
                    out_ << "    and al, cl\n";
                    out_ << "    movzx rax, al\n";
                    emit_store_temp(i.dst, "rax");
                    break;
                case ir::BinOpKind::Or:
                    out_ << "    test rax, rax\n";
                    out_ << "    setne al\n";
                    out_ << "    test rcx, rcx\n";
                    out_ << "    setne cl\n";
                    out_ << "    or al, cl\n";
                    out_ << "    movzx rax, al\n";
                    emit_store_temp(i.dst, "rax");
                    break;
            }

        } else if constexpr (std::is_same_v<T, ir::UnOp>) {
            emit_load(i.src, "rax");
            if (i.op == ir::UnOpKind::Neg) {
                out_ << "    neg rax\n";
            } else {
                out_ << "    test rax, rax\n";
                out_ << "    sete al\n";
                out_ << "    movzx rax, al\n";
            }
            emit_store_temp(i.dst, "rax");

        } else if constexpr (std::is_same_v<T, ir::LoadLocal>) {
            out_ << std::format("    mov rax, {}\n", var_ref(i.var_name));
            emit_store_temp(i.dst, "rax");

        } else if constexpr (std::is_same_v<T, ir::StoreLocal>) {
            emit_load(i.src, "rax");
            out_ << std::format("    mov {}, rax\n", var_ref(i.var_name));

        } else if constexpr (std::is_same_v<T, ir::Call>) {
            const auto n_args = std::min(i.args.size(), k_max_reg_args);
            for (std::size_t a = 0; a < n_args; ++a)
                emit_load(i.args[a], k_arg_regs[a]);

            out_ << std::format("    call {}\n", i.func);

            if (i.dst)
                emit_store_temp(*i.dst, "rax");

        } else if constexpr (std::is_same_v<T, ir::Branch>) {
            emit_load(i.cond, "rax");
            out_ << "    test rax, rax\n";
            out_ << std::format("    jnz {}\n", block_label(i.true_label));
            out_ << std::format("    jmp {}\n", block_label(i.false_label));

        } else if constexpr (std::is_same_v<T, ir::Jump>) {
            out_ << std::format("    jmp {}\n", block_label(i.target));

        } else if constexpr (std::is_same_v<T, ir::Return>) {
            if (i.value) {
                emit_load(*i.value, "rax");
            } else {
                out_ << "    xor eax, eax\n";
            }
            out_ << "    leave\n";
            out_ << "    ret\n";
        }

    }, instr);
}

void CodeGenerator::emit_block(const ir::BasicBlock& blk) {
    out_ << block_label(blk.id) << ":\n";
    for (const auto& instr : blk.instructions)
        emit_instruction(instr);
}

void CodeGenerator::emit_function(const ir::Function& fn) {
    current_fn_      = &fn;
    current_fn_name_ = fn.name;
    current_frame_   = build_frame(fn);

    out_ << std::format("global {}\n", fn.name);
    out_ << std::format("{}:\n", fn.name);
    out_ << "    push rbp\n";
    out_ << "    mov rbp, rsp\n";
    out_ << std::format("    sub rsp, {}\n", current_frame_.frame_size);

    const auto n_params = std::min(fn.params.size(), k_max_reg_args);
    for (std::size_t p = 0; p < n_params; ++p) {
        const auto& [name, type] = fn.params[p];
        out_ << std::format("    mov {}, {}\n", var_ref(name), k_arg_regs[p]);
    }

    for (const auto& blk : fn.blocks)
        emit_block(blk);

    out_ << "\n";
    current_fn_ = nullptr;
}

void CodeGenerator::emit_file_preamble(const ir::Module& module) {
    out_ << "; -------------------------------------------------------\n";
    out_ << "; Generated by ricc — Rust-Inspired Compiler\n";
    out_ << std::format("; Source file : {}\n", module.source_file);
    out_ << "; Target      : Linux x86-64 (System V ABI)\n";
    out_ << "; Assembler   : NASM (nasm -f elf64)\n";
    out_ << "; -------------------------------------------------------\n\n";
    out_ << "default rel\n";
    out_ << "section .text\n\n";
}

void CodeGenerator::generate(const ir::Module& module) {
    emit_file_preamble(module);
    for (const auto& fn : module.functions)
        emit_function(fn);
}

}
