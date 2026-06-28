#pragma once

#include "qsim/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace qsim {

struct CircuitOp {
    GateKind kind = GateKind::I;
    int column = 0;
    int qubit = 0;       // primary / control1
    int qubit2 = -1;     // target / control2
    int qubit3 = -1;     // toffoli target
    double param1 = 0.0;
    double param2 = 0.0;
    double param3 = 0.0;
    int id = 0;          // unique id for UI selection
};

struct Circuit {
    int num_qubits = 2;
    std::vector<CircuitOp> ops;

    int num_columns() const;
    /// Non-barrier operations placed on the circuit
    int gate_count() const;
    /// Deepest time column index + 1 (non-barrier gates only)
    int depth() const;
    void add_op(CircuitOp op);
    bool remove_op(int id);
    std::optional<CircuitOp> op_at(int qubit, int column) const;
    void clear();

    /// Chronological gate list (barriers excluded), sorted by column then qubit
    std::vector<CircuitOp> ordered_gates() const;
};

}  // namespace qsim
