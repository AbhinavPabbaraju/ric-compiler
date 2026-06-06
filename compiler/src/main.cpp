#include "driver/driver.hpp"
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

void print_usage(std::string_view program_name) {
    std::cout << std::format(R"(
ricc — Rust-Inspired Compiler v0.1.0

USAGE:
    {} [OPTIONS] <input.ric>

OPTIONS:
    -o <file>       Write executable to <file>  [default: a.out]
    --emit-tokens   Dump token stream to stdout
    --emit-ir       Dump IR before optimization
    --emit-opt-ir   Dump IR after optimization
    --emit-asm      Dump generated assembly
    --no-link       Stop after assembly (.o only)
    --no-assemble   Stop after code generation (.asm only)
    -O0             Disable optimizations
    -O1             Enable standard optimizations  [default]
    -h, --help      Print this help message
)", program_name);
}

}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    ric::CompilerOptions opts;
    std::vector<std::string_view> positional;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (arg == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "error: `-o` requires an argument\n";
                return EXIT_FAILURE;
            }
            opts.output_path = argv[++i];
        } else if (arg.starts_with("-o")) {
            opts.output_path = arg.substr(2);
        } else if (arg == "--emit-tokens") {
            opts.emit_tokens = true;
        } else if (arg == "--emit-ir") {
            opts.emit_ir = true;
        } else if (arg == "--emit-opt-ir") {
            opts.emit_opt_ir = true;
        } else if (arg == "--emit-asm") {
            opts.emit_asm = true;
        } else if (arg == "--no-link") {
            opts.no_link = true;
        } else if (arg == "--no-assemble") {
            opts.no_assemble = true;
        } else if (arg == "-O0") {
            opts.opt_level = 0;
        } else if (arg == "-O1" || arg == "-O2") {
            opts.opt_level = 1;
        } else if (arg.starts_with('-')) {
            std::cerr << std::format("error: unknown option '{}'\n", arg);
            return EXIT_FAILURE;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.empty()) {
        std::cerr << "error: no input file specified\n";
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (positional.size() > 1) {
        std::cerr << "error: multiple input files are not yet supported\n";
        return EXIT_FAILURE;
    }

    opts.input_path = positional[0];

    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << std::format("error: input file '{}' not found\n",
            opts.input_path.string());
        return EXIT_FAILURE;
    }

    const auto result = ric::compile(opts);

    if (result != ric::CompileResult::Success)
        std::cerr << std::format("ricc: compilation terminated ({})\n",
            ric::result_to_string(result));

    return result == ric::CompileResult::Success ? EXIT_SUCCESS : EXIT_FAILURE;
}
