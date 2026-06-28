# Quantum Circuit Lab

A desktop-first **state-vector quantum circuit simulator** for interactive learning — inspired by a 2-qubit web toy, scaled up to **2–12 qubits** with a visual circuit editor, step debugger, Bloch sphere projections, and preset library.

Built by **Mehar Ali** as an educational lab tool (not a production cloud SDK).

## Architecture

```
quantum-core/   Pure C++20 simulation engine (no UI dependencies)
ui/             Dear ImGui + GLFW desktop application
  app.cpp       Main UI and simulation wiring
  util.cpp      Settings, paths, file dialogs, app icon
  circuit_png.cpp  PNG export
presets/        JSON circuit library
tests/          Catch2 unit tests
```

### Engine vs UI split

| Layer | Responsibility |
|-------|----------------|
| **quantum-core** | State vector, gate application, circuit model, JSON I/O, Bloch math |
| **ui** | Circuit canvas, gate palette, panels, presets, undo/redo |

The engine exposes `StateVector`, `apply_gate()`, `Circuit`, and `Simulator`. The UI never touches amplitude indexing directly — it edits `Circuit` objects and delegates simulation to `Simulator`.

### State-vector update algorithm

**Qubit indexing:** `q0` is the **top wire** and **MSB** in ket labels. For `n` qubits, basis index  
`i = Σ bit(q_k) · 2^(n−1−k)`.

**Single-qubit gate on qubit `q`:** Let `mask = 1 << (n−1−q)`. For each index `i` where bit `q` is 0, pair with `j = i | mask`. Apply the 2×2 matrix to `(α_i, α_j)` without building a full 2^n × 2^n matrix:

```
α_i ← U00·α_i + U01·α_j
α_j ← U10·α_i + U11·α_j
```

**CNOT(control `c`, target `t`):** For each `i` where control bit is 1 and target bit is 0, swap amplitudes with `j = i | target_mask`. This is equivalent to XOR-ing the target bit when control is |1⟩.

**Normalization:** After every gate, amplitudes are divided by the L2 norm (with epsilon guard for near-zero states).

Memory is **O(2^n)** — the UI warns above 12 qubits.

## Requirements

- Linux (tested on Kali/Debian)
- CMake ≥ 3.20
- C++20 compiler (g++ 11+)
Dependencies are **vendored in `third_party/`** (Dear ImGui, GLFW, Catch2, nlohmann/json) — no Qt install required.

System packages needed for OpenGL/GLFW windowing:

```bash
sudo apt install build-essential cmake git libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev
```

On Debian/Kali, GLFW is built from source by CMake. X11 and Wayland headers are pulled in automatically.

## Build

```bash
cd "/home/mehar/Quantum Simulator"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Run

```bash
./build/ui/quantum-lab
# Optional: custom presets directory
./build/ui/quantum-lab /path/to/presets
```

When run from a portable bundle, presets are loaded automatically from the `presets/` folder next to the binary.

## Portable release (share with others)

Build a self-contained **Linux x86_64** tarball (~750 KB) you can upload or send:

```bash
./scripts/package-linux.sh 0.1.0
```

Output: `dist/QuantumCircuitLab-0.1.0-linux-x86_64.tar.gz`

Recipients extract and run:

```bash
tar xzf QuantumCircuitLab-0.1.0-linux-x86_64.tar.gz
cd QuantumCircuitLab-0.1.0-linux-x86_64
./run.sh
```

The bundle includes the binary, all presets, and a README. Recipients need **64-bit Linux** with **OpenGL 3.3+** (standard Mesa on Ubuntu/Debian/Fedora/Kali). No compiler or CMake required.

### Windows x64

**On Windows** (with [CMake](https://cmake.org/) and MinGW or Visual Studio):

```powershell
.\scripts\package-windows.ps1 0.1.0
```

**From Linux** (cross-compile with MinGW-w64):

```bash
sudo apt install mingw-w64 zip   # once
./scripts/package-windows.sh 0.1.0
```

Output: `dist/QuantumCircuitLab-0.1.0-win64.zip`

Recipients extract and double-click **`run.bat`** (or `quantum-lab.exe`). Requires **Windows 10/11 64-bit** with GPU drivers. Unsigned builds may show a SmartScreen prompt — choose “Run anyway”.

User settings on Windows: `%APPDATA%\quantum-lab\`

## Tests

```bash
cmake --build build --target qsim_tests
cd build && ctest --output-on-failure
# Or directly:
./build/tests/qsim_tests
```

## Usage

1. Select qubit count (2–12) in **Controls**
2. Click a gate in the palette, then click a circuit cell to place it
3. **CNOT / CZ / SWAP:** click control wire, then target wire
4. **CCX (Toffoli):** click control1, control2, then target
5. **Eraser:** select Eraser, click a gate cell
6. **Run All** or **Step >>** / **<< Step** to debug
7. Load presets from the Controls panel
8. **File → Export JSON** saves the circuit

### Panels

- **Measurement Probabilities** — basis state bars (optional >0.1% filter)
- **State Vector** — complex amplitudes
- **Qubit View** — P(|0⟩)/P(|1⟩) and Bloch ball projection

### Conventions

| Topic | Convention |
|-------|------------|
| Initial state | \|0…0⟩ |
| Bit order | q0 = MSB ( \|10⟩ → index 2 for 2 qubits ) |
| Barriers | Visual only — no simulation effect |
| Measurement | Shows probabilities; no collapse by default |

## Presets (8 included)

| File | Description |
|------|-------------|
| `bell_phi_plus.json` | Bell \|Φ+⟩ state |
| `superposition.json` | H on q0 |
| `phase_kick.json` | H–CZ–H phase demo |
| `swap_test.json` | X + SWAP |
| `entangle_y.json` | Bell + Y on q1 |
| `ghz_3.json` | 3-qubit GHZ |
| `teleportation.json` | Teleportation layout |

## JSON circuit format

```json
{
  "version": 1,
  "num_qubits": 2,
  "description": "optional",
  "operations": [
    { "gate": "H", "column": 0, "qubit": 0 },
    { "gate": "CNOT", "column": 1, "qubit": 0, "qubit2": 1 }
  ]
}
```

Supported gates: `I H X Y Z S T Sdg Tdg Rx Ry Rz U CNOT CZ SWAP CCX CRx CRy CRz Barrier Measure`

Parametric gates use `"param1"` (angle in radians); `U` also accepts `param2` (φ) and `param3` (λ).

## Performance

- **≤10 qubits:** interactive gate updates on typical hardware
- **11–12 qubits:** usable with probability filtering (4096–8192 amplitudes)
- **>12 qubits:** not recommended — O(2^n) memory grows quickly

## Tech choice note

Qt 6 was the preferred stack but is not required — this project uses **Dear ImGui + GLFW + OpenGL** for a lightweight, self-contained cross-platform build with no system Qt install.

## License

Educational / portfolio use. Extend freely.
