#!/usr/bin/env python3
import re
import sys

TIMING_KEYS = {
    "parse_time_us",
    "ast_build_time_us",
    "placement_time_us",
    "routing_time_us",
    "mcm_plan_time_us",
    "telemetry_time_us",
    "total_cpu_time_us",
}

def is_timing_line(line: str) -> bool:
    if " : " in line:
        key = line.split(" : ")[0].strip()
        if key in TIMING_KEYS or key.endswith("_time_us"):
            return True
    return False

def normalize_line(line: str) -> str:
    line = line.rstrip("\r\n")
    if is_timing_line(line):
        parts = line.split(" : ", 1)
        key = parts[0].strip()
        val = parts[1].strip()
        try:
            float(val)
            return f"{key} : <TIMING_MASKED>"
        except ValueError:
            return line
    return line

def read_file_lines(filepath: str):
    with open(filepath, "rb") as f:
        data = f.read()
    if data.startswith(b"\xff\xfe") or b"\x00" in data[:100]:
        text = data.decode("utf-16", errors="ignore")
    elif data.startswith(b"\xfe\xff"):
        text = data.decode("utf-16-be", errors="ignore")
    else:
        text = data.decode("utf-8-sig", errors="ignore")
    return [normalize_line(l.lstrip("\ufeff")) for l in text.splitlines()]

def extract_execution_report(lines):
    # An execution report starts and ends with '====...===='
    # Boundaries: 1: top of header, 2: bottom of header, 3: end of report
    boundary_count = 0
    extracted = []
    for line in lines:
        extracted.append(line)
        if line.startswith("=" * 20):
            boundary_count += 1
            if boundary_count == 3:
                return extracted
    return lines

def diff_reports(file1: str, file2: str) -> bool:
    lines1 = read_file_lines(file1)
    lines2 = read_file_lines(file2)

    # If one of the files contains appended benchmarks, extract the execution report
    lines1 = extract_execution_report(lines1)
    lines2 = extract_execution_report(lines2)

    # Remove empty trailing lines
    while lines1 and not lines1[-1].strip():
        lines1.pop()
    while lines2 and not lines2[-1].strip():
        lines2.pop()

    if len(lines1) != len(lines2):
        print(f"FAILED: Line count mismatch ({len(lines1)} vs {len(lines2)})")
        return False

    all_matched = True
    for i, (l1, l2) in enumerate(zip(lines1, lines2), start=1):
        if l1 != l2:
            print(f"DIFF at line {i}:")
            print(f"  File 1: {l1}")
            print(f"  File 2: {l2}")
            all_matched = False

    return all_matched

def main():
    if len(sys.argv) < 3:
        print("Usage: golden_diff.py <report1> <report2>")
        sys.exit(2)

    file1 = sys.argv[1]
    file2 = sys.argv[2]

    if diff_reports(file1, file2):
        print("GOLDEN MATCH PASS")
        sys.exit(0)
    else:
        print("GOLDEN MATCH FAIL")
        sys.exit(1)

if __name__ == "__main__":
    main()
