#include "qsim/ansatz.hpp"
#include "qsim/circuit.hpp"
#include "qsim/gates.hpp"
#include "qsim/optimizer.hpp"
#include "qsim/pauli.hpp"
#include "qsim/qaoa.hpp"
#include "qsim/simulator.hpp"
#include "qsim/vqe.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace qsim;

void benchmark_statevector_scaling() {
    std::cout << "\n===================================================================\n";
    std::cout << " [1] STATE-VECTOR ALLOCATION & SCALING BENCHMARK\n";
    std::cout << "===================================================================\n";
    std::cout << std::left << std::setw(8) << "Qubits"
              << std::setw(16) << "Dimension (2^N)"
              << std::setw(18) << "Memory (Bytes)"
              << std::setw(20) << "Reset/Alloc Time (us)"
              << "\n";
    std::cout << "-------------------------------------------------------------------\n";

    for (int n = 2; n <= 14; ++n) {
        const int dim = 1 << n;
        const std::size_t mem = static_cast<std::size_t>(dim) * sizeof(Complex);

        auto t0 = std::chrono::high_resolution_clock::now();
        StateVector sv(n);
        sv.reset();
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        std::cout << std::left << std::setw(8) << n
                  << std::setw(16) << dim
                  << std::setw(18) << mem
                  << std::fixed << std::setprecision(2) << std::setw(20) << elapsed_us
                  << "\n";
    }
}

void benchmark_gate_throughput() {
    std::cout << "\n===================================================================\n";
    std::cout << " [2] QUANTUM GATE SIMULATION THROUGHPUT BENCHMARK\n";
    std::cout << "===================================================================\n";
    std::cout << std::left << std::setw(8) << "Qubits"
              << std::setw(14) << "Gate Type"
              << std::setw(16) << "Total Gates"
              << std::setw(16) << "Total Time (ms)"
              << std::setw(18) << "Throughput (Mops/s)"
              << "\n";
    std::cout << "-------------------------------------------------------------------\n";

    const int n_qubits[] = {2, 4, 8, 10, 12};
    for (int n : n_qubits) {
        StateVector sv(n);
        const int reps = (n <= 8) ? 50000 : ((n <= 10) ? 5000 : 500);

        // 1-Qubit Hadamard Benchmark
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < reps; ++r) {
            apply_1q_gate(sv, r % n, GateKind::H);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms_h = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double mops_h = (static_cast<double>(reps) / (ms_h * 1000.0));

        std::cout << std::left << std::setw(8) << n
                  << std::setw(14) << "Hadamard (1q)"
                  << std::setw(16) << reps
                  << std::fixed << std::setprecision(3) << std::setw(16) << ms_h
                  << std::setw(18) << mops_h
                  << "\n";

        // 2-Qubit CNOT Benchmark
        t0 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < reps; ++r) {
            apply_cnot(sv, r % (n - 1), (r % (n - 1)) + 1);
        }
        t1 = std::chrono::high_resolution_clock::now();
        double ms_cx = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double mops_cx = (static_cast<double>(reps) / (ms_cx * 1000.0));

        std::cout << std::left << std::setw(8) << n
                  << std::setw(14) << "CNOT (2q)"
                  << std::setw(16) << reps
                  << std::fixed << std::setprecision(3) << std::setw(16) << ms_cx
                  << std::setw(18) << mops_cx
                  << "\n";
    }
}

