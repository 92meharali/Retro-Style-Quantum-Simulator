#pragma once

#include "qsim/state_vector.hpp"
#include "qsim/types.hpp"

#include <array>
#include <random>

namespace qsim {

using Matrix2 = std::array<std::array<Complex, 2>, 2>;

Matrix2 gate_matrix(GateKind kind, double param1 = 0.0, double param2 = 0.0, double param3 = 0.0);

void apply_1q_gate(StateVector& state, int qubit, const Matrix2& matrix);
void apply_1q_gate(StateVector& state, int qubit, GateKind kind,
                   double param1 = 0.0, double param2 = 0.0, double param3 = 0.0);

void apply_cnot(StateVector& state, int control, int target);
void apply_cz(StateVector& state, int control, int target);
void apply_swap(StateVector& state, int qubit_a, int qubit_b);
void apply_ccx(StateVector& state, int control1, int control2, int target);
void apply_controlled_rotation(StateVector& state, int control, int target, GateKind kind, double angle);

/// Z-basis measurement on one qubit; collapses when collapse=true.
void apply_measure(StateVector& state, int qubit, bool collapse, std::mt19937& rng);

/// Apply gate and renormalize. Barriers are no-ops.
void apply_gate(StateVector& state, GateKind kind, int qubit,
                int qubit2 = -1, int qubit3 = -1,
                double param1 = 0.0, double param2 = 0.0, double param3 = 0.0);

const GateInfo& gate_info(GateKind kind);

/// Human-readable matrix (1-qubit) or action summary (multi-qubit).
std::string gate_matrix_display(GateKind kind, double param1 = 0.0, double param2 = 0.0,
                                double param3 = 0.0);

}  // namespace qsim
