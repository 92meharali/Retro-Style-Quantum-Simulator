#include "qaoa_view.hpp"
#include "app.hpp"
#include "util.hpp"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

constexpr ImU32 kWhite = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kBlack = IM_COL32(0, 0, 0, 255);
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

void draw_qaoa_graph_canvas(const qsim::MaxCutGraph& graph, int best_bitstring, float width, float height) {
    const ImVec2 start_pos = ImGui::GetCursorScreenPos();
    const ImVec2 end_pos = {start_pos.x + width, start_pos.y + height};
    ImDrawList* dl = ImGui::GetWindowDrawList();

    draw_inset_rect(dl, start_pos, end_pos, IM_COL32(18, 22, 26, 255));

    const int n = graph.num_nodes();
    const auto& edges = graph.edges();
    const auto& positions = graph.node_positions();

    if (n <= 0 || positions.size() < static_cast<std::size_t>(n)) {
        ImGui::Dummy(ImVec2(width, height));
        return;
    }

    const float cx = start_pos.x + width * 0.5f;
    const float cy = start_pos.y + height * 0.5f;
    const float radius = std::min(width, height) * 0.38f;

    std::vector<ImVec2> screen_pos(n);
    for (int i = 0; i < n; ++i) {
        double angle = 2.0 * 3.1415926535 * i / n - 3.1415926535 / 2.0;
        screen_pos[i] = {
            cx + static_cast<float>(radius * std::cos(angle)),
            cy + static_cast<float>(radius * std::sin(angle))
        };
    }

    // Draw edges
    for (const auto& e : edges) {
        if (e.u >= n || e.v >= n) continue;
        int u_val = qsim::qubit_value(best_bitstring, n, e.u);
        int v_val = qsim::qubit_value(best_bitstring, n, e.v);
        bool is_cut = (u_val != v_val);

        if (is_cut) {
            // Neon Green cut edge
            dl->AddLine(screen_pos[e.u], screen_pos[e.v], IM_COL32(0, 240, 60, 255), 3.0f);
        } else {
            // Subdued dark gray uncut edge
            dl->AddLine(screen_pos[e.u], screen_pos[e.v], IM_COL32(75, 85, 95, 200), 1.5f);
        }
    }

    // Draw nodes
    for (int i = 0; i < n; ++i) {
        int val = qsim::qubit_value(best_bitstring, n, i);
        ImU32 fill = (val == 0) ? IM_COL32(40, 110, 220, 255) : IM_COL32(230, 130, 20, 255);
        dl->AddCircleFilled(screen_pos[i], 14.0f, fill);
        dl->AddCircle(screen_pos[i], 14.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);

        char lbl[8];
        std::snprintf(lbl, sizeof(lbl), "q%d:%d", i, val);
        const ImVec2 sz = ImGui::CalcTextSize(lbl);
        dl->AddText({screen_pos[i].x - sz.x * 0.5f, screen_pos[i].y - sz.y * 0.5f}, kWhite, lbl);
    }

    // Legend
    char leg[64];
    double cut_val = graph.evaluate_cut(best_bitstring);
    std::snprintf(leg, sizeof(leg), "Partition: %s | Cut: %.0f", qsim::basis_label(best_bitstring, n).c_str(), cut_val);
    dl->AddText({start_pos.x + 8.0f, start_pos.y + 6.0f}, IM_COL32(0, 240, 60, 255), leg);

    ImGui::Dummy(ImVec2(width, height));
}

