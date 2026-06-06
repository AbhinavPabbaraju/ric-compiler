#include "diagnostics/diagnostics.hpp"
#include <algorithm>
#include <format>
#include <sstream>

namespace ric {

namespace {

auto split_lines(std::string_view src) -> std::vector<std::string> {
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= src.size(); ++i) {
        if (i == src.size() || src[i] == '\n') {
            lines.emplace_back(src.substr(start, i - start));
            start = i + 1;
        }
    }
    return lines;
}

constexpr auto k_reset  = "\033[0m";
constexpr auto k_bold   = "\033[1m";
constexpr auto k_red    = "\033[31m";
constexpr auto k_yellow = "\033[33m";
constexpr auto k_cyan   = "\033[36m";
constexpr auto k_blue   = "\033[34m";
constexpr auto k_dim    = "\033[2m";

}

DiagnosticEngine::DiagnosticEngine(std::string filename, std::string_view source)
    : filename_(std::move(filename))
    , source_lines_(split_lines(source)) {}

auto DiagnosticEngine::emit_error(std::string code, std::string message, SourceRange range)
    -> Diagnostic& {
    ++error_count_;
    return diagnostics_.emplace_back(Diagnostic{
        .level           = DiagnosticLevel::Error,
        .code            = std::move(code),
        .primary_message = std::move(message),
        .primary_range   = range,
    });
}

auto DiagnosticEngine::emit_warning(std::string code, std::string message, SourceRange range)
    -> Diagnostic& {
    return diagnostics_.emplace_back(Diagnostic{
        .level           = DiagnosticLevel::Warning,
        .code            = std::move(code),
        .primary_message = std::move(message),
        .primary_range   = range,
    });
}

auto DiagnosticEngine::emit_note(std::string message, SourceRange range) -> Diagnostic& {
    return diagnostics_.emplace_back(Diagnostic{
        .level           = DiagnosticLevel::Note,
        .primary_message = std::move(message),
        .primary_range   = range,
    });
}

void DiagnosticEngine::add_label(Diagnostic& diag, SourceRange range, std::string message) {
    diag.labels.push_back({.range = range, .message = std::move(message)});
}

void DiagnosticEngine::add_note(Diagnostic& diag, std::string message) {
    diag.notes.push_back(std::move(message));
}

void DiagnosticEngine::add_suggestion(Diagnostic& diag, std::string text) {
    diag.suggestion = std::move(text);
}

auto DiagnosticEngine::has_errors()  const noexcept -> bool        { return error_count_ > 0; }
auto DiagnosticEngine::error_count() const noexcept -> std::size_t { return error_count_; }

auto DiagnosticEngine::diagnostics() const noexcept -> std::span<const Diagnostic> {
    return {diagnostics_.data(), diagnostics_.size()};
}

auto DiagnosticEngine::get_line(std::uint32_t line_num) const noexcept -> std::string_view {
    if (line_num == 0 || line_num > source_lines_.size()) return {};
    return source_lines_[line_num - 1];
}

auto DiagnosticEngine::level_prefix(DiagnosticLevel lvl) -> std::string_view {
    switch (lvl) {
        case DiagnosticLevel::Error:   return "error";
        case DiagnosticLevel::Warning: return "warning";
        case DiagnosticLevel::Note:    return "note";
        case DiagnosticLevel::Fatal:   return "fatal";
    }
    return "unknown";
}

auto DiagnosticEngine::level_color(DiagnosticLevel lvl) -> std::string_view {
    switch (lvl) {
        case DiagnosticLevel::Error:   return k_red;
        case DiagnosticLevel::Warning: return k_yellow;
        case DiagnosticLevel::Note:    return k_cyan;
        case DiagnosticLevel::Fatal:   return k_red;
    }
    return k_reset;
}

void DiagnosticEngine::print_one(const Diagnostic& diag, std::ostream& out) const {
    const auto color  = level_color(diag.level);
    const auto prefix = level_prefix(diag.level);

    out << k_bold << color;
    if (!diag.code.empty()) {
        out << prefix << "[" << diag.code << "]";
    } else {
        out << prefix;
    }
    out << k_reset << k_bold << ": " << diag.primary_message << k_reset << "\n";

    if (diag.primary_range) {
        const auto& loc = diag.primary_range->begin;
        if (loc.is_valid()) {
            out << k_dim << " --> " << k_reset
                << loc.filename << ":" << loc.line << ":" << loc.column << "\n";

            const auto line_text = get_line(loc.line);
            if (!line_text.empty()) {
                const auto line_num_str = std::to_string(loc.line);
                const auto pad = std::string(line_num_str.size(), ' ');

                out << " " << pad << k_dim << " |" << k_reset << "\n";
                out << " " << k_dim << line_num_str << " |" << k_reset << " " << line_text << "\n";
                out << " " << pad << k_dim << " |" << k_reset << " ";

                const auto col = static_cast<std::size_t>(loc.column > 0 ? loc.column - 1 : 0);
                const auto end_col = diag.primary_range->end.line == loc.line
                    ? static_cast<std::size_t>(diag.primary_range->end.column)
                    : col + 1;
                const auto caret_len = std::max(std::size_t{1}, end_col - col);

                out << std::string(col, ' ');
                out << color << k_bold;
                out << std::string(caret_len, '^');
                out << k_reset << "\n";
            }
        }
    }

    for (const auto& note : diag.notes) {
        out << k_cyan << " = note: " << k_reset << note << "\n";
    }

    if (diag.suggestion) {
        out << k_cyan << " = help: " << k_reset << *diag.suggestion << "\n";
    }

    out << "\n";
}

void DiagnosticEngine::print_all(std::ostream& out) const {
    for (const auto& diag : diagnostics_) {
        print_one(diag, out);
    }

    if (error_count_ > 0) {
        out << k_bold << k_red << "aborting due to " << error_count_
            << (error_count_ == 1 ? " error" : " errors") << k_reset << "\n";
    }
}

}
