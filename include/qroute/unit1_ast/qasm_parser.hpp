#pragma once

#include "qroute/unit1_ast/avl_symbol_table.hpp"
#include "qroute/unit1_ast/threaded_ast.hpp"

#include <string>
#include <vector>

namespace qroute::unit1 {

struct RegisterInfo {
    std::string name;
    bool        is_quantum = false;
    uint32_t    width      = 0;
    uint32_t    base_index = 0;   // flattened global qubit/clbit offset
};

struct ParsedProgram {
    ThreadedAST                  ast;
    AvlSymbolTable<RegisterInfo> registers;
    uint32_t                     total_logical_qubits = 0;
    uint32_t                     total_classical_bits = 0;
    std::vector<std::string>     warnings;
};

class QasmParser {
public:
    ParsedProgram parse(const std::string& source);
    static ParsedProgram parse_file(const std::string& path);
};

} // namespace qroute::unit1