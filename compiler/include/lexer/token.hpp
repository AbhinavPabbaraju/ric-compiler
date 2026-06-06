#pragma once

#include "diagnostics/source_location.hpp"
#include <cstdint>
#include <string>
#include <string_view>

namespace ric {

enum class TokenKind : std::uint8_t {
    IntLiteral,
    BoolLiteral,
    CharLiteral,
    Identifier,

    KwFn,
    KwLet,
    KwMut,
    KwIf,
    KwElse,
    KwWhile,
    KwLoop,
    KwBreak,
    KwContinue,
    KwReturn,
    KwTrue,
    KwFalse,
    KwI32,
    KwI64,
    KwBool,
    KwChar,

    Plus,
    Minus,
    Star,
    Slash,
    Percent,

    EqEq,
    BangEq,
    Lt,
    Gt,
    LtEq,
    GtEq,

    AmpAmp,
    PipePipe,
    Bang,

    Eq,
    Arrow,

    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,

    Semicolon,
    Colon,
    Comma,
    Dot,

    Eof,
    Invalid,
};

[[nodiscard]] auto token_kind_name(TokenKind kind) noexcept -> std::string_view;

struct Token {
    TokenKind    kind;
    std::string  lexeme;
    SourceLocation location;

    union LiteralValue {
        std::int64_t int_val;
        bool         bool_val;
        char32_t     char_val;
    } literal{};

    [[nodiscard]] auto is_keyword()   const noexcept -> bool;
    [[nodiscard]] auto is_literal()   const noexcept -> bool;
    [[nodiscard]] auto is_operator()  const noexcept -> bool;
    [[nodiscard]] auto is_delimiter() const noexcept -> bool;
    [[nodiscard]] auto is_type_keyword() const noexcept -> bool;
};

}
