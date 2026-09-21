#include "qroute/unit1_ast/lexer.hpp"

#include <cctype>
#include <cmath>
#include <stdexcept>

namespace qroute::unit1 {

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::string Lexer::token_type_name(TokenType t) noexcept {
    switch (t) {
        case TokenType::EndOfFile: return "EndOfFile";
        case TokenType::OpenQasm: return "OpenQasm";
        case TokenType::Include: return "Include";
        case TokenType::QReg: return "QReg";
        case TokenType::CReg: return "CReg";
        case TokenType::Barrier: return "Barrier";
        case TokenType::Measure: return "Measure";
        case TokenType::Reset: return "Reset";
        case TokenType::Ident: return "Ident";
        case TokenType::Integer: return "Integer";
        case TokenType::Float: return "Float";
        case TokenType::Pi: return "Pi";
        case TokenType::String: return "String";
        case TokenType::GateH: return "GateH";
        case TokenType::GateX: return "GateX";
        case TokenType::GateY: return "GateY";
        case TokenType::GateZ: return "GateZ";
        case TokenType::GateRX: return "GateRX";
        case TokenType::GateRY: return "GateRY";
        case TokenType::GateRZ: return "GateRZ";
        case TokenType::GateCX: return "GateCX";
        case TokenType::GateSwap: return "GateSwap";
        case TokenType::LParen: return "LParen";
        case TokenType::RParen: return "RParen";
        case TokenType::LBracket: return "LBracket";
        case TokenType::RBracket: return "RBracket";
        case TokenType::Comma: return "Comma";
        case TokenType::Semicolon: return "Semicolon";
        case TokenType::Arrow: return "Arrow";
        case TokenType::Plus: return "Plus";
        case TokenType::Minus: return "Minus";
        case TokenType::Star: return "Star";
        case TokenType::Slash: return "Slash";
        default: return "Unknown";
    }
}

void Lexer::skip_whitespace_and_comments() {
    while (pos_ < source_.size()) {
        char c = source_[pos_];
        if (c == '\n') {
            ++line_;
            col_ = 1;
            ++pos_;
        } else if (c == '\r') {
            ++pos_;
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            ++col_;
            ++pos_;
        } else if (c == '#') {
            // Comment until newline
            while (pos_ < source_.size() && source_[pos_] != '\n') {
                ++pos_;
            }
        } else if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
            // Line comment //
            while (pos_ < source_.size() && source_[pos_] != '\n') {
                ++pos_;
            }
        } else {
            break;
        }
    }
}

Token Lexer::lex_string() {
    uint32_t start_line = line_;
    uint32_t start_col  = col_;
    ++pos_; // consume opening '"'
    ++col_;
    std::string text;
    while (pos_ < source_.size() && source_[pos_] != '"') {
        if (source_[pos_] == '\n') {
            throw std::invalid_argument("Unterminated string literal at line " +
                                        std::to_string(start_line) + ":" + std::to_string(start_col));
        }
        text.push_back(source_[pos_]);
        ++pos_;
        ++col_;
    }
    if (pos_ >= source_.size()) {
        throw std::invalid_argument("Unterminated string literal at line " +
                                    std::to_string(start_line) + ":" + std::to_string(start_col));
    }
    ++pos_; // consume closing '"'
    ++col_;
    return Token{TokenType::String, text, 0.0, start_line, start_col};
}

Token Lexer::lex_number() {
    uint32_t start_line = line_;
    uint32_t start_col  = col_;
    std::string text;
    bool is_float = false;

    if (source_[pos_] == '.') {
        is_float = true;
        text.push_back('.');
        ++pos_;
        ++col_;
    }

    while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
        text.push_back(source_[pos_]);
        ++pos_;
        ++col_;
    }

    if (pos_ < source_.size() && source_[pos_] == '.' && !is_float) {
        is_float = true;
        text.push_back('.');
        ++pos_;
        ++col_;
        while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
            text.push_back(source_[pos_]);
            ++pos_;
            ++col_;
        }
    }

    // Scientific notation (e.g. 1e-3, 1.2e+4)
    if (pos_ < source_.size() && (source_[pos_] == 'e' || source_[pos_] == 'E')) {
        is_float = true;
        text.push_back(source_[pos_]);
        ++pos_;
        ++col_;
        if (pos_ < source_.size() && (source_[pos_] == '+' || source_[pos_] == '-')) {
            text.push_back(source_[pos_]);
            ++pos_;
            ++col_;
        }
        while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
            text.push_back(source_[pos_]);
            ++pos_;
            ++col_;
        }
    }

    double val = 0.0;
    try {
        val = std::stod(text);
    } catch (...) {
        throw std::invalid_argument("Malformed numeric constant '" + text + "' at " +
                                    std::to_string(start_line) + ":" + std::to_string(start_col));
    }

    TokenType tt = is_float ? TokenType::Float : TokenType::Integer;
    return Token{tt, text, val, start_line, start_col};
}

