#include <catch2/catch_test_macros.hpp>

#include "qsim/ansatz.hpp"
#include "qsim/gates.hpp"
#include "qsim/optimizer.hpp"
#include "qsim/pauli.hpp"
#include "qsim/qaoa.hpp"
#include "qsim/simulator.hpp"
#include "qsim/vqe.hpp"

#include <cmath>
#include <iostream>

using namespace qsim;

static constexpr double kTol = 1e-6;

TEST_CASE("Pauli expectation values on basis states", "[pauli]") {
    StateVector sv(2); // |00>
    PauliTerm z0_z1(1.0, {{0, PauliOp::Z}, {1, PauliOp::Z}});
    CHECK(std::abs(z0_z1.expectation(sv) - 1.0) < kTol);

    PauliTerm x0_x1(1.0, {{0, PauliOp::X}, {1, PauliOp::X}});
    CHECK(std::abs(x0_x1.expectation(sv) - 0.0) < kTol);

    // Apply X on qubit 0 -> |10>
    apply_1q_gate(sv, 0, GateKind::X);
    CHECK(std::abs(z0_z1.expectation(sv) - (-1.0)) < kTol);
}

TEST_CASE("Pauli expectation values on Bell state", "[pauli]") {
    StateVector sv(2);
    apply_1q_gate(sv, 0, GateKind::H);
    apply_cnot(sv, 0, 1); // (|00> + |11>) / sqrt(2)

    PauliTerm xx(1.0, {{0, PauliOp::X}, {1, PauliOp::X}});
    PauliTerm zz(1.0, {{0, PauliOp::Z}, {1, PauliOp::Z}});
    PauliTerm yy(1.0, {{0, PauliOp::Y}, {1, PauliOp::Y}});

    CHECK(std::abs(xx.expectation(sv) - 1.0) < kTol);
    CHECK(std::abs(zz.expectation(sv) - 1.0) < kTol);
    CHECK(std::abs(yy.expectation(sv) - (-1.0)) < kTol);
}

TEST_CASE("Hamiltonian exact ground state calculation", "[hamiltonian]") {
    auto h_xx_zz = Hamiltonian::two_qubit_xx_zz();
    double e_min = h_xx_zz.ground_state_energy_exact();
    // Theoretical minimum is -2.0
    CHECK(std::abs(e_min - (-2.0)) < 1e-4);
}

TEST_CASE("Hamiltonian string parser", "[hamiltonian]") {
    auto h = Hamiltonian::from_string("1.0*X0*X1 + 1.0*Z0*Z1 - 0.5*Z0", 2);
    CHECK(h.size() == 3);

    StateVector sv(2);
    apply_1q_gate(sv, 0, GateKind::H);
    apply_cnot(sv, 0, 1);
    // <Phi+| (XX + ZZ - 0.5*Z0) |Phi+> = 1.0 + 1.0 - 0.5*(0.0) = 2.0
    CHECK(std::abs(h.expectation(sv) - 2.0) < kTol);
}

TEST_CASE("Parameter-shift rule matches finite differences", "[gradient]") {
    TwoQubitRyCnotAnsatz ansatz(false); // 2 parameters
    auto h = Hamiltonian::two_qubit_xx_zz();
    VQEExperiment exp(h, std::make_shared<TwoQubitRyCnotAnsatz>(false));

    auto cost_fn = [&exp](const std::vector<double>& p) {
        return exp.evaluate_energy(p);
    };

    std::vector<double> test_params = {0.785, 1.25};
    auto ps_grad = parameter_shift_gradient(cost_fn, test_params);
    auto fd_grad = finite_diff_gradient(cost_fn, test_params, 1e-5);

    REQUIRE(ps_grad.size() == 2);
    REQUIRE(fd_grad.size() == 2);
    CHECK(std::abs(ps_grad[0] - fd_grad[0]) < 1e-4);
    CHECK(std::abs(ps_grad[1] - fd_grad[1]) < 1e-4);
}

TEST_CASE("VQE ground state convergence on XX + ZZ Hamiltonian", "[vqe]") {
    auto h = Hamiltonian::two_qubit_xx_zz();
    auto ansatz = std::make_shared<TwoQubitRyCnotAnsatz>(true); // 4 params
    VQEExperiment exp(h, ansatz);

    OptimizerConfig cfg;
    cfg.kind = OptimizerKind::Adam;
    cfg.learning_rate = 0.1;
    cfg.max_iterations = 80;
    cfg.use_parameter_shift = true;

    auto session = exp.create_session(cfg, {0.1, 0.2, 0.1, 0.2});
    double initial_energy = session->current_cost();

    session->run(80);

    double final_energy = session->best_cost();
    std::cout << "[Test VQE] Initial energy: " << initial_energy
              << " -> Final converged energy: " << final_energy << " (Exact: -2.0)\n";

    // Verifies significant optimization progress towards exact ground state -2.0
    CHECK(final_energy < -1.95);
    CHECK(session->history().size() > 1);
}

