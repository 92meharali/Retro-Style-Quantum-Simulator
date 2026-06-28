#include "qsim/qasm_io.hpp"

#include "qsim/gates.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace qsim {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string strip_comment(std::string line) {
    const std::size_t pos = line.find("//");
    if (pos != std::string::npos) {
        line = line.substr(0, pos);
    }
    return trim(line);
}

std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

double parse_angle_token(std::string token) {
    token = trim(token);
    token = to_lower(token);
    if (token.empty()) throw std::invalid_argument("empty angle");

    if (token == "pi") return kPi;
    if (token == "-pi") return -kPi;
    if (token == "pi/2") return kPi / 2.0;
    if (token == "-pi/2") return -kPi / 2.0;
    if (token == "pi/4") return kPi / 4.0;
    if (token == "-pi/4") return -kPi / 4.0;
    if (token == "2*pi" || token == "2pi") return 2.0 * kPi;
    if (token == "-2*pi" || token == "-2pi") return -2.0 * kPi;

    std::size_t idx = 0;
    const double v = std::stod(token, &idx);
    if (idx != token.size()) {
        throw std::invalid_argument("invalid angle: " + token);
    }
    return v;
}

double parse_angle_expr(const std::string& expr) {
    std::string inner = trim(expr);
    if (!inner.empty() && inner.front() == '(' && inner.back() == ')') {
        inner = trim(inner.substr(1, inner.size() - 2));
    }
    return parse_angle_token(inner);
}

int parse_qubit_index(const std::string& token) {
    const std::string t = trim(token);
    if (t.size() < 4 || t[0] != 'q' || t[1] != '[' || t.back() != ']') {
        throw std::invalid_argument("expected q[i], got: " + token);
    }
    return std::stoi(t.substr(2, t.size() - 3));
}

std::vector<std::string> split_args(const std::string& args) {
    std::vector<std::string> parts;
    std::string current;
    int depth = 0;
    for (char c : args) {
        if (c == '(') {
            ++depth;
            current.push_back(c);
        } else if (c == ')') {
            --depth;
            current.push_back(c);
        } else if (c == ',' && depth == 0) {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) parts.push_back(trim(current));
    return parts;
}

bool is_supported_qasm_gate(GateKind kind) {
    switch (kind) {
        case GateKind::H:
        case GateKind::X:
        case GateKind::Y:
        case GateKind::Z:
        case GateKind::CNOT:
        case GateKind::CZ:
        case GateKind::SWAP:
        case GateKind::Rx:
        case GateKind::Ry:
        case GateKind::Rz:
            return true;
        default:
            return false;
    }
}

void emit_qasm_gate(std::ostringstream& out, const CircuitOp& op) {
    switch (op.kind) {
        case GateKind::H:
            out << "h q[" << op.qubit << "];\n";
            break;
        case GateKind::X:
            out << "x q[" << op.qubit << "];\n";
            break;
        case GateKind::Y:
            out << "y q[" << op.qubit << "];\n";
            break;
        case GateKind::Z:
            out << "z q[" << op.qubit << "];\n";
            break;
        case GateKind::CNOT:
            out << "cx q[" << op.qubit << "],q[" << op.qubit2 << "];\n";
            break;
        case GateKind::CZ:
            out << "cz q[" << op.qubit << "],q[" << op.qubit2 << "];\n";
            break;
        case GateKind::SWAP:
            out << "swap q[" << op.qubit << "],q[" << op.qubit2 << "];\n";
            break;
        case GateKind::Rx:
            out << "rx(" << op.param1 << ") q[" << op.qubit << "];\n";
            break;
        case GateKind::Ry:
            out << "ry(" << op.param1 << ") q[" << op.qubit << "];\n";
            break;
        case GateKind::Rz:
            out << "rz(" << op.param1 << ") q[" << op.qubit << "];\n";
            break;
        default:
            break;
    }
}

CircuitOp parse_gate_line(const std::string& line, int column, int& next_id) {
    const std::string stmt = strip_comment(line);
    if (stmt.empty()) throw std::invalid_argument("empty statement");

    const std::size_t paren = stmt.find('(');
    const std::size_t space = stmt.find(' ');
    const bool has_angle = paren != std::string::npos && (space == std::string::npos || paren < space);

    if (has_angle) {
        const std::size_t close = stmt.find(')', paren);
        if (close == std::string::npos) throw std::invalid_argument("unclosed angle paren");
        const std::string name = to_lower(trim(stmt.substr(0, paren)));
        const double angle = parse_angle_expr(stmt.substr(paren + 1, close - paren - 1));
        const std::string qpart = trim(stmt.substr(close + 1));
        std::string qtok = qpart;
        if (!qtok.empty() && qtok.back() == ';') qtok.pop_back();
        const int q = parse_qubit_index(qtok);

        GateKind kind = GateKind::I;
        if (name == "rx") kind = GateKind::Rx;
        else if (name == "ry") kind = GateKind::Ry;
        else if (name == "rz") kind = GateKind::Rz;
        else throw std::invalid_argument("unsupported parametric gate: " + name);

        return {kind, column, q, -1, -1, angle, 0.0, 0.0, next_id++};
    }

    const std::size_t semi = stmt.find(';');
    const std::string body = semi == std::string::npos ? stmt : stmt.substr(0, semi);
    const std::size_t sp = body.find(' ');
    if (sp == std::string::npos) throw std::invalid_argument("invalid gate line: " + line);

    const std::string name = to_lower(trim(body.substr(0, sp)));
    const auto args = split_args(body.substr(sp + 1));

    if (name == "h" || name == "x" || name == "y" || name == "z") {
        if (args.size() != 1) throw std::invalid_argument("single-qubit gate expects one qubit");
        GateKind kind = GateKind::I;
        if (name == "h") kind = GateKind::H;
        else if (name == "x") kind = GateKind::X;
        else if (name == "y") kind = GateKind::Y;
        else if (name == "z") kind = GateKind::Z;
        return {kind, column, parse_qubit_index(args[0]), -1, -1, 0.0, 0.0, 0.0, next_id++};
    }

    if (name == "cx" || name == "cnot" || name == "cz" || name == "swap") {
        if (args.size() != 2) throw std::invalid_argument("two-qubit gate expects two qubits");
        GateKind kind = GateKind::CNOT;
        if (name == "cz") kind = GateKind::CZ;
        else if (name == "swap") kind = GateKind::SWAP;
        return {kind, column, parse_qubit_index(args[0]), parse_qubit_index(args[1]), -1,
                0.0, 0.0, 0.0, next_id++};
    }

    throw std::invalid_argument("unsupported or unknown QASM gate: " + name);
}

int count_unsupported_qasm_gates_impl(const Circuit& circuit) {
    int count = 0;
    for (const auto& op : circuit.ops) {
        if (op.kind != GateKind::Barrier && !is_supported_qasm_gate(op.kind)) {
            ++count;
        }
    }
    return count;
}

}  // namespace

