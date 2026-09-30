"""
Bare-Metal AMP TMR Flight Computer - Native Python Desktop Telemetry Monitor
============================================================================
An open-source desktop visualization GUI built with Tkinter and Matplotlib.
Allows launching QEMU, streaming live UART telemetry, and visualizing the
2oo3 voter, triplicated node outputs, and pitch tracking in real time.
"""

import os
import re
import sys
import time
import threading
import subprocess
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox

import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import numpy as np

class TmrFlightMonitorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Bare-Metal AMP TMR Flight Computer - Avionics Telemetry Monitor")
        self.root.geometry("1100x750")
        self.root.configure(bg="#0f172a")

        self.qemu_proc = None
        self.running = False

        self._setup_style()
        self._build_ui()

    def _setup_style(self):
        style = ttk.Style()
        style.theme_use('clam')
        style.configure("TFrame", background="#0f172a")
        style.configure("Card.TFrame", background="#1e293b", relief="solid", borderwidth=1)
        style.configure("TLabel", background="#0f172a", foreground="#f8fafc", font=("Segoe UI", 10))
        style.configure("Header.TLabel", background="#1e293b", foreground="#f8fafc", font=("Segoe UI", 13, "bold"))
        style.configure("Sub.TLabel", background="#1e293b", foreground="#94a3b8", font=("Segoe UI", 9))
        style.configure("Run.TButton", font=("Segoe UI", 10, "bold"), background="#10b981", foreground="#ffffff")

    def _build_ui(self):
        # Top Header Card
        header_frame = tk.Frame(self.root, bg="#1e293b", highlightbackground="#334155", highlightthickness=1, padx=15, pady=12)
        header_frame.pack(fill="x", padx=15, pady=10)

        title_lbl = tk.Label(header_frame, text="Bare-Metal AMP TMR Flight Computer Monitor", bg="#1e293b", fg="#f8fafc", font=("Segoe UI", 15, "bold"))
        title_lbl.pack(anchor="w")

        sub_lbl = tk.Label(header_frame, text="ARM Cortex-A15 Quad-Core • SIFT Architecture • 2oo3 Bounded Majority Voter (|Δ| ≤ 5 μs)", bg="#1e293b", fg="#94a3b8", font=("Segoe UI", 9))
        sub_lbl.pack(anchor="w", pady=(2, 8))

        # Control Bar
        ctrl_bar = tk.Frame(header_frame, bg="#1e293b")
        ctrl_bar.pack(fill="x")

        self.btn_run = tk.Button(ctrl_bar, text="▶  Run QEMU Simulation", command=self.start_simulation, bg="#10b981", fg="#ffffff", activebackground="#059669", activeforeground="#ffffff", font=("Segoe UI", 9, "bold"), padx=12, pady=5, relief="flat", cursor="hand2")
        self.btn_run.pack(side="left", padx=(0, 10))

        self.btn_clear = tk.Button(ctrl_bar, text="🧹  Clear Console", command=self.clear_console, bg="#334155", fg="#cbd5e1", activebackground="#475569", activeforeground="#ffffff", font=("Segoe UI", 9), padx=10, pady=5, relief="flat", cursor="hand2")
        self.btn_clear.pack(side="left")

        # Core Status Indicator Pills
        core_box = tk.Frame(ctrl_bar, bg="#1e293b")
        core_box.pack(side="right")

        self.core_indicators = {}
        core_names = [("Core 0", "Arbiter"), ("Core 1", "Node 1"), ("Core 2", "Node 2"), ("Core 3", "Node 3")]
        for cid, cname in core_names:
            pill = tk.Label(core_box, text=f"● {cid}: {cname}", bg="#0f172a", fg="#10b981", font=("Segoe UI", 8, "bold"), padx=8, pady=4, relief="solid", bd=1)
            pill.pack(side="left", padx=3)
            self.core_indicators[cid] = pill

        # Main Split Area (Left: Live Matplotlib Plots, Right: Scrolled UART Terminal)
        content_frame = tk.Frame(self.root, bg="#0f172a")
        content_frame.pack(fill="both", expand=True, padx=15, pady=(0, 15))

        # Left Frame: Matplotlib Live Plots
        plot_frame = tk.Frame(content_frame, bg="#1e293b", highlightbackground="#334155", highlightthickness=1)
        plot_frame.pack(side="left", fill="both", expand=True, padx=(0, 10))

        self.fig, (self.ax_bar, self.ax_wave) = plt.subplots(2, 1, figsize=(6.5, 5.5), dpi=100)
        self.fig.patch.set_facecolor('#1e293b')
        self.ax_bar.set_facecolor('#0f172a')
        self.ax_wave.set_facecolor('#0f172a')

        self.canvas = FigureCanvasTkAgg(self.fig, master=plot_frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        # Right Frame: UART Log Console
        log_frame = tk.Frame(content_frame, bg="#1e293b", width=380, highlightbackground="#334155", highlightthickness=1, padx=10, pady=10)
        log_frame.pack(side="right", fill="both", expand=False)
        log_frame.pack_propagate(False)

        log_title = tk.Label(log_frame, text="PL011 UART Telemetry Log (0x1C090000)", bg="#1e293b", fg="#f8fafc", font=("Segoe UI", 9, "bold"))
        log_title.pack(anchor="w", pady=(0, 5))

        self.console = scrolledtext.ScrolledText(log_frame, wrap="word", bg="#020617", fg="#10b981", insertbackground="#ffffff", font=("Consolas", 8), borderwidth=0)
        self.console.pack(fill="both", expand=True)

        self._draw_placeholder()

    def _draw_placeholder(self):
        """Draws initial plot placeholders."""
        self.ax_bar.clear()
        self.ax_wave.clear()

        # Bar chart placeholder
        nodes = ['Node 1\n(Core 1)', 'Node 2\n(Core 2)', 'Node 3\n(Core 3)', '2oo3 Voter\nCommand']
        pwms = [1500, 1500, 1500, 1500]
        colors = ['#38bdf8', '#fbbf24', '#a78bfa', '#10b981']
        bars = self.ax_bar.bar(nodes, pwms, color=colors, width=0.5, edgecolor='#475569')
        self.ax_bar.set_title("Triplicated Node Outputs vs 2oo3 Voter", color='#f8fafc', fontsize=9, pad=6)
        self.ax_bar.set_ylim(800, 2200)
        self.ax_bar.set_ylabel("Actuator PWM (μs)", color='#94a3b8', fontsize=8)
        self.ax_bar.tick_params(colors='#94a3b8', labelsize=8)
        self.ax_bar.grid(True, linestyle='--', alpha=0.3, color='#334155')

        # Waveform placeholder
        x = np.arange(1, 12)
        rates = [0, 20, 45, 80, 50, 10, -20, -60, -90, -40, 0]
        voted_pwm = [1500 + int(r * 0.3) for r in rates]
        self.ax_wave.plot(x, rates, 'o-', color='#818cf8', label='Pitch Rate (ddeg/s)')
        self.ax_wave.plot(x, voted_pwm, 's-', color='#10b981', label='Actuator PWM (μs)')
        self.ax_wave.set_title("Pitch Rate Ingest vs Commanded PWM", color='#f8fafc', fontsize=9, pad=6)
        self.ax_wave.set_xlabel("Flight Loop Iteration", color='#94a3b8', fontsize=8)
        self.ax_wave.tick_params(colors='#94a3b8', labelsize=8)
        self.ax_wave.grid(True, linestyle='--', alpha=0.3, color='#334155')
        leg = self.ax_wave.legend(facecolor='#0f172a', edgecolor='#334155', fontsize=7.5)
        for t in leg.get_texts(): t.set_color('#cbd5e1')

        self.fig.tight_layout()
        self.canvas.draw()

    def log(self, text):
        self.console.insert("end", text + "\n")
        self.console.see("end")

    def clear_console(self):
        self.console.delete("1.0", "end")

    def start_simulation(self):
        if self.running:
            return
        self.btn_run.config(state="disabled", text="Simulation Running...")
        self.clear_console()
        threading.Thread(target=self._run_qemu_worker, daemon=True).start()

    def _run_qemu_worker(self):
        self.running = True
        self.log("[INFO] Building project binary...")

        if os.path.exists("build.bat"):
            p = subprocess.run(["cmd", "/c", "build.bat"], capture_output=True, text=True)
            if p.returncode != 0:
                self.log(f"[ERROR] Build failed:\n{p.stderr}")
                self.btn_run.config(state="normal", text="▶  Run QEMU Simulation")
                self.running = False
                return

        qemu_exe = "D:\\tools\\qemu\\qemu-system-arm.exe"
        if not os.path.exists(qemu_exe):
            qemu_exe = "qemu-system-arm.exe"

        qemu_cmd = [
            qemu_exe,
            "-M", "vexpress-a15",
            "-cpu", "cortex-a15",
            "-smp", "4",
            "-nographic",
            "-kernel", "tmr_flight_computer.elf",
            "-accel", "tcg,thread=multi"
        ]

        self.log(f"[INFO] Launching QEMU: {' '.join(qemu_cmd)}")
        try:
            proc = subprocess.Popen(qemu_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
            self.qemu_proc = proc

            for line in iter(proc.stdout.readline, ''):
                if not line: break
                clean_line = line.rstrip()
                self.root.after(0, self.log, clean_line)
                self.root.after(0, self._parse_and_update_plot, clean_line)
                if "System entering standby" in clean_line:
                    break

            proc.terminate()
        except Exception as e:
            self.log(f"[EXCEPTION] {e}")
        finally:
            self.running = False
            self.root.after(0, lambda: self.btn_run.config(state="normal", text="▶  Run QEMU Simulation"))

    def _parse_and_update_plot(self, line):
        # Update on Node outputs line: Node Outputs : [Node 1: 1500 us] [Node 2: 1500 us] [Node 3: 1500 us]
        m_nodes = re.search(r'Node Outputs\s*:\s*\[Node 1:\s*(-?\d+)\s*us\]\s*\[Node 2:\s*(-?\d+)\s*us\]\s*\[Node 3:\s*(-?\d+)\s*us\]', line)
        if m_nodes:
            y1, y2, y3 = int(m_nodes.group(1)), int(m_nodes.group(2)), int(m_nodes.group(3))
            self.last_y = (y1, y2, y3)

        m_pwm = re.search(r'Commanded PWM:\s*(-?\d+)', line)
        if m_pwm and hasattr(self, 'last_y'):
            pwm = int(m_pwm.group(1))
            y1, y2, y3 = self.last_y
            self._update_bar_plot(y1, y2, y3, pwm)

    def _update_bar_plot(self, y1, y2, y3, pwm):
        self.ax_bar.clear()
        nodes = ['Node 1', 'Node 2', 'Node 3', '2oo3 Voter']
        pwms = [y1, y2, y3, pwm if pwm != -9999 else 850]
        colors = ['#38bdf8', '#fbbf24', '#a78bfa', '#10b981' if pwm != -9999 else '#f43f5e']
        self.ax_bar.bar(nodes, pwms, color=colors, width=0.5, edgecolor='#475569')
        if pwm == -9999:
            self.ax_bar.text(3, 900, "FAIL-SAFE\n-9999 μs", color='#f43f5e', fontweight='bold', ha='center', fontsize=7.5)
        self.ax_bar.set_title(f"Live Frame Outputs (Commanded PWM: {pwm} μs)", color='#f8fafc', fontsize=9, pad=6)
        self.ax_bar.set_ylim(800, 2200)
        self.ax_bar.set_ylabel("Actuator PWM (μs)", color='#94a3b8', fontsize=8)
        self.ax_bar.tick_params(colors='#94a3b8', labelsize=8)
        self.ax_bar.grid(True, linestyle='--', alpha=0.3, color='#334155')
        self.canvas.draw()

def main():
    root = tk.Tk()
    app = TmrFlightMonitorApp(root)
    root.mainloop()

if __name__ == "__main__":
    main()