void draw_qaoa_convergence_plot(const std::vector<qsim::StepRecord>& history,
                                double max_cut, float width, float height,
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

    double min_val = 0.0;
    double max_val = max_cut * 1.05;
    double span = max_val - min_val;
    if (span < 1e-4) span = 1.0;

    const float pad_left = 48.0f;
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
        std::snprintf(lbl, sizeof(lbl), "%.1f", val);
        dl->AddText({start_pos.x + 4.0f, gy - 7.0f}, IM_COL32(160, 170, 180, 255), lbl);
    }

    // Target baseline MaxCut
    float ty = start_pos.y + pad_top + plot_h * (1.0f - static_cast<float>((max_cut - min_val) / span));
    dl->AddLine({start_pos.x + pad_left, ty}, {start_pos.x + pad_left + plot_w, ty}, IM_COL32(0, 220, 120, 200), 1.5f);
    char t_lbl[48];
    std::snprintf(t_lbl, sizeof(t_lbl), "MaxCut: %.1f", max_cut);
    dl->AddText({start_pos.x + pad_left + 6.0f, ty - 14.0f}, IM_COL32(0, 220, 120, 255), t_lbl);

    // Plot trajectory line (Cost is -<C>, so expected cut is -cost)
    const std::size_t n_pts = history.size();
    for (std::size_t i = 0; i < n_pts; ++i) {
        double exp_cut = -history[i].cost;
        float frac_x = (n_pts == 1) ? 0.0f : static_cast<float>(i) / static_cast<float>(n_pts - 1);
        float px = start_pos.x + pad_left + frac_x * plot_w;
        float py = start_pos.y + pad_top + plot_h * (1.0f - static_cast<float>((exp_cut - min_val) / span));

        if (i > 0) {
            double prev_cut = -history[i - 1].cost;
            float prev_frac_x = static_cast<float>(i - 1) / static_cast<float>(n_pts - 1);
            float prev_px = start_pos.x + pad_left + prev_frac_x * plot_w;
            float prev_py = start_pos.y + pad_top + plot_h * (1.0f - static_cast<float>((prev_cut - min_val) / span));
            dl->AddLine({prev_px, prev_py}, {px, py}, IM_COL32(0, 240, 80, 255), 2.0f);
        }
        dl->AddCircleFilled({px, py}, 2.5f, IM_COL32(180, 255, 120, 255));
    }

    ImGui::Dummy(ImVec2(width, height));
    end_fieldset();
}

void draw_qaoa_landscape_heatmap(const std::vector<std::vector<double>>& grid, float width, float height,
                                 const std::vector<qsim::StepRecord>& history,
                                 const char* title) {
    begin_fieldset(title);
    const ImVec2 start_pos = ImGui::GetCursorScreenPos();
    const ImVec2 end_pos = {start_pos.x + width, start_pos.y + height};
    ImDrawList* dl = ImGui::GetWindowDrawList();

    draw_inset_rect(dl, start_pos, end_pos, IM_COL32(10, 12, 16, 255));

    if (grid.empty() || grid[0].empty()) {
        dl->AddText({start_pos.x + 10.0f, start_pos.y + 10.0f}, kWhite, "Computing QAOA landscape...");
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
            // High cut (green) to low cut (dark blue)
            ImU32 col;
            if (norm < 0.33f) {
                float t = norm / 0.33f;
                col = IM_COL32(0, static_cast<int>(50 * t), static_cast<int>(120 + 80 * t), 255);
            } else if (norm < 0.66f) {
                float t = (norm - 0.33f) / 0.33f;
                col = IM_COL32(static_cast<int>(50 + 150 * t), static_cast<int>(180 + 40 * t), static_cast<int>(200 - 150 * t), 255);
            } else {
                float t = (norm - 0.66f) / 0.34f;
                col = IM_COL32(static_cast<int>(200 - 200 * t), 240, static_cast<int>(50 - 50 * t), 255);
            }

            float cx0 = start_pos.x + pad_l + c * cell_w;
            float cy0 = start_pos.y + pad_t + (res_y - 1 - r) * cell_h;
            dl->AddRectFilled({cx0, cy0}, {cx0 + cell_w + 0.5f, cy0 + cell_h + 0.5f}, col);
        }
    }

    // Overlay optimizer trajectory (gamma on x, beta on y)
    if (!history.empty()) {
        for (std::size_t i = 0; i < history.size(); ++i) {
            if (history[i].params.size() < 2) continue;
            double vx = history[i].params[0];
            double vy = history[i].params[1];
            float fx = static_cast<float>(vx / 3.14159265);
            float fy = static_cast<float>(vy / 1.57079632);
            fx = std::clamp(fx, 0.0f, 1.0f);
            fy = std::clamp(fy, 0.0f, 1.0f);

            float pt_x = start_pos.x + pad_l + fx * map_w;
            float pt_y = start_pos.y + pad_t + (1.0f - fy) * map_h;

            if (i > 0) {
                double prev_vx = history[i - 1].params[0];
                double prev_vy = history[i - 1].params[1];
                float pfx = static_cast<float>(prev_vx / 3.14159265);
                float pfy = static_cast<float>(prev_vy / 1.57079632);
                pfx = std::clamp(pfx, 0.0f, 1.0f);
                pfy = std::clamp(pfy, 0.0f, 1.0f);
                float prev_pt_x = start_pos.x + pad_l + pfx * map_w;
                float prev_pt_y = start_pos.y + pad_t + (1.0f - pfy) * map_h;
                dl->AddLine({prev_pt_x, prev_pt_y}, {pt_x, pt_y}, IM_COL32(255, 255, 255, 220), 1.5f);
            }
            dl->AddCircleFilled({pt_x, pt_y}, 3.0f, IM_COL32(255, 255, 0, 255));
        }

        const auto& head = history.back();
        if (head.params.size() >= 2) {
            float fx = static_cast<float>(head.params[0] / 3.14159265);
            float fy = static_cast<float>(head.params[1] / 1.57079632);
            float hx = start_pos.x + pad_l + std::clamp(fx, 0.0f, 1.0f) * map_w;
            float hy = start_pos.y + pad_t + (1.0f - std::clamp(fy, 0.0f, 1.0f)) * map_h;
            dl->AddCircleFilled({hx, hy}, 6.0f, IM_COL32(255, 40, 40, 255));
            dl->AddCircle({hx, hy}, 8.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);
        }
    }

    dl->AddText({start_pos.x + pad_l + map_w * 0.4f, start_pos.y + pad_t + map_h + 2.0f}, kWhite, "gamma (0->pi)");
    dl->AddText({start_pos.x + 2.0f, start_pos.y + 2.0f}, kWhite, "beta");

    ImGui::Dummy(ImVec2(width, height));
    end_fieldset();
}

}  // namespace

