#pragma once

#include "qroute/unit1_ast/token.hpp"

#include <string>
#include <vector>

namespace qroute::unit1 {

class Lexer {
public:
    explicit Lexer(std::string source);

    // Tokenizes source. Throws std::invalid_argument on malformed token with line/col.
    std::vector<Token> tokenize();

    static std::string token_type_name(TokenType t) noexcept;
    static std::string tokentypename(TokenType t) noexcept { return token_type_name(t); }

private:
    std::string source_;
    size_t      pos_  = 0;
    uint32_t    line_ = 1;
    uint32_t    col_  = 1;

    void skip_whitespace_and_comments();
    Token lex_number();
    Token lex_ident();
    Token lex_string();
};

} // namespace qroute::unit1