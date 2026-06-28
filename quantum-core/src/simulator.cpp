#include "qsim/simulator.hpp"

#include "qsim/gates.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace qsim {

Simulator::Simulator(int num_qubits) : state_(num_qubits) {
    reset();
}

void Simulator::set_initial_state(InitialStatePreset preset) {
    initial_preset_ = preset;
}

void Simulator::set_collapse_on_measure(bool collapse) {
    collapse_on_measure_ = collapse;
}

void Simulator::apply_initial_state() {
    state_.reset();
    switch (initial_preset_) {
        case InitialStatePreset::AllZero:
            break;
        case InitialStatePreset::AllOne:
            state_.collapse((1 << state_.num_qubits()) - 1);
            break;
        case InitialStatePreset::Q0Plus:
            apply_1q_gate(state_, 0, GateKind::H);
            break;
    }
}

void Simulator::reset() {
    apply_initial_state();
    step_ = 0;
    checkpoints_.clear();
    checkpoints_.push_back(state_);
}

void Simulator::resync_from_circuit_() {
    if (circuit_.num_qubits != state_.num_qubits()) {
        state_ = StateVector(circuit_.num_qubits);
    }
    gate_sequence_ = circuit_.ordered_gates();
    reset();
}

void Simulator::load_circuit(const Circuit& circuit) {
    circuit_ = circuit;
    resync_from_circuit_();
}

void Simulator::apply_step(int index) {
    if (index < 0 || index >= static_cast<int>(gate_sequence_.size())) return;
    const auto& op = gate_sequence_[static_cast<std::size_t>(index)];
    if (op.kind == GateKind::Measure) {
        apply_measure(state_, op.qubit, collapse_on_measure_, rng_);
        return;
    }
    apply_gate(state_, op.kind, op.qubit, op.qubit2, op.qubit3,
               op.param1, op.param2, op.param3);
}

void Simulator::save_checkpoint(int step) {
    while (static_cast<int>(checkpoints_.size()) <= step) {
        checkpoints_.push_back(state_);
    }
    checkpoints_[static_cast<std::size_t>(step)] = state_;
}

void Simulator::restore_checkpoint(int step) {
    if (step >= 0 && step < static_cast<int>(checkpoints_.size())) {
        state_ = checkpoints_[static_cast<std::size_t>(step)];
        return;
    }
    if (step <= 0) {
        apply_initial_state();
        return;
    }
    apply_initial_state();
    for (int i = 0; i < step && i < total_steps(); ++i) {
        apply_step(i);
    }
}

void Simulator::run_all() {
    while (step_ < total_steps()) {
        save_checkpoint(step_);
        apply_step(step_);
        ++step_;
    }
    save_checkpoint(step_);
}

void Simulator::step_forward() {
    if (step_ >= total_steps()) return;
    save_checkpoint(step_);
    apply_step(step_);
    ++step_;
    save_checkpoint(step_);
}

void Simulator::step_back() {
    if (step_ <= 0) {
        apply_initial_state();
        step_ = 0;
        return;
    }
    --step_;
    restore_checkpoint(step_);
}

std::optional<int> Simulator::highlight_column() const {
    if (gate_sequence_.empty()) return std::nullopt;
    if (step_ >= total_steps()) {
        return gate_sequence_.back().column;
    }
    return gate_sequence_[static_cast<std::size_t>(step_)].column;
}

const StateVector* Simulator::checkpoint_state(int step) const {
    if (step < 0 || step >= static_cast<int>(checkpoints_.size())) return nullptr;
    return &checkpoints_[static_cast<std::size_t>(step)];
}

std::vector<std::pair<int, int>> Simulator::sample_shots(int shots) {
    if (shots <= 0) return {};
    std::unordered_map<int, int> counts;
    counts.reserve(static_cast<std::size_t>(shots));
    for (int i = 0; i < shots; ++i) {
        ++counts[state_.sample(rng_)];
    }
    std::vector<std::pair<int, int>> result;
    result.reserve(counts.size());
    for (const auto& [index, count] : counts) {
        result.emplace_back(index, count);
    }
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    return result;
}

QubitView compute_qubit_view(const StateVector& state, int qubit) {
    QubitView view{};
    const int mask = qubit_mask(state.num_qubits(), qubit);
    const auto& amps = state.amplitudes();
    Complex rho01(0.0, 0.0);

    for (std::size_t i = 0; i < amps.size(); ++i) {
        if ((static_cast<int>(i) & mask) == 0) {
            view.prob_zero += std::norm(amps[i]);
            rho01 += amps[i] * std::conj(amps[static_cast<std::size_t>(i | static_cast<std::size_t>(mask))]);
        }
    }
    view.prob_one = 1.0 - view.prob_zero;
    view.bloch_x = 2.0 * rho01.real();
    view.bloch_y = 2.0 * rho01.imag();
    view.bloch_z = view.prob_zero - view.prob_one;
    return view;
}

std::vector<QubitView> Simulator::qubit_views() const {
    std::vector<QubitView> views(static_cast<std::size_t>(num_qubits()));
    for (int q = 0; q < num_qubits(); ++q) {
        views[static_cast<std::size_t>(q)] = compute_qubit_view(state_, q);
    }
    return views;
}

std::vector<std::pair<int, double>> Simulator::nonzero_probabilities(double threshold) const {
    std::vector<std::pair<int, double>> result;
    const auto probs = state_.probabilities();
    for (std::size_t i = 0; i < probs.size(); ++i) {
        if (probs[i] >= threshold) {
            result.emplace_back(static_cast<int>(i), probs[i]);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    return result;
}

void Simulator::push_history(const Circuit& circuit) {
    undo_stack_.push_back(circuit_);
    circuit_ = circuit;
    if (undo_stack_.size() > kMaxUndo) {
        undo_stack_.erase(undo_stack_.begin());
    }
    redo_stack_.clear();
}

bool Simulator::undo(Circuit& circuit) {
    if (undo_stack_.empty()) return false;
    redo_stack_.push_back(circuit_);
    circuit_ = undo_stack_.back();
    undo_stack_.pop_back();
    circuit = circuit_;
    resync_from_circuit_();
    return true;
}

bool Simulator::redo(Circuit& circuit) {
    if (redo_stack_.empty()) return false;
    undo_stack_.push_back(circuit_);
    circuit_ = redo_stack_.back();
    redo_stack_.pop_back();
    circuit = circuit_;
    resync_from_circuit_();
    return true;
}

void Simulator::clear_history() {
    undo_stack_.clear();
    redo_stack_.clear();
}

}  // namespace qsim
