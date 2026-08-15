#pragma once

#include "qsim/gates.hpp"
#include "qsim/state_vector.hpp"
#include "qsim/types.hpp"

#include <random>
#include <string>
#include <utility>
#include <vector>

namespace qsim {

enum class PauliOp {
    I,
    X,
    Y,
    Z,
};

char pauli_char(PauliOp op);
PauliOp char_to_pauli(char c);

struct PauliTerm {
    double coefficient = 1.0;
    /// List of (qubit_index, pauli_op). Qubits not present default to Identity.
    std::vector<std::pair<int, PauliOp>> ops;

    PauliTerm() = default;
    PauliTerm(double coeff, std::vector<std::pair<int, PauliOp>> operations)
        : coefficient(coeff), ops(std::move(operations)) {}

    /// Returns the Pauli operator acting on the specified qubit (I if identity).
    PauliOp op_on_qubit(int qubit) const;

    /// Evaluates exact expectation value <psi| P |psi> * coefficient.
    double expectation(const StateVector& state) const;

    /// Evaluates expectation value using shot sampling with Pauli basis measurement rotations.
    double expectation_shots(const StateVector& state, int shots, std::mt19937& rng) const;

    std::string to_string(int num_qubits) const;
};

class Hamiltonian {
public:
    Hamiltonian() = default;
    explicit Hamiltonian(int num_qubits) : num_qubits_(num_qubits) {}
    Hamiltonian(int num_qubits, std::vector<PauliTerm> terms)
        : num_qubits_(num_qubits), terms_(std::move(terms)) {}

    int num_qubits() const { return num_qubits_; }
    void set_num_qubits(int n) { num_qubits_ = n; }

    const std::vector<PauliTerm>& terms() const { return terms_; }
    std::vector<PauliTerm>& terms() { return terms_; }

    void add_term(PauliTerm term) { terms_.push_back(std::move(term)); }
    void clear() { terms_.clear(); }
    bool empty() const { return terms_.empty(); }
    std::size_t size() const { return terms_.size(); }

    /// Exact expectation value <psi| H |psi>
    double expectation(const StateVector& state) const;

    /// Shot-sampled expectation value with finite shot noise in Pauli measurement bases
    double expectation_shots(const StateVector& state, int shots, std::mt19937& rng) const;

    /// Exact ground state eigenvalue E_0 computed via exact matrix diagonalization / power iteration.
    double ground_state_energy_exact() const;

    /// Construct 2^n x 2^n explicit matrix representation (for small n <= 10)
    std::vector<std::vector<Complex>> matrix() const;

    std::string to_string() const;
    static Hamiltonian from_string(const std::string& str, int num_qubits);

    // Standard Built-in Hamiltonians
    /// H = 1.0 * X0*X1 + 1.0 * Z0*Z1  (Ground state energy = -2.0)
    static Hamiltonian two_qubit_xx_zz();

    /// Minimal basis STO-3G H2 molecule with parity / Jordan-Wigner transformation
    static Hamiltonian h2_molecule(double bond_distance = 0.7414);

    /// 1D Transverse-Field Ising Model: H = -J sum Z_i Z_{i+1} - g sum X_i
    static Hamiltonian transverse_ising(int num_qubits, double J = 1.0, double g = 0.5, bool periodic = false);

    /// 1D Heisenberg XXZ Model: H = J sum (X_i X_{i+1} + Y_i Y_{i+1} + delta * Z_i Z_{i+1})
    static Hamiltonian heisenberg_xxz(int num_qubits, double J = 1.0, double delta = 1.0, bool periodic = false);

private:
    int num_qubits_ = 2;
    std::vector<PauliTerm> terms_;
};

}  // namespace qsim
