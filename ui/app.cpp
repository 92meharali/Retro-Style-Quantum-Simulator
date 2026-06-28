#include "app.hpp"
#include "util.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include "qsim/qasm_io.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <unordered_set>

namespace {

constexpr float kMinContentW = 320.0f;
constexpr float kMaxContentW = 1280.0f;
constexpr float kCellW = 58.0f;
constexpr float kCellH = 36.0f;
constexpr float kLabelW = 36.0f;
constexpr float kHeaderH = 28.0f;
constexpr double kProbFilterThreshold = 0.001;

struct LayoutMetrics {
    float full_w = 0.0f;
    float content_w = 0.0f;
    float pad = 6.0f;
    float scroll_h = 0.0f;
    bool wide = false;
};

LayoutMetrics compute_layout(const ImGuiViewport* vp) {
    LayoutMetrics m;
    m.full_w = vp->WorkSize.x;
    const float scrollbar = ImGui::GetStyle().ScrollbarSize;
    const float avail = m.full_w - m.pad * 2.0f - scrollbar;
    m.content_w = std::clamp(avail, kMinContentW, kMaxContentW);
    m.wide = m.content_w >= 920.0f;
    m.scroll_h = std::max(120.0f, vp->WorkSize.y - ImGui::GetCursorPosY());
    return m;
}

float content_offset_x(const LayoutMetrics& m) {
    return std::max(m.pad, (m.full_w - m.content_w) * 0.5f);
}

constexpr ImU32 kGrayPanel = IM_COL32(168, 168, 168, 255);
constexpr ImU32 kDarkTitle = IM_COL32(48, 48, 48, 255);
constexpr ImU32 kWhite = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kBlack = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kRed = IM_COL32(180, 0, 0, 255);
constexpr ImU32 kBorderLight = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kBorderDark = IM_COL32(80, 80, 80, 255);
constexpr ImU32 kBarFill = IM_COL32(48, 48, 48, 255);
constexpr ImU32 kMeasureBlue = IM_COL32(0, 0, 160, 255);

constexpr ImVec4 kRetroBg{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 kRetroPanel{0.66f, 0.66f, 0.66f, 1.0f};
constexpr ImVec4 kRetroButton{1.0f, 1.0f, 1.0f, 1.0f};
constexpr ImVec4 kRetroSelected{0.55f, 0.55f, 0.55f, 1.0f};
constexpr ImVec4 kRetroBorder{0.20f, 0.20f, 0.20f, 1.0f};

void apply_retro_theme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.FrameRounding = 0.0f;
    s.ChildRounding = 0.0f;
    s.PopupRounding = 0.0f;
    s.ScrollbarRounding = 0.0f;
    s.GrabRounding = 0.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 1.0f;
    s.FrameBorderSize = 2.0f;
    s.PopupBorderSize = 2.0f;
    s.ItemSpacing = ImVec2(4, 2);
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(6, 2);
    s.CellPadding = ImVec2(4, 2);
    s.ItemInnerSpacing = ImVec2(4, 2);
    s.ColumnsMinSpacing = 6.0f;

    const ImVec4 bg = kRetroBg;
    const ImVec4 text(0.0f, 0.0f, 0.0f, 1.0f);
    const ImVec4 text_dim(0.18f, 0.18f, 0.18f, 1.0f);
    const ImVec4 white = kRetroButton;
    const ImVec4 border = kRetroBorder;

    s.Colors[ImGuiCol_Text] = text;
    s.Colors[ImGuiCol_TextDisabled] = text_dim;
    s.Colors[ImGuiCol_WindowBg] = bg;
    s.Colors[ImGuiCol_ChildBg] = kRetroPanel;
    s.Colors[ImGuiCol_PopupBg] = white;
    s.Colors[ImGuiCol_MenuBarBg] = kRetroPanel;
    s.Colors[ImGuiCol_Border] = border;
    s.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    s.Colors[ImGuiCol_Button] = white;
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.90f, 0.92f, 1.0f, 1.0f);
    s.Colors[ImGuiCol_ButtonActive] = kRetroSelected;

    s.Colors[ImGuiCol_FrameBg] = white;
    s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.94f, 1.0f, 1.0f);
    s.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.84f, 0.86f, 0.96f, 1.0f);

    s.Colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
    s.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);

    s.Colors[ImGuiCol_Header] = ImVec4(0.78f, 0.78f, 0.78f, 1.0f);
    s.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.50f, 0.58f, 0.95f, 1.0f);
    s.Colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.48f, 0.92f, 1.0f);

    s.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    s.Colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    s.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    s.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);

    s.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.58f, 0.58f, 0.58f, 1.0f);
    s.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.38f, 0.38f, 0.38f, 1.0f);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
    s.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);

    s.Colors[ImGuiCol_TitleBg] = bg;
    s.Colors[ImGuiCol_TitleBgActive] = bg;
    s.Colors[ImGuiCol_TitleBgCollapsed] = bg;
    s.Colors[ImGuiCol_ResizeGrip] = border;
    s.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
    s.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    s.Colors[ImGuiCol_Tab] = kRetroPanel;
    s.Colors[ImGuiCol_TabHovered] = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    s.Colors[ImGuiCol_TabActive] = white;
    s.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    s.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    s.Colors[ImGuiCol_NavHighlight] = ImVec4(0.0f, 0.0f, 0.70f, 1.0f);
    s.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.45f);
}

void draw_inset_rect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 fill = kWhite) {
    dl->AddRectFilled(a, b, fill);
    dl->AddLine(a, {b.x, a.y}, kBorderDark, 2.0f);
    dl->AddLine(a, {a.x, b.y}, kBorderDark, 2.0f);
    dl->AddLine({a.x, b.y}, b, kBorderLight, 2.0f);
    dl->AddLine({b.x, a.y}, b, kBorderLight, 2.0f);
}

void begin_retro_tooltip() {
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::BeginTooltip();
}

void end_retro_tooltip() {
    ImGui::EndTooltip();
    ImGui::PopStyleColor(3);
}

void same_line_or_wrap(float min_w = 64.0f) {
    if (ImGui::GetContentRegionAvail().x < min_w) {
        ImGui::NewLine();
    } else {
        ImGui::SameLine(0, 4);
    }
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

bool retro_gate_button(const char* label, qsim::GateKind kind, bool selected, bool danger = false) {
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, kRetroSelected);
    }
    if (danger) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.0f, 0.0f, 1.0f));
    }
    const bool clicked = ImGui::Button(label, ImVec2(38, 28));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        const auto& info = qsim::gate_info(kind);
        begin_retro_tooltip();
        ImGui::Text("%s", info.name);
        ImGui::Separator();
        ImGui::TextWrapped("%s", info.tooltip);
        if (info.is_parametric) {
            ImGui::Separator();
            ImGui::Text("Uses angle from parametric controls below.");
        }
        end_retro_tooltip();
    }
    if (danger) ImGui::PopStyleColor();
    if (selected) ImGui::PopStyleColor();
    return clicked;
}

bool retro_button(const char* label, bool enabled = true) {
    if (!enabled) ImGui::BeginDisabled();
    const bool clicked = ImGui::Button(label);
    if (!enabled) ImGui::EndDisabled();
    return clicked && enabled;
}

bool column_has_gate(const qsim::Circuit& circuit, int column, std::initializer_list<int> qubits) {
    for (int q : qubits) {
        if (circuit.op_at(q, column)) return true;
    }
    return false;
}

std::string format_amplitude(qsim::Complex c) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.3f", c.real());
    return buf;
}

std::string format_pct(double p) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f%%", p * 100.0);
    return buf;
}

void draw_prob_bar(ImDrawList* dl, ImVec2 pos, float width, float height, double prob) {
    const ImVec2 bar_max = {pos.x + width, pos.y + height};
    draw_inset_rect(dl, pos, bar_max, kWhite);
    const float fill_w = static_cast<float>(prob) * (width - 4.0f);
    if (fill_w > 0.5f) {
        dl->AddRectFilled({pos.x + 2.0f, pos.y + 2.0f},
                          {pos.x + 2.0f + fill_w, bar_max.y - 2.0f}, kBarFill);
    }
}

void draw_bloch(ImDrawList* dl, ImVec2 center, float radius, const qsim::QubitView& view) {
    dl->AddCircle(center, radius, kBlack, 0, 1.5f);
    dl->AddLine({center.x - radius, center.y}, {center.x + radius, center.y}, kBlack, 1.0f);
    dl->AddLine({center.x, center.y - radius}, {center.x, center.y + radius}, kBlack, 1.0f);
    const float px = center.x + static_cast<float>(view.bloch_x) * (radius - 4.0f);
    const float py = center.y - static_cast<float>(view.bloch_y) * (radius - 4.0f);
    dl->AddCircleFilled({px, py}, 4.0f, kBlack);
}

