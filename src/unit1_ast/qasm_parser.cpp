#include "qroute/unit1_ast/qasm_parser.hpp"
#include "qroute/common/file_io.hpp"
#include "qroute/unit1_ast/lexer.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace qroute::unit1 {

namespace {

class ParserImpl {
public:
    explicit ParserImpl(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    ParsedProgram parse() {
        if (tokens_.empty() || tokens_.front().type == TokenType::EndOfFile) {
            throw std::invalid_argument("Empty OpenQASM source");
        }

        // P-11: Estimate statement count by counting semicolons
        size_t stmt_estimate = 0;
        for (const auto& tok : tokens_) {
            if (tok.type == TokenType::Semicolon) {
                ++stmt_estimate;
            }
        }
        prog_.ast.reserve(stmt_estimate);

        // P-1: OPENQASM 2.0; must be first statement
        parse_header();

        while (!is_at_end()) {
            if (match(TokenType::Semicolon)) {
                // Ignore extraneous semicolon
                continue;
            }
            parse_statement();
        }

        // P-12: After appending, build balanced projection and finalize threads
        prog_.ast.build_from_sequence();
        prog_.ast.finalize_threads();

        return std::move(prog_);
    }

private:
    std::vector<Token> tokens_;
    size_t             pos_ = 0;
    ParsedProgram      prog_;

    const Token& peek() const {
        return tokens_[pos_];
    }

    const Token& previous() const {
        return tokens_[pos_ - 1];
    }

    bool is_at_end() const {
        return pos_ >= tokens_.size() || peek().type == TokenType::EndOfFile;
    }

    const Token& advance() {
        if (!is_at_end()) ++pos_;
        return previous();
    }

    bool check(TokenType type) const {
        if (is_at_end()) return false;
        return peek().type == type;
    }

    bool match(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    const Token& consume(TokenType type, const std::string& message) {
        if (check(type)) return advance();
        const auto& tok = peek();
        throw std::invalid_argument("Syntax error at line " + std::to_string(tok.line) +
                                    ":" + std::to_string(tok.column) + ": " + message);
    }

    void parse_header() {
        if (!check(TokenType::OpenQasm)) {
            const auto& tok = peek();
            throw std::invalid_argument("Syntax error at line " + std::to_string(tok.line) +
                                        ":" + std::to_string(tok.column) +
                                        ": First statement must be 'OPENQASM 2.0;'");
        }
        const auto& tok = advance();
        uint32_t line = tok.line;

        if (!check(TokenType::Float) && !check(TokenType::Integer)) {
            throw std::invalid_argument("Expected version 2.0 after OPENQASM at line " +
                                        std::to_string(line));
        }
        const auto& ver_tok = advance();
        if (std::abs(ver_tok.numeric - 2.0) > 1e-6 && ver_tok.text != "2.0") {
            throw std::invalid_argument("Unsupported OpenQASM version '" + ver_tok.text +
                                        "' at line " + std::to_string(line) + "; expected 2.0");
        }
        consume(TokenType::Semicolon, "Expected ';' after 'OPENQASM 2.0'");

        GatePayload payload{};
        prog_.ast.append(OpCode::OpenQasmHeader, payload, line);
    }

    void parse_statement() {
        const auto& tok = peek();
        uint32_t line = tok.line;

        if (match(TokenType::Include)) {
            consume(TokenType::String, "Expected string after 'include'");
            consume(TokenType::Semicolon, "Expected ';' after include statement");
            GatePayload payload{};
            prog_.ast.append(OpCode::Include, payload, line);
        } else if (match(TokenType::QReg)) {
            parse_reg_decl(true);
        } else if (match(TokenType::CReg)) {
            parse_reg_decl(false);
        } else if (match(TokenType::GateH)) {
            parse_single_qubit_gate(OpCode::H);
        } else if (match(TokenType::GateX)) {
            parse_single_qubit_gate(OpCode::X);
        } else if (match(TokenType::GateY)) {
            parse_single_qubit_gate(OpCode::Y);
        } else if (match(TokenType::GateZ)) {
            parse_single_qubit_gate(OpCode::Z);
        } else if (match(TokenType::GateRX)) {
            parse_param_single_qubit_gate(OpCode::RX);
        } else if (match(TokenType::GateRY)) {
            parse_param_single_qubit_gate(OpCode::RY);
        } else if (match(TokenType::GateRZ)) {
            parse_param_single_qubit_gate(OpCode::RZ);
        } else if (match(TokenType::GateCX)) {
            parse_two_qubit_gate(OpCode::CX);
        } else if (match(TokenType::GateSwap)) {
            parse_two_qubit_gate(OpCode::SWAP);
        } else if (match(TokenType::Barrier)) {
            parse_barrier();
        } else if (match(TokenType::Measure)) {
            parse_measure();
        } else if (match(TokenType::Reset)) {
            parse_reset();
        } else {
            // P-8: Unsupported gate name
            throw std::invalid_argument("Unsupported instruction or gate '" + tok.text +
                                        "' at line " + std::to_string(tok.line) + ":" +
                                        std::to_string(tok.column) +
                                        ". Supported set: h, x, y, z, rx, ry, rz, cx, swap, barrier, measure, reset, qreg, creg");
        }
    }

    void parse_reg_decl(bool is_quantum) {
        uint32_t line = previous().line;
        const auto& name_tok = consume(TokenType::Ident, "Expected register identifier");
        consume(TokenType::LBracket, "Expected '[' after register name");
        const auto& size_tok = consume(TokenType::Integer, "Expected register width integer");
        consume(TokenType::RBracket, "Expected ']' after register width");
        consume(TokenType::Semicolon, "Expected ';' after register declaration");

        const std::string& reg_name = name_tok.text;
        if (prog_.registers.contains(reg_name)) {
            throw std::invalid_argument("Duplicate register name '" + reg_name + "' at line " +
                                        std::to_string(line));
        }

        auto width = static_cast<uint32_t>(size_tok.numeric);
        RegisterInfo info;
        info.name = reg_name;
        info.is_quantum = is_quantum;
        info.width = width;

        if (is_quantum) {
            info.base_index = prog_.total_logical_qubits;
            prog_.total_logical_qubits += width;
            prog_.registers.insert(reg_name, info);
            GatePayload payload{};
            payload.qubit_count = static_cast<uint8_t>(width);
            prog_.ast.append(OpCode::QReg, payload, line);
        } else {
            info.base_index = prog_.total_classical_bits;
            prog_.total_classical_bits += width;
            prog_.registers.insert(reg_name, info);
            GatePayload payload{};
            payload.clbit_count = static_cast<uint8_t>(width);
            prog_.ast.append(OpCode::CReg, payload, line);
        }
    }

    LogicalQubit parse_qarg() {
        const auto& id_tok = consume(TokenType::Ident, "Expected qubit register identifier");
        consume(TokenType::LBracket, "Expected '[' after qubit register name");
        const auto& idx_tok = consume(TokenType::Integer, "Expected qubit index integer");
        consume(TokenType::RBracket, "Expected ']' after qubit index");

        const auto* reg = prog_.registers.find(id_tok.text);
        if (!reg) {
            throw std::invalid_argument("Undeclared quantum register '" + id_tok.text +
                                        "' at line " + std::to_string(id_tok.line));
        }
        if (!reg->is_quantum) {
            throw std::invalid_argument("Register '" + id_tok.text + "' is classical, not quantum at line " +
                                        std::to_string(id_tok.line));
        }

        auto idx = static_cast<uint32_t>(idx_tok.numeric);
        if (idx >= reg->width) {
            throw std::invalid_argument("Qubit index " + std::to_string(idx) +
                                        " out of range for register '" + id_tok.text +
                                        "' (width " + std::to_string(reg->width) + ") at line " +
                                        std::to_string(id_tok.line));
        }

        return reg->base_index + idx;
    }

    ClbitIndex parse_carg() {
        const auto& id_tok = consume(TokenType::Ident, "Expected classical register identifier");
        consume(TokenType::LBracket, "Expected '[' after classical register name");
        const auto& idx_tok = consume(TokenType::Integer, "Expected classical bit index integer");
        consume(TokenType::RBracket, "Expected ']' after classical bit index");

        const auto* reg = prog_.registers.find(id_tok.text);
        if (!reg) {
            throw std::invalid_argument("Undeclared classical register '" + id_tok.text +
                                        "' at line " + std::to_string(id_tok.line));
        }
        if (reg->is_quantum) {
            throw std::invalid_argument("Register '" + id_tok.text + "' is quantum, not classical at line " +
                                        std::to_string(id_tok.line));
        }

        auto idx = static_cast<uint32_t>(idx_tok.numeric);
        if (idx >= reg->width) {
            throw std::invalid_argument("Classical bit index " + std::to_string(idx) +
                                        " out of range for register '" + id_tok.text +
                                        "' (width " + std::to_string(reg->width) + ") at line " +
                                        std::to_string(id_tok.line));
        }

        return reg->base_index + idx;
    }

    double parse_factor() {
        if (match(TokenType::Plus)) {
            return +parse_factor();
        }
        if (match(TokenType::Minus)) {
            return -parse_factor();
        }
        if (match(TokenType::Pi)) {
            return 3.14159265358979323846;
        }
        if (match(TokenType::Integer) || match(TokenType::Float)) {
            return previous().numeric;
        }
        if (match(TokenType::LParen)) {
            double val = parse_expr();
            consume(TokenType::RParen, "Expected ')' after expression");
            return val;
        }

        const auto& tok = peek();
        throw std::invalid_argument("Expected numeric factor in angle expression at line " +
                                    std::to_string(tok.line) + ":" + std::to_string(tok.column));
    }

    double parse_term() {
        double left = parse_factor();
        while (check(TokenType::Star) || check(TokenType::Slash)) {
            if (match(TokenType::Star)) {
                left *= parse_factor();
            } else if (match(TokenType::Slash)) {
                double denom = parse_factor();
                if (std::abs(denom) < 1e-15) {
                    throw std::invalid_argument("Division by zero in gate parameter expression");
                }
                left /= denom;
            }
        }
        return left;
    }

    double parse_expr() {
        double left = parse_term();
        while (check(TokenType::Plus) || check(TokenType::Minus)) {
            if (match(TokenType::Plus)) {
                left += parse_term();
            } else if (match(TokenType::Minus)) {
                left -= parse_term();
            }
        }
        return left;
    }

    void parse_single_qubit_gate(OpCode op) {
        uint32_t line = previous().line;
        LogicalQubit q = parse_qarg();
        consume(TokenType::Semicolon, "Expected ';' after gate operand");

        GatePayload payload{};
        payload.qubits[0] = q;
        payload.qubit_count = 1;
        prog_.ast.append(op, payload, line);
    }

    void parse_param_single_qubit_gate(OpCode op) {
        uint32_t line = previous().line;
        consume(TokenType::LParen, "Expected '(' after parameterized gate name");
        double param = parse_expr();
        consume(TokenType::RParen, "Expected ')' after gate parameter");

        LogicalQubit q = parse_qarg();
        consume(TokenType::Semicolon, "Expected ';' after gate operand");

        GatePayload payload{};
        payload.qubits[0] = q;
        payload.params[0] = param;
        payload.qubit_count = 1;
        payload.param_count = 1;
        prog_.ast.append(op, payload, line);
    }

    void parse_two_qubit_gate(OpCode op) {
        uint32_t line = previous().line;
        LogicalQubit q1 = parse_qarg();
        consume(TokenType::Comma, "Expected ',' between two-qubit gate operands");
        LogicalQubit q2 = parse_qarg();
        consume(TokenType::Semicolon, "Expected ';' after gate operands");

        // P-6: cx q[i], q[i]; (identical operands) -> error
        if (q1 == q2) {
            throw std::invalid_argument("Identical control and target qubit (" + std::to_string(q1) +
                                        ") in two-qubit gate at line " + std::to_string(line));
        }

        GatePayload payload{};
        payload.qubits[0] = q1;
        payload.qubits[1] = q2;
        payload.qubit_count = 2;
        prog_.ast.append(op, payload, line);
    }

    void parse_barrier() {
        uint32_t line = previous().line;
        GatePayload payload{};
        uint8_t count = 0;

        do {
            LogicalQubit q = parse_qarg();
            if (count < 4) {
                payload.qubits[count] = q;
            }
            ++count;
        } while (match(TokenType::Comma));

        consume(TokenType::Semicolon, "Expected ';' after barrier operands");
        payload.qubit_count = count;
        prog_.ast.append(OpCode::Barrier, payload, line);
    }

    void parse_measure() {
        uint32_t line = previous().line;
        LogicalQubit q = parse_qarg();
        consume(TokenType::Arrow, "Expected '->' in measure statement");
        ClbitIndex c = parse_carg();
        consume(TokenType::Semicolon, "Expected ';' after measure statement");

        GatePayload payload{};
        payload.qubits[0] = q;
        payload.clbits[0] = c;
        payload.qubit_count = 1;
        payload.clbit_count = 1;
        prog_.ast.append(OpCode::Measure, payload, line);
    }

    void parse_reset() {
        uint32_t line = previous().line;
        LogicalQubit q = parse_qarg();
        consume(TokenType::Semicolon, "Expected ';' after reset operand");

        GatePayload payload{};
        payload.qubits[0] = q;
        payload.qubit_count = 1;
        prog_.ast.append(OpCode::Reset, payload, line);
    }
};

} // anonymous namespace

ParsedProgram QasmParser::parse(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    ParserImpl impl(std::move(tokens));
    return impl.parse();
}

ParsedProgram QasmParser::parse_file(const std::string& path) {
    std::string source = read_text_file(path);
    return QasmParser().parse(source);
}

} // namespace qroute::unit1