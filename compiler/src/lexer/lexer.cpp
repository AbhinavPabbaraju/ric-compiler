#include "lexer/lexer.hpp"
#include <cctype>
#include <charconv>
#include <unordered_map>

namespace ric {

namespace {

const std::unordered_map<std::string_view, TokenKind> k_keywords = {
    {"fn",       TokenKind::KwFn},
    {"let",      TokenKind::KwLet},
    {"mut",      TokenKind::KwMut},
    {"if",       TokenKind::KwIf},
    {"else",     TokenKind::KwElse},
    {"while",    TokenKind::KwWhile},
    {"loop",     TokenKind::KwLoop},
    {"break",    TokenKind::KwBreak},
    {"continue", TokenKind::KwContinue},
    {"return",   TokenKind::KwReturn},
    {"true",     TokenKind::KwTrue},
    {"false",    TokenKind::KwFalse},
    {"i32",      TokenKind::KwI32},
    {"i64",      TokenKind::KwI64},
    {"bool",     TokenKind::KwBool},
    {"char",     TokenKind::KwChar},
};

}

auto token_kind_name(TokenKind kind) noexcept -> std::string_view {
    switch (kind) {
        case TokenKind::IntLiteral:  return "integer literal";
        case TokenKind::BoolLiteral: return "bool literal";
        case TokenKind::CharLiteral: return "char literal";
        case TokenKind::Identifier:  return "identifier";
        case TokenKind::KwFn:        return "`fn`";
        case TokenKind::KwLet:       return "`let`";
        case TokenKind::KwMut:       return "`mut`";
        case TokenKind::KwIf:        return "`if`";
        case TokenKind::KwElse:      return "`else`";
        case TokenKind::KwWhile:     return "`while`";
        case TokenKind::KwLoop:      return "`loop`";
        case TokenKind::KwBreak:     return "`break`";
        case TokenKind::KwContinue:  return "`continue`";
        case TokenKind::KwReturn:    return "`return`";
        case TokenKind::KwTrue:      return "`true`";
        case TokenKind::KwFalse:     return "`false`";
        case TokenKind::KwI32:       return "`i32`";
        case TokenKind::KwI64:       return "`i64`";
        case TokenKind::KwBool:      return "`bool`";
        case TokenKind::KwChar:      return "`char`";
        case TokenKind::Plus:        return "`+`";
        case TokenKind::Minus:       return "`-`";
        case TokenKind::Star:        return "`*`";
        case TokenKind::Slash:       return "`/`";
        case TokenKind::Percent:     return "`%`";
        case TokenKind::EqEq:        return "`==`";
        case TokenKind::BangEq:      return "`!=`";
        case TokenKind::Lt:          return "`<`";
        case TokenKind::Gt:          return "`>`";
        case TokenKind::LtEq:        return "`<=`";
        case TokenKind::GtEq:        return "`>=`";
        case TokenKind::AmpAmp:      return "`&&`";
        case TokenKind::PipePipe:    return "`||`";
        case TokenKind::Bang:        return "`!`";
        case TokenKind::Eq:          return "`=`";
        case TokenKind::Arrow:       return "`->`";
        case TokenKind::LParen:      return "`(`";
        case TokenKind::RParen:      return "`)`";
        case TokenKind::LBrace:      return "`{`";
        case TokenKind::RBrace:      return "`}`";
        case TokenKind::LBracket:    return "`[`";
        case TokenKind::RBracket:    return "`]`";
        case TokenKind::Semicolon:   return "`;`";
        case TokenKind::Colon:       return "`:`";
        case TokenKind::Comma:       return "`,`";
        case TokenKind::Dot:         return "`.`";
        case TokenKind::Eof:         return "<eof>";
        case TokenKind::Invalid:     return "<invalid>";
    }
    return "<unknown>";
}

auto Token::is_keyword() const noexcept -> bool {
    return kind >= TokenKind::KwFn && kind <= TokenKind::KwChar;
}

auto Token::is_literal() const noexcept -> bool {
    return kind == TokenKind::IntLiteral
        || kind == TokenKind::BoolLiteral
        || kind == TokenKind::CharLiteral;
}

auto Token::is_operator() const noexcept -> bool {
    return kind >= TokenKind::Plus && kind <= TokenKind::Arrow;
}

auto Token::is_delimiter() const noexcept -> bool {
    return kind >= TokenKind::LParen && kind <= TokenKind::Dot;
}

auto Token::is_type_keyword() const noexcept -> bool {
    return kind == TokenKind::KwI32
        || kind == TokenKind::KwI64
        || kind == TokenKind::KwBool
        || kind == TokenKind::KwChar;
}

Lexer::Lexer(std::string_view source, std::string filename, DiagnosticEngine& diags)
    : source_(source), filename_(std::move(filename)), diags_(diags) {}

auto Lexer::tokenize() -> std::vector<Token> {
    std::vector<Token> tokens;
    tokens.reserve(source_.size() / 4);

    while (true) {
        skip_whitespace_and_comments();
        if (at_end()) break;
        tokens.push_back(scan_token());
    }

    tokens.push_back(Token{
        .kind     = TokenKind::Eof,
        .lexeme   = "",
        .location = current_location(),
    });

    return tokens;
}

auto Lexer::peek(std::size_t offset) const noexcept -> char {
    const auto idx = pos_ + offset;
    return idx < source_.size() ? source_[idx] : '\0';
}

auto Lexer::advance() -> char {
    const char c = source_[pos_++];
    if (c == '\n') { ++line_; column_ = 1; }
    else           { ++column_; }
    return c;
}

auto Lexer::at_end() const noexcept -> bool { return pos_ >= source_.size(); }

auto Lexer::current_location() const noexcept -> SourceLocation {
    return {
        .filename = filename_,
        .line     = line_,
        .column   = column_,
        .offset   = static_cast<std::uint32_t>(pos_),
    };
}

auto Lexer::skip_whitespace_and_comments() -> void {
    while (!at_end()) {
        const char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            scan_line_comment();
        } else if (c == '/' && peek(1) == '*') {
            scan_block_comment();
        } else {
            break;
        }
    }
}

