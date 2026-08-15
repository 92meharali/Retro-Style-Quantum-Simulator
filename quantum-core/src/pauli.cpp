#include "qsim/pauli.hpp"

#include "qsim/gates.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace qsim {

char pauli_char(PauliOp op) {
    switch (op) {
        case PauliOp::I: return 'I';
        case PauliOp::X: return 'X';
        case PauliOp::Y: return 'Y';
        case PauliOp::Z: return 'Z';
    }
    return 'I';
}

PauliOp char_to_pauli(char c) {
    switch (std::toupper(static_cast<unsigned char>(c))) {
        case 'X': return PauliOp::X;
        case 'Y': return PauliOp::Y;
        case 'Z': return PauliOp::Z;
        default: return PauliOp::I;
    }
}

PauliOp PauliTerm::op_on_qubit(int qubit) const {
    for (const auto& [q, op] : ops) {
        if (q == qubit) return op;
    }
    return PauliOp::I;
}

double PauliTerm::expectation(const StateVector& state) const {
    if (std::abs(coefficient) < 1e-15) return 0.0;
    if (ops.empty()) return coefficient;

    // Check if all operators are Identity
    bool all_identity = true;
    for (const auto& [q, op] : ops) {
        if (op != PauliOp::I) {
            all_identity = false;
            break;
        }
    }
    if (all_identity) return coefficient;

    // Check if all operators are Z (diagonal shortcut for fast execution)
    bool all_z = true;
    for (const auto& [q, op] : ops) {
        if (op != PauliOp::Z && op != PauliOp::I) {
            all_z = false;
            break;
        }
    }

    const int n = state.num_qubits();
    if (all_z) {
        double exp_val = 0.0;
        const auto& amps = state.amplitudes();
        for (std::size_t i = 0; i < amps.size(); ++i) {
            const double prob = std::norm(amps[i]);
            if (prob < 1e-15) continue;
            int parity = 0;
            for (const auto& [q, op] : ops) {
                if (op == PauliOp::Z) {
                    parity ^= qubit_value(static_cast<int>(i), n, q);
                }
            }
            exp_val += (parity ? -prob : prob);
        }
        return exp_val * coefficient;
    }

    // General Pauli string: apply gates to a copy of statevector and compute inner product
    StateVector transformed = state;
    for (const auto& [q, op] : ops) {
        if (q < 0 || q >= n) continue;
        switch (op) {
            case PauliOp::X:
                apply_1q_gate(transformed, q, GateKind::X);
                break;
            case PauliOp::Y:
                apply_1q_gate(transformed, q, GateKind::Y);
                break;
            case PauliOp::Z:
                apply_1q_gate(transformed, q, GateKind::Z);
                break;
            case PauliOp::I:
                break;
        }
    }

    Complex inner_prod = 0.0;
    const auto& orig_amps = state.amplitudes();
    const auto& trans_amps = transformed.amplitudes();
    for (std::size_t i = 0; i < orig_amps.size(); ++i) {
        inner_prod += std::conj(orig_amps[i]) * trans_amps[i];
    }

    return inner_prod.real() * coefficient;
}

double PauliTerm::expectation_shots(const StateVector& state, int shots, std::mt19937& rng) const {
    if (shots <= 0) return expectation(state);
    if (std::abs(coefficient) < 1e-15) return 0.0;
    if (ops.empty()) return coefficient;

    const int n = state.num_qubits();
    StateVector meas_state = state;

    // Rotate into measurement basis
    for (const auto& [q, op] : ops) {
        if (q < 0 || q >= n) continue;
        if (op == PauliOp::X) {
            // X-basis measurement: apply Hadamard
            apply_1q_gate(meas_state, q, GateKind::H);
        } else if (op == PauliOp::Y) {
            // Y-basis measurement: apply Sdg then H
            apply_1q_gate(meas_state, q, GateKind::Sdg);
            apply_1q_gate(meas_state, q, GateKind::H);
        }
    }

    int parity_sum = 0;
    for (int s = 0; s < shots; ++s) {
        const int sample_idx = meas_state.sample(rng);
        int parity = 0;
        for (const auto& [q, op] : ops) {
            if (op != PauliOp::I) {
                parity ^= qubit_value(sample_idx, n, q);
            }
        }
        parity_sum += (parity ? -1 : 1);
    }

    return (static_cast<double>(parity_sum) / shots) * coefficient;
}

std::string PauliTerm::to_string(int num_qubits) const {
    std::ostringstream oss;
    oss << (coefficient >= 0 ? "+" : "") << coefficient << " * ";
    bool first = true;
    for (int q = 0; q < num_qubits; ++q) {
        PauliOp op = op_on_qubit(q);
        if (op != PauliOp::I) {
            if (!first) oss << " ";
            oss << pauli_char(op) << q;
            first = false;
        }
    }
    if (first) oss << "I";
    return oss.str();
}

double Hamiltonian::expectation(const StateVector& state) const {
    double total = 0.0;
    for (const auto& term : terms_) {
        total += term.expectation(state);
    }
    return total;
}

