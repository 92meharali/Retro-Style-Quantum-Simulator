#pragma once

#include "qsim/circuit.hpp"

#include <memory>
#include <string>
#include <vector>

namespace qsim {

enum class AnsatzKind {
    TwoQubitRyCnot,      // Minimal Ry(theta1) Ry(theta2) -> CNOT -> Ry(theta3) Ry(theta4)
    HardwareEfficient,   // N-qubit Ry rotation layers + CNOT ladder
    QAOAMaxCut,          // QAOA p-layer MaxCut ansatz (gamma, beta)
    CustomCircuit,       // Extracts parametric gates from any user circuit
};

class Ansatz {
public:
    virtual ~Ansatz() = default;

    virtual AnsatzKind kind() const = 0;
    virtual int num_qubits() const = 0;
    virtual int num_params() const = 0;
    virtual std::vector<std::string> param_names() const = 0;
    virtual std::vector<double> default_initial_params() const = 0;

    /// Build a concrete executable Circuit from parameter vector
    virtual Circuit build_circuit(const std::vector<double>& params) const = 0;
};

/// 2-qubit minimal VQE ansatz: Ry(t0) x Ry(t1) -> CNOT(0,1) -> Ry(t2) x Ry(t3)
class TwoQubitRyCnotAnsatz : public Ansatz {
public:
    explicit TwoQubitRyCnotAnsatz(bool four_params = true);

    AnsatzKind kind() const override { return AnsatzKind::TwoQubitRyCnot; }
    int num_qubits() const override { return 2; }
    int num_params() const override { return four_params_ ? 4 : 2; }
    std::vector<std::string> param_names() const override;
    std::vector<double> default_initial_params() const override;

    Circuit build_circuit(const std::vector<double>& params) const override;

private:
    bool four_params_ = true;
};

/// Hardware-Efficient Ansatz (HEA): N-qubit, L-layer Ry + CNOT ladder
class HardwareEfficientAnsatz : public Ansatz {
public:
    HardwareEfficientAnsatz(int num_qubits, int layers = 1);

    AnsatzKind kind() const override { return AnsatzKind::HardwareEfficient; }
    int num_qubits() const override { return num_qubits_; }
    int layers() const { return layers_; }
    int num_params() const override { return num_qubits_ * (layers_ + 1); }
    std::vector<std::string> param_names() const override;
    std::vector<double> default_initial_params() const override;

    Circuit build_circuit(const std::vector<double>& params) const override;

private:
    int num_qubits_ = 2;
    int layers_ = 1;
};

/// Custom Circuit Ansatz: Maps parameter vector to parametric gates inside a user-provided Circuit
class CustomCircuitAnsatz : public Ansatz {
public:
    explicit CustomCircuitAnsatz(const Circuit& base_circuit);

    AnsatzKind kind() const override { return AnsatzKind::CustomCircuit; }
    int num_qubits() const override { return num_qubits_; }
    int num_params() const override { return static_cast<int>(parametric_op_ids_.size()); }
    std::vector<std::string> param_names() const override;
    std::vector<double> default_initial_params() const override;

    Circuit build_circuit(const std::vector<double>& params) const override;

private:
    int num_qubits_ = 2;
    Circuit template_circuit_;
    std::vector<int> parametric_op_ids_;
    std::vector<double> initial_values_;
};

}  // namespace qsim
