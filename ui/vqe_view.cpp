#include "vqe_view.hpp"
#include "app.hpp"
#include "util.hpp"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

constexpr ImU32 kWhite = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kBorderLight = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kBorderDark = IM_COL32(80, 80, 80, 255);
constexpr ImVec4 kRetroSelected{0.55f, 0.55f, 0.55f, 1.0f};
constexpr ImVec4 kRetroPanel{0.66f, 0.66f, 0.66f, 1.0f};

void draw_inset_rect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 fill = kWhite) {
    dl->AddRectFilled(a, b, fill);
    dl->AddLine(a, {b.x, a.y}, kBorderDark, 2.0f);
    dl->AddLine(a, {a.x, b.y}, kBorderDark, 2.0f);
    dl->AddLine({a.x, b.y}, b, kBorderLight, 2.0f);
    dl->AddLine({b.x, a.y}, b, kBorderLight, 2.0f);
}

bool begin_fieldset(const char* label) {
    ImGui::PushID(label);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
    ImGui::TextUnformatted(label);
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kRetroPanel);
    ImGui::BeginChild("##fieldset", ImVec2(-1, 0),
                      ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY |
                          ImGuiChildFlags_AlwaysUseWindowPadding);
    return true;
}

void end_fieldset() {
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::PopID();
}

bool retro_button(const char* label, bool enabled = true) {
    if (!enabled) ImGui::BeginDisabled();
    const bool clicked = ImGui::Button(label);
    if (!enabled) ImGui::EndDisabled();
    return clicked && enabled;
}

void draw_convergence_plot(const std::vector<qsim::StepRecord>& history,
                           double target_exact, float width, float height,
                           const char* title) {
    begin_fieldset(title);
    const ImVec2 start_pos = ImGui::GetCursorScreenPos();
    const ImVec2 end_pos = {start_pos.x + width, start_pos.y + height};
    ImDrawList* dl = ImGui::GetWindowDrawList();

    draw_inset_rect(dl, start_pos, end_pos, IM_COL32(18, 22, 26, 255));

    if (history.empty()) {
        dl->AddText({start_pos.x + 10.0f, start_pos.y + 10.0f}, kWhite, "No optimization data yet. Click [ Step > ] or [ Run ].");
        ImGui::Dummy(ImVec2(width, height));
        end_fieldset();
        return;
    }

    double min_val = target_exact;
    double max_val = target_exact;
    for (const auto& r : history) {
        min_val = std::min(min_val, r.cost);
        max_val = std::max(max_val, r.cost);
    }
    double span = max_val - min_val;
    if (span < 1e-4) span = 1.0;
    min_val -= span * 0.08;
    max_val += span * 0.08;
    span = max_val - min_val;

    const float pad_left = 54.0f;
    const float pad_right = 16.0f;
    const float pad_top = 16.0f;
    const float pad_bot = 20.0f;
    const float plot_w = width - pad_left - pad_right;
    const float plot_h = height - pad_top - pad_bot;

    // Grid lines
    for (int g = 0; g <= 4; ++g) {
        float gy = start_pos.y + pad_top + plot_h * (1.0f - static_cast<float>(g) / 4.0f);
        dl->AddLine({start_pos.x + pad_left, gy}, {start_pos.x + pad_left + plot_w, gy}, IM_COL32(45, 52, 60, 255), 1.0f);
        double val = min_val + (span * g / 4.0);
        char lbl[32];
        std::snprintf(lbl, sizeof(lbl), "%+.2f", val);
        dl->AddText({start_pos.x + 4.0f, gy - 7.0f}, IM_COL32(160, 170, 180, 255), lbl);
    }

    // Target baseline
    if (target_exact >= min_val && target_exact <= max_val) {
        float ty = start_pos.y + pad_top + plot_h * (1.0f - static_cast<float>((target_exact - min_val) / span));
        dl->AddLine({start_pos.x + pad_left, ty}, {start_pos.x + pad_left + plot_w, ty}, IM_COL32(0, 220, 120, 200), 1.5f);
        char t_lbl[48];
        std::snprintf(t_lbl, sizeof(t_lbl), "E_exact: %+.3f", target_exact);
        dl->AddText({start_pos.x + pad_left + 6.0f, ty - 14.0f}, IM_COL32(0, 220, 120, 255), t_lbl);
    }

    // Plot trajectory line
    const std::size_t n_pts = history.size();
    for (std::size_t i = 0; i < n_pts; ++i) {
        float frac_x = (n_pts == 1) ? 0.0f : static_cast<float>(i) / static_cast<float>(n_pts - 1);
        float px = start_pos.x + pad_left + frac_x * plot_w;
        float py = start_pos.y + pad_top + plot_h * (1.0f - static_cast<float>((history[i].cost - min_val) / span));

        if (i > 0) {
            float prev_frac_x = static_cast<float>(i - 1) / static_cast<float>(n_pts - 1);
            float prev_px = start_pos.x + pad_left + prev_frac_x * plot_w;
            float prev_py = start_pos.y + pad_top + plot_h * (1.0f - static_cast<float>((history[i - 1].cost - min_val) / span));
            dl->AddLine({prev_px, prev_py}, {px, py}, IM_COL32(255, 200, 50, 255), 2.0f);
        }
        dl->AddCircleFilled({px, py}, 2.5f, IM_COL32(255, 240, 120, 255));
    }

    // Current point marker
    if (!history.empty()) {
        float cur_x = start_pos.x + pad_left + plot_w;
        float cur_y = start_pos.y + pad_top + plot_h * (1.0f - static_cast<float>((history.back().cost - min_val) / span));
        dl->AddCircleFilled({cur_x, cur_y}, 5.0f, IM_COL32(255, 60, 60, 255));
        dl->AddCircle({cur_x, cur_y}, 7.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);
    }

    ImGui::Dummy(ImVec2(width, height));
    end_fieldset();
}

