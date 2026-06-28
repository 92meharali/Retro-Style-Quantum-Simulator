#include "util.hpp"

#include "qsim/gates.hpp"

#include <GLFW/glfw3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#else
#include <cstdlib>
#include <limits.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
std::string exe_dir() {
    char buf[MAX_PATH];
    const DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return fs::current_path().string();
    return fs::path(buf).parent_path().string();
}
#else
std::string exe_dir() {
    char buf[PATH_MAX];
    const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return fs::current_path().string();
    buf[len] = '\0';
    return fs::path(buf).parent_path().string();
}
#endif

void put_rgba(std::vector<std::uint8_t>& img, int w, int h, int x, int y, std::uint8_t r,
              std::uint8_t g, std::uint8_t b) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    const int i = (y * w + x) * 4;
    img[static_cast<std::size_t>(i + 0)] = r;
    img[static_cast<std::size_t>(i + 1)] = g;
    img[static_cast<std::size_t>(i + 2)] = b;
    img[static_cast<std::size_t>(i + 3)] = 255;
}

std::vector<std::uint8_t> render_phi_icon(int size) {
    std::vector<std::uint8_t> img(static_cast<std::size_t>(size * size * 4), 0);
    const float cx = (size - 1) * 0.5f;
    const float cy = (size - 1) * 0.5f;
    const float radius = size * 0.36f;
    const float line_half = size * 0.30f;
    const float stroke = std::max(1.2f, size * 0.075f);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            put_rgba(img, size, size, x, y, 48, 48, 48);
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float dist = std::hypot(px - cx, py - cy);
            const bool ring = std::abs(dist - radius) <= stroke;
            const bool stem = std::abs(px - cx) <= stroke && std::abs(py - cy) <= line_half;
            if (ring || stem) put_rgba(img, size, size, x, y, 255, 255, 255);
        }
    }
    return img;
}

#ifdef _WIN32
bool linux_style_filter(const char* filter) {
    return filter && std::strchr(filter, '|') != nullptr;
}

std::optional<std::string> win32_dialog(const char* title, const char* filter,
                                          const char* default_name, bool save) {
    char filename[MAX_PATH] = {};
    if (default_name && default_name[0]) std::snprintf(filename, sizeof(filename), "%s", default_name);

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    if (!(save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn))) return std::nullopt;
    return std::string(filename);
}
#else
std::optional<std::string> zenity_dialog(const char* args) {
    std::array<char, 4096> buffer{};
    const std::string cmd = std::string("zenity ") + args + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return std::nullopt;
    if (!fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        pclose(pipe);
        return std::nullopt;
    }
    pclose(pipe);
    std::string path(buffer.data());
    while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
    return path.empty() ? std::nullopt : std::optional<std::string>(path);
}
#endif

}  // namespace

std::string resolve_presets_dir(int argc, char** argv) {
    if (argc > 1 && argv[1][0]) return argv[1];
    const fs::path bundled = fs::path(exe_dir()) / "presets";
    return fs::is_directory(bundled) ? bundled.string() : "presets";
}

bool export_app_icon_png(const std::string& path, int size) {
    const auto rgba = render_phi_icon(size);
    return write_png_file(path, size, size, rgba);
}

void setup_linux_desktop_integration() {
#if defined(__linux__)
    fs::path bin;
    try {
        bin = fs::read_symlink("/proc/self/exe");
    } catch (const fs::filesystem_error&) {
        return;
    }
    const fs::path dir = bin.parent_path();
    const fs::path bundled = dir / "quantum-lab.png";
    if (!fs::exists(bundled)) export_app_icon_png(bundled.string(), 128);

    const char* home = std::getenv("HOME");
    if (!home) return;

    const fs::path icon_dir = fs::path(home) / ".local/share/icons/hicolor/128x128/apps";
    fs::create_directories(icon_dir);
    try {
        fs::copy_file(bundled, icon_dir / "quantum-lab.png", fs::copy_options::overwrite_existing);
    } catch (const fs::filesystem_error&) {
    }

    const fs::path apps_dir = fs::path(home) / ".local/share/applications";
    fs::create_directories(apps_dir);
    const fs::path presets = dir / "presets";
    const std::string presets_arg =
        fs::is_directory(presets) ? (" \"" + presets.string() + "\"") : std::string{};

    std::ofstream out(apps_dir / "quantum-lab.desktop");
    if (!out) return;
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=Quantum Circuit Lab\n"
        << "Comment=Interactive quantum circuit simulator\n"
        << "Exec=\"" << bin.string() << "\"" << presets_arg << "\n"
        << "Icon=quantum-lab\n"
        << "Terminal=false\n"
        << "Categories=Education;Science;\n"
        << "StartupWMClass=quantum-lab\n";

    const fs::path hicolor = fs::path(home) / ".local/share/icons/hicolor";
    const std::string cache_cmd =
        "gtk-update-icon-cache -f -t \"" + hicolor.string() + "\" 2>/dev/null";
    std::system(cache_cmd.c_str());
    const std::string db_cmd =
        "update-desktop-database \"" + apps_dir.string() + "\" 2>/dev/null";
    std::system(db_cmd.c_str());
#endif
}

