# QForge — Quantum Simulation & Optimization Workbench

A **C++20 state-vector quantum simulator and variational optimization framework** implementing VQE, QAOA, parameter-shift gradients, classical optimization, numerical validation, and reproducible performance benchmarking.

QForge provides interactive 2–12 qubit circuit simulation, benchmark-oriented state-vector execution up to 14 qubits, and a modular quantum-classical optimization pipeline built from the simulation layer upward.

---

## Demo

> **Visual Tour**
> Circuit editor step-through · VQE ground-state energy convergence · QAOA MaxCut graph partition animation.

---

## Highlights

- **C++20 quantum simulation core** with UI-independent architecture
- **State-vector simulation** with support for 2–12 qubits interactively and 13–14 qubits for benchmarking
- **VQE implementation** for $\text{H}_2$, Pauli Hamiltonians, Ising, and XXZ models
- **QAOA MaxCut implementation** with configurable circuit depth ($p$ layers) and approximation-ratio tracking
- **Analytical parameter-shift gradients** for variational optimization
- **Classical optimizers:** Adam, Momentum GD, COBYLA / Nelder-Mead Simplex, and SPSA
- **40 Catch2 test cases / 110 assertions** covering simulation and numerical correctness
- **Numerical cross-validation** against exact linear-algebra references
- **State-vector and gate-throughput benchmarks** across 2–14 qubits
- **OpenQASM 2.0 / JSON circuit serialization**
- **Cross-platform CMake build** with Linux and Windows packaging

---

## Why This Project?

State-vector quantum simulation has exponential computational and memory scaling with qubit count. QForge explores this constraint while implementing the complete quantum-classical optimization workflow in C++20.

The system exposes the full pipeline:

```text
Circuit → State Vector → Hamiltonian Expectation → Quantum Gradient → Classical Optimizer → Convergence
```

Rather than treating the simulator as a black box, each stage is independently inspectable, testable, and benchmarkable.

---

## Architecture

```text
                 QForge
                    │
        ┌───────────┴───────────┐
        │                       │
   Quantum Core              Desktop UI
        │                       │
 ┌──────┼────────┐        Dear ImGui
 │      │        │          + GLFW
Circuit Pauli  Optimizer       │
 │      │        │             │
 └──────┼────────┘             │
        │                      │
        └───────────┬──────────┘
                    │
              VQE / QAOA
                    │
             Benchmarks
             & Validation
```

### Optimization Pipeline

```text
Parameterized Circuit U(θ)
        ↓
Quantum State-Vector Simulator |ψ(θ)⟩
        ↓
Pauli Hamiltonian Expectation ⟨ψ(θ)| H |ψ(θ)⟩
        ↓
Analytical Parameter-Shift Gradient ∇_θ ⟨H⟩
        ↓
Classical Optimizer (Adam / Momentum GD / COBYLA / SPSA)
        ↓
Update Parameters θ ← θ - α·g
        ↓
Run Circuit Again ↺ (Convergence Tracking)
```

### Engine Directory Structure

```text
quantum-core/        Pure C++20 quantum engine (no UI dependencies)
  pauli.hpp/cpp      Pauli operators, Hamiltonian algebra, exact ground state solver
  ansatz.hpp/cpp     Parameterized circuits (2-qubit minimal, HEA, custom, QAOA)
  qaoa.hpp/cpp       MaxCut graph engine, cost Hamiltonians, cut evaluator
  optimizer.hpp/cpp  Parameter-shift gradients, Adam, Momentum GD, COBYLA, SPSA
  vqe.hpp/cpp        VQE & QAOA experiment orchestrator, 2D landscape evaluator
  simulator.hpp/cpp  State vector simulator, step debugger, shot sampling
  circuit.hpp/cpp    Circuit data model, gate ordering, depth/count utilities
  state_vector.hpp/cpp  Complex amplitude array, norm, probability accessors
ui/                  Dear ImGui + GLFW desktop application
  app.cpp            Main application loop & mode coordinator
  vqe_view.cpp       VQE Studio UI, convergence charts, 2D landscape heatmap
  qaoa_view.cpp      QAOA MaxCut Studio UI, graph partition visualizer
tests/               Catch2 test suite (40 test cases, 110 assertions)
  test_gates.cpp     Gate correctness, simulator, JSON, QASM, Bloch vector
  test_vqe_qaoa.cpp  VQE/QAOA/Pauli/Gradient verification
  test_qasm.cpp      OpenQASM import/export round-trip tests
  benchmark_suite.cpp  Standalone performance benchmark executable
scripts/
  validate_reference.py Pure-Python numerical cross-validation against exact linear algebra references
```