std::string circuit_to_qasm(const Circuit& circuit) {
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "OPENQASM 2.0;\n";
    out << "include \"qelib1.inc\";\n";
    out << "qreg q[" << circuit.num_qubits << "];\n";

    for (const auto& op : circuit.ordered_gates()) {
        if (!is_supported_qasm_gate(op.kind)) continue;
        emit_qasm_gate(out, op);
    }
    return out.str();
}

Circuit circuit_from_qasm(const std::string& qasm) {
    Circuit circuit;
    circuit.num_qubits = 1;
    int column = 0;
    int next_id = 1;

    std::istringstream in(qasm);
    std::string line;
    while (std::getline(in, line)) {
        const std::string stmt = strip_comment(line);
        if (stmt.empty()) continue;

        const std::string lower = to_lower(stmt);
        if (lower.rfind("openqasm", 0) == 0 || lower.rfind("include", 0) == 0 ||
            lower.rfind("creg", 0) == 0 || lower.rfind("measure", 0) == 0 ||
            lower.rfind("barrier", 0) == 0) {
            continue;
        }

        if (lower.rfind("qreg", 0) == 0) {
            const std::size_t lb = stmt.find('[');
            const std::size_t rb = stmt.find(']');
            if (lb == std::string::npos || rb == std::string::npos) {
                throw std::invalid_argument("invalid qreg line");
            }
            circuit.num_qubits = std::stoi(stmt.substr(lb + 1, rb - lb - 1));
            continue;
        }

        circuit.add_op(parse_gate_line(stmt, column, next_id));
        ++column;
    }

    return circuit;
}

Circuit load_qasm_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("failed to open QASM file: " + path);
    std::ostringstream ss;
    ss << file.rdbuf();
    return circuit_from_qasm(ss.str());
}

void save_qasm_file(const std::string& path, const Circuit& circuit) {
    std::ofstream file(path);
    if (!file) throw std::runtime_error("failed to write QASM file: " + path);
    file << circuit_to_qasm(circuit);
}

int count_unsupported_qasm_gates(const Circuit& circuit) {
    return count_unsupported_qasm_gates_impl(circuit);
}

}  // namespace qsim