void AppState::init_qaoa() {
    qaoa_opt_config.kind = qsim::OptimizerKind::Adam;
    qaoa_opt_config.learning_rate = 0.08;
    qaoa_opt_config.max_iterations = 80;
    qaoa_opt_config.use_parameter_shift = true;
    sync_qaoa_graph();
}

void AppState::sync_qaoa_graph() {
    switch (qaoa_graph_preset) {
        case QAOAGraphPreset::Triangle:
            qaoa_graph = qsim::MaxCutGraph::triangle();
            break;
        case QAOAGraphPreset::Cycle4:
            qaoa_graph = qsim::MaxCutGraph::cycle4();
            break;
        case QAOAGraphPreset::Bowtie5:
            qaoa_graph = qsim::MaxCutGraph::bowtie5();
            break;
        case QAOAGraphPreset::Regular6:
            qaoa_graph = qsim::MaxCutGraph::regular6();
            break;
        case QAOAGraphPreset::Custom:
            break;
    }

    qaoa_experiment = std::make_unique<qsim::QAOAExperiment>(qaoa_graph, qaoa_layers);
    qaoa_experiment->set_shots(qaoa_shot_mode ? qaoa_shots : 0);
    reset_qaoa_session();
}

void AppState::reset_qaoa_session() {
    if (!qaoa_experiment) return;
    qaoa_experiment->set_shots(qaoa_shot_mode ? qaoa_shots : 0);
    qaoa_session = qaoa_experiment->create_session(qaoa_opt_config);
    qaoa_auto_running = false;
    qaoa_landscape_dirty = true;
    update_qaoa_landscape();
}

void AppState::update_qaoa_landscape() {
    if (!qaoa_experiment || !qaoa_session) return;
    qaoa_landscape = qaoa_experiment->compute_2d_landscape(25, 3.14159265, 1.57079632);
    qaoa_landscape_dirty = false;
}

void AppState::push_qaoa_to_editor() {
    if (!qaoa_experiment || !qaoa_session) return;
    circuit = qaoa_experiment->build_current_circuit(qaoa_session->best_params());
    simulator = qsim::Simulator(circuit.num_qubits);
    sync_simulator();
    current_tab = AppTab::CircuitEditor;
    status_message = "Loaded QAOA MaxCut circuit into Circuit Editor.";
}

