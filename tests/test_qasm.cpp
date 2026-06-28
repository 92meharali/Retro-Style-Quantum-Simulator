#include <catch2/catch_test_macros.hpp>

#include "qsim/circuit.hpp"
#include "qsim/gates.hpp"
#include "qsim/qasm_io.hpp"

#include <cmath>
#include <fstream>

using namespace qsim;

namespace {

constexpr double kTol = 1e-9;

bool is_qasm_gate(GateKind kind) {
    switch (kind) {
        case GateKind::H:
        case GateKind::X:
        case GateKind::Y:
        case GateKind::Z:
        case GateKind::CNOT:
        case GateKind::CZ:
        case GateKind::SWAP:
        case GateKind::Rx:
        case GateKind::Ry:
        case GateKind::Rz:
            return true;
        default:
            return false;
    }
}

std::vector<CircuitOp> qasm_ops(const Circuit& circuit) {
    std::vector<CircuitOp> ops;
    for (const auto& op : circuit.ordered_gates()) {
        if (is_qasm_gate(op.kind)) ops.push_back(op);
    }
    return ops;
}

bool same_qasm_ops(const Circuit& a, const Circuit& b) {
    const auto oa = qasm_ops(a);
    const auto ob = qasm_ops(b);
    if (oa.size() != ob.size()) return false;
    for (std::size_t i = 0; i < oa.size(); ++i) {
        const auto& x = oa[i];
        const auto& y = ob[i];
        if (x.kind != y.kind || x.column != y.column || x.qubit != y.qubit ||
            x.qubit2 != y.qubit2) {
            return false;
        }
        if (std::abs(x.param1 - y.param1) > kTol) return false;
    }
    return true;
}

Circuit sample_circuit() {
    Circuit circuit;
    circuit.num_qubits = 3;
    circuit.add_op({GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1});
    circuit.add_op({GateKind::Rx, 1, 1, -1, -1, 1.5707963267948966, 0, 0, 2});
    circuit.add_op({GateKind::CNOT, 2, 0, 2, -1, 0, 0, 0, 3});
    circuit.add_op({GateKind::CZ, 3, 1, 2, -1, 0, 0, 0, 4});
    circuit.add_op({GateKind::SWAP, 4, 0, 1, -1, 0, 0, 0, 5});
    circuit.add_op({GateKind::Rz, 5, 2, -1, -1, 0.7853981633974483, 0, 0, 6});
    circuit.add_op({GateKind::Y, 6, 0, -1, -1, 0, 0, 0, 7});
    circuit.add_op({GateKind::Z, 7, 1, -1, -1, 0, 0, 0, 8});
    circuit.add_op({GateKind::Ry, 8, 2, -1, -1, 0.3, 0, 0, 9});
    return circuit;
}

}  // namespace

TEST_CASE("OpenQASM export contains supported gates", "[qasm]") {
    const auto circuit = sample_circuit();
    const auto qasm = circuit_to_qasm(circuit);
    CHECK(qasm.find("OPENQASM 2.0") != std::string::npos);
    CHECK(qasm.find("qreg q[3]") != std::string::npos);
    CHECK(qasm.find("h q[0]") != std::string::npos);
    CHECK(qasm.find("cx q[0],q[2]") != std::string::npos);
    CHECK(qasm.find("cz q[1],q[2]") != std::string::npos);
    CHECK(qasm.find("swap q[0],q[1]") != std::string::npos);
    CHECK(qasm.find("rx(") != std::string::npos);
    CHECK(qasm.find("rz(") != std::string::npos);
}

TEST_CASE("OpenQASM round-trip preserves circuit", "[qasm]") {
    const auto original = sample_circuit();
    const auto roundtrip = circuit_from_qasm(circuit_to_qasm(original));
    CHECK(roundtrip.num_qubits == original.num_qubits);
    CHECK(same_qasm_ops(original, roundtrip));
}

TEST_CASE("OpenQASM import parses pi angles", "[qasm]") {
    const char* qasm = R"(
OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
rx(pi/2) q[0];
ry(pi) q[1];
)";
    const auto circuit = circuit_from_qasm(qasm);
    CHECK(circuit.num_qubits == 2);
    const auto ops = qasm_ops(circuit);
    REQUIRE(ops.size() == 2);
    CHECK(ops[0].kind == GateKind::Rx);
    CHECK(std::abs(ops[0].param1 - 1.5707963267948966) < kTol);
    CHECK(ops[1].kind == GateKind::Ry);
    CHECK(std::abs(ops[1].param1 - 3.14159265358979323846) < kTol);
}

TEST_CASE("OpenQASM file round-trip", "[qasm]") {
    const auto original = sample_circuit();
    const std::string path = "test_roundtrip.qasm";
    save_qasm_file(path, original);
    const auto loaded = load_qasm_file(path);
    CHECK(same_qasm_ops(original, loaded));
    std::remove(path.c_str());
}

TEST_CASE("OpenQASM ignores unsupported lines in import", "[qasm]") {
    const char* qasm = R"(
OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg c[2];
h q[0];
barrier q[0], q[1];
x q[1];
measure q[0] -> c[0];
)";
    const auto circuit = circuit_from_qasm(qasm);
    const auto ops = qasm_ops(circuit);
    REQUIRE(ops.size() == 2);
    CHECK(ops[0].kind == GateKind::H);
    CHECK(ops[1].kind == GateKind::X);
}

TEST_CASE("OpenQASM export reports unsupported gates", "[qasm]") {
    Circuit circuit;
    circuit.num_qubits = 2;
    circuit.add_op({GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1});
    circuit.add_op({GateKind::CCX, 1, 0, 1, 0, 0, 0, 0, 2});
    CHECK(count_unsupported_qasm_gates(circuit) == 1);
    const auto qasm = circuit_to_qasm(circuit);
    CHECK(qasm.find("h q[0]") != std::string::npos);
    CHECK(qasm.find("ccx") == std::string::npos);
}
