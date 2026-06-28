Quantum Circuit Lab — Portable (Linux x86_64)
==============================================

Interactive state-vector quantum circuit simulator (2–12 qubits).

Quick start
-----------
  Option A: double-click or run ./run.sh
  Option B: ./quantum-lab

Requirements
------------
  • 64-bit Linux (glibc-based: Ubuntu, Debian, Fedora, Kali, etc.)
  • OpenGL 3.3+ (Mesa or proprietary GPU driver)
  • X11 or Wayland desktop

If the window does not open, install Mesa OpenGL:
  sudo apt install libgl1   # Debian/Ubuntu/Kali

Contents
--------
  quantum-lab   Application binary
  presets/      Built-in circuit library (Bell, GHZ, Grover, …)
  run.sh        Launcher (sets presets path automatically)
  README.txt    This file

User data (saved automatically on first run)
--------------------------------------------
  ~/.config/quantum-lab/   Settings and user presets

Tips
----
  • File menu: import/export JSON, OpenQASM, PNG
  • Pick a gate, click a circuit cell; CNOT/CZ need two clicks
  • Enable Auto-run to update probabilities as you edit

Built by Mehar Ali — educational use.
