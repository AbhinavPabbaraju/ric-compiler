#include "driver/driver.hpp"
#include "codegen/codegen.hpp"
#include "diagnostics/diagnostics.hpp"
#include "ir/ir_builder.hpp"
#include "lexer/lexer.hpp"
#include "optimizer/passes.hpp"
#include "parser/parser.hpp"
#include "semantic/semantic_analyzer.hpp"
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ric {

namespace {

auto read_file(const std::filesystem::path& path) -> std::optional<std::string> {
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file.is_open()) return std::nullopt;

    const auto size = file.tellg();
    file.seekg(0);
    std::string content(static_cast<std::size_t>(size), '\0');
    file.read(content.data(), size);
    return content;
}

auto run_subprocess(const std::string& cmd) -> int {
    return std::system(cmd.c_str());
}

}

auto result_to_string(CompileResult result) noexcept -> std::string_view {
    switch (result) {
        case CompileResult::Success:       return "success";
        case CompileResult::LexError:      return "lexical error";
        case CompileResult::ParseError:    return "parse error";
        case CompileResult::SemanticError: return "semantic error";
        case CompileResult::CodegenError:  return "codegen error";
        case CompileResult::LinkError:     return "link error";
    }
    return "unknown";
}

auto compile(const CompilerOptions& opts) -> CompileResult {
    const auto source = read_file(opts.input_path);
    if (!source) {
        std::cerr << "error: cannot open input file '"
                  << opts.input_path.string() << "'\n";
        return CompileResult::CodegenError;
    }

    const auto filename = opts.input_path.filename().string();
    DiagnosticEngine diags{filename, *source};

    std::cerr << "\033[1m[1/5] Lexing\033[0m  " << filename << "\n";
    Lexer lexer{*source, filename, diags};
    auto tokens = lexer.tokenize();

    if (opts.emit_tokens) {
        for (const auto& tok : tokens) {
            std::cout << std::format("  {:20} {:4}:{:3}  {}\n",
                std::string(token_kind_name(tok.kind)),
                tok.location.line,
                tok.location.column,
                tok.lexeme);
        }
    }

    if (diags.has_errors()) {
        diags.print_all(std::cerr);
        return CompileResult::LexError;
    }

    std::cerr << "\033[1m[2/5] Parsing\033[0m\n";
    Parser parser{tokens, diags};
    auto program = parser.parse();

    if (diags.has_errors()) {
        diags.print_all(std::cerr);
        return CompileResult::ParseError;
    }

    std::cerr << "\033[1m[3/5] Semantic analysis\033[0m\n";
    SemanticAnalyzer analyzer{diags};
    analyzer.analyze(*program);

    if (diags.has_errors()) {
        diags.print_all(std::cerr);
        return CompileResult::SemanticError;
    }

    std::cerr << "\033[1m[4/5] IR lowering + optimization\033[0m\n";
    IRBuilder builder;
    auto module = builder.lower(*program);
    module.source_file = opts.input_path.string();

    if (opts.emit_ir) {
        std::cout << "\n--- IR (before optimization) ---\n";
        module.print(std::cout);
        std::cout << "--------------------------------\n\n";
    }

    if (opts.opt_level > 0) {
        auto pm = PassManager::default_pipeline();
        std::ostringstream opt_log;
        const auto passes_run = pm.run(module, opt_log);
        if (passes_run > 0)
            std::cerr << "    " << opt_log.str().size() << " bytes of optimization log\n";
    }

    if (opts.emit_opt_ir) {
        std::cout << "\n--- IR (after optimization) ---\n";
        module.print(std::cout);
        std::cout << "-------------------------------\n\n";
    }

    std::cerr << "\033[1m[5/5] Code generation\033[0m\n";
    const auto asm_path  = opts.output_path.parent_path() / (opts.output_path.stem().string() + ".asm");
    const auto obj_path  = opts.output_path.parent_path() / (opts.output_path.stem().string() + ".o");
    const auto rt_asm_path = opts.output_path.parent_path() / "_ric_runtime.asm";
    const auto rt_obj_path = opts.output_path.parent_path() / "_ric_runtime.o";

    {
        std::ofstream asm_out{asm_path};
        if (!asm_out.is_open()) {
            std::cerr << "error: cannot write to '" << asm_path.string() << "'\n";
            return CompileResult::CodegenError;
        }
        CodeGenerator codegen{asm_out};
        codegen.generate(module);
    }

    if (opts.emit_asm) {
        std::ifstream asm_in{asm_path};
        std::cout << "\n--- Assembly ---\n";
        std::cout << asm_in.rdbuf();
        std::cout << "----------------\n\n";
    }

    if (opts.no_assemble) return CompileResult::Success;

    {
        std::ofstream rt_out{rt_asm_path};
        rt_out << R"(
; ric runtime — minimal entry point for Linux x86-64
bits 64
section .text
global _start
extern main

_start:
    xor rbp, rbp
    call main
    mov rdi, rax
    mov rax, 60
    syscall
)";
    }

    const auto nasm_cmd = std::format("nasm -f elf64 -o {} {}",
        obj_path.string(), asm_path.string());
    const auto nasm_rt_cmd = std::format("nasm -f elf64 -o {} {}",
        rt_obj_path.string(), rt_asm_path.string());

    if (run_subprocess(nasm_cmd) != 0) {
        std::cerr << "error: NASM assembly failed\n";
        return CompileResult::LinkError;
    }
    if (run_subprocess(nasm_rt_cmd) != 0) {
        std::cerr << "error: NASM runtime assembly failed\n";
        return CompileResult::LinkError;
    }

    if (opts.no_link) return CompileResult::Success;

    const auto ld_cmd = std::format("ld -o {} {} {}",
        opts.output_path.string(),
        obj_path.string(),
        rt_obj_path.string());

    if (run_subprocess(ld_cmd) != 0) {
        std::cerr << "error: linking failed\n";
        return CompileResult::LinkError;
    }

    std::filesystem::remove(rt_asm_path);
    std::filesystem::remove(rt_obj_path);
    if (!opts.emit_asm) std::filesystem::remove(asm_path);
    std::filesystem::remove(obj_path);

    std::cerr << "\033[1;32m    Compiled\033[0m  "
              << opts.output_path.string() << "\n";
    return CompileResult::Success;
}

}