void set_window_icon(GLFWwindow* window) {
    if (!window) return;
    static std::vector<std::uint8_t> s16, s32, s48, s64, s128;
    static bool ready = false;
    if (!ready) {
        s16 = render_phi_icon(16);
        s32 = render_phi_icon(32);
        s48 = render_phi_icon(48);
        s64 = render_phi_icon(64);
        s128 = render_phi_icon(128);
        ready = true;
    }
    GLFWimage images[5] = {{16, 16, s16.data()},  {32, 32, s32.data()},  {48, 48, s48.data()},
                           {64, 64, s64.data()}, {128, 128, s128.data()}};
    glfwSetWindowIcon(window, 5, images);
}

void draw_phi_logo(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, float thickness) {
    dl->AddCircle(center, radius, color, 0, thickness);
    const float stem = radius * 0.88f;
    dl->AddLine({center.x, center.y - stem}, {center.x, center.y + stem}, color, thickness);
}

fs::path AppSettings::config_dir() {
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA")) return fs::path(appdata) / "quantum-lab";
    return fs::path("quantum-lab");
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) return fs::path(xdg) / "quantum-lab";
    if (const char* home = std::getenv("HOME")) return fs::path(home) / ".config" / "quantum-lab";
    return fs::path(".config") / "quantum-lab";
#endif
}

fs::path AppSettings::user_presets_dir() { return config_dir() / "presets"; }
fs::path AppSettings::settings_path() { return config_dir() / "settings.json"; }

void AppSettings::ensure_dirs() const {
    fs::create_directories(config_dir());
    fs::create_directories(user_presets_dir());
}

void AppSettings::load() {
    ensure_dirs();
    std::ifstream in(settings_path());
    if (!in) return;
    try {
        const auto j = nlohmann::json::parse(in);
        if (j.contains("filter_probs")) filter_probs = j["filter_probs"].get<bool>();
        if (j.contains("auto_run_on_edit")) auto_run_on_edit = j["auto_run_on_edit"].get<bool>();
        if (j.contains("collapse_on_measure")) collapse_on_measure = j["collapse_on_measure"].get<bool>();
        if (j.contains("initial_state")) initial_state = std::clamp(j["initial_state"].get<int>(), 0, 2);
        if (j.contains("shot_count")) shot_count = std::clamp(j["shot_count"].get<int>(), 1, 100000);
        if (j.contains("window_width")) window_width = std::clamp(j["window_width"].get<int>(), 640, 4096);
        if (j.contains("window_height")) window_height = std::clamp(j["window_height"].get<int>(), 480, 4096);
        if (j.contains("recent_files")) {
            recent_files.clear();
            for (const auto& entry : j["recent_files"]) {
                const auto path = entry.get<std::string>();
                if (fs::exists(path)) recent_files.push_back(path);
            }
        }
    } catch (const std::exception&) {
    }
}

void AppSettings::save() const {
    ensure_dirs();
    nlohmann::json j;
    j["version"] = 1;
    j["filter_probs"] = filter_probs;
    j["auto_run_on_edit"] = auto_run_on_edit;
    j["collapse_on_measure"] = collapse_on_measure;
    j["initial_state"] = initial_state;
    j["shot_count"] = shot_count;
    j["window_width"] = window_width;
    j["window_height"] = window_height;
    j["recent_files"] = recent_files;
    std::ofstream out(settings_path());
    if (out) out << j.dump(2);
}