The engine is fully decoupled from Dear ImGui. This means the core is independently testable, benchmarkable, and replaceable with any alternative frontend. The UI communicates with the quantum core through explicit simulation and optimization interfaces, allowing the computational engine to be tested and benchmarked independently of rendering.

---

## Core Capabilities

### 1. Interactive State-Vector Simulation (2–12 Qubits)
* Step-by-step gate execution, Bloch sphere projections, complex amplitude accessors, multi-shot sampling, and OpenQASM / JSON import/export.

### 2. Variational Quantum Eigensolver (VQE) Studio
* **Ground-state estimation for $\text{H}_2$ ($\text{STO-3G}$ minimal-basis Hamiltonian)** and configurable Pauli Hamiltonians including $X \otimes X + Z \otimes Z$, Transverse-Field Ising, and Heisenberg XXZ models.
* Custom Pauli string parser (`c_k · P_k`).
* Parameterized ansätze: 2-qubit minimal $[R_y \rightarrow \text{CNOT} \rightarrow R_y]$, Hardware-Efficient Ansatz (HEA), and custom circuit binding.
* Real-time convergence plot with exact theoretical eigenvalue baseline ($E_0$).
* 2D parameter energy landscape heatmap with animated optimizer trajectory path.

### 3. QAOA MaxCut Studio
* Combinatorial graph optimization on Triangle, 4-Cycle Square, Bowtie, and 3-Regular graphs.
* Parameterized cost unitary $e^{-i\gamma H_C}$ and mixer unitary $e^{-i\beta H_M}$ compilation ($p$ layers).
* Interactive 2D graph visualizer with partition color-coding and cut edge highlighting.
* Optimal solution ranking and approximation ratio $\alpha = \langle C \rangle / C_{\text{max}}$ tracking.

### 4. Analytical Parameter-Shift Gradients
Analytical parameter-shift evaluation for supported rotation parameters:
$$\frac{\partial \langle H \rangle}{\partial \theta_k} = \frac{\langle H \rangle_{\theta_k + \pi/2} - \langle H \rangle_{\theta_k - \pi/2}}{2}$$

### 5. Classical Optimization
Multiple optimization algorithms implemented in pure C++:
* **Adam** (Adaptive Moment Estimation)
* **Momentum Gradient Descent**
* **COBYLA** / **Nelder-Mead** Simplex (derivative-free)
* **SPSA** (Simultaneous Perturbation Stochastic Approximation)

### 6. Validation & Benchmarking
* Validated against exact linear algebra & analytical reference results.
* Benchmarked for state-vector scaling, gate throughput, and optimization step rate.

---

## Numerical Validation

QForge validates numerical behavior using three complementary approaches:

1. **Analytical / exact references** for known quantum states and small Hamiltonians.
2. **Automated regression tests** covering gates, state normalization, expectation values, gradients, VQE, QAOA, and serialization.
3. **Independent Python cross-validation** (`scripts/validate_reference.py`) against exact linear-algebra formulations.

Representative validation includes:
- Parameter-shift gradient accuracy
- VQE convergence against exact ground-state energies
- QAOA MaxCut objective values against known optimal cuts
- State-vector normalization stability
- JSON / OpenQASM round-trip consistency

---

## Tests

The test suite contains **40 Catch2 test cases and 110 assertions**, covering:

- Gate and state-vector correctness
- State normalization stability
- Simulator step-through execution
- JSON and OpenQASM round-trip serialization
- Bloch-vector calculations
- Pauli expectation values
- Parameter-shift gradient accuracy
- VQE convergence on $XX+ZZ$ and $\text{H}_2$
- QAOA MaxCut approximation ratios on Triangle and 4-Cycle graphs

```bash
cmake --build build --target qsim_tests
cd build && ctest --output-on-failure
# Or directly:
./build/tests/qsim_tests
```

---

## Benchmark Highlights

*Measured on 12th Gen Intel® Core™ i3-1215U (3.70 GHz), 16 GB RAM, MinGW GCC 13.1, Release build.*

