import os
import sys
import subprocess

# Ensure UTF-8 output on Windows consoles
if hasattr(sys.stdout, 'reconfigure'):
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

def print_banner():
    print("+-------------------------------------------------------------------------------+")
    print("|         BARE-METAL AMP TMR FLIGHT COMPUTER -- MULTI-CORE VISUALIZER           |")
    print("|            ARM Cortex-A15 Quad-Core * SIFT Architecture * 2oo3 Voter          |")
    print("+-------------------------------------------------------------------------------+\n")

def print_memory_map():
    print("=================================================================================")
    print(" 1. PHYSICAL SPATIAL MEMORY MAP & CORE ISOLATION")
    print("=================================================================================")
    print("  Physical RAM: 0x80000000 - 0x88000000 (128 MB Allocated)")
    print("  Stack Model : 16KB Total [sp = _stack_top - (core_id * 4096)]\n")
    print("  +-------------------------+------------------------+------------------------+")
    print("  | Core 0: Master Arbiter  | System/Text Partition  | 0x80000000 - 0x80FFFFFF|")
    print("  |                         | Arbiter Stack (4KB)    | 0x80008000 - 0x80009000|")
    print("  +-------------------------+------------------------+------------------------+")
    print("  | Core 1: Compute Node 1  | Zone 1 Partition       | 0x81000000 - 0x81001000|")
    print("  |                         | Input: 0x81000000      | Output: 0x81000004     |")
    print("  |                         | Node 1 Stack (4KB)     | 0x80007000 - 0x80008000|")
    print("  +-------------------------+------------------------+------------------------+")
    print("  | Core 2: Compute Node 2  | Zone 2 Partition       | 0x82000000 - 0x82001000|")
    print("  |                         | Input: 0x82000000      | Output: 0x82000004     |")
    print("  |                         | Node 2 Stack (4KB)     | 0x80006000 - 0x80007000|")
    print("  +-------------------------+------------------------+------------------------+")
    print("  | Core 3: Compute Node 3  | Zone 3 Partition       | 0x83000000 - 0x83001000|")
    print("  |                         | Input: 0x83000000      | Output: 0x83000004     |")
    print("  |                         | Node 3 Stack (4KB)     | 0x80005000 - 0x80006000|")
    print("  +-------------------------+------------------------+------------------------+\n")

def print_voter_matrix():
    print("=================================================================================")
    print(" 2. 2-OUT-OF-3 (2oo3) BOUNDED MAJORITY VOTER MATRIX (Tolerance: |delta| <= 5 us)")
    print("=================================================================================")
    data = [
        ("TEST 1", "Nominal Flight", "0 ddeg/s", "1500 us", "1500 us", "1500 us", "0, 0, 0", "UNANIMOUS (All Agree)", "1500 us (PASS)"),
        ("TEST 2", "Bounded Noise", "120 ddeg/s", "1538 us", "1533 us", "1537 us", "5, 4, 1", "UNANIMOUS (Averaged)", "1536 us (PASS)"),
        ("TEST 3", "Node 1 SEU Flip", "50 ddeg/s", "2000 us", "1515 us", "1515 us", "485, 0, 485", "NODE 1 MASKED", "1515 us (PASS)"),
        ("TEST 4", "Node 2 SEU Flip", "-80 ddeg/s", "1476 us", "1220 us", "1476 us", "256, 256, 0", "NODE 2 MASKED", "1476 us (PASS)"),
        ("TEST 5", "Node 3 SEU Flip", "150 ddeg/s", "1545 us", "1545 us", "1000 us", "0, 545, 545", "NODE 3 MASKED", "1545 us (PASS)"),
        ("TEST 6", "Total Disagreement", "30 ddeg/s", "1629 us", "1429 us", "1769 us", "200, 340, 140", "FAIL-SAFE ENGAGED", "-9999 us (SAFE)"),
        ("TEST 7", "Watchdog Hang (N2)", "25 ddeg/s", "1507 us", "1500 us", "1507 us", "TIMEOUT 0x04", "WATCHDOG EXPIRED", "-9999 us (SAFE)"),
    ]

    header = f"{'Test':<8} {'Scenario':<20} {'Sensor':<11} {'Node 1':<9} {'Node 2':<9} {'Node 3':<9} {'Deltas':<14} {'Voter Status':<22} {'Commanded'}"
    print(header)
    print("-" * len(header))
    for d in data:
        print(f"{d[0]:<8} {d[1]:<20} {d[2]:<11} {d[3]:<9} {d[4]:<9} {d[5]:<9} {d[6]:<14} {d[7]:<22} {d[8]}")
    print("\n")

def print_pitch_tracking():
    print("=================================================================================")
    print(" 3. CONTINUOUS FLIGHT CONTROL LOOP ATTITUDE TRACKING (Kp = 0.3)")
    print("=================================================================================")
    rates = [0, 20, 45, 80, 50, 10, -20, -60, -90, -40, 0]
    for i, r in enumerate(rates):
        pwm = 1500 + int((r * 3) / 10)
        offset = pwm - 1500
        bar_len = abs(offset)
        if offset < 0:
            bar = " " * (30 - bar_len) + "<" + "#" * bar_len + "|" + " " * 30
        elif offset > 0:
            bar = " " * 30 + "|" + "#" * bar_len + ">" + " " * (30 - bar_len)
        else:
            bar = " " * 30 + "+" + " " * 30
        print(f"  Loop #{i+1:2d} | Rate: {r:4d} ddeg/s | [{bar}] | PWM: {pwm:4d} us")
    print("=================================================================================\n")

if __name__ == "__main__":
    print_banner()
    print_memory_map()
    print_voter_matrix()
    print_pitch_tracking()
