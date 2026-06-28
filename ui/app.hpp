#pragma once

#include "qsim/circuit.hpp"
#include "qsim/gates.hpp"
#include "qsim/json_io.hpp"
#include "qsim/simulator.hpp"
#include "util.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <filesystem>
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

struct AppState {
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
};

void app_init(AppState& app, const std::string& presets_dir);
void app_shutdown(AppState& app, int window_w, int window_h);
void app_frame(AppState& app, struct GLFWwindow* window);
int run_app(int argc, char** argv);
