#include "qsim/vqe.hpp"

#include <algorithm>
#include <cmath>

namespace qsim {

VQEExperiment::VQEExperiment(Hamiltonian hamiltonian, std::shared_ptr<Ansatz> ansatz)
    : hamiltonian_(std::move(hamiltonian)), ansatz_(std::move(ansatz)) {}

StateVector VQEExperiment::compute_state(const std::vector<double>& params) const {
    if (!ansatz_) return StateVector(hamiltonian_.num_qubits());
    Circuit circuit = ansatz_->build_circuit(params);
    Simulator sim(circuit.num_qubits);
    sim.load_circuit(circuit);
    sim.run_all();
    return sim.state();
}

double VQEExperiment::evaluate_energy(const std::vector<double>& params) {
    StateVector state = compute_state(params);
    if (shots_ > 0) {
        return hamiltonian_.expectation_shots(state, shots_, rng_);
    }
    return hamiltonian_.expectation(state);
}

std::unique_ptr<OptimizerSession> VQEExperiment::create_session(const OptimizerConfig& config,
                                                                 std::vector<double> initial_params) {
    if (initial_params.empty() && ansatz_) {
        initial_params = ansatz_->default_initial_params();
    }
    auto cost_fn = [this](const std::vector<double>& p) {
        return this->evaluate_energy(p);
    };
    return std::make_unique<OptimizerSession>(cost_fn, std::move(initial_params), config);
}

std::vector<std::vector<double>> VQEExperiment::compute_2d_landscape(
    int px_idx, int py_idx, int resolution,
    double x_min, double x_max, double y_min, double y_max,
    const std::vector<double>& base_params) {

    resolution = std::clamp(resolution, 5, 100);
    std::vector<std::vector<double>> grid(resolution, std::vector<double>(resolution, 0.0));

    std::vector<double> p = base_params;
    if (p.empty() && ansatz_) {
        p = ansatz_->default_initial_params();
    }
    while (p.size() <= static_cast<std::size_t>(std::max(px_idx, py_idx))) {
        p.push_back(0.0);
    }

    const double dx = (x_max - x_min) / (resolution - 1);
    const double dy = (y_max - y_min) / (resolution - 1);

    for (int iy = 0; iy < resolution; ++iy) {
        p[py_idx] = y_min + iy * dy;
        for (int ix = 0; ix < resolution; ++ix) {
            p[px_idx] = x_min + ix * dx;
            // Always evaluate exact expectation for landscape rendering
            StateVector state = compute_state(p);
            grid[iy][ix] = hamiltonian_.expectation(state);
        }
    }
    return grid;
}

QAOAExperiment::QAOAExperiment(MaxCutGraph graph, int layers)
    : graph_(std::move(graph)), layers_(std::max(1, layers)),
      ansatz_(graph_, layers_),
      cost_hamiltonian_(graph_.cost_hamiltonian()),
      qaoa_min_hamiltonian_(graph_.qaoa_hamiltonian()) {}

void QAOAExperiment::set_graph(MaxCutGraph g) {
    graph_ = std::move(g);
    ansatz_.set_graph(graph_);
    cost_hamiltonian_ = graph_.cost_hamiltonian();
    qaoa_min_hamiltonian_ = graph_.qaoa_hamiltonian();
}

void QAOAExperiment::set_layers(int p) {
    layers_ = std::max(1, p);
    ansatz_.set_layers(layers_);
}

Circuit QAOAExperiment::build_current_circuit(const std::vector<double>& params) const {
    return ansatz_.build_circuit(params);
}

double QAOAExperiment::evaluate_cost(const std::vector<double>& params) {
    Circuit circuit = ansatz_.build_circuit(params);
    Simulator sim(circuit.num_qubits);
    sim.load_circuit(circuit);
    sim.run_all();

    if (shots_ > 0) {
        return qaoa_min_hamiltonian_.expectation_shots(sim.state(), shots_, rng_);
    }
    return qaoa_min_hamiltonian_.expectation(sim.state());
}

double QAOAExperiment::evaluate_expected_cut(const std::vector<double>& params) {
    Circuit circuit = ansatz_.build_circuit(params);
    Simulator sim(circuit.num_qubits);
    sim.load_circuit(circuit);
    sim.run_all();

    if (shots_ > 0) {
        return cost_hamiltonian_.expectation_shots(sim.state(), shots_, rng_);
    }
    return cost_hamiltonian_.expectation(sim.state());
}

double QAOAExperiment::compute_approximation_ratio(const std::vector<double>& params) {
    double exp_cut = evaluate_expected_cut(params);
    double max_cut = graph_.max_cut_exact();
    if (max_cut < 1e-9) return 1.0;
    return std::clamp(exp_cut / max_cut, 0.0, 1.0);
}

std::unique_ptr<OptimizerSession> QAOAExperiment::create_session(const OptimizerConfig& config,
                                                                 std::vector<double> initial_params) {
    if (initial_params.empty()) {
        initial_params = ansatz_.default_initial_params();
    }
    auto cost_fn = [this](const std::vector<double>& p) {
        return this->evaluate_cost(p);
    };
    return std::make_unique<OptimizerSession>(cost_fn, std::move(initial_params), config);
}

std::vector<std::vector<double>> QAOAExperiment::compute_2d_landscape(
    int resolution, double gamma_max, double beta_max) {

    resolution = std::clamp(resolution, 5, 100);
    std::vector<std::vector<double>> grid(resolution, std::vector<double>(resolution, 0.0));

    const double d_gamma = gamma_max / (resolution - 1);
    const double d_beta = beta_max / (resolution - 1);

    std::vector<double> p(static_cast<std::size_t>(2 * layers_), 0.0);

    for (int ib = 0; ib < resolution; ++ib) {
        p[1] = ib * d_beta;
        for (int ig = 0; ig < resolution; ++ig) {
            p[0] = ig * d_gamma;
            Circuit circuit = ansatz_.build_circuit(p);
            Simulator sim(circuit.num_qubits);
            sim.load_circuit(circuit);
            sim.run_all();
            grid[ib][ig] = cost_hamiltonian_.expectation(sim.state());
        }
    }
    return grid;
}

std::vector<QAOAExperiment::SolutionOutcome> QAOAExperiment::rank_solutions(
    const std::vector<double>& params, int max_results) const {

    Circuit circuit = ansatz_.build_circuit(params);
    Simulator sim(circuit.num_qubits);
    sim.load_circuit(circuit);
    sim.run_all();

    const auto& state = sim.state();
    const int dim = 1 << graph_.num_nodes();
    const double max_cut = graph_.max_cut_exact();

    std::vector<SolutionOutcome> outcomes;
    outcomes.reserve(static_cast<std::size_t>(dim));

    for (int i = 0; i < dim; ++i) {
        double p = state.probability(i);
        double cut = graph_.evaluate_cut(i);
        bool opt = std::abs(cut - max_cut) < 1e-6;
        outcomes.push_back({i, p, cut, opt});
    }

    std::sort(outcomes.begin(), outcomes.end(), [](const SolutionOutcome& a, const SolutionOutcome& b) {
        return a.probability > b.probability;
    });

    if (static_cast<int>(outcomes.size()) > max_results) {
        outcomes.resize(static_cast<std::size_t>(max_results));
    }
    return outcomes;
}

}  // namespace qsim
