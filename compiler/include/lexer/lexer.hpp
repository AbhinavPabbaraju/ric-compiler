#pragma once

#include "diagnostics/diagnostics.hpp"
#include "lexer/token.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ric {

class Lexer {
public:
    explicit Lexer(std::string_view source, std::string filename, DiagnosticEngine& diags);

    [[nodiscard]] auto tokenize() -> std::vector<Token>;

private:
    std::string_view  source_;
    std::string       filename_;
    DiagnosticEngine& diags_;
    std::size_t       pos_{0};
    std::uint32_t     line_{1};
    std::uint32_t     column_{1};

    [[nodiscard]] auto peek(std::size_t offset = 0) const noexcept -> char;
    auto               advance()                               -> char;
    [[nodiscard]] auto at_end()                const noexcept -> bool;
    [[nodiscard]] auto current_location()      const noexcept -> SourceLocation;

    auto scan_token()                          -> Token;
    auto scan_identifier_or_keyword()          -> Token;
    auto scan_integer_literal()                -> Token;
    auto scan_char_literal()                   -> Token;
    auto skip_whitespace_and_comments()        -> void;
    auto scan_line_comment()                   -> void;
    auto scan_block_comment()                  -> void;

    [[nodiscard]] auto make_token(TokenKind kind, std::string lexeme, SourceLocation loc) -> Token;
    [[nodiscard]] static auto keyword_for(std::string_view lexeme) noexcept
        -> std::optional<TokenKind>;
};

}
