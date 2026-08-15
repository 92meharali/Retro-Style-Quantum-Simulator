#include "qsim/optimizer.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

namespace qsim {

const char* optimizer_name(OptimizerKind kind) {
    switch (kind) {
        case OptimizerKind::Adam: return "Adam";
        case OptimizerKind::GradientDescent: return "Gradient Descent (Momentum)";
        case OptimizerKind::COBYLA: return "COBYLA / Simplex";
        case OptimizerKind::SPSA: return "SPSA (Stochastic)";
    }
    return "Adam";
}

std::vector<double> parameter_shift_gradient(const CostFunction& cost_fn,
                                             const std::vector<double>& params,
                                             double shift) {
    const std::size_t n = params.size();
    std::vector<double> grad(n, 0.0);
    const double denominator = 2.0 * std::sin(shift);
    const double denom = (std::abs(denominator) < 1e-12) ? 2.0 : denominator;

    std::vector<double> shifted = params;
    for (std::size_t i = 0; i < n; ++i) {
        shifted[i] = params[i] + shift;
        const double f_plus = cost_fn(shifted);

        shifted[i] = params[i] - shift;
        const double f_minus = cost_fn(shifted);

        shifted[i] = params[i]; // restore
        grad[i] = (f_plus - f_minus) / denom;
    }
    return grad;
}

std::vector<double> finite_diff_gradient(const CostFunction& cost_fn,
                                         const std::vector<double>& params,
                                         double eps) {
    const std::size_t n = params.size();
    std::vector<double> grad(n, 0.0);
    std::vector<double> shifted = params;

    for (std::size_t i = 0; i < n; ++i) {
        shifted[i] = params[i] + eps;
        const double f_plus = cost_fn(shifted);

        shifted[i] = params[i] - eps;
        const double f_minus = cost_fn(shifted);

        shifted[i] = params[i];
        grad[i] = (f_plus - f_minus) / (2.0 * eps);
    }
    return grad;
}

OptimizerSession::OptimizerSession(CostFunction cost_fn, std::vector<double> initial_params,
                                   OptimizerConfig config)
    : cost_fn_(std::move(cost_fn)), config_(config), params_(std::move(initial_params)) {
    reset();
}

void OptimizerSession::reset() {
    reset(params_);
}

void OptimizerSession::reset(std::vector<double> initial_params) {
    params_ = std::move(initial_params);
    best_params_ = params_;
    iteration_ = 0;
    converged_ = false;
    history_.clear();

    const std::size_t dim = params_.size();
    m_.assign(dim, 0.0);
    v_.assign(dim, 0.0);
    momentum_.assign(dim, 0.0);

    if (cost_fn_ && !params_.empty()) {
        current_cost_ = cost_fn_(params_);
        best_cost_ = current_cost_;
        // Record initial state (Iteration 0)
        history_.push_back({0, current_cost_, 0.0, 0.0, params_});
    } else {
        current_cost_ = 0.0;
        best_cost_ = 1e9;
    }

    simplex_initialized_ = false;
}

void OptimizerSession::clear_history() {
    history_.clear();
}

StepRecord OptimizerSession::step() {
    if (!cost_fn_ || params_.empty() || converged_) {
        return history_.empty() ? StepRecord{} : history_.back();
    }

    iteration_++;

    if (config_.kind == OptimizerKind::SPSA) {
        return step_spsa();
    }
    if (config_.kind == OptimizerKind::COBYLA) {
        return step_simplex();
    }

    // Gradient-based step
    std::vector<double> grad;
    if (config_.use_parameter_shift) {
        grad = parameter_shift_gradient(cost_fn_, params_);
    } else {
        grad = finite_diff_gradient(cost_fn_, params_);
    }

    if (config_.kind == OptimizerKind::Adam) {
        return step_adam(grad);
    }
    return step_gd(grad);
}

StepRecord OptimizerSession::step_adam(const std::vector<double>& grad) {
    const std::size_t dim = params_.size();
    double grad_norm_sq = 0.0;

    const double b1 = config_.beta1;
    const double b2 = config_.beta2;
    const double alpha = config_.learning_rate;
    const double eps = config_.epsilon;

    const double b1_t = std::pow(b1, static_cast<double>(iteration_));
    const double b2_t = std::pow(b2, static_cast<double>(iteration_));

    for (std::size_t i = 0; i < dim; ++i) {
        const double g = grad[i];
        grad_norm_sq += g * g;

        m_[i] = b1 * m_[i] + (1.0 - b1) * g;
        v_[i] = b2 * v_[i] + (1.0 - b2) * g * g;

        const double m_hat = m_[i] / (1.0 - b1_t);
        const double v_hat = v_[i] / (1.0 - b2_t);

        params_[i] -= (alpha / (std::sqrt(v_hat) + eps)) * m_hat;
    }

    const double grad_norm = std::sqrt(grad_norm_sq);
    const double prev_cost = current_cost_;
    current_cost_ = cost_fn_(params_);
    const double delta_cost = current_cost_ - prev_cost;

    if (current_cost_ < best_cost_) {
        best_cost_ = current_cost_;
        best_params_ = params_;
    }

    if (std::abs(delta_cost) < config_.tolerance && grad_norm < config_.tolerance * 10.0) {
        converged_ = true;
    }
    if (iteration_ >= config_.max_iterations) {
        converged_ = true;
    }

    StepRecord rec{iteration_, current_cost_, delta_cost, grad_norm, params_};
    history_.push_back(rec);
    return rec;
}

StepRecord OptimizerSession::step_gd(const std::vector<double>& grad) {
    const std::size_t dim = params_.size();
    double grad_norm_sq = 0.0;
    const double lr = config_.learning_rate;
    const double beta = config_.beta1; // Momentum factor

    for (std::size_t i = 0; i < dim; ++i) {
        const double g = grad[i];
        grad_norm_sq += g * g;
        momentum_[i] = beta * momentum_[i] + lr * g;
        params_[i] -= momentum_[i];
    }

    const double grad_norm = std::sqrt(grad_norm_sq);
    const double prev_cost = current_cost_;
    current_cost_ = cost_fn_(params_);
    const double delta_cost = current_cost_ - prev_cost;

    if (current_cost_ < best_cost_) {
        best_cost_ = current_cost_;
        best_params_ = params_;
    }

    if (std::abs(delta_cost) < config_.tolerance) {
        converged_ = true;
    }
    if (iteration_ >= config_.max_iterations) {
        converged_ = true;
    }

    StepRecord rec{iteration_, current_cost_, delta_cost, grad_norm, params_};
    history_.push_back(rec);
    return rec;
}

StepRecord OptimizerSession::step_spsa() {
    const std::size_t dim = params_.size();
    const double k = static_cast<double>(iteration_);
    const double ak = config_.learning_rate / std::pow(k + 10.0, 0.602);
    const double ck = config_.spsa_c / std::pow(k + 1.0, 0.101);

    std::uniform_int_distribution<int> dist(0, 1);
    std::vector<double> delta(dim);
    for (std::size_t i = 0; i < dim; ++i) {
        delta[i] = dist(rng_) ? 1.0 : -1.0;
    }

    std::vector<double> p_plus = params_;
    std::vector<double> p_minus = params_;
    for (std::size_t i = 0; i < dim; ++i) {
        p_plus[i] += ck * delta[i];
        p_minus[i] -= ck * delta[i];
    }

    const double y_plus = cost_fn_(p_plus);
    const double y_minus = cost_fn_(p_minus);

    double grad_norm_sq = 0.0;
    for (std::size_t i = 0; i < dim; ++i) {
        const double gh = (y_plus - y_minus) / (2.0 * ck * delta[i]);
        grad_norm_sq += gh * gh;
        params_[i] -= ak * gh;
    }

    const double grad_norm = std::sqrt(grad_norm_sq);
    const double prev_cost = current_cost_;
    current_cost_ = cost_fn_(params_);
    const double delta_cost = current_cost_ - prev_cost;

    if (current_cost_ < best_cost_) {
        best_cost_ = current_cost_;
        best_params_ = params_;
    }

    if (iteration_ >= config_.max_iterations) {
        converged_ = true;
    }

    StepRecord rec{iteration_, current_cost_, delta_cost, grad_norm, params_};
    history_.push_back(rec);
    return rec;
}

StepRecord OptimizerSession::step_simplex() {
    const std::size_t dim = params_.size();
    if (!simplex_initialized_) {
        simplex_points_.clear();
        simplex_values_.clear();
        simplex_points_.push_back(params_);
        simplex_values_.push_back(current_cost_);

        for (std::size_t i = 0; i < dim; ++i) {
            std::vector<double> pt = params_;
            pt[i] += (std::abs(pt[i]) > 1e-4) ? 0.05 * pt[i] : 0.05;
            simplex_points_.push_back(pt);
            simplex_values_.push_back(cost_fn_(pt));
        }
        simplex_initialized_ = true;
    }

    // Sort simplex vertices by cost value
    std::vector<std::size_t> order(simplex_points_.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [this](std::size_t a, std::size_t b) {
        return simplex_values_[a] < simplex_values_[b];
    });

    const std::size_t best_idx = order[0];
    const std::size_t worst_idx = order.back();
    const std::size_t second_worst_idx = order[order.size() - 2];

    // Compute centroid of all vertices except worst
    std::vector<double> centroid(dim, 0.0);
    for (std::size_t i = 0; i < order.size() - 1; ++i) {
        const auto& pt = simplex_points_[order[i]];
        for (std::size_t d = 0; d < dim; ++d) {
            centroid[d] += pt[d];
        }
    }
    for (std::size_t d = 0; d < dim; ++d) {
        centroid[d] /= static_cast<double>(dim);
    }

    // Reflection
    const double alpha = 1.0;
    std::vector<double> reflected(dim);
    for (std::size_t d = 0; d < dim; ++d) {
        reflected[d] = centroid[d] + alpha * (centroid[d] - simplex_points_[worst_idx][d]);
    }
    const double r_val = cost_fn_(reflected);

    if (r_val < simplex_values_[best_idx]) {
        // Expansion
        const double gamma = 2.0;
        std::vector<double> expanded(dim);
        for (std::size_t d = 0; d < dim; ++d) {
            expanded[d] = centroid[d] + gamma * (reflected[d] - centroid[d]);
        }
        const double e_val = cost_fn_(expanded);
        if (e_val < r_val) {
            simplex_points_[worst_idx] = expanded;
            simplex_values_[worst_idx] = e_val;
        } else {
            simplex_points_[worst_idx] = reflected;
            simplex_values_[worst_idx] = r_val;
        }
    } else if (r_val < simplex_values_[second_worst_idx]) {
        simplex_points_[worst_idx] = reflected;
        simplex_values_[worst_idx] = r_val;
    } else {
        // Contraction
        const double rho = 0.5;
        std::vector<double> contracted(dim);
        for (std::size_t d = 0; d < dim; ++d) {
            contracted[d] = centroid[d] + rho * (simplex_points_[worst_idx][d] - centroid[d]);
        }
        const double c_val = cost_fn_(contracted);
        if (c_val < simplex_values_[worst_idx]) {
            simplex_points_[worst_idx] = contracted;
            simplex_values_[worst_idx] = c_val;
        } else {
            // Shrink
            const double sigma = 0.5;
            for (std::size_t i = 1; i < simplex_points_.size(); ++i) {
                const std::size_t idx = order[i];
                for (std::size_t d = 0; d < dim; ++d) {
                    simplex_points_[idx][d] = simplex_points_[best_idx][d] +
                                              sigma * (simplex_points_[idx][d] - simplex_points_[best_idx][d]);
                }
                simplex_values_[idx] = cost_fn_(simplex_points_[idx]);
            }
        }
    }

    // Update current best
    std::size_t new_best = 0;
    for (std::size_t i = 1; i < simplex_values_.size(); ++i) {
        if (simplex_values_[i] < simplex_values_[new_best]) {
            new_best = i;
        }
    }

    params_ = simplex_points_[new_best];
    const double prev_cost = current_cost_;
    current_cost_ = simplex_values_[new_best];
    const double delta_cost = current_cost_ - prev_cost;

    if (current_cost_ < best_cost_) {
        best_cost_ = current_cost_;
        best_params_ = params_;
    }

    if (iteration_ >= config_.max_iterations) {
        converged_ = true;
    }

    StepRecord rec{iteration_, current_cost_, delta_cost, 0.0, params_};
    history_.push_back(rec);
    return rec;
}

void OptimizerSession::run(int max_steps) {
    const int limit = (max_steps > 0) ? max_steps : config_.max_iterations;
    for (int i = 0; i < limit && !converged_; ++i) {
        step();
    }
}

}  // namespace qsim