const char* preset_label(const std::string& stem) {
    if (stem == "bell_phi_plus") return "Bell |Phi+>";
    if (stem == "superposition") return "Superposition";
    if (stem == "phase_kick") return "Phase kick";
    if (stem == "swap_test") return "SWAP test";
    if (stem == "entangle_y") return "Entangle + Y";
    if (stem == "ghz_3") return "GHZ (3q)";
    if (stem == "teleportation") return "Teleportation";
    if (stem == "deutsch_jozsa") return "Deutsch-Jozsa";
    if (stem == "qft_3") return "QFT (3q)";
    if (stem == "grover_2") return "Grover |11>";
    if (stem == "bernstein_vazirani") return "Bernstein-Vazirani";
    if (stem == "superdense_coding") return "Superdense coding";
    if (stem == "vqc_demo") return "VQC demo";
    return stem.c_str();
}

bool column_has_barrier(const qsim::Circuit& circuit, int column) {
    for (const auto& op : circuit.ops) {
        if (op.column == column && op.kind == qsim::GateKind::Barrier) return true;
    }
    return false;
}

void draw_barrier_column(ImDrawList* dl, float cx, float y_top, float y_bot) {
    const float gap = 4.0f;
    dl->AddLine({cx - gap, y_top}, {cx - gap, y_bot}, kBlack, 2.0f);
    dl->AddLine({cx + gap, y_top}, {cx + gap, y_bot}, kBlack, 2.0f);
}

void draw_active_column_highlight(ImDrawList* dl, float cx, float y_top, float y_bot) {
    const float half = kCellW * 0.5f;
    dl->AddRectFilled({cx - half, y_top}, {cx + half, y_bot}, IM_COL32(255, 255, 0, 70));
    dl->AddRect({cx - half, y_top}, {cx + half, y_bot}, IM_COL32(200, 160, 0, 200), 0.0f, 0, 2.0f);
}

void draw_top_states(const qsim::StateVector& state, int num_qubits, bool filter, int max_rows) {
    struct Row {
        int index;
        double prob;
    };
    std::vector<Row> rows;
    const int dim = 1 << num_qubits;
    for (int i = 0; i < dim; ++i) {
        const double p = state.probability(i);
        if (filter && p < kProbFilterThreshold) continue;
        rows.push_back({i, p});
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.prob > b.prob; });
    if (rows.empty()) {
        ImGui::TextDisabled("(empty)");
        return;
    }
    const int shown = std::min(max_rows, static_cast<int>(rows.size()));
    for (int i = 0; i < shown; ++i) {
        const auto label = qsim::basis_label(rows[static_cast<std::size_t>(i)].index, num_qubits);
        ImGui::Text("%s  %s", label.c_str(), format_pct(rows[static_cast<std::size_t>(i)].prob).c_str());
    }
    if (static_cast<int>(rows.size()) > shown) {
        ImGui::TextDisabled("... +%d more", static_cast<int>(rows.size()) - shown);
    }
}

const char* initial_state_label(qsim::InitialStatePreset preset) {
    switch (preset) {
        case qsim::InitialStatePreset::AllZero: return "|0...0>";
        case qsim::InitialStatePreset::AllOne: return "|1...1>";
        case qsim::InitialStatePreset::Q0Plus: return "|+0...0>";
    }
    return "|0...0>";
}

void draw_measure_gate(ImDrawList* dl, float cx, float cy) {
    dl->AddRect({cx - 12.0f, cy - 12.0f}, {cx + 12.0f, cy + 12.0f}, kMeasureBlue, 0.0f, 0, 1.5f);
    dl->AddText({cx - 5.0f, cy - 7.0f}, kMeasureBlue, "M");
    dl->AddLine({cx + 12.0f, cy}, {cx + 20.0f, cy}, kMeasureBlue, 1.5f);
    dl->AddLine({cx + 20.0f, cy - 4.0f}, {cx + 20.0f, cy + 4.0f}, kMeasureBlue, 1.5f);
}

void draw_two_qubit_link(ImDrawList* dl, float cx, float y0, float y1, bool target_plus) {
    dl->AddLine({cx, std::min(y0, y1)}, {cx, std::max(y0, y1)}, kBlack, 1.5f);
    dl->AddCircle({cx, y0}, 5.0f, kBlack, 0, 1.5f);
    if (target_plus) {
        dl->AddLine({cx - 7, y1}, {cx + 7, y1}, kBlack, 2.0f);
        dl->AddLine({cx, y1 - 7}, {cx, y1 + 7}, kBlack, 2.0f);
    } else {
        dl->AddCircle({cx, y1}, 5.0f, kBlack, 0, 1.5f);
    }
}

void draw_ccx_column(ImDrawList* dl, const qsim::CircuitOp& op, float cx,
                     float table_y, int /*num_qubits*/) {
    auto row_center_y = [&](int q) {
        return table_y + kHeaderH + q * kCellH + kCellH * 0.5f;
    };
    const float yc1 = row_center_y(op.qubit);
    const float yc2 = row_center_y(op.qubit2);
    const float yt = row_center_y(op.qubit3);
    const float y_top = std::min({yc1, yc2, yt});
    const float y_bot = std::max({yc1, yc2, yt});
    dl->AddLine({cx, y_top}, {cx, y_bot}, kBlack, 1.5f);
    dl->AddCircle({cx, yc1}, 5.0f, kBlack, 0, 1.5f);
    dl->AddCircle({cx, yc2}, 5.0f, kBlack, 0, 1.5f);
    dl->AddLine({cx - 7, yt}, {cx + 7, yt}, kBlack, 2.0f);
    dl->AddLine({cx, yt - 7}, {cx, yt + 7}, kBlack, 2.0f);
}

struct GateBtn {
    const char* label;
    qsim::GateKind kind;
    bool danger = false;
};

void draw_palette_row(AppState& app, const GateBtn* gates, int count, float /*max_width*/) {
    for (int i = 0; i < count; ++i) {
        if (i > 0) {
            const float next_w = 44.0f;
            if (ImGui::GetContentRegionAvail().x < next_w) {
                ImGui::NewLine();
            } else {
                ImGui::SameLine(0, 4);
            }
        }
        const auto& g = gates[i];
        const bool sel = !app.eraser_mode && app.selected_gate == g.kind;
        if (retro_gate_button(g.label, g.kind, sel, g.danger)) {
            app.selected_gate = g.kind;
            app.eraser_mode = false;
            app.pending_qubit.reset();
            app.pending_qubit2.reset();
            app.pending_column.reset();
            app.status_message = std::string("Selected ") + qsim::gate_info(g.kind).name + ".";
        }
    }
}

