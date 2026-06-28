#include "qsim/json_io.hpp"

#include "qsim/gates.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace qsim {

namespace {

std::string gate_kind_to_string(GateKind kind) {
    switch (kind) {
        case GateKind::I: return "I";
        case GateKind::H: return "H";
        case GateKind::X: return "X";
        case GateKind::Y: return "Y";
        case GateKind::Z: return "Z";
        case GateKind::S: return "S";
        case GateKind::T: return "T";
        case GateKind::Sdg: return "Sdg";
        case GateKind::Tdg: return "Tdg";
        case GateKind::Rx: return "Rx";
        case GateKind::Ry: return "Ry";
        case GateKind::Rz: return "Rz";
        case GateKind::U: return "U";
        case GateKind::CNOT: return "CNOT";
        case GateKind::CZ: return "CZ";
        case GateKind::SWAP: return "SWAP";
        case GateKind::CCX: return "CCX";
        case GateKind::CRx: return "CRx";
        case GateKind::CRy: return "CRy";
        case GateKind::CRz: return "CRz";
        case GateKind::Barrier: return "Barrier";
        case GateKind::Measure: return "Measure";
    }
    return "I";
}

GateKind gate_kind_from_string(const std::string& s) {
    if (s == "I") return GateKind::I;
    if (s == "H") return GateKind::H;
    if (s == "X") return GateKind::X;
    if (s == "Y") return GateKind::Y;
    if (s == "Z") return GateKind::Z;
    if (s == "S") return GateKind::S;
    if (s == "T") return GateKind::T;
    if (s == "Sdg") return GateKind::Sdg;
    if (s == "Tdg") return GateKind::Tdg;
    if (s == "Rx") return GateKind::Rx;
    if (s == "Ry") return GateKind::Ry;
    if (s == "Rz") return GateKind::Rz;
    if (s == "U") return GateKind::U;
    if (s == "CNOT" || s == "CX") return GateKind::CNOT;
    if (s == "CZ") return GateKind::CZ;
    if (s == "SWAP") return GateKind::SWAP;
    if (s == "CCX") return GateKind::CCX;
    if (s == "CRx") return GateKind::CRx;
    if (s == "CRy") return GateKind::CRy;
    if (s == "CRz") return GateKind::CRz;
    if (s == "Barrier" || s == "BAR") return GateKind::Barrier;
    if (s == "Measure" || s == "M") return GateKind::Measure;
    throw std::invalid_argument("unknown gate kind: " + s);
}

void validate_circuit_op(const CircuitOp& op, int num_qubits) {
    const auto& info = gate_info(op.kind);
    const auto in_range = [num_qubits](int q) { return q >= 0 && q < num_qubits; };

    if (!in_range(op.qubit)) {
        throw std::invalid_argument("qubit index out of range for gate " + gate_kind_to_string(op.kind));
    }
    if (info.num_qubits >= 2) {
        if (op.qubit2 < 0 || !in_range(op.qubit2)) {
            throw std::invalid_argument("missing or invalid qubit2 for gate " +
                                        gate_kind_to_string(op.kind));
        }
        if (op.kind == GateKind::CNOT || op.kind == GateKind::CZ ||
            op.kind == GateKind::CRx || op.kind == GateKind::CRy || op.kind == GateKind::CRz) {
            if (op.qubit == op.qubit2) {
                throw std::invalid_argument("two-qubit gate requires distinct qubits");
            }
        }
    }
    if (info.num_qubits >= 3) {
        if (op.qubit3 < 0 || !in_range(op.qubit3)) {
            throw std::invalid_argument("missing or invalid qubit3 for gate " +
                                        gate_kind_to_string(op.kind));
        }
        if (op.qubit == op.qubit2 || op.qubit == op.qubit3 || op.qubit2 == op.qubit3) {
            throw std::invalid_argument("CCX requires three distinct qubits");
        }
    }
}

Circuit parse_circuit_ops(const nlohmann::json& j) {
    Circuit circuit;
    circuit.num_qubits = j.at("num_qubits").get<int>();
    int id = 1;
    for (const auto& entry : j.at("operations")) {
        CircuitOp op;
        op.kind = gate_kind_from_string(entry.at("gate").get<std::string>());
        op.column = entry.at("column").get<int>();
        op.qubit = entry.at("qubit").get<int>();
        if (entry.contains("qubit2")) op.qubit2 = entry["qubit2"].get<int>();
        if (entry.contains("qubit3")) op.qubit3 = entry["qubit3"].get<int>();
        if (entry.contains("param1")) op.param1 = entry["param1"].get<double>();
        if (entry.contains("param2")) op.param2 = entry["param2"].get<double>();
        if (entry.contains("param3")) op.param3 = entry["param3"].get<double>();
        op.id = id++;
        validate_circuit_op(op, circuit.num_qubits);
        circuit.add_op(op);
    }
    return circuit;
}

}  // namespace

std::string circuit_to_json(const Circuit& circuit, const std::string& description,
                           const std::string& category) {
    nlohmann::json j;
    j["version"] = 1;
    j["num_qubits"] = circuit.num_qubits;
    j["description"] = description;
    if (!category.empty()) j["category"] = category;
    nlohmann::json ops = nlohmann::json::array();
    for (const auto& op : circuit.ops) {
        nlohmann::json entry;
        entry["gate"] = gate_kind_to_string(op.kind);
        entry["column"] = op.column;
        entry["qubit"] = op.qubit;
        if (op.qubit2 >= 0) entry["qubit2"] = op.qubit2;
        if (op.qubit3 >= 0) entry["qubit3"] = op.qubit3;
        if (op.param1 != 0.0) entry["param1"] = op.param1;
        if (op.param2 != 0.0) entry["param2"] = op.param2;
        if (op.param3 != 0.0) entry["param3"] = op.param3;
        ops.push_back(entry);
    }
    j["operations"] = ops;
    return j.dump(2);
}

Circuit circuit_from_json(const std::string& json) {
    return circuit_document_from_json(json).circuit;
}

CircuitDocument circuit_document_from_json(const std::string& json) {
    const auto j = nlohmann::json::parse(json);
    CircuitDocument doc;
    doc.circuit = parse_circuit_ops(j);
    if (j.contains("description")) {
        doc.description = j["description"].get<std::string>();
    }
    if (j.contains("category")) {
        doc.category = j["category"].get<std::string>();
    }
    return doc;
}

CircuitDocument load_circuit_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("failed to open circuit file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return circuit_document_from_json(ss.str());
}

void save_circuit_file(const std::string& path, const Circuit& circuit,
                       const std::string& description, const std::string& category) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("failed to write circuit file: " + path);
    }
    file << circuit_to_json(circuit, description, category);
}

std::string read_circuit_description(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    const auto j = nlohmann::json::parse(file, nullptr, false);
    if (j.is_discarded() || !j.contains("description")) return "";
    return j["description"].get<std::string>();
}

std::string read_circuit_category(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    const auto j = nlohmann::json::parse(file, nullptr, false);
    if (j.is_discarded() || !j.contains("category")) return "";
    return j["category"].get<std::string>();
}

}  // namespace qsim
