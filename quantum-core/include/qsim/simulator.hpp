#pragma once

#include "qsim/circuit.hpp"
#include "qsim/state_vector.hpp"
#include "qsim/types.hpp"

#include <optional>
#include <random>
#include <utility>
#include <vector>

namespace qsim {

struct QubitView {
    double prob_zero = 0.0;
    double prob_one = 0.0;
    double bloch_x = 0.0;
    double bloch_y = 0.0;
    double bloch_z = 1.0;
};

QubitView compute_qubit_view(const StateVector& state, int qubit);

class Simulator {
public:
    explicit Simulator(int num_qubits);

    int num_qubits() const { return state_.num_qubits(); }
    const StateVector& state() const { return state_; }
    StateVector& state() { return state_; }

    void reset();
    void load_circuit(const Circuit& circuit);

    void set_initial_state(InitialStatePreset preset);
    InitialStatePreset initial_state() const { return initial_preset_; }
    void set_collapse_on_measure(bool collapse);
    bool collapse_on_measure() const { return collapse_on_measure_; }

    /// Run all gates from current step to end (or full circuit on reset)
    void run_all();
    void step_forward();
    void step_back();

    int current_step() const { return step_; }
    int total_steps() const { return static_cast<int>(gate_sequence_.size()); }
    bool at_end() const { return step_ >= total_steps(); }

    std::vector<QubitView> qubit_views() const;
    std::vector<std::pair<int, double>> nonzero_probabilities(double threshold = 0.001) const;

    /// Column to highlight during step-through (next gate, or last gate when finished)
    std::optional<int> highlight_column() const;
    /// State snapshot saved before/after each step (0 = initial)
    const StateVector* checkpoint_state(int step) const;

    /// Sample measurement outcomes from the current state (circuit should be fully run)
    std::vector<std::pair<int, int>> sample_shots(int shots);

    /// History for undo during editing (not simulation steps)
    void push_history(const Circuit& circuit);
    bool undo(Circuit& circuit);
    bool redo(Circuit& circuit);
    void clear_history();

private:
    void apply_step(int index);
    void apply_initial_state();
    void save_checkpoint(int step);
    void restore_checkpoint(int step);
    void resync_from_circuit_();

    StateVector state_;
    Circuit circuit_;
    std::vector<CircuitOp> gate_sequence_;
    int step_ = 0;
    InitialStatePreset initial_preset_ = InitialStatePreset::AllZero;
    bool collapse_on_measure_ = false;
    mutable std::mt19937 rng_{std::random_device{}()};

    std::vector<StateVector> checkpoints_;
    std::vector<Circuit> undo_stack_;
    std::vector<Circuit> redo_stack_;
    static constexpr int kMaxUndo = 48;
};

}  // namespace qsim