void AppSettings::add_recent_file(const std::string& path) {
    if (path.empty()) return;
    recent_files.erase(std::remove(recent_files.begin(), recent_files.end(), path), recent_files.end());
    recent_files.insert(recent_files.begin(), path);
    if (recent_files.size() > 10) recent_files.resize(10);
}

void open_external_url(const char* url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#else
    std::system((std::string("xdg-open '") + url + "' >/dev/null 2>&1").c_str());
#endif
}

std::optional<std::string> native_open_file_dialog(const char* title, const char* filter) {
#ifdef _WIN32
    const char* f = linux_style_filter(filter) ? "JSON Files\0*.json\0All Files (*.*)\0*.*\0\0" : filter;
    return win32_dialog(title, f, nullptr, false);
#else
    return zenity_dialog((std::string("--file-selection --title=\"") + title + "\" --file-filter=\"" +
                          filter + "\"")
                             .c_str());
#endif
}

std::optional<std::string> native_save_file_dialog(const char* title, const char* default_name,
                                                   const char* filter) {
#ifdef _WIN32
    const char* f = linux_style_filter(filter) ? "JSON Files\0*.json\0All Files (*.*)\0*.*\0\0" : filter;
    return win32_dialog(title, f, default_name, true);
#else
    return zenity_dialog((std::string("--file-selection --save --confirm-overwrite --title=\"") +
                          title + "\" --filename=\"" + default_name + "\" --file-filter=\"" + filter + "\"")
                             .c_str());
#endif
}

std::optional<std::string> native_open_qasm_dialog() {
#ifdef _WIN32
    return win32_dialog("Import OpenQASM 2.0", "OpenQASM (*.qasm)\0*.qasm\0All Files (*.*)\0*.*\0\0",
                        nullptr, false);
#else
    return native_open_file_dialog("Import OpenQASM 2.0", "OpenQASM (*.qasm)|*.qasm");
#endif
}

std::optional<std::string> native_save_qasm_dialog() {
#ifdef _WIN32
    return win32_dialog("Export OpenQASM 2.0", "OpenQASM (*.qasm)\0*.qasm\0All Files (*.*)\0*.*\0\0",
                        "circuit.qasm", true);
#else
    return native_save_file_dialog("Export OpenQASM 2.0", "circuit.qasm", "OpenQASM (*.qasm)|*.qasm");
#endif
}

std::optional<std::string> native_save_png_dialog() {
#ifdef _WIN32
    return win32_dialog("Export Circuit PNG", "PNG (*.png)\0*.png\0All Files (*.*)\0*.*\0\0",
                        "circuit.png", true);
#else
    return native_save_file_dialog("Export Circuit PNG", "circuit.png", "PNG (*.png)|*.png");
#endif
}

namespace {

constexpr int kPngCellW = 58;
constexpr int kPngCellH = 36;
constexpr int kPngLabelW = 36;
constexpr int kPngHeaderH = 28;
constexpr int kPngPad = 8;

struct Rgba {
    std::uint8_t r, g, b, a;
};

inline Rgba png_gray() { return {192, 192, 192, 255}; }
inline Rgba png_white() { return {255, 255, 255, 255}; }
inline Rgba png_black() { return {0, 0, 0, 255}; }
inline Rgba png_measure() { return {0, 0, 160, 255}; }

void png_put(std::vector<std::uint8_t>& img, int w, int h, int x, int y, Rgba c) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    const int i = (y * w + x) * 4;
    img[static_cast<std::size_t>(i + 0)] = c.r;
    img[static_cast<std::size_t>(i + 1)] = c.g;
    img[static_cast<std::size_t>(i + 2)] = c.b;
    img[static_cast<std::size_t>(i + 3)] = c.a;
}

void png_fill(std::vector<std::uint8_t>& img, int iw, int ih, int x0, int y0, int x1, int y1, Rgba c) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) png_put(img, iw, ih, x, y, c);
}

void png_inset(std::vector<std::uint8_t>& img, int iw, int ih, int x0, int y0, int x1, int y1, Rgba fill) {
    png_fill(img, iw, ih, x0, y0, x1, y1, fill);
    for (int x = x0; x < x1; ++x) {
        png_put(img, iw, ih, x, y0, png_black());
        png_put(img, iw, ih, x, y1 - 1, png_white());
    }
    for (int y = y0; y < y1; ++y) {
        png_put(img, iw, ih, x0, y, png_black());
        png_put(img, iw, ih, x1 - 1, y, png_white());
    }
}

