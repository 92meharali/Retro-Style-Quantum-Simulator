#pragma once

#include <functional>
#include <random>
#include <string>
#include <vector>

namespace qsim {

enum class OptimizerKind {
    Adam,
    GradientDescent,
    COBYLA, // Nelder-Mead simplex derivative-free
    SPSA,   // Simultaneous perturbation stochastic approximation
};

const char* optimizer_name(OptimizerKind kind);

using CostFunction = std::function<double(const std::vector<double>& params)>;
using GradientFunction = std::function<std::vector<double>(const std::vector<double>& params)>;

/// Parameter-shift rule analytical gradient:
/// df/dθ_k = (f(θ_k + π/2) - f(θ_k - π/2)) / 2
std::vector<double> parameter_shift_gradient(const CostFunction& cost_fn,
                                             const std::vector<double>& params,
                                             double shift = 1.5707963267948966);

/// Central finite difference gradient: (f(θ + ε) - f(θ - ε)) / (2ε)
std::vector<double> finite_diff_gradient(const CostFunction& cost_fn,
                                         const std::vector<double>& params,
                                         double eps = 1e-5);

struct StepRecord {
    int iteration = 0;
    double cost = 0.0;
    double delta_cost = 0.0;
    double grad_norm = 0.0;
    std::vector<double> params;
};

struct OptimizerConfig {
    OptimizerKind kind = OptimizerKind::Adam;
    double learning_rate = 0.08;
    int max_iterations = 100;
    double tolerance = 1e-6;
    bool use_parameter_shift = true;
    double beta1 = 0.9;     // Adam / Momentum
    double beta2 = 0.999;   // Adam
    double epsilon = 1e-8;  // Adam
    double spsa_a = 0.1;    // SPSA step
    double spsa_c = 0.1;    // SPSA perturbation scale
};

class OptimizerSession {
public:
    OptimizerSession(CostFunction cost_fn, std::vector<double> initial_params,
                     OptimizerConfig config = {});

    void set_cost_function(CostFunction fn) { cost_fn_ = std::move(fn); }
    void set_config(const OptimizerConfig& cfg) { config_ = cfg; }
    const OptimizerConfig& config() const { return config_; }
    OptimizerConfig& config() { return config_; }

    void reset();
    void reset(std::vector<double> initial_params);

    /// Execute a single optimization step
    StepRecord step();

    /// Run up to max_steps (or until converged)
    void run(int max_steps = -1);

    int current_iteration() const { return iteration_; }
    bool is_converged() const { return converged_; }
    const std::vector<double>& current_params() const { return params_; }
    double current_cost() const { return current_cost_; }

    const std::vector<double>& best_params() const { return best_params_; }
    double best_cost() const { return best_cost_; }

    const std::vector<StepRecord>& history() const { return history_; }
    void clear_history();

private:
    StepRecord step_adam(const std::vector<double>& grad);
    StepRecord step_gd(const std::vector<double>& grad);
    StepRecord step_spsa();
    StepRecord step_simplex();

    CostFunction cost_fn_;
    OptimizerConfig config_;
    std::vector<double> params_;
    std::vector<double> best_params_;
    double current_cost_ = 0.0;
    double best_cost_ = 1e9;
    int iteration_ = 0;
    bool converged_ = false;
    std::vector<StepRecord> history_;

    // Optimizer internal states
    std::vector<double> m_;      // Adam 1st moment
    std::vector<double> v_;      // Adam 2nd moment
    std::vector<double> momentum_; // GD momentum

    // Nelder-Mead simplex state
    std::vector<std::vector<double>> simplex_points_;
    std::vector<double> simplex_values_;
    bool simplex_initialized_ = false;

    std::mt19937 rng_{42};
};

}  // namespace qsim