| Workload | Result |
|---|---:|
| State-vector benchmark | 2–14 qubits ($16,384$ dim, $964.2\,\mu\text{s}$) |
| Gate throughput (CNOT 2q) | $9.017\text{ Mops/s}$ |
| VQE — $\text{H}_2$ step rate | $1,185\text{ iter/s}$ |
| VQE — $\text{H}_2$ final energy | **$-1.91536\text{ Ha}$** |
| $\text{H}_2$ exact reference | $-1.91537\text{ Ha}$ |
| Absolute error | **$\sim 1 \times 10^{-5}\text{ Ha}$** |
| QAOA — 4-cycle step rate | $116,171\text{ iter/s}$ |
| Test suite | 40 test cases / 110 assertions |

---

## Detailed Benchmark Results

### 1. State-Vector Memory & Allocation Time

| Qubits | State-Vector Dimension ($2^N$) | Memory Overhead | Reset / Alloc Time ($\mu$s) |
|:------:|:------------------------------:|:---------------:|:--------------------------:|
| 2 | 4 | 64 B | 62.4 $\mu$s |
| 4 | 16 | 256 B | 1.9 $\mu$s |
| 6 | 64 | 1.0 KB | 3.3 $\mu$s |
| 8 | 256 | 4.0 KB | 9.2 $\mu$s |
| 10 | 1,024 | 16.0 KB | 40.5 $\mu$s |
| 12 | 4,096 | 65.5 KB | 226.1 $\mu$s |
| 13 | 8,192 | 131.1 KB | 460.7 $\mu$s |
| 14 | 16,384 | 262.1 KB | 964.2 $\mu$s |

### 2. Quantum Gate Throughput

| Qubits | Gate Type | Total Gates | Execution Time (ms) | Throughput (Mops/s) |
|:------:|:---------:|:-----------:|:------------------:|:-------------------:|
| 2 | Hadamard (1q) | 50,000 | 73.16 ms | 0.683 Mops/s |
| 2 | CNOT (2q) | 50,000 | 5.55 ms | 9.017 Mops/s |
| 4 | Hadamard (1q) | 50,000 | 243.44 ms | 0.205 Mops/s |
| 4 | CNOT (2q) | 50,000 | 32.85 ms | 1.522 Mops/s |
| 8 | Hadamard (1q) | 50,000 | 3,956.27 ms | 0.013 Mops/s |
| 8 | CNOT (2q) | 50,000 | 353.33 ms | 0.142 Mops/s |
| 10 | CNOT (2q) | 5,000 | 103.66 ms | 0.048 Mops/s |
| 12 | CNOT (2q) | 500 | 33.87 ms | 0.015 Mops/s |

### 3. Variational Optimization Pipeline Performance

| Algorithm / Target | Parameters | Iterations | Total Time (ms) | Step Rate (iter/s) | Final Converged Energy / Value |
|:------------------|:----------:|:----------:|:---------------:|:------------------:|:------------------------------:|
| VQE ($X \otimes X + Z \otimes Z$, 2q) | 4 | 100 | 57.60 ms | 1,736 iter/s | **-1.99749** *(exact $E_0 = -2.0$)* |
| VQE ($\text{H}_2$ Molecule, STO-3G) | 4 | 100 | 84.34 ms | 1,185 iter/s | **-1.91536 Ha** *(exact $E_0 = -1.91537$)* |
| QAOA MaxCut (4-Cycle Graph, $p=1$) | 2 | 100 | 0.86 ms | 116,171 iter/s | **-3.00000** *(optimal cut $= 4$)* |
| QAOA MaxCut (6-Node Regular, $p=2$) | 4 | 60 | 964.24 ms | 62.23 iter/s | **-5.87537** *(near-optimal partition)* |

*The $\text{H}_2$ VQE result differs from the exact reference by approximately $0.00001\text{ Ha}$ on the reported benchmark configuration.*

---

## Current Limitations

- State-vector simulation scales exponentially as $\mathcal{O}(2^N)$.
- Interactive circuit editing is capped at 12 qubits for usability.
- 13–14 qubit execution is intended for benchmark and optimization workloads.
- The current simulator is a classical state-vector simulator and does not execute circuits on physical quantum hardware.
- Noise and density-matrix simulation are outside the current scope.

---

## Performance

| Qubit count | Intended workload | State-vector dimension |
|------------|-------------------|-----------------------:|
| 2–10 | Interactive circuit simulation | 4 – 1,024 |
| 11–12 | Interactive simulation with filtering | 2,048 – 4,096 |
| 13–14 | Benchmark / optimization workloads | 8,192 – 16,384 |

> These results are workload-specific and are intended to demonstrate scaling characteristics and provide reproducible reference measurements rather than represent peak simulator performance. The circuit editor GUI is intentionally capped at 12 qubits.