auto Lexer::scan_line_comment() -> void {
    while (!at_end() && peek() != '\n') advance();
}

auto Lexer::scan_block_comment() -> void {
    const auto start_loc = current_location();
    advance(); advance();
    int depth = 1;
    while (!at_end() && depth > 0) {
        if (peek() == '/' && peek(1) == '*') { advance(); advance(); ++depth; }
        else if (peek() == '*' && peek(1) == '/') { advance(); advance(); --depth; }
        else advance();
    }
    if (depth > 0) {
        diags_.emit_error("E0001", "unterminated block comment",
            SourceRange::at(start_loc));
    }
}

auto Lexer::scan_token() -> Token {
    const auto loc = current_location();
    const char c   = advance();

    switch (c) {
        case '+': return make_token(TokenKind::Plus,    "+",  loc);
        case '*': return make_token(TokenKind::Star,    "*",  loc);
        case '/': return make_token(TokenKind::Slash,   "/",  loc);
        case '%': return make_token(TokenKind::Percent, "%",  loc);
        case '(': return make_token(TokenKind::LParen,  "(",  loc);
        case ')': return make_token(TokenKind::RParen,  ")",  loc);
        case '{': return make_token(TokenKind::LBrace,  "{",  loc);
        case '}': return make_token(TokenKind::RBrace,  "}",  loc);
        case '[': return make_token(TokenKind::LBracket,"[",  loc);
        case ']': return make_token(TokenKind::RBracket,"]",  loc);
        case ';': return make_token(TokenKind::Semicolon,";", loc);
        case ':': return make_token(TokenKind::Colon,   ":",  loc);
        case ',': return make_token(TokenKind::Comma,   ",",  loc);
        case '.': return make_token(TokenKind::Dot,     ".",  loc);

        case '-':
            if (peek() == '>') { advance(); return make_token(TokenKind::Arrow,  "->", loc); }
            return make_token(TokenKind::Minus, "-", loc);

        case '=':
            if (peek() == '=') { advance(); return make_token(TokenKind::EqEq,    "==", loc); }
            return make_token(TokenKind::Eq, "=", loc);

        case '!':
            if (peek() == '=') { advance(); return make_token(TokenKind::BangEq,  "!=", loc); }
            return make_token(TokenKind::Bang, "!", loc);

        case '<':
            if (peek() == '=') { advance(); return make_token(TokenKind::LtEq,    "<=", loc); }
            return make_token(TokenKind::Lt, "<", loc);

        case '>':
            if (peek() == '=') { advance(); return make_token(TokenKind::GtEq,    ">=", loc); }
            return make_token(TokenKind::Gt, ">", loc);

        case '&':
            if (peek() == '&') { advance(); return make_token(TokenKind::AmpAmp,  "&&", loc); }
            diags_.emit_error("E0002", "bare `&` is not supported; did you mean `&&`?",
                SourceRange::at(loc));
            return make_token(TokenKind::Invalid, "&", loc);

        case '|':
            if (peek() == '|') { advance(); return make_token(TokenKind::PipePipe,"||", loc); }
            diags_.emit_error("E0002", "bare `|` is not supported; did you mean `||`?",
                SourceRange::at(loc));
            return make_token(TokenKind::Invalid, "|", loc);

        case '\'': {
            --pos_; --column_;
            return scan_char_literal();
        }

        default:
            if (std::isdigit(static_cast<unsigned char>(c))) {
                --pos_; --column_;
                return scan_integer_literal();
            }
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                --pos_; --column_;
                return scan_identifier_or_keyword();
            }
            diags_.emit_error("E0003",
                std::string("unexpected character '") + c + "'",
                SourceRange::at(loc));
            return make_token(TokenKind::Invalid, std::string(1, c), loc);
    }
}

