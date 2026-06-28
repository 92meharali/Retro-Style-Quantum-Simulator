#pragma once

#include "qsim/circuit.hpp"

#include <string>

namespace qsim {

/// OpenQASM 2.0 subset: h, x, y, z, cx, cz, swap, rx, ry, rz
std::string circuit_to_qasm(const Circuit& circuit);

Circuit circuit_from_qasm(const std::string& qasm);

Circuit load_qasm_file(const std::string& path);
void save_qasm_file(const std::string& path, const Circuit& circuit);

/// Gates not representable in the OpenQASM 2.0 subset (CCX, S, T, Measure, etc.)
int count_unsupported_qasm_gates(const Circuit& circuit);

}  // namespace qsim
