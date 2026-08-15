cmake_minimum_required(VERSION 3.20)
project(quantum-core LANGUAGES CXX)

add_library(quantum-core
    src/state_vector.cpp
    src/gates.cpp
    src/circuit.cpp
    src/simulator.cpp
    src/json_io.cpp
    src/qasm_io.cpp
    src/pauli.cpp
    src/ansatz.cpp
    src/qaoa.cpp
    src/optimizer.cpp
    src/vqe.cpp
)

target_include_directories(quantum-core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(quantum-core PUBLIC cxx_std_20)

target_link_libraries(quantum-core PUBLIC nlohmann_json::nlohmann_json)
