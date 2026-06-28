#pragma once

#include <complex>
#include <cstdint>
#include <string>

namespace qsim {

using Complex = std::complex<double>;
using Amplitude = Complex;

constexpr double kEpsilon = 1e-12;

enum class InitialStatePreset {
    AllZero,
    AllOne,
    Q0Plus,
};

enum class GateKind {
    I,
    H,
    X,
    Y,
    Z,
    S,
    T,
    Sdg,
    Tdg,
    Rx,
    Ry,
    Rz,
    U,       // arbitrary U(θ, φ, λ) — params in operation
    CNOT,
    CZ,
    SWAP,
    CCX,     // Toffoli
    CRx,
    CRy,
    CRz,
    Barrier,
    Measure,
};

struct GateInfo {
    GateKind kind;
    const char* symbol;
    const char* name;
    const char* tooltip;
    int num_qubits;
    bool is_parametric;
};

inline int qubit_mask(int num_qubits, int qubit) {
    return 1 << (num_qubits - 1 - qubit);
}

inline int qubit_value(int index, int num_qubits, int qubit) {
    return (index & qubit_mask(num_qubits, qubit)) ? 1 : 0;
}

inline std::string basis_label(int index, int num_qubits) {
    std::string label;
    label.reserve(static_cast<size_t>(num_qubits) + 2);
    label += '|';
    for (int q = 0; q < num_qubits; ++q) {
        label += qubit_value(index, num_qubits, q) ? '1' : '0';
    }
    label += '>';
    return label;
}

}  // namespace qsim