auto Lexer::scan_identifier_or_keyword() -> Token {
    const auto loc   = current_location();
    const auto start = pos_;

    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
        advance();

    const auto lexeme = std::string{source_.substr(start, pos_ - start)};

    if (const auto kw = keyword_for(lexeme)) {
        auto tok = make_token(*kw, lexeme, loc);
        if (*kw == TokenKind::KwTrue)  { tok.literal.bool_val = true;  }
        if (*kw == TokenKind::KwFalse) { tok.literal.bool_val = false; }
        if (*kw == TokenKind::KwTrue || *kw == TokenKind::KwFalse)
            tok.kind = TokenKind::BoolLiteral;
        return tok;
    }

    return make_token(TokenKind::Identifier, lexeme, loc);
}

auto Lexer::scan_integer_literal() -> Token {
    const auto loc   = current_location();
    const auto start = pos_;

    while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) advance();

    const auto is_i64_suffix = !at_end() && peek() == 'i'
        && pos_ + 3 <= source_.size()
        && source_.substr(pos_, 3) == "i64";

    if (is_i64_suffix) { advance(); advance(); advance(); }

    const auto lexeme  = std::string{source_.substr(start, pos_ - start)};
    const auto num_str = is_i64_suffix ? lexeme.substr(0, lexeme.size() - 3) : lexeme;

    std::int64_t value = 0;
    auto [ptr, ec] = std::from_chars(num_str.data(), num_str.data() + num_str.size(), value);
    if (ec != std::errc{}) {
        diags_.emit_error("E0004", "integer literal is too large", SourceRange::at(loc));
    }

    Token tok     = make_token(TokenKind::IntLiteral, lexeme, loc);
    tok.literal.int_val = value;
    return tok;
}

auto Lexer::scan_char_literal() -> Token {
    const auto loc = current_location();
    advance();

    if (at_end() || peek() == '\n') {
        diags_.emit_error("E0005", "unterminated character literal", SourceRange::at(loc));
        return make_token(TokenKind::Invalid, "'", loc);
    }

    char32_t value;
    if (peek() == '\\') {
        advance();
        const char esc = advance();
        switch (esc) {
            case 'n':  value = '\n'; break;
            case 't':  value = '\t'; break;
            case 'r':  value = '\r'; break;
            case '0':  value = '\0'; break;
            case '\\': value = '\\'; break;
            case '\'': value = '\''; break;
            default:
                diags_.emit_error("E0006",
                    std::string("unknown escape sequence '\\") + esc + "'",
                    SourceRange::at(loc));
                value = '?';
        }
    } else {
        value = static_cast<unsigned char>(advance());
    }

    if (at_end() || peek() != '\'') {
        diags_.emit_error("E0005", "unterminated character literal", SourceRange::at(loc));
    } else {
        advance();
    }

    const auto lexeme = std::string{source_.substr(loc.offset, pos_ - loc.offset)};
    Token tok         = make_token(TokenKind::CharLiteral, lexeme, loc);
    tok.literal.char_val = value;
    return tok;
}

auto Lexer::make_token(TokenKind kind, std::string lexeme, SourceLocation loc) -> Token {
    return Token{.kind = kind, .lexeme = std::move(lexeme), .location = std::move(loc)};
}

auto Lexer::keyword_for(std::string_view lexeme) noexcept -> std::optional<TokenKind> {
    const auto it = k_keywords.find(lexeme);
    return it != k_keywords.end() ? std::optional{it->second} : std::nullopt;
}

}