void draw_qaoa_studio(AppState& app, float content_w, bool wide) {
    if (!app.qaoa_experiment || !app.qaoa_session) {
        app.init_qaoa();
    }

    // Top Telemetry Banner
    {
        begin_fieldset("QAOA MaxCut Optimization Telemetry");
        const double max_cut = app.qaoa_graph.max_cut_exact();
        const double exp_cut = -app.qaoa_session->current_cost();
        const double approx_ratio = (max_cut > 0) ? std::clamp(exp_cut / max_cut, 0.0, 1.0) : 1.0;
        const int iter = app.qaoa_session->current_iteration();

        ImGui::Columns(4, "##qaoa_telemetry", false);
        ImGui::Text("Iteration: %d", iter);
        ImGui::Text("Status: %s", app.qaoa_session->is_converged() ? "CONVERGED" : (app.qaoa_auto_running ? "RUNNING" : "PAUSED"));
        ImGui::NextColumn();

        ImGui::Text("Expected Cut <C>: %.3f", exp_cut);
        ImGui::Text("Max Possible Cut: %.0f", max_cut);
        ImGui::NextColumn();

        ImGui::PushStyleColor(ImGuiCol_Text, (approx_ratio >= 0.85) ? ImVec4(0, 0.6f, 0, 1) : ImVec4(0.8f, 0.4f, 0, 1));
        ImGui::Text("Approximation Ratio α: %.1f%%", approx_ratio * 100.0);
        ImGui::PopStyleColor();
        ImGui::Text("QAOA Layers (p): %d", app.qaoa_layers);
        ImGui::NextColumn();

        ImGui::Text("Optimizer: %s", qsim::optimizer_name(app.qaoa_opt_config.kind));
        ImGui::Text("Mode: %s", app.qaoa_shot_mode ? "Shot-Noise" : "Exact State");
        ImGui::Columns(1);
        end_fieldset();
    }

    const float gap = 6.0f;
    const float col_w = wide ? (content_w - gap) * 0.5f : content_w;

    if (wide) {
        ImGui::Columns(2, "##qaoa_main_columns", false);
        ImGui::SetColumnWidth(0, col_w);
    }

    // Left Column: Graph Configuration, Visualizer, and Execution
    {
        begin_fieldset("1. MaxCut Graph Problem");
        const char* g_names[] = {"Triangle (3q)", "4-Cycle (4q)", "Bowtie (5q)", "3-Regular (6q)", "Custom"};
        int g_sel = static_cast<int>(app.qaoa_graph_preset);
        for (int i = 0; i < 5; ++i) {
            bool sel = (g_sel == i);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, kRetroSelected);
            if (ImGui::Button(g_names[i])) {
                app.qaoa_graph_preset = static_cast<QAOAGraphPreset>(i);
                app.sync_qaoa_graph();
            }
            if (sel) ImGui::PopStyleColor();
            ImGui::SameLine(0, 4);
        }
        ImGui::NewLine();

        if (app.qaoa_graph_preset == QAOAGraphPreset::Custom) {
            int n = app.qaoa_graph.num_nodes();
            if (ImGui::SliderInt("Vertices", &n, 2, 8)) {
                app.qaoa_graph.set_num_nodes(n);
                app.sync_qaoa_graph();
            }
            ImGui::SetNextItemWidth(60.0f);
            ImGui::InputInt("u", &app.qaoa_new_edge_u);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::InputInt("v", &app.qaoa_new_edge_v);
            ImGui::SameLine();
            if (retro_button("Add Edge")) {
                app.qaoa_graph.add_edge(app.qaoa_new_edge_u, app.qaoa_new_edge_v);
                app.sync_qaoa_graph();
            }
            ImGui::SameLine();
            if (retro_button("Remove")) {
                app.qaoa_graph.remove_edge(app.qaoa_new_edge_u, app.qaoa_new_edge_v);
                app.sync_qaoa_graph();
            }
        }

        // Get top sampled bitstring for interactive graph partition coloring
        const auto solutions = app.qaoa_experiment->rank_solutions(app.qaoa_session->best_params(), 1);
        int best_bit = solutions.empty() ? 0 : solutions[0].bitstring;

        draw_qaoa_graph_canvas(app.qaoa_graph, best_bit, col_w - 12.0f, 180.0f);
        end_fieldset();

        begin_fieldset("2. QAOA Circuit & Optimizer");
        if (ImGui::SliderInt("Circuit Depth (Layers p)", &app.qaoa_layers, 1, 4)) {
            app.qaoa_experiment->set_layers(app.qaoa_layers);
            app.reset_qaoa_session();
        }

        const char* opt_names[] = {"Adam", "Gradient Descent", "COBYLA / Simplex", "SPSA"};
        int opt_sel = static_cast<int>(app.qaoa_opt_config.kind);
        for (int i = 0; i < 4; ++i) {
            bool sel = (opt_sel == i);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, kRetroSelected);
            if (ImGui::Button(opt_names[i])) {
                app.qaoa_opt_config.kind = static_cast<qsim::OptimizerKind>(i);
                app.qaoa_session->set_config(app.qaoa_opt_config);
            }
            if (sel) ImGui::PopStyleColor();
            ImGui::SameLine(0, 4);
        }
        ImGui::NewLine();

        float lr = static_cast<float>(app.qaoa_opt_config.learning_rate);
        if (ImGui::SliderFloat("Learning Rate", &lr, 0.005f, 0.3f, "%.3f")) {
            app.qaoa_opt_config.learning_rate = lr;
            app.qaoa_session->set_config(app.qaoa_opt_config);
        }
        if (ImGui::Checkbox("Shot-Noise Mode", &app.qaoa_shot_mode)) {
            app.qaoa_experiment->set_shots(app.qaoa_shot_mode ? app.qaoa_shots : 0);
        }

        ImGui::Separator();
        if (retro_button("Step >")) {
            app.qaoa_session->step();
        }
        ImGui::SameLine(0, 4);
        if (retro_button(app.qaoa_auto_running ? "Pause" : "Run / Animate")) {
            app.qaoa_auto_running = !app.qaoa_auto_running;
        }
        ImGui::SameLine(0, 4);
        if (retro_button("Reset")) {
            app.reset_qaoa_session();
        }
        ImGui::SameLine(0, 4);
        if (retro_button("Push to Circuit Editor")) {
            app.push_qaoa_to_editor();
        }
        end_fieldset();
    }

    if (wide) {
        ImGui::NextColumn();
    }

    // Right Column: Convergence Plot, 2D Landscape, and Solution Ranking
    {
        const float plot_w = wide ? (col_w - 12.0f) : (content_w - 12.0f);
        const double max_cut = app.qaoa_graph.max_cut_exact();

        draw_qaoa_convergence_plot(app.qaoa_session->history(), max_cut, plot_w, 180.0f,
                                   "Expected Cut Convergence <C(gamma, beta)>");

        // 2D Landscape Heatmap
        draw_qaoa_landscape_heatmap(app.qaoa_landscape, plot_w, 180.0f,
                                   app.qaoa_session->history(),
                                   "QAOA Energy Landscape C(gamma, beta) & Trajectory");

        // Sampled Solution Ranking Table
        begin_fieldset("Sampled Solutions (Optimal Partition Ranking)");
        const auto solutions = app.qaoa_experiment->rank_solutions(app.qaoa_session->best_params(), 6);

        ImGui::BeginChild("##qaoa_sol_scroll", ImVec2(-1, 140.0f), true);
        if (ImGui::BeginTable("##qaoa_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Partition");
            ImGui::TableSetupColumn("Probability");
            ImGui::TableSetupColumn("Cut Value");
            ImGui::TableSetupColumn("Quality");
            ImGui::TableHeadersRow();

            for (const auto& s : solutions) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", qsim::basis_label(s.bitstring, app.qaoa_graph.num_nodes()).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f%%", s.probability * 100.0);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.0f", s.cut_value);
                ImGui::TableSetColumnIndex(3);
                if (s.is_optimal) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0.7f, 0, 1));
                    ImGui::Text("OPTIMAL MAXCUT");
                    ImGui::PopStyleColor();
                } else {
                    ImGui::TextDisabled("Sub-optimal");
                }
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