void draw_landscape_heatmap(const std::vector<std::vector<double>>& grid, float width, float height,
                            const std::vector<qsim::StepRecord>& history,
                            int px_idx, int py_idx,
                            double x_min, double x_max, double y_min, double y_max,
                            const char* x_label, const char* y_label, const char* title) {
    begin_fieldset(title);
    const ImVec2 start_pos = ImGui::GetCursorScreenPos();
    const ImVec2 end_pos = {start_pos.x + width, start_pos.y + height};
    ImDrawList* dl = ImGui::GetWindowDrawList();

    draw_inset_rect(dl, start_pos, end_pos, IM_COL32(10, 12, 16, 255));

    if (grid.empty() || grid[0].empty()) {
        dl->AddText({start_pos.x + 10.0f, start_pos.y + 10.0f}, kWhite, "Computing energy landscape...");
        ImGui::Dummy(ImVec2(width, height));
        end_fieldset();
        return;
    }

    const int res_y = static_cast<int>(grid.size());
    const int res_x = static_cast<int>(grid[0].size());

    double g_min = 1e9;
    double g_max = -1e9;
    for (int r = 0; r < res_y; ++r) {
        for (int c = 0; c < res_x; ++c) {
            g_min = std::min(g_min, grid[r][c]);
            g_max = std::max(g_max, grid[r][c]);
        }
    }
    double g_span = g_max - g_min;
    if (g_span < 1e-6) g_span = 1.0;

    const float pad_l = 36.0f;
    const float pad_r = 10.0f;
    const float pad_t = 12.0f;
    const float pad_b = 20.0f;
    const float map_w = width - pad_l - pad_r;
    const float map_h = height - pad_t - pad_b;
    const float cell_w = map_w / res_x;
    const float cell_h = map_h / res_y;

    for (int r = 0; r < res_y; ++r) {
        for (int c = 0; c < res_x; ++c) {
            float norm = static_cast<float>((grid[r][c] - g_min) / g_span);
            ImU32 col;
            if (norm < 0.25f) {
                float t = norm / 0.25f;
                col = IM_COL32(0, static_cast<int>(60 * t), static_cast<int>(120 + 100 * t), 255);
            } else if (norm < 0.5f) {
                float t = (norm - 0.25f) / 0.25f;
                col = IM_COL32(0, static_cast<int>(60 + 160 * t), static_cast<int>(220 - 80 * t), 255);
            } else if (norm < 0.75f) {
                float t = (norm - 0.5f) / 0.25f;
                col = IM_COL32(static_cast<int>(220 * t), 220, static_cast<int>(140 - 140 * t), 255);
            } else {
                float t = (norm - 0.75f) / 0.25f;
                col = IM_COL32(220, static_cast<int>(220 * (1.0f - t)), 0, 255);
            }

            float cx0 = start_pos.x + pad_l + c * cell_w;
            float cy0 = start_pos.y + pad_t + (res_y - 1 - r) * cell_h;
            dl->AddRectFilled({cx0, cy0}, {cx0 + cell_w + 0.5f, cy0 + cell_h + 0.5f}, col);
        }
    }

    // Overlay optimizer trajectory
    if (!history.empty()) {
        for (std::size_t i = 0; i < history.size(); ++i) {
            if (static_cast<std::size_t>(std::max(px_idx, py_idx)) >= history[i].params.size()) continue;
            double vx = history[i].params[px_idx];
            double vy = history[i].params[py_idx];
            float fx = static_cast<float>((vx - x_min) / (x_max - x_min));
            float fy = static_cast<float>((vy - y_min) / (y_max - y_min));
            fx = std::clamp(fx, 0.0f, 1.0f);
            fy = std::clamp(fy, 0.0f, 1.0f);

            float pt_x = start_pos.x + pad_l + fx * map_w;
            float pt_y = start_pos.y + pad_t + (1.0f - fy) * map_h;

            if (i > 0) {
                double prev_vx = history[i - 1].params[px_idx];
                double prev_vy = history[i - 1].params[py_idx];
                float pfx = static_cast<float>((prev_vx - x_min) / (x_max - x_min));
                float pfy = static_cast<float>((prev_vy - y_min) / (y_max - y_min));
                pfx = std::clamp(pfx, 0.0f, 1.0f);
                pfy = std::clamp(pfy, 0.0f, 1.0f);
                float prev_pt_x = start_pos.x + pad_l + pfx * map_w;
                float prev_pt_y = start_pos.y + pad_t + (1.0f - pfy) * map_h;
                dl->AddLine({prev_pt_x, prev_pt_y}, {pt_x, pt_y}, IM_COL32(255, 255, 255, 220), 1.5f);
            }
            dl->AddCircleFilled({pt_x, pt_y}, 3.0f, IM_COL32(255, 255, 0, 255));
        }

        const auto& head = history.back();
        if (static_cast<std::size_t>(std::max(px_idx, py_idx)) < head.params.size()) {
            float fx = static_cast<float>((head.params[px_idx] - x_min) / (x_max - x_min));
            float fy = static_cast<float>((head.params[py_idx] - y_min) / (y_max - y_min));
            float hx = start_pos.x + pad_l + std::clamp(fx, 0.0f, 1.0f) * map_w;
            float hy = start_pos.y + pad_t + (1.0f - std::clamp(fy, 0.0f, 1.0f)) * map_h;
            dl->AddCircleFilled({hx, hy}, 6.0f, IM_COL32(255, 40, 40, 255));
            dl->AddCircle({hx, hy}, 8.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);
        }
    }

    char x_buf[64], y_buf[64];
    std::snprintf(x_buf, sizeof(x_buf), "%s ->", x_label);
    std::snprintf(y_buf, sizeof(y_buf), "%s", y_label);
    dl->AddText({start_pos.x + pad_l + map_w * 0.4f, start_pos.y + pad_t + map_h + 2.0f}, kWhite, x_buf);
    dl->AddText({start_pos.x + 2.0f, start_pos.y + 2.0f}, kWhite, y_buf);

    ImGui::Dummy(ImVec2(width, height));
    end_fieldset();
}

}  // namespace

