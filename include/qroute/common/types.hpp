#pragma once

#include <cstdint>
#include <limits>
#include <string>

namespace qroute {

inline constexpr uint32_t kNil = std::numeric_limits<uint32_t>::max();

using LogicalQubit  = uint32_t;
using PhysicalQubit = uint32_t;
using NodeIndex     = uint32_t;
using ClbitIndex    = uint32_t;

enum class OpCode : uint8_t {
    Nop, OpenQasmHeader, Include, QReg, CReg,
    H, X, Y, Z, RX, RY, RZ,
    CX, SWAP, Barrier, Measure, Reset
};

inline const char* opcode_to_string(OpCode op) noexcept {
    switch (op) {
        case OpCode::Nop: return "nop";
        case OpCode::OpenQasmHeader: return "OPENQASM";
        case OpCode::Include: return "include";
        case OpCode::QReg: return "qreg";
        case OpCode::CReg: return "creg";
        case OpCode::H: return "h";
        case OpCode::X: return "x";
        case OpCode::Y: return "y";
        case OpCode::Z: return "z";
        case OpCode::RX: return "rx";
        case OpCode::RY: return "ry";
        case OpCode::RZ: return "rz";
        case OpCode::CX: return "cx";
        case OpCode::SWAP: return "swap";
        case OpCode::Barrier: return "barrier";
        case OpCode::Measure: return "measure";
        case OpCode::Reset: return "reset";
        default: return "unknown";
    }
}
inline const char* opcodetostring(OpCode op) noexcept { return opcode_to_string(op); }

inline bool is_single_qubit_gate(OpCode op) noexcept {
    return op == OpCode::H || op == OpCode::X || op == OpCode::Y || op == OpCode::Z ||
           op == OpCode::RX || op == OpCode::RY || op == OpCode::RZ;
}
inline bool issinglequbit_gate(OpCode op) noexcept { return is_single_qubit_gate(op); }

inline bool is_two_qubit_gate(OpCode op) noexcept {
    return op == OpCode::CX || op == OpCode::SWAP;
}
inline bool istwoqubit_gate(OpCode op) noexcept { return is_two_qubit_gate(op); }

inline bool is_classical_instruction(OpCode op) noexcept {
    return op == OpCode::Measure || op == OpCode::Barrier || op == OpCode::Reset;
}
inline bool isclassicalinstruction(OpCode op) noexcept { return is_classical_instruction(op); }

struct ParseError {
    std::string message;
    uint32_t    line = 0;
    uint32_t    column = 0;
};

} // namespace qroute