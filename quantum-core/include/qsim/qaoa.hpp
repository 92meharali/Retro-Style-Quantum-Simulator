#pragma once

#include "qsim/ansatz.hpp"
#include "qsim/circuit.hpp"
#include "qsim/pauli.hpp"

#include <string>
#include <utility>
#include <vector>

namespace qsim {

struct Edge {
    int u = 0;
    int v = 1;
    double weight = 1.0;

    bool operator==(const Edge& other) const {
        return (u == other.u && v == other.v) || (u == other.v && v == other.u);
    }
};

class MaxCutGraph {
public:
    MaxCutGraph() : MaxCutGraph(4) {}
    explicit MaxCutGraph(int num_nodes);

    int num_nodes() const { return num_nodes_; }
    void set_num_nodes(int n);

    const std::vector<Edge>& edges() const { return edges_; }
    void add_edge(int u, int v, double weight = 1.0);
    bool remove_edge(int u, int v);
    bool has_edge(int u, int v) const;
    void clear_edges() { edges_.clear(); }

    const std::vector<std::pair<float, float>>& node_positions() const { return node_positions_; }
    void set_node_position(int node, float x, float y);

    /// Evaluates cut value for a binary state bitstring
    double evaluate_cut(int bitstring) const;

    /// Evaluates MaxCut brute force optimum and finds all maximum cut bitstrings
    double max_cut_exact(std::vector<int>* best_bitstrings = nullptr) const;

    /// Cost Hamiltonian H_C = sum_{(u,v)} w/2 * (I - Z_u Z_v)
    /// Expectation <x| H_C |x> equals the cut value for state |x>.
    Hamiltonian cost_hamiltonian() const;

    /// QAOA Minimization Hamiltonian H_min = -H_C = sum_{(u,v)} w/2 * (Z_u Z_v - I)
    Hamiltonian qaoa_hamiltonian() const;

    // Standard Benchmark Graphs
    static MaxCutGraph triangle();      // 3 nodes, 3 edges, max cut = 2
    static MaxCutGraph cycle4();        // 4 nodes, 4 edges, max cut = 4
    static MaxCutGraph bowtie5();       // 5 nodes, 6 edges, max cut = 5
    static MaxCutGraph regular6();      // 6 nodes, 9 edges (3-regular)

private:
    void auto_layout();

    int num_nodes_ = 4;
    std::vector<Edge> edges_;
    std::vector<std::pair<float, float>> node_positions_;
};

class QAOAMaxCutAnsatz : public Ansatz {
public:
    QAOAMaxCutAnsatz(MaxCutGraph graph, int layers = 1);

    AnsatzKind kind() const override { return AnsatzKind::QAOAMaxCut; }
    int num_qubits() const override { return graph_.num_nodes(); }
    int layers() const { return layers_; }
    int num_params() const override { return 2 * layers_; }

    const MaxCutGraph& graph() const { return graph_; }
    void set_graph(MaxCutGraph g) { graph_ = std::move(g); }
    void set_layers(int p) { layers_ = std::max(1, p); }

    std::vector<std::string> param_names() const override;
    std::vector<double> default_initial_params() const override;

    Circuit build_circuit(const std::vector<double>& params) const override;

private:
    MaxCutGraph graph_;
    int layers_ = 1;
};

}  // namespace qsim
