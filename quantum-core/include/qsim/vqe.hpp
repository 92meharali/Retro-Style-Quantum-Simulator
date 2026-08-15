#pragma once

#include "qsim/ansatz.hpp"
#include "qsim/optimizer.hpp"
#include "qsim/pauli.hpp"
#include "qsim/qaoa.hpp"
#include "qsim/simulator.hpp"

#include <memory>
#include <random>
#include <vector>

namespace qsim {

class VQEExperiment {
public:
    VQEExperiment(Hamiltonian hamiltonian, std::shared_ptr<Ansatz> ansatz);

    const Hamiltonian& hamiltonian() const { return hamiltonian_; }
    Hamiltonian& hamiltonian() { return hamiltonian_; }
    void set_hamiltonian(Hamiltonian h) { hamiltonian_ = std::move(h); }

    const std::shared_ptr<Ansatz>& ansatz() const { return ansatz_; }
    void set_ansatz(std::shared_ptr<Ansatz> a) { ansatz_ = std::move(a); }

    int shots() const { return shots_; }
    void set_shots(int s) { shots_ = s; }

    /// Evaluates energy expectation <psi(params)| H |psi(params)>
    double evaluate_energy(const std::vector<double>& params);

    /// Generates statevector resulting from parameter vector
    StateVector compute_state(const std::vector<double>& params) const;

    /// Creates an OptimizerSession initialized with this experiment
    std::unique_ptr<OptimizerSession> create_session(const OptimizerConfig& config,
                                                     std::vector<double> initial_params = {});

    /// Computes a 2D energy landscape grid for parameter pair (px, py)
    std::vector<std::vector<double>> compute_2d_landscape(int px_idx, int py_idx,
                                                          int resolution = 25,
                                                          double x_min = 0.0, double x_max = 6.2831853,
                                                          double y_min = 0.0, double y_max = 6.2831853,
                                                          const std::vector<double>& base_params = {});

private:
    Hamiltonian hamiltonian_;
    std::shared_ptr<Ansatz> ansatz_;
    int shots_ = 0; // 0 = exact statevector, > 0 = shot noise
    mutable std::mt19937 rng_{42};
};

class QAOAExperiment {
public:
    explicit QAOAExperiment(MaxCutGraph graph, int layers = 1);

    const MaxCutGraph& graph() const { return graph_; }
    MaxCutGraph& graph() { return graph_; }
    void set_graph(MaxCutGraph g);

    int layers() const { return layers_; }
    void set_layers(int p);

    int shots() const { return shots_; }
    void set_shots(int s) { shots_ = s; }

    /// Evaluates QAOA cost (negative expected cut value to minimize)
    double evaluate_cost(const std::vector<double>& params);

    /// Evaluates expected cut value
    double evaluate_expected_cut(const std::vector<double>& params);

    /// Computes approximation ratio alpha = <C> / C_max
    double compute_approximation_ratio(const std::vector<double>& params);

    /// Creates an OptimizerSession initialized for QAOA
    std::unique_ptr<OptimizerSession> create_session(const OptimizerConfig& config,
                                                     std::vector<double> initial_params = {});

    /// Computes 2D landscape of expected cut value for (gamma, beta) at layer 1
    std::vector<std::vector<double>> compute_2d_landscape(int resolution = 25,
                                                          double gamma_max = 3.14159265,
                                                          double beta_max = 1.57079632);

    /// Samples bitstrings and returns list of (bitstring, probability, cut_value)
    struct SolutionOutcome {
        int bitstring = 0;
        double probability = 0.0;
        double cut_value = 0.0;
        bool is_optimal = false;
    };
    std::vector<SolutionOutcome> rank_solutions(const std::vector<double>& params,
                                                int max_results = 8) const;

    Circuit build_current_circuit(const std::vector<double>& params) const;

private:
    MaxCutGraph graph_;
    int layers_ = 1;
    QAOAMaxCutAnsatz ansatz_;
    Hamiltonian cost_hamiltonian_;
    Hamiltonian qaoa_min_hamiltonian_;
    int shots_ = 0;
    mutable std::mt19937 rng_{42};
};

}  // namespace qsim
