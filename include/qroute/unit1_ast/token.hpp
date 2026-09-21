#pragma once

#include <cstdint>
#include <iostream>
#include <string>

namespace qroute::unit1 {

enum class TokenType : uint8_t {
    EndOfFile, OpenQasm, Include, QReg, CReg, Barrier, Measure, Reset,
    Ident, Integer, Float, Pi, String,
    GateH, GateX, GateY, GateZ, GateRX, GateRY, GateRZ, GateCX, GateSwap,
    LParen, RParen, LBracket, RBracket, Comma, Semicolon, Arrow,
    Plus, Minus, Star, Slash
};

inline std::ostream& operator<<(std::ostream& os, TokenType t) {
    return os << static_cast<int>(t);
}

struct Token {
    TokenType   type = TokenType::EndOfFile;
    std::string text;
    double      numeric = 0.0;
    uint32_t    line = 0;
    uint32_t    column = 0;
};

} // namespace qroute::unit1