void png_line(std::vector<std::uint8_t>& img, int iw, int ih, int x0, int y0, int x1, int y1, Rgba c,
              int thickness = 1) {
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int steps = std::max(dx, dy);
    if (steps == 0) return;
    for (int i = 0; i <= steps; ++i) {
        const int x = x0 + (x1 - x0) * i / steps;
        const int y = y0 + (y1 - y0) * i / steps;
        for (int ty = -thickness / 2; ty <= thickness / 2; ++ty)
            for (int tx = -thickness / 2; tx <= thickness / 2; ++tx)
                png_put(img, iw, ih, x + tx, y + ty, c);
    }
}

void png_circle(std::vector<std::uint8_t>& img, int iw, int ih, int cx, int cy, int r, Rgba c) {
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x) {
            const int d2 = x * x + y * y;
            if (d2 >= (r - 1) * (r - 1) && d2 <= (r + 1) * (r + 1)) png_put(img, iw, ih, cx + x, cy + y, c);
        }
}

bool png_glyph(char ch, int row, unsigned char& bits) {
    bits = 0;
    if (row < 0 || row >= 7) return false;
    struct Glyph {
        char c;
        unsigned char rows[7];
    };
    static const Glyph glyphs[] = {
        {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}}, {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
        {'2', {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F}}, {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
        {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}}, {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
        {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}}, {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
        {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}}, {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
        {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}}, {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
        {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}}, {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
        {'S', {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}}, {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
        {'M', {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}}, {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
        {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}}, {'+', {0x04, 0x04, 0x1F, 0x04, 0x04, 0x00, 0x00}},
        {'|', {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}}, {'>', {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10}},
        {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}}, {'q', {0x00, 0x00, 0x0E, 0x11, 0x11, 0x0F, 0x01}},
        {'t', {0x08, 0x08, 0x1C, 0x08, 0x08, 0x09, 0x06}},
    };
    for (const auto& g : glyphs) {
        if (g.c == ch) {
            bits = g.rows[row];
            return true;
        }
    }
    return false;
}

void png_text(std::vector<std::uint8_t>& img, int iw, int ih, int x, int y, const char* text, Rgba c,
              int scale = 2) {
    int cx = x;
    for (const char* p = text; *p; ++p) {
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = 0;
            if (!png_glyph(*p, row, bits)) continue;
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    png_fill(img, iw, ih, cx + col * scale, y + row * scale, cx + (col + 1) * scale,
                             y + (row + 1) * scale, c);
                }
            }
        }
        cx += 6 * scale;
    }
}

void png_cnot(std::vector<std::uint8_t>& img, int iw, int ih, int cx, int y0, int y1) {
    png_line(img, iw, ih, cx, y0, cx, y1, png_black(), 2);
    png_circle(img, iw, ih, cx, y0, 5, png_black());
    png_line(img, iw, ih, cx - 7, y1, cx + 7, y1, png_black(), 2);
    png_line(img, iw, ih, cx, y1 - 7, cx, y1 + 7, png_black(), 2);
}

void png_cz(std::vector<std::uint8_t>& img, int iw, int ih, int cx, int y0, int y1) {
    png_line(img, iw, ih, cx, y0, cx, y1, png_black(), 2);
    png_circle(img, iw, ih, cx, y0, 5, png_black());
    png_circle(img, iw, ih, cx, y1, 5, png_black());
}

void png_ccx(std::vector<std::uint8_t>& img, int iw, int ih, int cx, int yc1, int yc2, int yt) {
    const int y_top = std::min({yc1, yc2, yt});
    const int y_bot = std::max({yc1, yc2, yt});
    png_line(img, iw, ih, cx, y_top, cx, y_bot, png_black(), 2);
    png_circle(img, iw, ih, cx, yc1, 5, png_black());
    png_circle(img, iw, ih, cx, yc2, 5, png_black());
    png_line(img, iw, ih, cx - 7, yt, cx + 7, yt, png_black(), 2);
    png_line(img, iw, ih, cx, yt - 7, cx, yt + 7, png_black(), 2);
}