void draw_gate_cheat_sheet(const AppState& app) {
    if (ImGui::CollapsingHeader("Gate cheat sheet", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
        if (ImGui::BeginTable("##cheat", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Gate", ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn("Matrix / action");
            ImGui::TableSetupColumn("Plain language");
            ImGui::TableHeadersRow();

            const qsim::GateKind gates[] = {
                qsim::GateKind::H,  qsim::GateKind::X,  qsim::GateKind::Y,  qsim::GateKind::Z,
                qsim::GateKind::S,  qsim::GateKind::T,  qsim::GateKind::Sdg, qsim::GateKind::Tdg,
                qsim::GateKind::Rx, qsim::GateKind::Ry, qsim::GateKind::Rz,
                qsim::GateKind::CNOT, qsim::GateKind::CZ, qsim::GateKind::SWAP, qsim::GateKind::CCX,
                qsim::GateKind::Measure,
            };

            for (const auto kind : gates) {
                const auto& info = qsim::gate_info(kind);
                double p1 = 0.0;
                double p2 = 0.0;
                double p3 = 0.0;
                if (info.is_parametric && app.selected_gate == kind) {
                    p1 = app.rotation_param;
                } else if (kind == qsim::GateKind::Rx || kind == qsim::GateKind::Ry ||
                           kind == qsim::GateKind::Rz) {
                    p1 = 1.5707963267948966;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", info.symbol);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", qsim::gate_matrix_display(kind, p1, p2, p3).c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextWrapped("%s", info.tooltip);
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }
}

void draw_about_modal(AppState& app) {
    if (app.show_about) ImGui::OpenPopup("About Quantum Circuit Lab");
    ImGui::SetNextWindowSize(ImVec2(480, 340), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("About Quantum Circuit Lab", &app.show_about,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        const ImVec2 logo_center = {ImGui::GetCursorScreenPos().x + 28.0f,
                                    ImGui::GetCursorScreenPos().y + 28.0f};
        draw_phi_logo(ImGui::GetWindowDrawList(), logo_center, 22.0f, kBlack, 2.5f);
        ImGui::Dummy(ImVec2(56.0f, 56.0f));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Quantum Circuit Lab");
        ImGui::TextDisabled("State-vector quantum simulator");
        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Quantum Circuit Lab — an educational state-vector simulator.\n\n"
            "Built by Mehar Ali\n"
            "CS @ Information Technology University (ITU)\n"
            "Qiskit Fall Fest organizer | VQC research | QCIT on YouTube\n\n"
            "Conventions: q0 is top wire (MSB). Initial state |0...0>.\n"
            "Engine: C++20 state-vector simulator. UI: Dear ImGui.");
        ImGui::Separator();
        ImGui::TextUnformatted("Links");
        if (ImGui::Button("QCIT on YouTube")) {
            open_external_url("https://www.youtube.com/@QCIT");
        }
        ImGui::SameLine();
        if (ImGui::Button("Portfolio (meharali.dev)")) {
            open_external_url("https://meharali.dev");
        }
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(120, 0))) app.show_about = false;
        ImGui::EndPopup();
    }
}

void draw_preset_browser(AppState& app) {
    if (app.show_preset_browser) ImGui::OpenPopup("Preset Browser");
    ImGui::SetNextWindowSize(ImVec2(640, 440), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Preset Browser", &app.show_preset_browser,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        static int category_filter = 0;
        const char* filters[] = {"All", "Basics", "Algorithms", "Advanced", "Research", "User"};
        ImGui::SetNextItemWidth(160.0f);
        ImGui::Combo("Category", &category_filter, filters, IM_ARRAYSIZE(filters));

        const auto presets = app.list_all_presets();
        if (ImGui::BeginTable("presets", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              ImVec2(600, 300))) {
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Description");
            ImGui::TableSetupColumn("Load", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();
            for (const auto& pe : presets) {
                if (category_filter > 0) {
                    const std::string want = filters[category_filter];
                    if (pe.category != want) continue;
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(pe.category.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s%s", preset_label(pe.name), pe.user_preset ? " (user)" : "");
                ImGui::TableSetColumnIndex(2);
                ImGui::TextWrapped("%s",
                                   pe.description.empty() ? "(no description)" : pe.description.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::PushID(pe.path.c_str());
                if (ImGui::SmallButton("Load")) {
                    app.try_load_circuit_file(pe.path);
                    app.show_preset_browser = false;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (ImGui::Button("Close", ImVec2(120, 0))) app.show_preset_browser = false;
        ImGui::EndPopup();
    }
}

void draw_save_preset_modal(AppState& app) {
    if (app.show_save_preset) {
        if (app.save_preset_desc_buf[0] == '\0' && !app.circuit_description.empty()) {
            std::snprintf(app.save_preset_desc_buf.data(), app.save_preset_desc_buf.size(), "%s",
                          app.circuit_description.c_str());
        }
        ImGui::OpenPopup("Save User Preset");
    }
    if (ImGui::BeginPopupModal("Save User Preset", &app.show_save_preset,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", app.save_preset_name_buf.data(), app.save_preset_name_buf.size());
        ImGui::InputTextMultiline("Description", app.save_preset_desc_buf.data(),
                                  app.save_preset_desc_buf.size(), ImVec2(400, 80));
        ImGui::TextDisabled("Saved to ~/.config/quantum-lab/presets/");
        if (ImGui::Button("Save", ImVec2(100, 0))) {
            app.save_user_preset(app.save_preset_name_buf.data(), app.save_preset_desc_buf.data());
            app.show_save_preset = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) app.show_save_preset = false;
        ImGui::EndPopup();
    }
}

void draw_import_modal(AppState& app) {
    if (app.import_dialog == ImportDialog::None) return;
    const bool qasm = app.import_dialog == ImportDialog::Qasm;
    const char* title = qasm ? "Import OpenQASM Circuit" : "Import JSON Circuit";
    ImGui::OpenPopup(title);
    bool open = true;
    if (ImGui::BeginPopupModal(title, &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!open) app.import_dialog = ImportDialog::None;
        ImGui::InputText("File path", app.import_path_buf.data(), app.import_path_buf.size());
        if (ImGui::Button("Browse...")) {
            const auto path =
                qasm ? native_open_qasm_dialog()
                     : native_open_file_dialog("Import Circuit JSON", "JSON (*.json)|*.json");
            if (path) {
                std::snprintf(app.import_path_buf.data(), app.import_path_buf.size(), "%s",
                              path->c_str());
            }
        }
        if (ImGui::Button("Import", ImVec2(100, 0))) {
            const bool ok =
                qasm ? app.try_import_qasm(app.import_path_buf.data())
                     : app.try_load_circuit_file(app.import_path_buf.data());
            if (ok) app.import_dialog = ImportDialog::None;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) app.import_dialog = ImportDialog::None;
        ImGui::EndPopup();
    }
}

constexpr float kTitleBarH = 28.0f;
constexpr float kTitleBtnW = 24.0f;
constexpr float kTitleBtnH = 22.0f;

void draw_title_bar(GLFWwindow* window, const char* title, float width) {
    const ImVec2 bar_pos = ImGui::GetCursorScreenPos();
    const ImVec2 bar_max = {bar_pos.x + width, bar_pos.y + kTitleBarH};
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(bar_pos, bar_max, kDarkTitle);

    const float controls_w = kTitleBtnW * 3.0f + 4.0f;
    const float drag_w = width - controls_w;

    ImGui::SetCursorScreenPos(bar_pos);
    ImGui::InvisibleButton("##title_drag", ImVec2(drag_w, kTitleBarH));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        int wx = 0;
        int wy = 0;
        glfwGetWindowPos(window, &wx, &wy);
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        glfwSetWindowPos(window, wx + static_cast<int>(delta.x), wy + static_cast<int>(delta.y));
    }

    const float logo_r = 9.0f;
    const float title_x = bar_pos.x + 8.0f + logo_r * 2.0f + 6.0f;
    const ImVec2 logo_center = {bar_pos.x + 8.0f + logo_r, bar_pos.y + kTitleBarH * 0.5f};
    draw_phi_logo(dl, logo_center, logo_r, kWhite, 2.0f);

    const ImVec2 ts = ImGui::CalcTextSize(title);
    dl->AddText({title_x, bar_pos.y + (kTitleBarH - ts.y) * 0.5f}, kWhite, title);

    ImGui::SetCursorScreenPos({bar_max.x - controls_w, bar_pos.y + 3.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, kRetroButton);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

    if (ImGui::Button("_", ImVec2(kTitleBtnW, kTitleBtnH))) {
        glfwIconifyWindow(window);
    }
    if (ImGui::IsItemHovered()) {
        begin_retro_tooltip();
        ImGui::TextUnformatted("Minimize");
        end_retro_tooltip();
    }
    ImGui::SameLine();
    if (ImGui::Button("#", ImVec2(kTitleBtnW, kTitleBtnH))) {
        if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) {
            glfwRestoreWindow(window);
        } else {
            glfwMaximizeWindow(window);
        }
    }
    if (ImGui::IsItemHovered()) {
        begin_retro_tooltip();
        ImGui::TextUnformatted("Maximize / Restore");
        end_retro_tooltip();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    if (ImGui::Button("X", ImVec2(kTitleBtnW, kTitleBtnH))) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (ImGui::IsItemHovered()) {
        begin_retro_tooltip();
        ImGui::TextUnformatted("Close");
        end_retro_tooltip();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    ImGui::SetCursorScreenPos({bar_pos.x, bar_max.y});
    ImGui::Dummy(ImVec2(width, 0));
}

void draw_file_menu(AppState& app, GLFWwindow* window) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Import JSON...")) app.import_dialog = ImportDialog::Json;
            if (ImGui::MenuItem("Import QASM...")) app.import_dialog = ImportDialog::Qasm;
            if (ImGui::MenuItem("Export JSON...")) {
                if (auto path = native_save_file_dialog("Export Circuit JSON", "circuit_export.json")) {
                    app.export_json(*path);
                }
            }
            if (ImGui::MenuItem("Export QASM...")) {
                if (auto path = native_save_qasm_dialog()) app.export_qasm(*path);
            }
            if (ImGui::MenuItem("Export PNG...")) {
                if (auto path = native_save_png_dialog()) app.export_circuit_png(*path);
            }
            if (ImGui::MenuItem("Copy QASM")) app.copy_qasm_to_clipboard(window);
            if (ImGui::MenuItem("Save as Preset...")) app.show_save_preset = true;
            if (ImGui::MenuItem("Preset Browser...")) app.show_preset_browser = true;
            ImGui::Separator();
            if (ImGui::BeginMenu("Recent Files")) {
                if (app.settings.recent_files.empty()) {
                    ImGui::MenuItem("(none)", nullptr, false, false);
                } else {
                    for (const auto& path : app.settings.recent_files) {
                        const auto filename = std::filesystem::path(path).filename().string();
                        if (ImGui::MenuItem(filename.c_str())) app.try_load_circuit_file(path);
                        if (ImGui::IsItemHovered()) {
                            begin_retro_tooltip();
                            ImGui::TextUnformatted(path.c_str());
                            end_retro_tooltip();
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) app.show_about = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

}  // namespace

void app_init(AppState& app, const std::string& presets_dir) {
    apply_retro_theme();
    app.presets_dir = presets_dir;
    app.settings.load();
    app.apply_settings();
    app.settings.ensure_dirs();
    app.circuit.num_qubits = 2;
    app.simulator = qsim::Simulator(2);
    app.sync_simulator();
    app.status_message = "Ready.";
}

void AppState::apply_settings() {
    filter_probs = settings.filter_probs;
    auto_run_on_edit = settings.auto_run_on_edit;
    collapse_on_measure = settings.collapse_on_measure;
    shot_count = std::clamp(settings.shot_count, 1, 100000);
    settings.shot_count = shot_count;
    const int preset = std::clamp(settings.initial_state, 0, 2);
    initial_state = static_cast<qsim::InitialStatePreset>(preset);
}

void AppState::persist_settings(int window_w, int window_h) {
    settings.filter_probs = filter_probs;
    settings.auto_run_on_edit = auto_run_on_edit;
    settings.collapse_on_measure = collapse_on_measure;
    settings.initial_state = static_cast<int>(initial_state);
    settings.shot_count = shot_count;
    settings.window_width = window_w;
    settings.window_height = window_h;
    settings.save();
}

void app_shutdown(AppState& app, int window_w, int window_h) {
    app.persist_settings(window_w, window_h);
}

int AppState::time_columns() const {
    return std::max(min_time_columns, circuit.num_columns() + 2);
}

std::string AppState::pending_message() const {
    if (eraser_mode) return "ERASE mode — click a cell to remove its gate.";
    if (selected_gate == qsim::GateKind::CCX) {
        if (!pending_qubit.has_value()) return "CCX: click first control qubit.";
        if (!pending_qubit2.has_value()) return "CCX: click second control qubit.";
        return "CCX: click target qubit.";
    }
    const auto& info = qsim::gate_info(selected_gate);
    if (info.num_qubits == 2 && !pending_qubit.has_value()) {
        return std::string(info.name) + ": click control qubit, then target.";
    }
    if (info.num_qubits == 2 && pending_qubit.has_value()) {
        return std::string(info.name) + ": click target qubit.";
    }
    return std::string("Selected ") + info.name + " — click a circuit cell.";
}

void AppState::sync_simulator() {
    try {
        simulator.set_initial_state(initial_state);
        simulator.set_collapse_on_measure(collapse_on_measure);
        simulator.load_circuit(circuit);
        if (auto_run_on_edit) {
            simulator.run_all();
        }
        shot_histogram.clear();
    } catch (const std::exception& ex) {
        status_message = std::string("Simulation error: ") + ex.what();
    }
}

void AppState::apply_initial_state() {
    simulator.set_initial_state(initial_state);
    simulator.set_collapse_on_measure(collapse_on_measure);
    reset_simulation();
    if (auto_run_on_edit) {
        simulator.run_all();
    }
    shot_histogram.clear();
}

void AppState::run_shots() {
    if (!auto_run_on_edit && simulator.current_step() < simulator.total_steps()) {
        status_message = "Run the full circuit before sampling shots.";
        return;
    }
    if (simulator.current_step() < simulator.total_steps()) {
        simulator.run_all();
    }
    shot_histogram = simulator.sample_shots(shot_count);
    status_message = "Sampled " + std::to_string(shot_count) + " shots.";
}

void AppState::reset_simulation() {
    simulator.set_initial_state(initial_state);
    simulator.set_collapse_on_measure(collapse_on_measure);
    try {
        simulator.load_circuit(circuit);
        if (auto_run_on_edit) {
            simulator.run_all();
        }
        shot_histogram.clear();
        status_message = std::string("Reset to ") + initial_state_label(initial_state) + ".";
    } catch (const std::exception& ex) {
        status_message = std::string("Simulation error: ") + ex.what();
    }
}

void AppState::set_num_qubits(int n) {
    n = std::clamp(n, 2, 12);
    if (n == circuit.num_qubits) return;
    circuit.num_qubits = n;
    circuit.ops.erase(
        std::remove_if(circuit.ops.begin(), circuit.ops.end(),
                       [n](const qsim::CircuitOp& op) {
                           auto spans = [&](int q) { return q >= 0 && q < n; };
                           if (!spans(op.qubit)) return true;
                           if (op.qubit2 >= 0 && !spans(op.qubit2)) return true;
                           if (op.qubit3 >= 0 && !spans(op.qubit3)) return true;
                           return false;
                       }),
        circuit.ops.end());
    simulator = qsim::Simulator(n);
    sync_simulator();
    status_message = "Qubits set to " + std::to_string(n) + ".";
}

void commit_loaded_circuit(AppState& app, const std::string& path) {
    app.circuit.num_qubits = std::clamp(app.circuit.num_qubits, 2, 12);
    app.current_file_path = path;
    app.next_op_id = 1;
    for (auto& op : app.circuit.ops) op.id = app.next_op_id++;
    app.simulator = qsim::Simulator(app.circuit.num_qubits);
    app.simulator.clear_history();
    app.sync_simulator();
    app.settings.add_recent_file(path);
    app.pending_qubit.reset();
    app.pending_qubit2.reset();
    app.pending_column.reset();
}

bool AppState::try_load_circuit_file(const std::string& path) {
    try {
        auto doc = qsim::load_circuit_file(path);
        circuit = std::move(doc.circuit);
        circuit_description = doc.description;
        commit_loaded_circuit(*this, path);
        status_message =
            circuit_description.empty() ? "Loaded circuit." : ("Loaded — " + circuit_description);
        return true;
    } catch (const std::exception& ex) {
        status_message = std::string("Load error: ") + ex.what();
        return false;
    }
}

void AppState::export_json(const std::string& path) {
    const std::string out_path = path.empty() ? "circuit_export.json" : path;
    try {
        qsim::save_circuit_file(out_path, circuit, circuit_description);
        current_file_path = out_path;
        settings.add_recent_file(out_path);
        status_message = "Exported to " + out_path;
    } catch (const std::exception& ex) {
        status_message = std::string("Export failed: ") + ex.what();
    }
}

bool AppState::try_import_qasm(const std::string& path) {
    if (path.empty()) {
        status_message = "Import path is empty.";
        return false;
    }
    try {
        circuit = qsim::load_qasm_file(path);
        circuit_description =
            "Imported OpenQASM: " + std::filesystem::path(path).filename().string();
        commit_loaded_circuit(*this, path);
        status_message = "Imported QASM — " + circuit_description;
        return true;
    } catch (const std::exception& ex) {
        status_message = std::string("QASM import error: ") + ex.what();
        return false;
    }
}

void AppState::export_qasm(const std::string& path) {
    const std::string out_path = path.empty() ? "circuit.qasm" : path;
    try {
        const int omitted = qsim::count_unsupported_qasm_gates(circuit);
        qsim::save_qasm_file(out_path, circuit);
        current_file_path = out_path;
        settings.add_recent_file(out_path);
        status_message = "Exported QASM to " + out_path;
        if (omitted > 0) {
            status_message += " (warning: " + std::to_string(omitted) +
                              " gate(s) omitted — use JSON for full fidelity)";
        }
    } catch (const std::exception& ex) {
        status_message = std::string("QASM export failed: ") + ex.what();
    }
}

bool AppState::export_circuit_png(const std::string& path) {
    const std::string out_path = path.empty() ? "circuit.png" : path;
    int w = 0;
    int h = 0;
    const auto rgba = render_circuit_rgba(circuit, min_time_columns, w, h);
    if (!write_png_file(out_path, w, h, rgba)) {
        status_message = "PNG export failed.";
        return false;
    }
    status_message = "Exported PNG to " + out_path;
    return true;
}

void AppState::copy_qasm_to_clipboard(GLFWwindow* window) {
    if (!window) return;
    const std::string qasm = qsim::circuit_to_qasm(circuit);
    glfwSetClipboardString(window, qasm.c_str());
    const int omitted = qsim::count_unsupported_qasm_gates(circuit);
    status_message = "Copied OpenQASM to clipboard.";
    if (omitted > 0) {
        status_message += " (warning: " + std::to_string(omitted) + " gate(s) omitted)";
    }
}

void AppState::save_user_preset(const std::string& name, const std::string& description) {
    if (name.empty()) {
        status_message = "Preset name required.";
        return;
    }
    std::string safe_name;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            safe_name += c;
        } else if (c == ' ') {
            safe_name += '_';
        }
    }
    if (safe_name.empty()) safe_name = "preset";

    const auto path = AppSettings::user_presets_dir() / (safe_name + ".json");
    try {
        settings.ensure_dirs();
        qsim::save_circuit_file(path.string(), circuit, description, "User");
        circuit_description = description;
        settings.add_recent_file(path.string());
        status_message = "Saved user preset: " + safe_name;
    } catch (const std::exception& ex) {
        status_message = std::string("Save preset failed: ") + ex.what();
    }
}

std::vector<PresetEntry> AppState::list_all_presets() const {
    namespace fs = std::filesystem;
    std::vector<PresetEntry> entries;

    auto collect = [&](const fs::path& dir, bool user) {
        if (!fs::is_directory(dir)) return;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() != ".json") continue;
            PresetEntry pe;
            pe.path = entry.path().string();
            pe.name = entry.path().stem().string();
            pe.description = qsim::read_circuit_description(pe.path);
            pe.category = qsim::read_circuit_category(pe.path);
            if (pe.category.empty()) pe.category = user ? "User" : "Uncategorized";
            pe.user_preset = user;
            entries.push_back(std::move(pe));
        }
    };

    collect(presets_dir, false);
    collect(AppSettings::user_presets_dir(), true);
    std::sort(entries.begin(), entries.end(), [](const PresetEntry& a, const PresetEntry& b) {
        if (a.category != b.category) return a.category < b.category;
        return a.name < b.name;
    });
    return entries;
}

int AppState::place_gate(int qubit, int column) {
    if (eraser_mode) {
        erase_at(qubit, column);
        return -1;
    }

    if (selected_gate == qsim::GateKind::CCX) {
        if (!pending_qubit.has_value()) {
            if (column_has_gate(circuit, column, {qubit})) {
                status_message = "Cell already occupied.";
                return -1;
            }
            pending_qubit = qubit;
            pending_column = column;
            pending_gate = qsim::GateKind::CCX;
            status_message = pending_message();
            return -1;
        }
        if (!pending_qubit2.has_value()) {
            const int col = pending_column.value_or(column);
            if (column != col) {
                status_message = "CCX: use the same time column for all three clicks.";
                return -1;
            }
            if (column_has_gate(circuit, col, {qubit})) {
                status_message = "Cell already occupied.";
                return -1;
            }
            if (qubit == *pending_qubit) {
                status_message = "CCX: controls must differ.";
                return -1;
            }
            pending_qubit2 = qubit;
            status_message = pending_message();
            return -1;
        }
        const int col = pending_column.value_or(column);
        if (column != col) {
            status_message = "CCX: use the same time column for all three clicks.";
            return -1;
        }
        const int c1 = *pending_qubit;
        const int c2 = *pending_qubit2;
        if (c1 == c2 || c1 == qubit || c2 == qubit) {
            pending_qubit.reset();
            pending_qubit2.reset();
            pending_column.reset();
            status_message = "CCX: all three qubits must differ.";
            return -1;
        }
        if (column_has_gate(circuit, col, {c1, c2, qubit})) {
            status_message = "Cell already occupied.";
            return -1;
        }
        qsim::CircuitOp op{qsim::GateKind::CCX, col, c1, c2, qubit, 0.0, 0.0, 0.0, next_op_id++};
        pending_qubit.reset();
        pending_qubit2.reset();
        pending_column.reset();
        circuit.add_op(op);
        simulator.push_history(circuit);
        sync_simulator();
        status_message = "Placed CCX.";
        return op.id;
    }

    const auto& info = qsim::gate_info(selected_gate);
    if (info.num_qubits == 2) {
        if (!pending_qubit.has_value()) {
            if (column_has_gate(circuit, column, {qubit})) {
                status_message = "Cell already occupied.";
                return -1;
            }
            pending_qubit = qubit;
            pending_column = column;
            pending_gate = selected_gate;
            status_message = pending_message();
            return -1;
        }
        const int q0 = *pending_qubit;
        const int col = pending_column.value_or(column);
        if (column != col) {
            status_message = "Use the same time column for control and target.";
            return -1;
        }
        const qsim::GateKind gate = pending_gate;
        pending_qubit.reset();
        pending_column.reset();
        if (gate == qsim::GateKind::SWAP && q0 == qubit) {
            status_message = "SWAP: control and target must differ.";
            return -1;
        }
        if ((gate == qsim::GateKind::CNOT || gate == qsim::GateKind::CZ) && q0 == qubit) {
            status_message = "Control and target must differ.";
            return -1;
        }
        if (column_has_gate(circuit, col, {q0, qubit})) {
            status_message = "Cell already occupied.";
            return -1;
        }
        qsim::CircuitOp op{gate, col, q0, qubit, -1, rotation_param, 0.0, 0.0, next_op_id++};
        circuit.add_op(op);
        simulator.push_history(circuit);
        sync_simulator();
        status_message = "Placed " + std::string(qsim::gate_info(gate).name) + ".";
        return op.id;
    }

    if (circuit.op_at(qubit, column)) {
        status_message = "Cell already occupied.";
        return -1;
    }
    qsim::CircuitOp op{selected_gate, column, qubit, -1, -1,
                       rotation_param, 0.0, 0.0, next_op_id++};
    circuit.add_op(op);
    simulator.push_history(circuit);
    sync_simulator();
    status_message = "Placed " + std::string(info.name) + ".";
    return op.id;
}

void AppState::erase_at(int qubit, int column) {
    const auto existing = circuit.op_at(qubit, column);
    if (!existing) return;
    circuit.remove_op(existing->id);
    simulator.push_history(circuit);
    sync_simulator();
    status_message = "Gate erased.";
}

void AppState::undo() {
    qsim::Circuit c = circuit;
    if (simulator.undo(c)) {
        circuit = c;
        sync_simulator();
        status_message = "Undo.";
    }
}

void AppState::redo() {
    qsim::Circuit c = circuit;
    if (simulator.redo(c)) {
        circuit = c;
        sync_simulator();
        status_message = "Redo.";
    }
}

void app_frame(AppState& app, GLFWwindow* window) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kRetroBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollbar);

    const float full_w = vp->WorkSize.x;
    char title[64];
    std::snprintf(title, sizeof(title), "%d-Qubit Quantum Circuit Simulator", app.circuit.num_qubits);
    draw_title_bar(window, title, full_w);

    draw_file_menu(app, window);

    const LayoutMetrics layout = compute_layout(vp);
    const float content_w = layout.content_w;
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.58f, 0.58f, 0.58f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.38f, 0.38f, 0.38f, 1.0f));
    ImGui::BeginChild("##scroll", ImVec2(full_w, layout.scroll_h), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    ImGui::SetCursorPosX(content_offset_x(layout));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
    ImGui::BeginGroup();

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + content_w);
    ImGui::TextWrapped(
        "Pick a gate, click a circuit cell. CX/CZ/CCX: multi-click. ERASE removes gates.");
    ImGui::PopTextWrapPos();

    if (!app.circuit_description.empty()) {
        begin_fieldset("Preset / circuit description");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.55f, 1.0f));
        ImGui::TextWrapped("%s", app.circuit_description.c_str());
        ImGui::PopStyleColor();
        end_fieldset();
    }

    draw_gate_cheat_sheet(app);

    // Qubit count + initial state
    int nq = app.circuit.num_qubits;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Qubits");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::SliderInt("##qubit_count", &nq, 2, 12, "%d")) {
        if (nq > 10 && app.circuit.num_qubits <= 10) {
            app.status_message =
                "Warning: >10 qubits uses 2048+ amplitudes — expect slower updates.";
        }
        app.set_num_qubits(nq);
    }
    if (ImGui::IsItemHovered()) {
        begin_retro_tooltip();
        ImGui::Text("Drag to set register size (2–12 qubits).");
        ImGui::Text("Current: %d qubit%s", nq, nq == 1 ? "" : "s");
        end_retro_tooltip();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Initial");
    ImGui::SameLine();
    const qsim::InitialStatePreset presets[] = {qsim::InitialStatePreset::AllZero,
                                                qsim::InitialStatePreset::AllOne,
                                                qsim::InitialStatePreset::Q0Plus};
    for (const auto preset : presets) {
        const bool sel = app.initial_state == preset;
        if (sel) ImGui::PushStyleColor(ImGuiCol_Button, kRetroSelected);
        if (ImGui::Button(initial_state_label(preset))) {
            app.initial_state = preset;
            app.apply_initial_state();
            app.status_message =
                std::string("Initial state set to ") + initial_state_label(preset) + ".";
        }
        if (sel) ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);
    }
    ImGui::NewLine();
    if (app.circuit.num_qubits > 10) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.0f, 0.0f, 1.0f));
        ImGui::TextWrapped(
            "WARNING: %d qubits = %d amplitudes (O(2^n) memory). Simulation may be slow or "
            "unresponsive.",
            app.circuit.num_qubits, 1 << app.circuit.num_qubits);
        ImGui::PopStyleColor();
    }

    // Gate palette — basic row
    begin_fieldset("Gate palette");
    {
        const float palette_w = ImGui::GetContentRegionAvail().x;
        const GateBtn basic[] = {
            {"H", qsim::GateKind::H}, {"X", qsim::GateKind::X}, {"Y", qsim::GateKind::Y},
            {"Z", qsim::GateKind::Z}, {"S", qsim::GateKind::S}, {"T", qsim::GateKind::T},
            {"CX", qsim::GateKind::CNOT}, {"SW", qsim::GateKind::SWAP},
        };
        draw_palette_row(app, basic, static_cast<int>(sizeof(basic) / sizeof(basic[0])), palette_w);
        ImGui::NewLine();

        const GateBtn advanced[] = {
            {"CZ", qsim::GateKind::CZ}, {"CCX", qsim::GateKind::CCX},
            {"Rx", qsim::GateKind::Rx}, {"Ry", qsim::GateKind::Ry}, {"Rz", qsim::GateKind::Rz},
            {"S+", qsim::GateKind::Sdg}, {"T+", qsim::GateKind::Tdg},
            {"||", qsim::GateKind::Barrier}, {"M", qsim::GateKind::Measure},
        };
        draw_palette_row(app, advanced, static_cast<int>(sizeof(advanced) / sizeof(advanced[0])),
                         palette_w);
        ImGui::NewLine();

        const bool er_sel = app.eraser_mode;
        if (retro_gate_button("ERASE", qsim::GateKind::I, er_sel, true)) {
            app.eraser_mode = true;
            app.pending_qubit.reset();
            app.pending_qubit2.reset();
            app.pending_column.reset();
            app.status_message = app.pending_message();
        }
        if (ImGui::IsItemHovered()) {
            begin_retro_tooltip();
            ImGui::TextUnformatted("Eraser");
            ImGui::Separator();
            ImGui::TextWrapped("Click a circuit cell to remove the gate placed there.");
            end_retro_tooltip();
        }

        // Parametric controls (A4)
        if (qsim::gate_info(app.selected_gate).is_parametric && !app.eraser_mode) {
            ImGui::Separator();
            float angle = static_cast<float>(app.rotation_param);
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::SliderFloat("Rotation angle (rad)", &angle, 0.0f, 6.2831853f, "%.3f")) {
                app.rotation_param = angle;
            }
            ImGui::SameLine();
            ImGui::Text("(%.1f deg)", angle * 57.2957795f);
        }
    }
    end_fieldset();

    // Circuit grid
    {
        const int n = app.circuit.num_qubits;
        const int cols = app.time_columns();
        const float table_h = kHeaderH + n * kCellH + 4.0f;
        const float table_w = kLabelW + cols * kCellW;

        begin_fieldset("Circuit");
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
        ImGui::BeginChild("##circuit_scroll", ImVec2(-1, table_h), true,
                          ImGuiWindowFlags_HorizontalScrollbar);

        const ImVec2 table_start = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        draw_inset_rect(dl, table_start, {table_start.x + kLabelW, table_start.y + kHeaderH}, kGrayPanel);
        for (int c = 0; c < cols; ++c) {
            const float x = table_start.x + kLabelW + c * kCellW;
            draw_inset_rect(dl, {x, table_start.y}, {x + kCellW, table_start.y + kHeaderH}, kGrayPanel);
            char hdr[8];
            std::snprintf(hdr, sizeof(hdr), "t%d", c + 1);
            const ImVec2 ts = ImGui::CalcTextSize(hdr);
            dl->AddText({x + (kCellW - ts.x) * 0.5f, table_start.y + (kHeaderH - ts.y) * 0.5f},
                        kBlack, hdr);
        }

        // Step column highlight
        if (app.simulator.total_steps() > 0) {
            if (const auto col = app.simulator.highlight_column()) {
                const float cx = table_start.x + kLabelW + *col * kCellW + kCellW * 0.5f;
                const float y_top = table_start.y + kHeaderH + 2.0f;
                const float y_bot = table_start.y + kHeaderH + n * kCellH - 2.0f;
                draw_active_column_highlight(dl, cx, y_top, y_bot);
            }
        }

        // Barriers
        for (int c = 0; c < cols; ++c) {
            if (!column_has_barrier(app.circuit, c)) continue;
            const float cx = table_start.x + kLabelW + c * kCellW + kCellW * 0.5f;
            const float y_top = table_start.y + kHeaderH + 2.0f;
            const float y_bot = table_start.y + kHeaderH + n * kCellH - 2.0f;
            draw_barrier_column(dl, cx, y_top, y_bot);
        }

        // Multi-qubit gate visuals (draw once per op)
        std::unordered_set<int> drawn;
        for (const auto& op : app.circuit.ops) {
            if (drawn.count(op.id)) continue;
            if (op.column < 0 || op.column >= cols) continue;
            const float cx = table_start.x + kLabelW + op.column * kCellW + kCellW * 0.5f;
            auto row_center_y = [&](int qi) {
                return table_start.y + kHeaderH + qi * kCellH + kCellH * 0.5f;
            };

            switch (op.kind) {
                case qsim::GateKind::CNOT:
                    draw_two_qubit_link(dl, cx, row_center_y(op.qubit), row_center_y(op.qubit2), true);
                    drawn.insert(op.id);
                    break;
                case qsim::GateKind::CZ:
                    draw_two_qubit_link(dl, cx, row_center_y(op.qubit), row_center_y(op.qubit2), false);
                    drawn.insert(op.id);
                    break;
                case qsim::GateKind::CCX:
                    draw_ccx_column(dl, op, cx, table_start.y, n);
                    drawn.insert(op.id);
                    break;
                default:
                    break;
            }
        }

        for (int q = 0; q < n; ++q) {
            const float row_y = table_start.y + kHeaderH + q * kCellH;
            char qlab[8];
            std::snprintf(qlab, sizeof(qlab), "q%d", q);
            draw_inset_rect(dl, {table_start.x, row_y}, {table_start.x + kLabelW, row_y + kCellH},
                            kGrayPanel);
            const ImVec2 qts = ImGui::CalcTextSize(qlab);
            dl->AddText({table_start.x + (kLabelW - qts.x) * 0.5f, row_y + (kCellH - qts.y) * 0.5f},
                        kBlack, qlab);

            for (int c = 0; c < cols; ++c) {
                const float x = table_start.x + kLabelW + c * kCellW;
                const ImVec2 cmin = {x, row_y};
                const ImVec2 cmax = {x + kCellW, row_y + kCellH};
                draw_inset_rect(dl, cmin, cmax, kWhite);

                ImGui::SetCursorScreenPos(cmin);
                ImGui::PushID(q * 1000 + c);
                ImGui::InvisibleButton("cell", ImVec2(kCellW, kCellH));
                if (ImGui::IsItemHovered()) {
                    begin_retro_tooltip();
                    ImGui::Text("q%d, t%d — click to place gate", q, c + 1);
                    end_retro_tooltip();
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    app.place_gate(q, c);
                }
                ImGui::PopID();

                const auto op = app.circuit.op_at(q, c);
                if (!op || op->kind == qsim::GateKind::Barrier) continue;

                const float cx = x + kCellW * 0.5f;
                const float cy = row_y + kCellH * 0.5f;

                switch (op->kind) {
                    case qsim::GateKind::CNOT:
                    case qsim::GateKind::CZ:
                    case qsim::GateKind::CCX:
                        break;
                    case qsim::GateKind::Measure:
                        draw_measure_gate(dl, cx, cy);
                        break;
                    default: {
                        const char* sym = qsim::gate_info(op->kind).symbol;
                        const ImVec2 sts = ImGui::CalcTextSize(sym);
                        dl->AddText({cx - sts.x * 0.5f, cy - sts.y * 0.5f}, kBlack, sym);
                        break;
                    }
                }
            }
        }

        ImGui::SetCursorScreenPos({table_start.x, table_start.y + table_h});
        ImGui::Dummy(ImVec2(table_w, 1.0f));
        ImGui::EndChild();
        ImGui::PopStyleVar();
        end_fieldset();
    }

    // Controls and presets
    {
        const float gap = 6.0f;
        const float panel_w =
            layout.wide ? (content_w - gap) * 0.5f : ImGui::GetContentRegionAvail().x;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
        ImGui::BeginChild("##controls", ImVec2(panel_w, 0),
                          ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
        ImGui::TextUnformatted("Controls");
        ImGui::Separator();
        if (retro_button("Reset")) app.reset_simulation();
        same_line_or_wrap();
        if (retro_button("Undo")) app.undo();
        same_line_or_wrap();
        if (retro_button("Redo")) app.redo();
        same_line_or_wrap();
        const bool can_back = app.simulator.current_step() > 0;
        const bool can_fwd = app.simulator.current_step() < app.simulator.total_steps();
        if (retro_button("<< Step", can_back)) {
            app.simulator.step_back();
            app.shot_histogram.clear();
        }
        same_line_or_wrap();
        if (retro_button("Step >>", can_fwd)) {
            app.simulator.step_forward();
            app.shot_histogram.clear();
        }
        same_line_or_wrap();
        if (retro_button("Run All")) {
            app.simulator.run_all();
            app.shot_histogram.clear();
            app.status_message = "Ran full circuit.";
        }
        same_line_or_wrap();
        if (ImGui::Checkbox("Auto-run on edit", &app.auto_run_on_edit)) {
            app.settings.auto_run_on_edit = app.auto_run_on_edit;
            if (app.auto_run_on_edit) {
                app.simulator.run_all();
            }
        }
        same_line_or_wrap();
        if (ImGui::Checkbox("Collapse on measure", &app.collapse_on_measure)) {
            app.settings.collapse_on_measure = app.collapse_on_measure;
            app.simulator.set_collapse_on_measure(app.collapse_on_measure);
            app.sync_simulator();
        }
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::InputInt("Shots", &app.shot_count, 256, 1024)) {
            app.shot_count = std::clamp(app.shot_count, 1, 100000);
            app.settings.shot_count = app.shot_count;
            app.shot_histogram.clear();
        }
        ImGui::SameLine();
        if (retro_button("Sample")) app.run_shots();
        if (retro_button("Import")) app.import_dialog = ImportDialog::Json;
        same_line_or_wrap();
        if (retro_button("QASM In")) app.import_dialog = ImportDialog::Qasm;
        same_line_or_wrap();
        if (retro_button("Export")) {
            if (auto path = native_save_file_dialog("Export Circuit JSON", "circuit_export.json")) {
                app.export_json(*path);
            }
        }
        same_line_or_wrap();
        if (retro_button("QASM Out")) {
            if (auto path = native_save_qasm_dialog()) app.export_qasm(*path);
        }
        same_line_or_wrap();
        if (retro_button("PNG")) {
            if (auto path = native_save_png_dialog()) app.export_circuit_png(*path);
        }
        same_line_or_wrap();
        if (retro_button("Copy QASM")) app.copy_qasm_to_clipboard(window);
        same_line_or_wrap();
        if (retro_button("Save Preset")) app.show_save_preset = true;
        same_line_or_wrap();
        if (retro_button("Browse")) app.show_preset_browser = true;
        ImGui::EndChild();

        if (layout.wide) {
            ImGui::SameLine(0, gap);
        }
        ImGui::BeginChild("##presets", ImVec2(panel_w, 0),
                          ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
        ImGui::TextUnformatted("Quick Presets");
        ImGui::Separator();
        const auto all_presets = app.list_all_presets();
        std::map<std::string, std::vector<PresetEntry>> by_category;
        for (const auto& pe : all_presets) {
            if (pe.user_preset) continue;
            by_category[pe.category].push_back(pe);
        }
        static const char* kCategoryOrder[] = {"Basics", "Algorithms", "Advanced", "Research",
                                               "Uncategorized"};
        for (const char* cat : kCategoryOrder) {
            const auto it = by_category.find(cat);
            if (it == by_category.end() || it->second.empty()) continue;
            if (ImGui::TreeNode(cat)) {
                bool first = true;
                for (const auto& pe : it->second) {
                    if (!first) {
                        if (ImGui::GetContentRegionAvail().x < 72.0f) {
                            ImGui::NewLine();
                        } else {
                            ImGui::SameLine(0, 4);
                        }
                    }
                    if (retro_button(preset_label(pe.name))) app.try_load_circuit_file(pe.path);
                    first = false;
                }
                ImGui::TreePop();
            }
        }
        ImGui::TextUnformatted("Recent:");
        if (app.settings.recent_files.empty()) {
            ImGui::TextDisabled("(none yet)");
        } else {
            namespace fs = std::filesystem;
            for (std::size_t i = 0; i < std::min(app.settings.recent_files.size(), std::size_t(3)); ++i) {
                const auto& path = app.settings.recent_files[i];
                const auto name = fs::path(path).filename().string();
                if (ImGui::SmallButton(name.c_str())) app.try_load_circuit_file(path);
                if (ImGui::IsItemHovered()) {
                    begin_retro_tooltip();
                    ImGui::TextUnformatted(path.c_str());
                    end_retro_tooltip();
                }
                ImGui::SameLine(0, 4);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    const float analysis_max_h = std::max(100.0f, layout.scroll_h * 0.28f);

    auto count_visible_states = [&]() {
        const int dim = 1 << app.circuit.num_qubits;
        int count = 0;
        for (int i = 0; i < dim; ++i) {
            if (!app.filter_probs ||
                app.simulator.state().probability(i) >= kProbFilterThreshold) {
                ++count;
            }
        }
        return count;
    };

    auto scroll_panel_height = [&](int rows, float row_h, float min_h = 48.0f) {
        const float needed = rows > 0 ? row_h * static_cast<float>(rows) + 8.0f : min_h;
        return std::min(analysis_max_h, std::max(min_h, needed));
    };

    auto draw_state_compare = [&]() {
        begin_fieldset("State Compare (before / after)");
        const int step = app.simulator.current_step();
        const int total = app.simulator.total_steps();
        if (total == 0) {
            ImGui::TextDisabled("Place gates and use Step >> to compare states.");
        } else if (step == 0) {
            ImGui::TextDisabled("At initial state — step forward to see the effect of the next gate.");
            ImGui::Text("Current: %s", initial_state_label(app.initial_state));
        } else {
            const qsim::StateVector* before = app.simulator.checkpoint_state(step - 1);
            const qsim::StateVector& after = app.simulator.state();
            ImGui::Columns(2, "##state_compare", false);
            ImGui::TextUnformatted("Before");
            if (before) {
                draw_top_states(*before, app.circuit.num_qubits, app.filter_probs, 6);
            }
            ImGui::NextColumn();
            ImGui::TextUnformatted("After");
            draw_top_states(after, app.circuit.num_qubits, app.filter_probs, 6);
            ImGui::Columns(1);
        }
        end_fieldset();
    };

    auto draw_shot_histogram = [&]() {
        begin_fieldset("Multi-shot histogram");
        if (app.shot_histogram.empty()) {
            ImGui::TextDisabled("Click Sample to run %d shots from the current final state.",
                                 app.shot_count);
        } else {
            int max_count = 1;
            for (const auto& [idx, count] : app.shot_histogram) {
                max_count = std::max(max_count, count);
            }
            const float bar_w = std::max(80.0f, ImGui::GetContentRegionAvail().x - 130.0f);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            int shown = 0;
            for (const auto& [idx, count] : app.shot_histogram) {
                if (shown >= 16) break;
                const auto label = qsim::basis_label(idx, app.circuit.num_qubits);
                const double frac = static_cast<double>(count) / app.shot_count;
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(56.0f);
                const ImVec2 bar_pos = ImGui::GetCursorScreenPos();
                draw_prob_bar(dl, bar_pos, bar_w, 12.0f, frac);
                ImGui::Dummy(ImVec2(bar_w, 12.0f));
                ImGui::SameLine();
                ImGui::Text("%d (%.1f%%)", count, frac * 100.0);
                ++shown;
            }
            if (static_cast<int>(app.shot_histogram.size()) > shown) {
                ImGui::TextDisabled("... showing top %d outcomes", shown);
            }
        }
        end_fieldset();
    };

    auto draw_probabilities = [&]() {
        const int dim = 1 << app.circuit.num_qubits;
        begin_fieldset("Measurement Probabilities");
        if (ImGui::Checkbox("Hide states below 0.1%", &app.filter_probs)) {
            app.settings.filter_probs = app.filter_probs;
        }
        const int visible = count_visible_states();
        const float prob_h = scroll_panel_height(visible, 18.0f);
        ImGui::BeginChild("##prob_scroll", ImVec2(-1, prob_h), ImGuiChildFlags_Border);
        const float bar_w = std::max(80.0f, ImGui::GetContentRegionAvail().x - 120.0f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        int shown = 0;
        for (int i = 0; i < dim; ++i) {
            const double p = app.simulator.state().probability(i);
            if (app.filter_probs && p < kProbFilterThreshold) continue;
            ++shown;
            const auto label = qsim::basis_label(i, app.circuit.num_qubits);
            ImGui::Text("%s", label.c_str());
            ImGui::SameLine(56.0f);
            const ImVec2 bar_pos = ImGui::GetCursorScreenPos();
            draw_prob_bar(dl, bar_pos, bar_w, 14.0f, p);
            ImGui::Dummy(ImVec2(bar_w, 14.0f));
            ImGui::SameLine();
            ImGui::Text("%s", format_pct(p).c_str());
        }
        if (shown == 0) {
            ImGui::TextDisabled("(no states above 0.1%% — disable filter to see all)");
        }
        ImGui::EndChild();
        end_fieldset();
    };

    auto draw_state_vector = [&]() {
        begin_fieldset("State Vector |psi>");
        const int visible = count_visible_states();
        const float state_h = scroll_panel_height(visible, 16.0f);
        ImGui::BeginChild("##state_scroll", ImVec2(-1, state_h), ImGuiChildFlags_Border);
        const ImVec2 box = ImGui::GetCursorScreenPos();
        const float box_w = ImGui::GetContentRegionAvail().x;
        const float box_h = ImGui::GetContentRegionAvail().y;
        draw_inset_rect(ImGui::GetWindowDrawList(), box, {box.x + box_w, box.y + box_h},
                        IM_COL32(0, 0, 0, 255));
        ImGui::SetCursorScreenPos({box.x + 6.0f, box.y + 4.0f});

        const auto& amps = app.simulator.state().amplitudes();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        for (std::size_t i = 0; i < amps.size(); ++i) {
            if (app.filter_probs && std::norm(amps[i]) < kProbFilterThreshold) continue;
            const auto label = qsim::basis_label(static_cast<int>(i), app.circuit.num_qubits);
            ImGui::Text("%s  %s", format_amplitude(amps[i]).c_str(), label.c_str());
        }
        ImGui::PopStyleColor();
        ImGui::EndChild();
        end_fieldset();
    };

    auto draw_qubit_viz = [&]() {
        begin_fieldset("Qubit Visualization");
        const auto views = app.simulator.qubit_views();
        const float bar_w = std::max(80.0f, ImGui::GetContentRegionAvail().x - 140.0f);
        for (int q = 0; q < app.circuit.num_qubits; ++q) {
            const auto& v = views[static_cast<std::size_t>(q)];
            char row_label[8];
            std::snprintf(row_label, sizeof(row_label), "q%d", q);
            ImGui::Text("%s", row_label);
            ImGui::SameLine(36.0f);

            const ImVec2 bloch_pos = ImGui::GetCursorScreenPos();
            const float bloch_r = 26.0f;
            const ImVec2 center = {bloch_pos.x + bloch_r + 4.0f, bloch_pos.y + bloch_r + 2.0f};
            draw_bloch(ImGui::GetWindowDrawList(), center, bloch_r, v);
            char zlab[24];
            std::snprintf(zlab, sizeof(zlab), "z = %+.2f", v.bloch_z);
            ImGui::GetWindowDrawList()->AddText({center.x - 22.0f, center.y + bloch_r + 4.0f}, kBlack,
                                                zlab);
            ImGui::Dummy(ImVec2(bloch_r * 2.0f + 12.0f, bloch_r * 2.0f + 16.0f));

            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::Text("|0> %s", format_pct(v.prob_zero).c_str());
            draw_prob_bar(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), bar_w, 12.0f,
                          v.prob_zero);
            ImGui::Dummy(ImVec2(bar_w, 12.0f));
            ImGui::Text("|1> %s", format_pct(v.prob_one).c_str());
            draw_prob_bar(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), bar_w, 12.0f,
                          v.prob_one);
            ImGui::Dummy(ImVec2(bar_w, 12.0f));
            ImGui::EndGroup();
        }
        end_fieldset();
    };

    if (layout.wide) {
        const float gap = 6.0f;
        ImGui::Columns(2, "##analysis", false);
        ImGui::SetColumnWidth(0, (content_w - gap) * 0.52f);
        draw_probabilities();
        draw_state_vector();
        ImGui::NextColumn();
        draw_qubit_viz();
        ImGui::Columns(1);
    } else {
        draw_probabilities();
        draw_state_vector();
        draw_qubit_viz();
    }

    draw_state_compare();
    draw_shot_histogram();

    // Status bar
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = 22.0f;
        const float status_w = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        draw_inset_rect(dl, p, {p.x + status_w, p.y + h}, kWhite);

        const int gate_count = app.circuit.gate_count();
        const int depth = app.circuit.depth();
        const char* hint = app.pending_qubit.has_value() || app.pending_qubit2.has_value() ||
                                   app.eraser_mode
                               ? app.pending_message().c_str()
                               : app.status_message.c_str();

        std::time_t now = std::time(nullptr);
        char status[512];
        std::snprintf(status, sizeof(status),
                      "Gates:%d Depth:%d | Step %d/%d | t1-t%d | Auto-run:%s | %s | %s",
                      gate_count, depth, app.simulator.current_step(), app.simulator.total_steps(),
                      app.time_columns(), app.auto_run_on_edit ? "on" : "off", hint, std::ctime(&now));
        status[sizeof(status) - 1] = '\0';
        if (char* nl = std::strchr(status, '\n')) *nl = '\0';

        dl->AddText({p.x + 6.0f, p.y + 3.0f}, kBlack, status);
        ImGui::Dummy(ImVec2(status_w, h + 2.0f));
    }

    draw_about_modal(app);
    draw_preset_browser(app);
    draw_save_preset_modal(app);
    draw_import_modal(app);

    ImGui::EndGroup();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << '\n';
}

int run_app(int argc, char** argv) {
    if (argc >= 3 && std::strcmp(argv[1], "--write-icon") == 0) {
        return export_app_icon_png(argv[2]) ? 0 : 1;
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (const char* use_x11 = std::getenv("QUANTUM_LAB_USE_X11"); use_x11 && use_x11[0] == '1') {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    }
    if (!glfwInit()) return 1;

    setup_linux_desktop_integration();

    AppSettings settings;
    settings.load();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "quantum-lab");

    GLFWwindow* window = glfwCreateWindow(settings.window_width, settings.window_height,
                                          "Quantum Circuit Lab", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    set_window_icon(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsClassic();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    AppState app;
    app_init(app, resolve_presets_dir(argc, argv));

    bool icon_refreshed = false;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (!icon_refreshed) {
            set_window_icon(window);
            icon_refreshed = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app_frame(app, window);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.753f, 0.753f, 0.753f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    int w, h;
    glfwGetWindowSize(window, &w, &h);
    app_shutdown(app, w, h);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

int main(int argc, char** argv) { return run_app(argc, argv); }