TEST_CASE("VQE ground state convergence on H2 molecule", "[vqe]") {
    auto h_h2 = Hamiltonian::h2_molecule(0.7414);
    auto ansatz = std::make_shared<TwoQubitRyCnotAnsatz>(true);
    VQEExperiment exp(h_h2, ansatz);

    OptimizerConfig cfg;
    cfg.kind = OptimizerKind::Adam;
    cfg.learning_rate = 0.08;
    cfg.max_iterations = 100;

    auto session = exp.create_session(cfg, {0.1, 0.1, 0.1, 0.1});
    session->run(100);

    double exact_e0 = h_h2.ground_state_energy_exact();
    double vqe_e0 = session->best_cost();

    std::cout << "[Test H2 VQE] VQE energy: " << vqe_e0
              << " vs Exact ground state: " << exact_e0 << "\n";

    CHECK(std::abs(vqe_e0 - exact_e0) < 0.05);
}

TEST_CASE("QAOA MaxCut on Triangle Graph", "[qaoa]") {
    auto g = MaxCutGraph::triangle();
    CHECK(g.num_nodes() == 3);
    CHECK(g.edges().size() == 3);

    double max_cut = g.max_cut_exact();
    CHECK(std::abs(max_cut - 2.0) < kTol);

    QAOAExperiment qaoa(g, 1);
    OptimizerConfig cfg;
    cfg.kind = OptimizerKind::Adam;
    cfg.learning_rate = 0.1;
    cfg.max_iterations = 50;

    auto session = qaoa.create_session(cfg, {0.6, 0.3});
    session->run(50);

    double approx_ratio = qaoa.compute_approximation_ratio(session->best_params());
    std::cout << "[Test QAOA Triangle] Approximation ratio: " << approx_ratio << "\n";
    CHECK(approx_ratio >= 0.85);

    auto solutions = qaoa.rank_solutions(session->best_params(), 4);
    REQUIRE(!solutions.empty());
    // Top solution should be an optimal cut with cut value 2.0
    CHECK(solutions[0].cut_value == 2.0);
    CHECK(solutions[0].is_optimal == true);
}

TEST_CASE("QAOA MaxCut on 4-Cycle Graph", "[qaoa]") {
    auto g = MaxCutGraph::cycle4();
    CHECK(g.num_nodes() == 4);
    CHECK(g.edges().size() == 4);

    double max_cut = g.max_cut_exact();
    CHECK(std::abs(max_cut - 4.0) < kTol);

    // p=1 layer test
    QAOAExperiment qaoa1(g, 1);
    OptimizerConfig cfg1;
    cfg1.kind = OptimizerKind::Adam;
    cfg1.learning_rate = 0.08;
    cfg1.max_iterations = 60;

    auto session1 = qaoa1.create_session(cfg1, {0.785, 0.393});
    session1->run(60);

    double approx_ratio1 = qaoa1.compute_approximation_ratio(session1->best_params());
    std::cout << "[Test QAOA Cycle4 p=1] Approximation ratio: " << approx_ratio1 << "\n";
    CHECK(approx_ratio1 >= 0.65);

    auto solutions1 = qaoa1.rank_solutions(session1->best_params(), 2);
    REQUIRE(!solutions1.empty());
    CHECK(solutions1[0].cut_value == 4.0);

    // p=2 layer test: higher approximation ratio
    QAOAExperiment qaoa2(g, 2);
    OptimizerConfig cfg2;
    cfg2.kind = OptimizerKind::Adam;
    cfg2.learning_rate = 0.08;
    cfg2.max_iterations = 80;

    auto session2 = qaoa2.create_session(cfg2, {0.785, 0.393, 0.785, 0.393});
    session2->run(80);

    double approx_ratio2 = qaoa2.compute_approximation_ratio(session2->best_params());
    std::cout << "[Test QAOA Cycle4 p=2] Approximation ratio: " << approx_ratio2 << "\n";
    CHECK(approx_ratio2 >= 0.70);
}
