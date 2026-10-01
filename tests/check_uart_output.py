#!/usr/bin/env python3
"""
Bare-Metal AMP TMR Flight Computer - UART Telemetry Verification Script
======================================================================
Validates QEMU simulation output against expected flight verdicts.
Exits 0 on success, non-zero on failure.
"""

import sys
import re
import os

EXPECTED_PATTERNS = [
    r"\[BOOT\] Master Arbiter active on Core ID: 0",
    r"\[SYNC\] Core Readiness Status: Node 1=ONLINE, Node 2=ONLINE, Node 3=ONLINE",
    r"\[FRAME #1\].*?Voter Status : UNANIMOUS \(All Nodes Agree\)",
    r"\[FRAME #2\].*?Voter Status : UNANIMOUS \(All Nodes Agree\)",
    r"\[FRAME #3\].*?Voter Status : MAJORITY 2oo3 \(Node 1 Outlier Masked\)",
    r"\[FRAME #4\].*?Voter Status : MAJORITY 2oo3 \(Node 2 Outlier Masked\)",
    r"\[FRAME #5\].*?Voter Status : MAJORITY 2oo3 \(Node 3 Outlier Masked\)",
    r"\[FRAME #6\].*?Voter Status : FAIL-SAFE ACTIVATED \(Total Disagreement\)",
    r"\[FRAME #7\].*?Voter Status : DEGRADED 2oo2 \(Consensus Reached\)",
    r"\[STATUS\] Flight computer completed mission profile smoothly\.",
    r"\[STATUS\] All spatial memory zones intact\. System entering standby\."
]

def verify_log(log_path):
    if not os.path.exists(log_path):
        print(f"[ERROR] Log file '{log_path}' not found!")
        return 1

    with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    missing = 0
    print(f"[INFO] Verifying UART output in '{log_path}'...")
    for idx, pattern in enumerate(EXPECTED_PATTERNS, 1):
        if re.search(pattern, content, re.DOTALL):
            print(f"  [PASS] #{idx:02d}: Pattern found -> {pattern[:60]}...")
        else:
            print(f"  [FAIL] #{idx:02d}: Missing expected pattern -> {pattern}")
            missing += 1

    if missing == 0:
        print(f"\n[SUCCESS] All {len(EXPECTED_PATTERNS)} flight telemetry assertions passed!")
        return 0
    else:
        print(f"\n[ERROR] {missing} assertions failed in simulation output.")
        return 1

if __name__ == "__main__":
    target_log = sys.argv[1] if len(sys.argv) > 1 else "docs/baseline_output.txt"
    sys.exit(verify_log(target_log))
