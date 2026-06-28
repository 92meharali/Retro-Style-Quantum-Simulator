#pragma once

#include "imgui.h"
#include "qsim/circuit.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct AppSettings {
    bool filter_probs = false;
    bool auto_run_on_edit = true;
    bool collapse_on_measure = false;
    int initial_state = 0;
    int shot_count = 1024;
    int window_width = 820;
    int window_height = 920;
    std::vector<std::string> recent_files;

    static std::filesystem::path config_dir();
    static std::filesystem::path user_presets_dir();
    static std::filesystem::path settings_path();
    void load();
    void save() const;
    void add_recent_file(const std::string& path);
    void ensure_dirs() const;
};

struct GLFWwindow;

std::string resolve_presets_dir(int argc, char** argv);
bool export_app_icon_png(const std::string& path, int size = 128);
void setup_linux_desktop_integration();
void set_window_icon(GLFWwindow* window);
void draw_phi_logo(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, float thickness = 2.0f);

void open_external_url(const char* url);
std::optional<std::string> native_open_file_dialog(const char* title = "Open Circuit JSON",
                                                   const char* filter = "JSON (*.json)|*.json");
std::optional<std::string> native_save_file_dialog(const char* title = "Save Circuit JSON",
                                                   const char* default_name = "circuit.json",
                                                   const char* filter = "JSON (*.json)|*.json");
std::optional<std::string> native_open_qasm_dialog();
std::optional<std::string> native_save_qasm_dialog();
std::optional<std::string> native_save_png_dialog();

std::vector<std::uint8_t> render_circuit_rgba(const qsim::Circuit& circuit, int min_columns,
                                              int& out_w, int& out_h);
bool write_png_file(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgba);
