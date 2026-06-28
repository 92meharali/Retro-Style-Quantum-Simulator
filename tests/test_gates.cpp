#include <catch2/catch_test_macros.hpp>

#include "qsim/simulator.hpp"
#include "qsim/gates.hpp"
#include "qsim/json_io.hpp"
#include "qsim/simulator.hpp"

#include <cmath>
#include <numeric>

using namespace qsim;

static constexpr double kTol = 1e-9;

TEST_CASE("H on |0> gives equal superposition", "[gates]") {
    StateVector sv(1);
    apply_1q_gate(sv, 0, GateKind::H);
    CHECK(std::abs(sv.probability(0) - 0.5) < kTol);
    CHECK(std::abs(sv.probability(1) - 0.5) < kTol);
}

TEST_CASE("Bell circuit: H + CNOT -> 50% |00>, 50% |11>", "[circuit]") {
    StateVector sv(2);
    apply_1q_gate(sv, 0, GateKind::H);
    apply_cnot(sv, 0, 1);
    CHECK(std::abs(sv.probability(0) - 0.5) < kTol);
    CHECK(std::abs(sv.probability(3) - 0.5) < kTol);
    CHECK(sv.probability(1) < kTol);
    CHECK(sv.probability(2) < kTol);
}

TEST_CASE("SWAP moves excitation", "[gates]") {
    StateVector sv(2);
    apply_1q_gate(sv, 0, GateKind::X);
    apply_swap(sv, 0, 1);
    CHECK(sv.probability(0) < kTol);
    CHECK(std::abs(sv.probability(1) - 1.0) < kTol);
}

TEST_CASE("CNOT direction: control q0, target q1", "[gates]") {
    StateVector sv(2);
    apply_1q_gate(sv, 0, GateKind::X);  // |10>
    apply_cnot(sv, 0, 1);               // |11>
    CHECK(std::abs(sv.probability(3) - 1.0) < kTol);
}

TEST_CASE("CNOT does not flip when control is 0", "[gates]") {
    StateVector sv(2);
    apply_1q_gate(sv, 1, GateKind::X);  // |01>
    apply_cnot(sv, 0, 1);
    CHECK(std::abs(sv.probability(1) - 1.0) < kTol);
}

TEST_CASE("Y gate preserves equal superposition probabilities", "[gates]") {
    StateVector sv(1);
    apply_1q_gate(sv, 0, GateKind::H);
    apply_1q_gate(sv, 0, GateKind::Y);
    CHECK(std::abs(sv.probability(0) - 0.5) < kTol);
    CHECK(std::abs(sv.probability(1) - 0.5) < kTol);
    CHECK(std::abs(sv.amplitudes()[0].imag() + sv.amplitudes()[1].imag()) < kTol);
}

TEST_CASE("CZ applies pi phase on |11>", "[gates]") {
    StateVector sv(2);
    apply_1q_gate(sv, 0, GateKind::X);
    apply_1q_gate(sv, 1, GateKind::X);
    apply_cz(sv, 0, 1);
    CHECK(std::abs(sv.amplitudes()[3].real() + 1.0) < kTol);
    CHECK(std::abs(sv.norm() - 1.0) < kTol);
}

TEST_CASE("Normalization preserved after long circuit", "[stability]") {
    StateVector sv(3);
    for (int i = 0; i < 100; ++i) {
        apply_1q_gate(sv, i % 3, GateKind::H);
        apply_1q_gate(sv, (i + 1) % 3, GateKind::T);
        apply_cnot(sv, 0, 1);
        apply_cz(sv, 1, 2);
    }
    CHECK(std::abs(sv.norm() - 1.0) < 1e-8);
}

