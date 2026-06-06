#pragma once

#include "diagnostics/source_location.hpp"
#include <ostream>
#include <span>
#include <string>
#include <vector>
#include <optional>

namespace ric {

enum class DiagnosticLevel { Note, Warning, Error, Fatal };

struct DiagnosticLabel {
    SourceRange range;
    std::string message;
};

struct Diagnostic {
    DiagnosticLevel           level;
    std::string               code;
    std::string               primary_message;
    std::optional<SourceRange> primary_range;
    std::vector<DiagnosticLabel> labels;
    std::vector<std::string>  notes;
    std::optional<std::string> suggestion;

    [[nodiscard]] auto is_error() const noexcept -> bool {
        return level == DiagnosticLevel::Error || level == DiagnosticLevel::Fatal;
    }
};

class DiagnosticEngine {
public:
    explicit DiagnosticEngine(std::string filename, std::string_view source);

    auto emit_error(std::string code, std::string message, SourceRange range)   -> Diagnostic&;
    auto emit_warning(std::string code, std::string message, SourceRange range) -> Diagnostic&;
    auto emit_note(std::string message, SourceRange range)                       -> Diagnostic&;

    void add_label(Diagnostic& diag, SourceRange range, std::string message);
    void add_note(Diagnostic& diag, std::string message);
    void add_suggestion(Diagnostic& diag, std::string text);

    [[nodiscard]] auto has_errors()   const noexcept -> bool;
    [[nodiscard]] auto error_count()  const noexcept -> std::size_t;
    [[nodiscard]] auto diagnostics()  const noexcept -> std::span<const Diagnostic>;

    void print_all(std::ostream& out) const;

private:
    std::string              filename_;
    std::vector<std::string> source_lines_;
    std::vector<Diagnostic>  diagnostics_;
    std::size_t              error_count_{0};

    void                         print_one(const Diagnostic& diag, std::ostream& out) const;
    [[nodiscard]] auto           get_line(std::uint32_t line_num) const noexcept -> std::string_view;
    [[nodiscard]] static auto    level_prefix(DiagnosticLevel lvl) -> std::string_view;
    [[nodiscard]] static auto    level_color(DiagnosticLevel lvl)  -> std::string_view;
};

}