double Hamiltonian::expectation_shots(const StateVector& state, int shots, std::mt19937& rng) const {
    double total = 0.0;
    for (const auto& term : terms_) {
        total += term.expectation_shots(state, shots, rng);
    }
    return total;
}

std::vector<std::vector<Complex>> Hamiltonian::matrix() const {
    const int dim = 1 << num_qubits_;
    std::vector<std::vector<Complex>> mat(dim, std::vector<Complex>(dim, 0.0));

    for (int col = 0; col < dim; ++col) {
        StateVector sv(num_qubits_);
        sv.collapse(col);
        for (const auto& term : terms_) {
            if (std::abs(term.coefficient) < 1e-15) continue;
            StateVector trans = sv;
            for (const auto& [q, op] : term.ops) {
                if (q < 0 || q >= num_qubits_) continue;
                switch (op) {
                    case PauliOp::X: apply_1q_gate(trans, q, GateKind::X); break;
                    case PauliOp::Y: apply_1q_gate(trans, q, GateKind::Y); break;
                    case PauliOp::Z: apply_1q_gate(trans, q, GateKind::Z); break;
                    case PauliOp::I: break;
                }
            }
            for (int row = 0; row < dim; ++row) {
                mat[row][col] += trans.amplitudes()[row] * term.coefficient;
            }
        }
    }
    return mat;
}

double Hamiltonian::ground_state_energy_exact() const {
    const int dim = 1 << num_qubits_;
    if (dim == 0 || terms_.empty()) return 0.0;

    const auto mat = matrix();

    // Power iteration with shift to find smallest eigenvalue
    // Estimate Gershgorin upper bound for shifting
    double max_diag = -1e9;
    double max_row_sum = 0.0;
    for (int i = 0; i < dim; ++i) {
        double row_sum = 0.0;
        for (int j = 0; j < dim; ++j) {
            row_sum += std::abs(mat[i][j]);
        }
        max_row_sum = std::max(max_row_sum, row_sum);
        max_diag = std::max(max_diag, mat[i][i].real());
    }

    const double shift = max_row_sum + std::abs(max_diag) + 10.0;
    // B = shift * I - H has dominant eigenvalue (shift - E_min)
    std::vector<std::vector<Complex>> B = mat;
    for (int i = 0; i < dim; ++i) {
        for (int j = 0; j < dim; ++j) {
            B[i][j] = -B[i][j];
        }
        B[i][i] += shift;
    }

    // Power iteration with multiple initial vectors to prevent symmetry trapping
    double min_energy = 1e9;
    std::vector<std::vector<Complex>> initial_trials;

    // Trial 1: basis states
    for (int b = 0; b < std::min(dim, 8); ++b) {
        std::vector<Complex> vb(dim, 0.0);
        vb[b] = 1.0;
        initial_trials.push_back(vb);
    }
    // Trial 2: equal superposition
    initial_trials.push_back(std::vector<Complex>(dim, 1.0 / std::sqrt(dim)));
    // Trial 3: alternating signs
    std::vector<Complex> alt(dim, 0.0);
    for (int i = 0; i < dim; ++i) {
        alt[i] = (i % 2 == 0) ? (1.0 / std::sqrt(dim)) : (-1.0 / std::sqrt(dim));
    }
    initial_trials.push_back(alt);

    for (const auto& init_v : initial_trials) {
        std::vector<Complex> v = init_v;
        for (int iter = 0; iter < 200; ++iter) {
            std::vector<Complex> next_v(dim, 0.0);
            for (int i = 0; i < dim; ++i) {
                for (int j = 0; j < dim; ++j) {
                    next_v[i] += B[i][j] * v[j];
                }
            }
            double norm = 0.0;
            for (int i = 0; i < dim; ++i) {
                norm += std::norm(next_v[i]);
            }
            norm = std::sqrt(norm);
            if (norm < 1e-12) break;
            for (int i = 0; i < dim; ++i) {
                v[i] = next_v[i] / norm;
            }
        }

        // Rayleigh quotient: v^H H v
        Complex rayleigh = 0.0;
        for (int i = 0; i < dim; ++i) {
            Complex Hv_i = 0.0;
            for (int j = 0; j < dim; ++j) {
                Hv_i += mat[i][j] * v[j];
            }
            rayleigh += std::conj(v[i]) * Hv_i;
        }

        if (rayleigh.real() < min_energy) {
            min_energy = rayleigh.real();
        }
    }

    return min_energy;
}

std::string Hamiltonian::to_string() const {
    if (terms_.empty()) return "0.0";
    std::ostringstream oss;
    for (std::size_t i = 0; i < terms_.size(); ++i) {
        if (i > 0) oss << " ";
        oss << terms_[i].to_string(num_qubits_);
    }
    return oss.str();
}