void benchmark_vqe_qaoa_optimization() {
    std::cout << "\n===================================================================\n";
    std::cout << " [3] VQE & QAOA OPTIMIZATION PIPELINE PERFORMANCE\n";
    std::cout << "===================================================================\n";
    std::cout << std::left << std::setw(22) << "Algorithm / Target"
              << std::setw(12) << "Params"
              << std::setw(14) << "Iterations"
              << std::setw(16) << "Total Time (ms)"
              << std::setw(18) << "Step Rate (iter/s)"
              << std::setw(16) << "Final Energy"
              << "\n";
    std::cout << "-------------------------------------------------------------------\n";

    // Benchmark VQE on XX + ZZ
    {
        auto h = Hamiltonian::two_qubit_xx_zz();
        VQEExperiment exp(h, std::make_shared<TwoQubitRyCnotAnsatz>(true));
        OptimizerConfig cfg;
        cfg.kind = OptimizerKind::Adam;
        cfg.learning_rate = 0.1;
        cfg.max_iterations = 100;

        auto session = exp.create_session(cfg, {0.1, 0.2, 0.1, 0.2});
        auto t0 = std::chrono::high_resolution_clock::now();
        session->run(100);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double rate = (100.0 / ms) * 1000.0;

        std::cout << std::left << std::setw(22) << "VQE (XX+ZZ, 2q)"
                  << std::setw(12) << 4
                  << std::setw(14) << 100
                  << std::fixed << std::setprecision(2) << std::setw(16) << ms
                  << std::setw(18) << rate
                  << std::fixed << std::setprecision(5) << std::setw(16) << session->best_cost()
                  << "\n";
    }

    // Benchmark VQE on H2 Molecule
    {
        auto h = Hamiltonian::h2_molecule(0.7414);
        VQEExperiment exp(h, std::make_shared<TwoQubitRyCnotAnsatz>(true));
        OptimizerConfig cfg;
        cfg.kind = OptimizerKind::Adam;
        cfg.learning_rate = 0.08;
        cfg.max_iterations = 100;

        auto session = exp.create_session(cfg, {0.1, 0.1, 0.1, 0.1});
        auto t0 = std::chrono::high_resolution_clock::now();
        session->run(100);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double rate = (100.0 / ms) * 1000.0;

        std::cout << std::left << std::setw(22) << "VQE (H2 Molecule)"
                  << std::setw(12) << 4
                  << std::setw(14) << 100
                  << std::fixed << std::setprecision(2) << std::setw(16) << ms
                  << std::setw(18) << rate
                  << std::fixed << std::setprecision(5) << std::setw(16) << session->best_cost()
                  << "\n";
    }

    // Benchmark QAOA on 4-Cycle (p=1)
    {
        auto g = MaxCutGraph::cycle4();
        QAOAExperiment qaoa(g, 1);
        OptimizerConfig cfg;
        cfg.kind = OptimizerKind::Adam;
        cfg.learning_rate = 0.08;
        cfg.max_iterations = 100;

        auto session = qaoa.create_session(cfg, {0.785, 0.393});
        auto t0 = std::chrono::high_resolution_clock::now();
        session->run(100);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double rate = (100.0 / ms) * 1000.0;

        std::cout << std::left << std::setw(22) << "QAOA MaxCut (4-Cycle)"
                  << std::setw(12) << 2
                  << std::setw(14) << 100
                  << std::fixed << std::setprecision(2) << std::setw(16) << ms
                  << std::setw(18) << rate
                  << std::fixed << std::setprecision(5) << std::setw(16) << session->best_cost()
                  << "\n";
    }

    // Benchmark QAOA on 6-Node 3-Regular (p=2)
    {
        auto g = MaxCutGraph::regular6();
        QAOAExperiment qaoa(g, 2);
        OptimizerConfig cfg;
        cfg.kind = OptimizerKind::Adam;
        cfg.learning_rate = 0.08;
        cfg.max_iterations = 60;

        auto session = qaoa.create_session(cfg, {0.5, 0.4, 0.3, 0.2});
        auto t0 = std::chrono::high_resolution_clock::now();
        session->run(60);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double rate = (60.0 / ms) * 1000.0;

        std::cout << std::left << std::setw(22) << "QAOA (6-Node Reg, p=2)"
                  << std::setw(12) << 4
                  << std::setw(14) << 60
                  << std::fixed << std::setprecision(2) << std::setw(16) << ms
                  << std::setw(18) << rate
                  << std::fixed << std::setprecision(5) << std::setw(16) << session->best_cost()
                  << "\n";
    }
    std::cout << "===================================================================\n\n";
}

int main() {
    std::cout << "===================================================================\n";
    std::cout << " RETRO QUANTUM SIMULATOR - VQE & QAOA PERFORMANCE BENCHMARK SUITE\n";
    std::cout << " C++20 High-Performance Quantum State-Vector & Optimization Engine\n";
    std::cout << "===================================================================\n";

    benchmark_statevector_scaling();
    benchmark_gate_throughput();
    benchmark_vqe_qaoa_optimization();

    return 0;
}
