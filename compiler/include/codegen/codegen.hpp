#pragma once

#include "ir/ir.hpp"
#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ric {

struct LiveInterval {
    ir::TempId   id;
    std::uint32_t start;
    std::uint32_t end;
};

struct RegisterAssignment {
    enum class Kind { Register, Spill };
    Kind kind;
    std::string reg;
    std::int32_t spill_offset{0};
};

class LinearScanAllocator {
public:
    explicit LinearScanAllocator(std::uint32_t num_temps);

    auto allocate(const std::vector<LiveInterval>& intervals)
        -> std::unordered_map<ir::TempId, RegisterAssignment>;

private:
    std::uint32_t                           num_temps_;
    std::int32_t                            next_spill_offset_{-8};
    std::vector<std::string>                free_regs_;
    std::vector<std::pair<ir::TempId, std::string>> active_;

    void expire_old(const LiveInterval& current,
                    std::unordered_map<ir::TempId, RegisterAssignment>& result);
    void spill_at_interval(const LiveInterval& current,
                           std::unordered_map<ir::TempId, RegisterAssignment>& result);
    [[nodiscard]] auto next_spill() -> std::int32_t;
};

class CodeGenerator {
public:
    explicit CodeGenerator(std::ostream& out);

    void generate(const ir::Module& module);

private:
    std::ostream&           out_;
    const ir::Function*     current_fn_{nullptr};
    std::string             current_fn_name_;

    struct FrameLayout {
        std::unordered_map<std::string, std::int32_t> var_slots;
        std::unordered_map<ir::TempId,  std::int32_t> temp_slots;
        std::int32_t                                   frame_size{0};

        [[nodiscard]] auto var_offset(const std::string& name) const -> std::int32_t;
        [[nodiscard]] auto temp_offset(ir::TempId id)          const -> std::int32_t;
    };

    FrameLayout current_frame_;

    void emit_file_preamble(const ir::Module& module);
    void emit_function(const ir::Function& fn);
    void emit_block(const ir::BasicBlock& blk);
    void emit_instruction(const ir::Instruction& instr);

    [[nodiscard]] auto build_frame(const ir::Function& fn) const -> FrameLayout;

    void emit_load(const ir::Value& src, std::string_view reg);
    void emit_store_temp(ir::TempId dst, std::string_view reg);

    [[nodiscard]] auto block_label(ir::LabelId id) const -> std::string;
    [[nodiscard]] auto var_ref(const std::string& name)  const -> std::string;
    [[nodiscard]] auto temp_ref(ir::TempId id)           const -> std::string;
};

}