void AppState::init_vqe() {
    vqe_opt_config.kind = qsim::OptimizerKind::Adam;
    vqe_opt_config.learning_rate = 0.08;
    vqe_opt_config.max_iterations = 100;
    vqe_opt_config.use_parameter_shift = true;
    sync_vqe_hamiltonian();
}

void AppState::sync_vqe_hamiltonian() {
    qsim::Hamiltonian h(vqe_num_qubits);
    switch (vqe_h_preset) {
        case VQEHamiltonianPreset::XX_ZZ:
            vqe_num_qubits = 2;
            h = qsim::Hamiltonian::two_qubit_xx_zz();
            break;
        case VQEHamiltonianPreset::H2_Molecule:
            vqe_num_qubits = 2;
            h = qsim::Hamiltonian::h2_molecule(vqe_h2_distance);
            break;
        case VQEHamiltonianPreset::TransverseIsing:
            vqe_num_qubits = std::clamp(vqe_num_qubits, 2, 6);
            h = qsim::Hamiltonian::transverse_ising(vqe_num_qubits, vqe_ising_j, vqe_ising_g);
            break;
        case VQEHamiltonianPreset::HeisenbergXXZ:
            vqe_num_qubits = std::clamp(vqe_num_qubits, 2, 6);
            h = qsim::Hamiltonian::heisenberg_xxz(vqe_num_qubits, vqe_ising_j, 1.0);
            break;
        case VQEHamiltonianPreset::Custom:
            h = qsim::Hamiltonian::from_string(vqe_custom_h_buf.data(), vqe_num_qubits);
            break;
    }

    sync_vqe_ansatz();
    if (vqe_experiment) {
        vqe_experiment->set_hamiltonian(std::move(h));
        vqe_experiment->set_shots(vqe_shot_mode ? vqe_shots : 0);
    }
    reset_vqe_session();
}