TEST_CASE("Simulator step debugger", "[simulator]") {
    Circuit circuit;
    circuit.num_qubits = 2;
    CircuitOp h{GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1};
    CircuitOp cx{GateKind::CNOT, 1, 0, 1, -1, 0, 0, 0, 2};
    circuit.add_op(h);
    circuit.add_op(cx);

    Simulator sim(2);
    sim.load_circuit(circuit);
    CHECK(sim.current_step() == 0);
    sim.step_forward();
    CHECK(std::abs(sim.state().probability(0) - 0.5) < kTol);
    CHECK(std::abs(sim.state().probability(2) - 0.5) < kTol);
    sim.step_forward();
    CHECK(std::abs(sim.state().probability(0) - 0.5) < kTol);
    CHECK(std::abs(sim.state().probability(3) - 0.5) < kTol);
    sim.step_back();
    CHECK(std::abs(sim.state().probability(0) - 0.5) < kTol);
}

TEST_CASE("JSON round-trip", "[json]") {
    Circuit circuit;
    circuit.num_qubits = 2;
    circuit.add_op({GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1});
    circuit.add_op({GateKind::CNOT, 1, 0, 1, -1, 0, 0, 0, 2});
    const auto json = circuit_to_json(circuit);
    const auto loaded = circuit_from_json(json);
    CHECK(loaded.num_qubits == 2);
    CHECK(loaded.ops.size() == 2);
}

TEST_CASE("JSON description round-trip", "[json]") {
    Circuit circuit;
    circuit.num_qubits = 2;
    circuit.add_op({GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1});
    const auto json = circuit_to_json(circuit, "Bell state demo");
    const auto doc = circuit_document_from_json(json);
    CHECK(doc.description == "Bell state demo");
    CHECK(doc.circuit.ops.size() == 1);
}

TEST_CASE("Barrier does not affect simulation", "[circuit]") {
    Circuit circuit;
    circuit.num_qubits = 2;
    circuit.add_op({GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1});
    circuit.add_op({GateKind::Barrier, 1, 0, -1, -1, 0, 0, 0, 2});
    circuit.add_op({GateKind::CNOT, 2, 0, 1, -1, 0, 0, 0, 3});

    Simulator sim(2);
    sim.load_circuit(circuit);
    sim.run_all();
    CHECK(std::abs(sim.state().probability(0) - 0.5) < kTol);
    CHECK(std::abs(sim.state().probability(3) - 0.5) < kTol);
}

TEST_CASE("Bloch vector for |0>", "[bloch]") {
    StateVector sv(1);
    const auto view = compute_qubit_view(sv, 0);
    CHECK(std::abs(view.bloch_z - 1.0) < kTol);
    CHECK(std::abs(view.bloch_x) < kTol);
    CHECK(std::abs(view.bloch_y) < kTol);
}

TEST_CASE("Circuit gate count and depth", "[circuit]") {
    Circuit circuit;
    circuit.num_qubits = 2;
    circuit.add_op({GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1});
    circuit.add_op({GateKind::Barrier, 1, 0, -1, -1, 0, 0, 0, 2});
    circuit.add_op({GateKind::CNOT, 2, 0, 1, -1, 0, 0, 0, 3});
    CHECK(circuit.gate_count() == 2);
    CHECK(circuit.depth() == 3);
}

TEST_CASE("Initial state presets", "[simulator]") {
    Simulator sim(2);
    sim.set_initial_state(InitialStatePreset::AllOne);
    sim.reset();
    CHECK(std::abs(sim.state().probability(3) - 1.0) < kTol);

    sim.set_initial_state(InitialStatePreset::Q0Plus);
    sim.reset();
    CHECK(std::abs(sim.state().probability(0) - 0.5) < kTol);
    CHECK(std::abs(sim.state().probability(2) - 0.5) < kTol);
}

TEST_CASE("Measure collapse zeros qubit", "[gates]") {
    StateVector sv(1);
    apply_1q_gate(sv, 0, GateKind::H);
    std::mt19937 rng(42);
    apply_measure(sv, 0, true, rng);
    CHECK((std::abs(sv.probability(0) - 1.0) < kTol || std::abs(sv.probability(1) - 1.0) < kTol));
}