Hamiltonian Hamiltonian::from_string(const std::string& str, int num_qubits) {
    Hamiltonian h(num_qubits);
    std::string s = str;
    for (char& c : s) {
        if (c == '+' || c == '-') {
            // keep
        } else if (c == '*' || c == ',' || c == ';') {
            c = ' ';
        }
    }

    // Tokenize string like: "+1.0 X0 X1 - 0.5 Z0 Z1 + 0.2 Z0"
    std::istringstream iss(s);
    std::string token;
    double current_sign = 1.0;
    double current_coeff = 1.0;
    bool has_coeff = false;
    std::vector<std::pair<int, PauliOp>> current_ops;

    auto finish_term = [&]() {
        if (has_coeff || !current_ops.empty()) {
            h.add_term(PauliTerm(current_sign * current_coeff, current_ops));
            current_sign = 1.0;
            current_coeff = 1.0;
            has_coeff = false;
            current_ops.clear();
        }
    };

    while (iss >> token) {
        if (token == "+") {
            finish_term();
            current_sign = 1.0;
        } else if (token == "-") {
            finish_term();
            current_sign = -1.0;
        } else {
            // Check if token is a number
            char* end_ptr = nullptr;
            double val = std::strtod(token.c_str(), &end_ptr);
            if (end_ptr != token.c_str() && *end_ptr == '\0') {
                if (has_coeff) {
                    finish_term();
                }
                current_coeff = val;
                has_coeff = true;
            } else {
                // Parse Pauli token, e.g. "X0", "Z1", "Y0", "I"
                char p_char = token[0];
                PauliOp op = char_to_pauli(p_char);
                if (op != PauliOp::I && token.size() > 1) {
                    int q = std::atoi(token.c_str() + 1);
                    current_ops.push_back({q, op});
                }
            }
        }
    }
    finish_term();
    return h;
}

Hamiltonian Hamiltonian::two_qubit_xx_zz() {
    Hamiltonian h(2);
    // H = 1.0 * X0 * X1 + 1.0 * Z0 * Z1 (Ground state energy = -2.0)
    h.add_term(PauliTerm(1.0, {{0, PauliOp::X}, {1, PauliOp::X}}));
    h.add_term(PauliTerm(1.0, {{0, PauliOp::Z}, {1, PauliOp::Z}}));
    return h;
}

Hamiltonian Hamiltonian::h2_molecule(double bond_distance) {
    Hamiltonian h(2);
    // STO-3G minimal basis Hydrogen molecule at R=0.7414 Å (equilibrium)
    // Parity / Jordan-Wigner 2-qubit effective Hamiltonian
    // Coefficients derived from standard quantum chemistry integrals
    const double scale = (bond_distance > 0.1) ? (0.7414 / bond_distance) : 1.0;
    const double g0 = -1.0523732 * scale;
    const double g1 = 0.3979374 * scale;
    const double g2 = -0.3979374 * scale;
    const double g3 = -0.0112801;
    const double g4 = 0.1809312;
    const double g5 = 0.1809312;

    h.add_term(PauliTerm(g0, {}));                                   // g0 * I
    h.add_term(PauliTerm(g1, {{0, PauliOp::Z}}));                   // g1 * Z0
    h.add_term(PauliTerm(g2, {{1, PauliOp::Z}}));                   // g2 * Z1
    h.add_term(PauliTerm(g3, {{0, PauliOp::Z}, {1, PauliOp::Z}})); // g3 * Z0*Z1
    h.add_term(PauliTerm(g4, {{0, PauliOp::X}, {1, PauliOp::X}})); // g4 * X0*X1
    h.add_term(PauliTerm(g5, {{0, PauliOp::Y}, {1, PauliOp::Y}})); // g5 * Y0*Y1

    return h;
}

Hamiltonian Hamiltonian::transverse_ising(int num_qubits, double J, double g, bool periodic) {
    Hamiltonian h(num_qubits);
    // H = -J sum Z_i Z_{i+1} - g sum X_i
    const int pairs = periodic ? num_qubits : (num_qubits - 1);
    for (int i = 0; i < pairs; ++i) {
        int next_q = (i + 1) % num_qubits;
        h.add_term(PauliTerm(-J, {{i, PauliOp::Z}, {next_q, PauliOp::Z}}));
    }
    for (int i = 0; i < num_qubits; ++i) {
        h.add_term(PauliTerm(-g, {{i, PauliOp::X}}));
    }
    return h;
}

Hamiltonian Hamiltonian::heisenberg_xxz(int num_qubits, double J, double delta, bool periodic) {
    Hamiltonian h(num_qubits);
    // H = J sum (X_i X_{i+1} + Y_i Y_{i+1} + delta * Z_i Z_{i+1})
    const int pairs = periodic ? num_qubits : (num_qubits - 1);
    for (int i = 0; i < pairs; ++i) {
        int next_q = (i + 1) % num_qubits;
        h.add_term(PauliTerm(J, {{i, PauliOp::X}, {next_q, PauliOp::X}}));
        h.add_term(PauliTerm(J, {{i, PauliOp::Y}, {next_q, PauliOp::Y}}));
        h.add_term(PauliTerm(J * delta, {{i, PauliOp::Z}, {next_q, PauliOp::Z}}));
    }
    return h;
}

}  // namespace qsim
