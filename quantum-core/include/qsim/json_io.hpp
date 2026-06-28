#pragma once

#include "qsim/circuit.hpp"

#include <string>

namespace qsim {

struct CircuitDocument {
    Circuit circuit;
    std::string description;
    std::string category;
};

std::string circuit_to_json(const Circuit& circuit, const std::string& description = "",
                           const std::string& category = "");
Circuit circuit_from_json(const std::string& json);
CircuitDocument circuit_document_from_json(const std::string& json);

CircuitDocument load_circuit_file(const std::string& path);
void save_circuit_file(const std::string& path, const Circuit& circuit,
                       const std::string& description = "",
                       const std::string& category = "");

std::string read_circuit_description(const std::string& path);
std::string read_circuit_category(const std::string& path);

}  // namespace qsim
