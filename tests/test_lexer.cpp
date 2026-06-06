#include <catch2/catch_test_macros.hpp>
#include "diagnostics/diagnostics.hpp"
#include "lexer/lexer.hpp"
#include <vector>

using namespace ric;

static auto lex(std::string_view src) -> std::vector<Token> {
    DiagnosticEngine diags{"<test>", src};
    Lexer lexer{src, "<test>", diags};
    return lexer.tokenize();
}

static auto kinds(const std::vector<Token>& tokens) -> std::vector<TokenKind> {
    std::vector<TokenKind> result;
    for (const auto& t : tokens)
        if (t.kind != TokenKind::Eof)
            result.push_back(t.kind);
    return result;
}

TEST_CASE("Lexer: empty source produces single Eof", "[lexer]") {
    const auto tokens = lex("");
    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].kind == TokenKind::Eof);
}

TEST_CASE("Lexer: integer literals", "[lexer]") {
    const auto tokens = lex("42 0 999");
    REQUIRE(kinds(tokens) == std::vector{TokenKind::IntLiteral, TokenKind::IntLiteral, TokenKind::IntLiteral});
    REQUIRE(tokens[0].literal.int_val == 42);
    REQUIRE(tokens[1].literal.int_val == 0);
    REQUIRE(tokens[2].literal.int_val == 999);
}

TEST_CASE("Lexer: bool literals", "[lexer]") {
    const auto tokens = lex("true false");
    REQUIRE(kinds(tokens) == std::vector{TokenKind::BoolLiteral, TokenKind::BoolLiteral});
    REQUIRE(tokens[0].literal.bool_val == true);
    REQUIRE(tokens[1].literal.bool_val == false);
}

TEST_CASE("Lexer: char literals", "[lexer]") {
    const auto tokens = lex("'a' 'z' '\\n'");
    REQUIRE(kinds(tokens) == std::vector{TokenKind::CharLiteral, TokenKind::CharLiteral, TokenKind::CharLiteral});
    REQUIRE(tokens[0].literal.char_val == 'a');
    REQUIRE(tokens[1].literal.char_val == 'z');
    REQUIRE(tokens[2].literal.char_val == '\n');
}

TEST_CASE("Lexer: keywords", "[lexer]") {
    const auto tokens = lex("fn let mut if else while loop break continue return");
    REQUIRE(kinds(tokens) == std::vector{
        TokenKind::KwFn, TokenKind::KwLet, TokenKind::KwMut,
        TokenKind::KwIf, TokenKind::KwElse, TokenKind::KwWhile,
        TokenKind::KwLoop, TokenKind::KwBreak, TokenKind::KwContinue,
        TokenKind::KwReturn,
    });
}

TEST_CASE("Lexer: type keywords", "[lexer]") {
    const auto tokens = lex("i32 i64 bool char");
    REQUIRE(kinds(tokens) == std::vector{
        TokenKind::KwI32, TokenKind::KwI64, TokenKind::KwBool, TokenKind::KwChar
    });
}

TEST_CASE("Lexer: arithmetic operators", "[lexer]") {
    const auto tokens = lex("+ - * / %");
    REQUIRE(kinds(tokens) == std::vector{
        TokenKind::Plus, TokenKind::Minus, TokenKind::Star,
        TokenKind::Slash, TokenKind::Percent,
    });
}

TEST_CASE("Lexer: comparison operators", "[lexer]") {
    const auto tokens = lex("== != < > <= >=");
    REQUIRE(kinds(tokens) == std::vector{
        TokenKind::EqEq, TokenKind::BangEq,
        TokenKind::Lt, TokenKind::Gt,
        TokenKind::LtEq, TokenKind::GtEq,
    });
}

TEST_CASE("Lexer: logical operators", "[lexer]") {
    const auto tokens = lex("&& || !");
    REQUIRE(kinds(tokens) == std::vector{
        TokenKind::AmpAmp, TokenKind::PipePipe, TokenKind::Bang,
    });
}

TEST_CASE("Lexer: arrow and assignment", "[lexer]") {
    const auto tokens = lex("-> =");
    REQUIRE(kinds(tokens) == std::vector{TokenKind::Arrow, TokenKind::Eq});
}

TEST_CASE("Lexer: delimiters", "[lexer]") {
    const auto tokens = lex("( ) { } [ ] ; : , .");
    REQUIRE(kinds(tokens) == std::vector{
        TokenKind::LParen, TokenKind::RParen,
        TokenKind::LBrace, TokenKind::RBrace,
        TokenKind::LBracket, TokenKind::RBracket,
        TokenKind::Semicolon, TokenKind::Colon,
        TokenKind::Comma, TokenKind::Dot,
    });
}

TEST_CASE("Lexer: identifiers distinct from keywords", "[lexer]") {
    const auto tokens = lex("foo bar baz fn_like let_x");
    REQUIRE(kinds(tokens) == std::vector{
        TokenKind::Identifier, TokenKind::Identifier, TokenKind::Identifier,
        TokenKind::Identifier, TokenKind::Identifier,
    });
    REQUIRE(tokens[0].lexeme == "foo");
    REQUIRE(tokens[3].lexeme == "fn_like");
}

TEST_CASE("Lexer: line comments are skipped", "[lexer]") {
    const auto tokens = lex("42 // this is ignored\n99");
    REQUIRE(kinds(tokens) == std::vector{TokenKind::IntLiteral, TokenKind::IntLiteral});
    REQUIRE(tokens[0].literal.int_val == 42);
    REQUIRE(tokens[1].literal.int_val == 99);
}

TEST_CASE("Lexer: block comments are skipped", "[lexer]") {
    const auto tokens = lex("1 /* a + b */ 2");
    REQUIRE(kinds(tokens) == std::vector{TokenKind::IntLiteral, TokenKind::IntLiteral});
}

TEST_CASE("Lexer: source location tracking", "[lexer]") {
    const auto tokens = lex("foo\nbar");
    REQUIRE(tokens[0].location.line == 1);
    REQUIRE(tokens[0].location.column == 1);
    REQUIRE(tokens[1].location.line == 2);
    REQUIRE(tokens[1].location.column == 1);
}

TEST_CASE("Lexer: simple function tokenization", "[lexer]") {
    const auto src = "fn add(a: i32, b: i32) -> i32 { a + b }";
    const auto tokens = lex(src);
    const auto ks = kinds(tokens);
    REQUIRE(ks[0]  == TokenKind::KwFn);
    REQUIRE(ks[1]  == TokenKind::Identifier);
    REQUIRE(ks[2]  == TokenKind::LParen);
    REQUIRE(ks[3]  == TokenKind::Identifier);
    REQUIRE(ks[4]  == TokenKind::Colon);
    REQUIRE(ks[5]  == TokenKind::KwI32);
    REQUIRE(ks[6]  == TokenKind::Comma);
    REQUIRE(ks[9]  == TokenKind::RParen);
    REQUIRE(ks[10] == TokenKind::Arrow);
    REQUIRE(ks[11] == TokenKind::KwI32);
}
