#pragma once

#include <filesystem>
#include <string>

namespace ric {

struct CompilerOptions {
    std::filesystem::path input_path;
    std::filesystem::path output_path{"a.out"};

    bool emit_tokens{false};
    bool emit_ir{false};
    bool emit_opt_ir{false};
    bool emit_asm{false};
    bool no_link{false};
    bool no_assemble{false};
    int  opt_level{1};
};

enum class CompileResult { Success, LexError, ParseError, SemanticError, CodegenError, LinkError };

[[nodiscard]] auto compile(const CompilerOptions& opts) -> CompileResult;
[[nodiscard]] auto result_to_string(CompileResult result) noexcept -> std::string_view;

}
