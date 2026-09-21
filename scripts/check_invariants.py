#!/usr/bin/env python3
import os
import re
import sys

# Invariant check rules as specified in PRD Section 0.3
RULES = [
    ("INV-4", ["include", "src", "apps", "tests", "benchmarks"],
     r"(__int128|_umul128|uint128_t|int128_t|uint128t|int128|umul128)",
     "128-bit compiler extension integer"),

    ("INV-1", ["include/qroute/unit2_graph", "src/unit2_graph"],
     r"FibonacciHeap",
     "Fibonacci heap forbidden in Unit 2 routing"),

    ("INV-2", ["include/qroute/unit4_bnb", "src/unit4_bnb"],
     r"(std::priority_queue|FibonacciHeap|std::multiset)",
     "best-first open list forbidden in Unit 4"),

    ("INV-3", ["include/qroute/unit3_dp", "src/unit3_dp"],
     r"(std::complex|blas|lapack|Eigen|state_vector|amplitude)",
     "numerical simulation forbidden in Unit 3"),

    ("MEM",   ["include", "src", "apps"],
     r"(^|[^_a-zA-Z0-9])(new|delete|malloc|free)\s*[\(\[]",
     "raw dynamic memory management"),
]

EXTENSIONS = {".hpp", ".h", ".cpp", ".c", ".cc"}

def normalize_path(p):
    return p.replace("\\", "/")

def scan_file(filepath, rule_id, pattern, description):
    compiled = re.compile(pattern)
    violations = []
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        for lineno, line in enumerate(f, start=1):
            if compiled.search(line):
                violations.append((rule_id, filepath, lineno, line.strip(), description))
    return violations

def main():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    all_violations = []

    for rule_id, paths_to_scan, pattern, desc in RULES:
        for scan_path in paths_to_scan:
            full_scan_path = os.path.join(root_dir, scan_path)
            if not os.path.exists(full_scan_path):
                continue
            for dirpath, _, filenames in os.walk(full_scan_path):
                for fname in filenames:
                    ext = os.path.splitext(fname)[1].lower()
                    if ext in EXTENSIONS:
                        fpath = os.path.join(dirpath, fname)
                        rel_path = normalize_path(os.path.relpath(fpath, root_dir))
                        violations = scan_file(fpath, rule_id, pattern, desc)
                        all_violations.extend(violations)

    if all_violations:
        print(f"FAILED: Found {len(all_violations)} invariant violation(s):")
        for r_id, fpath, lineno, line, desc in all_violations:
            print(f"  [{r_id}] {fpath}:{lineno}: {desc}")
            print(f"    Line: {line}")
        sys.exit(1)
    else:
        print("ALL INVARIANTS SATISFIED")
        sys.exit(0)

if __name__ == "__main__":
    main()