void png_measure(std::vector<std::uint8_t>& img, int iw, int ih, int cx, int cy) {
    png_inset(img, iw, ih, cx - 12, cy - 12, cx + 12, cy + 12, png_white());
    for (int y = cy - 12; y < cy + 12; ++y) {
        png_put(img, iw, ih, cx - 12, y, png_measure());
        png_put(img, iw, ih, cx + 11, y, png_measure());
    }
    for (int x = cx - 12; x < cx + 12; ++x) {
        png_put(img, iw, ih, x, cy - 12, png_measure());
        png_put(img, iw, ih, x, cy + 11, png_measure());
    }
    png_text(img, iw, ih, cx - 4, cy - 8, "M", png_measure(), 1);
}

}  // namespace

std::vector<std::uint8_t> render_circuit_rgba(const qsim::Circuit& circuit, int min_columns, int& out_w,
                                              int& out_h) {
    const int n = circuit.num_qubits;
    const int cols = std::max(min_columns, circuit.num_columns() + 2);
    out_w = kPngPad * 2 + kPngLabelW + cols * kPngCellW;
    out_h = kPngPad * 2 + kPngHeaderH + n * kPngCellH;

    std::vector<std::uint8_t> img(static_cast<std::size_t>(out_w * out_h * 4));
    png_fill(img, out_w, out_h, 0, 0, out_w, out_h, png_gray());

    const int ox = kPngPad;
    const int oy = kPngPad;
    png_inset(img, out_w, out_h, ox, oy, ox + kPngLabelW, oy + kPngHeaderH, png_gray());
    for (int c = 0; c < cols; ++c) {
        const int x = ox + kPngLabelW + c * kPngCellW;
        png_inset(img, out_w, out_h, x, oy, x + kPngCellW, oy + kPngHeaderH, png_gray());
        char hdr[8];
        std::snprintf(hdr, sizeof(hdr), "t%d", c + 1);
        png_text(img, out_w, out_h, x + 16, oy + 8, hdr, png_black(), 1);
    }

    std::unordered_set<int> drawn;
    for (const auto& op : circuit.ops) {
        if (drawn.count(op.id)) continue;
        if (op.column < 0 || op.column >= cols) continue;
        const int cx = ox + kPngLabelW + op.column * kPngCellW + kPngCellW / 2;
        auto row_cy = [&](int q) { return oy + kPngHeaderH + q * kPngCellH + kPngCellH / 2; };
        switch (op.kind) {
            case qsim::GateKind::CNOT:
                png_cnot(img, out_w, out_h, cx, row_cy(op.qubit), row_cy(op.qubit2));
                drawn.insert(op.id);
                break;
            case qsim::GateKind::CZ:
                png_cz(img, out_w, out_h, cx, row_cy(op.qubit), row_cy(op.qubit2));
                drawn.insert(op.id);
                break;
            case qsim::GateKind::CCX:
                png_ccx(img, out_w, out_h, cx, row_cy(op.qubit), row_cy(op.qubit2), row_cy(op.qubit3));
                drawn.insert(op.id);
                break;
            default:
                break;
        }
    }

    for (int q = 0; q < n; ++q) {
        const int row_y = oy + kPngHeaderH + q * kPngCellH;
        png_inset(img, out_w, out_h, ox, row_y, ox + kPngLabelW, row_y + kPngCellH, png_gray());
        char qlab[8];
        std::snprintf(qlab, sizeof(qlab), "q%d", q);
        png_text(img, out_w, out_h, ox + 8, row_y + 12, qlab, png_black(), 1);

        for (int c = 0; c < cols; ++c) {
            const int x = ox + kPngLabelW + c * kPngCellW;
            png_inset(img, out_w, out_h, x, row_y, x + kPngCellW, row_y + kPngCellH, png_white());
            const auto op = circuit.op_at(q, c);
            if (!op || op->kind == qsim::GateKind::Barrier) continue;
            const int cx = x + kPngCellW / 2;
            const int cy = row_y + kPngCellH / 2;
            switch (op->kind) {
                case qsim::GateKind::CNOT:
                case qsim::GateKind::CZ:
                case qsim::GateKind::CCX:
                    break;
                case qsim::GateKind::Measure:
                    png_measure(img, out_w, out_h, cx, cy);
                    break;
                default:
                    png_text(img, out_w, out_h, cx - 6, cy - 8, qsim::gate_info(op->kind).symbol, png_black(), 1);
                    break;
            }
        }
    }
    return img;
}

bool write_png_file(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgba) {
    if (w <= 0 || h <= 0 || rgba.empty()) return false;
    return stbi_write_png(path.c_str(), w, h, 4, rgba.data(), w * 4) != 0;
}
