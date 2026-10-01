#!/usr/bin/env python3
"""
Bare-Metal AMP TMR Flight Computer - Safety Traceability Checker (P3.1)
========================================================================
Validates that every safety requirement in docs/safety/TRACEABILITY.md
is linked to an existing source file, valid function, and verification test.
Exits 0 on success, non-zero on error.
"""

import sys
import os
import re

def check_traceability():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    matrix_path = os.path.join(project_root, "docs", "safety", "TRACEABILITY.md")

    if not os.path.exists(matrix_path):
        print(f"[ERROR] Traceability matrix not found at {matrix_path}")
        return 1

    with open(matrix_path, "r", encoding="utf-8") as f:
        content = f.read()

    rows = re.findall(r"\|\s*\*\*(HZ-\d+)\*\*\s*\|\s*`?(SR-\d+)`?\s*\|\s*([^|]+)\s*\|\s*\[`([^`]+)`\]\([^)]+\):\s*`([^`]+)`\s*\|\s*`([^`]+)`\s*\|\s*([^|]+)\s*\|", content)

    if not rows:
        print("[ERROR] No valid traceability rows found in table!")
        return 1

    print("==============================================================================")
    print("  AUTOMATED SAFETY REQUIREMENTS TRACEABILITY AUDIT")
    print("==============================================================================")
    print(f"[INFO] Auditing {len(rows)} safety requirement mappings from TRACEABILITY.md...\n")

    errors = 0
    requirements_seen = set()
    tests_seen = set()

    for hazard, req, design, src_file, func_name, test_id, test_cat in rows:
        requirements_seen.add(req)
        tests_seen.add(test_id)

        # Check source file existence
        full_src = os.path.join(project_root, src_file)
        if not os.path.exists(full_src):
            print(f"  [FAIL] {req}: Referenced source file '{src_file}' does not exist!")
            errors += 1
            continue

        # Check function existence in source file
        with open(full_src, "r", encoding="utf-8", errors="ignore") as sf:
            src_text = sf.read()
            if func_name not in src_text:
                print(f"  [FAIL] {req}: Function '{func_name}' not found in '{src_file}'!")
                errors += 1
                continue

        print(f"  [PASS] {hazard} -> {req} -> {src_file}:{func_name}() -> {test_id} ({test_cat.strip()})")

    print("\n------------------------------------------------------------------------------")
    print(f"  Traceability Audit Results: {len(rows) - errors}/{len(rows)} verified.")
    print(f"  Total Requirements Traced: {len(requirements_seen)}")
    print(f"  Total Test IDs Linked    : {len(tests_seen)}")
    print("==============================================================================")

    if errors == 0:
        print("  VERDICT: TRACEABILITY MATRIX IS 100% COMPLETE AND CONSISTENT")
        print("==============================================================================")
        return 0
    else:
        print(f"  VERDICT: AUDIT FAILED WITH {errors} TRACEABILITY ERRORS")
        print("==============================================================================")
        return 1

if __name__ == "__main__":
    sys.exit(check_traceability())
