#include "qsim/gates.hpp"

#include <cmath>
#include <cstdio>
#include <random>
#include <sstream>
#include <stdexcept>

namespace qsim {

namespace {

constexpr double kPi = 3.14159265358979323846;

void require_qubit_in_range(int n, int qubit, const char* label) {
    if (qubit < 0 || qubit >= n) {
        throw std::out_of_range(std::string(label) + " qubit index out of range");
    }
}

void require_distinct_qubits(int a, int b) {
    if (a == b) {
        throw std::invalid_argument("qubit indices must differ");
    }
}

void require_distinct_qubits(int a, int b, int c) {
    if (a == b || a == c || b == c) {
        throw std::invalid_argument("qubit indices must be distinct");
    }
}

Matrix2 identity() {
    return {{{1.0, 0.0}, {0.0, 1.0}}};
}

Matrix2 hadamard() {
    const double h = 1.0 / std::sqrt(2.0);
    return {{{h, h}, {h, -h}}};
}

Matrix2 pauli_x() { return {{{0.0, 1.0}, {1.0, 0.0}}}; }
Matrix2 pauli_y() { return {{{0.0, Complex(0.0, -1.0)}, {Complex(0.0, 1.0), 0.0}}}; }
Matrix2 pauli_z() { return {{{1.0, 0.0}, {0.0, -1.0}}}; }

Matrix2 phase_s() { return {{{1.0, 0.0}, {0.0, Complex(0.0, 1.0)}}}; }
Matrix2 phase_t() {
    const Complex phase(std::cos(kPi / 4.0), std::sin(kPi / 4.0));
    return {{{1.0, 0.0}, {0.0, phase}}};
}

Matrix2 rx(double theta) {
    const double c = std::cos(theta / 2.0);
    const double s = std::sin(theta / 2.0);
    return {{{c, Complex(0.0, -s)}, {Complex(0.0, -s), c}}};
}

Matrix2 ry(double theta) {
    const double c = std::cos(theta / 2.0);
    const double s = std::sin(theta / 2.0);
    return {{{c, -s}, {s, c}}};
}

Matrix2 rz(double theta) {
    const Complex phase_neg(std::cos(-theta / 2.0), std::sin(-theta / 2.0));
    const Complex phase_pos(std::cos(theta / 2.0), std::sin(theta / 2.0));
    return {{{phase_neg, 0.0}, {0.0, phase_pos}}};
}

/// U(θ, φ, λ) from Qiskit convention
Matrix2 u_gate(double theta, double phi, double lambda) {
    const double c = std::cos(theta / 2.0);
    const double s = std::sin(theta / 2.0);
    const Complex exp_i_phi(std::cos(phi), std::sin(phi));
    const Complex exp_i_lambda(std::cos(lambda), std::sin(lambda));
    const Complex exp_i_phi_lambda(std::cos(phi + lambda), std::sin(phi + lambda));
    Matrix2 m{};
    m[0][0] = c;
    m[0][1] = -exp_i_lambda * s;
    m[1][0] = exp_i_phi * s;
    m[1][1] = exp_i_phi_lambda * c;
    return m;
}

}  // namespace

const GateInfo& gate_info(GateKind kind) {
    static const GateInfo kTable[] = {
        {GateKind::I, "I", "Identity", "No operation; leaves the qubit unchanged.", 1, false},
        {GateKind::H, "H", "Hadamard", "Creates equal superposition: |0> -> (|0>+|1>)/sqrt(2).", 1, false},
        {GateKind::X, "X", "Pauli-X", "Bit flip, quantum NOT gate.", 1, false},
        {GateKind::Y, "Y", "Pauli-Y", "Bit and phase flip: i|0><1| - i|1><0|.", 1, false},
        {GateKind::Z, "Z", "Pauli-Z", "Phase flip on |1>.", 1, false},
        {GateKind::S, "S", "Phase S", "Quarter-turn phase: |1> -> i|1>.", 1, false},
        {GateKind::T, "T", "Phase T", "Pi/8 phase gate (T gate).", 1, false},
        {GateKind::Sdg, "S+", "S-dagger", "Inverse S gate.", 1, false},
        {GateKind::Tdg, "T+", "T-dagger", "Inverse T gate.", 1, false},
        {GateKind::Rx, "Rx", "Rotate X", "Rotation about X axis by angle (radians).", 1, true},
        {GateKind::Ry, "Ry", "Rotate Y", "Rotation about Y axis by angle (radians).", 1, true},
        {GateKind::Rz, "Rz", "Rotate Z", "Rotation about Z axis by angle (radians).", 1, true},
        {GateKind::U, "U", "U gate", "Arbitrary single-qubit unitary U(theta, phi, lambda).", 1, true},
        {GateKind::CNOT, "CX", "CNOT", "Controlled-NOT: flips target when control is |1>.", 2, false},
        {GateKind::CZ, "CZ", "Controlled-Z", "Applies Z on target when control is |1>.", 2, false},
        {GateKind::SWAP, "SW", "SWAP", "Exchanges the states of two qubits.", 2, false},
        {GateKind::CCX, "CCX", "Toffoli", "Doubly-controlled X (CCX).", 3, false},
        {GateKind::CRx, "CRx", "Ctrl-Rx", "Controlled Rx rotation.", 2, true},
        {GateKind::CRy, "CRy", "Ctrl-Ry", "Controlled Ry rotation.", 2, true},
        {GateKind::CRz, "CRz", "Ctrl-Rz", "Controlled Rz rotation.", 2, true},
        {GateKind::Barrier, "||", "Barrier", "Visual separator only; no effect on simulation.", 1, false},
        {GateKind::Measure, "M", "Measure", "Measurement (shows probabilities; optional collapse).", 1, false},
    };
    const auto idx = static_cast<std::size_t>(kind);
    if (idx >= sizeof(kTable) / sizeof(kTable[0])) {
        throw std::out_of_range("unknown gate kind");
    }
    return kTable[idx];
}

Matrix2 gate_matrix(GateKind kind, double param1, double param2, double param3) {
    switch (kind) {
        case GateKind::I: return identity();
        case GateKind::H: return hadamard();
        case GateKind::X: return pauli_x();
        case GateKind::Y: return pauli_y();
        case GateKind::Z: return pauli_z();
        case GateKind::S: return phase_s();
        case GateKind::T: return phase_t();
        case GateKind::Sdg: {
            Matrix2 s = phase_s();
            return {{{std::conj(s[0][0]), std::conj(s[1][0])}, {std::conj(s[0][1]), std::conj(s[1][1])}}};
        }
        case GateKind::Tdg: {
            Matrix2 t = phase_t();
            return {{{std::conj(t[0][0]), std::conj(t[1][0])}, {std::conj(t[0][1]), std::conj(t[1][1])}}};
        }
        case GateKind::Rx: return rx(param1);
        case GateKind::Ry: return ry(param1);
        case GateKind::Rz: return rz(param1);
        case GateKind::U: return u_gate(param1, param2, param3);
        default:
            throw std::invalid_argument("gate_matrix: not a single-qubit gate");
    }
}

void apply_1q_gate(StateVector& state, int qubit, const Matrix2& matrix) {
    const int n = state.num_qubits();
    if (qubit < 0 || qubit >= n) {
        throw std::out_of_range("qubit index out of range");
    }
    const int mask = qubit_mask(n, qubit);
    auto& amps = state.amplitudes();
    for (std::size_t i = 0; i < amps.size(); ++i) {
        if ((static_cast<int>(i) & mask) == 0) {
            const std::size_t j = i | static_cast<std::size_t>(mask);
            const Complex a0 = amps[i];
            const Complex a1 = amps[j];
            amps[i] = matrix[0][0] * a0 + matrix[0][1] * a1;
            amps[j] = matrix[1][0] * a0 + matrix[1][1] * a1;
        }
    }
    state.renormalize();
}

void apply_1q_gate(StateVector& state, int qubit, GateKind kind,
                   double param1, double param2, double param3) {
    apply_1q_gate(state, qubit, gate_matrix(kind, param1, param2, param3));
}

void apply_cnot(StateVector& state, int control, int target) {
    const int n = state.num_qubits();
    if (control < 0 || control >= n || target < 0 || target >= n || control == target) {
        throw std::invalid_argument("invalid CNOT qubits");
    }
    const int c_mask = qubit_mask(n, control);
    const int t_mask = qubit_mask(n, target);
    auto& amps = state.amplitudes();
    for (std::size_t i = 0; i < amps.size(); ++i) {
        if ((static_cast<int>(i) & c_mask) && !(static_cast<int>(i) & t_mask)) {
            const std::size_t j = i | static_cast<std::size_t>(t_mask);
            std::swap(amps[i], amps[j]);
        }
    }
}

void apply_cz(StateVector& state, int control, int target) {
    const int n = state.num_qubits();
    require_qubit_in_range(n, control, "CZ control");
    require_qubit_in_range(n, target, "CZ target");
    require_distinct_qubits(control, target);
    const int c_mask = qubit_mask(n, control);
    const int t_mask = qubit_mask(n, target);
    auto& amps = state.amplitudes();
    for (std::size_t i = 0; i < amps.size(); ++i) {
        if ((static_cast<int>(i) & c_mask) && (static_cast<int>(i) & t_mask)) {
            amps[i] = -amps[i];
        }
    }
}

void apply_swap(StateVector& state, int qubit_a, int qubit_b) {
    const int n = state.num_qubits();
    require_qubit_in_range(n, qubit_a, "SWAP");
    require_qubit_in_range(n, qubit_b, "SWAP");
    if (qubit_a == qubit_b) return;
    const int mask_a = qubit_mask(n, qubit_a);
    const int mask_b = qubit_mask(n, qubit_b);
    auto& amps = state.amplitudes();
    for (std::size_t i = 0; i < amps.size(); ++i) {
        if (!(static_cast<int>(i) & mask_a) && (static_cast<int>(i) & mask_b)) {
            const std::size_t j = (i & ~static_cast<std::size_t>(mask_b)) | static_cast<std::size_t>(mask_a);
            std::swap(amps[i], amps[j]);
        }
    }
}

void apply_ccx(StateVector& state, int control1, int control2, int target) {
    const int n = state.num_qubits();
    require_qubit_in_range(n, control1, "CCX control1");
    require_qubit_in_range(n, control2, "CCX control2");
    require_qubit_in_range(n, target, "CCX target");
    require_distinct_qubits(control1, control2, target);
    const int c1 = qubit_mask(n, control1);
    const int c2 = qubit_mask(n, control2);
    const int t = qubit_mask(n, target);
    auto& amps = state.amplitudes();
    for (std::size_t i = 0; i < amps.size(); ++i) {
        const int idx = static_cast<int>(i);
        if ((idx & c1) && (idx & c2) && !(idx & t)) {
            const std::size_t j = i | static_cast<std::size_t>(t);
            std::swap(amps[i], amps[j]);
        }
    }
}

void apply_controlled_rotation(StateVector& state, int control, int target, GateKind kind, double angle) {
    const int n = state.num_qubits();
    require_qubit_in_range(n, control, "controlled rotation control");
    require_qubit_in_range(n, target, "controlled rotation target");
    require_distinct_qubits(control, target);
    const int c_mask = qubit_mask(n, control);
    const int t_mask = qubit_mask(n, target);
    const Matrix2 full = gate_matrix(kind, angle);
    // Controlled: apply target subspace only when control=1
    const Matrix2 id = identity();
    auto& amps = state.amplitudes();
    for (std::size_t i = 0; i < amps.size(); ++i) {
        if ((static_cast<int>(i) & c_mask) == 0) continue;
        if ((static_cast<int>(i) & t_mask) != 0) continue;
        const std::size_t j = i | static_cast<std::size_t>(t_mask);
        const Complex a0 = amps[i];
        const Complex a1 = amps[j];
        amps[i] = full[0][0] * a0 + full[0][1] * a1;
        amps[j] = full[1][0] * a0 + full[1][1] * a1;
    }
    (void)id;
    state.renormalize();
}

void apply_gate(StateVector& state, GateKind kind, int qubit,
                int qubit2, int qubit3,
                double param1, double param2, double param3) {
    switch (kind) {
        case GateKind::I:
        case GateKind::H:
        case GateKind::X:
        case GateKind::Y:
        case GateKind::Z:
        case GateKind::S:
        case GateKind::T:
        case GateKind::Sdg:
        case GateKind::Tdg:
        case GateKind::Rx:
        case GateKind::Ry:
        case GateKind::Rz:
        case GateKind::U:
            apply_1q_gate(state, qubit, kind, param1, param2, param3);
            break;
        case GateKind::CNOT:
            apply_cnot(state, qubit, qubit2);
            break;
        case GateKind::CZ:
            apply_cz(state, qubit, qubit2);
            break;
        case GateKind::SWAP:
            apply_swap(state, qubit, qubit2);
            break;
        case GateKind::CCX:
            apply_ccx(state, qubit, qubit2, qubit3);
            break;
        case GateKind::CRx:
            apply_controlled_rotation(state, qubit, qubit2, GateKind::Rx, param1);
            break;
        case GateKind::CRy:
            apply_controlled_rotation(state, qubit, qubit2, GateKind::Ry, param1);
            break;
        case GateKind::CRz:
            apply_controlled_rotation(state, qubit, qubit2, GateKind::Rz, param1);
            break;
        case GateKind::Barrier:
            break;
        case GateKind::Measure:
            break;
    }
}

void apply_measure(StateVector& state, int qubit, bool collapse, std::mt19937& rng) {
    if (!collapse) return;
    require_qubit_in_range(state.num_qubits(), qubit, "measure");
    const double p0 = state.prob_qubit_zero(qubit);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    const int outcome = dist(rng) < p0 ? 0 : 1;
    const int mask = qubit_mask(state.num_qubits(), qubit);
    auto& amps = state.amplitudes();
    for (std::size_t i = 0; i < amps.size(); ++i) {
        const int bit = (static_cast<int>(i) & mask) ? 1 : 0;
        if (bit != outcome) {
            amps[i] = Complex(0.0, 0.0);
        }
    }
    state.renormalize();
}

namespace {

std::string format_complex(Complex c) {
    char buf[48];
    if (std::abs(c.imag()) < 1e-9) {
        std::snprintf(buf, sizeof(buf), "%.3f", c.real());
    } else if (std::abs(c.real()) < 1e-9) {
        std::snprintf(buf, sizeof(buf), "%.3fi", c.imag());
    } else {
        std::snprintf(buf, sizeof(buf), "%.3f%+.3fi", c.real(), c.imag());
    }
    return buf;
}

}  // namespace

std::string gate_matrix_display(GateKind kind, double param1, double param2, double param3) {
    switch (kind) {
        case GateKind::CNOT:
            return "|00>->|00>, |01>->|01>, |10>->|11>, |11>->|10>";
        case GateKind::CZ:
            return "Adds -1 phase to |11>; other basis states unchanged.";
        case GateKind::SWAP:
            return "Swaps amplitudes of the two qubit wires.";
        case GateKind::CCX:
            return "X on target when both controls are |1>.";
        case GateKind::CRx:
        case GateKind::CRy:
        case GateKind::CRz:
            return "Applies rotation on target when control is |1>.";
        case GateKind::Barrier:
            return "Visual only — no unitary (simulation skips).";
        case GateKind::Measure:
            return "Z-basis readout; optional stochastic collapse.";
        default:
            break;
    }
    try {
        const Matrix2 m = gate_matrix(kind, param1, param2, param3);
        std::ostringstream oss;
        oss << "[[" << format_complex(m[0][0]) << ", " << format_complex(m[0][1]) << "], ["
            << format_complex(m[1][0]) << ", " << format_complex(m[1][1]) << "]]";
        return oss.str();
    } catch (const std::exception&) {
        return "(see gate tooltip)";
    }
}

}  // namespace qsim
