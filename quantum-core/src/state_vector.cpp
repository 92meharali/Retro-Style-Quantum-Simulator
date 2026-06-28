#include "qsim/state_vector.hpp"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace qsim {

StateVector::StateVector(int num_qubits)
    : num_qubits_(num_qubits),
      amplitudes_(static_cast<std::size_t>(1) << num_qubits, Complex(0.0, 0.0)) {
    if (num_qubits < 1 || num_qubits > 20) {
        throw std::invalid_argument("num_qubits must be between 1 and 20");
    }
    reset();
}

void StateVector::reset() {
    std::fill(amplitudes_.begin(), amplitudes_.end(), Complex(0.0, 0.0));
    amplitudes_[0] = Complex(1.0, 0.0);
}

void StateVector::renormalize() {
    const double n = norm();
    if (n < kEpsilon) {
        reset();
        return;
    }
    const double inv = 1.0 / n;
    for (auto& amp : amplitudes_) {
        amp *= inv;
    }
}

double StateVector::norm() const {
    double sum = 0.0;
    for (const auto& amp : amplitudes_) {
        sum += std::norm(amp);
    }
    return std::sqrt(sum);
}

double StateVector::probability(int index) const {
    return std::norm(amplitudes_[static_cast<std::size_t>(index)]);
}

std::vector<double> StateVector::probabilities() const {
    std::vector<double> probs(amplitudes_.size());
    for (std::size_t i = 0; i < amplitudes_.size(); ++i) {
        probs[i] = std::norm(amplitudes_[i]);
    }
    return probs;
}

double StateVector::prob_qubit_zero(int qubit) const {
    const int mask = qubit_mask(num_qubits_, qubit);
    double sum = 0.0;
    for (std::size_t i = 0; i < amplitudes_.size(); ++i) {
        if ((static_cast<int>(i) & mask) == 0) {
            sum += std::norm(amplitudes_[i]);
        }
    }
    return sum;
}

int StateVector::sample(std::mt19937& rng) const {
    std::vector<double> weights(amplitudes_.size());
    double total = 0.0;
    for (std::size_t i = 0; i < amplitudes_.size(); ++i) {
        weights[i] = std::norm(amplitudes_[i]);
        total += weights[i];
    }
    if (total < kEpsilon) {
        throw std::runtime_error("cannot sample from zero-norm state");
    }
    std::discrete_distribution<int> weighted(weights.begin(), weights.end());
    return weighted(rng);
}

void StateVector::collapse(int index) {
    if (index < 0 || index >= static_cast<int>(amplitudes_.size())) {
        throw std::out_of_range("collapse index out of range");
    }
    for (std::size_t i = 0; i < amplitudes_.size(); ++i) {
        amplitudes_[i] = (static_cast<int>(i) == index) ? Complex(1.0, 0.0) : Complex(0.0, 0.0);
    }
}

}  // namespace qsim
