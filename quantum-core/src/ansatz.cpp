#include "qsim/ansatz.hpp"

#include "qsim/gates.hpp"

#include <cmath>
#include <sstream>

namespace qsim {

TwoQubitRyCnotAnsatz::TwoQubitRyCnotAnsatz(bool four_params) : four_params_(four_params) {}

std::vector<std::string> TwoQubitRyCnotAnsatz::param_names() const {
    if (four_params_) {
        return {"theta1 (q0)", "theta2 (q1)", "theta3 (q0)", "theta4 (q1)"};
    }
    return {"theta1 (q0)", "theta2 (q1)"};
}

std::vector<double> TwoQubitRyCnotAnsatz::default_initial_params() const {
    if (four_params_) {
        return {0.1, 0.2, 0.1, 0.2};
    }
    return {0.1, 0.2};
}

Circuit TwoQubitRyCnotAnsatz::build_circuit(const std::vector<double>& params) const {
    Circuit c;
    c.num_qubits = 2;
    int op_id = 1;

    double t0 = params.size() > 0 ? params[0] : 0.0;
    double t1 = params.size() > 1 ? params[1] : 0.0;

    // Layer 1: Ry rotations
    c.add_op({GateKind::Ry, 0, 0, -1, -1, t0, 0.0, 0.0, op_id++});
    c.add_op({GateKind::Ry, 0, 1, -1, -1, t1, 0.0, 0.0, op_id++});

    // Entangling CNOT
    c.add_op({GateKind::CNOT, 1, 0, 1, -1, 0.0, 0.0, 0.0, op_id++});

    // Layer 2: Optional Ry rotations
    if (four_params_) {
        double t2 = params.size() > 2 ? params[2] : 0.0;
        double t3 = params.size() > 3 ? params[3] : 0.0;
        c.add_op({GateKind::Ry, 2, 0, -1, -1, t2, 0.0, 0.0, op_id++});
        c.add_op({GateKind::Ry, 2, 1, -1, -1, t3, 0.0, 0.0, op_id++});
    }

    return c;
}

HardwareEfficientAnsatz::HardwareEfficientAnsatz(int num_qubits, int layers)
    : num_qubits_(std::max(1, num_qubits)), layers_(std::max(1, layers)) {}

std::vector<std::string> HardwareEfficientAnsatz::param_names() const {
    std::vector<std::string> names;
    names.reserve(num_params());
    for (int l = 0; l <= layers_; ++l) {
        for (int q = 0; q < num_qubits_; ++q) {
            std::ostringstream oss;
            oss << "theta_L" << l << "_q" << q;
            names.push_back(oss.str());
        }
    }
    return names;
}

std::vector<double> HardwareEfficientAnsatz::default_initial_params() const {
    std::vector<double> init(num_params(), 0.1);
    for (std::size_t i = 0; i < init.size(); ++i) {
        init[i] = 0.1 * (static_cast<double>(i) + 1.0);
    }
    return init;
}

Circuit HardwareEfficientAnsatz::build_circuit(const std::vector<double>& params) const {
    Circuit c;
    c.num_qubits = num_qubits_;
    int op_id = 1;
    int col = 0;
    std::size_t param_idx = 0;

    for (int l = 0; l < layers_; ++l) {
        // Rotation layer
        for (int q = 0; q < num_qubits_; ++q) {
            double angle = (param_idx < params.size()) ? params[param_idx++] : 0.0;
            c.add_op({GateKind::Ry, col, q, -1, -1, angle, 0.0, 0.0, op_id++});
        }
        col++;

        // Entangling ladder
        if (num_qubits_ > 1) {
            for (int q = 0; q < num_qubits_ - 1; ++q) {
                c.add_op({GateKind::CNOT, col, q, q + 1, -1, 0.0, 0.0, 0.0, op_id++});
                col++;
            }
        }
    }

    // Final rotation layer
    for (int q = 0; q < num_qubits_; ++q) {
        double angle = (param_idx < params.size()) ? params[param_idx++] : 0.0;
        c.add_op({GateKind::Ry, col, q, -1, -1, angle, 0.0, 0.0, op_id++});
    }

    return c;
}

CustomCircuitAnsatz::CustomCircuitAnsatz(const Circuit& base_circuit)
    : num_qubits_(base_circuit.num_qubits), template_circuit_(base_circuit) {
    const auto ordered = base_circuit.ordered_gates();
    for (const auto& op : ordered) {
        if (gate_info(op.kind).is_parametric) {
            parametric_op_ids_.push_back(op.id);
            initial_values_.push_back(op.param1);
        }
    }
}

std::vector<std::string> CustomCircuitAnsatz::param_names() const {
    std::vector<std::string> names;
    names.reserve(parametric_op_ids_.size());
    for (std::size_t i = 0; i < parametric_op_ids_.size(); ++i) {
        std::ostringstream oss;
        oss << "theta" << (i + 1);
        names.push_back(oss.str());
    }
    return names;
}

std::vector<double> CustomCircuitAnsatz::default_initial_params() const {
    return initial_values_;
}

Circuit CustomCircuitAnsatz::build_circuit(const std::vector<double>& params) const {
    Circuit c = template_circuit_;
    for (std::size_t i = 0; i < parametric_op_ids_.size() && i < params.size(); ++i) {
        int id = parametric_op_ids_[i];
        for (auto& op : c.ops) {
            if (op.id == id) {
                op.param1 = params[i];
                break;
            }
        }
    }
    return c;
}

}  // namespace qsim
