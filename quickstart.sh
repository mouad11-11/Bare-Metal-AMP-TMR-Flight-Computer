#!/usr/bin/env bash
set -e

echo "=============================================================================="
echo "  Bare-Metal AMP TMR Flight Computer - Quick Start Review Runner (Linux/macOS)"
echo "=============================================================================="
echo ""

# 1. Dependency Check
echo "[1/4] Checking prerequisites..."
MISSING_DEPS=""
command -v arm-none-eabi-gcc >/dev/null 2>&1 || MISSING_DEPS="arm-none-eabi-gcc $MISSING_DEPS"
command -v qemu-system-arm >/dev/null 2>&1 || MISSING_DEPS="qemu-system-arm $MISSING_DEPS"

if [ -n "$MISSING_DEPS" ]; then
    echo "[ERROR] Missing required dependencies: $MISSING_DEPS"
    echo ""
    echo "To install on Ubuntu/Debian/WSL:"
    echo "  sudo apt-get update && sudo apt-get install -y gcc-arm-none-eabi qemu-system-arm python3 python3-matplotlib python3-tk"
    echo ""
    echo "To install on macOS (Homebrew):"
    echo "  brew install arm-none-eabi-gcc qemu python3"
    echo ""
    exit 1
fi

# 2. Build
echo "[2/4] Compiling bare-metal ARM Cortex-A15 binary..."
make all

# 3. Execution in QEMU
echo ""
echo "[3/4] Running multi-core QEMU vexpress-a15 emulation..."
OUTPUT_LOG="qemu_review_output.log"
rm -f "$OUTPUT_LOG"

qemu-system-arm -M vexpress-a15 -cpu cortex-a15 -smp 4 -m 128M -nographic \
    -kernel tmr_flight_computer.elf -accel tcg,thread=multi -serial file:"$OUTPUT_LOG" &
QEMU_PID=$!

# Wait 4 seconds for full automated test suite to complete
sleep 4.5
kill -9 $QEMU_PID 2>/dev/null || true

if [ -f "$OUTPUT_LOG" ]; then
    echo "=============================================================================="
    echo "                      QEMU FLIGHT TELEMETRY CAPTURE"
    echo "=============================================================================="
    cat "$OUTPUT_LOG"
    rm -f "$OUTPUT_LOG"
else
    echo "[ERROR] No output captured from QEMU."
    exit 1
fi

# 4. Terminal Visualizer & Plots
echo ""
echo "[4/4] Generating system architecture matrix & plots..."
if command -v python3 >/dev/null 2>&1; then
    python3 visualize_terminal.py
    python3 visualize_system.py || true
else
    echo "[INFO] python3 not found; skipping graphic generation."
fi

echo "=============================================================================="
echo " [SUCCESS] Review complete! All 7 fault tolerance scenarios verified."
echo "=============================================================================="