TEST_CASE("Multi-shot sampling sums to shot count", "[simulator]") {
    Circuit circuit;
    circuit.num_qubits = 2;
    circuit.add_op({GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1});
    circuit.add_op({GateKind::CNOT, 1, 0, 1, -1, 0, 0, 0, 2});

    Simulator sim(2);
    sim.load_circuit(circuit);
    sim.run_all();
    const auto hist = sim.sample_shots(1000);
    int total = 0;
    for (const auto& [idx, count] : hist) {
        total += count;
        CHECK((idx == 0 || idx == 3));
    }
    CHECK(total == 1000);
}

TEST_CASE("Highlight column tracks step", "[simulator]") {
    Circuit circuit;
    circuit.num_qubits = 2;
    circuit.add_op({GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1});
    circuit.add_op({GateKind::CNOT, 3, 0, 1, -1, 0, 0, 0, 2});

    Simulator sim(2);
    sim.load_circuit(circuit);
    REQUIRE(sim.highlight_column().has_value());
    CHECK(*sim.highlight_column() == 0);
    sim.step_forward();
    CHECK(*sim.highlight_column() == 3);
    sim.run_all();
    CHECK(*sim.highlight_column() == 3);
}

TEST_CASE("Preset JSON category field", "[json]") {
    Circuit circuit;
    circuit.num_qubits = 2;
    circuit.add_op({GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1});
    const auto json = circuit_to_json(circuit, "Test algo", "Algorithms");
    const auto doc = circuit_document_from_json(json);
    CHECK(doc.category == "Algorithms");
    CHECK(doc.description == "Test algo");
}

TEST_CASE("Gate matrix display", "[gates]") {
    CHECK(gate_matrix_display(GateKind::H).find("0.707") != std::string::npos);
    CHECK(gate_matrix_display(GateKind::CNOT).find("|10>") != std::string::npos);
}

TEST_CASE("CNOT rejects identical control and target", "[gates]") {
    StateVector sv(2);
    CHECK_THROWS_AS(apply_cnot(sv, 0, 0), std::invalid_argument);
}

TEST_CASE("CZ rejects identical qubits", "[gates]") {
    StateVector sv(2);
    CHECK_THROWS_AS(apply_cz(sv, 1, 1), std::invalid_argument);
}

TEST_CASE("ordered_gates preserves insertion order within column", "[circuit]") {
    Circuit circuit;
    circuit.num_qubits = 2;
    circuit.add_op({GateKind::Ry, 0, 1, -1, -1, 0.1, 0, 0, 1});
    circuit.add_op({GateKind::Ry, 0, 0, -1, -1, 0.2, 0, 0, 2});
    const auto ordered = circuit.ordered_gates();
    REQUIRE(ordered.size() == 2);
    CHECK(ordered[0].qubit == 1);
    CHECK(ordered[1].qubit == 0);
}

TEST_CASE("Simulator resizes when circuit qubit count changes", "[simulator]") {
    Circuit circuit;
    circuit.num_qubits = 3;
    circuit.add_op({GateKind::H, 0, 0, -1, -1, 0, 0, 0, 1});

    Simulator sim(2);
    sim.load_circuit(circuit);
    CHECK(sim.num_qubits() == 3);
    sim.run_all();
    CHECK(std::abs(sim.state().probability(0) - 0.5) < kTol);
    CHECK(std::abs(sim.state().probability(4) - 0.5) < kTol);
}

TEST_CASE("JSON rejects CNOT without qubit2", "[json]") {
    const char* json = R"({
  "version": 1,
  "num_qubits": 2,
  "operations": [
    {"gate": "CNOT", "column": 0, "qubit": 0}
  ]
})";
    CHECK_THROWS_AS(circuit_from_json(json), std::invalid_argument);
}
