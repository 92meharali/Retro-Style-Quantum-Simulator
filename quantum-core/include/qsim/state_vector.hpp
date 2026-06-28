#pragma once

#include "qsim/types.hpp"

#include <random>
#include <vector>

namespace qsim {

class StateVector {
public:
    explicit StateVector(int num_qubits);

    int num_qubits() const { return num_qubits_; }
    std::size_t size() const { return amplitudes_.size(); }
    const std::vector<Amplitude>& amplitudes() const { return amplitudes_; }
    std::vector<Amplitude>& amplitudes() { return amplitudes_; }

    void reset();
    void renormalize();
    double norm() const;

    /// P(return index i) = |α_i|²
    double probability(int index) const;
    std::vector<double> probabilities() const;

    /// Marginal P(qubit q = 0)
    double prob_qubit_zero(int qubit) const;

    /// Sample measurement outcome (basis index)
    int sample(std::mt19937& rng) const;

    /// Collapse to |index⟩
    void collapse(int index);

private:
    int num_qubits_;
    std::vector<Amplitude> amplitudes_;
};

}  // namespace qsim
