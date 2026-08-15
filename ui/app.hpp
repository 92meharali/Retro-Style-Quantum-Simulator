#pragma once

#include "qsim/ansatz.hpp"
#include "qsim/circuit.hpp"
#include "qsim/gates.hpp"
#include "qsim/json_io.hpp"
#include "qsim/optimizer.hpp"
#include "qsim/pauli.hpp"
#include "qsim/qaoa.hpp"
#include "qsim/simulator.hpp"
#include "qsim/vqe.hpp"
#include "util.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct PresetEntry {
    std::string path;
    std::string name;
    std::string description;
    std::string category;
    bool user_preset = false;
};

enum class ImportDialog { None, Json, Qasm };

enum class AppTab {
    CircuitEditor,
    VQEStudio,
    QAOAStudio,
};

enum class VQEHamiltonianPreset {
    XX_ZZ,
    H2_Molecule,
    TransverseIsing,
    HeisenbergXXZ,
    Custom,
};

enum class VQEAnsatzPreset {
    TwoQubitMinimal,
    HardwareEfficient,
    ActiveCircuit,
};

enum class QAOAGraphPreset {
    Triangle,
    Cycle4,
    Bowtie5,
    Regular6,
    Custom,
};

struct AppState {
    AppTab current_tab = AppTab::CircuitEditor;

    qsim::Circuit circuit;
    qsim::Simulator simulator{2};
    qsim::GateKind selected_gate = qsim::GateKind::H;
    bool eraser_mode = false;
    double rotation_param = 1.5707963267948966;

    bool filter_probs = false;
    bool auto_run_on_edit = true;
    bool collapse_on_measure = false;
    qsim::InitialStatePreset initial_state = qsim::InitialStatePreset::AllZero;
    int shot_count = 1024;
    int min_time_columns = 8;

    std::vector<std::pair<int, int>> shot_histogram;

    std::optional<int> pending_qubit;
    std::optional<int> pending_qubit2;
    std::optional<int> pending_column;
    qsim::GateKind pending_gate = qsim::GateKind::CNOT;

    int next_op_id = 1;
    std::string status_message;
    std::string circuit_description;
    std::string current_file_path;
    std::string presets_dir;

    AppSettings settings;

    bool show_about = false;
    bool show_preset_browser = false;
    bool show_save_preset = false;
    ImportDialog import_dialog = ImportDialog::None;

    std::array<char, 512> import_path_buf{};
    std::array<char, 128> save_preset_name_buf{};
    std::array<char, 256> save_preset_desc_buf{};

    // VQE Studio State
    VQEHamiltonianPreset vqe_h_preset = VQEHamiltonianPreset::XX_ZZ;
    VQEAnsatzPreset vqe_ansatz_preset = VQEAnsatzPreset::TwoQubitMinimal;
    int vqe_num_qubits = 2;
    double vqe_h2_distance = 0.7414;
    double vqe_ising_j = 1.0;
    double vqe_ising_g = 0.5;
    std::array<char, 256> vqe_custom_h_buf{"1.0*X0*X1 + 1.0*Z0*Z1"};
    int vqe_hea_layers = 1;
    bool vqe_shot_mode = false;
    int vqe_shots = 1024;
    qsim::OptimizerConfig vqe_opt_config;
    std::unique_ptr<qsim::VQEExperiment> vqe_experiment;
    std::unique_ptr<qsim::OptimizerSession> vqe_session;
    bool vqe_auto_running = false;
    int vqe_steps_per_frame = 1;
    std::vector<std::vector<double>> vqe_landscape;
    bool vqe_landscape_dirty = true;
    int vqe_landscape_px = 0;
    int vqe_landscape_py = 1;

    // QAOA MaxCut Studio State
    QAOAGraphPreset qaoa_graph_preset = QAOAGraphPreset::Cycle4;
    qsim::MaxCutGraph qaoa_graph{4};
    int qaoa_layers = 1;
    bool qaoa_shot_mode = false;
    int qaoa_shots = 1024;
    qsim::OptimizerConfig qaoa_opt_config;
    std::unique_ptr<qsim::QAOAExperiment> qaoa_experiment;
    std::unique_ptr<qsim::OptimizerSession> qaoa_session;
    bool qaoa_auto_running = false;
    int qaoa_steps_per_frame = 1;
    std::vector<std::vector<double>> qaoa_landscape;
    bool qaoa_landscape_dirty = true;
    int qaoa_new_edge_u = 0;
    int qaoa_new_edge_v = 1;

    void sync_simulator();
    void reset_simulation();
    int place_gate(int qubit, int column);
    void erase_at(int qubit, int column);
    bool try_load_circuit_file(const std::string& path);
    bool try_import_qasm(const std::string& path);
    void undo();
    void redo();
    void set_num_qubits(int n);
    void export_json(const std::string& path);
    void export_qasm(const std::string& path);
    bool export_circuit_png(const std::string& path);
    void copy_qasm_to_clipboard(struct GLFWwindow* window);
    void save_user_preset(const std::string& name, const std::string& description);
    std::vector<PresetEntry> list_all_presets() const;
    int time_columns() const;
    std::string pending_message() const;
    void apply_settings();
    void persist_settings(int window_w, int window_h);
    void run_shots();
    void apply_initial_state();

    // VQE Studio Methods
    void init_vqe();
    void sync_vqe_hamiltonian();
    void sync_vqe_ansatz();
    void reset_vqe_session();
    void push_vqe_to_editor();
    void update_vqe_landscape();

    // QAOA Studio Methods
    void init_qaoa();
    void sync_qaoa_graph();
    void reset_qaoa_session();
    void push_qaoa_to_editor();
    void update_qaoa_landscape();
};

void app_init(AppState& app, const std::string& presets_dir);
void app_shutdown(AppState& app, int window_w, int window_h);
void app_frame(AppState& app, struct GLFWwindow* window);
int run_app(int argc, char** argv);