void AppState::sync_vqe_ansatz() {
    std::shared_ptr<qsim::Ansatz> ansatz;
    switch (vqe_ansatz_preset) {
        case VQEAnsatzPreset::TwoQubitMinimal:
            ansatz = std::make_shared<qsim::TwoQubitRyCnotAnsatz>(true);
            break;
        case VQEAnsatzPreset::HardwareEfficient:
            ansatz = std::make_shared<qsim::HardwareEfficientAnsatz>(vqe_num_qubits, vqe_hea_layers);
            break;
        case VQEAnsatzPreset::ActiveCircuit:
            ansatz = std::make_shared<qsim::CustomCircuitAnsatz>(circuit);
            break;
    }

    if (!vqe_experiment) {
        qsim::Hamiltonian h = qsim::Hamiltonian::two_qubit_xx_zz();
        vqe_experiment = std::make_unique<qsim::VQEExperiment>(std::move(h), ansatz);
    } else {
        vqe_experiment->set_ansatz(ansatz);
    }
}

void AppState::reset_vqe_session() {
    if (!vqe_experiment) return;
    vqe_experiment->set_shots(vqe_shot_mode ? vqe_shots : 0);
    vqe_session = vqe_experiment->create_session(vqe_opt_config);
    vqe_auto_running = false;
    vqe_landscape_dirty = true;
    update_vqe_landscape();
}

void AppState::update_vqe_landscape() {
    if (!vqe_experiment || !vqe_session) return;
    const auto& params = vqe_session->current_params();
    vqe_landscape = vqe_experiment->compute_2d_landscape(vqe_landscape_px, vqe_landscape_py, 25,
                                                        0.0, 6.2831853, 0.0, 6.2831853, params);
    vqe_landscape_dirty = false;
}

void AppState::push_vqe_to_editor() {
    if (!vqe_experiment || !vqe_session) return;
    const auto& ansatz = vqe_experiment->ansatz();
    if (!ansatz) return;
    circuit = ansatz->build_circuit(vqe_session->best_params());
    simulator = qsim::Simulator(circuit.num_qubits);
    sync_simulator();
    current_tab = AppTab::CircuitEditor;
    status_message = "Loaded optimized VQE circuit into Circuit Editor.";
}

