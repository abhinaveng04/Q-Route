#include "qroute/unit1_ast/qasm_parser.hpp"
#include "qroute/unit2_graph/physical_coupling_graph.hpp"

#include <iostream>
#include <locale>
#include <string>

int main(int argc, char* argv[]) {
    std::locale::global(std::locale::classic());

    std::string circuit_path;
    std::string coupling_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: qroute [options]\n";
            std::cout << "  --circuit <file>     OpenQASM 2.0 input trace\n";
            std::cout << "  --coupling <file>    Physical coupling map\n";
            std::cout << "  --out <file>         Routed physical circuit output path\n";
            std::cout << "  --version            Print version\n";
            std::cout << "  --help               Print help\n";
            return 0;
        }
        if (arg == "--version") {
            std::cout << "qroute 1.0.0 (R2)\n";
            return 0;
        }
        if (arg == "--circuit" && i + 1 < argc) {
            circuit_path = argv[++i];
        } else if (arg == "--coupling" && i + 1 < argc) {
            coupling_path = argv[++i];
        }
    }

    if (!circuit_path.empty()) {
        try {
            auto prog = qroute::unit1::QasmParser::parse_file(circuit_path);
            std::cout << "ast_nodes               : " << prog.ast.size() << "\n";
            std::cout << "symbol_table_entries    : " << prog.registers.size() << "\n";
            if (!coupling_path.empty()) {
                auto graph = qroute::unit2::PhysicalCouplingGraph::load_from_file(coupling_path);
                std::cout << "coupling_vertices       : " << graph.vertex_count() << "\n";
                std::cout << "coupling_edges          : " << graph.edge_count() << "\n";
            }
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 3;
        }
    } else if (!coupling_path.empty()) {
        try {
            auto graph = qroute::unit2::PhysicalCouplingGraph::load_from_file(coupling_path);
            std::cout << "coupling_vertices       : " << graph.vertex_count() << "\n";
            std::cout << "coupling_edges          : " << graph.edge_count() << "\n";
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 4;
        }
    }

    std::cout << "TASK-3 stub\n";
    return 0;
}