---

## Design Goals

- **Separation of concerns** — quantum computation is independent of the UI.
- **Numerical correctness** — algorithms are verified against analytical and exact reference results.
- **Reproducibility** — benchmark workloads and optimization experiments can be rerun consistently.
- **Observability** — intermediate quantum states, optimization trajectories, and convergence behavior are exposed visually.
- **Extensibility** — new gates, ansätze, Hamiltonians, optimizers, and frontends can be added without coupling them to the UI.

---

## Requirements

- Linux (tested on Kali/Debian) or Windows 10/11
- CMake ≥ 3.20
- C++20 compiler (g++ 11+, Clang 13+, or MSVC 2022)

Dependencies are **vendored in `third_party/`** (Dear ImGui, GLFW 3.4, Catch2 3.5.2, nlohmann/json).

System packages needed for OpenGL/GLFW windowing on Debian/Linux:

```bash
sudo apt install build-essential cmake git libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev
```

---

## Build

```bash
cd /path/to/quantum-circuit-lab
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

---

## Usage

1. Select qubit count (2–12) in **Controls**
2. Click a gate in the palette, then click a circuit cell to place it
3. **CNOT / CZ / SWAP:** click control wire, then target wire
4. **CCX (Toffoli):** click control1, control2, then target
5. **Eraser:** select Eraser, click a gate cell
6. **Run All** or **Step >>** / **<< Step** to debug gate-by-gate
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
| Measurement | Shows probabilities; no wavefunction collapse by default |

---

## Presets (18 included)

### Circuit Algorithms

| File | Description |
|------|-------------|
| `bell_phi_plus.json` | Bell \|Φ+⟩ state |
| `superposition.json` | H on q0 — single-qubit superposition |
| `phase_kick.json` | H–CZ–H phase kickback demo |
| `swap_test.json` | X + SWAP |
| `entangle_y.json` | Bell state + Y on q1 |
| `ghz_3.json` | 3-qubit GHZ state |
| `teleportation.json` | Quantum teleportation layout |
| `superdense_coding.json` | Superdense coding protocol |
| `qft_3.json` | 3-qubit Quantum Fourier Transform |
| `grover_2.json` | 2-qubit Grover's search |
| `deutsch_jozsa.json` | Deutsch-Jozsa algorithm |
| `bernstein_vazirani.json` | Bernstein-Vazirani algorithm |

### VQE Presets

| File | Description |
|------|-------------|
| `vqe_xx_zz.json` | VQE on X⊗X + Z⊗Z Hamiltonian |
| `vqe_h2_molecule.json` | VQE ground state of H₂ molecule (STO-3G) |
| `vqe_ising_model.json` | VQE on Transverse-Field Ising model |
| `vqc_demo.json` | Generic parameterized variational circuit |

### QAOA Presets

| File | Description |
|------|-------------|
| `qaoa_maxcut_triangle.json` | QAOA MaxCut on triangle graph (3 nodes) |
| `qaoa_maxcut_4cycle.json` | QAOA MaxCut on 4-cycle square graph |

---

## JSON Circuit Format

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

Parametric gates use `"param1"` (angle in radians); `U` also accepts `"param2"` (φ) and `"param3"` (λ).

---

## OpenQASM 2.0 Support

Imports and exports OpenQASM 2.0 circuit definitions (`OPENQASM 2.0;`, `include "qelib1.inc";`, `qreg`, `h`, `cx`, `cz`, `swap`, `rx`, `ry`, `rz`).

---

## Portable Release

### Linux x86_64

Build a self-contained tarball (~750 KB):

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

### Windows x64

**On Windows** (CMake + MinGW or Visual Studio):

```powershell
.\scripts\package-windows.ps1 0.1.0
```

Output: `dist/QuantumCircuitLab-0.1.0-win64.zip`

---

## Tech Stack

| Component | Technology |
|---|---|
| Language | C++20 |
| Quantum engine | Custom state-vector simulator |
| GUI | Dear ImGui + GLFW + OpenGL 3.3 |
| Serialization | nlohmann/json |
| Testing | Catch2 v3.5.2 + CTest |
| Build | CMake 3.20+ |
| Validation | Pure Python / analytical exact references |
| Packaging | Linux tarball + Windows ZIP |

Dear ImGui + GLFW were selected to keep the desktop application lightweight and self-contained while maintaining a clear separation from the C++ simulation core.

---

## License

Educational / portfolio use. Extend freely.