void draw_vqe_studio(AppState& app, float content_w, bool wide) {
    if (!app.vqe_experiment || !app.vqe_session) {
        app.init_vqe();
    }

    // Top Telemetry Banner
    {
        begin_fieldset("VQE Real-Time Optimization Telemetry");
        const double exact_e0 = app.vqe_experiment->hamiltonian().ground_state_energy_exact();
        const double current_e = app.vqe_session->current_cost();
        const double best_e = app.vqe_session->best_cost();
        const double error = std::abs(best_e - exact_e0);
        const int iter = app.vqe_session->current_iteration();

        ImGui::Columns(4, "##vqe_telemetry", false);
        ImGui::Text("Iteration: %d", iter);
        ImGui::Text("Status: %s", app.vqe_session->is_converged() ? "CONVERGED" : (app.vqe_auto_running ? "RUNNING" : "PAUSED"));
        ImGui::NextColumn();

        ImGui::Text("Current Energy: %+.5f", current_e);
        ImGui::Text("Best Energy:    %+.5f", best_e);
        ImGui::NextColumn();

        ImGui::Text("Theoretical E0: %+.5f", exact_e0);
        ImGui::PushStyleColor(ImGuiCol_Text, (error < 0.05) ? ImVec4(0, 0.6f, 0, 1) : ImVec4(0.8f, 0, 0, 1));
        ImGui::Text("Discrepancy:    %.5f Ha", error);
        ImGui::PopStyleColor();
        ImGui::NextColumn();

        ImGui::Text("Optimizer: %s", qsim::optimizer_name(app.vqe_opt_config.kind));
        ImGui::Text("Mode: %s", app.vqe_shot_mode ? "Shot-Noise" : "Exact State");
        ImGui::Columns(1);
        end_fieldset();
    }

    const float gap = 6.0f;
    const float col_w = wide ? (content_w - gap) * 0.5f : content_w;

    if (wide) {
        ImGui::Columns(2, "##vqe_main_columns", false);
        ImGui::SetColumnWidth(0, col_w);
    }

    // Left Column: Configuration & Controls
    {
        begin_fieldset("1. Target Hamiltonian & Molecular System");
        const char* h_names[] = {"XX + ZZ (2q, E0=-2.0)", "H2 Molecule (STO-3G)", "Transverse Ising", "Heisenberg XXZ", "Custom Pauli String"};
        int h_sel = static_cast<int>(app.vqe_h_preset);
        for (int i = 0; i < 5; ++i) {
            bool sel = (h_sel == i);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, kRetroSelected);
            if (ImGui::Button(h_names[i])) {
                app.vqe_h_preset = static_cast<VQEHamiltonianPreset>(i);
                app.sync_vqe_hamiltonian();
            }
            if (sel) ImGui::PopStyleColor();
            if (i == 1 || i == 3) ImGui::NewLine(); else ImGui::SameLine(0, 4);
        }
        ImGui::NewLine();

        if (app.vqe_h_preset == VQEHamiltonianPreset::H2_Molecule) {
            float d = static_cast<float>(app.vqe_h2_distance);
            if (ImGui::SliderFloat("Bond Distance R (Å)", &d, 0.2f, 2.5f, "%.4f Å")) {
                app.vqe_h2_distance = d;
                app.sync_vqe_hamiltonian();
            }
        } else if (app.vqe_h_preset == VQEHamiltonianPreset::TransverseIsing || app.vqe_h_preset == VQEHamiltonianPreset::HeisenbergXXZ) {
            int qn = app.vqe_num_qubits;
            if (ImGui::SliderInt("Qubit Register Size", &qn, 2, 6)) {
                app.vqe_num_qubits = qn;
                app.sync_vqe_hamiltonian();
            }
            float j_val = static_cast<float>(app.vqe_ising_j);
            if (ImGui::SliderFloat("Coupling J", &j_val, 0.1f, 3.0f)) {
                app.vqe_ising_j = j_val;
                app.sync_vqe_hamiltonian();
            }
        } else if (app.vqe_h_preset == VQEHamiltonianPreset::Custom) {
            if (ImGui::InputText("Pauli Input", app.vqe_custom_h_buf.data(), app.vqe_custom_h_buf.size())) {
                app.sync_vqe_hamiltonian();
            }
        }
        ImGui::TextWrapped("Active: %s", app.vqe_experiment->hamiltonian().to_string().c_str());
        end_fieldset();

        begin_fieldset("2. Variational Quantum Circuit (Ansatz)");
        const char* a_names[] = {"Minimal 2-Qubit Ry-CNOT", "Hardware-Efficient (HEA)", "Active Editor Circuit"};
        int a_sel = static_cast<int>(app.vqe_ansatz_preset);
        for (int i = 0; i < 3; ++i) {
            bool sel = (a_sel == i);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, kRetroSelected);
            if (ImGui::Button(a_names[i])) {
                app.vqe_ansatz_preset = static_cast<VQEAnsatzPreset>(i);
                app.sync_vqe_ansatz();
                app.reset_vqe_session();
            }
            if (sel) ImGui::PopStyleColor();
            ImGui::SameLine(0, 4);
        }
        ImGui::NewLine();
        if (app.vqe_ansatz_preset == VQEAnsatzPreset::HardwareEfficient) {
            if (ImGui::SliderInt("HEA Layers (L)", &app.vqe_hea_layers, 1, 4)) {
                app.sync_vqe_ansatz();
                app.reset_vqe_session();
            }
        }
        ImGui::Text("Parameters: %d  |  Qubits: %d", app.vqe_session->current_params().size(), app.vqe_num_qubits);
        end_fieldset();

        begin_fieldset("3. Classical Optimizer & Execution");
        const char* opt_names[] = {"Adam", "Gradient Descent", "COBYLA / Simplex", "SPSA"};
        int opt_sel = static_cast<int>(app.vqe_opt_config.kind);
        for (int i = 0; i < 4; ++i) {
            bool sel = (opt_sel == i);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, kRetroSelected);
            if (ImGui::Button(opt_names[i])) {
                app.vqe_opt_config.kind = static_cast<qsim::OptimizerKind>(i);
                app.vqe_session->set_config(app.vqe_opt_config);
            }
            if (sel) ImGui::PopStyleColor();
            ImGui::SameLine(0, 4);
        }
        ImGui::NewLine();

        float lr = static_cast<float>(app.vqe_opt_config.learning_rate);
        if (ImGui::SliderFloat("Learning Rate", &lr, 0.005f, 0.3f, "%.3f")) {
            app.vqe_opt_config.learning_rate = lr;
            app.vqe_session->set_config(app.vqe_opt_config);
        }
        if (ImGui::Checkbox("Shot-Noise Simulation", &app.vqe_shot_mode)) {
            app.vqe_experiment->set_shots(app.vqe_shot_mode ? app.vqe_shots : 0);
        }
        if (app.vqe_shot_mode) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::SliderInt("Shots", &app.vqe_shots, 256, 8192)) {
                app.vqe_experiment->set_shots(app.vqe_shots);
            }
        }

        ImGui::Separator();
        if (retro_button("Step >")) {
            app.vqe_session->step();
        }
        ImGui::SameLine(0, 4);
        if (retro_button(app.vqe_auto_running ? "Pause" : "Run / Animate")) {
            app.vqe_auto_running = !app.vqe_auto_running;
        }
        ImGui::SameLine(0, 4);
        if (retro_button("Reset")) {
            app.reset_vqe_session();
        }
        ImGui::SameLine(0, 4);
        if (retro_button("Push to Circuit Editor")) {
            app.push_vqe_to_editor();
        }
        end_fieldset();
    }

    if (wide) {
        ImGui::NextColumn();
    }

    // Right Column: Plots, 2D Landscapes, and Iteration Table
    {
        const float plot_w = wide ? (col_w - 12.0f) : (content_w - 12.0f);
        const double exact_e0 = app.vqe_experiment->hamiltonian().ground_state_energy_exact();

        draw_convergence_plot(app.vqe_session->history(), exact_e0, plot_w, 180.0f,
                              "VQE Energy Convergence Curve <H(theta)>");

        // 2D Energy Landscape Heatmap
        if (app.vqe_session->current_params().size() >= 2) {
            draw_landscape_heatmap(app.vqe_landscape, plot_w, 180.0f,
                                  app.vqe_session->history(),
                                  app.vqe_landscape_px, app.vqe_landscape_py,
                                  0.0, 6.2831853, 0.0, 6.2831853,
                                  "theta1 (rad)", "theta2 (rad)",
                                  "2D Energy Landscape E(theta1, theta2) & Optimizer Path");
        }

        // Iteration Table
        begin_fieldset("Optimization Telemetry Log (Step -> Energy)");
        const auto& hist = app.vqe_session->history();
        ImGui::BeginChild("##vqe_hist_scroll", ImVec2(-1, 140.0f), true);
        if (ImGui::BeginTable("##vqe_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Iter", ImGuiTableColumnFlags_WidthFixed, 36.0f);
            ImGui::TableSetupColumn("Energy <H>");
            ImGui::TableSetupColumn("Delta E");
            ImGui::TableSetupColumn("Gradient Norm");
            ImGui::TableHeadersRow();

            for (std::size_t i = 0; i < hist.size(); ++i) {
                const auto& rec = hist[hist.size() - 1 - i]; // newest top
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", rec.iteration);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%+.5f", rec.cost);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%+.5f", rec.delta_cost);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.5f", rec.grad_norm);
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        end_fieldset();
    }

    if (wide) {
        ImGui::Columns(1);
    }
}
