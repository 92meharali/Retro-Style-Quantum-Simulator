#include "qsim/gates.hpp"
#include "qsim/circuit.hpp"

#include <algorithm>

namespace qsim {

int Circuit::num_columns() const {
    if (ops.empty()) return 0;
    int max_col = 0;
    for (const auto& op : ops) {
        max_col = std::max(max_col, op.column);
    }
    return max_col + 1;
}

int Circuit::gate_count() const {
    int count = 0;
    for (const auto& op : ops) {
        if (op.kind != GateKind::Barrier) ++count;
    }
    return count;
}

int Circuit::depth() const {
    int max_col = -1;
    for (const auto& op : ops) {
        if (op.kind != GateKind::Barrier) {
            max_col = std::max(max_col, op.column);
        }
    }
    return max_col + 1;
}

void Circuit::add_op(CircuitOp op) {
    ops.push_back(std::move(op));
}

bool Circuit::remove_op(int id) {
    const auto it = std::remove_if(ops.begin(), ops.end(),
                                   [id](const CircuitOp& op) { return op.id == id; });
    if (it == ops.end()) return false;
    ops.erase(it, ops.end());
    return true;
}

std::optional<CircuitOp> Circuit::op_at(int qubit, int column) const {
    for (const auto& op : ops) {
        if (op.column != column) continue;
        const auto& info = gate_info(op.kind);
        if (info.num_qubits == 1 && op.qubit == qubit) {
            return op;
        }
        if (info.num_qubits == 2 &&
            (op.qubit == qubit || op.qubit2 == qubit)) {
            return op;
        }
        if (info.num_qubits == 3 &&
            (op.qubit == qubit || op.qubit2 == qubit || op.qubit3 == qubit)) {
            return op;
        }
    }
    return std::nullopt;
}

void Circuit::clear() {
    ops.clear();
}

std::vector<CircuitOp> Circuit::ordered_gates() const {
    std::vector<CircuitOp> gates;
    gates.reserve(ops.size());
    for (const auto& op : ops) {
        if (op.kind != GateKind::Barrier) {
            gates.push_back(op);
        }
    }
    std::sort(gates.begin(), gates.end(), [](const CircuitOp& a, const CircuitOp& b) {
        if (a.column != b.column) return a.column < b.column;
        return a.id < b.id;
    });
    return gates;
}

}  // namespace qsim