Token Lexer::lex_ident() {
    uint32_t start_line = line_;
    uint32_t start_col  = col_;
    std::string text;
    while (pos_ < source_.size() &&
           (std::isalnum(static_cast<unsigned char>(source_[pos_])) || source_[pos_] == '_')) {
        text.push_back(source_[pos_]);
        ++pos_;
        ++col_;
    }

    TokenType tt = TokenType::Ident;
    double num = 0.0;
    if (text == "OPENQASM") tt = TokenType::OpenQasm;
    else if (text == "include") tt = TokenType::Include;
    else if (text == "qreg") tt = TokenType::QReg;
    else if (text == "creg") tt = TokenType::CReg;
    else if (text == "barrier") tt = TokenType::Barrier;
    else if (text == "measure") tt = TokenType::Measure;
    else if (text == "reset") tt = TokenType::Reset;
    else if (text == "pi") {
        tt = TokenType::Pi;
        num = 3.14159265358979323846;
    }
    else if (text == "h") tt = TokenType::GateH;
    else if (text == "x") tt = TokenType::GateX;
    else if (text == "y") tt = TokenType::GateY;
    else if (text == "z") tt = TokenType::GateZ;
    else if (text == "rx") tt = TokenType::GateRX;
    else if (text == "ry") tt = TokenType::GateRY;
    else if (text == "rz") tt = TokenType::GateRZ;
    else if (text == "cx") tt = TokenType::GateCX;
    else if (text == "swap") tt = TokenType::GateSwap;

    return Token{tt, text, num, start_line, start_col};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skip_whitespace_and_comments();
        if (pos_ >= source_.size()) {
            tokens.push_back(Token{TokenType::EndOfFile, "", 0.0, line_, col_});
            break;
        }

        char c = source_[pos_];
        uint32_t cur_line = line_;
        uint32_t cur_col  = col_;

        if (c == '"') {
            tokens.push_back(lex_string());
        } else if (c == '-' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '>') {
            pos_ += 2;
            col_ += 2;
            tokens.push_back(Token{TokenType::Arrow, "->", 0.0, cur_line, cur_col});
        } else if (c == '-') {
            ++pos_;
            ++col_;
            tokens.push_back(Token{TokenType::Minus, "-", 0.0, cur_line, cur_col});
        } else if (c == '+') {
            ++pos_;
            ++col_;
            tokens.push_back(Token{TokenType::Plus, "+", 0.0, cur_line, cur_col});
        } else if (c == '*') {
            ++pos_;
            ++col_;
            tokens.push_back(Token{TokenType::Star, "*", 0.0, cur_line, cur_col});
        } else if (c == '/') {
            ++pos_;
            ++col_;
            tokens.push_back(Token{TokenType::Slash, "/", 0.0, cur_line, cur_col});
        } else if (c == '(') {
            ++pos_;
            ++col_;
            tokens.push_back(Token{TokenType::LParen, "(", 0.0, cur_line, cur_col});
        } else if (c == ')') {
            ++pos_;
            ++col_;
            tokens.push_back(Token{TokenType::RParen, ")", 0.0, cur_line, cur_col});
        } else if (c == '[') {
            ++pos_;
            ++col_;
            tokens.push_back(Token{TokenType::LBracket, "[", 0.0, cur_line, cur_col});
        } else if (c == ']') {
            ++pos_;
            ++col_;
            tokens.push_back(Token{TokenType::RBracket, "]", 0.0, cur_line, cur_col});
        } else if (c == ',') {
            ++pos_;
            ++col_;
            tokens.push_back(Token{TokenType::Comma, ",", 0.0, cur_line, cur_col});
        } else if (c == ';') {
            ++pos_;
            ++col_;
            tokens.push_back(Token{TokenType::Semicolon, ";", 0.0, cur_line, cur_col});
        } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                   (c == '.' && pos_ + 1 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_ + 1])))) {
            tokens.push_back(lex_number());
        } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(lex_ident());
        } else {
            throw std::invalid_argument("Unknown character '" + std::string(1, c) + "' at " +
                                        std::to_string(cur_line) + ":" + std::to_string(cur_col));
        }
    }

    return tokens;
}

} // namespace qroute::unit1